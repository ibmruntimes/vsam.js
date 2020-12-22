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

// VSAM register 15 for interpreting some of the errors:
// https://www.ibm.com/support/knowledgecenter/SSB27H_6.2.0/fa2mc2_vsevsam_return_and_error_codes.html
#define R15 __amrc->__code.__feedback.__rc

static std::string &createErrorMsg(std::string &errmsg, int err, int err2,
                                   int r15, const std::string &errPrefix);

static void print_amrc() {
  __amrc_type currErr = *__amrc;
  printf("R15 value = %d\n", currErr.__code.__feedback.__rc);
  printf("Reason code = %d\n", currErr.__code.__feedback.__fdbk);
  printf("RBA = %d\n", currErr.__RBA);
  printf("Last op = %d\n", currErr.__last_op);
}

void VsamFile::getFindKey(UvWorkData *pdata, char **pbuf, int *pbuflen) {
  const LayoutItem &key_layout = layout_[key_i_];

  if (pdata->keybuf_) {
    DCHECK(pdata->keybuf_len_ > 0);
    *pbuf = pdata->keybuf_;
    *pbuflen = pdata->keybuf_len_;
#ifdef DEBUG
    fprintf(stderr,
            "FindExecute key_layout.maxLength=%d, pdata->keybuf_len_=%d, "
            "buf=",
            key_layout.maxLength, pdata->keybuf_len_);
    for (int i = 0; i < *pbuflen; i++)
      fprintf(stderr, "%02x ", (*pbuf)[i]);
    fprintf(stderr, "\n");
#endif
    return;
  }
  if (pdata->equality_ == __KEY_FIRST || pdata->equality_ == __KEY_LAST) {
    *pbuflen = key_layout.maxLength;
    *pbuf = NULL;
    return;
  }
  DCHECK(pdata->keystr_.length() > 0);
  DCHECK(pdata->keybuf_len_ == 0);

  if (key_layout.type == LayoutItem::HEXADECIMAL) {
    DCHECK(key_layout.maxLength == keylen_ && keylen_ > 0);
    // caller frees *pbuf if != pdata->keystr.c_str() && != pdata->keybuf_
    *pbuf = (char *)malloc(key_layout.maxLength);
    *pbuflen =
        hexstrToBuffer(*pbuf, key_layout.maxLength, pdata->keystr_.c_str());
#ifdef DEBUG
    fprintf(
        stderr,
        "FindExecute user string=<%s>, key_layout.maxLength=%d, keylen_=%d, "
        "flocate(): stream=%p, tid=%d, buflen=%d, equality=%d, buf=",
        pdata->keystr_.c_str(), key_layout.maxLength, keylen_, stream_,
        gettid(), *pbuflen, pdata->equality_);
    for (int i = 0; i < *pbuflen; i++)
      fprintf(stderr, "%02x ", (*pbuf)[i]);
    fprintf(stderr, "\n");
#endif
  } else {
    *pbuf = const_cast<char *>(pdata->keystr_.c_str());
    *pbuflen = pdata->keystr_.length();
#ifdef DEBUG
    fprintf(stderr, "FindExecute keystr_.length()=%d, buf=", *pbuflen);
    for (int i = 0; i < *pbuflen; i++)
      fprintf(stderr, "%02x ", (*pbuf)[i]);
    fprintf(stderr, "\n");
#endif
  }
}

int VsamFile::FindExecute(UvWorkData *pdata, const char *buf, int buflen) {
  DCHECK(pdata->recbuf_ != NULL);
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
  int r15 = R15;
#ifdef DEBUG
  fprintf(stderr, "FindExecute flocate() returned rc=%d, r15=%d\n", pdata->rc_,
          r15);
#endif
  if (pdata->rc_ == 0) {
#ifdef DEBUG_CRUD
    assert(fgetpos(stream_, &freadpos_) == 0);
#endif
    int nread = fread(pdata->recbuf_, reclen_, 1, stream_);
    r15 = R15;
    if (nread == 1) {
#if defined(DEBUG) || defined(DEBUG_CRUD)
      displayRecord(pdata->recbuf_, "fread() in Find");
#endif
      return 0;
    }
    pdata->rc_ = 1;
    createErrorMsg(pdata->errmsg_, errno, __errno2(), r15,
                   "find error: record found but could not be read");
  } else if (r15 == 8) {
    pdata->errmsg_ = "no record found";
    // flocate() returns only 0 or EOF (-1)
    assert(pdata->rc_ == EOF);
    pdata->rc_ = r15;
  } else {
    assert(pdata->rc_ != 0);
    createErrorMsg(pdata->errmsg_, errno, __errno2(), r15,
                   "find error: flocate() failed");
  }
  return pdata->rc_;
}

