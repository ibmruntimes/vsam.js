/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2017. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
*/
#include "VsamFile.h"
#include <node_buffer.h>
#include <unistd.h>
#include <dynit.h>
#include <sstream>
#include <numeric>

Napi::FunctionReference VsamFile::constructor_;

static const char* hexstrToBuffer (char* hexbuf, int buflen, const char* hexstr);
static const char* bufferToHexstr (char* hexstr, const char* hexbuf, const int hexbuflen);

static void print_amrc() {
  __amrc_type currErr = *__amrc;
  printf("R15 value = %d\n", currErr.__code.__feedback.__rc);
  printf("Reason code = %d\n", currErr.__code.__feedback.__fdbk);
  printf("RBA = %d\n", currErr.__RBA);
  printf("Last op = %d\n", currErr.__last_op);
}


void VsamFile::DeleteCallback(uv_work_t* req, int status) {
  VsamFile* obj = (VsamFile*)(req->data);
  delete req;
  //TODO: what if the write failed (buf != NULL)

  if (status == UV_ECANCELED)
    return;

  Napi::HandleScope scope(obj->env_);
  if (obj->lastrc_ != 0) {
    obj->cb_.Call(obj->env_.Global(), {Napi::String::New(obj->env_, "Failed to delete")});
    obj->lastrc_ = 0;
  }
  else
    obj->cb_.Call(obj->env_.Global(), {obj->env_.Null()});
}

void VsamFile::WriteCallback(uv_work_t* req, int status) {
  VsamFile* obj = (VsamFile*)(req->data);
  delete req;

  if (status == UV_ECANCELED)
    return;

  Napi::HandleScope scope(obj->env_);
  if (obj->lastrc_ != obj->reclen_) {
    obj->cb_.Call(obj->env_.Global(), {Napi::String::New(obj->env_,"Failed to write")});
  }
  else {
    obj->cb_.Call(obj->env_.Global(), {obj->env_.Null()});
  }
}


void VsamFile::UpdateCallback(uv_work_t* req, int status) {
  VsamFile* obj = (VsamFile*)(req->data);
  delete req;
  //TODO: what if the update failed (buf != NULL)

  if (status == UV_ECANCELED)
    return;

  Napi::HandleScope scope(obj->env_);
  obj->cb_.Call(obj->env_.Global(), {obj->env_.Null()});
}


void VsamFile::ReadCallback(uv_work_t* req, int status) {
  VsamFile* obj = (VsamFile*)(req->data);
  delete req;

  if (status == UV_ECANCELED)
    return;

  char* buf = (char*)(obj->buf_);

  if (buf != NULL) {
    Napi::HandleScope scope(obj->env_);
    Napi::Object record = Napi::Object::New(obj->env_);
    for(auto i = obj->layout_.begin(); i != obj->layout_.end(); ++i) {
      if (i->type == LayoutItem::STRING) { 
        std::string str(buf,i->maxLength+1);
        str[i->maxLength] = 0;
        record.Set(&(i->name[0]), Napi::String::New(obj->env_, str.c_str()));
      }
      else if (i->type == LayoutItem::HEXADECIMAL) { 
        char hexstr[(i->maxLength*2)+1];
        bufferToHexstr(hexstr, buf, i->maxLength);
        record.Set(&(i->name[0]), Napi::String::New(obj->env_, hexstr));
      }
      buf += i->maxLength;
    }
    obj->cb_.Call(obj->env_.Global(), {record, obj->env_.Null()});
  }
  else {
    Napi::HandleScope scope(obj->env_);
    obj->cb_.Call(obj->env_.Global(), { obj->env_.Null(), obj->env_.Null()});
  }
}


