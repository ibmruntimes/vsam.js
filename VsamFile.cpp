#include "VsamFile.h"
#include <node_buffer.h>
#include <unistd.h>

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::Value;
using v8::Exception;
using v8::HandleScope;
using v8::Handle;
using v8::Array;
using v8::Integer;
using v8::MaybeLocal;

Persistent<Function> VsamFile::constructor;

static void print_amrc() {
  __amrc_type currErr = *__amrc; /* copy contents of __amrc */
  /* structure so that values */
#pragma convert("IBM-1047")
  printf("R15 value = %d\n", currErr.__code.__feedback.__rc);
  printf("Reason code = %d\n", currErr.__code.__feedback.__fdbk);
  printf("RBA = %d\n", currErr.__RBA);
  printf("Last op = %d\n", currErr.__last_op);
#pragma convert(pop)
}


void VsamFile::UpdateCallback(uv_work_t* req, int status) {
  VsamFile* obj = (VsamFile*)(req->data);
  delete req;
  //TODO: what if the update failed (buf != NULL)

  if (status == UV_ECANCELED)
    return;

  const unsigned argc = 1;
  HandleScope scope(obj->isolate_);
  Local<Value> argv[argc] = { v8::Null(obj->isolate_) };
  auto fn = Local<Function>::New(obj->isolate_, obj->cb_);
  fn->Call(Null(obj->isolate_), argc, argv);
}


void VsamFile::ReadCallback(uv_work_t* req, int status) {
  VsamFile* obj = (VsamFile*)(req->data);
  delete req;

  if (status == UV_ECANCELED)
    return;

  char* buf = (char*)(obj->buf_);

  if (buf != NULL) {
    const unsigned argc = 2;
    HandleScope scope(obj->isolate_);
    Local<Object> record = Object::New(obj->isolate_);
    for(auto i = obj->layout_.begin(); i != obj->layout_.end(); ++i) {
      if (i->type == LayoutItem::STRING) { 
        std::string str; 
        transform(buf, buf + i->maxLength, back_inserter(str), [](char c) -> char {
          __e2a_l(&c, 1);
          return c;
        });
        record->Set(String::NewFromUtf8(obj->isolate_, &(i->name[0])),
                //node::Buffer::Copy(obj->isolate_, key.data(), key.size()).ToLocalChecked());
                String::NewFromUtf8(obj->isolate_, str.c_str()));
      }
      buf += i->maxLength;
    }
    Local<Value> argv[argc] = { record, v8::Null(obj->isolate_) };

    auto fn = Local<Function>::New(obj->isolate_, obj->cb_);
    fn->Call(Null(obj->isolate_), argc, argv);
  }
  else {
    const unsigned argc = 2;
    HandleScope scope(obj->isolate_);
    Local<Value> argv[argc] = { v8::Null(obj->isolate_),
                                v8::Null(obj->isolate_) };
    auto fn = Local<Function>::New(obj->isolate_, obj->cb_);
    fn->Call(Null(obj->isolate_), argc, argv);
  }
}


void VsamFile::CloseCallback(uv_work_t* req, int status) {
  VsamFile* obj = (VsamFile*)(req->data);
  delete req;

  if (status == UV_ECANCELED)
    return;

  const unsigned argc = 1;
  HandleScope scope(obj->isolate_);
  Local<Value> argv[argc] = { v8::Null(obj->isolate_) };
  auto fn = Local<Function>::New(obj->isolate_, obj->cb_);
  fn->Call(Null(obj->isolate_), argc, argv);
}


