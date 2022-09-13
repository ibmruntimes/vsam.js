{
  "variables": {
    "NODE_VERSION%":"<!(node -p \"process.versions.node.split(\\\".\\\")[0]\")"
  },
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
      "conditions": [
        [ "NODE_VERSION < 16", {
          "cflags": [ "-qascii" ],
          "defines": [ "_AE_BIMODAL=1", "_ALL_SOURCE", "_ENHANCED_ASCII_EXT=0x42020010", "_LARGE_TIME_API", "_OPEN_MSGQ_EXT", "_OPEN_SYS_FILE_EXT=1", "_OPEN_SYS_SOCK_IPV6", "_UNIX03_SOURCE", "_UNIX03_THREADS", "_UNIX03_WITHDRAWN", "_XOPEN_SOURCE=600", "_XOPEN_SOURCE_EXTENDED" ],
        }],
      ],
      "defines+": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
    }
  ]
}