void VsamFile::FindExecute(UvWorkData *pdata) {
  DCHECK(pdata->rc_ != 0);
  char *buf;
  int buflen;
  getFindKey(pdata, &buf, &buflen);

  DCHECK(pdata->recbuf_ == NULL);
  pdata->recbuf_ = (char *)malloc(reclen_);
  DCHECK(pdata->recbuf_ != NULL);
  FindExecute(pdata, buf, buflen);

  if (buf != pdata->keystr_.c_str() && buf != pdata->keybuf_)
    free(buf);
}

void VsamFile::FindUpdateExecute(UvWorkData *pdata) {
  DCHECK(pdata->rc_ != 0);
  DCHECK(pdata->recbuf_ != NULL);
  DCHECK(pdata->pFieldsToUpdate_ != NULL);
  // recbuf_ should contain fields to update, save it as FindExecute()
  // overwrites it:
  char *pupdrecbuf = (char *)malloc(reclen_);
  DCHECK(pupdrecbuf != NULL);
  memcpy(pupdrecbuf, pdata->recbuf_, reclen_);

  char *keybuf;
  int keybuflen;
  getFindKey(pdata, &keybuf, &keybuflen);

  pdata->rc_ = FindExecute(pdata, keybuf, keybuflen);
  int nread, r15;

  for (pdata->count_ = 0; pdata->rc_ == 0;) {
    for (auto i = pdata->pFieldsToUpdate_->begin();
         i != pdata->pFieldsToUpdate_->end(); ++i) {
#ifdef DEBUG
      fprintf(stderr,
              "FindUpdateExecute rec #%d, updating %s to: ", pdata->count_ + 1,
              i->name.c_str());
      for (int o = 0; o < i->len; o++)
        fprintf(stderr, "%02x ", pupdrecbuf[i->offset + o]);
      fprintf(stderr, "\n");
#endif
      memcpy(pdata->recbuf_ + i->offset, pupdrecbuf + i->offset, i->len);
    }
    pdata->rc_ = 1;
    UpdateExecute(pdata);
    if (pdata->rc_ != 0)
      break;
    pdata->count_++;
#ifdef DEBUG_CRUD
    assert(fgetpos(stream_, &freadpos_) == 0);
#endif
    nread = fread(pdata->recbuf_, reclen_, 1, stream_);
    r15 = R15;
    if (feof(stream_))
      break;
    if (nread != 1) {
      pdata->rc_ = 1;
      createErrorMsg(pdata->errmsg_, errno, __errno2(), r15,
                     "FindUpdateExecute error: fread failed");
      break;
    }
#if defined(DEBUG) || defined(DEBUG_CRUD)
    displayRecord(pdata->recbuf_, "fread() in FindUpdate");
#endif
    if (memcmp(pdata->recbuf_ + keypos_, keybuf, keybuflen)) {
#ifdef DEBUG
      fprintf(stderr, "FindUpdateExecute: record doesn't match, stop\n");
#endif
      break;
    }
  }
  if (keybuf != pdata->keystr_.c_str() && keybuf != pdata->keybuf_)
    free(keybuf);
  free(pupdrecbuf);

  if (pdata->rc_ == 8) {
    if (pdata->count_ == 0)
      pdata->errmsg_ = "no record found with the key for update";
    else {
      pdata->errmsg_ = "";
      pdata->rc_ = 0;
    }
  }
}

