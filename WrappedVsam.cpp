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
                       CbFirstArgType firstArgType, bool isTypeErr,
                       const char *fmt, ...) {
  char msg[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
#ifdef DEBUG
  fprintf(stderr, "%s\n", msg);
#endif
  Napi::Env env = info.Env();
  if (errArgNum >= 0 && firstArgType != ARG0_TYPE_NONE) {
    for (int i = 0; i < info.Length(); i++) {
      if (!info[i].IsFunction())
        continue;
      Napi::Function cb = info[i].As<Napi::Function>();
      if (errArgNum == 0) {
        assert(firstArgType == ARG0_TYPE_ERR);
        cb.Call(env.Global(), {Napi::String::New(env, msg)});
      } else if (errArgNum == 1) {
        if (firstArgType == ARG0_TYPE_NULL)
          cb.Call(env.Global(), {env.Null(), Napi::String::New(env, msg)});
        else if (firstArgType == ARG0_TYPE_0)
          cb.Call(env.Global(),
                  {Napi::Number::New(env, 0), Napi::String::New(env, msg)});
        else
          assert(0);
      } else
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
  DCHECK(pdata->cb_ != nullptr && pdata->env_ != nullptr);
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
  DCHECK(pdata->cb_ != nullptr && pdata->env_ != nullptr);
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

Napi::Value WrappedVsam::createRecordObject(UvWorkData *pdata) {
  if (pdata->recbuf_ == nullptr)
    return pdata->env_.Null();

  Napi::Object record = Napi::Object::New(pdata->env_);
  const char *recbuf = pdata->recbuf_;
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != nullptr);
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
  return record;
}

void WrappedVsam::ReadComplete(uv_work_t *req, int status) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  delete req;

  if (status == UV_ECANCELED) {
    delete pdata;
    return;
  }
  Napi::HandleScope scope(pdata->env_);
  DCHECK(pdata->cb_ != nullptr && pdata->env_ != nullptr);
  if (pdata->rc_ != 0) {
    pdata->cb_.Call(
        pdata->env_.Global(),
        {pdata->env_.Null(), Napi::String::New(pdata->env_, pdata->errmsg_)});
    delete pdata;
    return;
  }
  if (pdata->recbuf_ == nullptr) {
    pdata->cb_.Call(pdata->env_.Global(),
                    {pdata->env_.Null(), pdata->env_.Null()});
    delete pdata;
    return;
  }

  Napi::Value record = createRecordObject(pdata);
  pdata->cb_.Call(pdata->env_.Global(), {record, pdata->env_.Null()});
  delete pdata;
}

void WrappedVsam::FindExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != nullptr);
  obj->routeToVsamThread(MSG_FIND, &VsamFile::FindExecute, pdata);
}

void WrappedVsam::FindUpdateExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != nullptr);
  obj->routeToVsamThread(MSG_FIND_UPDATE, &VsamFile::FindUpdateExecute, pdata);
}

void WrappedVsam::FindDeleteExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != nullptr);
  obj->routeToVsamThread(MSG_FIND_DELETE, &VsamFile::FindDeleteExecute, pdata);
}

void WrappedVsam::ReadExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != nullptr);
  obj->routeToVsamThread(MSG_READ, &VsamFile::ReadExecute, pdata);
}

void WrappedVsam::DeleteExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != nullptr);
  obj->routeToVsamThread(MSG_DELETE, &VsamFile::DeleteExecute, pdata);
}

void WrappedVsam::WriteExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != nullptr);
  obj->routeToVsamThread(MSG_WRITE, &VsamFile::WriteExecute, pdata);
}

void WrappedVsam::UpdateExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  VsamFile *obj = pdata->pVsamFile_;
  DCHECK(obj != nullptr);
  obj->routeToVsamThread(MSG_UPDATE, &VsamFile::UpdateExecute, pdata);
}

void WrappedVsam::DeallocExecute(uv_work_t *req) {
  UvWorkData *pdata = (UvWorkData *)(req->data);
  DCHECK(pdata->pVsamFile_ == nullptr);
  DCHECK(pdata->path_.length() > 0);
  VsamFile::DeallocExecute(pdata);
}

