/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2017. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 */
#include "WrappedVsam.h"
#include "VsamThread.h"
#include <sstream>
#include <unistd.h>

Napi::FunctionReference WrappedVsam::constructor_;

static void throwError(const Napi::CallbackInfo &info, int errArgNum,
                       bool isTypeErr, const char *fmt, ...) {
  char msg[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
#ifdef DEBUG
  fprintf(stderr, "%s\n", msg);
#endif
  Napi::Env env = info.Env();
  if (errArgNum >= 0) {
    for (int i = 0; i < info.Length(); i++) {
      if (!info[i].IsFunction())
        continue;
      Napi::Function cb = info[i].As<Napi::Function>();
      if (errArgNum == 0)
        cb.Call(env.Global(), {Napi::String::New(env, msg)});
      else if (errArgNum == 1)
        cb.Call(env.Global(), {env.Null(), Napi::String::New(env, msg)});
      else
        assert(0); // currently errArgNum is only 0 or 1
      return;
    }
  }
  if (isTypeErr)
    Napi::TypeError::New(env, msg).ThrowAsJavaScriptException();
  else
    Napi::Error::New(env, msg).ThrowAsJavaScriptException();
}

void WrappedVsam::DefaultComplete(uv_work_t *req, int status) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  delete req;

  if (status == UV_ECANCELED) {
    delete pdata;
    return;
  }
  Napi::HandleScope scope(pdata->env_);
  DCHECK(pdata->cb_ != NULL && pdata->env_ != NULL);
  if (pdata->rc_ != 0)
    pdata->cb_.Call(pdata->env_.Global(),
                    {Napi::String::New(pdata->env_, pdata->errmsg_)});
  else
    pdata->cb_.Call(pdata->env_.Global(), {pdata->env_.Null()});
  delete pdata;
}

void WrappedVsam::FindUpdateComplete(uv_work_t *req, int status) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  delete req;

  if (status == UV_ECANCELED) {
    delete pdata;
    return;
  }
  Napi::HandleScope scope(pdata->env_);
  DCHECK(pdata->cb_ != NULL && pdata->env_ != NULL);
  if (pdata->rc_ != 0)
    // even on error, 1 or more records may have been updated before the error
    pdata->cb_.Call(pdata->env_.Global(),
                    {Napi::Number::New(pdata->env_, pdata->count_),
                     Napi::String::New(pdata->env_, pdata->errmsg_)});
  else
    pdata->cb_.Call(
        pdata->env_.Global(),
        {Napi::Number::New(pdata->env_, pdata->count_), pdata->env_.Null()});
  delete pdata;
}

void WrappedVsam::FindDeleteComplete(uv_work_t *req, int status) {
  FindUpdateComplete(req, status);
}

void WrappedVsam::DeleteComplete(uv_work_t *req, int status) {
  DefaultComplete(req, status);
}

void WrappedVsam::WriteComplete(uv_work_t *req, int status) {
  DefaultComplete(req, status);
}

void WrappedVsam::UpdateComplete(uv_work_t *req, int status) {
  DefaultComplete(req, status);
}

void WrappedVsam::DeallocComplete(uv_work_t *req, int status) {
  DefaultComplete(req, status);
}

void WrappedVsam::ReadComplete(uv_work_t *req, int status) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  delete req;

  if (status == UV_ECANCELED) {
    delete pdata;
    return;
  }
  Napi::HandleScope scope(pdata->env_);
  DCHECK(pdata->cb_ != NULL && pdata->env_ != NULL);
  if (pdata->rc_ != 0) {
    pdata->cb_.Call(
        pdata->env_.Global(),
        {pdata->env_.Null(), Napi::String::New(pdata->env_, pdata->errmsg_)});
    delete pdata;
    return;
  }
  if (pdata->recbuf_ == NULL) {
    pdata->cb_.Call(pdata->env_.Global(),
                    {pdata->env_.Null(), pdata->env_.Null()});
    delete pdata;
    return;
  }

  Napi::Object record = Napi::Object::New(pdata->env_);
  const char *recbuf = pdata->recbuf_;
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != NULL);
  std::vector<LayoutItem> &layout = obj->getLayout();

  for (auto i = layout.begin(); i != layout.end(); ++i) {
    if (i->type == LayoutItem::STRING) {
      std::string str(recbuf, i->maxLength + 1);
      str[i->maxLength] = 0;
      record.Set(i->name, Napi::String::New(pdata->env_, str.c_str()));
    } else if (i->type == LayoutItem::HEXADECIMAL) {
      char hexstr[(i->maxLength * 2) + 1];
      VsamFile::bufferToHexstr(hexstr, sizeof(hexstr), recbuf, i->maxLength);
      record.Set(i->name, Napi::String::New(pdata->env_, hexstr));
    }
    recbuf += i->maxLength;
  }
  pdata->cb_.Call(pdata->env_.Global(), {record, pdata->env_.Null()});
  delete pdata;
}