void VsamFile::FindDeleteExecute(UvWorkData *pdata) {
  DCHECK(pdata->rc_ != 0);

  char *keybuf;
  int keybuflen;
  getFindKey(pdata, &keybuf, &keybuflen);

  DCHECK(pdata->recbuf_ == NULL);
  pdata->recbuf_ = (char *)malloc(reclen_);
  DCHECK(pdata->recbuf_ != NULL);

  pdata->rc_ = FindExecute(pdata, keybuf, keybuflen);
  int nread, r15;

  for (pdata->count_ = 0; pdata->rc_ == 0;) {
    pdata->rc_ = 1;
    DeleteExecute(pdata);
    if (pdata->rc_ != 0)
      break;
    pdata->count_++;
#ifdef DEBUG_CRUD
    assert(fgetpos(stream_, &freadpos_) == 0);
#endif
    nread = fread(pdata->recbuf_, reclen_, 1, stream_);
    r15 = R15;
    if (feof(stream_))
      break;
    if (nread != 1) {
      pdata->rc_ = 1;
      createErrorMsg(pdata->errmsg_, errno, __errno2(), r15,
                     "FindDeleteExecute error: fread failed");
      break;
    }
#if defined(DEBUG) || defined(DEBUG_CRUD)
    displayRecord(pdata->recbuf_, "fread() in FindDelete");
#endif
    if (memcmp(pdata->recbuf_ + keypos_, keybuf, keybuflen)) {
#ifdef DEBUG
      fprintf(stderr, "FindDeleteExecute: record doesn't match, stop\n");
#endif
      break;
    }
  }
  if (keybuf != pdata->keystr_.c_str() && keybuf != pdata->keybuf_)
    free(keybuf);

  if (pdata->rc_ == 8) {
    if (pdata->count_ == 0)
      pdata->errmsg_ = "no record found with the key for delete";
    else {
      pdata->errmsg_ = "";
      pdata->rc_ = 0;
    }
  }
}

void VsamFile::ReadExecute(UvWorkData *pdata) {
  DCHECK(pdata->rc_ != 0);
  DCHECK(pdata->recbuf_ == NULL);
  pdata->recbuf_ = (char *)malloc(reclen_);
  DCHECK(pdata->recbuf_ != NULL);
#ifdef DEBUG
  fprintf(stderr, "ReadExecute fread() %d bytes from tid=%d\n", reclen_,
          gettid());
#endif
#ifdef DEBUG_CRUD
  assert(fgetpos(stream_, &freadpos_) == 0);
#endif
  int nread = fread(pdata->recbuf_, reclen_, 1, stream_);
  if (nread == 1) {
    pdata->rc_ = 0;
#if defined(DEBUG) || defined(DEBUG_CRUD)
    displayRecord(pdata->recbuf_, "fread() in Read");
#endif
    return;
  }
  free(pdata->recbuf_);
  pdata->recbuf_ = NULL;
}

void VsamFile::DeleteExecute(UvWorkData *pdata) {
  DCHECK(pdata->rc_ != 0);
#ifdef DEBUG
  fprintf(stderr, "DeleteExecute fdelrec() to %p, tid=%d...\n", stream_,
          gettid());
#endif
#ifdef DEBUG_CRUD
  if (pdata->recbuf_ == NULL) {
    // coming from delete((err) => {});
    assert(fsetpos(stream_, &freadpos_) == 0);
    ReadExecute(pdata);
  }
#endif
  pdata->rc_ = fdelrec(stream_);
  int r15 = R15;
  if (pdata->rc_ != 0) {
    createErrorMsg(pdata->errmsg_, errno, __errno2(), r15,
                   "delete error: fdelrec() failed");
    return;
  }
#if defined(DEBUG) || defined(DEBUG_CRUD)
  displayRecord(pdata->recbuf_, "fdelrec() in Delete");
#endif
}