void VsamFile::Find(uv_work_t* req) {
  VsamFile* obj = (VsamFile*)(req->data);
  int rc;
  const char* buf;
  int buflen;
  if (obj->keybuf_) {
    if (obj->keybuf_len_==0) {
      Napi::TypeError::New(obj->env_, "find: Buffer object is empty.").ThrowAsJavaScriptException();
      return;
    }
    buf = obj->keybuf_;
    buflen = obj->keybuf_len_;
  } else {
    LayoutItem& key_layout = obj->layout_[obj->key_i_];
    if (key_layout.type == LayoutItem::HEXADECIMAL) {
      char buf[key_layout.maxLength+1];
      hexstrToBuffer(buf, sizeof(buf), obj->key_.c_str());
      rc = flocate(obj->stream_, buf, obj->keylen_, obj->equality_);
      goto chk;
    } else {
      buf = obj->key_.c_str();
      buflen = obj->keylen_;
    }
  }
  rc = flocate(obj->stream_, buf, buflen, obj->equality_);

chk: 
  if (rc==0) {
    char buf[obj->reclen_];
    int ret = fread(buf, obj->reclen_, 1, obj->stream_);
    //TODO: if read fails
    if (ret == 1) {
      if (obj->buf_) {
        free(obj->buf_);
      }
      obj->buf_ = malloc(obj->reclen_);
      //TODO: if malloc fails
      memcpy(obj->buf_, buf, obj->reclen_);
      return;
    }
  }
  if (obj->buf_) {
    free(obj->buf_);
    obj->buf_ = NULL;
  }
  if (obj->keybuf_) {
    free(obj->keybuf_);
    obj->keybuf_ = NULL;
  }
}

void VsamFile::Read(uv_work_t* req) {
  VsamFile* obj = (VsamFile*)(req->data);
  char buf[obj->reclen_];
  int ret = fread(buf, obj->reclen_, 1, obj->stream_);
  //TODO: if read fails
  if (ret == 1) {
    obj->buf_ = malloc(obj->reclen_);
    //TODO: if malloc fails
    memcpy(obj->buf_, buf, obj->reclen_);
    return;
  }
  if (obj->buf_) {
    free(obj->buf_);
    obj->buf_ = NULL;
  }
  if (obj->keybuf_) {
    free(obj->keybuf_);
    obj->keybuf_ = NULL;
  }
}


void VsamFile::Delete(uv_work_t* req) {
  VsamFile* obj = (VsamFile*)(req->data);
  obj->lastrc_ = fdelrec(obj->stream_);
}


void VsamFile::Write(uv_work_t* req) {
  VsamFile* obj = (VsamFile*)(req->data);
  obj->lastrc_ = fwrite(obj->buf_, 1, obj->reclen_, obj->stream_);
  if (obj->buf_) {
    free(obj->buf_);
    obj->buf_ = NULL;
  }
  if (obj->keybuf_) {
    free(obj->keybuf_);
    obj->keybuf_ = NULL;
  }
}


void VsamFile::Update(uv_work_t* req) {
  VsamFile* obj = (VsamFile*)(req->data);
  int ret = fupdate(obj->buf_, obj->reclen_, obj->stream_);
  if (ret == 0) {
    //TODO: error
  }
  free(obj->buf_);
  obj->buf_ = NULL;
  if (obj->keybuf_) {
    free(obj->keybuf_);
    obj->keybuf_ = NULL;
  }
}


void VsamFile::Dealloc(uv_work_t* req) {
  VsamFile* obj = (VsamFile*)(req->data);

  std::ostringstream dataset;
  dataset << "//'" << obj->path_.c_str() << "'";

  obj->lastrc_ = remove(dataset.str().c_str());
}


void VsamFile::DeallocCallback(uv_work_t* req, int status) {
  VsamFile* obj = (VsamFile*)(req->data);
  delete req;

  Napi::HandleScope scope(obj->env_);
  if (status == UV_ECANCELED) {
    return;
  }
  else if (obj->lastrc_ != 0) {
    obj->cb_.Call(obj->env_.Global(), {Napi::String::New(obj->env_, "Couldn't deallocate dataset")});
  }
  else {
    obj->cb_.Call(obj->env_.Global(), {obj->env_.Null()});
  }
}


static std::string& createErrorMsg (std::string& errmsg, int err, int err2, const char* title) {
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
  return errmsg;
}