void WrappedVsam::FindExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != NULL);
  if (obj->isReadOnly())
    obj->FindExecute(pdata);
  else
    obj->routeToVsamThread(MSG_FIND, &VsamFile::FindExecute, pdata);
}

void WrappedVsam::FindUpdateExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != NULL);
  if (obj->isReadOnly())
    obj->FindUpdateExecute(pdata); // let it deal with the error
  else
    obj->routeToVsamThread(MSG_FIND_UPDATE, &VsamFile::FindUpdateExecute,
                           pdata);
}

void WrappedVsam::FindDeleteExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != NULL);
  if (obj->isReadOnly())
    obj->FindDeleteExecute(pdata); // let it deal with the error
  else
    obj->routeToVsamThread(MSG_FIND_DELETE, &VsamFile::FindDeleteExecute,
                           pdata);
}

void WrappedVsam::ReadExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != NULL);
  if (obj->isReadOnly())
    obj->ReadExecute(pdata);
  else
    obj->routeToVsamThread(MSG_READ, &VsamFile::ReadExecute, pdata);
}

void WrappedVsam::DeleteExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != NULL);
  if (obj->isReadOnly())
    obj->DeleteExecute(pdata); // let it deal with the error
  else
    obj->routeToVsamThread(MSG_DELETE, &VsamFile::DeleteExecute, pdata);
}

void WrappedVsam::WriteExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != NULL);
  if (obj->isReadOnly())
    obj->WriteExecute(pdata); // let it deal with the error
  else
    obj->routeToVsamThread(MSG_WRITE, &VsamFile::WriteExecute, pdata);
}

void WrappedVsam::UpdateExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != NULL);
  if (obj->isReadOnly())
    obj->UpdateExecute(pdata); // let it deal with the error
  else
    obj->routeToVsamThread(MSG_UPDATE, &VsamFile::UpdateExecute, pdata);
}

void WrappedVsam::DeallocExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  DCHECK(pdata->pVsamFile_ == NULL);
  DCHECK(pdata->path_.length() > 0);
  VsamFile::DeallocExecute(pdata);
}