WrappedVsam::WrappedVsam(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<WrappedVsam>(info) {
  if (info.Length() != 6) {
    Napi::HandleScope scope(info.Env());
    throwError(
        info, -1, ARG0_TYPE_NONE, true, //-1 throws an exception, not callback
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
  pVsamFile_ = nullptr;
}

Napi::Object WrappedVsam::Init(Napi::Env env, Napi::Object exports) {
  Napi::HandleScope scope(env);

  Napi::Function func =
      DefineClass(env, "WrappedVsam",
                  {InstanceMethod("read", &WrappedVsam::Read),
                   InstanceMethod("readSync", &WrappedVsam::ReadSync),
                   InstanceMethod("find", &WrappedVsam::FindEq),
                   InstanceMethod("findSync", &WrappedVsam::FindEqSync),
                   InstanceMethod("findeqSync", &WrappedVsam::FindEqSync),
                   InstanceMethod("findge", &WrappedVsam::FindGe),
                   InstanceMethod("findgeSync", &WrappedVsam::FindGeSync),
                   InstanceMethod("findfirst", &WrappedVsam::FindFirst),
                   InstanceMethod("findfirstSync", &WrappedVsam::FindFirstSync),
                   InstanceMethod("findlast", &WrappedVsam::FindLast),
                   InstanceMethod("findlastSync", &WrappedVsam::FindLastSync),
                   InstanceMethod("update", &WrappedVsam::Update),
                   InstanceMethod("updateSync", &WrappedVsam::UpdateSync),
                   InstanceMethod("write", &WrappedVsam::Write),
                   InstanceMethod("writeSync", &WrappedVsam::WriteSync),
                   InstanceMethod("delete", &WrappedVsam::Delete),
                   InstanceMethod("deleteSync", &WrappedVsam::DeleteSync),
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
          ? "rb+,type=record"
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
      throwError(info, -1, ARG0_TYPE_NONE, false,
                 "%s error in JSON: item %d is empty.", pApiName, i + 1);
      return env.Null().ToObject();
    }

    int minLength = 0; // minLength is optional, default 0 unless it's a key
    if (item.Has("minLength")) {
      const Napi::Value &vminLength = item.Get("minLength");
      if (!vminLength.IsEmpty() && !vminLength.IsNumber()) {
        throwError(
            info, -1, ARG0_TYPE_NONE, true,
            "%s error in JSON (item %d): minLength value must be numeric.",
            pApiName, i + 1);
        return env.Null().ToObject();
      } else if (!vminLength.IsEmpty()) {
        minLength = vminLength.ToNumber().Int32Value();
        if (minLength < 0) {
          throwError(
              info, -1, ARG0_TYPE_NONE, true,
              "%s error in JSON (item %d): minLength value cannot be negative.",
              pApiName, i + 1);
          return env.Null().ToObject();
        }
      }
    }

    int maxLength = 0;
    if (!item.Has("maxLength")) {
      throwError(info, -1, ARG0_TYPE_NONE, true,
                 "%s error in JSON (item %d): maxLength must be specified.",
                 pApiName, i + 1);
      return env.Null().ToObject();
    }
    const Napi::Value &vmaxLength = item.Get("maxLength");
    if (!vmaxLength.IsNumber()) {
      throwError(info, -1, ARG0_TYPE_NONE, true,
                 "%s error in JSON (item %d): maxLength must be numeric.",
                 pApiName, i + 1);
      return env.Null().ToObject();
    } else {
      maxLength = vmaxLength.ToNumber().Int32Value();
      if (maxLength <= 0) {
        throwError(info, -1, ARG0_TYPE_NONE, true,
                   "%s error in JSON (item %d): maxLength value must be "
                   "greater than 0.",
                   pApiName, i + 1);
        return env.Null().ToObject();
      }
    }

    if (minLength > maxLength) {
      throwError(info, -1, ARG0_TYPE_NONE, true,
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
        throwError(info, -1, ARG0_TYPE_NONE, true,
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
          info, -1, ARG0_TYPE_NONE, true,
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
      throwError(info, -1, ARG0_TYPE_NONE, true,
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
    throwError(info, -1, ARG0_TYPE_NONE, false,
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
    throwError(info, -1, ARG0_TYPE_NONE, true,
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
    throwError(info, -1, ARG0_TYPE_NONE, true,
               "openSync error: openSync() expects arguments: VSAM dataset "
               "name, schema JSON object, optional fopen() mode.");
    return info.Env().Null().ToObject();
  }
  return Construct(info, false);
}

// static
Napi::Boolean WrappedVsam::Exist(const Napi::CallbackInfo &info) {
  if (info.Length() != 1 || !info[0].IsString()) {
    Napi::HandleScope scope(info.Env());
    throwError(info, -1, ARG0_TYPE_NONE, true,
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
  pVsamFile_->routeToVsamThread(MSG_CLOSE, &VsamFile::Close, &uvdata);

  if (uvdata.rc_) {
    Napi::HandleScope scope(info.Env());
    throwError(info, 0, ARG0_TYPE_ERR, false, uvdata.errmsg_.c_str());
    return;
  }
#ifdef DEBUG
  fprintf(stderr, "Close deleteVsamFileObj...\n");
#endif
  deleteVsamFileObj();
}

void WrappedVsam::Delete(const Napi::CallbackInfo &info) {
  if (info.Length() == 2 && info[0].IsString() && info[1].IsFunction()) {
    FindDelete_(info, "delete", nullptr, 1); // callback arg #
  } else if (info.Length() == 3 && info[0].IsObject() && info[1].IsNumber() &&
             info[2].IsFunction()) {
    FindDelete_(info, "delete", nullptr, 2);
  } else if (info.Length() == 1 && info[0].IsFunction())
    Delete_(info);
  else {
    Napi::HandleScope scope(info.Env());
    throwError(info, 0, ARG0_TYPE_ERR, true,
               "delete error: delete() expects arguments: "
               "(err), or: "
               "key-string, (count, err), or: "
               "key-buffer, key-buffer-length, (count, err).");
  }
}

int WrappedVsam::Delete_(const Napi::CallbackInfo &info, UvWorkData **ppdata) {
  Napi::HandleScope scope(info.Env());

  if (ppdata != nullptr) {
    // called for a sync API
    Napi::Function dummycb;
    *ppdata = new UvWorkData(pVsamFile_, dummycb, info.Env());
    return 0;
  }
  uv_work_t *request = new uv_work_t;
  Napi::Function cb = info[0].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env());
  uv_queue_work(uv_default_loop(), request, DeleteExecute, DeleteComplete);
  return 0;
}

Napi::Value WrappedVsam::DeleteSync(const Napi::CallbackInfo &info) {
  Napi::HandleScope scope(info.Env());
  UvWorkData *pdata = nullptr;
  int rc;
  bool findDelete = true;

  if ((info.Length() == 1 && info[0].IsString()) ||
      (info.Length() == 2 && info[0].IsObject() && info[1].IsNumber())) {
    if (FindDelete_(info, "deleteSync", &pdata))
      return Napi::Number::New(info.Env(), 0);
  } else if (info.Length() == 0) {
    if (Delete_(info, &pdata))
      return Napi::Number::New(info.Env(), 0);
    findDelete = false;
  } else {
    throwError(info, -1, ARG0_TYPE_NONE, true,
               "deleteSync error: deleteSync() expects arguments: "
               "key-string, "
               "or: key-buffer, key-buffer-length.");
    return Napi::Number::New(info.Env(), 0);
  }

  assert(pdata != nullptr);

  if (findDelete) {
    rc = pVsamFile_->routeToVsamThread(MSG_FIND_DELETE,
                                       &VsamFile::FindDeleteExecute, pdata);
    if (rc || pdata->rc_) {
      if (pdata->rc_ == 8) { // no record found
        delete pdata;
        return Napi::Number::New(info.Env(), 0);
      }
      throwError(info, -1, ARG0_TYPE_NONE, true, pdata->errmsg_.c_str());
      delete pdata;
      return Napi::Number::New(info.Env(), 0);
    }
    int count = pdata->count_;
    delete pdata;
    return Napi::Number::New(info.Env(), count);
  } else {
    rc = pVsamFile_->routeToVsamThread(MSG_DELETE, &VsamFile::DeleteExecute,
                                       pdata);
    if (rc || pdata->rc_) {
      throwError(info, -1, ARG0_TYPE_NONE, true, pdata->errmsg_.c_str());
      delete pdata;
      return Napi::Number::New(info.Env(), 0);
    }
    delete pdata;
    return Napi::Number::New(info.Env(), 1);
  }
}

int WrappedVsam::Write_(const Napi::CallbackInfo &info, const char *pApiName,
                        UvWorkData **ppdata) {
  Napi::HandleScope scope(info.Env());

  const Napi::Object &record = info[0].ToObject();
  int reclen = pVsamFile_->getRecordLength();
  char *recbuf = (char *)malloc(reclen);
  DCHECK(recbuf != nullptr && reclen > 0);
  memset(recbuf, 0, reclen);
  char *fldbuf = recbuf;
  std::vector<LayoutItem> &layout = pVsamFile_->getLayout();
  std::string errmsg;
  CbFirstArgType firstArgType =
      ppdata == nullptr ? ARG0_TYPE_ERR : ARG0_TYPE_NONE;

  for (auto i = layout.begin(); i != layout.end(); ++i) {
    const Napi::Value &field = record.Get(i->name);

    if (i->type == LayoutItem::STRING || i->type == LayoutItem::HEXADECIMAL) {
#ifdef DEBUG
      if (field.IsUndefined()) {
        fprintf(stderr,
                "%s value of %s was not set, will attempt to set it to "
                "all 0x00\n",
                pApiName, i->name.c_str());
      }
#endif
      const std::string &str =
          field.IsUndefined() ? ""
                              : static_cast<std::string>(
                                    Napi::String(info.Env(), field.ToString()));
      if (i->type == LayoutItem::STRING) {
        if (!VsamFile::isStrValid(*i, str, pApiName, errmsg)) {
          throwError(info, 0, firstArgType, true, errmsg.c_str());
          return -1;
        }
        DCHECK(str.length() <= i->maxLength);
        DCHECK((fldbuf - recbuf) + str.length() <= reclen);
        if (str.length() > 0)
          memcpy(fldbuf, str.c_str(), str.length());
      } else {
        if (!VsamFile::isHexStrValid(*i, str, pApiName, errmsg)) {
          throwError(info, 0, firstArgType, true, errmsg.c_str());
          return -1;
        }
        DCHECK((fldbuf - recbuf) + i->maxLength <= reclen);
        VsamFile::hexstrToBuffer(fldbuf, i->maxLength, str.c_str());
      }
    } else {
      throwError(info, 0, firstArgType, true,
                 "%s error: unexpected JSON data type %d for %s.", pApiName,
                 i->type, i->name.c_str());
      return -1;
    }
    fldbuf += i->maxLength;
  }

  if (ppdata != nullptr) {
    // called for a sync API
    Napi::Function dummycb;
    *ppdata = new UvWorkData(pVsamFile_, dummycb, info.Env(), "", recbuf);
    return 0;
  }
  uv_work_t *request = new uv_work_t;
  Napi::Function cb = info[1].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env(), "", recbuf);
  uv_queue_work(uv_default_loop(), request, WriteExecute, WriteComplete);
  return 0;
}

void WrappedVsam::Write(const Napi::CallbackInfo &info) {
  if (info.Length() == 2 && info[0].IsObject() && info[1].IsFunction())
    Write_(info, "write");
  else {
    Napi::HandleScope scope(info.Env());
    throwError(info, 0, ARG0_TYPE_ERR, true,
               "write error: write() expects arguments: record, (err).");
  }
}

Napi::Value WrappedVsam::WriteSync(const Napi::CallbackInfo &info) {
  Napi::HandleScope scope(info.Env());
  UvWorkData *pdata = nullptr;

  if (info.Length() == 1 && info[0].IsObject()) {
    if (Write_(info, "writeSync", &pdata))
      return Napi::Number::New(info.Env(), 0);
  } else {
    throwError(info, -1, ARG0_TYPE_NONE, true,
               "writeSync error: writeSync() expects arguments: record.");
    return Napi::Number::New(info.Env(), 0);
  }
  int rc =
      pVsamFile_->routeToVsamThread(MSG_WRITE, &VsamFile::WriteExecute, pdata);
  if (rc || pdata->rc_) {
    throwError(info, -1, ARG0_TYPE_NONE, true, pdata->errmsg_.c_str());
    delete pdata;
    return Napi::Number::New(info.Env(), 0);
  }
  delete pdata;
  return Napi::Number::New(info.Env(), 1);
}

void WrappedVsam::Update(const Napi::CallbackInfo &info) {
  if (info.Length() == 3 && info[0].IsString() && info[1].IsObject() &&
      info[2].IsFunction()) {
    FindUpdate_(info, "update", nullptr, 1, 2); // record and callback arg #s
  } else if (info.Length() == 4 && info[0].IsObject() && info[1].IsNumber() &&
             info[2].IsObject() && info[3].IsFunction()) {
    FindUpdate_(info, "update", nullptr, 2, 3); // record and callbak>ck arg #s
  } else if (info.Length() == 2 && info[0].IsObject() && info[1].IsFunction())
    Update_(info, "update");
  else {
    Napi::HandleScope scope(info.Env());
    throwError(info, 0, ARG0_TYPE_ERR, true,
               "update error: update() expects arguments: "
               "record, (err), "
               "or: key-string, record, (count, err), "
               "or: key-buffer, key-buffer-length, record, (count, err).");
  }
}

int WrappedVsam::Update_(const Napi::CallbackInfo &info, const char *pApiName,
                         UvWorkData **ppdata) {
  Napi::HandleScope scope(info.Env());

  const Napi::Object &record = info[0].ToObject();
  int reclen = pVsamFile_->getRecordLength();
  char *recbuf = (char *)malloc(reclen);
  DCHECK(recbuf != nullptr);
  memset(recbuf, 0, reclen);
  char *fldbuf = recbuf;
  std::vector<LayoutItem> &layout = pVsamFile_->getLayout();
  std::string errmsg;
  CbFirstArgType firstArgType =
      ppdata == nullptr ? ARG0_TYPE_ERR : ARG0_TYPE_NONE;

  for (auto i = layout.begin(); i != layout.end(); ++i) {
    const Napi::Value &field = record.Get(i->name);
    if (field.IsUndefined()) {
      throwError(info, 0, firstArgType, true,
                 "%s error: update value for %s has not been set.", pApiName,
                 i->name.c_str());
      return -1;
    }
    if (i->type == LayoutItem::STRING || i->type == LayoutItem::HEXADECIMAL) {
      const std::string &str =
          static_cast<std::string>(Napi::String(info.Env(), field.ToString()));
      if (i->type == LayoutItem::STRING) {
        if (!VsamFile::isStrValid(*i, str, pApiName, errmsg)) {
          throwError(info, 0, firstArgType, true, errmsg.c_str());
          return -1;
        }
        DCHECK(str.length() <= i->maxLength);
        if (str.length() > 0)
          memcpy(fldbuf, str.c_str(), str.length());
      } else {
        if (!VsamFile::isHexStrValid(*i, str, pApiName, errmsg)) {
          throwError(info, 0, firstArgType, true, errmsg.c_str());
          return -1;
        }
        VsamFile::hexstrToBuffer(fldbuf, i->maxLength, str.c_str());
      }
      fldbuf += i->maxLength;
    } else {
      throwError(info, 0, firstArgType, true,
                 "%s error: unexpected data type %d for %s.", pApiName, i->type,
                 i->name.c_str());
      return -1;
    }
  }

  if (ppdata != nullptr) {
    // called for a sync API
    Napi::Function dummycb;
    *ppdata = new UvWorkData(pVsamFile_, dummycb, info.Env(), "", recbuf);
    return 0;
  }
  uv_work_t *request = new uv_work_t;
  Napi::Function cb = info[1].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env(), "", recbuf);
  uv_queue_work(uv_default_loop(), request, UpdateExecute, UpdateComplete);
  return 0;
}

Napi::Value WrappedVsam::UpdateSync(const Napi::CallbackInfo &info) {
  Napi::HandleScope scope(info.Env());
  UvWorkData *pdata = nullptr;
  int rc;
  bool findUpdate = true;

  if (info.Length() == 2 && info[0].IsString() && info[1].IsObject()) {
    if (FindUpdate_(info, "updateSync", &pdata, 1,
                    -1)) // record and callback arg #s
      return Napi::Number::New(info.Env(), 0);
  } else if (info.Length() == 3 && info[0].IsObject() && info[1].IsNumber() &&
             info[2].IsObject()) {
    if (FindUpdate_(info, "updateSync", &pdata, 2, -1))
      return Napi::Number::New(info.Env(), 0);
  } else if (info.Length() == 1 && info[0].IsObject()) {
    if (Update_(info, "updateSync", &pdata))
      return Napi::Number::New(info.Env(), 0);
    findUpdate = false;
  } else {
    throwError(info, -1, ARG0_TYPE_NONE, true,
               "updateSync error: updateSync() expects arguments: "
               "record, "
               "or: key-string, record, "
               "or: key-buffer, key-buffer-length, record.");
    return Napi::Number::New(info.Env(), 0);
  }

  assert(pdata != nullptr);

  if (findUpdate) {
    rc = pVsamFile_->routeToVsamThread(MSG_FIND_UPDATE,
                                       &VsamFile::FindUpdateExecute, pdata);
    if (rc || pdata->rc_) {
      if (pdata->rc_ == 8) { // no record found
        delete pdata;
        return Napi::Number::New(info.Env(), 0);
      }
      throwError(info, -1, ARG0_TYPE_NONE, true, pdata->errmsg_.c_str());
      delete pdata;
      return Napi::Number::New(info.Env(), 0);
    }
    int count = pdata->count_;
    delete pdata;
    return Napi::Number::New(info.Env(), count);
  } else {
    rc = pVsamFile_->routeToVsamThread(MSG_UPDATE, &VsamFile::UpdateExecute,
                                       pdata);
    if (rc || pdata->rc_) {
      throwError(info, -1, ARG0_TYPE_NONE, true, pdata->errmsg_.c_str());
      delete pdata;
      return Napi::Number::New(info.Env(), 0);
    }
    delete pdata;
    return Napi::Number::New(info.Env(), 1);
  }
}

void WrappedVsam::FindEq(const Napi::CallbackInfo &info) {
  if (info.Length() == 2 && info[0].IsString() && info[1].IsFunction())
    Find(info, __KEY_EQ, "find", 1);
  else if (info.Length() == 3 && info[0].IsObject() && info[1].IsNumber() &&
           info[2].IsFunction())
    Find(info, __KEY_EQ, "find", 2);
  else {
    Napi::HandleScope scope(info.Env());
    throwError(info, 1, ARG0_TYPE_NULL, true,
               "find error: find() expects arguments: "
               "key-string, (record, err), "
               "or key-buffer, key-buffer-length, (record, err).");
  }
}

Napi::Value WrappedVsam::FindSync_(const Napi::CallbackInfo &info,
                                   UvWorkData *pdata) {
  assert(pdata != nullptr);
  int rc =
      pVsamFile_->routeToVsamThread(MSG_FIND, &VsamFile::FindExecute, pdata);
  if (rc || pdata->rc_) {
    if (pdata->rc_ == 8) { // no record found
      delete pdata;
      return info.Env().Null();
    }
    throwError(info, -1, ARG0_TYPE_NONE, true, pdata->errmsg_.c_str());
    delete pdata;
    return info.Env().Null();
  }
  Napi::Value record = createRecordObject(pdata);
  delete pdata;
  return record;
}

Napi::Value WrappedVsam::FindEqSync(const Napi::CallbackInfo &info) {
  Napi::HandleScope scope(info.Env());
  UvWorkData *pdata = nullptr;
  int rc;
  if ((info.Length() == 2 && info[0].IsObject() && info[1].IsNumber()) ||
      (info.Length() == 1 && info[0].IsString())) {
    if (Find(info, __KEY_EQ, "findSync", -1, &pdata, ARG0_TYPE_NONE))
      return info.Env().Null();
  } else {
    throwError(info, -1, ARG0_TYPE_NONE, true,
               "findSync error: findSync() expects arguments: "
               "key-string, "
               "or key-buffer, key-buffer-length.");
    return info.Env().Null();
  }
  return FindSync_(info, pdata);
}

void WrappedVsam::FindGe(const Napi::CallbackInfo &info) {
  if (info.Length() == 2 && info[0].IsString() && info[1].IsFunction()) {
    Find(info, __KEY_GE, "findge", 1);
  } else if (info.Length() == 3 && info[0].IsObject() && info[1].IsNumber() &&
             info[2].IsFunction()) {
    Find(info, __KEY_GE, "findge", 2);
  } else {
    Napi::HandleScope scope(info.Env());
    throwError(info, 1, ARG0_TYPE_NULL, true,
               "findge error: findge() expects arguments: "
               "or key-string, (record, err), "
               "or key-buffer, key-buffer-length, (record, err).");
  }
}

Napi::Value WrappedVsam::FindGeSync(const Napi::CallbackInfo &info) {
  Napi::HandleScope scope(info.Env());
  UvWorkData *pdata = nullptr;
  if ((info.Length() == 2 && info[0].IsObject() && info[1].IsNumber()) ||
      (info.Length() == 1 && info[0].IsString())) {
    if (Find(info, __KEY_GE, "findgeSync", -1, &pdata, ARG0_TYPE_NONE))
      return info.Env().Null();
  } else {
    throwError(info, -1, ARG0_TYPE_NONE, true,
               "findgeSync error: findgeSync() expects arguments: "
               "key-string, "
               "or key-buffer, key-buffer-length.");
    return info.Env().Null();
  }
  return FindSync_(info, pdata);
}

void WrappedVsam::FindFirst(const Napi::CallbackInfo &info) {
  if (info.Length() == 1 && info[0].IsFunction())
    Find(info, __KEY_FIRST, "findfirst", 0);
  else {
    Napi::HandleScope scope(info.Env());
    throwError(info, 1, ARG0_TYPE_NULL, true,
               "findfirst error: findfirst() expects argument: (record, err).");
  }
}

Napi::Value WrappedVsam::FindFirstSync(const Napi::CallbackInfo &info) {
  UvWorkData *pdata = nullptr;
  if (info.Length() == 0) {
    if (Find(info, __KEY_FIRST, "findfirstSync", -1, &pdata, ARG0_TYPE_NONE))
      return info.Env().Null();
  } else {
    Napi::HandleScope scope(info.Env());
    throwError(info, 1, ARG0_TYPE_NULL, true,
               "findfirstSync error: findfirstSync() expects no argument.");
    return info.Env().Null();
  }
  return FindSync_(info, pdata);
}

void WrappedVsam::FindLast(const Napi::CallbackInfo &info) {
  if (info.Length() == 1 && info[0].IsFunction())
    Find(info, __KEY_LAST, "findlast", 0);
  else {
    Napi::HandleScope scope(info.Env());
    throwError(info, 1, ARG0_TYPE_NULL, true,
               "findlast error: findlast() expects argument: (record, err).");
  }
}

Napi::Value WrappedVsam::FindLastSync(const Napi::CallbackInfo &info) {
  UvWorkData *pdata = nullptr;
  if (info.Length() == 0) {
    if (Find(info, __KEY_LAST, "findlastSync", -1, &pdata, ARG0_TYPE_NONE))
      return info.Env().Null();
  } else {
    Napi::HandleScope scope(info.Env());
    throwError(info, 1, ARG0_TYPE_NULL, true,
               "findfirstSync error: findlastSync() expects no argument.");
    return info.Env().Null();
  }
  return FindSync_(info, pdata);
}

int WrappedVsam::Find(const Napi::CallbackInfo &info, int equality,
                      const char *pApiName, int callbackArg,
                      UvWorkData **ppdata, CbFirstArgType firstArgType,
                      uv_work_cb pExecuteFunc, uv_after_work_cb pCompleteFunc,
                      char *pUpdateRecBuf,
                      std::vector<FieldToUpdate> *pFieldsToUpdate) {
  Napi::HandleScope scope(info.Env());
  std::string key;
  char *keybuf = nullptr;
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
          throwError(info, 1, firstArgType, true, errmsg.c_str());
          return -1;
        }
        keybuf = (char *)malloc(layout[key_i].maxLength);
        keybuf_len = VsamFile::hexstrToBuffer(keybuf, layout[key_i].maxLength,
                                              key.c_str());
      } else {
        if (!VsamFile::isStrValid(layout[key_i], key, pApiName, errmsg)) {
          throwError(info, 1, firstArgType, true, errmsg.c_str());
          return -1;
        }
        keybuf = (char *)malloc(layout[key_i].maxLength);
        keybuf_len = key.length();
        memcpy(keybuf, key.c_str(), keybuf_len);
      }
    } else if (info[0].IsObject()) {
      const char *ubuf = info[0].As<Napi::Buffer<char>>().Data();
      if (!info[1].IsNumber()) {
        throwError(info, 1, firstArgType, true,
                   "%s error: buffer argument must be followed by its length.",
                   pApiName);
        return -1;
      }
      keybuf_len = info[1].As<Napi::Number>().Uint32Value();
      if (!VsamFile::isHexBufValid(layout[key_i], ubuf, keybuf_len, pApiName,
                                   errmsg)) {
        throwError(info, 1, firstArgType, true, errmsg.c_str());
        return -1;
      }
      DCHECK(keybuf_len > 0);
      keybuf = (char *)malloc(keybuf_len);
      DCHECK(keybuf != nullptr);
      memcpy(keybuf, ubuf, keybuf_len);
    } else {
      throwError(info, 1, firstArgType, true,
                 "%s error: first argument must be "
                 "either a string or a Buffer object.",
                 pApiName);
      return -1;
    }
  }
  if (ppdata != nullptr) {
    // called for a sync API
    Napi::Function dummycb;
    *ppdata = new UvWorkData(pVsamFile_, dummycb, info.Env(), "", pUpdateRecBuf,
                             keybuf, keybuf_len, equality, pFieldsToUpdate);
    return 0;
  }

  uv_work_t *request = new uv_work_t;
  Napi::Function cb = info[callbackArg].As<Napi::Function>();

  request->data = new UvWorkData(pVsamFile_, cb, info.Env(), "", pUpdateRecBuf,
                                 keybuf, keybuf_len, equality, pFieldsToUpdate);
  uv_queue_work(uv_default_loop(), request, pExecuteFunc, pCompleteFunc);
  return 0;
}

void WrappedVsam::Read(const Napi::CallbackInfo &info) {
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::HandleScope scope(info.Env());
    throwError(info, 1, ARG0_TYPE_NULL, true,
               "read error: read() expects argument: (record, err).");
    return;
  }
  uv_work_t *request = new uv_work_t;
  Napi::Function cb = info[0].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env());
  uv_queue_work(uv_default_loop(), request, ReadExecute, ReadComplete);
}

Napi::Value WrappedVsam::ReadSync(const Napi::CallbackInfo &info) {
  Napi::HandleScope scope(info.Env());
  if (info.Length() != 0) {
    throwError(info, -1, ARG0_TYPE_NONE, true,
               "readSync error: readSync() expects no argument.");
    return info.Env().Null();
  }
  Napi::Function dummycb;
  UvWorkData *pdata = new UvWorkData(pVsamFile_, dummycb, info.Env());
  int rc =
      pVsamFile_->routeToVsamThread(MSG_READ, &VsamFile::ReadExecute, pdata);
  if (rc || pdata->rc_) {
    throwError(info, -1, ARG0_TYPE_NONE, true, pdata->errmsg_.c_str());
    delete pdata;
    return info.Env().Null();
  }
  Napi::Value record = createRecordObject(pdata);
  delete pdata;
  return record;
}

void WrappedVsam::Dealloc(const Napi::CallbackInfo &info) {
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::HandleScope scope(info.Env());
    throwError(info, -1, ARG0_TYPE_NONE, true,
               "dealloc error: dealloc() expects argument: (err).");
    return;
  }
  if (pVsamFile_ && pVsamFile_->isDatasetOpen()) {
    Napi::HandleScope scope(info.Env());
    throwError(info, 0, ARG0_TYPE_ERR, false,
               "dalloc error: cannot dealloc an open VSAM dataset, call "
               "close() first.");
    return;
  }
  DCHECK(pVsamFile_ == nullptr);
  uv_work_t *request = new uv_work_t;
  Napi::Function cb = info[0].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env(), path_);
  uv_queue_work(uv_default_loop(), request, DeallocExecute, DeallocComplete);
}

