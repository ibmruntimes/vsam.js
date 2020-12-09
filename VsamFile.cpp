/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2017. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 */
#include "VsamFile.h"
#include "VsamThread.h"
#include <dynit.h>
#include <numeric>
#include <sstream>
#include <unistd.h>

static std::string &createErrorMsg(std::string &errmsg, int err, int err2,
                                   const char *title);

static void print_amrc() {
  __amrc_type currErr = *__amrc;
  printf("R15 value = %d\n", currErr.__code.__feedback.__rc);
  printf("Reason code = %d\n", currErr.__code.__feedback.__fdbk);
  printf("RBA = %d\n", currErr.__RBA);
  printf("Last op = %d\n", currErr.__last_op);
}

void VsamFile::FindExecute(UvWorkData *pdata) {
  DCHECK(pdata->rc_ != 0);
  int rc;
  const char *buf;
  int buflen;
  const LayoutItem &key_layout = layout_[key_i_];

  if (pdata->keybuf_) {
    DCHECK(pdata->keybuf_len_ > 0);
    buf = pdata->keybuf_;
    buflen = pdata->keybuf_len_;
#ifdef DEBUG
    fprintf(stderr,
            "FindExecute key_layout.maxLength=%d, pdata->keybuf_len_=%d, "
            "buflen=%d, buf=",
            key_layout.maxLength, pdata->keybuf_len_, buflen);
    for (int i = 0; i < buflen; i++)
      fprintf(stderr, "%02x ", buf[i]);
    fprintf(stderr, "\n");
#endif
  } else if (pdata->equality_ != __KEY_FIRST &&
             pdata->equality_ != __KEY_LAST) {
    DCHECK(pdata->keystr_.length() > 0);
    DCHECK(pdata->keybuf_len_ == 0);
    if (key_layout.type == LayoutItem::HEXADECIMAL) {
      DCHECK(key_layout.maxLength == keylen_ && keylen_ > 0);
      char buf[key_layout.maxLength];
      buflen = hexstrToBuffer(buf, sizeof(buf), pdata->keystr_.c_str());
#ifdef DEBUG
      fprintf(
          stderr,
          "FindExecute user string=<%s>, key_layout.maxLength=%d, keylen_=%d, "
          "flocate(): stream=%p, tid=%d, buflen=%d, equality=%d, buf=",
          pdata->keystr_.c_str(), key_layout.maxLength, keylen_, stream_,
          gettid(), buflen, pdata->equality_);
      for (int i = 0; i < buflen; i++)
        fprintf(stderr, "%02x ", buf[i]);
      fprintf(stderr, "\n");
#endif
      pdata->rc_ = flocate(stream_, buf, buflen, pdata->equality_);
#ifdef DEBUG
      fprintf(stderr, "FindExecute flocate() returned rc=%d\n", pdata->rc_);
#endif
      goto chk;
    } else {
      buf = pdata->keystr_.c_str();
      buflen = pdata->keystr_.length();
#ifdef DEBUG
      fprintf(stderr, "FindExecute keylen_=%d, buf=", keylen_);
      for (int i = 0; i < buflen; i++)
        fprintf(stderr, "%02x ", buf[i]);
      fprintf(stderr, "\n");
#endif
    }
  } else
    buflen = key_layout.maxLength;

  DCHECK(buflen <= key_layout.maxLength);
#ifdef DEBUG
  fprintf(stderr,
          "FindExecute flocate() stream=%p, tid=%d, buflen=%d,  equality_=%d, "
          "buf=",
          stream_, gettid(), buflen, pdata->equality_);
  for (int i = 0; i < buflen; i++)
    fprintf(stderr, "%02x ", buf[i]);
  fprintf(stderr, "\n");
#endif
  pdata->rc_ = flocate(stream_, buf, buflen, pdata->equality_);
#ifdef DEBUG
  fprintf(stderr, "FindExecute flocate() returned rc=%d\n", pdata->rc_);
#endif
chk:
  if (pdata->rc_ == 0) {
    DCHECK(pdata->recbuf_ == NULL);
    pdata->recbuf_ = (char *)malloc(reclen_);
    DCHECK(pdata->recbuf_ != NULL);
    int nread = fread(pdata->recbuf_, reclen_, 1, stream_);
    if (nread == 1) {
#ifdef DEBUG
      fprintf(stderr, "FindExecute reclen_=%d, fread=", reclen_);
      for (int i = 0; i < reclen_; i++)
        fprintf(stderr, "%02x ", pdata->recbuf_[i]);
      fprintf(stderr, "\n");
#endif
      return;
    }
    pdata->rc_ = 1;
    createErrorMsg(pdata->errmsg_, errno, __errno2(),
                   "Error: find() record found but could not be read");
  } else {
    pdata->rc_ = 0;
    DCHECK(pdata->recbuf_ == NULL);
  }
}

