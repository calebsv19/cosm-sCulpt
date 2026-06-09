#pragma once

#include <stdbool.h>
#include <stddef.h>

bool LayoutImportedMeshAsset_ImportStlToRuntime(const char* stl_path,
                                                const char* asset_root,
                                                char* out_authoring_path,
                                                size_t out_authoring_path_size,
                                                char* out_runtime_path,
                                                size_t out_runtime_path_size,
                                                char* diagnostics,
                                                size_t diagnostics_size);
