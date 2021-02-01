/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2021. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#pragma once
#include <napi.h>
#include <queue>
#include <string>
#include <thread>

#ifdef DEBUG
#define DCHECK_WITH_MSG(condition, message)                                    \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr,                                                          \
              "Fatal error in %s line %d:\n"                                   \
              "Debug check failed: %s\n",                                      \
              __FILE__, __LINE__, message);                                    \
      exit(-1);                                                                \
    }                                                                          \
  } while (0)
#define DCHECK(condition) DCHECK_WITH_MSG(condition, #condition)
#else
#define DCHECK(condition) ((void)0)
#endif

struct LayoutItem {
  enum DataType { STRING, HEXADECIMAL };

  std::string name;
  int minLength;
  int maxLength;
  DataType type;
  LayoutItem(std::string &n, int mn, int mx, DataType t)
      : name(n), minLength(mn), maxLength(mx), type(t) {}
};

struct FieldToUpdate {
  int offset;
  int len;
#ifdef DEBUG
  std::string name;
  LayoutItem::DataType type;
  FieldToUpdate(int ofs, int len, const std::string &nm, LayoutItem::DataType t)
      : offset(ofs), len(len), name(nm), type(t) {}
#else
  FieldToUpdate(int ofs, int len) : offset(ofs), len(len) {}
#endif
};

class VsamFile;

// This is the 'data' member in uv_work_t request:
struct UvWorkData {
  UvWorkData(VsamFile *pVsamFile, Napi::Function cbfunc, Napi::Env env,
             const std::string &path = "", char *recbuf = nullptr,
             char *keybuf = nullptr, int keybuf_len = 0, int equality = 0,
             std::vector<FieldToUpdate> *pFieldsToUpdate = nullptr)
      : pVsamFile_(pVsamFile), cb_(Napi::Persistent(cbfunc)), env_(env),
        path_(path), recbuf_(recbuf), keybuf_(keybuf), keybuf_len_(keybuf_len),
        equality_(equality), pFieldsToUpdate_(pFieldsToUpdate), count_(0),
        rc_(1) {}

  ~UvWorkData() {
    if (recbuf_) {
      free(recbuf_);
      recbuf_ = nullptr;
    }
    if (keybuf_) {
      free(keybuf_);
      keybuf_ = nullptr;
    }
    if (pFieldsToUpdate_) {
      delete pFieldsToUpdate_;
      pFieldsToUpdate_ = nullptr;
    }
  }

  VsamFile *pVsamFile_;
  Napi::FunctionReference cb_;
  Napi::Env env_;
  std::string path_;
  char *recbuf_;
  char *keybuf_;
  int keybuf_len_;
  int equality_;
  std::vector<FieldToUpdate> *pFieldsToUpdate_;
  int rc_;
  int count_; // of records updated or deleted to report back
  std::string errmsg_;
};

typedef enum {
  MSG_OPEN = 1,
  MSG_CLOSE,
  MSG_READ,
  MSG_WRITE,
  MSG_UPDATE,
  MSG_DELETE,
  MSG_FIND,
  MSG_FIND_UPDATE,
  MSG_FIND_DELETE,
  MSG_EXIT
} VSAM_THREAD_MSGID;

typedef struct {
  VSAM_THREAD_MSGID msgid;
  std::condition_variable &cv;
  void (VsamFile::*pWorkFunc)(UvWorkData *);
  UvWorkData *pdata;
  int rc;
} ST_VsamThreadMsg;

class VsamFile {
public:
  VsamFile(const std::string &path, const std::vector<LayoutItem> &layout,
           int key_i, int keypos, const std::string &omode);
  ~VsamFile();

  int getKeyNum() const { return key_i_; }
  int getRecordLength() const { return reclen_; }
  int getLastError(std::string &errmsg) const {
    errmsg = errmsg_;
    return rc_;
  }
  std::vector<LayoutItem> &getLayout() { return layout_; }
  bool isDatasetOpen() const { return (stream_ != nullptr); }
  bool isReadOnly() const {
    const char *omode = omode_.c_str();
    if (strchr(omode, 'w') || strchr(omode, 'a') || strchr(omode, '+'))
      return false;
    return true;
  }

  static std::string formatDatasetName(const std::string &path);
  static bool isDatasetExist(const std::string &path, int *perr = 0,
                             int *perr2 = 0, int *pr15 = 0);
  static bool isStrValid(const LayoutItem &item, const std::string &str,
                         const std::string &errPrefix, std::string &errmsg);
  static bool isHexBufValid(const LayoutItem &item, const char *buf, int len,
                            const std::string &errPrefix, std::string &errmsg);
  static bool isHexStrValid(const LayoutItem &item, const std::string &hexstr,
                            const std::string &errPrefix, std::string &errmsg);
  static int hexstrToBuffer(char *hexbuf, int buflen, const char *hexstr);
  static int bufferToHexstr(char *hexstr, int hexstrlen, const char *hexbuf,
                            int hexbuflen);

  /* static because WrappedVsam::Close() delete its VsamFile object */
  static void DeallocExecute(UvWorkData *pdata);

  /* The pdata arg here is only so those functions can be passed in the */
  /* message to the thread as ST_VsamThreadMsg's pWorkFunc */
  void open(UvWorkData *pdata);
  void alloc(UvWorkData *pdata);
  void Close(UvWorkData *pdata);

  /* Work functions */
  void ReadExecute(UvWorkData *pdata);
  void FindExecute(UvWorkData *pdata);
  void FindUpdateExecute(UvWorkData *pdata);
  void FindDeleteExecute(UvWorkData *pdata);
  void UpdateExecute(UvWorkData *pdata);
  void WriteExecute(UvWorkData *pdata);
  void DeleteExecute(UvWorkData *pdata);

  int routeToVsamThread(VSAM_THREAD_MSGID msgid,
                        void (VsamFile::*pWorkFunc)(UvWorkData *),
                        UvWorkData *pdata = nullptr);
  void detachVsamThread() { vsamThread_.detach(); }
  int exitVsamThread();
  int getVsamThreadId() { return vsamThread_.native_handle().__ & 0x7fffffff; }

private:
  int setKeyRecordLengths(const std::string &errPrefix);
  int FindExecute(UvWorkData *pdata, const char *buf, int buflen);
  void displayRecord(const char *recbuf, const char *pPrefix);
  int freadRecord(UvWorkData *pdata, int *pr15, bool expectEOF,
                   const char *pDisplayPrefix, const char *pErrPrefix);

private:
  FILE *stream_;
  std::string path_;
  std::string omode_;
  std::vector<LayoutItem> layout_;
  int rc_;
  std::string errmsg_;
  int key_i_;
  int keypos_;
  unsigned keylen_, reclen_;
#ifdef DEBUG_CRUD
  // to read the record before delete((err)) for display
  fpos_t freadpos_; // =fgetpos() before fread()
#endif

private:
  // These are used only if dataset is opened in read/write mode:
  std::thread vsamThread_;
  std::condition_variable vsamThreadCV_;
  std::mutex vsamThreadMmutex_;
  std::queue<ST_VsamThreadMsg *> vsamThreadQueue_;
};
