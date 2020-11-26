/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2017. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
*/
#include "WrappedVsam.h"
#include <unistd.h>
#include <sstream>

Napi::FunctionReference WrappedVsam::constructor_;

static void throwError(Napi::Env env, const char* fmt, ...)
{
  char msg[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
#ifdef DEBUG
  fprintf(stderr,"%s\n",msg);
#endif
  Napi::Error::New(env, msg).ThrowAsJavaScriptException();
}


void WrappedVsam::DefaultComplete(uv_work_t* req, int status) {
  UvWorkData *pdata = (UvWorkData*)(req->data);
  delete req;

  if (status == UV_ECANCELED) {
    delete pdata;
    return;
  }
  Napi::HandleScope scope(pdata->env_);
  assert(pdata->cb_ != NULL && pdata->env_ != NULL);
  if (pdata->rc_ != 0)
    pdata->cb_.Call(pdata->env_.Global(), {Napi::String::New(pdata->env_, pdata->errmsg_)});
  else
    pdata->cb_.Call(pdata->env_.Global(), {pdata->env_.Null()});
  delete pdata;
}


void WrappedVsam::DeleteComplete(uv_work_t* req, int status) {
  return DefaultComplete(req, status);
}


void WrappedVsam::WriteComplete(uv_work_t* req, int status) {
  return DefaultComplete(req, status);
}


void WrappedVsam::UpdateComplete(uv_work_t* req, int status) {
  return DefaultComplete(req, status);
}


void WrappedVsam::DeallocComplete(uv_work_t* req, int status) {
  return DefaultComplete(req, status);
}


void WrappedVsam::ReadComplete(uv_work_t* req, int status) {
  UvWorkData *pdata = (UvWorkData*)(req->data);
  delete req;

  if (status == UV_ECANCELED) {
    delete pdata;
    return;
  }
  Napi::HandleScope scope(pdata->env_);
  assert(pdata->cb_ != NULL && pdata->env_ != NULL);
  if (pdata->rc_ != 0) {
    pdata->cb_.Call(pdata->env_.Global(), {pdata->env_.Null(), Napi::String::New(pdata->env_, pdata->errmsg_)});
    delete pdata;
    return;
  }
  if (pdata->recbuf_ == NULL) {
    pdata->cb_.Call(pdata->env_.Global(), { pdata->env_.Null(), pdata->env_.Null()});
    delete pdata;
    return;
  }

  Napi::Object record = Napi::Object::New(pdata->env_);
  const char *recbuf = pdata->recbuf_;
  VsamFile *obj = pdata->pVsamFile_;
  assert(obj != NULL);
  std::vector<LayoutItem>& layout = obj->getLayout();

  for(auto i = layout.begin(); i != layout.end(); ++i) {
    if (i->type == LayoutItem::STRING) { 
      std::string str(recbuf,i->maxLength+1);
      str[i->maxLength] = 0;
      record.Set(i->name, Napi::String::New(pdata->env_, str.c_str()));
    }
    else if (i->type == LayoutItem::HEXADECIMAL) { 
      char hexstr[(i->maxLength*2)+1];
      VsamFile::bufferToHexstr(hexstr, recbuf, i->maxLength);
      record.Set(i->name, Napi::String::New(pdata->env_, hexstr));
    }
    recbuf += i->maxLength;
  }
  pdata->cb_.Call(pdata->env_.Global(), {record, pdata->env_.Null()});
  delete pdata;
}


void WrappedVsam::FindExecute(uv_work_t* req) {
  UvWorkData *pdata = (UvWorkData*)(req->data);
  VsamFile* obj = pdata->pVsamFile_;
  assert (obj != NULL);
  obj->FindExecute(pdata);
}


void WrappedVsam::ReadExecute(uv_work_t* req) {
  UvWorkData *pdata = (UvWorkData*)(req->data);
  VsamFile* obj = pdata->pVsamFile_;
  assert (obj != NULL);
  obj->ReadExecute(pdata);
}


void WrappedVsam::DeleteExecute(uv_work_t* req) {
  UvWorkData *pdata = (UvWorkData*)(req->data);
  VsamFile* obj = pdata->pVsamFile_;
  assert (obj != NULL);
  obj->DeleteExecute(pdata);
}


void WrappedVsam::WriteExecute(uv_work_t* req) {
  UvWorkData *pdata = (UvWorkData*)(req->data);
  VsamFile* obj = pdata->pVsamFile_;
  assert (obj != NULL);
  obj->WriteExecute(pdata);
}


void WrappedVsam::UpdateExecute(uv_work_t* req) {
  UvWorkData *pdata = (UvWorkData*)(req->data);
  VsamFile* obj = pdata->pVsamFile_;
  assert (obj != NULL);
  obj->UpdateExecute(pdata);
}


void WrappedVsam::DeallocExecute(uv_work_t* req) {
  UvWorkData *pdata = (UvWorkData*)(req->data);
  VsamFile* obj = pdata->pVsamFile_;
  assert (obj != NULL);
  obj->DeallocExecute(pdata);
}


WrappedVsam::WrappedVsam(const Napi::CallbackInfo& info)
: Napi::ObjectWrap<WrappedVsam>(info) {
#ifdef DEBUG
  fprintf(stderr,"In WrappedVsam constructor.\n");
#endif
  if (info.Length() != 5) {
    Napi::HandleScope scope(info.Env());
    throwError(info.Env(), "Error: wrong number of arguments to WrappedVsam constructor: got %d, expected 5.", info.Length());
    return;
  }

  std::string path = static_cast<std::string>(info[0].As<Napi::String>());
  Napi::Buffer<std::vector<LayoutItem>> b = info[1].As<Napi::Buffer<std::vector<LayoutItem>>>();
  std::vector<LayoutItem> layout = *(static_cast<std::vector<LayoutItem>*>(b.Data()));
  int key_i = static_cast<int>(info[2].As<Napi::Number>().Int32Value());
  std::string omode = static_cast<std::string>(info[3].As<Napi::String>());
  bool alloc(static_cast<bool>(info[4].As<Napi::Boolean>()));

  pVsamFile_ = new VsamFile(path, layout, key_i, omode, alloc);
}


WrappedVsam::~WrappedVsam() {
#ifdef DEBUG
  fprintf(stderr,"In WrappedVsam destructor this=%p, pVsamFile_=%p.\n", this, pVsamFile_);
#endif
  if (pVsamFile_) {
    delete pVsamFile_;
    pVsamFile_ = NULL;
  }
}

Napi::Object WrappedVsam::Init(Napi::Env env, Napi::Object exports) {
  Napi::HandleScope scope(env);

  Napi::Function func = DefineClass(env, "WrappedVsam", {
    InstanceMethod("read", &WrappedVsam::Read),
    InstanceMethod("find", &WrappedVsam::FindEq),
    InstanceMethod("findeq", &WrappedVsam::FindEq),
    InstanceMethod("findge", &WrappedVsam::FindGe),
    InstanceMethod("findfirst", &WrappedVsam::FindFirst),
    InstanceMethod("findlast", &WrappedVsam::FindLast),
    InstanceMethod("update", &WrappedVsam::Update),
    InstanceMethod("write", &WrappedVsam::Write),
    InstanceMethod("delete", &WrappedVsam::Delete),
    InstanceMethod("close", &WrappedVsam::Close),
    InstanceMethod("dealloc", &WrappedVsam::Dealloc)
  });

  constructor_ = Napi::Persistent(func);
  constructor_.SuppressDestruct();

  exports.Set("WrappedVsam", func);
  return exports;
}

// static
Napi::Object WrappedVsam::Construct(const Napi::CallbackInfo& info, bool alloc) {
  Napi::Env env = info.Env();
  Napi::EscapableHandleScope scope(env);

  std::string path (static_cast<std::string>(info[0].As<Napi::String>()));
  Napi::Object schema = info[1].ToObject();
  std::string mode = info.Length() == 2 ? "ab+,type=record"
                     : (static_cast<std::string>(info[2].As<Napi::String>()));
  Napi::Array properties = schema.GetPropertyNames();
  std::vector<LayoutItem> layout;
  int key_i = 0; // for its data type - default to first field if no "key" found

  for (int i = 0; i < properties.Length(); ++i) {
    std::string name (static_cast<std::string>(Napi::String (env, properties.Get(i).ToString())));

    Napi::Object item = schema.Get(properties.Get(i)).As<Napi::Object>();
    if (item.IsEmpty()) {
      throwError(env, "Error in JSON: item %d is empty.", i+1);
      return env.Null().ToObject();
    }

    int minLength = 0; // minLength is optional, default 0 unless it's a key
    if (item.Has("minLength")) {
      Napi::Value vminLength = item.Get("minLength");
      if (!vminLength.IsEmpty() && !vminLength.IsNumber()) {
        throwError(env, "Error in JSON (item %d): minLength value must be numeric.", i+1);
        return env.Null().ToObject();
      } else if (!vminLength.IsEmpty()) {
        minLength = vminLength.ToNumber().Int32Value();
        if (minLength < 0) {
          throwError(env, "Error in JSON (item %d): minLength value cannot be negative.", i+1);
          return env.Null().ToObject();
        }
      }
    }

    int maxLength = 0;
    if (!item.Has("maxLength")) {
      throwError(env, "Error in JSON (item %d): maxLength must be specified.", i+1);
      return env.Null().ToObject();
    }
    Napi::Value vmaxLength = item.Get("maxLength");
    if (!vmaxLength.IsNumber()) {
      throwError(env, "Error in JSON (item %d): maxLength must be numeric.", i+1);
      return env.Null().ToObject();
    } else {
      maxLength = vmaxLength.ToNumber().Int32Value();
      if (maxLength <= 0) {
        throwError(env, "Error in JSON (item %d): maxLength value must be greater than 0.", i+1);
        return env.Null().ToObject();
      }
    }

    if (minLength > maxLength) {
      throwError(env, "Error in JSON (item %d): minLength cannot be greater than maxLength.", i+1);
      return env.Null().ToObject();
    }

    Napi::Value jtype;
    if (!item.Has("type") || !(jtype = item.Get("type")) || jtype.IsEmpty()) {
      throwError(env, "Error in JSON (item %d): \"type\" must be specified (string or hexadecimal)", i+1);
      return env.Null().ToObject();
    }

    std::string stype(static_cast<std::string>(jtype.As<Napi::String>()));
    if (!strcmp(stype.c_str(),"string")) {
      layout.push_back(LayoutItem(name, minLength, maxLength, LayoutItem::STRING));
    } else if (!strcmp(stype.c_str(),"hexadecimal")) {
      layout.push_back(LayoutItem(name, minLength, maxLength, LayoutItem::HEXADECIMAL));
    } else {
      throwError(env, "Error in JSON (item %d): \"type\" must be either \"string\" or \"hexadecimal\"", i+1);
      return env.Null().ToObject();
    }
    if (!strcmp(name.c_str(),"key")) {
      key_i = i;
    }
  }

  Napi::Object item = schema.Get(properties.Get(key_i)).As<Napi::Object>();
  assert (!item.IsEmpty());
  if (item.Has("minLength")) {
    Napi::Value vminLength = item.Get(Napi::String::New(env,"minLength"));
    if (vminLength.ToNumber().Int32Value() == 0) {
      throwError(env, "Error in JSON (item %d): minLength of key '%s' must be greater than 0.", key_i+1, layout[key_i].name.c_str());
      return env.Null().ToObject();
    }
  } else
    layout[key_i].minLength = 1;
  
  Napi::Object obj = constructor_.New({
    Napi::String::New(env, path),
    Napi::Buffer<std::vector<LayoutItem>>::Copy(env, &layout, layout.size()),
    Napi::Number::New(env, key_i),
    Napi::String::New(env, mode),
    Napi::Boolean::New(env, alloc)});

  std::string errmsg;
  WrappedVsam *p = Napi::ObjectWrap<WrappedVsam>::Unwrap(obj);
  if (!p || !p->pVsamFile_ || p->pVsamFile_->getLastError(errmsg) || !p->pVsamFile_->isDatasetOpen()) {
    const char *perr = errmsg.c_str();
    Napi::Error::New(env, perr && *perr ? perr : "Error: failed to construct VsamFile object.").ThrowAsJavaScriptException();
    delete p;
    return env.Null().ToObject();
  }
  return scope.Escape(napi_value(obj)).ToObject();
}

// static
Napi::Object WrappedVsam::AllocSync(const Napi::CallbackInfo& info) {
  if (info.Length() != 2 || !info[0].IsString() || !info[1].IsObject()) {
    Napi::HandleScope scope(info.Env());
    Napi::TypeError::New(info.Env(), "Error: openSync() expects arguments: VSAM dataset name, schema JSON object.").ThrowAsJavaScriptException();
    return info.Env().Null().ToObject();
  }
  return Construct(info, true);
}

// static
Napi::Object WrappedVsam::OpenSync(const Napi::CallbackInfo& info) {
  if ((info.Length() < 2 || !info[0].IsString() || !info[1].IsObject())
  ||  (info.Length() == 3 && !info[2].IsString())
  ||  (info.Length() > 3)) {
    Napi::HandleScope scope(info.Env());
    Napi::TypeError::New(info.Env(), "Error: openSync() expects arguments: VSAM dataset name, schema JSON object, optional fopen() mode.").ThrowAsJavaScriptException();
    return info.Env().Null().ToObject();
  }
  return Construct(info, false);
}

// static
Napi::Boolean WrappedVsam::Exist(const Napi::CallbackInfo& info) {
  Napi::HandleScope scope(info.Env());
  if (info.Length() != 1 || !info[0].IsString()) {
    Napi::TypeError::New(info.Env(), "Error: exist() expects argument: VSAM dataset name.").ThrowAsJavaScriptException();
    return Napi::Boolean::New(info.Env(), false);
  }
  std::string path (static_cast<std::string>(info[0].As<Napi::String>()));
  return Napi::Boolean::New(info.Env(), VsamFile::isDatasetExist(path));
}


void WrappedVsam::Close(const Napi::CallbackInfo& info) {
  std::string errmsg;
#ifdef DEBUG
  fprintf(stderr,"Closing VSAM dataset...\n");
#endif
  if (pVsamFile_->Close(errmsg)) {
    Napi::HandleScope scope(info.Env());
    Napi::Error::New(info.Env(), errmsg).ThrowAsJavaScriptException();
    return;
  }
}


void WrappedVsam::Delete(const Napi::CallbackInfo& info) {
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::TypeError::New(info.Env(), "Error: delete() expects argument: function.").ThrowAsJavaScriptException();
    return;
  }
  uv_work_t* request = new uv_work_t;
  Napi::Function cb = info[0].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env());
  uv_queue_work(uv_default_loop(), request, DeleteExecute, DeleteComplete);
}