void VsamFile::displayRecord(const char *recbuf, const char *pSuffix) {
  int i, pos = 0;
  fprintf(stderr, "REC=|");
  for (auto l = layout_.begin(); l != layout_.end(); ++l) {
    // fprintf(stderr, "%s:", l->name.c_str());
    if (l->type == LayoutItem::HEXADECIMAL) {
      for (i = 0; i < l->maxLength; i++, pos++)
        fprintf(stderr, "%02x", recbuf[pos]);
    } else {
      assert(l->type == LayoutItem::STRING);
      for (i = 0; i < l->maxLength; i++, pos++)
        fprintf(stderr, "%c", recbuf[pos] ? recbuf[pos] : '.');
    }
    fprintf(stderr, "|");
  }
  fprintf(stderr, " %s\n", pSuffix);
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
  int r15 = R15;
#ifdef DEBUG
  fprintf(stderr, "WriteExecute fwrite() wrote %d bytes, errno=%d, errno2=%d\n",
          nelem, errno, __errno2());
#endif
  if (nelem != reclen_) {
    if (r15 == 8)
      createErrorMsg(pdata->errmsg_, errno, __errno2(), r15,
                     "write error: an attempt was made to store a record with "
                     "a duplicate key");
    else
      createErrorMsg(pdata->errmsg_, errno, __errno2(), r15,
                     "write error: fwrite() failed");
    return;
  }
#if defined(DEBUG) || defined(DEBUG_CRUD)
  displayRecord(pdata->recbuf_, "fwrite() in Write");
#endif
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
  int r15 = R15;
#ifdef DEBUG
  fprintf(stderr, "UpdateExecute fupdate() wrote %d bytes\n", nbytes);
#endif
  if (nbytes != reclen_) {
    if (r15 == 8)
      createErrorMsg(pdata->errmsg_, errno, __errno2(), r15,
                     "update error: an attempt was made to store a record with "
                     "a duplicate key");
    else
      createErrorMsg(pdata->errmsg_, errno, __errno2(), r15,
                     "update error: fupdate() failed");
    return;
  }
  pdata->rc_ = 0;
#if defined(DEBUG) || defined(DEBUG_CRUD)
  displayRecord(pdata->recbuf_, "fupdate() in Update");
#endif
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
  int r15 = R15;
#ifdef DEBUG
  fprintf(stderr, "DeallocExecute remove() returned %d\n", pdata->rc_);
#endif
  if (pdata->rc_ != 0)
    createErrorMsg(pdata->errmsg_, errno, __errno2(), r15,
                   "dealloc error: remove() failed");
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
bool VsamFile::isDatasetExist(const std::string &path, int *perr, int *perr2,
                              int *pr15) {
  std::string dataset = formatDatasetName(path);
  FILE *stream = fopen(dataset.c_str(), "rb,type=record");
  if (pr15)
    *pr15 = R15;
  int err2 = __errno2();
  if (perr)
    *perr = errno;
  if (perr2)
    *perr2 = err2;
#ifdef DEBUG
  fprintf(stderr,
          "isDatasetExist fopen(%s, rb,type=record) returned %p, tid=%d\n",
          dataset.c_str(), stream, gettid());
#endif
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
  int r15 = R15;
#ifdef DEBUG
  fprintf(stderr, "VsamFile: fopen(%s, %s) returned %p, tid=%d\n",
          dsname.c_str(), omode_.c_str(), stream_, gettid());
#endif
  int err = errno;
  int err2 = __errno2();

  if (stream_ == NULL) {
    createErrorMsg(errmsg_, errno, __errno2(), r15,
                   "open error: fopen() failed");
    return;
  }
  rc_ = setKeyRecordLengths("open");
#ifdef DEBUG
  fprintf(stderr, "VsamFile: VSAM dataset opened rc=%d.\n", rc_);
#endif
}

void VsamFile::alloc(UvWorkData *pdata) {
  DCHECK(rc_ != 0);
  DCHECK(pdata == NULL);
  DCHECK(stream_ == NULL);
  int err, err2, r15;
  std::string dsname = formatDatasetName(path_);
  if (isDatasetExist(dsname.c_str(), &err, &err2, &r15)) {
    errmsg_ = "Dataset already exists";
    return;
  }
  if (err2 != 0xC00B0641) {
    // 0xC00B0641 is file not found
    createErrorMsg(errmsg_, err, err2, r15, "alloc error: fopen() failed");
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
    createErrorMsg(errmsg_, err, err2, R15, "alloc error: dynalloc() failed");
    return;
  }
  stream_ = fopen(dsname.c_str(), "ab+,type=record");
  r15 = R15;
#ifdef DEBUG
  fprintf(stderr, "VsamFile: fopen(%s, ab+,type=record) returned %p, tid=%d\n",
          dsname.c_str(), stream_, gettid());
#endif
  if (stream_ == NULL) {
    createErrorMsg(errmsg_, errno, __errno2(), r15,
                   "open error: fopen() failed to open new dataset");
    return;
  }
  rc_ = setKeyRecordLengths("alloc");
#ifdef DEBUG
  if (rc_ == 0)
    fprintf(stderr,
            "VsamFile: VSAM dataset created and opened successfully.\n");
#endif
}

int VsamFile::setKeyRecordLengths(const std::string &errPrefix) {
  DCHECK(stream_ != NULL);
  fldata_t dinfo;
  fldata(stream_, NULL, &dinfo);
  keylen_ = dinfo.__vsamkeylen;
  reclen_ = dinfo.__maxreclen;
  if (keylen_ == layout_[key_i_].maxLength) {
    return 0;
  }

  errmsg_ = errPrefix + " error: key length " + std::to_string(keylen_) +
            " doesn't match length " +
            std::to_string(layout_[key_i_].maxLength) + " in schema.";
#ifdef DEBUG
  fprintf(stderr, "%s setKeyRecordLengths %s\nClosing stream %p",
          errPrefix.c_str(), errmsg_.c_str(), stream_);
#endif
  fclose(stream_);
  stream_ = NULL;
  return -1;
}

VsamFile::VsamFile(const std::string &path,
                   const std::vector<LayoutItem> &layout, int key_i, int keypos,
                   const std::string &omode)
    : path_(path), layout_(layout), key_i_(key_i), keypos_(keypos),
      omode_(omode), stream_(NULL), keylen_(0), rc_(1) {
#ifdef DEBUG
  fprintf(stderr, "In VsamFile constructor for %s.\n", path_.c_str());
#endif
  // open() or alloc() should be called directly by WrappedVsam that created
  // this.
  if (!isReadOnly()) {
    vsamThread_ = std::thread(vsamThread, this, &vsamThreadCV_,
                              &vsamThreadMmutex_, &vsamThreadQueue_);
  }
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
    createErrorMsg(pdata->errmsg_, errno, __errno2(), R15,
                   "close error: fclose() failed");
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
                          const std::string &errPrefix, std::string &errmsg) {
  int len = str.length();
  if (len < item.minLength || len < 0) {
    errmsg = errPrefix + " error: length of '" + item.name + "' must be " +
             std::to_string(item.minLength) + " or more.";
    return false;
  } else if (len == 0)
    return true;
  if (str.length() > item.maxLength) {
    errmsg = errPrefix + " error: length " + std::to_string(str.length()) +
             " of '" + item.name.c_str() + "' exceeds schema's length " +
             std::to_string(item.maxLength) + ".";
    return false;
  }
  return true;
}

bool VsamFile::isHexBufValid(const LayoutItem &item, const char *buf, int len,
                             const std::string &errPrefix,
                             std::string &errmsg) {
  if (len < item.minLength || len < 0) {
    errmsg = errPrefix + " error: length of '" + item.name + "' must be " +
             std::to_string(item.minLength) + " or more.";
    return false;
  } else if (len == 0)
    return true;
  if (len > item.maxLength) {
    errmsg = errPrefix + " error: length " + std::to_string(len) + " of '" +
             item.name + "' exceeds schema's length " +
             std::to_string(item.maxLength) + ".";
    return false;
  }
  return true;
}

bool VsamFile::isHexStrValid(const LayoutItem &item, const std::string &hexstr,
                             const std::string &errPrefix,
                             std::string &errmsg) {
  int len = hexstr.length();
  if (len < item.minLength || len < 0) {
    errmsg = errPrefix + " error: length of '" + item.name + "' must be " +
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
    errmsg = errPrefix + " error: hex string for '" + item.name +
             "' must contain only hex digits 0-9 and a-f or A-F, with an "
             "optional 0x prefix, found <" +
             hexstr + ">.";
    return false;
  }
  len -= start;
  int digits = (len + (len % 2)) / 2;
  if (digits > item.maxLength) {
    errmsg = errPrefix + " error: number of hex digits " +
             std::to_string(digits) + " for '" + item.name +
             "' exceed schema's length " + std::to_string(item.maxLength) +
             ", found <" + hexstr + ">.";
    return false;
  }
  return true;
}

static std::string &createErrorMsg(std::string &errmsg, int err, int err2,
                                   int r15, const std::string &errPrefix) {
  // err is errno, err2 is __errno2()
  errmsg = errPrefix;
  std::string e(strerror(err));
  if (!e.empty())
    errmsg += ": " + e;
  if (err2 || r15) {
    char ebuf[64];
    sprintf(ebuf, " (R15=%d, errno2=0x%08x)", R15, err2);
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
