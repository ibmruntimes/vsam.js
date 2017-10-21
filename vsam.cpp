#include <node.h>
#include "VsamFile.h"

using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

void CreateObject(const FunctionCallbackInfo<Value>& args) {
  VsamFile::NewInstance(args);
}

void InitAll(Local<Object> exports, Local<Object> module) {
  VsamFile::Init(exports->GetIsolate());

  NODE_SET_METHOD(module, "exports", CreateObject);
}

NODE_MODULE(vsam, InitAll)