WrappedVsam::WrappedVsam(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<WrappedVsam>(info) {
  if (info.Length() != 6) {
    Napi::HandleScope scope(info.Env());
    throwError(
        info, -1, true, //-1 throws an exception, not callback
        "Internal Error: wrong number of arguments to WrappedVsam constructor: "
        "got %d, expected 6.",
        info.Length());
    return;
  }

  path_ = static_cast<std::string>(info[0].As<Napi::String>());
  Napi::Buffer<std::vector<LayoutItem>> b =
      info[1].As<Napi::Buffer<std::vector<LayoutItem>>>();
  std::vector<LayoutItem> layout =
      *(static_cast<std::vector<LayoutItem> *>(b.Data()));
  int key_i = static_cast<int>(info[2].As<Napi::Number>().Int32Value());
  int keypos = static_cast<int>(info[3].As<Napi::Number>().Int32Value());
  const std::string &omode =
      static_cast<std::string>(info[4].As<Napi::String>());
  bool alloc(static_cast<bool>(info[5].As<Napi::Boolean>()));

  pVsamFile_ = new VsamFile(path_, layout, key_i, keypos, omode);
#if defined(DEBUG) || defined(XDEBUG)
  fprintf(stderr, "WrappedVsam this=%p created pVsamFile_=%p\n", this,
          pVsamFile_);
#endif
  if (pVsamFile_->isReadOnly()) {
    // no need for a separate thread to handle I/O for this dataset.
    if (alloc)
      pVsamFile_->alloc(nullptr);
    else
      pVsamFile_->open(nullptr);
    return;
  }
  pVsamFile_->routeToVsamThread(MSG_OPEN,
                                alloc ? &VsamFile::alloc : &VsamFile::open);
}

WrappedVsam::~WrappedVsam() {
#if defined(DEBUG) || defined(XDEBUG)
  fprintf(stderr, "~WrappedVsam this=%p, deleteVsamFileObj...\n", this);
#endif
  deleteVsamFileObj();
}

void WrappedVsam::deleteVsamFileObj() {
  if (pVsamFile_ == nullptr) {
#ifdef DEBUG
    fprintf(stderr,
            "deleteVsamFileObj pVsamFile_ already null, nothing to do.\n");
#endif
    return;
  }
#ifdef DEBUG
  fprintf(stderr, "deleteVsamFileObj calling vsamExitThread...\n");
#endif
  pVsamFile_->exitVsamThread();
#ifdef DEBUG
  fprintf(stderr, "deleteVsamFileObj delete pVsamFile_ %p...\n", pVsamFile_);
#endif
  delete pVsamFile_;
#ifdef DEBUG
  fprintf(stderr, "deleteVsamFileObj done...\n");
#endif
  pVsamFile_ = NULL;
}

Napi::Object WrappedVsam::Init(Napi::Env env, Napi::Object exports) {
  Napi::HandleScope scope(env);

  Napi::Function func =
      DefineClass(env, "WrappedVsam",
                  {InstanceMethod("read", &WrappedVsam::Read),
                   InstanceMethod("find", &WrappedVsam::FindEq),
                   InstanceMethod("findeq", &WrappedVsam::FindEq),
                   InstanceMethod("findge", &WrappedVsam::FindGe),
                   InstanceMethod("findfirst", &WrappedVsam::FindFirst),
                   InstanceMethod("findlast", &WrappedVsam::FindLast),
                   InstanceMethod("update", &WrappedVsam::Update),
                   InstanceMethod("findUpdate", &WrappedVsam::FindUpdate),
                   InstanceMethod("findDelete", &WrappedVsam::FindDelete),
                   InstanceMethod("write", &WrappedVsam::Write),
                   InstanceMethod("delete", &WrappedVsam::Delete),
                   InstanceMethod("close", &WrappedVsam::Close),
                   InstanceMethod("dealloc", &WrappedVsam::Dealloc)});

  constructor_ = Napi::Persistent(func);
  constructor_.SuppressDestruct();

  exports.Set("WrappedVsam", func);
  return exports;
}

// static
Napi::Object WrappedVsam::Construct(const Napi::CallbackInfo &info,
                                    bool alloc) {
  Napi::Env env = info.Env();
  Napi::EscapableHandleScope scope(env);
  const char *pApiName = alloc ? "allocSync" : "openSync";

  std::string path(static_cast<std::string>(info[0].As<Napi::String>()));
  const Napi::Object &schema = info[1].ToObject();
  const std::string &mode =
      info.Length() == 2
          ? "ab+,type=record"
          : (static_cast<std::string>(info[2].As<Napi::String>()));
  const Napi::Array &properties = schema.GetPropertyNames();
  std::vector<LayoutItem> layout;
  int key_i = 0; // for its data type - default to first field if no "key" found
  int curpos = 0, keypos = 0;

  for (int i = 0; i < properties.Length(); ++i) {
    std::string name(static_cast<std::string>(
        Napi::String(env, properties.Get(i).ToString())));

    const Napi::Object &item = schema.Get(properties.Get(i)).As<Napi::Object>();
    if (item.IsEmpty()) {
      throwError(info, -1, false, "%s error in JSON: item %d is empty.",
                 pApiName, i + 1);
      return env.Null().ToObject();
    }

    int minLength = 0; // minLength is optional, default 0 unless it's a key
    if (item.Has("minLength")) {
      const Napi::Value &vminLength = item.Get("minLength");
      if (!vminLength.IsEmpty() && !vminLength.IsNumber()) {
        throwError(
            info, -1, true,
            "%s error in JSON (item %d): minLength value must be numeric.",
            pApiName, i + 1);
        return env.Null().ToObject();
      } else if (!vminLength.IsEmpty()) {
        minLength = vminLength.ToNumber().Int32Value();
        if (minLength < 0) {
          throwError(
              info, -1, true,
              "%s error in JSON (item %d): minLength value cannot be negative.",
              pApiName, i + 1);
          return env.Null().ToObject();
        }
      }
    }

    int maxLength = 0;
    if (!item.Has("maxLength")) {
      throwError(info, -1, true,
                 "%s error in JSON (item %d): maxLength must be specified.",
                 pApiName, i + 1);
      return env.Null().ToObject();
    }
    const Napi::Value &vmaxLength = item.Get("maxLength");
    if (!vmaxLength.IsNumber()) {
      throwError(info, -1, true,
                 "%s error in JSON (item %d): maxLength must be numeric.",
                 pApiName, i + 1);
      return env.Null().ToObject();
    } else {
      maxLength = vmaxLength.ToNumber().Int32Value();
      if (maxLength <= 0) {
        throwError(info, -1, true,
                   "%s error in JSON (item %d): maxLength value must be "
                   "greater than 0.",
                   pApiName, i + 1);
        return env.Null().ToObject();
      }
    }

    if (minLength > maxLength) {
      throwError(info, -1, true,
                 "%s error in JSON (item %d): minLength cannot be greater than "
                 "maxLength.",
                 pApiName, i + 1);
      return env.Null().ToObject();
    }

    if (item.Has("type")) {
      const Napi::Value &jtype = item.Get("type");
      const std::string &stype(
          static_cast<std::string>(jtype.As<Napi::String>()));

      if (!strcmp(stype.c_str(), "string")) {
        layout.push_back(
            LayoutItem(name, minLength, maxLength, LayoutItem::STRING));
      } else if (!strcmp(stype.c_str(), "hexadecimal")) {
        layout.push_back(
            LayoutItem(name, minLength, maxLength, LayoutItem::HEXADECIMAL));
      } else {
        throwError(info, -1, true,
                   "%s error in JSON (item %d): \"type\" must be either "
                   "\"string\" or \"hexadecimal\"",
                   pApiName, i + 1);
        return env.Null().ToObject();
      }
      if (!strcmp(name.c_str(), "key")) {
        key_i = i;
        keypos = curpos;
      }
    } else {
      throwError(
          info, -1, true,
          "%s error in JSON (item %d): \"type\" must be specified (string "
          "or hexadecimal)",
          pApiName, i + 1);
      return env.Null().ToObject();
    }
    curpos += maxLength;
  }
  const Napi::Object &item =
      schema.Get(properties.Get(key_i)).As<Napi::Object>();
  DCHECK(!item.IsEmpty());
  if (item.Has("minLength")) {
    const Napi::Value &vminLength =
        item.Get(Napi::String::New(env, "minLength"));
    if (vminLength.ToNumber().Int32Value() == 0) {
      throwError(info, -1, true,
                 "%s error in JSON (item %d): minLength of key '%s' must be "
                 "greater than 0.",
                 pApiName, key_i + 1, layout[key_i].name.c_str());
      return env.Null().ToObject();
    }
  } else
    layout[key_i].minLength = 1;

  Napi::Object obj = constructor_.New(
      {Napi::String::New(env, path),
       Napi::Buffer<std::vector<LayoutItem>>::Copy(env, &layout, layout.size()),
       Napi::Number::New(env, key_i), Napi::Number::New(env, keypos),
       Napi::String::New(env, mode), Napi::Boolean::New(env, alloc)});
  std::string errmsg;
  WrappedVsam *p = Napi::ObjectWrap<WrappedVsam>::Unwrap(obj);
  if (!p || !p->pVsamFile_ || p->pVsamFile_->getLastError(errmsg) ||
      !p->pVsamFile_->isDatasetOpen()) {
    const char *perr = errmsg.c_str();
    throwError(info, -1, false,
               perr && *perr ? perr
                             : "%s error: failed to construct VsamFile object.",
               pApiName);
    if (p && p->pVsamFile_) {
#ifdef DEBUG
      fprintf(stderr, "Construct deleteVsamFileObj...\n");
#endif
      p->deleteVsamFileObj();
    }
  }
  return scope.Escape(napi_value(obj)).ToObject();
}

// static
Napi::Object WrappedVsam::AllocSync(const Napi::CallbackInfo &info) {
  if (info.Length() != 2 || !info[0].IsString() || !info[1].IsObject()) {
    Napi::HandleScope scope(info.Env());
    throwError(info, -1, true,
               "allocSync error: allocSync() expects arguments: "
               "VSAM dataset name, schema JSON object.");
    return info.Env().Null().ToObject();
  }
  return Construct(info, true);
}

// static
Napi::Object WrappedVsam::OpenSync(const Napi::CallbackInfo &info) {
  if ((info.Length() < 2 || !info[0].IsString() || !info[1].IsObject()) ||
      (info.Length() == 3 && !info[2].IsString()) || (info.Length() > 3)) {
    Napi::HandleScope scope(info.Env());
    throwError(info, -1, true,
               "openSync error: openSync() expects arguments: VSAM dataset "
               "name, schema JSON object, optional fopen() mode.");
    return info.Env().Null().ToObject();
  }
  return Construct(info, false);
}

// static
Napi::Boolean WrappedVsam::Exist(const Napi::CallbackInfo &info) {
  Napi::HandleScope scope(info.Env());
  if (info.Length() != 1 || !info[0].IsString()) {
    throwError(info, -1, true,
               "exist error: exist() expects argument: VSAM dataset name.");
    return Napi::Boolean::New(info.Env(), false);
  }
  const std::string &path(static_cast<std::string>(info[0].As<Napi::String>()));
  return Napi::Boolean::New(info.Env(), VsamFile::isDatasetExist(path));
}

void WrappedVsam::Close(const Napi::CallbackInfo &info) {
  int rc;
#ifdef DEBUG
  fprintf(stderr, "Closing VSAM dataset...\n");
#endif
  static Napi::Function dummy;
  UvWorkData uvdata(nullptr, dummy, nullptr);
  if (pVsamFile_->isReadOnly())
    pVsamFile_->Close(&uvdata);
  else
    pVsamFile_->routeToVsamThread(MSG_CLOSE, &VsamFile::Close, &uvdata);

  if (uvdata.rc_) {
    Napi::HandleScope scope(info.Env());
    throwError(info, 0, false, uvdata.errmsg_.c_str());
    return;
  }
#ifdef DEBUG
  fprintf(stderr, "Close deleteVsamFileObj...\n");
#endif
  deleteVsamFileObj();
}

void WrappedVsam::Delete(const Napi::CallbackInfo &info) {
  if ((info.Length() == 2 && info[0].IsString() && info[1].IsFunction()) ||
      (info.Length() == 3 && info[0].IsObject() && info[1].IsNumber() &&
       info[2].IsFunction())) {
    FindDelete_(info, false);
    return;
  }

  if (info.Length() != 1 || !info[0].IsFunction()) {
    throwError(info, 0, true,
               "delete error: delete() expects arguments: "
               "(err), or: "
               "key-string, (count, err), or: "
               "key-buffer, key-buffer-length, (count, err).");
    return;
  }
  uv_work_t *request = new uv_work_t;
  Napi::Function cb = info[0].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env());
  uv_queue_work(uv_default_loop(), request, DeleteExecute, DeleteComplete);
}

