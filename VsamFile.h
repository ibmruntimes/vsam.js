/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2017. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
*/

#pragma once
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>
#include <uv.h>
#include <string>

class VsamFile : public Napi::ObjectWrap<VsamFile> {
 public:
  static void Init(Napi::Env env, Napi::Object exports);
  VsamFile(const Napi::CallbackInfo& info);

  static Napi::Value OpenSync(const Napi::CallbackInfo& info);
  static Napi::Value AllocSync(const Napi::CallbackInfo& info);
  static Napi::Boolean Exist(const Napi::CallbackInfo& info);
  ~VsamFile();

 private:
  struct LayoutItem {
    enum DataType {
      STRING,
      HEXADECIMAL
    };

    std::vector<char> name;
    int maxLength;
    DataType type;
    LayoutItem(std::string& n, int m, DataType t) :
      name(n.length()+1), maxLength(m), type(t) {
      strcpy(&name[0], n.c_str());
    }
  };

  /* Entry point from Javascript */
  void Close(const Napi::CallbackInfo& info);
  void Read(const Napi::CallbackInfo& info);
  void Find(const Napi::CallbackInfo& info, int equality);
  void FindEq(const Napi::CallbackInfo& info);
  void FindGe(const Napi::CallbackInfo& info);
  void FindFirst(const Napi::CallbackInfo& info);
  void FindLast(const Napi::CallbackInfo& info);
  void Update(const Napi::CallbackInfo& info);
  void Write(const Napi::CallbackInfo& info);
  void Delete(const Napi::CallbackInfo& info);
  void Dealloc(const Napi::CallbackInfo& info);

  /* Work functions */
  static void Open(uv_work_t* req);
  static void Alloc(uv_work_t* req);
  static void Dealloc(uv_work_t* req);
  static void Read(uv_work_t* req);
  static void Find(uv_work_t* req);
  static void Update(uv_work_t* req);
  static void Write(uv_work_t* req);
  static void Delete(uv_work_t* req);

  /* Work callback functions */
  static void OpenCallback(uv_work_t* req, int statusj);
  static void AllocCallback(uv_work_t* req, int statusj);
  static void DeallocCallback(uv_work_t* req, int statusj);
  static void ReadCallback(uv_work_t* req, int status);
  static void UpdateCallback(uv_work_t* req, int status);
  static void WriteCallback(uv_work_t* req, int status);
  static void DeleteCallback(uv_work_t* req, int status);

  /* Private methods */
  static Napi::Value Construct(const Napi::CallbackInfo& info, bool alloc);

  /* Data */
  static Napi::FunctionReference constructor_;
  Napi::Env env_;
  Napi::FunctionReference cb_;
  std::string path_;
  std::string omode_;
  std::string key_;
  char* keybuf_;
  int keybuf_len_;
  std::vector<LayoutItem> layout_;
  int key_i_;
  unsigned keylen_, reclen_;
  FILE *stream_;
  char* buf_;
  int lastrc_;
  int equality_;
  std::string errmsg_;
};