void VsamFile::Find(uv_work_t* req) {
  VsamFile* obj = (VsamFile*)(req->data);

  if (flocate(obj->stream_, obj->key_.c_str(), obj->keylen_, __KEY_EQ)) {
    obj->buf_ = NULL;
    return;
  }

  char buf[obj->reclen_];
  int ret = fread(buf, obj->reclen_, 1, obj->stream_);
  //TODO: if read fails
  if (ret == 1) {
    obj->buf_ = malloc(obj->reclen_);
    //TODO: if malloc fails
    memcpy(obj->buf_, buf, obj->reclen_);
  }
  else {
    obj->buf_ = NULL;
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
  }
  else {
    obj->buf_ = NULL;
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
}


void VsamFile::Close(uv_work_t* req) {
  VsamFile* obj = (VsamFile*)(req->data);
  fclose(obj->stream_);
  obj->stream_ = NULL;
}


void VsamFile::Open(uv_work_t* req) {
  VsamFile* obj = (VsamFile*)(req->data);

#pragma convert("IBM-1047")
  obj->stream_ = fopen(obj->path_.c_str(), "rb+,type=record");
#pragma convert(pop)

  fldata_t info;
  fldata(obj->stream_, NULL, &info);
  obj->keylen_ = info.__vsamkeylen;
  obj->reclen_ = info.__maxreclen;
}


void VsamFile::OpenCallback(uv_work_t* req, int status) {
  VsamFile* obj = (VsamFile*)(req->data);
  delete req;

  if (status == UV_ECANCELED)
    return;

  const unsigned argc = 1;
  HandleScope scope(obj->isolate_);
  Local<Value> argv[argc] = { obj->handle()  };
  auto fn = Local<Function>::New(obj->isolate_, obj->cb_);
  fn->Call(Null(obj->isolate_), argc, argv);
}


VsamFile::VsamFile(const std::string& path) :
    path_(path),
    stream_(NULL),
    keylen_(0),
    buf_(NULL) {

  uv_work_t* request = new uv_work_t;
  request->data = this;
  uv_queue_work(uv_default_loop(), request, Open, OpenCallback);
}

VsamFile::~VsamFile() {
  if (stream_ != NULL)
    fclose(stream_);
}

void VsamFile::Init(Isolate* isolate) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, VsamFile::New);
  tpl->SetClassName(String::NewFromUtf8(isolate, "VsamFile"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  NODE_SET_PROTOTYPE_METHOD(tpl, "fd", FileDescriptor);
  NODE_SET_PROTOTYPE_METHOD(tpl, "read", Read);
  NODE_SET_PROTOTYPE_METHOD(tpl, "find", Find);
  NODE_SET_PROTOTYPE_METHOD(tpl, "update", Update);
  NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);

  constructor.Reset(isolate, tpl->GetFunction());
}

void VsamFile::New(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  if (args.IsConstructCall()) {
    // Invoked as constructor: `new MyObject(...)`

    // Check the number of arguments passed.
    if (args.Length() < 3) {
      // Throw an Error that is passed back to JavaScript
      isolate->ThrowException(Exception::TypeError(
          String::NewFromUtf8(isolate, "Wrong number of arguments")));
      return;
    }

    if (!args[0]->IsString() || !args[2]->IsFunction()) {
      isolate->ThrowException(Exception::TypeError(
          String::NewFromUtf8(isolate, "Wrong arguments")));
      return;
    }

    std::string path (*v8::String::Utf8Value(args[0]->ToString()));
    transform(path.begin(), path.end(), path.begin(), [](char c) -> char {
      __a2e_l(&c, 1);
      return c;
    });
    VsamFile* obj = new VsamFile(path);

    Local<Object> schema = args[1]->ToObject();
    Local<Array> properties = schema->GetPropertyNames();
    int len = properties->Length();
    for (int i = 0; i < properties->Length(); ++i) {
      String::Utf8Value name(properties->Get(i)->ToString());

      Local<Object> item = Local<Object>::Cast(schema->Get(properties->Get(i)));
      if (item.IsEmpty()) {
        isolate->ThrowException(Exception::TypeError( String::NewFromUtf8(isolate, "Json is incorrect")));
        return;
      }

      Local<Value> length = Local<Value>::Cast(item->Get( Local<String>(String::NewFromUtf8(isolate, "maxLength"))));
      if (length.IsEmpty() || !length->IsNumber()) {
        isolate->ThrowException(Exception::TypeError( String::NewFromUtf8(isolate, "Json is incorrect")));
        return;
      }
      
      obj->layout_.push_back(LayoutItem(name, length->ToInteger()->Value(), LayoutItem::STRING));
    }


    Handle<Function> callback = Handle<Function>::Cast(args[2]);
    obj->cb_ = Persistent<Function>(isolate, callback);
    obj->isolate_ = isolate;

    obj->Wrap(args.This());
    args.GetReturnValue().Set(args.This());

  } else {
    // Invoked as plain function `MyObject(...)`, turn into construct call.
    const int argc = 1;
    Local<Value> argv[argc] = { args[0] };
    Local<Function> cons = Local<Function>::New(isolate, constructor);
    Local<Context> context = isolate->GetCurrentContext();
    Local<Object> instance =
        cons->NewInstance(context, argc, argv).ToLocalChecked();
    args.GetReturnValue().Set(instance);
  }
}