void VsamFile::ReadExecute(UvWorkData *pdata) {
  DCHECK(pdata->rc_ != 0);
  pdata->recbuf_ = (char *)malloc(reclen_);
  DCHECK(pdata->recbuf_ != NULL);
#ifdef DEBUG
  fprintf(stderr, "ReadExecute fread() %d bytes from tid=%d\n", reclen_,
          gettid());
#endif
  int nread = fread(pdata->recbuf_, reclen_, 1, stream_);
#ifdef DEBUG
  fprintf(stderr, "ReadExecute fread() read %d elements\n", nread);
#endif
  if (nread == 1) {
    pdata->rc_ = 0;
    return;
  }
  free(pdata->recbuf_);
  pdata->recbuf_ = NULL;
}

void VsamFile::DeleteExecute(UvWorkData *pdata) {
  DCHECK(pdata->rc_ != 0);
#ifdef DEBUG
  fprintf(stderr, "DeleteExecute fdelrec() to %p, tid=%d...", stream_, gettid());
#endif
  pdata->rc_ = fdelrec(stream_);
  if (pdata->rc_ != 0) {
    createErrorMsg(pdata->errmsg_, errno, __errno2(), "Error: delete() failed");
    return;
  }
#ifdef DEBUG
  fprintf(stderr, "fdelrec() done.\n");
#endif
}

void VsamFile::WriteExecute(UvWorkData *pdata) {
  DCHECK(pdata->rc_ != 0);
  DCHECK(pdata->recbuf_ != NULL);
#ifdef DEBUG
  fprintf(stderr, "WriteExecute fwrite() to %p, tid=%d, reclen=%d: ", stream_,
          gettid(), reclen_);
  for (int i = 0; i < reclen_; i++)
    fprintf(stderr, "%02x ", pdata->recbuf_[i]);
  fprintf(stderr, "\n");
#endif
  int nelem = fwrite(pdata->recbuf_, 1, reclen_, stream_);
#ifdef DEBUG
  fprintf(stderr, "WriteExecute fwrite() wrote %d bytes\n", nelem);
#endif
  if (nelem != reclen_) {
    createErrorMsg(pdata->errmsg_, errno, __errno2(), "Error: write() failed");
    return;
  }
  pdata->rc_ = 0;
}

void VsamFile::UpdateExecute(UvWorkData *pdata) {
  DCHECK(pdata->rc_ != 0);
  DCHECK(pdata->recbuf_ != NULL);
#ifdef DEBUG
  fprintf(stderr, "UpdateExecute fupdate() to %p, tid=%d: ", stream_, gettid());
  for (int i = 0; i < reclen_; i++)
    fprintf(stderr, "%02x ", pdata->recbuf_[i]);
  fprintf(stderr, "\n");
#endif
  int nbytes = fupdate(pdata->recbuf_, reclen_, stream_);
#ifdef DEBUG
  fprintf(stderr, "UpdateExecute fupdate() wrote %d bytes\n", nbytes);
#endif
  if (nbytes != reclen_) {
    createErrorMsg(pdata->errmsg_, errno, __errno2(), "Error: update() failed");
    return;
  }
  pdata->rc_ = 0;
}

// static
void VsamFile::DeallocExecute(UvWorkData *pdata) {
  DCHECK(pdata->rc_ != 0);
  DCHECK(pdata->path_.length() > 0);
  std::string dataset = formatDatasetName(pdata->path_);
#ifdef DEBUG
  fprintf(stderr, "DeallocExecute remove() from tid=%d\n", gettid());
#endif
  pdata->rc_ = remove(dataset.c_str());
#ifdef DEBUG
  fprintf(stderr, "DeallocExecute remove() returned %d\n", pdata->rc_);
#endif
  if (pdata->rc_ != 0)
    createErrorMsg(pdata->errmsg_, errno, __errno2(),
                   "Error: dealloc() failed");
}

// static
std::string VsamFile::formatDatasetName(const std::string &path) {
  std::ostringstream dataset;
  if (path[0] == '/' && path[1] == '/' && path[2] == '\'')
    dataset << path;
  else
    dataset << "//'" << path << "'";
  return dataset.str();
}

