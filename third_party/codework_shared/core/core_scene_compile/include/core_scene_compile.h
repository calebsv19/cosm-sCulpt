#ifndef CORE_SCENE_COMPILE_H
#define CORE_SCENE_COMPILE_H

#include <stddef.h>

#include "core_base.h"

#define CORE_SCENE_COMPILE_VERSION "0.5.0"
#define CORE_SCENE_COMPILE_SHA256_HEX_SIZE 65u

typedef struct CoreSceneCompileOptions {
    const char *dependency_digest_sha256;
    size_t dependency_count;
} CoreSceneCompileOptions;

typedef struct CoreSceneCompileProvenance {
    char authoring_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char runtime_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char dependency_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    size_t dependency_count;
    const char *compiler_version;
    const char *normalization_version;
} CoreSceneCompileProvenance;

typedef struct CoreSceneCompileBundlePaths {
    char scene_dir[1024];
    char authoring_path[1024];
    char runtime_path[1024];
    char receipt_path[1024];
    char bundle_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
} CoreSceneCompileBundlePaths;

#ifdef __cplusplus
extern "C" {
#endif

CoreResult core_scene_compile_authoring_to_runtime(const char *authoring_json,
                                                   char **out_runtime_json,
                                                   char *diagnostics,
                                                   size_t diagnostics_size);

CoreResult core_scene_compile_authoring_to_runtime_with_provenance(
    const char *authoring_json,
    const CoreSceneCompileOptions *options,
    char **out_runtime_json,
    CoreSceneCompileProvenance *out_provenance,
    char *diagnostics,
    size_t diagnostics_size);

CoreResult core_scene_compile_sha256(const void *data,
                                     size_t data_size,
                                     char out_hex[CORE_SCENE_COMPILE_SHA256_HEX_SIZE]);

CoreResult core_scene_compile_publish_bundle(const char *authoring_json,
                                             const CoreSceneCompileOptions *options,
                                             const char *final_scene_dir,
                                             CoreSceneCompileBundlePaths *out_paths,
                                             char *diagnostics,
                                             size_t diagnostics_size);

CoreResult core_scene_compile_authoring_file_to_runtime_file(const char *authoring_path,
                                                             const char *runtime_path,
                                                             char *diagnostics,
                                                             size_t diagnostics_size);

#ifdef __cplusplus
}
#endif

#endif