void WrappedVsam::Write(const Napi::CallbackInfo& info) {
  Napi::HandleScope scope(info.Env());
  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsFunction()) {
    Napi::TypeError::New(info.Env(), "Error: write() expects arguments: record, function.").ThrowAsJavaScriptException();
    return;
  }
  Napi::Object record = info[0].ToObject();
  int reclen = pVsamFile_->getRecordLength();
  char *recbuf = (char*)malloc(reclen);
  assert(recbuf != NULL && reclen > 0);
  memset(recbuf, 0, reclen);
  char* fldbuf = recbuf;
  std::vector<LayoutItem>& layout = pVsamFile_->getLayout();
  std::string errmsg;

  for(auto i = layout.begin(); i != layout.end(); ++i) {
    Napi::Value field = record.Get(i->name);
    if (i->type == LayoutItem::STRING || i->type == LayoutItem::HEXADECIMAL) {
      std::string str = static_cast<std::string>(Napi::String (info.Env(), field.ToString()));
      if (i->type == LayoutItem::STRING) {
        if (!VsamFile::isStrValid(*i, str, errmsg)) {
          Napi::TypeError::New(info.Env(), errmsg).ThrowAsJavaScriptException();
          return;
        }
        assert((fldbuf - recbuf) + str.length() <= reclen);
        if (str.length() > 0)
          memcpy(fldbuf, str.c_str(), str.length());
      } else {
        if (!VsamFile::isHexStrValid(*i, str, errmsg)) {
          Napi::TypeError::New(info.Env(), errmsg).ThrowAsJavaScriptException();
          return;
        }
        assert((fldbuf - recbuf) + i->maxLength <= reclen);
        VsamFile::hexstrToBuffer(fldbuf, i->maxLength, str.c_str());
      }
    } else {
      Napi::TypeError::New(info.Env(), "Unexpected JSON data type").ThrowAsJavaScriptException();
      return;
    }
    fldbuf += i->maxLength;
  }

  uv_work_t* request = new uv_work_t;
  Napi::Function cb = info[1].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env(), recbuf);
  uv_queue_work(uv_default_loop(), request, WriteExecute, WriteComplete);
}


