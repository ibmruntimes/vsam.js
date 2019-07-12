/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2017. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
*/

#ifndef VSAM_H
#define VSAM_H

#include <node.h>
#include <node_object_wrap.h>
#include <uv.h>
#include <string>

class VsamFile : public node::ObjectWrap {
 public:
  static void Init(v8::Isolate* isolate);
  static void OpenSync(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AllocSync(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Exist(const v8::FunctionCallbackInfo<v8::Value>& args);
  static v8::Persistent<v8::Function> OpenSyncConstructor;
  static v8::Persistent<v8::Function> AllocSyncConstructor;
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
    LayoutItem(v8::String::Utf8Value& n, int m, DataType t) :
      name(n.length()), maxLength(m), type(t) {
      memcpy(&name[0], *n, n.length());
    }
  };

  explicit VsamFile(std::string&, std::vector<LayoutItem>&,
                    v8::Isolate* isolate, bool alloc, int key_i);

  /* Entry point from Javascript */
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Read(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Find(const v8::FunctionCallbackInfo<v8::Value>& args, int equality);
  static void FindEq(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FindGe(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FindFirst(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FindLast(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Update(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Write(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Delete(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Dealloc(const v8::FunctionCallbackInfo<v8::Value>& args);

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
  v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>> cb_;
  static void OpenCallback(uv_work_t* req, int statusj);
  static void AllocCallback(uv_work_t* req, int statusj);
  static void DeallocCallback(uv_work_t* req, int statusj);
  static void ReadCallback(uv_work_t* req, int status);
  static void UpdateCallback(uv_work_t* req, int status);
  static void WriteCallback(uv_work_t* req, int status);
  static void DeleteCallback(uv_work_t* req, int status);

  /* Private methods */
  static void SetPrototypeMethods(v8::Local<v8::FunctionTemplate>& tpl);
  static void Construct(const v8::FunctionCallbackInfo<v8::Value>& args, bool alloc);

  /* Data */
  v8::Isolate* isolate_;
  std::string path_;
  std::string key_;
  std::vector<LayoutItem> layout_;
  int key_i_;
  unsigned keylen_, reclen_;
  FILE *stream_;
  void *buf_;
  int lastrc_;
  int equality_;
  std::string errmsg_;
};

#endif
