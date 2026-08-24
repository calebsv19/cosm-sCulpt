#pragma once

#include <stdbool.h>
#include <stddef.h>

#define SCULPT_SCENE_PACKAGE_FILENAME "scene_package.json"
#define SCULPT_SCENE_PACKAGE_SCHEMA "sculpt_scene_package_v1"

typedef enum SculptScenePackageInputKind {
    SCULPT_SCENE_PACKAGE_INPUT_INVALID = 0,
    SCULPT_SCENE_PACKAGE_INPUT_LAYOUT_JSON,
    SCULPT_SCENE_PACKAGE_INPUT_AUTHORING
} SculptScenePackageInputKind;

typedef enum SculptScenePackageRuntimeState {
    SCULPT_SCENE_PACKAGE_RUNTIME_MISSING = 0,
    SCULPT_SCENE_PACKAGE_RUNTIME_STALE,
    SCULPT_SCENE_PACKAGE_RUNTIME_COMPILED
} SculptScenePackageRuntimeState;

typedef enum SculptScenePackageReceiptState {
    SCULPT_SCENE_PACKAGE_RECEIPT_MISSING = 0,
    SCULPT_SCENE_PACKAGE_RECEIPT_STALE,
    SCULPT_SCENE_PACKAGE_RECEIPT_VERIFIED
} SculptScenePackageReceiptState;

typedef struct SculptScenePackagePaths {
    char scene_dir[1024];
    char package_path[1024];
    char authoring_path[1024];
    char runtime_path[1024];
    char dependency_manifest_path[1024];
    char receipt_path[1024];
    char project_path[1024];
    bool has_package_manifest;
    bool has_runtime;
    bool has_receipt;
} SculptScenePackagePaths;

/* Derives the stable Sculpt scene id used by authoring, package, and runtime
 * export paths from a user-visible scene iteration name. */
bool SculptScenePackage_BuildSceneId(const char* scene_name,
                                     char* out_scene_id,
                                     size_t out_scene_id_size);

bool SculptScenePackage_BuildSceneIdFromAuthoringPath(const char* authoring_path,
                                                      char* out_scene_id,
                                                      size_t out_scene_id_size);

/* Builds the deterministic Sculpt-owned package entrypoint. The shared scene
 * compiler may retain these bytes inside its atomic publication transaction. */
bool SculptScenePackage_BuildManifestJson(const char* scene_id,
                                          SculptScenePackageRuntimeState runtime_state,
                                          SculptScenePackageReceiptState receipt_state,
                                          bool project_metadata_present,
                                          char** out_json,
                                          char* diagnostics,
                                          size_t diagnostics_size);

void SculptScenePackage_FreeManifestJson(char* json);

/* Writes an authoring package descriptor atomically for Save As and legacy
 * project roots that are not published by the shared bundle transaction. */
bool SculptScenePackage_WriteManifest(const char* scene_dir,
                                      const char* scene_id,
                                      SculptScenePackageRuntimeState runtime_state,
                                      SculptScenePackageReceiptState receipt_state,
                                      bool project_metadata_present,
                                      char* diagnostics,
                                      size_t diagnostics_size);

/* Resolves descriptor, directory, canonical authoring, and legacy loose-layout
 * inputs without treating compiled runtime JSON as editable source. */
bool SculptScenePackage_ResolveInput(const char* input_path,
                                     SculptScenePackageInputKind* out_kind,
                                     SculptScenePackagePaths* out_paths,
                                     char* diagnostics,
                                     size_t diagnostics_size);
