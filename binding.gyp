{
  "targets": [
    {
      "target_name": "vsam.js",
      "sources": [ "vsam.cpp", "WrappedVsam.cpp", "VsamFile.cpp", "VsamThread.cpp" ],
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