void WrappedVsam::Update(const Napi::CallbackInfo& info) {
  Napi::HandleScope scope(info.Env());
  if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsFunction()) {
    Napi::TypeError::New(info.Env(), "Error: write() expects arguments: record, function.").ThrowAsJavaScriptException();
    return;
  }

  Napi::Object record = info[0].ToObject();
  int reclen = pVsamFile_->getRecordLength();
  char *recbuf = (char*)malloc(reclen);
  assert(recbuf != NULL);
  memset(recbuf, 0, reclen);
  char* fldbuf = recbuf;
  std::vector<LayoutItem>& layout = pVsamFile_->getLayout();
  std::string errmsg;

  for(auto i = layout.begin(); i != layout.end(); ++i) {
    Napi::Value field = record.Get(i->name);
    if (i->type == LayoutItem::STRING || i->type == LayoutItem::HEXADECIMAL) {
      std::string str = static_cast<std::string>(Napi::String (info.Env(), field.ToString()));
      if (i->type == LayoutItem::STRING) {
        if (!VsamFile::isStrValid(*i, str, errmsg)) {
          Napi::TypeError::New(info.Env(), errmsg).ThrowAsJavaScriptException();
          return;
        }
        if (str.length() > 0)
          memcpy(fldbuf, str.c_str(), str.length());
      } else {
        if (!VsamFile::isHexStrValid(*i, str, errmsg)) {
          Napi::TypeError::New(info.Env(), errmsg).ThrowAsJavaScriptException();
          return;
        }
        VsamFile::hexstrToBuffer(fldbuf, i->maxLength, str.c_str());
      }
      fldbuf += i->maxLength;
    } else {
      throwError(info.Env(), "Error: unexpected data type %d.", i->type);
      return;
    }
  }

  uv_work_t* request = new uv_work_t;
  Napi::Function cb = info[1].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env(), recbuf);
  uv_queue_work(uv_default_loop(), request, UpdateExecute, UpdateComplete);
}