// static
bool VsamFile::isDatasetExist(const std::string &path, int *perr, int *perr2) {
  std::string dataset = formatDatasetName(path);
  FILE *stream = fopen(dataset.c_str(), "rb,type=record");
#ifdef DEBUG
  fprintf(stderr,
          "isDatasetExist fopen(%s, rb,type=record) returned %p, tid=%d\n",
          dataset.c_str(), stream, gettid());
#endif
  int err2 = __errno2();
  if (perr)
    *perr = errno;
  if (perr2)
    *perr2 = err2;
  if (stream != NULL) {
#ifdef DEBUG
    fprintf(stderr, "isDatasetExist() fclose(%p)\n", stream);
#endif
    fclose(stream);
    return true;
  }
  if (err2 == 0xC00B0641) {
    // 0xC00B0641 is file not found
    return false;
  }
  if (err2 == 0xC00A0022) {
    // 0xC00A0022 could be if opening an empty dataset as read-only,
    // double-check:
    stream = fopen(dataset.c_str(), "rb+,type=record");
#ifdef DEBUG
    fprintf(stderr,
            "isDatasetExist fopen(%s, ab+,type=record) returned %p, tid=%d\n",
            dataset.c_str(), stream, gettid());
#endif
    if (perr)
      *perr = errno;
    if (perr2)
      *perr2 = __errno2();
    if (stream == NULL) {
      return false;
    }
#ifdef DEBUG
    fprintf(stderr, "isDatasetExist fclose(%p)\n", stream);
#endif
    fclose(stream);
    return true;
  }
  return false;
}

void VsamFile::open(UvWorkData *pdata) {
  DCHECK(rc_ != 0);
  DCHECK(pdata == NULL);
  DCHECK(stream_ == NULL);
  std::string dsname = formatDatasetName(path_);
  stream_ = fopen(dsname.c_str(), omode_.c_str());
#ifdef DEBUG
  fprintf(stderr, "VsamFile: fopen(%s, %s) returned %p, tid=%d\n",
          dsname.c_str(), omode_.c_str(), stream_, gettid());
#endif
  int err = errno;
  int err2 = __errno2();

  if (stream_ == NULL) {
    createErrorMsg(errmsg_, errno, __errno2(), "Error: failed to open dataset");
    return;
  }
  rc_ = setKeyRecordLengths();
#ifdef DEBUG
  fprintf(stderr, "VsamFile: VSAM dataset opened rc=%d.\n", rc_);
#endif
}

void VsamFile::alloc(UvWorkData *pdata) {
  DCHECK(rc_ != 0);
  DCHECK(pdata == NULL);
  DCHECK(stream_ == NULL);
  int err, err2;
  std::string dsname = formatDatasetName(path_);
  if (isDatasetExist(dsname.c_str(), &err, &err2)) {
    errmsg_ = "Dataset already exists";
    return;
  }
  if (err2 != 0xC00B0641) {
    // 0xC00B0641 is file not found
    createErrorMsg(errmsg_, err, err2, "Unexpected fopen error");
    return;
  }
  std::ostringstream ddname;
  ddname << "NAMEDD";

  __dyn_t dyn;
  dyninit(&dyn);
  dyn.__dsname = &(path_[0]);
  dyn.__ddname = &(ddname.str()[0]);
  dyn.__normdisp = __DISP_CATLG;
  dyn.__lrecl = std::accumulate(
      layout_.begin(), layout_.end(), 0,
      [](int n, LayoutItem &l) -> int { return n + l.maxLength; });
  dyn.__keyoffset = std::accumulate(
      layout_.begin(), layout_.begin() + key_i_, 0,
      [](int n, LayoutItem &l) -> int { return n + l.maxLength; });
  dyn.__keylength = layout_[key_i_].maxLength;
  dyn.__recorg = __KS;
  if (dynalloc(&dyn) != 0) {
    createErrorMsg(errmsg_, err, err2, "Failed to allocate dataset");
    return;
  }
  stream_ = fopen(dsname.c_str(), "ab+,type=record");
#ifdef DEBUG
  fprintf(stderr, "VsamFile: fopen(%s, ab+,type=record) returned %p, tid=%d\n",
          dsname.c_str(), stream_, gettid());
#endif
  if (stream_ == NULL) {
    createErrorMsg(errmsg_, errno, __errno2(),
                   "Error: failed to open new dataset");
    return;
  }
  rc_ = setKeyRecordLengths();
#ifdef DEBUG
  if (rc_ == 0)
    fprintf(stderr,
            "VsamFile: VSAM dataset created and opened successfully.\n");
#endif
}