void WrappedVsam::Write(const Napi::CallbackInfo &info) {
  Napi::HandleScope scope(info.Env());
  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsFunction()) {
    throwError(info, 0, true,
               "write error: write() expects arguments: record, (err).");
    return;
  }
  const Napi::Object &record = info[0].ToObject();
  int reclen = pVsamFile_->getRecordLength();
  char *recbuf = (char *)malloc(reclen);
  DCHECK(recbuf != NULL && reclen > 0);
  memset(recbuf, 0, reclen);
  char *fldbuf = recbuf;
  std::vector<LayoutItem> &layout = pVsamFile_->getLayout();
  std::string errmsg;

  for (auto i = layout.begin(); i != layout.end(); ++i) {
    const Napi::Value &field = record.Get(i->name);

    if (i->type == LayoutItem::STRING || i->type == LayoutItem::HEXADECIMAL) {
#ifdef DEBUG
      if (field.IsUndefined()) {
        fprintf(stderr,
                "write value of %s was not set, will attempt to set it to "
                "all 0x00\n",
                i->name.c_str());
      }
#endif
      const std::string &str =
          field.IsUndefined() ? ""
                              : static_cast<std::string>(
                                    Napi::String(info.Env(), field.ToString()));
      if (i->type == LayoutItem::STRING) {
        if (!VsamFile::isStrValid(*i, str, "write", errmsg)) {
          throwError(info, 0, true, errmsg.c_str());
          return;
        }
        DCHECK(str.length() <= i->maxLength);
        DCHECK((fldbuf - recbuf) + str.length() <= reclen);
        if (str.length() > 0)
          memcpy(fldbuf, str.c_str(), str.length());
      } else {
        if (!VsamFile::isHexStrValid(*i, str, "write", errmsg)) {
          throwError(info, 0, true, errmsg.c_str());
          return;
        }
        DCHECK((fldbuf - recbuf) + i->maxLength <= reclen);
        VsamFile::hexstrToBuffer(fldbuf, i->maxLength, str.c_str());
      }
    } else {
      throwError(info, 0, true,
                 "write error: unexpected JSON data type %d for %s.", i->type,
                 i->name.c_str());
      return;
    }
    fldbuf += i->maxLength;
  }

  uv_work_t *request = new uv_work_t;
  Napi::Function cb = info[1].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env(), "", recbuf);
  uv_queue_work(uv_default_loop(), request, WriteExecute, WriteComplete);
}

