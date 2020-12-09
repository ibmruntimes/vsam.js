/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2017. All Rights Reserved.
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

class VsamFile;

// This is the 'data' member in uv_work_t request:
struct UvWorkData {
  UvWorkData(VsamFile *pVsamFile, Napi::Function cbfunc, Napi::Env env,
             const std::string &path = "", char *recbuf = NULL,
             const std::string &keystr = "", char *keybuf = NULL,
             int keybuf_len = 0, int equality = 0)
      : pVsamFile_(pVsamFile), cb_(Napi::Persistent(cbfunc)), env_(env),
        path_(path), recbuf_(recbuf), keystr_(keystr), keybuf_(keybuf),
        keybuf_len_(keybuf_len), equality_(equality), rc_(1) {}

  ~UvWorkData() {
    if (recbuf_) {
      free(recbuf_);
      recbuf_ = NULL;
    }
    if (keybuf_) {
      free(keybuf_);
      keybuf_ = NULL;
    }
  }

  VsamFile *pVsamFile_;
  Napi::FunctionReference cb_;
  Napi::Env env_;
  std::string path_;
  char *recbuf_;
  std::string keystr_;
  char *keybuf_;
  int keybuf_len_;
  int equality_;
  int rc_;
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
           int key_i, const std::string &omode);
  ~VsamFile();

  int getKeyNum() const { return key_i_; }
  int getRecordLength() const { return reclen_; }
  int getLastError(std::string &errmsg) const {
    errmsg = errmsg_;
    return rc_;
  }
  std::vector<LayoutItem> &getLayout() { return layout_; }
  bool isDatasetOpen() const { return (stream_ != NULL); }
  bool isReadOnly() const {
    const char *omode = omode_.c_str();
    if (strchr(omode, 'w') || strchr(omode, 'a') || strchr(omode, '+'))
      return false;
    return true;
  }

  static std::string formatDatasetName(const std::string &path);
  static bool isDatasetExist(const std::string &path, int *perr = 0,
                             int *perr2 = 0);
  static bool isStrValid(const LayoutItem &item, const std::string &str,
                         std::string &errmsg);
  static bool isHexBufValid(const LayoutItem &item, const char *buf, int len,
                            std::string &errmsg);
  static bool isHexStrValid(const LayoutItem &item, const std::string &hexstr,
                            std::string &errmsg);
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

  /* Work functions TODO - change to lowercase and remove Execute */
  void ReadExecute(UvWorkData *pdata);
  void FindExecute(UvWorkData *pdata);
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
  int setKeyRecordLengths();

private:
  FILE *stream_;
  std::string path_;
  std::string omode_;
  std::vector<LayoutItem> layout_;
  int rc_;
  std::string errmsg_;
  int key_i_;
  unsigned keylen_, reclen_;

private:
  // These are used only if dataset is opened in read/write mode:
  std::thread vsamThread_;
  std::condition_variable vsamThreadCV_;
  std::mutex vsamThreadMmutex_;
  std::queue<ST_VsamThreadMsg *> vsamThreadQueue_;
};
