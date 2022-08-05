/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2017, 2021. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#include <napi.h>

#include "WrappedVsam.h"


Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
  WrappedVsam::Init(env, exports);

  exports.Set(Napi::String::New(env, "openSync"),
              Napi::Function::New(env, WrappedVsam::OpenSync));
  exports.Set(Napi::String::New(env, "allocSync"),
              Napi::Function::New(env, WrappedVsam::AllocSync));
  exports.Set(Napi::String::New(env, "exist"),
              Napi::Function::New(env, WrappedVsam::Exist));
  return exports;
}

NODE_API_MODULE(vsam, InitAll)
