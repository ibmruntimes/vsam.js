/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2017. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
*/

#include <node.h>
#include "VsamFile.h"

using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

void InitAll(Local<Object> exports, Local<Object> module) {
  VsamFile::Init(exports->GetIsolate());

  NODE_SET_METHOD(exports, "openSync", VsamFile::OpenSync);
  NODE_SET_METHOD(exports, "allocSync", VsamFile::AllocSync);
  NODE_SET_METHOD(exports, "exist", VsamFile::Exist);
}

NODE_MODULE(vsam, InitAll)
