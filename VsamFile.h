#ifndef VSAM_H
#define VSAM_H

#include <node.h>
#include <node_object_wrap.h>
#include <uv.h>
#include <string>

class VsamFile : public node::ObjectWrap {
 public:
  static void Init(v8::Isolate* isolate);
  static void NewInstance(const v8::FunctionCallbackInfo<v8::Value>& args);
  static v8::Persistent<v8::Function> constructor;

 private:
  explicit VsamFile(const std::string& path);
  ~VsamFile();

  /* Entry point from Javascript */
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FileDescriptor(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Read(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Find(const v8::FunctionCallbackInfo<v8::Value>& args);

  /* Work functions */
  static void Close(uv_work_t* req);
  static void Open(uv_work_t* req);
  static void Read(uv_work_t* req);
  static void Find(uv_work_t* req);

  /* Work callback functions */
  v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>> cb_;
  static void OpenCallback(uv_work_t* req, int statusj);
  static void CloseCallback(uv_work_t* req, int status);
  static void ReadCallback(uv_work_t* req, int status);

  /* Data */
  v8::Isolate* isolate_;
  std::string path_;
  std::string key_;
  unsigned keylen_, reclen_;
  FILE *stream_;
  void *buf_;
};

#endif
