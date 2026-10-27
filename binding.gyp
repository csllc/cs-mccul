{
  "targets": [
    {
      # Measurement Computing Universal Library for Linux, built from the
      # vendor/uldaq submodule as a static library and linked into the addon
      "target_name": "uldaq",
      "type": "static_library",
      "sources": [
        "<!@(node tools/uldaq-sources.js)"
      ],
      "include_dirs": [
        "vendor/uldaq/src"
      ],
      # Silence warnings from vendor code (our own sources keep -Wall -Wextra)
      "cflags": [
        "-w"
      ],
      # uldaq uses C++ exceptions and RTTI, which node-gyp disables by default
      "cflags_cc!": [
        "-fno-exceptions",
        "-fno-rtti"
      ],
      "cflags_cc": [
        "-fexceptions",
        "-frtti"
      ],
      "direct_dependent_settings": {
        "include_dirs": [
          "vendor/uldaq/src"
        ]
      },
      "link_settings": {
        "libraries": [
          "-lusb-1.0",
          "-lpthread"
        ]
      }
    },
    {
      "target_name": "<(module_name)",
      "sources": [
        "src/cs_mccul.c",
        "src/cs_mccul_helper.c",
        "src/napi_helper.c",
        "src/common_helper.c"
      ],
      "dependencies": [
        "uldaq"
      ],
      "conditions": [
        [
          "OS=='linux'",
          {
            # uldaq's main.cpp has a __attribute__((constructor)) lib_load()
            # that initializes its error map. Nothing references that object,
            # so force it in or the static link silently drops it.
            "ldflags": [
              "-Wl,-u,_Z8lib_loadv"
            ]
          }
        ]
      ]
    },
    {
      "target_name": "action_after_build",
      "type": "none",
      "dependencies": [
        "<(module_name)"
      ],
      "copies": [
        {
          "files": [
            "<(PRODUCT_DIR)/<(module_name).node"
          ],
          "destination": "<(module_path)"
        }
      ]
    }
  ]
}