VsamFile::VsamFile(const Napi::CallbackInfo& info)
: Napi::ObjectWrap<VsamFile>(info),
    env_(info.Env()),
    stream_(NULL),
    keylen_(-1),
    lastrc_(0),
    buf_(NULL),
    keybuf_(NULL),
    keybuf_len_(0) {
  Napi::HandleScope scope(env_);

  if (info.Length() != 5) {
    Napi::Error::New(env_, "Wrong number of arguments to VsamFile::VsamFile")
        .ThrowAsJavaScriptException();
    return;
  }

  path_ = static_cast<std::string>(info[0].As<Napi::String>());
  Napi::Buffer<std::vector<LayoutItem>> b = info[1].As<Napi::Buffer<std::vector<LayoutItem>>>();
  layout_ = *(static_cast<std::vector<LayoutItem>*>(b.Data()));
  bool alloc(static_cast<bool>(info[2].As<Napi::Boolean>()));
  key_i_ = static_cast<int>(info[3].As<Napi::Number>().Int32Value());
  omode_ = static_cast<std::string>(info[4].As<Napi::String>());

  std::ostringstream dataset;
  dataset << "//'" << path_.c_str() << "'";

  stream_ = fopen(dataset.str().c_str(), "rb+,type=record");
  int err = errno;
  int err2 = __errno2();

  if (!alloc) {
    if (stream_ == NULL) {
      createErrorMsg(errmsg_, err, err2, "Failed to open dataset");
      lastrc_ = -1;
      return;
    }
    stream_ = freopen(dataset.str().c_str(), omode_.c_str(), stream_);
    if (stream_ == NULL) {
      createErrorMsg(errmsg_, errno, __errno2(), "Failed to open dataset");
      lastrc_ = -1;
      return;
    }
  } else {
    if (stream_ != NULL) {
      errmsg_ = "Dataset already exists";
      fclose(stream_);
      stream_ = NULL;
      lastrc_ = -1;
      return;
    }
    if (err2 != 0xC00B0641) {
      createErrorMsg(errmsg_, err, err2, "Unexpected fopen error");
      lastrc_ = -1;
      return;
    }

    std::ostringstream ddname;
    ddname << "NAMEDD";

    __dyn_t dyn;
    dyninit(&dyn);
    dyn.__dsname = &(path_[0]);
    dyn.__ddname = &(ddname.str()[0]);
    dyn.__normdisp = __DISP_CATLG;
    dyn.__lrecl = std::accumulate(layout_.begin(), layout_.end(), 0,
                                  [](int n, LayoutItem& l) -> int { return n + l.maxLength; });
    dyn.__keylength = layout_[0].maxLength;
    dyn.__recorg = __KS;
    if (dynalloc(&dyn) != 0) {
      errmsg_ = "Failed to allocate dataset";
      lastrc_ = -1;
      return;
    }
    stream_ = fopen(dataset.str().c_str(), "ab+,type=record");
    if (stream_ == NULL) {
      createErrorMsg(errmsg_, errno, __errno2(), "Failed to open new dataset");
      lastrc_ = -1;
      return;
    }
  }

  fldata_t dinfo;
  fldata(stream_, NULL, &dinfo);
  keylen_ = dinfo.__vsamkeylen;
  reclen_ = dinfo.__maxreclen;
  if (keylen_ != layout_[0].maxLength) {
    errmsg_ = "Incorrect key length";
    fclose(stream_);
    stream_ = NULL;
    lastrc_ = -1;
    return;
  }
}


VsamFile::~VsamFile() {
  if (stream_ != NULL)
    fclose(stream_);
}


void VsamFile::Init(Napi::Env env, Napi::Object exports) {
  // Prepare constructor template
  Napi::HandleScope scope(env);

  Napi::Function func = DefineClass(env, "VsamFile", {
    InstanceMethod("read", &VsamFile::Read),
    InstanceMethod("find", &VsamFile::FindEq),
    InstanceMethod("findeq", &VsamFile::FindEq),
    InstanceMethod("findge", &VsamFile::FindGe),
    InstanceMethod("findfirst", &VsamFile::FindFirst),
    InstanceMethod("findlast", &VsamFile::FindLast),
    InstanceMethod("update", &VsamFile::Update),
    InstanceMethod("write", &VsamFile::Write),
    InstanceMethod("delete", &VsamFile::Delete),
    InstanceMethod("close", &VsamFile::Close),
    InstanceMethod("dealloc", &VsamFile::Dealloc)
  });

  constructor_ = Napi::Persistent(func);
  constructor_.SuppressDestruct();

  exports.Set("VsamFile", func);
}