void WrappedVsam::Update(const Napi::CallbackInfo &info) {
  Napi::HandleScope scope(info.Env());

  if ((info.Length() == 3 && info[0].IsString() && info[1].IsObject() &&
       info[2].IsFunction()) ||
      (info.Length() == 4 && info[0].IsObject() && info[1].IsNumber() &&
       info[2].IsObject() && info[3].IsFunction())) {
    FindUpdate_(info, false);
    return;
  }

  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsFunction()) {
    throwError(info, 0, true,
               "update error: update() expects arguments: "
               "record, (err), "
               "or: key-string, record, (count, err), "
               "or: key-buffer [,key-buffer-length], record, (count, err).");
    return;
  }

  const Napi::Object &record = info[0].ToObject();
  int reclen = pVsamFile_->getRecordLength();
  char *recbuf = (char *)malloc(reclen);
  DCHECK(recbuf != NULL);
  memset(recbuf, 0, reclen);
  char *fldbuf = recbuf;
  std::vector<LayoutItem> &layout = pVsamFile_->getLayout();
  std::string errmsg;

  for (auto i = layout.begin(); i != layout.end(); ++i) {
    const Napi::Value &field = record.Get(i->name);
    if (field.IsUndefined()) {
      throwError(info, 0, true,
                 "update error: update value for %s has not been set.",
                 i->name.c_str());
      return;
    }
    if (i->type == LayoutItem::STRING || i->type == LayoutItem::HEXADECIMAL) {
      const std::string &str =
          static_cast<std::string>(Napi::String(info.Env(), field.ToString()));
      if (i->type == LayoutItem::STRING) {
        if (!VsamFile::isStrValid(*i, str, "update", errmsg)) {
          throwError(info, 0, true, errmsg.c_str());
          return;
        }
        DCHECK(str.length() <= i->maxLength);
        if (str.length() > 0)
          memcpy(fldbuf, str.c_str(), str.length());
      } else {
        if (!VsamFile::isHexStrValid(*i, str, "update", errmsg)) {
          throwError(info, 0, true, errmsg.c_str());
          return;
        }
        VsamFile::hexstrToBuffer(fldbuf, i->maxLength, str.c_str());
      }
      fldbuf += i->maxLength;
    } else {
      throwError(info, 0, true, "update error: unexpected data type %d for %s.",
                 i->type, i->name.c_str());
      return;
    }
  }

  uv_work_t *request = new uv_work_t;
  Napi::Function cb = info[1].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env(), "", recbuf);
  uv_queue_work(uv_default_loop(), request, UpdateExecute, UpdateComplete);
}