void WrappedVsam::FindEq(const Napi::CallbackInfo& info) {
  Find(info, __KEY_EQ);
}


void WrappedVsam::FindGe(const Napi::CallbackInfo& info) {
  Find(info, __KEY_GE);
}


void WrappedVsam::FindFirst(const Napi::CallbackInfo& info) {
  Find(info, __KEY_FIRST);
}


void WrappedVsam::FindLast(const Napi::CallbackInfo& info) {
  Find(info, __KEY_LAST);
}


void WrappedVsam::Find(const Napi::CallbackInfo& info, int equality) {
  Napi::HandleScope scope(info.Env());
  std::string key;
  char* keybuf = NULL;
  int keybuf_len = 0;
  int key_i = pVsamFile_->getKeyNum();
  std::vector<LayoutItem>& layout = pVsamFile_->getLayout();
  int callbackArg = 0;
  std::string errmsg;

  if (equality != __KEY_LAST && equality != __KEY_FIRST)  {
    if (info.Length() < 2) {
      Napi::Error::New(info.Env(), "Error: find() expects at least 2 arguments.").ThrowAsJavaScriptException();
      return;
    }
    if (info[0].IsString()) {
      key = static_cast<std::string>(info[0].As<Napi::String>());
#ifdef DEBUG
      fprintf(stderr,"Find() line %d: key=<%s>\n",__LINE__,key.c_str());
#endif
      if (layout[key_i].type == LayoutItem::HEXADECIMAL) {
        if (!VsamFile::isHexStrValid(layout[key_i], key, errmsg)) {
          Napi::TypeError::New(info.Env(), errmsg).ThrowAsJavaScriptException();
          return;
        }
      } else {
        if (!VsamFile::isStrValid(layout[key_i], key, errmsg)) {
          Napi::TypeError::New(info.Env(), errmsg).ThrowAsJavaScriptException();
          return;
        }
      }
      callbackArg = 1;
    } else if (info[0].IsObject()) {
      char* buf = info[0].As<Napi::Buffer<char>>().Data();
      if (!info[1].IsNumber()) {
        Napi::TypeError::New(info.Env(), "Error: find() buffer argument must be followed by its length.").ThrowAsJavaScriptException();
        return;
      }
      keybuf_len = info[1].As<Napi::Number>().Uint32Value();
#ifdef DEBUG
      fprintf(stderr,"Find() line %d: keybuf_len=<%d>\n",__LINE__,keybuf_len);
#endif
      if (!VsamFile::isHexBufValid(layout[key_i], buf, keybuf_len, errmsg)) {
#ifdef DEBUG
        fprintf(stderr,"Find() line %d: %s\n",__LINE__,errmsg.c_str());
#endif
        Napi::TypeError::New(info.Env(), errmsg).ThrowAsJavaScriptException();
        return;
      }
      assert(keybuf_len > 0);
      keybuf = (char*)malloc(keybuf_len);
      assert(keybuf != NULL);
      memcpy(keybuf, buf, keybuf_len);
      callbackArg = 2;
    } else {
      Napi::TypeError::New(info.Env(), "Error: find() first argument must be either a string or a Buffer object.").ThrowAsJavaScriptException();
      return;
    }
    if (!info[callbackArg].IsFunction()) {
      char err[64];
      if (callbackArg==1) {
        strcpy(err,"Error: find() second argument must be a function.");
      } else if (callbackArg==2) {
        strcpy(err,"Error: find() thrid argument must be a function.");
      }
      Napi::Error::New(info.Env(),err).ThrowAsJavaScriptException();
      return;
    }
  } else {
#ifdef DEBUG
    if (equality == __KEY_LAST)
      fprintf(stderr,"Find() line %d: equality=__KEY_LAST\n",__LINE__);
    else if (equality == __KEY_FIRST)
      fprintf(stderr,"Find() line %d: equality=__KEY_FIRST\n",__LINE__);
    else
      assert(0);
#endif
    if (info.Length() < 1) {
      Napi::Error::New(info.Env(), "Error: find() wrong number of arguments; one argument expected.").ThrowAsJavaScriptException();
      return;
    }
    if (!info[0].IsFunction()) {
      Napi::TypeError::New(info.Env(), "Error: find() first argument must be a function.").ThrowAsJavaScriptException();
      return;
    }
  }

  uv_work_t* request = new uv_work_t;
  Napi::Function cb = info[callbackArg].As<Napi::Function>();
#ifdef DEBUG
  if (equality != __KEY_LAST && equality != __KEY_FIRST)  {
    if (key.length() > 0)
      fprintf(stderr,"Find() line %d: key=<%s>\n",__LINE__,key.c_str()); 
    else if (keybuf) {
      fprintf(stderr,"Find() line %d: keybuf=",__LINE__);
      for (int i=0; i<keybuf_len; i++)
        fprintf(stderr,"%x ",keybuf[i]);
      fprintf(stderr,"\n");
    } else
      assert(0);
  }
#endif
  request->data = new UvWorkData(pVsamFile_, cb, info.Env(), 0, key, keybuf, keybuf_len, equality);
  uv_queue_work(uv_default_loop(), request, FindExecute, ReadComplete);
}


void WrappedVsam::Read(const Napi::CallbackInfo& info) {
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::HandleScope scope(info.Env());
    Napi::Error::New(info.Env(), "Error: read() expects argument: function.").ThrowAsJavaScriptException();
    return;
  }
  uv_work_t* request = new uv_work_t;
  Napi::Function cb = info[0].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env());
  uv_queue_work(uv_default_loop(), request, ReadExecute, ReadComplete);
}


void WrappedVsam::Dealloc(const Napi::CallbackInfo& info) {
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::HandleScope scope(info.Env());
    Napi::Error::New(info.Env(), "Error: dealloc() expects argument: function.").ThrowAsJavaScriptException();
    return;
  }
  if (pVsamFile_ && pVsamFile_->isDatasetOpen()) {
    Napi::HandleScope scope(info.Env());
    Napi::Error::New(info.Env(), "Cannot dealloc an open VSAM dataset.").ThrowAsJavaScriptException();
    return;
  }
  uv_work_t* request = new uv_work_t;
  Napi::Function cb = info[0].As<Napi::Function>();
  request->data = new UvWorkData(pVsamFile_, cb, info.Env());
  uv_queue_work(uv_default_loop(), request, DeallocExecute, DeallocComplete);
}