Napi::Value VsamFile::Construct(const Napi::CallbackInfo& info, bool alloc) {
  Napi::Env env = info.Env();

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
      Napi::Error::New(env, "JSON is incorrect.").ThrowAsJavaScriptException();
      return env.Null();
    }

    Napi::Value length = item.Get(Napi::String::New(env,"maxLength"));
    if (length.IsEmpty() || !length.IsNumber()) {
      Napi::Error::New(env, "JSON is incorrect.").ThrowAsJavaScriptException();
      return env.Null();
    }

    Napi::Value jtype = item.Get(Napi::String::New(env,"type"));
    if (jtype.IsEmpty()) {
      Napi::Error::New(env, "JSON \"type\" is empty.").ThrowAsJavaScriptException();
      return env.Null();
    }

    std::string stype(static_cast<std::string>(jtype.As<Napi::String>()));
    if (!strcmp(stype.c_str(),"string")) {
      layout.push_back(LayoutItem(name, length.ToNumber().Int32Value(), LayoutItem::STRING));
    } else if (!strcmp(stype.c_str(),"hexadecimal")) {
      layout.push_back(LayoutItem(name, length.ToNumber().Int32Value(), LayoutItem::HEXADECIMAL));
    } else {
      Napi::Error::New(env, "JSON \"type\" must be \"string\" or \"hexadecimal\"").ThrowAsJavaScriptException();
      return env.Null();
    }

    if (!strcmp(name.c_str(),"key")) {
      key_i = i;
    }
  }

  Napi::HandleScope scope(env);
  Napi::Object obj = constructor_.New({
    Napi::String::New(env, path),
    Napi::Buffer<std::vector<LayoutItem>>::Copy(env, &layout, layout.size()),
    Napi::Boolean::New(env, alloc),
    Napi::Number::New(env, key_i),
    Napi::String::New(env, mode)});

  VsamFile* p = Napi::ObjectWrap<VsamFile>::Unwrap(obj);
  if (p->lastrc_) {
    Napi::Error::New(env, p->errmsg_.c_str()).ThrowAsJavaScriptException();
    delete p;
    return env.Null();
  }
  return obj;
}


Napi::Value VsamFile::AllocSync(const Napi::CallbackInfo& info) {
  if (info.Length() != 2 || !info[0].IsString() || !info[1].IsObject()) {
    Napi::Error::New(info.Env(), "Wrong arguments to allocSync(), must be: "\
                          "VSAM dataset name, schema JSON object").ThrowAsJavaScriptException();
    return info.Env().Null();
  }
  return Construct(info, true);
}


Napi::Value VsamFile::OpenSync(const Napi::CallbackInfo& info) {
  if ((info.Length() < 2 || !info[0].IsString() || !info[1].IsObject())
  ||  (info.Length() == 3 && !info[2].IsString())
  ||  (info.Length() > 3)) {
    Napi::Error::New(info.Env(), "Wrong arguments to openSync(), must be: "\
                          "VSAM dataset name, schema JSON object, optional fopen() mode")
                          .ThrowAsJavaScriptException();
    return info.Env().Null();
  }
  return Construct(info, false);
}


Napi::Boolean VsamFile::Exist(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() != 1) {
    // Throw an Error that is passed back to JavaScript
    Napi::Error::New(env, "Wrong number of arguments.").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }

  if (!info[0].IsString()) {
    Napi::TypeError::New(env, "Wrong arguments").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }

  std::string path (static_cast<std::string>(info[0].As<Napi::String>()));
  std::ostringstream dataset;
  dataset << "//'" << path.c_str() << "'";
  FILE *stream = fopen(dataset.str().c_str(), "rb+,type=record");

  if (stream == NULL) {
    return Napi::Boolean::New(env, false);
  }

  fclose(stream);
  return Napi::Boolean::New(env, true);
}


void VsamFile::Close(const Napi::CallbackInfo& args) {
  if (stream_ == NULL) {
    Napi::Error::New(env_, "VSAM file is not open.").ThrowAsJavaScriptException();
    return;
  }

  if (fclose(stream_)) {
    Napi::Error::New(env_, "Error closing file.").ThrowAsJavaScriptException();
    return;
  }
  stream_ = NULL;
}


void VsamFile::Delete(const Napi::CallbackInfo& info) {
  if (info.Length() < 1) {
    // Throw an Error that is passed back to JavaScript
    Napi::Error::New(env_, "Wrong number of arguments.").ThrowAsJavaScriptException();
    return;
  }

  if (!info[0].IsFunction()) {
    Napi::TypeError::New(env_, "Wrong arguments.").ThrowAsJavaScriptException();
    return;
  }

  uv_work_t* request = new uv_work_t;
  request->data = this;
  cb_ = Napi::Persistent(info[0].As<Napi::Function>());
  uv_queue_work(uv_default_loop(), request, Delete, DeleteCallback);
}


