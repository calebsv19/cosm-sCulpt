#include "Core/sculpt_scene_package.h"

#include "core_io.h"
#include "cjson/cJSON.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static void package_diag(char* diagnostics, size_t size, const char* message) {
    if (diagnostics && size > 0u) snprintf(diagnostics, size, "%s", message ? message : "");
}

static const char* package_basename(const char* path) {
    const char* slash = path ? strrchr(path, '/') : NULL;
    return slash ? slash + 1 : path;
}

static bool package_is_regular_file(const char* path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool package_is_directory(const char* path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool package_join(const char* root, const char* leaf, char* out, size_t out_size) {
    int written;
    if (!root || !root[0] || !leaf || !leaf[0] || !out || out_size == 0u) return false;
    written = snprintf(out, out_size, "%s%s%s", root,
                       root[strlen(root) - 1u] == '/' ? "" : "/", leaf);
    return written > 0 && (size_t)written < out_size;
}

static bool package_parent(const char* path, char* out, size_t out_size) {
    const char* slash = path ? strrchr(path, '/') : NULL;
    size_t length;
    if (!slash || slash == path || !out || out_size == 0u) return false;
    length = (size_t)(slash - path);
    if (length >= out_size) return false;
    memcpy(out, path, length);
    out[length] = '\0';
    return true;
}

static bool package_build_paths(const char* scene_dir, SculptScenePackagePaths* out) {
    if (!scene_dir || !scene_dir[0] || !out) return false;
    memset(out, 0, sizeof(*out));
    if (snprintf(out->scene_dir, sizeof(out->scene_dir), "%s", scene_dir) >=
            (int)sizeof(out->scene_dir) ||
        !package_join(scene_dir, SCULPT_SCENE_PACKAGE_FILENAME,
                      out->package_path, sizeof(out->package_path)) ||
        !package_join(scene_dir, "scene_authoring.json",
                      out->authoring_path, sizeof(out->authoring_path)) ||
        !package_join(scene_dir, "scene_runtime.json",
                      out->runtime_path, sizeof(out->runtime_path)) ||
        !package_join(scene_dir, "scene_dependencies.json",
                      out->dependency_manifest_path, sizeof(out->dependency_manifest_path)) ||
        !package_join(scene_dir, "scene_export_receipt.json",
                      out->receipt_path, sizeof(out->receipt_path)) ||
        !package_join(scene_dir, "scene_project.json",
                      out->project_path, sizeof(out->project_path))) {
        return false;
    }
    out->has_package_manifest = package_is_regular_file(out->package_path);
    out->has_runtime = package_is_regular_file(out->runtime_path);
    out->has_receipt = package_is_regular_file(out->receipt_path);
    return true;
}

bool SculptScenePackage_BuildSceneId(const char* scene_name,
                                     char* out_scene_id,
                                     size_t out_scene_id_size) {
    static const char* prefix = "scene_line_drawing";
    size_t used;
    if (!out_scene_id || out_scene_id_size == 0u) return false;
    if (!scene_name || !scene_name[0]) scene_name = "layout_export";
    used = strlen(prefix);
    if (used + 2u >= out_scene_id_size) return false;
    memcpy(out_scene_id, prefix, used);
    out_scene_id[used++] = '_';
    for (const char* p = scene_name; *p && used + 1u < out_scene_id_size; ++p) {
        const unsigned char c = (unsigned char)*p;
        out_scene_id[used++] = (char)((isalnum(c) || c == '_' || c == '-' || c == '.') ? c : '_');
    }
    out_scene_id[used] = '\0';
    return used > strlen(prefix) + 1u;
}

bool SculptScenePackage_BuildSceneIdFromAuthoringPath(const char* authoring_path,
                                                      char* out_scene_id,
                                                      size_t out_scene_id_size) {
    char scene_dir[1024];
    const char* scene_name;
    if (!package_parent(authoring_path, scene_dir, sizeof(scene_dir))) return false;
    scene_name = package_basename(scene_dir);
    return SculptScenePackage_BuildSceneId(scene_name, out_scene_id, out_scene_id_size);
}

bool SculptScenePackage_BuildManifestJson(const char* scene_id,
                                          SculptScenePackageRuntimeState runtime_state,
                                          SculptScenePackageReceiptState receipt_state,
                                          bool project_metadata_present,
                                          char** out_json,
                                          char* diagnostics,
                                          size_t diagnostics_size) {
    cJSON* root = NULL;
    cJSON* authoring = NULL;
    cJSON* runtime = NULL;
    cJSON* dependencies = NULL;
    cJSON* receipt = NULL;
    cJSON* project = NULL;
    char* json = NULL;
    if (out_json) *out_json = NULL;
    package_diag(diagnostics, diagnostics_size, NULL);
    if (!scene_id || !scene_id[0] || !out_json) {
        package_diag(diagnostics, diagnostics_size, "scene package identity is missing");
        return false;
    }
    root = cJSON_CreateObject();
    authoring = cJSON_CreateObject();
    runtime = cJSON_CreateObject();
    dependencies = cJSON_CreateObject();
    receipt = cJSON_CreateObject();
    if (!root || !authoring || !runtime || !dependencies || !receipt) goto fail;
    cJSON_AddStringToObject(root, "schema_family", "sculpt_scene_package");
    cJSON_AddStringToObject(root, "schema_variant", SCULPT_SCENE_PACKAGE_SCHEMA);
    cJSON_AddNumberToObject(root, "schema_version", 1);
    cJSON_AddStringToObject(root, "scene_id", scene_id);
    cJSON_AddStringToObject(authoring, "path", "scene_authoring.json");
    cJSON_AddStringToObject(authoring, "schema_variant", "scene_authoring_v1");
    cJSON_AddItemToObject(root, "authoring", authoring);
    authoring = NULL;
    cJSON_AddStringToObject(runtime, "path", "scene_runtime.json");
    cJSON_AddStringToObject(runtime, "schema_variant", "scene_runtime_v1");
    cJSON_AddStringToObject(runtime, "state",
                            runtime_state == SCULPT_SCENE_PACKAGE_RUNTIME_COMPILED
                                ? "compiled"
                                : (runtime_state == SCULPT_SCENE_PACKAGE_RUNTIME_STALE ? "stale" : "missing"));
    cJSON_AddItemToObject(root, "runtime", runtime);
    runtime = NULL;
    cJSON_AddStringToObject(dependencies, "path", "scene_dependencies.json");
    cJSON_AddStringToObject(dependencies, "state",
                            runtime_state == SCULPT_SCENE_PACKAGE_RUNTIME_COMPILED
                                ? "compiled"
                                : (runtime_state == SCULPT_SCENE_PACKAGE_RUNTIME_STALE ? "stale" : "missing"));
    cJSON_AddItemToObject(root, "dependencies", dependencies);
    dependencies = NULL;
    cJSON_AddStringToObject(receipt, "path", "scene_export_receipt.json");
    cJSON_AddStringToObject(receipt, "state",
                            receipt_state == SCULPT_SCENE_PACKAGE_RECEIPT_VERIFIED
                                ? "verified"
                                : (receipt_state == SCULPT_SCENE_PACKAGE_RECEIPT_STALE ? "stale" : "missing"));
    cJSON_AddItemToObject(root, "export_receipt", receipt);
    receipt = NULL;
    if (project_metadata_present) {
        project = cJSON_CreateObject();
        if (!project) goto fail;
        cJSON_AddStringToObject(project, "path", "scene_project.json");
        cJSON_AddItemToObject(root, "project", project);
        project = NULL;
    }
    json = cJSON_Print(root);
    cJSON_Delete(root);
    if (!json) goto fail_without_root;
    *out_json = json;
    return true;

fail:
    cJSON_Delete(root);
    cJSON_Delete(authoring);
    cJSON_Delete(runtime);
    cJSON_Delete(dependencies);
    cJSON_Delete(receipt);
    cJSON_Delete(project);
fail_without_root:
    package_diag(diagnostics, diagnostics_size, "failed to build scene package manifest");
    return false;
}

void SculptScenePackage_FreeManifestJson(char* json) {
    cJSON_free(json);
}

bool SculptScenePackage_WriteManifest(const char* scene_dir,
                                      const char* scene_id,
                                      SculptScenePackageRuntimeState runtime_state,
                                      SculptScenePackageReceiptState receipt_state,
                                      bool project_metadata_present,
                                      char* diagnostics,
                                      size_t diagnostics_size) {
    char path[1024];
    char* json = NULL;
    CoreResult result;
    if (!package_join(scene_dir, SCULPT_SCENE_PACKAGE_FILENAME, path, sizeof(path)) ||
        !SculptScenePackage_BuildManifestJson(scene_id,
                                              runtime_state,
                                              receipt_state,
                                              project_metadata_present,
                                              &json,
                                              diagnostics,
                                              diagnostics_size)) {
        return false;
    }
    result = core_io_write_all_atomic(path, json, strlen(json));
    SculptScenePackage_FreeManifestJson(json);
    if (result.code != CORE_OK) {
        package_diag(diagnostics, diagnostics_size, "failed to write scene package manifest");
        return false;
    }
    return true;
}

static bool package_validate_descriptor(const SculptScenePackagePaths* paths,
                                        char* diagnostics,
                                        size_t diagnostics_size) {
    CoreBuffer buffer = {0};
    cJSON* root = NULL;
    const cJSON* schema = NULL;
    const cJSON* authoring = NULL;
    const cJSON* authoring_path = NULL;
    char* text = NULL;
    bool ok = false;
    if (!paths || core_io_read_all(paths->package_path, &buffer).code != CORE_OK) {
        package_diag(diagnostics, diagnostics_size, "failed to read scene package manifest");
        return false;
    }
    text = (char*)malloc(buffer.size + 1u);
    if (!text) goto cleanup;
    if (buffer.size > 0u) memcpy(text, buffer.data, buffer.size);
    text[buffer.size] = '\0';
    root = cJSON_Parse(text);
    schema = root ? cJSON_GetObjectItemCaseSensitive(root, "schema_variant") : NULL;
    authoring = root ? cJSON_GetObjectItemCaseSensitive(root, "authoring") : NULL;
    authoring_path = authoring ? cJSON_GetObjectItemCaseSensitive(authoring, "path") : NULL;
    if (!cJSON_IsString(schema) || strcmp(schema->valuestring, SCULPT_SCENE_PACKAGE_SCHEMA) != 0 ||
        !cJSON_IsString(authoring_path) || strcmp(authoring_path->valuestring, "scene_authoring.json") != 0) {
        package_diag(diagnostics, diagnostics_size, "unsupported or unsafe scene package manifest");
        goto cleanup;
    }
    if (!package_is_regular_file(paths->authoring_path)) {
        package_diag(diagnostics, diagnostics_size, "scene package is missing scene_authoring.json");
        goto cleanup;
    }
    ok = true;
cleanup:
    cJSON_Delete(root);
    free(text);
    core_io_buffer_free(&buffer);
    return ok;
}

bool SculptScenePackage_ResolveInput(const char* input_path,
                                     SculptScenePackageInputKind* out_kind,
                                     SculptScenePackagePaths* out_paths,
                                     char* diagnostics,
                                     size_t diagnostics_size) {
    char scene_dir[1024];
    const char* base;
    size_t length;
    if (out_kind) *out_kind = SCULPT_SCENE_PACKAGE_INPUT_INVALID;
    if (out_paths) memset(out_paths, 0, sizeof(*out_paths));
    package_diag(diagnostics, diagnostics_size, NULL);
    if (!input_path || !input_path[0] || !out_kind || !out_paths) {
        package_diag(diagnostics, diagnostics_size, "scene package input is missing");
        return false;
    }
    if (package_is_directory(input_path)) {
        if (!package_build_paths(input_path, out_paths)) goto path_too_long;
    } else if (package_is_regular_file(input_path)) {
        base = package_basename(input_path);
        if (base && strcmp(base, SCULPT_SCENE_PACKAGE_FILENAME) == 0) {
            if (!package_parent(input_path, scene_dir, sizeof(scene_dir)) ||
                !package_build_paths(scene_dir, out_paths)) goto path_too_long;
        } else if (base && strcmp(base, "scene_authoring.json") == 0) {
            if (!package_parent(input_path, scene_dir, sizeof(scene_dir)) ||
                !package_build_paths(scene_dir, out_paths)) goto path_too_long;
        } else if (base && strcmp(base, "scene_runtime.json") == 0) {
            package_diag(diagnostics, diagnostics_size,
                         "scene_runtime.json is compiled output; load scene_authoring.json instead");
            return false;
        } else {
            length = base ? strlen(base) : 0u;
            if (length >= 5u && strcasecmp(base + length - 5u, ".json") == 0) {
                *out_kind = SCULPT_SCENE_PACKAGE_INPUT_LAYOUT_JSON;
                snprintf(out_paths->authoring_path, sizeof(out_paths->authoring_path), "%s", input_path);
                return true;
            }
            package_diag(diagnostics, diagnostics_size, "unsupported scene package input");
            return false;
        }
    } else {
        package_diag(diagnostics, diagnostics_size, "scene package input does not exist");
        return false;
    }
    if (out_paths->has_package_manifest &&
        !package_validate_descriptor(out_paths, diagnostics, diagnostics_size)) {
        return false;
    }
    if (!package_is_regular_file(out_paths->authoring_path)) {
        package_diag(diagnostics, diagnostics_size, "scene package is missing scene_authoring.json");
        return false;
    }
    *out_kind = SCULPT_SCENE_PACKAGE_INPUT_AUTHORING;
    return true;

path_too_long:
    package_diag(diagnostics, diagnostics_size, "scene package path is too long");
    return false;
}
