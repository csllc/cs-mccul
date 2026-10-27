{
  "targets": [
    {
      "target_name": "<(module_name)",
      "sources": [
        "src/cs_mccul.c",
        "src/cs_mccul_helper.c",
        "src/napi_helper.c",
        "src/common_helper.c"
      ],
      "conditions": [
        [
          "OS=='linux'",
          {
            "include_dirs": [
              "vendor/uldaq/src"
            ],
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