void VsamFile::Write(const Napi::CallbackInfo& info) {
  if (info.Length() < 2) {
    // Throw an Error that is passed back to JavaScript
    Napi::Error::New(env_, "Wrong number of arguments.").ThrowAsJavaScriptException();
    return;
  }

  if (!info[1].IsFunction()) {
    Napi::TypeError::New(env_, "Wrong arguments.").ThrowAsJavaScriptException();
    return;
  }

  Napi::Object record = info[0].ToObject();
  if (buf_) {
    free(buf_);
  }
  buf_ = malloc(reclen_); //TODO: error
  memset(buf_,0,reclen_);
  char* buf = (char*)buf_;
  for(auto i = layout_.begin(); i != layout_.end(); ++i) {
    Napi::Value field = record.Get(&(i->name[0]));
    if (i->type == LayoutItem::STRING || i->type == LayoutItem::HEXADECIMAL) {
      std::string key = static_cast<std::string>(Napi::String (env_, field.ToString()));
      if (i->type == LayoutItem::STRING) {
        memcpy(buf, key.c_str(), key.length());
      } else {
        hexstrToBuffer(buf, i->maxLength, key.c_str());
      }
    } else {
      Napi::TypeError::New(env_, "Unexpected JSON data type").ThrowAsJavaScriptException();
      return;
    }
    buf += i->maxLength;
  }

  uv_work_t* request = new uv_work_t;
  request->data = this;
  cb_ = Napi::Persistent(info[1].As<Napi::Function>());
  uv_queue_work(uv_default_loop(), request, Write, WriteCallback);
}


void VsamFile::Update(const Napi::CallbackInfo& info) {
  if (info.Length() < 2) {
    // Throw an Error that is passed back to JavaScript
    Napi::Error::New(env_, "Wrong number of arguments.").ThrowAsJavaScriptException();
    return;
  }

  if (!info[1].IsFunction()) {
    Napi::TypeError::New(env_, "Wrong arguments.").ThrowAsJavaScriptException();
    return;
  }

  Napi::Object record = info[0].ToObject();
  if (buf_) {
    free(buf_);
  }
  buf_ = malloc(reclen_); //TODO: error
  memset(buf_,0,reclen_);
  char* buf = (char*)buf_;
  for(auto i = layout_.begin(); i != layout_.end(); ++i) {
    Napi::Value field = record.Get(&(i->name[0]));
    if (i->type == LayoutItem::STRING || i->type == LayoutItem::HEXADECIMAL) {
      std::string key = static_cast<std::string>(Napi::String (env_, field.ToString()));
      if (i->type == LayoutItem::STRING) {
        memcpy(buf, key.c_str(), key.length());
      } else {
        hexstrToBuffer(buf, i->maxLength, key.c_str());
      }
      buf += i->maxLength;

    } else {
      Napi::TypeError::New(env_, "Unexpected JSON data type").ThrowAsJavaScriptException();
      return;
    }
  }

  uv_work_t* request = new uv_work_t;
  request->data = this;
  cb_ = Napi::Persistent(info[1].As<Napi::Function>());
  uv_queue_work(uv_default_loop(), request, Update, UpdateCallback);
}

void VsamFile::FindEq(const Napi::CallbackInfo& info) {
  Find(info, __KEY_EQ);
}

void VsamFile::FindGe(const Napi::CallbackInfo& info) {
  Find(info, __KEY_GE);
}

void VsamFile::FindFirst(const Napi::CallbackInfo& info) {
  Find(info, __KEY_FIRST);
}

void VsamFile::FindLast(const Napi::CallbackInfo& info) {
  Find(info, __KEY_LAST);
}

