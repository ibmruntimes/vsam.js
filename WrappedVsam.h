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

// TODO(gabylb): this and throwError() should probably be refactored
// if more arguments were to be passed to the callback.
enum CbFirstArgType {
  ARG0_TYPE_NONE,
  ARG0_TYPE_0,
  ARG0_TYPE_NULL,
  ARG0_TYPE_ERR
};

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
  static Napi::Value createRecordObject(UvWorkData *pdata);

  void deleteVsamFileObj();
  bool validateStr(const LayoutItem &item, const std::string &str);
  bool validateHexBuf(const LayoutItem &item, const char *buf, int len);
  bool validateHexStr(const LayoutItem &item, const std::string &hexstr);

  /* Entry point from Javascript */
  void Close(const Napi::CallbackInfo &info);
  void Read(const Napi::CallbackInfo &info);
  void FindEq(const Napi::CallbackInfo &info);
  void FindGe(const Napi::CallbackInfo &info);
  void FindFirst(const Napi::CallbackInfo &info);
  void FindLast(const Napi::CallbackInfo &info);
  void Update(const Napi::CallbackInfo &info);
  void Write(const Napi::CallbackInfo &info);
  void Delete(const Napi::CallbackInfo &info);
  void Dealloc(const Napi::CallbackInfo &info);

  /* Helpers for Entry point from Javascript */
  int Write_(const Napi::CallbackInfo &info, const char *pApiName,
             UvWorkData **ppdata = nullptr);
  int Update_(const Napi::CallbackInfo &info, const char *pApiname,
              UvWorkData **ppdata = nullptr);
  int Delete_(const Napi::CallbackInfo &info, UvWorkData **ppdata = nullptr);
  int FindUpdate_(const Napi::CallbackInfo &info, const char *pApiName,
                  UvWorkData **ppdata = nullptr, const int recArg = -1,
                  const int cbArg = -1);
  int FindDelete_(const Napi::CallbackInfo &info, const char *pApiName,
                  UvWorkData **ppdata = nullptr, const int cbArg = -1);
  Napi::Value FindSync_(const Napi::CallbackInfo &info, UvWorkData *pdata);

  Napi::Value ReadSync(const Napi::CallbackInfo &info);
  Napi::Value FindEqSync(const Napi::CallbackInfo &info);
  Napi::Value FindGeSync(const Napi::CallbackInfo &info);
  Napi::Value FindFirstSync(const Napi::CallbackInfo &info);
  Napi::Value FindLastSync(const Napi::CallbackInfo &info);
  Napi::Value UpdateSync(const Napi::CallbackInfo &info);
  Napi::Value WriteSync(const Napi::CallbackInfo &info);
  Napi::Value DeleteSync(const Napi::CallbackInfo &info);

  /* Work functions */
  static void DeallocExecute(uv_work_t *req);
  static void ReadExecute(uv_work_t *req);
  static void FindExecute(uv_work_t *req);
  static void UpdateExecute(uv_work_t *req);
  static void FindUpdateExecute(uv_work_t *req);
  static void FindDeleteExecute(uv_work_t *req);
  static void WriteExecute(uv_work_t *req);
  static void DeleteExecute(uv_work_t *req);

  /* Work callback functions */
  static void DefaultComplete(uv_work_t *req, int status);
  static void DeallocComplete(uv_work_t *req, int status);
  static void ReadComplete(uv_work_t *req, int status);
  static void UpdateComplete(uv_work_t *req, int status);
  static void FindUpdateComplete(uv_work_t *req, int status);
  static void FindDeleteComplete(uv_work_t *req, int status);
  static void WriteComplete(uv_work_t *req, int status);
  static void DeleteComplete(uv_work_t *req, int status);

  int Find(const Napi::CallbackInfo &info, int equality, const char *pApiName,
           int callbackArg, UvWorkData **ppdata = nullptr,
           CbFirstArgType firstArgType = ARG0_TYPE_NULL,
           uv_work_cb pExecuteFunc = FindExecute,
           uv_after_work_cb pCompleteFunc = ReadComplete,
           char *pUpdateRecBuf = nullptr,
           std::vector<FieldToUpdate> *pFieldsToUpdate = nullptr);
  bool errorIfNotOpen(const Napi::CallbackInfo &info, int errArgNum,
                      CbFirstArgType firstArgType, const char *pApiName);

private:
  static Napi::FunctionReference constructor_;
  VsamFile *pVsamFile_;
  std::string path_; // for Dealloc(), pVsamFile_ is deleted in Close()
};