int VsamFile::setKeyRecordLengths() {
  DCHECK(stream_ != NULL);
  fldata_t dinfo;
  fldata(stream_, NULL, &dinfo);
  keylen_ = dinfo.__vsamkeylen;
  reclen_ = dinfo.__maxreclen;
  if (keylen_ == layout_[key_i_].maxLength) {
    return 0;
  }

  errmsg_ = "Error: key length " + std::to_string(keylen_) +
            " doesn't match length " +
            std::to_string(layout_[key_i_].maxLength) + " in schema.";
#ifdef DEBUG
  fprintf(stderr, "setKeyRecordLengths error: %s\nClosing stream %p", errmsg_.c_str(), stream_);
#endif
  fclose(stream_);
  stream_ = NULL;
  return -1;
}

VsamFile::VsamFile(const std::string &path,
                   const std::vector<LayoutItem> &layout, int key_i,
                   const std::string &omode)
    : path_(path), layout_(layout), key_i_(key_i), omode_(omode), stream_(NULL),
      keylen_(0), rc_(1) {
#ifdef DEBUG
  fprintf(stderr, "In VsamFile constructor for %s.\n", path_.c_str());
#endif
  // open() or alloc() should be called directly by WrappedVsam that created
  // this.
  vsamThread_ = std::thread(vsamThread, this, &vsamThreadCV_,
                            &vsamThreadMmutex_, &vsamThreadQueue_);
}

VsamFile::~VsamFile() {
#ifdef DEBUG
  fprintf(stderr, "~VsamFile: this=%p, stream_=%p.\n", this, stream_);
#endif
  if (stream_ != NULL) {
#ifdef DEBUG
    fprintf(stderr, "~VsamFile: fclose(%p)\n", stream_);
#endif
    fclose(stream_);
    stream_ = NULL;
  }
}

void VsamFile::Close(UvWorkData *pdata) {
  // non-async
  if (stream_ == NULL) {
    pdata->errmsg_ = "VSAM dataset is not open.";
    return;
  }
#ifdef DEBUG
  fprintf(stderr, "Close fclose(%p)\n", stream_);
#endif
  if (fclose(stream_)) {
    createErrorMsg(pdata->errmsg_, errno, __errno2(),
                   "Error: failed to close VSAM dataset");
    return;
  }
  stream_ = NULL;
#ifdef DEBUG
  fprintf(stderr, "VSAM dataset closed successfully.\n");
#endif
  pdata->rc_ = 0;
}

int VsamFile::hexstrToBuffer(char *hexbuf, int buflen, const char *hexstr) {
  DCHECK(hexstr != NULL && hexbuf != NULL && buflen > 0);
  memset(hexbuf, 0, buflen);
  if (hexstr[0] == 0)
    return 0;

  char xx[2];
  int i, j, x;
  if (hexstr[0] == '0' && (hexstr[1] == 'x' || hexstr[1] == 'X'))
    hexstr += 2;
  else if (hexstr[0] == 'x' || hexstr[0] == 'X')
    hexstr += 1;

  int hexstrlen = strlen(hexstr);
  DCHECK(hexstrlen <= (buflen * 2));
  for (i = 0, j = 0; j < buflen && i < hexstrlen - (hexstrlen % 2); ++j) {
    xx[0] = hexstr[i++];
    xx[1] = hexstr[i++];
    sscanf(xx, "%2x", &x);
    hexbuf[j] = x;
  }
  if (j < buflen && hexstrlen % 2) {
    DCHECK(i < strlen(hexstr));
    xx[0] = hexstr[i];
    xx[1] = '0';
    sscanf(xx, "%2x", &x);
    hexbuf[j++] = x;
  }
  DCHECK(j <= buflen);
  return j;
}

int VsamFile::bufferToHexstr(char *hexstr, int hexstrlen, const char *hexbuf,
                             int hexbuflen) {
  DCHECK(hexstr != NULL && hexbuf != NULL && hexbuflen > 0);
  int i, j;
  *hexstr = 0;
  for (i = 0, j = 0; i < hexbuflen; i++, j += 2)
    sprintf(hexstr + j, "%02x", hexbuf[i]);

  hexstr[j] = 0;

  // remove trailing '00's
  for (--j; j > 2 && hexstr[j] == '0' && hexstr[j - 1] == '0'; j -= 2)
    hexstr[j - 1] = 0;
  DCHECK(j < hexstrlen);
  return j;
}