int WrappedVsam::FindUpdate_(const Napi::CallbackInfo &info,
                             const char *pApiName, UvWorkData **ppdata,
                             const int recArg, const int cbArg) {
  // This is used by update, updateSync, find-update and find-update-sync;
  // update or find-update is determined by the arguments;
  // sync is if ppdata != nullptr
  Napi::HandleScope scope(info.Env());
  const int errArg = 1;
  CbFirstArgType firstArgType =
      ppdata == nullptr ? ARG0_TYPE_0 : ARG0_TYPE_NONE;

  const Napi::Object &record = info[recArg].ToObject();
  int reclen = pVsamFile_->getRecordLength();
  char *recbuf = (char *)malloc(reclen);
  DCHECK(recbuf != nullptr);
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
          throwError(info, errArg, firstArgType, true, errmsg.c_str());
          return -1;
        }
        DCHECK(str.length() <= i->maxLength);
        if (str.length() > 0)
          memcpy(fldbuf, str.c_str(), str.length());
      } else {
        if (!VsamFile::isHexStrValid(*i, str, pApiName, errmsg)) {
          throwError(info, errArg, firstArgType, true, errmsg.c_str());
          return -1;
        }
        VsamFile::hexstrToBuffer(fldbuf, i->maxLength, str.c_str());
      }
#ifdef DEBUG
      pupd->push_back(FieldToUpdate((int)(fldbuf - recbuf), i->maxLength,
                                    i->name, i->type));
#else
      pupd->push_back(FieldToUpdate((int)(fldbuf - recbuf), i->maxLength));
#endif
      fldbuf += i->maxLength;
    } else {
      throwError(info, errArg, firstArgType, true,
                 "Error: unexpected data type %d.", i->type);
      return -1;
    }
  }
  return Find(info, __KEY_EQ, pApiName, cbArg, ppdata, firstArgType,
              FindUpdateExecute, FindUpdateComplete, recbuf, pupd);
}

int WrappedVsam::FindDelete_(const Napi::CallbackInfo &info,
                             const char *pApiName, UvWorkData **ppdata,
                             const int cbArg) {
  // This is used by delete, deleteSync, find-delete and find-delete-sync;
  // delete or find-delete is determined by the arguments;
  // sync is if ppdata != nullptr
  CbFirstArgType firstArgType =
      ppdata == nullptr ? ARG0_TYPE_0 : ARG0_TYPE_NONE;

  Find(info, __KEY_EQ, pApiName, cbArg, ppdata, firstArgType, FindDeleteExecute,
       FindDeleteComplete);
  return 0;
}