void WrappedVsam::FindEq(const Napi::CallbackInfo &info) {
  if (info.Length() == 2 && info[0].IsString() && info[1].IsFunction())
    Find(info, __KEY_EQ, "find", 1);
  else if (info.Length() == 3 && info[0].IsObject() && info[1].IsNumber() &&
           info[2].IsFunction())
    Find(info, __KEY_EQ, "find", 2);
  else
    throwError(info, 1, true,
               "find error: find() expects arguments: "
               "or key-string, (record, err), "
               "or key-buffer, key-buffer-length, (record, err).");
}

void WrappedVsam::FindGe(const Napi::CallbackInfo &info) {
  if (info.Length() == 2 && info[0].IsString() && info[1].IsFunction())
    Find(info, __KEY_GE, "findge", 1);

  else if (info.Length() == 3 && info[0].IsObject() && info[1].IsNumber() &&
           info[2].IsFunction())
    Find(info, __KEY_GE, "findge", 2);

  else
    throwError(info, 1, true,
               "findge error: findge() expects arguments: "
               "or key-string, (record, err), "
               "or key-buffer, key-buffer-length, (record, err).");
}

void WrappedVsam::FindFirst(const Napi::CallbackInfo &info) {
  if (info.Length() == 1 && info[0].IsFunction())
    Find(info, __KEY_FIRST, "findfirst", 0);
  else
    throwError(info, 1, true,
               "findfirst error: findfirst() expects argument: (record, err).");
}