void VsamFile::Find(const Napi::CallbackInfo& info, int equality) {
  std::string key;
  int callbackArg = 0;
  char* keybuf = NULL;
  int keybuf_len = 0;

  if (equality != __KEY_LAST && equality != __KEY_FIRST)  {
    if (info.Length() < 2) {
      // Throw an Error that is passed back to JavaScript
      Napi::Error::New(env_, "Wrong number of arguments.").ThrowAsJavaScriptException();
      return;
    }

    if (info[0].IsString()) {
      key = static_cast<std::string>(info[0].As<Napi::String>());
      callbackArg = 1;
    } else if (info[0].IsObject()) {
      char* buf = info[0].As<Napi::Buffer<char>>().Data();
      if (!info[1].IsNumber()) {
        Napi::Error::New(env_, "Buffer argument must be followed by its length.").ThrowAsJavaScriptException();
        return;
      }
      keybuf_len = info[1].As<Napi::Number>().Uint32Value();
      if (keybuf_len > 0) {
        keybuf = (char*)malloc(keybuf_len); // TODO check error
        memcpy(keybuf, buf, keybuf_len);
      } else {
        Napi::TypeError::New(env_, "Key buffer length must be greater than 0.").ThrowAsJavaScriptException();
        return;
      }
      callbackArg = 2;
    } else {
      Napi::TypeError::New(env_, "First argument must be either a string or a Buffer object.").ThrowAsJavaScriptException();
      return;
    }
    if (!info[callbackArg].IsFunction()) {
      char err[64];
      if (callbackArg==1) {
        strcpy(err,"Second argument must be a function.");
      } else if (callbackArg==2) {
        strcpy(err,"Thrid argument must be a function.");
      }
      Napi::Error::New(env_,err).ThrowAsJavaScriptException();
      return;
    }
  } else {
    if (info.Length() < 1) {
      Napi::Error::New(env_, "Wrong number of arguments; one argument expected.").ThrowAsJavaScriptException();
      return;
    }
    if (!info[0].IsFunction()) {
      Napi::TypeError::New(env_, "First argument must be a function.").ThrowAsJavaScriptException();
      return;
    }
  }

  uv_work_t* request = new uv_work_t;
  request->data = this;
  cb_ = Napi::Persistent(info[callbackArg].As<Napi::Function>());

  if (!keybuf) {
    key_ = key;
  } else {
    key_ = "";
    keybuf_ = keybuf;
    keybuf_len_ = keybuf_len;
  }
  equality_ = equality;

  uv_queue_work(uv_default_loop(), request, Find, ReadCallback);
}

void VsamFile::Read(const Napi::CallbackInfo& info) {
  if (info.Length() < 1) {
    // Throw an Error that is passed back to JavaScript
    Napi::Error::New(env_, "Wrong number of arguments.").ThrowAsJavaScriptException();
    return;
  }

  if (!info[0].IsFunction()) {
    Napi::TypeError::New(env_, "Wrong arguments.").ThrowAsJavaScriptException();
    return;
  }

  uv_work_t* request = new uv_work_t;
  request->data = this;
  cb_ = Napi::Persistent(info[0].As<Napi::Function>());
  uv_queue_work(uv_default_loop(), request, Read, ReadCallback);
}


void VsamFile::Dealloc(const Napi::CallbackInfo& info) {
  if (info.Length() < 1) {
    // Throw an Error that is passed back to JavaScript
    Napi::Error::New(env_, "Wrong number of arguments.").ThrowAsJavaScriptException();
    return;
  }

  if (!info[0].IsFunction()) {
    Napi::TypeError::New(env_, "Wrong arguments.").ThrowAsJavaScriptException();
    return;
  }
  if (stream_ != NULL) {
    Napi::Error::New(env_, "Cannot dealloc an open VSAM file.").ThrowAsJavaScriptException();
    return;
  }
  uv_work_t* request = new uv_work_t;
  cb_ = Napi::Persistent(info[0].As<Napi::Function>());
  request->data = this;
  uv_queue_work(uv_default_loop(), request, Dealloc, DeallocCallback);
}


static const char* hexstrToBuffer (char* hexbuf, int buflen, const char* hexstr) {
   const int hexstrlen = strlen(hexstr);
   memset(hexbuf,0,buflen);
   char xx[2];
   int i, j, x;
   for (i=0,j=0; i<hexstrlen-(hexstrlen%2); ) {
     xx[0] = hexstr[i++];
     xx[1] = hexstr[i++];
     sscanf(xx,"%2x", &x);
     hexbuf[j++] = x;
   }
   if (hexstrlen%2) {
     xx[0] = hexstr[i];
     xx[1] = '0';
     sscanf(xx,"%2x", &x);
     hexbuf[j] = x;
   }
   return hexbuf;
}


static const char* bufferToHexstr (char* hexstr, const char* hexbuf, const int hexbuflen) {
   int i, j;
   for (i=0,j=0; i<hexbuflen; i++,j+=2) {
     if (hexbuf[i]==0) {
       memset(hexstr+j,'0',2);
     } else {
       sprintf(hexstr+j,"%02x", hexbuf[i]);
     }
   }
   hexstr[j] = 0;

   //remove trailing '00's
   for (--j; j>2 && hexstr[j]=='0' && hexstr[j-1]=='0'; j-=2)
     hexstr[j-1] = 0;
   return hexstr;
}
