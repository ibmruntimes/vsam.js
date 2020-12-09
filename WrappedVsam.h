/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2017. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#pragma once
#include "VsamFile.h"
#include "VsamThread.h"
#include <napi.h>
#include <uv.h>

class WrappedVsam : public Napi::ObjectWrap<WrappedVsam> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  static Napi::Object OpenSync(const Napi::CallbackInfo &info);
  static Napi::Object AllocSync(const Napi::CallbackInfo &info);
  static Napi::Boolean Exist(const Napi::CallbackInfo &info);

  WrappedVsam(const Napi::CallbackInfo &info);
  ~WrappedVsam();

private:
  static Napi::Object Construct(const Napi::CallbackInfo &info, bool alloc);

  void deleteVsamFileObj();
  bool validateStr(const LayoutItem &item, const std::string &str);
  bool validateHexBuf(const LayoutItem &item, const char *buf, int len);
  bool validateHexStr(const LayoutItem &item, const std::string &hexstr);

  /* Entry point from Javascript */
  void Close(const Napi::CallbackInfo &info);
  void Read(const Napi::CallbackInfo &info);
  void Find(const Napi::CallbackInfo &info, int equality);
  void FindEq(const Napi::CallbackInfo &info);
  void FindGe(const Napi::CallbackInfo &info);
  void FindFirst(const Napi::CallbackInfo &info);
  void FindLast(const Napi::CallbackInfo &info);
  void Update(const Napi::CallbackInfo &info);
  void Write(const Napi::CallbackInfo &info);
  void Delete(const Napi::CallbackInfo &info);
  void Dealloc(const Napi::CallbackInfo &info);

  /* Work functions */
  static void DeallocExecute(uv_work_t *req);
  static void ReadExecute(uv_work_t *req);
  static void FindExecute(uv_work_t *req);
  static void UpdateExecute(uv_work_t *req);
  static void WriteExecute(uv_work_t *req);
  static void DeleteExecute(uv_work_t *req);

  /* Work callback functions */
  static void DefaultComplete(uv_work_t *req, int status);
  static void DeallocComplete(uv_work_t *req, int status);
  static void ReadComplete(uv_work_t *req, int status);
  static void UpdateComplete(uv_work_t *req, int status);
  static void WriteComplete(uv_work_t *req, int status);
  static void DeleteComplete(uv_work_t *req, int status);

private:
  static Napi::FunctionReference constructor_;
  VsamFile *pVsamFile_;
  std::string path_; // for Dealloc(), pVsamFile_ is deleted in Close()
};