void WrappedVsam::FindLast(const Napi::CallbackInfo &info) {
  if (info.Length() == 1 && info[0].IsFunction())
    Find(info, __KEY_LAST, "findlast", 0);
  else
    throwError(info, 1, true,
               "findlast error: findlast() expects argument: (record, err).");
}

void WrappedVsam::Find(const Napi::CallbackInfo &info, int equality,
                       const char *pApiName, int callbackArg,
                       uv_work_cb pExecuteFunc, uv_after_work_cb pCompleteFunc,
                       char *pUpdateRecBuf,
                       std::vector<FieldToUpdate> *pFieldsToUpdate) {
  Napi::HandleScope scope(info.Env());
  std::string key;
  char *keybuf = NULL;
  int keybuf_len = 0;
  int key_i = pVsamFile_->getKeyNum();
  std::vector<LayoutItem> &layout = pVsamFile_->getLayout();
  std::string errmsg;

  if (equality != __KEY_LAST && equality != __KEY_FIRST) {
    if (info[0].IsString()) {
      key = static_cast<std::string>(info[0].As<Napi::String>());
#ifdef DEBUG
      fprintf(stderr, "%s key=<%s>\n", pApiName, key.c_str());
#endif
      if (layout[key_i].type == LayoutItem::HEXADECIMAL) {
        if (!VsamFile::isHexStrValid(layout[key_i], key, pApiName, errmsg)) {
          throwError(info, 1, true, errmsg.c_str());
          return;
        }
      } else {
        if (!VsamFile::isStrValid(layout[key_i], key, pApiName, errmsg)) {
          throwError(info, 1, true, errmsg.c_str());
          return;
        }
      }
    } else if (info[0].IsObject()) {
      const char *buf = info[0].As<Napi::Buffer<char>>().Data();
      if (!info[1].IsNumber()) {
        throwError(info, 1, true,
                   "%s error: buffer argument must be followed by its length.",
                   pApiName);
        return;
      }
      keybuf_len = info[1].As<Napi::Number>().Uint32Value();
      if (!VsamFile::isHexBufValid(layout[key_i], buf, keybuf_len, pApiName,
                                   errmsg)) {
        throwError(info, 1, true, errmsg.c_str());
        return;
      }
      DCHECK(keybuf_len > 0);
      keybuf = (char *)malloc(keybuf_len);
      DCHECK(keybuf != NULL);
      memcpy(keybuf, buf, keybuf_len);
    } else {
      throwError(info, 1, true,
                 "%s error: first argument must be "
                 "either a string or a Buffer object.",
                 pApiName);
      return;
    }
  }

  uv_work_t *request = new uv_work_t;
  Napi::Function cb = info[callbackArg].As<Napi::Function>();

  request->data =
      new UvWorkData(pVsamFile_, cb, info.Env(), "", pUpdateRecBuf, key, keybuf,
                     keybuf_len, equality, pFieldsToUpdate);

  uv_queue_work(uv_default_loop(), request, pExecuteFunc, pCompleteFunc);
}

void WrappedVsam::Read(const Napi::CallbackInfo &info) {
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::HandleScope scope(info.Env());
    throwError(info, 1, true, "read error: read() expects argument: (record, err).");
    return;
  }
  uv_work_t *request = new uv_work_t;
  Napi::Function cb = info[0].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env());
  uv_queue_work(uv_default_loop(), request, ReadExecute, ReadComplete);
}

void WrappedVsam::Dealloc(const Napi::CallbackInfo &info) {
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::HandleScope scope(info.Env());
    throwError(info, 0, true,
               "dealloc error: dealloc() expects argument: (err).");
    return;
  }
  if (pVsamFile_ && pVsamFile_->isDatasetOpen()) {
    Napi::HandleScope scope(info.Env());
    throwError(info, 0, false,
               "dalloc error: cannot dealloc an open VSAM dataset, call "
               "close() first.");
    return;
  }
  DCHECK(pVsamFile_ == NULL);
  uv_work_t *request = new uv_work_t;
  Napi::Function cb = info[0].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env(), path_);
  uv_queue_work(uv_default_loop(), request, DeallocExecute, DeallocComplete);
}

void WrappedVsam::FindUpdate(const Napi::CallbackInfo &info) {
  FindUpdate_(info, true);
}

