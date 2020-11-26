{
  "targets": [
    {
      "target_name": "vsam.js",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "cflags": [ "-qascii" ],
      "cflags_cc": [ "-qascii" ],
      "sources": [ "vsam.cpp", "WrappedVsam.cpp", "VsamFile.cpp" ],
      "include_dirs": [
         "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": [
         "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
    }
  ]
}