bool VsamFile::isStrValid(const LayoutItem &item, const std::string &str,
                          std::string &errmsg) {
  int len = str.length();
  if (len < item.minLength || len < 0) {
    errmsg = "Error: length of '" + item.name + "' must be " +
             std::to_string(item.minLength) + " or more.";
    return false;
  } else if (len == 0)
    return true;
  if (str.length() > item.maxLength) {
    errmsg = "Error: length " + std::to_string(str.length()) + " of '" +
             item.name.c_str() + "' exceeds schema's length " +
             std::to_string(item.maxLength) + ".";
    return false;
  }
  return true;
}

bool VsamFile::isHexBufValid(const LayoutItem &item, const char *buf, int len,
                             std::string &errmsg) {
  if (len < item.minLength || len < 0) {
    errmsg = "Error: length of '" + item.name + "' must be " +
             std::to_string(item.minLength) + " or more.";
    return false;
  } else if (len == 0)
    return true;
  if (len > item.maxLength) {
    errmsg = "Error: length " + std::to_string(len) + " of '" + item.name +
             "' exceeds schema's length " + std::to_string(item.maxLength) +
             ".";
    return false;
  }
  return true;
}

bool VsamFile::isHexStrValid(const LayoutItem &item, const std::string &hexstr,
                             std::string &errmsg) {
  int len = hexstr.length();
  if (len < item.minLength || len < 0) {
    errmsg = "Error: length of '" + item.name + "' must be " +
             std::to_string(item.minLength) + " or more.";
    return false;
  } else if (len == 0)
    return true;
  int start = 0;
  if (hexstr[0] == '0' && (hexstr[1] == 'x' || hexstr[1] == 'X')) {
    start = 2;
  } else if (hexstr[0] == 'x' || hexstr[0] == 'X') {
    start = 1;
  }
  if (!std::all_of(hexstr.begin() + start, hexstr.end(), ::isxdigit)) {
    errmsg = "Error: hex string for '" + item.name +
             "' must contain only hex digits 0-9 and a-f or A-F, with an "
             "optional 0x prefix.";
    return false;
  }
  len -= start;
  int digits = (len + (len % 2)) / 2;
  if (digits > item.maxLength) {
    errmsg = "Error: number of hex digits " + std::to_string(digits) +
             " for '" + item.name + "' exceed schema's length " +
             std::to_string(item.maxLength) + ".";
    return false;
  }
  return true;
}

static std::string &createErrorMsg(std::string &errmsg, int err, int err2,
                                   const char *title) {
  // err is errno, err2 is __errno2()
  errmsg = title;
  std::string e(strerror(err));
  if (!e.empty())
    errmsg += ": " + e;
  if (err2) {
    char ebuf[32];
    sprintf(ebuf, " (errno2=0x%08x)", err2);
    errmsg += ebuf;
  }
  errmsg += '.';
#ifdef DEBUG
  fprintf(stderr, "%s\n", errmsg.c_str());
#endif
  return errmsg;
}

int VsamFile::routeToVsamThread(VSAM_THREAD_MSGID msgid,
                                void (VsamFile::*pWorkFunc)(UvWorkData *),
                                UvWorkData *pdata) {
  std::condition_variable cv;
  ST_VsamThreadMsg msg = {msgid, cv, pWorkFunc, pdata, -1};
  std::unique_lock<std::mutex> lck(vsamThreadMmutex_);
  vsamThreadQueue_.push(&msg);
  vsamThreadCV_.notify_one();
  cv.wait(lck);
  return msg.rc;
}

int VsamFile::exitVsamThread() {
  if (getVsamThreadId() == 0) {
#ifdef DEBUG
    fprintf(stderr, "exitVsamThread thread id is 0, nothing to do.\n");
#endif
    return 0;
  }
  if (!vsamThread_.joinable()) {
#ifdef DEBUG
    fprintf(stderr, "exitVsamThread thread %d not joinable, nothing to do.\n",
            getVsamThreadId());
#endif
    return 0;
  }
  std::condition_variable cv;
  ST_VsamThreadMsg msg = {MSG_EXIT, cv, nullptr, nullptr, -1};
  std::unique_lock<std::mutex> lck(vsamThreadMmutex_);
  vsamThreadQueue_.push(&msg);
  vsamThreadCV_.notify_one();
  cv.wait(lck);
  if (vsamThread_.joinable()) {
#ifdef DEBUG
    fprintf(stderr, "exitVsamThread calling join() on thread %d...",
            getVsamThreadId());
    vsamThread_.join();
    fprintf(stderr, "done\n.");
#else
    vsamThread_.join();
#endif
  }
#ifdef DEBUG
  else {
    fprintf(stderr,
            "exitVsamThread thread %d joinable state changed from true to "
            "false, nothing to do.\n",
            getVsamThreadId());
  }
#endif
  return msg.rc;
}
