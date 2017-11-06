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
  ~VsamFile();

 private:
  struct LayoutItem {
    enum DataType {
      STRING
    };

    std::vector<char> name;
    int maxLength;
    DataType type;
    LayoutItem(v8::String::Utf8Value& n, int m, DataType t) :
      name(n.length()), maxLength(m), type(t) {
      memcpy(&name[0], *n, n.length());
    }
  };

  explicit VsamFile(const std::string& path);

  /* Entry point from Javascript */
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FileDescriptor(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Read(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Find(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Update(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Write(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Delete(const v8::FunctionCallbackInfo<v8::Value>& args);

  /* Work functions */
  static void Open(uv_work_t* req);
  static void Read(uv_work_t* req);
  static void Find(uv_work_t* req);
  static void Update(uv_work_t* req);
  static void Write(uv_work_t* req);
  static void Delete(uv_work_t* req);

  /* Work callback functions */
  v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>> cb_;
  static void OpenCallback(uv_work_t* req, int statusj);
  static void ReadCallback(uv_work_t* req, int status);
  static void UpdateCallback(uv_work_t* req, int status);
  static void WriteCallback(uv_work_t* req, int status);
  static void DeleteCallback(uv_work_t* req, int status);

  /* Data */
  v8::Isolate* isolate_;
  std::string path_;
  std::string key_;
  std::vector<LayoutItem> layout_;
  unsigned keylen_, reclen_;
  FILE *stream_;
  void *buf_;
  int lastrc_;
};

#endif