void WrappedVsam::FindUpdate_(const Napi::CallbackInfo &info,
                              bool isCountInCB) {
  /*
   * This is also used by Update if the arguments indicate a find-update,
   * however the user's update() API doesn't require a count arg in its
   * callback (hence isCountInCB=false), while findUpdate() does.
   */
  Napi::HandleScope scope(info.Env());
  int errArg = isCountInCB ? 1 : 0;
  int recArg, cbArg;
  const char *pApiName = isCountInCB ? "findUpdate" : "update";
  if (info.Length() == 3 && info[0].IsString() && info[1].IsObject() &&
      info[2].IsFunction()) {
    recArg = 1;
    cbArg = 2;
  } else if (info.Length() == 4 && info[0].IsObject() && info[1].IsNumber() &&
             info[2].IsObject() && info[3].IsFunction()) {
    recArg = 2;
    cbArg = 3;
  } else {
    throwError(info, errArg, true,
               "%s error: %s() expects arguments: "
               "key-string, record, (count, err), or: "
               "key-buffer, key-buffer-length, record, (count, err).",
               pApiName, pApiName);
    return;
  }

  const Napi::Object &record = info[recArg].ToObject();
  int reclen = pVsamFile_->getRecordLength();
  char *recbuf = (char *)malloc(reclen);
  DCHECK(recbuf != NULL);
  memset(recbuf, 0, reclen);
  char *fldbuf = recbuf;
  std::vector<FieldToUpdate> *pupd = new std::vector<FieldToUpdate>;
  std::vector<LayoutItem> &layout = pVsamFile_->getLayout();
  std::string errmsg;

  for (auto i = layout.begin(); i != layout.end(); ++i) {
    const Napi::Value &field = record.Get(i->name);
    if (field.IsUndefined()) {
      fldbuf += i->maxLength;
      continue;
    }
    if (i->type == LayoutItem::STRING || i->type == LayoutItem::HEXADECIMAL) {
      const std::string &str =
          static_cast<std::string>(Napi::String(info.Env(), field.ToString()));
      if (i->type == LayoutItem::STRING) {
        if (!VsamFile::isStrValid(*i, str, pApiName, errmsg)) {
          throwError(info, errArg, true, errmsg.c_str());
          return;
        }
        DCHECK(str.length() <= i->maxLength);
        if (str.length() > 0)
          memcpy(fldbuf, str.c_str(), str.length());
      } else {
        if (!VsamFile::isHexStrValid(*i, str, pApiName, errmsg)) {
          throwError(info, errArg, true, errmsg.c_str());
          return;
        }
        VsamFile::hexstrToBuffer(fldbuf, i->maxLength, str.c_str());
      }
#ifdef DEBUG
      pupd->push_back(
          FieldToUpdate((int)(fldbuf - recbuf), i->maxLength, i->name));
#else
      pupd->push_back(FieldToUpdate((int)(fldbuf - recbuf), i->maxLength));
#endif
      fldbuf += i->maxLength;
    } else {
      throwError(info, errArg, true, "Error: unexpected data type %d.",
                 i->type);
      return;
    }
  }
  Find(info, __KEY_EQ, "findUpdate", cbArg, FindUpdateExecute,
       isCountInCB ? FindUpdateComplete : DefaultComplete, recbuf, pupd);
}

void WrappedVsam::FindDelete(const Napi::CallbackInfo &info) {
  FindDelete_(info, true);
}

void WrappedVsam::FindDelete_(const Napi::CallbackInfo &info,
                              bool isCountInCB) {
  /*
   * This is also used by Delete if the arguments indicate a find-delete,
   * however the user's delete() API doesn't require a count arg in its
   * callback (hence isCountInCB=false), while findDelete() does.
   */
  Napi::HandleScope scope(info.Env());
  int errArg = isCountInCB ? 1 : 0;
  int recArg, cbArg;
  const char *pApiName = isCountInCB ? "findDelete" : "delete";
  if (info.Length() == 2 && info[0].IsString() && info[1].IsFunction()) {
    cbArg = 1;
  } else if (info.Length() == 3 && info[0].IsObject() && info[1].IsNumber() &&
             info[2].IsFunction()) {
    cbArg = 2;
  } else {
    throwError(info, errArg, true,
               "%s error: %s() expects arguments: "
               "key-string, (count, err), or: "
               "key-buffer, key-buffer-length, (count, err).",
               pApiName, pApiName);
    return;
  }

  Find(info, __KEY_EQ, "findDelete", cbArg, FindDeleteExecute,
       isCountInCB ? FindDeleteComplete : DefaultComplete);
}
