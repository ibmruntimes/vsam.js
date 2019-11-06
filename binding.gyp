{
  "targets": [
    {
      "target_name": "vsam.js",
      "cflags!": [ "-fno-exceptions", "-qxclang=-fexec-charset=ISO8859-1" ],
      "cflags_cc!": [ "-fno-exceptions", "-qxclang=-fexec-charset=ISO8859-1" ],
      "cflags": [ "-qascii" ],
      "cflags_cc": [ "-qascii" ],
      "xcode_settings": { "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.7",
      },
      "msvs_settings": {
        "VCCLCompilerTool": { "ExceptionHandling": 1 },
      },
      "include_dirs": [
         "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": [
         "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "xcode_settings": { "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.7",
      },
      "msvs_settings": {
        "VCCLCompilerTool": { "ExceptionHandling": 1 },
      },
      "sources": [ "vsam.cpp", "VsamFile.cpp" ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
    }
  ]
}