void VsamFile::NewInstance(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  const unsigned argc = 3;
  Local<Value> argv[argc] = { args[0] , args[1], args[2] };
  Local<Function> cons = Local<Function>::New(isolate, constructor);
  Local<Context> context = isolate->GetCurrentContext();
  Local<Object> instance =
      cons->NewInstance(context, argc, argv).ToLocalChecked();

  args.GetReturnValue().Set(instance);
}


void VsamFile::Close(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  if (args.Length() < 1) {
    // Throw an Error that is passed back to JavaScript
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }

  if (!args[0]->IsFunction()) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Wrong arguments")));
    return;
  }

  VsamFile* obj = ObjectWrap::Unwrap<VsamFile>(args.Holder());
  if (obj->stream_ == NULL) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "VSAM file is not open")));
    return;
  }

  uv_work_t* request = new uv_work_t;
  request->data = obj;

  obj->cb_ = Persistent<Function>(isolate, Handle<Function>::Cast(args[0]));
  obj->isolate_ = isolate;

  uv_queue_work(uv_default_loop(), request, Close, CloseCallback);
}


void VsamFile::Update(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  if (args.Length() < 2) {
    // Throw an Error that is passed back to JavaScript
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }

  if (!args[1]->IsFunction()) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Wrong arguments")));
    return;
  }

  Local<Object> record = args[0]->ToObject();
  VsamFile* obj = ObjectWrap::Unwrap<VsamFile>(args.Holder());
  obj->buf_ = malloc(obj->reclen_); //TODO: error
  char* buf = (char*)obj->buf_;
  for(auto i = obj->layout_.begin(); i != obj->layout_.end(); ++i) {
    if (i->type == LayoutItem::STRING) { 
      Local<String> field = Local<String>::Cast(record->Get(String::NewFromUtf8(obj->isolate_, &(i->name[0])))); //TODO: error if type is not string
      std::string key (*v8::String::Utf8Value(field));
      transform(key.begin(), key.end(), key.begin(), [](char c) -> char {
        __a2e_l(&c, 1);
        return c;
      });
      memcpy(buf, key.c_str(), key.length() + 1);
      buf += i->maxLength; //TODO: error if key length > maxLength
    }
  }

  uv_work_t* request = new uv_work_t;
  request->data = obj;

  obj->cb_ = Persistent<Function>(isolate, Handle<Function>::Cast(args[1]));
  obj->isolate_ = isolate;

  uv_queue_work(uv_default_loop(), request, Update, UpdateCallback);
}


void VsamFile::Find(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  if (args.Length() < 2) {
    // Throw an Error that is passed back to JavaScript
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }

  if (!args[0]->IsString() || !args[1]->IsFunction()) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Wrong arguments")));
    return;
  }

  std::string key (*v8::String::Utf8Value(args[0]->ToString()));
  transform(key.begin(), key.end(), key.begin(), [](char c) -> char {
    __a2e_l(&c, 1);
    return c;
  });

  VsamFile* obj = ObjectWrap::Unwrap<VsamFile>(args.Holder());
  uv_work_t* request = new uv_work_t;
  request->data = obj;

  obj->cb_ = Persistent<Function>(isolate, Handle<Function>::Cast(args[1]));
  obj->isolate_ = isolate;
  obj->key_ = key;

  uv_queue_work(uv_default_loop(), request, Find, ReadCallback);
}


void VsamFile::Read(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  if (args.Length() < 1) {
    // Throw an Error that is passed back to JavaScript
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }

  if (!args[0]->IsFunction()) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Wrong arguments")));
    return;
  }

  VsamFile* obj = ObjectWrap::Unwrap<VsamFile>(args.Holder());
  uv_work_t* request = new uv_work_t;
  request->data = obj;

  obj->cb_ = Persistent<Function>(isolate, Handle<Function>::Cast(args[0]));
  obj->isolate_ = isolate;

  uv_queue_work(uv_default_loop(), request, Read, ReadCallback);
}


void VsamFile::FileDescriptor(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  VsamFile* obj = ObjectWrap::Unwrap<VsamFile>(args.Holder());

  if (obj->stream_ == NULL) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "VSAM file is invalid")));
    return;
  }

  int fd = fileno(obj->stream_);
  if (fd == -1) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Cannot retrieve fd")));
    return;
  }

  args.GetReturnValue().Set(Number::New(isolate, fd));
}
