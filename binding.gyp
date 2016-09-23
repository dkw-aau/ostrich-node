{
  "targets": [
    {
      "target_name": "hdt",
      "sources": [
        "lib/ostrich.cc",
        "lib/OstrichDocument.cc",
        "<!@(ls -1 deps/ostrich/src/main/cpp/controller/*.cc)",
        "<!@(ls -1 deps/ostrich/src/main/cpp/patch/*.cc)",
        "<!@(ls -1 deps/ostrich/src/main/cpp/dictionary/*.cc)",
        "<!@(ls -1 deps/ostrich/src/main/cpp/snapshot/*.cc)",
        "<!@(ls -1 deps/ostrich/src/main/cpp/*.cc)",
        "<!@(ls -1 deps/ostrich/deps/hdt/hdt-lib/src/bitsequence/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/hdt-lib/src/dictionary/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/hdt-lib/src/hdt/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/hdt-lib/src/header/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/hdt-lib/src/huffman/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/hdt-lib/src/libdcs/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/hdt-lib/src/libdcs/fmindex/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/hdt-lib/src/rdf/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/hdt-lib/src/sequence/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/hdt-lib/src/triples/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/hdt-lib/src/util/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/libcds-v1.0.12/src/static/bitsequence/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/libcds-v1.0.12/src/static/coders/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/libcds-v1.0.12/src/static/mapper/*.cpp)",
        "<!@(ls -1 deps/ostrich/deps/hdt/libcds-v1.0.12/src/static/sequence/*.cpp)",
      ],
      "sources!": [
        "<!@(ls -1 deps/ostrich/deps/hdt/libcds-v1.0.12/src/static/sequence/*S.cpp)",
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")",
        "deps/ostrich/deps/hdt/hdt-lib/include",
        "deps/ostrich/deps/hdt/hdt-lib/src/dictionary/",
        "deps/ostrich/deps/hdt/libcds-v1.0.12/src/static/bitsequence",
        "deps/ostrich/deps/hdt/libcds-v1.0.12/src/static/coders",
        "deps/ostrich/deps/hdt/libcds-v1.0.12/src/static/mapper",
        "deps/ostrich/deps/hdt/libcds-v1.0.12/src/static/permutation",
        "deps/ostrich/deps/hdt/libcds-v1.0.12/src/static/sequence",
        "deps/ostrich/deps/hdt/libcds-v1.0.12/src/utils",
      ],
      "defines": [
        "HAVE_CDS",
      ],
      "cflags!":    [ "-fno-rtti", "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-rtti", "-fno-exceptions" ],
      "xcode_settings": {
        "GCC_ENABLE_CPP_RTTI": "YES",
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
      },
    },
  ],
}
