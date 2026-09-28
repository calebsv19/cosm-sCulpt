#include "test_framework.h"

#include "Layout/layout.h"
#include "Layout/layout_json.h"
#include "Tools/canonical_scene_export.h"
#include "Tools/scene_import.h"
#include "Tools/scene_export.h"
#include "Tools/scene_project_export.h"
#include "cjson/cJSON.h"
#include "core_mesh_asset.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static bool file_contains(const char* path, const char* needle) {
    FILE* fp = NULL;
    char buffer[4096];
    size_t count = 0;
    if (!path || !needle) return false;
    fp = fopen(path, "rb");
    if (!fp) return false;
    count = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);
    buffer[count] = '\0';
    return strstr(buffer, needle) != NULL;
}

static char* read_text_file(const char* path) {
    FILE* fp = NULL;
    long len = 0;
    char* text = NULL;
    if (!path) return NULL;
    fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    len = ftell(fp);
    if (len < 0) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    text = (char*)malloc((size_t)len + 1u);
    if (!text) {
        fclose(fp);
        return NULL;
    }
    if (fread(text, 1u, (size_t)len, fp) != (size_t)len) {
        free(text);
        fclose(fp);
        return NULL;
    }
    text[len] = '\0';
    fclose(fp);
    return text;
}

static bool write_text_file(const char* path, const char* text) {
    FILE* fp = NULL;
    size_t len = 0u;
    if (!path || !text) return false;
    fp = fopen(path, "wb");
    if (!fp) return false;
    len = strlen(text);
    if (fwrite(text, 1u, len, fp) != len) {
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;
}

static bool path_exists(const char* path) {
    return path && access(path, F_OK) == 0;
}

static bool build_fixture_path(const char* root, const char* leaf, char* out_path, size_t out_path_size) {
    int written = 0;
    if (!root || !leaf || !out_path || out_path_size == 0u) return false;
    written = snprintf(out_path, out_path_size, "%s/%s", root, leaf);
    return written > 0 && (size_t)written < out_path_size;
}

static bool json_string_equals(const cJSON* object, const char* key, const char* expected) {
    const cJSON* item = NULL;
    if (!object || !key || !expected) return false;
    item = cJSON_GetObjectItem(object, key);
    return cJSON_IsString(item) && item->valuestring && strcmp(item->valuestring, expected) == 0;
}

static bool test_scene_export_emits_authoring_and_runtime_files(void) {
    char root_template[] = "/tmp/ld_scene_export_basic_XXXXXX";
    char* root = NULL;
    Layout layout;
    LineDrawingSceneExportPaths export_paths;
    CoreSceneCompileVerification verification = {0};
    char diagnostics[256];

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);

    Layout_Init(&layout, 1.0f);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){0.0f, 0.0f, 0.0f}) >= 0);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){2.0f, 1.0f, 0.0f}) >= 0);
    Layout_AddWall3(&layout, (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){2.0f, 1.0f, 0.0f});

    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToOutputRoot(&layout,
                                                                "tests/fixtures/demo room.json",
                                                                root,
                                                                &export_paths,
                                                                diagnostics,
                                                                sizeof(diagnostics)));
    TEST_ASSERT(strstr(export_paths.scene_dir, "/demo room") != NULL);
    TEST_ASSERT(strstr(export_paths.authoring_path, "/demo room/scene_authoring.json") != NULL);
    TEST_ASSERT(strstr(export_paths.runtime_path, "/demo room/scene_runtime.json") != NULL);
    TEST_ASSERT(strstr(export_paths.dependency_manifest_path,
                       "/demo room/scene_dependencies.json") != NULL);
    TEST_ASSERT(strstr(export_paths.receipt_path, "/demo room/scene_export_receipt.json") != NULL);
    TEST_ASSERT(strstr(export_paths.package_manifest_path, "/demo room/scene_package.json") != NULL);
    TEST_ASSERT(strlen(export_paths.bundle_sha256) == 64u);
    TEST_ASSERT(strlen(export_paths.dependency_sha256) == 64u);
    TEST_ASSERT(export_paths.dependency_count == 0u);
    TEST_ASSERT(strstr(export_paths.scene_id, "scene_line_drawing_demo_room") != NULL);
    TEST_ASSERT(file_contains(export_paths.authoring_path, "\"scene_authoring_v1\""));
    TEST_ASSERT(file_contains(export_paths.runtime_path, "\"schema_variant\":\"scene_runtime_v1\""));
    TEST_ASSERT(file_contains(export_paths.runtime_path, "\"authoring_sha256\":"));
    TEST_ASSERT(file_contains(export_paths.dependency_manifest_path,
                              "\"scene_dependency_manifest_v2\""));
    TEST_ASSERT(file_contains(export_paths.receipt_path, "\"scene_export_receipt_v1\""));
    TEST_ASSERT(file_contains(export_paths.receipt_path, export_paths.bundle_sha256));
    TEST_ASSERT(file_contains(export_paths.package_manifest_path, "\"sculpt_scene_package_v1\""));
    TEST_ASSERT(file_contains(export_paths.package_manifest_path, "\"compiled\""));
    TEST_ASSERT(file_contains(export_paths.package_manifest_path, "\"verified\""));
    TEST_ASSERT(file_contains(export_paths.receipt_path, "\"package_manifest\""));
    TEST_ASSERT(core_scene_compile_verify_bundle(export_paths.scene_dir,
                                                  export_paths.bundle_sha256,
                                                  &verification,
                                                  diagnostics,
                                                  sizeof(diagnostics)).code == CORE_OK);
    TEST_ASSERT(verification.package_manifest_bytes > 0u);
    TEST_ASSERT(strlen(verification.package_manifest_sha256) == 64u);
    TEST_ASSERT(write_text_file(export_paths.package_manifest_path, "{}"));
    TEST_ASSERT(core_scene_compile_verify_bundle(export_paths.scene_dir,
                                                  export_paths.bundle_sha256,
                                                  NULL,
                                                  diagnostics,
                                                  sizeof(diagnostics)).code != CORE_OK);

    Layout_Free(&layout);
    (void)unlink(export_paths.authoring_path);
    (void)unlink(export_paths.runtime_path);
    (void)unlink(export_paths.dependency_manifest_path);
    (void)unlink(export_paths.receipt_path);
    (void)unlink(export_paths.package_manifest_path);
    (void)rmdir(export_paths.scene_dir);
    (void)rmdir(root);
    return true;
}

static bool test_scene_export_uses_parent_scene_name_for_authoring_hint(void) {
    char root_template[] = "/tmp/ld_scene_export_parent_XXXXXX";
    char* root = NULL;
    char layout_path_hint[512];
    Layout layout;
    LineDrawingSceneExportPaths export_paths;
    char diagnostics[256];

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);

    Layout_Init(&layout, 1.0f);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){0.0f, 0.0f, 0.0f}) >= 0);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){3.0f, 1.0f, 0.0f}) >= 0);
    Layout_AddWall3(&layout, (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){3.0f, 1.0f, 0.0f});
    TEST_ASSERT(snprintf(layout_path_hint,
                         sizeof(layout_path_hint),
                         "%s/imported bodyparts/scene_authoring.json",
                         root) < (int)sizeof(layout_path_hint));

    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToOutputRoot(&layout,
                                                                layout_path_hint,
                                                                root,
                                                                &export_paths,
                                                                diagnostics,
                                                                sizeof(diagnostics)));
    TEST_ASSERT(strstr(export_paths.scene_dir, "imported bodyparts") != NULL);
    TEST_ASSERT(strstr(export_paths.scene_dir, "scene_authoring") == NULL);
    TEST_ASSERT(strstr(export_paths.authoring_path,
                       "imported bodyparts/scene_authoring.json") != NULL);
    TEST_ASSERT(strstr(export_paths.runtime_path,
                       "imported bodyparts/scene_runtime.json") != NULL);
    TEST_ASSERT(file_contains(export_paths.authoring_path, "\"scene_authoring_v1\""));
    TEST_ASSERT(file_contains(export_paths.runtime_path, "\"schema_variant\":\"scene_runtime_v1\""));
    TEST_ASSERT(file_contains(export_paths.receipt_path, "\"scene_export_receipt_v1\""));

    remove(export_paths.authoring_path);
    remove(export_paths.runtime_path);
    remove(export_paths.receipt_path);
    rmdir(export_paths.scene_dir);
    rmdir(root);
    Layout_Free(&layout);
    return true;
}

static bool test_scene_export_refuses_to_replace_existing_bundle(void) {
    char root_template[] = "/tmp/ld_scene_export_collision_XXXXXX";
    char* root = mkdtemp(root_template);
    Layout layout;
    LineDrawingSceneExportPaths export_paths;
    char diagnostics[256];
    char* original_authoring = NULL;
    char* original_receipt = NULL;
    char* current_authoring = NULL;
    char* current_receipt = NULL;

    TEST_ASSERT(root != NULL);
    Layout_Init(&layout, 1.0f);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){0.0f, 0.0f, 0.0f}) >= 0);
    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToOutputRoot(&layout,
                                                                "iteration.json",
                                                                root,
                                                                &export_paths,
                                                                diagnostics,
                                                                sizeof(diagnostics)));
    original_authoring = read_text_file(export_paths.authoring_path);
    original_receipt = read_text_file(export_paths.receipt_path);
    TEST_ASSERT(original_authoring != NULL);
    TEST_ASSERT(original_receipt != NULL);

    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){9.0f, 8.0f, 7.0f}) >= 0);
    TEST_ASSERT(!LineDrawingSceneExport_ExportLayoutToOutputRoot(&layout,
                                                                 "iteration.json",
                                                                 root,
                                                                 NULL,
                                                                 diagnostics,
                                                                 sizeof(diagnostics)));
    TEST_ASSERT(strstr(diagnostics, "already exists") != NULL);
    current_authoring = read_text_file(export_paths.authoring_path);
    current_receipt = read_text_file(export_paths.receipt_path);
    TEST_ASSERT(current_authoring != NULL && strcmp(original_authoring, current_authoring) == 0);
    TEST_ASSERT(current_receipt != NULL && strcmp(original_receipt, current_receipt) == 0);

    free(original_authoring);
    free(original_receipt);
    free(current_authoring);
    free(current_receipt);
    unlink(export_paths.receipt_path);
    unlink(export_paths.dependency_manifest_path);
    unlink(export_paths.runtime_path);
    unlink(export_paths.authoring_path);
    rmdir(export_paths.scene_dir);
    rmdir(root);
    Layout_Free(&layout);
    return true;
}

static bool test_scene_export_collects_and_verifies_mesh_dependencies(void) {
    char root_template[] = "/tmp/ld_scene_export_dependencies_XXXXXX";
    char* root = mkdtemp(root_template);
    char mesh_path[512];
    char packaged_relative[CORE_SCENE_COMPILE_DEPENDENCY_PATH_SIZE];
    char packaged_path[1024];
    char packaged_kind_dir[1024];
    char packaged_root_dir[1024];
    char diagnostics[256];
    const char* mesh_bytes = "fixture-runtime-mesh-bytes";
    char expected_digest[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    Layout layout;
    Transform3D transform;
    Object3D* object = NULL;
    uint32_t object_id;
    LineDrawingSceneExportPaths export_paths;
    CoreSceneCompileVerification verification = {0};

    TEST_ASSERT(root != NULL);
    TEST_ASSERT(build_fixture_path(root, "mesh.runtime.json", mesh_path, sizeof(mesh_path)));
    TEST_ASSERT(write_text_file(mesh_path, mesh_bytes));
    TEST_ASSERT(core_scene_compile_sha256(mesh_bytes, strlen(mesh_bytes), expected_digest).code == CORE_OK);
    Layout_Init(&layout, 1.0f);
    transform = Layout_Transform3D_Default();
    object_id = Layout_ObjectStore_Create(&layout.objectStore,
                                          OBJECT3D_KIND_MESH_ASSET_INSTANCE,
                                          &transform,
                                          "mesh_asset_instance",
                                          CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                          CORE_OBJECT_PLANE_XY);
    TEST_ASSERT(object_id != 0u);
    object = Layout_ObjectStore_Find(&layout.objectStore, object_id);
    TEST_ASSERT(object != NULL);
    snprintf(object->meshInstance.assetId, sizeof(object->meshInstance.assetId), "%s", "mesh_fixture");
    snprintf(object->meshInstance.runtimePath, sizeof(object->meshInstance.runtimePath), "%s", mesh_path);
    object->meshInstance.vertexCount = 3u;
    object->meshInstance.triangleCount = 1u;
    object->meshInstance.localBoundsMin = (Vec3){0.0f, 0.0f, 0.0f};
    object->meshInstance.localBoundsMax = (Vec3){1.0f, 1.0f, 1.0f};
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));

    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToOutputRoot(&layout,
                                                                "mesh_dependency_scene.json",
                                                                root,
                                                                &export_paths,
                                                                diagnostics,
                                                                sizeof(diagnostics)));
    TEST_ASSERT(export_paths.dependency_count == 1u);
    TEST_ASSERT(file_contains(export_paths.dependency_manifest_path, "\"identity\":\"mesh_fixture\""));
    TEST_ASSERT(file_contains(export_paths.dependency_manifest_path, expected_digest));
    TEST_ASSERT(core_scene_compile_dependency_payload_path("mesh_asset_runtime",
                                                           expected_digest,
                                                           packaged_relative).code == CORE_OK);
    TEST_ASSERT(snprintf(packaged_path,
                         sizeof(packaged_path),
                         "%s/%s",
                         export_paths.scene_dir,
                         packaged_relative) < (int)sizeof(packaged_path));
    TEST_ASSERT(file_contains(packaged_path, mesh_bytes));
    TEST_ASSERT(file_contains(export_paths.runtime_path, export_paths.dependency_sha256));
    TEST_ASSERT(core_scene_compile_verify_bundle(export_paths.scene_dir,
                                                 export_paths.bundle_sha256,
                                                 &verification,
                                                 diagnostics,
                                                 sizeof(diagnostics)).code == CORE_OK);
    TEST_ASSERT(verification.dependency_count == 1u);
    TEST_ASSERT(verification.dependency_payload_bytes == strlen(mesh_bytes));
    TEST_ASSERT(strcmp(verification.dependency_sha256, export_paths.dependency_sha256) == 0);
    TEST_ASSERT(write_text_file(packaged_path, "fixture-runtime-mesh-byteX"));
    TEST_ASSERT(core_scene_compile_verify_bundle(export_paths.scene_dir,
                                                 export_paths.bundle_sha256,
                                                 NULL,
                                                 diagnostics,
                                                 sizeof(diagnostics)).code != CORE_OK);
    TEST_ASSERT(write_text_file(packaged_path, mesh_bytes));
    TEST_ASSERT(core_scene_compile_verify_bundle(export_paths.scene_dir,
                                                 export_paths.bundle_sha256,
                                                 NULL,
                                                 diagnostics,
                                                 sizeof(diagnostics)).code == CORE_OK);

    unlink(packaged_path);
    TEST_ASSERT(snprintf(packaged_kind_dir,
                         sizeof(packaged_kind_dir),
                         "%s/dependencies/mesh_asset_runtime",
                         export_paths.scene_dir) < (int)sizeof(packaged_kind_dir));
    TEST_ASSERT(snprintf(packaged_root_dir,
                         sizeof(packaged_root_dir),
                         "%s/dependencies",
                         export_paths.scene_dir) < (int)sizeof(packaged_root_dir));
    rmdir(packaged_kind_dir);
    rmdir(packaged_root_dir);
    unlink(export_paths.receipt_path);
    unlink(export_paths.dependency_manifest_path);
    unlink(export_paths.runtime_path);
    unlink(export_paths.authoring_path);
    rmdir(export_paths.scene_dir);
    unlink(mesh_path);
    rmdir(root);
    Layout_Free(&layout);
    return true;
}

static bool test_scene_export_prefers_scene_owned_mesh_dependency(void) {
    char root_template[] = "/tmp/ld_scene_export_owned_mesh_XXXXXX";
    char* root = mkdtemp(root_template);
    char source_dir[512];
    char assets_dir[512];
    char mesh_assets_dir[512];
    char authoring_hint[512];
    char owned_mesh_path[512];
    char output_root[512];
    char diagnostics[256];
    const char* owned_bytes = "scene-owned-high-quality-mesh";
    char expected_digest[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    Layout layout;
    Transform3D transform;
    Object3D* object = NULL;
    LineDrawingSceneExportPaths export_paths;

    TEST_ASSERT(root != NULL);
    TEST_ASSERT(build_fixture_path(root, "source_scene", source_dir, sizeof(source_dir)));
    TEST_ASSERT(build_fixture_path(source_dir, "assets", assets_dir, sizeof(assets_dir)));
    TEST_ASSERT(build_fixture_path(assets_dir, "mesh_assets", mesh_assets_dir, sizeof(mesh_assets_dir)));
    TEST_ASSERT(build_fixture_path(source_dir, "scene_authoring.json", authoring_hint, sizeof(authoring_hint)));
    TEST_ASSERT(build_fixture_path(mesh_assets_dir, "dragon.runtime.json", owned_mesh_path, sizeof(owned_mesh_path)));
    TEST_ASSERT(build_fixture_path(root, "exports", output_root, sizeof(output_root)));
    TEST_ASSERT(mkdir(source_dir, 0700) == 0);
    TEST_ASSERT(mkdir(assets_dir, 0700) == 0);
    TEST_ASSERT(mkdir(mesh_assets_dir, 0700) == 0);
    TEST_ASSERT(mkdir(output_root, 0700) == 0);
    TEST_ASSERT(write_text_file(owned_mesh_path, owned_bytes));
    TEST_ASSERT(core_scene_compile_sha256(owned_bytes,
                                          strlen(owned_bytes),
                                          expected_digest).code == CORE_OK);

    Layout_Init(&layout, 1.0f);
    transform = Layout_Transform3D_Default();
    const uint32_t object_id = Layout_ObjectStore_Create(&layout.objectStore,
                                                         OBJECT3D_KIND_MESH_ASSET_INSTANCE,
                                                         &transform,
                                                         "mesh_asset_instance",
                                                         CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                                         CORE_OBJECT_PLANE_XY);
    TEST_ASSERT(object_id != 0u);
    object = Layout_ObjectStore_Find(&layout.objectStore, object_id);
    TEST_ASSERT(object != NULL);
    snprintf(object->meshInstance.assetId, sizeof(object->meshInstance.assetId), "%s", "dragon");
    snprintf(object->meshInstance.runtimePath,
             sizeof(object->meshInstance.runtimePath),
             "%s",
             "/missing/library/dragon.runtime.json");
    object->meshInstance.vertexCount = 3u;
    object->meshInstance.triangleCount = 1u;
    object->meshInstance.localBoundsMin = (Vec3){0.0f, 0.0f, 0.0f};
    object->meshInstance.localBoundsMax = (Vec3){1.0f, 1.0f, 1.0f};
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));

    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToOutputRoot(&layout,
                                                                authoring_hint,
                                                                output_root,
                                                                &export_paths,
                                                                diagnostics,
                                                                sizeof(diagnostics)));
    TEST_ASSERT(file_contains(export_paths.dependency_manifest_path, expected_digest));

    char packaged_relative[CORE_SCENE_COMPILE_DEPENDENCY_PATH_SIZE];
    char packaged_path[1024];
    char packaged_kind_dir[1024];
    char packaged_root_dir[1024];
    TEST_ASSERT(core_scene_compile_dependency_payload_path("mesh_asset_runtime",
                                                           expected_digest,
                                                           packaged_relative).code == CORE_OK);
    TEST_ASSERT(snprintf(packaged_path,
                         sizeof(packaged_path),
                         "%s/%s",
                         export_paths.scene_dir,
                         packaged_relative) < (int)sizeof(packaged_path));
    TEST_ASSERT(file_contains(packaged_path, owned_bytes));

    unlink(packaged_path);
    snprintf(packaged_kind_dir, sizeof(packaged_kind_dir), "%s/dependencies/mesh_asset_runtime", export_paths.scene_dir);
    snprintf(packaged_root_dir, sizeof(packaged_root_dir), "%s/dependencies", export_paths.scene_dir);
    rmdir(packaged_kind_dir);
    rmdir(packaged_root_dir);
    unlink(export_paths.receipt_path);
    unlink(export_paths.dependency_manifest_path);
    unlink(export_paths.runtime_path);
    unlink(export_paths.authoring_path);
    rmdir(export_paths.scene_dir);
    unlink(owned_mesh_path);
    rmdir(mesh_assets_dir);
    rmdir(assets_dir);
    rmdir(source_dir);
    rmdir(output_root);
    rmdir(root);
    Layout_Free(&layout);
    return true;
}

static bool test_scene_import_reconciles_mesh_metadata_without_rewriting_source(void) {
    char root_template[] = "/tmp/ld_scene_import_reconcile_XXXXXX";
    char* root = mkdtemp(root_template);
    char source_dir[512];
    char assets_dir[512];
    char mesh_assets_dir[512];
    char authoring_path[512];
    char runtime_path[512];
    char output_root[512];
    char packaged_relative[CORE_SCENE_COMPILE_DEPENDENCY_PATH_SIZE];
    char packaged_path[1024];
    char packaged_kind_dir[1024];
    char packaged_root_dir[1024];
    char diagnostics[256];
    char runtime_digest[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char* source_before = NULL;
    char* source_after = NULL;
    Layout stale_layout;
    Layout imported_layout;
    Transform3D transform;
    Object3D* object = NULL;
    Object3D* imported_object = NULL;
    LineDrawingSceneExportPaths export_paths;
    const char* runtime_json =
        "{"
        "\"schema_variant\":\"mesh_asset_runtime_v1\","
        "\"asset_id\":\"dragon\","
        "\"source_asset_id\":\"stanford_dragon\","
        "\"vertex_count\":433400,"
        "\"triangle_count\":840682,"
        "\"local_bounds\":{"
            "\"min\":{\"x\":-1.0,\"y\":-2.0,\"z\":-3.0},"
            "\"max\":{\"x\":4.0,\"y\":5.0,\"z\":6.0}"
        "}"
        "}";

    TEST_ASSERT(root != NULL);
    TEST_ASSERT(build_fixture_path(root, "source_scene", source_dir, sizeof(source_dir)));
    TEST_ASSERT(build_fixture_path(source_dir, "assets", assets_dir, sizeof(assets_dir)));
    TEST_ASSERT(build_fixture_path(assets_dir, "mesh_assets", mesh_assets_dir, sizeof(mesh_assets_dir)));
    TEST_ASSERT(build_fixture_path(source_dir, "scene_authoring.json", authoring_path, sizeof(authoring_path)));
    TEST_ASSERT(build_fixture_path(mesh_assets_dir, "dragon.runtime.json", runtime_path, sizeof(runtime_path)));
    TEST_ASSERT(build_fixture_path(root, "exports", output_root, sizeof(output_root)));
    TEST_ASSERT(mkdir(source_dir, 0700) == 0);
    TEST_ASSERT(mkdir(assets_dir, 0700) == 0);
    TEST_ASSERT(mkdir(mesh_assets_dir, 0700) == 0);
    TEST_ASSERT(mkdir(output_root, 0700) == 0);
    TEST_ASSERT(write_text_file(runtime_path, runtime_json));
    TEST_ASSERT(core_scene_compile_sha256(runtime_json,
                                          strlen(runtime_json),
                                          runtime_digest).code == CORE_OK);

    Layout_Init(&stale_layout, 1.0f);
    transform = Layout_Transform3D_Default();
    const uint32_t object_id = Layout_ObjectStore_Create(&stale_layout.objectStore,
                                                         OBJECT3D_KIND_MESH_ASSET_INSTANCE,
                                                         &transform,
                                                         "mesh_asset_instance",
                                                         CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                                         CORE_OBJECT_PLANE_XY);
    TEST_ASSERT(object_id != 0u);
    object = Layout_ObjectStore_Find(&stale_layout.objectStore, object_id);
    TEST_ASSERT(object != NULL);
    snprintf(object->meshInstance.assetId, sizeof(object->meshInstance.assetId), "%s", "dragon");
    snprintf(object->meshInstance.runtimePath,
             sizeof(object->meshInstance.runtimePath),
             "%s",
             "/missing/library/dragon.runtime.json");
    object->meshInstance.vertexCount = 100207u;
    object->meshInstance.triangleCount = 202520u;
    object->meshInstance.localBoundsMin = (Vec3){-0.5f, -0.5f, -0.5f};
    object->meshInstance.localBoundsMax = (Vec3){0.5f, 0.5f, 0.5f};
    object->meshInstance.lockToBounds = true;
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));
    TEST_ASSERT(LineDrawingCanonicalScene_ExportLayoutToFile(&stale_layout,
                                                              "scene_reconcile_fixture",
                                                              authoring_path));
    source_before = read_text_file(authoring_path);
    TEST_ASSERT(source_before != NULL);

    Layout_Init(&imported_layout, 1.0f);
    TEST_ASSERT(LineDrawingSceneImport_LoadLayoutFromAuthoringFile(&imported_layout,
                                                                   authoring_path,
                                                                   diagnostics,
                                                                   sizeof(diagnostics)));
    imported_object = Layout_ObjectStore_Find(&imported_layout.objectStore, object_id);
    TEST_ASSERT(imported_object != NULL);
    TEST_ASSERT(strcmp(imported_object->meshInstance.runtimePath, runtime_path) == 0);
    TEST_ASSERT(imported_object->meshInstance.vertexCount == 433400u);
    TEST_ASSERT(imported_object->meshInstance.triangleCount == 840682u);
    TEST_ASSERT(imported_object->meshInstance.localBoundsMin.y == -2.0f);
    TEST_ASSERT(imported_object->meshInstance.localBoundsMax.z == 6.0f);
    source_after = read_text_file(authoring_path);
    TEST_ASSERT(source_after != NULL);
    TEST_ASSERT(strcmp(source_before, source_after) == 0);

    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToOutputRoot(&imported_layout,
                                                                authoring_path,
                                                                output_root,
                                                                &export_paths,
                                                                diagnostics,
                                                                sizeof(diagnostics)));
    TEST_ASSERT(file_contains(export_paths.authoring_path, "433400"));
    TEST_ASSERT(file_contains(export_paths.authoring_path, "840682"));
    TEST_ASSERT(file_contains(export_paths.dependency_manifest_path, runtime_digest));
    TEST_ASSERT(core_scene_compile_dependency_payload_path("mesh_asset_runtime",
                                                           runtime_digest,
                                                           packaged_relative).code == CORE_OK);
    TEST_ASSERT(snprintf(packaged_path,
                         sizeof(packaged_path),
                         "%s/%s",
                         export_paths.scene_dir,
                         packaged_relative) < (int)sizeof(packaged_path));
    TEST_ASSERT(file_contains(packaged_path, "433400"));

    free(source_before);
    free(source_after);
    unlink(packaged_path);
    snprintf(packaged_kind_dir, sizeof(packaged_kind_dir), "%s/dependencies/mesh_asset_runtime", export_paths.scene_dir);
    snprintf(packaged_root_dir, sizeof(packaged_root_dir), "%s/dependencies", export_paths.scene_dir);
    rmdir(packaged_kind_dir);
    rmdir(packaged_root_dir);
    unlink(export_paths.receipt_path);
    unlink(export_paths.dependency_manifest_path);
    unlink(export_paths.runtime_path);
    unlink(export_paths.authoring_path);
    rmdir(export_paths.scene_dir);
    unlink(authoring_path);
    unlink(runtime_path);
    rmdir(mesh_assets_dir);
    rmdir(assets_dir);
    rmdir(source_dir);
    rmdir(output_root);
    rmdir(root);
    Layout_Free(&stale_layout);
    Layout_Free(&imported_layout);
    return true;
}

static bool test_scene_export_cleans_new_directory_on_authoring_failure(void) {
    char root_template[] = "/tmp/ld_scene_export_cleanup_XXXXXX";
    char* root = NULL;
    char expected_scene_dir[512];
    Layout layout;
    LineDrawingSceneExportPaths export_paths;
    char diagnostics[256];
    bool exported = false;

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);

    Layout_Init(&layout, 1.0f);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){0.0f, 0.0f, 0.0f}) >= 0);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){1.0f, 0.0f, 0.0f}) >= 0);
    Layout_AddWall3(&layout, (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){1.0f, 0.0f, 0.0f});
    TEST_ASSERT(snprintf(expected_scene_dir,
                         sizeof(expected_scene_dir),
                         "%s/bad_scene",
                         root) < (int)sizeof(expected_scene_dir));

    TEST_ASSERT(setenv("LINE_DRAWING_TEST_FORCE_SCENE_EXPORT_COMPILE_FAIL", "1", 1) == 0);
    exported = LineDrawingSceneExport_ExportLayoutToOutputRoot(&layout,
                                                               "bad_scene.json",
                                                               root,
                                                               &export_paths,
                                                               diagnostics,
                                                               sizeof(diagnostics));
    unsetenv("LINE_DRAWING_TEST_FORCE_SCENE_EXPORT_COMPILE_FAIL");
    TEST_ASSERT(!exported);
    TEST_ASSERT(access(expected_scene_dir, F_OK) != 0);

    rmdir(root);
    Layout_Free(&layout);
    return true;
}

static bool test_scene_export_embeds_layout_snapshot_and_round_trips_import(void) {
    Layout layout;
    Layout imported;
    LineDrawingSceneExportPaths export_paths;
    char layout_path_hint[256];
    char diagnostics[256];
    char* authoring_json = NULL;

    Layout_Init(&layout, 1.0f);
    Layout_Init(&imported, 1.0f);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){0.0f, 0.0f, 0.0f}) >= 0);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){2.0f, 0.0f, 1.0f}) >= 0);
    Layout_AddWall3(&layout, (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){2.0f, 0.0f, 1.0f});
    TEST_ASSERT(layout.wallCount == 1u);
    layout.scene3d.bounds.enabled = true;
    layout.scene3d.bounds.clampOnEdit = true;
    layout.scene3d.bounds.min = (Vec3){-3.0f, -2.0f, -1.0f};
    layout.scene3d.bounds.max = (Vec3){ 3.0f,  2.0f,  4.0f};
    snprintf(layout_path_hint, sizeof(layout_path_hint), "tests/fixtures/roundtrip room %d.json", (int)getpid());

    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToOutputRoot(&layout,
                                                                layout_path_hint,
                                                                "/tmp",
                                                                &export_paths,
                                                                diagnostics,
                                                                sizeof(diagnostics)));

    {
        FILE* fp = fopen(export_paths.authoring_path, "rb");
        long len = 0;
        TEST_ASSERT(fp != NULL);
        TEST_ASSERT(fseek(fp, 0, SEEK_END) == 0);
        len = ftell(fp);
        TEST_ASSERT(len > 0);
        TEST_ASSERT(fseek(fp, 0, SEEK_SET) == 0);
        authoring_json = (char*)malloc((size_t)len + 1u);
        TEST_ASSERT(authoring_json != NULL);
        TEST_ASSERT(fread(authoring_json, 1u, (size_t)len, fp) == (size_t)len);
        authoring_json[len] = '\0';
        TEST_ASSERT(fclose(fp) == 0);
    }
    TEST_ASSERT(strstr(authoring_json, "\"layout_snapshot\"") != NULL);
    free(authoring_json);

    TEST_ASSERT(LineDrawingSceneImport_LoadLayoutFromAuthoringFile(&imported,
                                                                  export_paths.authoring_path,
                                                                  diagnostics,
                                                                  sizeof(diagnostics)));
    TEST_ASSERT(imported.anchorCount == layout.anchorCount);
    TEST_ASSERT(imported.wallCount == layout.wallCount);
    TEST_ASSERT(imported.scene3d.bounds.enabled == layout.scene3d.bounds.enabled);
    TEST_ASSERT(imported.scene3d.bounds.clampOnEdit == layout.scene3d.bounds.clampOnEdit);
    TEST_ASSERT(imported.anchors[0].pos.x == layout.anchors[0].pos.x);
    TEST_ASSERT(imported.anchors[1].pos.z == layout.anchors[1].pos.z);
    TEST_ASSERT(imported.scene3d.bounds.max.z == layout.scene3d.bounds.max.z);

    Layout_Free(&layout);
    Layout_Free(&imported);
    return true;
}

static bool test_scene_project_export_writes_project_metadata_scaffold(void) {
    char root_template[] = "/tmp/ld_scene_project_export_XXXXXX";
    char* root = NULL;
    char authoring_path[512];
    char runtime_path[512];
    char scene_project_path[512];
    char object_manifest_path[512];
    char scaffold_path[512];
    char diagnostics[256];
    char* scene_project_text = NULL;
    char* object_manifest_text = NULL;
    cJSON* scene_project = NULL;
    cJSON* object_manifest = NULL;
    const cJSON* objects = NULL;
    LineDrawingSceneProjectExportOptions options = {
        .project_name = "fixture_project",
        .created_by = "line_drawing_test",
        .timestamp_utc = "2026-06-24T00:00:00Z",
        .authoring_scene = "scene_authoring.json",
        .runtime_scene = "scene_runtime.json",
    };

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);
    TEST_ASSERT(build_fixture_path(root, "scene_authoring.json", authoring_path, sizeof(authoring_path)));
    TEST_ASSERT(build_fixture_path(root, "scene_runtime.json", runtime_path, sizeof(runtime_path)));
    TEST_ASSERT(write_text_file(authoring_path,
                                "{\"schema_variant\":\"scene_authoring_v1\","
                                "\"scene_id\":\"scene_line_drawing_fixture_project\"}\n"));
    TEST_ASSERT(write_text_file(runtime_path,
                                "{\"schema_variant\":\"scene_runtime_v1\","
                                "\"scene_id\":\"scene_line_drawing_fixture_project\"}\n"));

    TEST_ASSERT(LineDrawingSceneProjectExport_WriteProjectFiles(root,
                                                               &options,
                                                               diagnostics,
                                                               sizeof(diagnostics)));

    TEST_ASSERT(build_fixture_path(root, "scene_project.json", scene_project_path, sizeof(scene_project_path)));
    TEST_ASSERT(build_fixture_path(root, "object_manifest.json", object_manifest_path, sizeof(object_manifest_path)));
    scene_project_text = read_text_file(scene_project_path);
    object_manifest_text = read_text_file(object_manifest_path);
    TEST_ASSERT(scene_project_text != NULL);
    TEST_ASSERT(object_manifest_text != NULL);
    scene_project = cJSON_Parse(scene_project_text);
    object_manifest = cJSON_Parse(object_manifest_text);
    TEST_ASSERT(cJSON_IsObject(scene_project));
    TEST_ASSERT(cJSON_IsObject(object_manifest));

    TEST_ASSERT(json_string_equals(scene_project, "schema", "codework_scene_project_v1"));
    TEST_ASSERT(json_string_equals(scene_project, "project_name", "fixture_project"));
    TEST_ASSERT(json_string_equals(scene_project, "created_by", "line_drawing_test"));
    TEST_ASSERT(json_string_equals(scene_project, "created_at", "2026-06-24T00:00:00Z"));
    TEST_ASSERT(json_string_equals(scene_project, "updated_at", "2026-06-24T00:00:00Z"));
    TEST_ASSERT(json_string_equals(scene_project, "authoring_scene", "scene_authoring.json"));
    TEST_ASSERT(json_string_equals(scene_project, "runtime_scene", "scene_runtime.json"));
    TEST_ASSERT(json_string_equals(scene_project, "object_manifest", "object_manifest.json"));
    TEST_ASSERT(json_string_equals(scene_project, "mesh_assets_dir", "assets/mesh_assets"));
    TEST_ASSERT(json_string_equals(scene_project, "active_cache", "physics_sim/active_cache_manifest.json"));
    TEST_ASSERT(json_string_equals(scene_project, "active_render_request", "ray_tracing/render_request.json"));

    TEST_ASSERT(json_string_equals(object_manifest, "schema", "line_drawing_object_manifest_v1"));
    objects = cJSON_GetObjectItem(object_manifest, "objects");
    TEST_ASSERT(cJSON_IsArray(objects));
    TEST_ASSERT(cJSON_GetArraySize(objects) == 0);

    TEST_ASSERT(build_fixture_path(root, "assets/mesh_assets", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "assets/vf3d/active", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "assets/vf3d/runs", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "assets/physics/active", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "assets/physics/runs", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "line_drawing/notes.md", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "physics_sim/runs", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "ray_tracing/presets", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "ray_tracing/frames_temp", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "ray_tracing/videos", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "ray_tracing/runs", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "ray_tracing/review", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "worker_export", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "assets/vf3d/active/frame_000000.vf3d",
                                   scaffold_path,
                                   sizeof(scaffold_path)));
    TEST_ASSERT(!path_exists(scaffold_path));
    TEST_ASSERT(build_fixture_path(root, "ray_tracing/render_request.json", scaffold_path, sizeof(scaffold_path)));
    TEST_ASSERT(!path_exists(scaffold_path));

    cJSON_Delete(scene_project);
    cJSON_Delete(object_manifest);
    free(scene_project_text);
    free(object_manifest_text);
    return true;
}

static bool test_scene_export_project_root_writes_scene_and_project_files(void) {
    char root_template[] = "/tmp/ld_scene_project_integrated_XXXXXX";
    char* root = NULL;
    char path[512];
    char diagnostics[256];
    char* scene_project_text = NULL;
    cJSON* scene_project = NULL;
    Layout layout;
    LineDrawingSceneExportPaths export_paths;

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);

    Layout_Init(&layout, 1.0f);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){0.0f, 0.0f, 0.0f}) >= 0);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){2.0f, 1.0f, 0.0f}) >= 0);
    Layout_AddWall3(&layout, (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){2.0f, 1.0f, 0.0f});

    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToProjectRoot(&layout,
                                                                 root,
                                                                 &export_paths,
                                                                 diagnostics,
                                                                 sizeof(diagnostics)));
    TEST_ASSERT(strstr(export_paths.scene_dir, root) != NULL);
    TEST_ASSERT(strstr(export_paths.authoring_path, "/scene_authoring.json") != NULL);
    TEST_ASSERT(strstr(export_paths.runtime_path, "/scene_runtime.json") != NULL);
    TEST_ASSERT(file_contains(export_paths.authoring_path, "\"scene_authoring_v1\""));
    TEST_ASSERT(file_contains(export_paths.runtime_path, "\"schema_variant\":\"scene_runtime_v1\""));

    TEST_ASSERT(build_fixture_path(root, "scene_project.json", path, sizeof(path)));
    TEST_ASSERT(path_exists(path));
    scene_project_text = read_text_file(path);
    TEST_ASSERT(scene_project_text != NULL);
    scene_project = cJSON_Parse(scene_project_text);
    TEST_ASSERT(cJSON_IsObject(scene_project));
    TEST_ASSERT(json_string_equals(scene_project, "schema", "codework_scene_project_v1"));
    TEST_ASSERT(json_string_equals(scene_project, "authoring_scene", "scene_authoring.json"));
    TEST_ASSERT(json_string_equals(scene_project, "runtime_scene", "scene_runtime.json"));
    TEST_ASSERT(json_string_equals(scene_project, "object_manifest", "object_manifest.json"));
    TEST_ASSERT(json_string_equals(scene_project, "mesh_assets_dir", "assets/mesh_assets"));

    TEST_ASSERT(build_fixture_path(root, "object_manifest.json", path, sizeof(path)));
    TEST_ASSERT(path_exists(path));
    TEST_ASSERT(build_fixture_path(root, "assets/mesh_assets", path, sizeof(path)));
    TEST_ASSERT(path_exists(path));
    TEST_ASSERT(build_fixture_path(root, "assets/vf3d/active", path, sizeof(path)));
    TEST_ASSERT(path_exists(path));
    TEST_ASSERT(build_fixture_path(root, "assets/physics/active", path, sizeof(path)));
    TEST_ASSERT(path_exists(path));
    TEST_ASSERT(build_fixture_path(root, "physics_sim/runs", path, sizeof(path)));
    TEST_ASSERT(path_exists(path));
    TEST_ASSERT(build_fixture_path(root, "ray_tracing/presets", path, sizeof(path)));
    TEST_ASSERT(path_exists(path));
    TEST_ASSERT(build_fixture_path(root, "ray_tracing/frames_temp", path, sizeof(path)));
    TEST_ASSERT(path_exists(path));
    TEST_ASSERT(build_fixture_path(root, "worker_export", path, sizeof(path)));
    TEST_ASSERT(path_exists(path));

    cJSON_Delete(scene_project);
    free(scene_project_text);
    Layout_Free(&layout);
    return true;
}

static bool test_scene_export_project_root_populates_mesh_manifest_sidecar(void) {
    char source_template[] = "/tmp/ld_scene_project_mesh_source_XXXXXX";
    char project_template[] = "/tmp/ld_scene_project_mesh_export_XXXXXX";
    char* source_root = NULL;
    char* project_root = NULL;
    char runtime_path[512];
    char manifest_path[512];
    char copied_runtime_path[512];
    char diagnostics[256];
    char* manifest_text = NULL;
    cJSON* manifest = NULL;
    const cJSON* objects = NULL;
    const cJSON* object = NULL;
    CoreMeshAssetRuntimeDocument copied_runtime;
    Transform3D transform;
    uint32_t object_id = 0u;
    Layout layout;
    LineDrawingSceneExportPaths export_paths;
    const char* runtime_json =
        "{"
        "\"schema_family\":\"codework_geometry\","
        "\"schema_variant\":\"mesh_asset_runtime_v1\","
        "\"schema_version\":1,"
        "\"asset_id\":\"asset_project_mesh\","
        "\"source_asset_id\":\"source_project_mesh\","
        "\"asset_type\":\"solid_mesh\","
        "\"local_bounds\":{"
            "\"min\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"max\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
        "},"
        "\"topology_flags\":{\"closed_volume\":true,\"manifold_expected\":true},"
        "\"mesh\":{"
            "\"vertex_count\":4,\"triangle_count\":4,"
            "\"vertices\":["
                "{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
                "{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
                "{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
                "{\"x\":0.0,\"y\":0.0,\"z\":1.0}"
            "],"
            "\"triangles\":["
                "{\"a\":0,\"b\":1,\"c\":2,\"surface_group_id\":\"surface\"},"
                "{\"a\":0,\"b\":3,\"c\":1,\"surface_group_id\":\"surface\"},"
                "{\"a\":0,\"b\":2,\"c\":3,\"surface_group_id\":\"surface\"},"
                "{\"a\":1,\"b\":3,\"c\":2,\"surface_group_id\":\"surface\"}"
            "]"
        "},"
        "\"surface_groups\":[{"
            "\"group_id\":\"surface\","
            "\"triangle_span\":{\"start\":0,\"count\":4}"
        "}],"
        "\"extensions\":{}"
        "}";

    source_root = mkdtemp(source_template);
    TEST_ASSERT(source_root != NULL);
    project_root = mkdtemp(project_template);
    TEST_ASSERT(project_root != NULL);
    TEST_ASSERT(build_fixture_path(source_root, "source_mesh.runtime.json", runtime_path, sizeof(runtime_path)));
    TEST_ASSERT(write_text_file(runtime_path, runtime_json));

    Layout_Init(&layout, 1.0f);
    transform = Layout_Transform3D_Default();
    transform.position = (Vec3){ 2.0f, 3.0f, 4.0f };
    TEST_ASSERT(Layout_CreateMeshAssetInstanceFromRuntimeAsset(&layout,
                                                              runtime_path,
                                                              &transform,
                                                              &object_id,
                                                              diagnostics,
                                                              sizeof(diagnostics)));
    TEST_ASSERT(object_id == 1u);

    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToProjectRoot(&layout,
                                                                 project_root,
                                                                 &export_paths,
                                                                 diagnostics,
                                                                 sizeof(diagnostics)));
    TEST_ASSERT(build_fixture_path(project_root, "object_manifest.json", manifest_path, sizeof(manifest_path)));
    manifest_text = read_text_file(manifest_path);
    TEST_ASSERT(manifest_text != NULL);
    manifest = cJSON_Parse(manifest_text);
    TEST_ASSERT(cJSON_IsObject(manifest));
    objects = cJSON_GetObjectItem(manifest, "objects");
    TEST_ASSERT(cJSON_IsArray(objects));
    TEST_ASSERT(cJSON_GetArraySize(objects) == 1);
    object = cJSON_GetArrayItem(objects, 0);
    TEST_ASSERT(cJSON_IsObject(object));
    TEST_ASSERT(json_string_equals(object, "id", "obj3d_1"));
    TEST_ASSERT(json_string_equals(object, "name", "asset_project_mesh"));
    TEST_ASSERT(json_string_equals(object, "kind", "mesh_asset_instance"));
    TEST_ASSERT(json_string_equals(object, "mesh_asset_id", "asset_project_mesh"));
    TEST_ASSERT(json_string_equals(object, "source_asset_id", "source_project_mesh"));
    TEST_ASSERT(json_string_equals(object, "mesh_sidecar_path", "assets/mesh_assets/asset_project_mesh.runtime.json"));
    TEST_ASSERT(cJSON_GetObjectItem(object, "vertex_count")->valueint == 4);
    TEST_ASSERT(cJSON_GetObjectItem(object, "triangle_count")->valueint == 4);
    TEST_ASSERT(cJSON_IsFalse(cJSON_GetObjectItem(object, "physics_extension_present")));
    TEST_ASSERT(cJSON_IsFalse(cJSON_GetObjectItem(object, "ray_tracing_extension_present")));

    TEST_ASSERT(build_fixture_path(project_root,
                                   "assets/mesh_assets/asset_project_mesh.runtime.json",
                                   copied_runtime_path,
                                   sizeof(copied_runtime_path)));
    TEST_ASSERT(path_exists(copied_runtime_path));
    TEST_ASSERT(file_contains(copied_runtime_path, "\"asset_id\":\"asset_project_mesh\""));
    core_mesh_asset_runtime_document_init(&copied_runtime);
    TEST_ASSERT(core_mesh_asset_runtime_document_load_file(copied_runtime_path,
                                                           &copied_runtime).code == CORE_OK);
    TEST_ASSERT(core_mesh_asset_runtime_document_validate(&copied_runtime).code == CORE_OK);
    TEST_ASSERT(copied_runtime.vertex_count == 4u);
    TEST_ASSERT(copied_runtime.triangle_count == 4u);
    core_mesh_asset_runtime_document_free(&copied_runtime);

    cJSON_Delete(manifest);
    free(manifest_text);
    Layout_Free(&layout);
    return true;
}

static bool test_scene_project_export_rejects_metadata_only_mesh_sidecar(void) {
    char source_template[] = "/tmp/ld_scene_project_invalid_mesh_source_XXXXXX";
    char project_template[] = "/tmp/ld_scene_project_invalid_mesh_export_XXXXXX";
    char* source_root = mkdtemp(source_template);
    char* project_root = mkdtemp(project_template);
    char runtime_path[512];
    char diagnostics[256];
    LineDrawingSceneProjectManifestObject object = {0};
    LineDrawingSceneProjectExportOptions options = {0};
    const char* metadata_only_json =
        "{"
        "\"schema_variant\":\"mesh_asset_runtime_v1\","
        "\"asset_id\":\"asset_metadata_only\","
        "\"source_asset_id\":\"source_metadata_only\","
        "\"vertex_count\":8,"
        "\"triangle_count\":12"
        "}";

    TEST_ASSERT(source_root != NULL);
    TEST_ASSERT(project_root != NULL);
    TEST_ASSERT(build_fixture_path(source_root,
                                   "metadata_only.runtime.json",
                                   runtime_path,
                                   sizeof(runtime_path)));
    TEST_ASSERT(write_text_file(runtime_path, metadata_only_json));

    object.object_id = "obj_metadata_only";
    object.mesh_asset_id = "asset_metadata_only";
    object.source_asset_id = "source_metadata_only";
    object.source_mesh_sidecar_path = runtime_path;
    object.vertex_count = 8u;
    object.triangle_count = 12u;
    options.objects = &object;
    options.object_count = 1u;

    TEST_ASSERT(!LineDrawingSceneProjectExport_WriteProjectFiles(project_root,
                                                                 &options,
                                                                 diagnostics,
                                                                 sizeof(diagnostics)));
    TEST_ASSERT(strcmp(diagnostics, "failed to copy project mesh sidecar") == 0);
    return true;
}

static bool test_scene_project_scaffold_keeps_authoring_as_editable_import(void) {
    char root_template[] = "/tmp/ld_scene_project_import_XXXXXX";
    char* root = NULL;
    char diagnostics[256];
    Layout layout;
    Layout imported;
    LineDrawingSceneExportPaths export_paths;

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);

    Layout_Init(&layout, 1.0f);
    Layout_Init(&imported, 1.0f);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){0.0f, 0.0f, 0.0f}) >= 0);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){4.0f, 0.0f, 1.0f}) >= 0);
    Layout_AddWall3(&layout, (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){4.0f, 0.0f, 1.0f});

    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToProjectRoot(&layout,
                                                                 root,
                                                                 &export_paths,
                                                                 diagnostics,
                                                                 sizeof(diagnostics)));
    TEST_ASSERT(LineDrawingSceneImport_LoadLayoutFromAuthoringFile(&imported,
                                                                  export_paths.authoring_path,
                                                                  diagnostics,
                                                                  sizeof(diagnostics)));
    TEST_ASSERT(imported.anchorCount == layout.anchorCount);
    TEST_ASSERT(imported.wallCount == layout.wallCount);
    TEST_ASSERT(!LineDrawingSceneImport_LoadLayoutFromAuthoringFile(&imported,
                                                                   export_paths.runtime_path,
                                                                   diagnostics,
                                                                   sizeof(diagnostics)));
    TEST_ASSERT(strstr(diagnostics, "scene_runtime.json is compiled output") != NULL);

    Layout_Free(&layout);
    Layout_Free(&imported);
    return true;
}

static bool test_scene_import_accepts_supported_authoring_unit_metadata(void) {
    Layout layout;
    Layout imported;
    LineDrawingSceneAuthoringOptions options = {
        .world_scale = 1.25,
        .unit_system = "meters",
        .conversion_policy = "explicit_only",
    };
    const char* path = "/tmp/line_drawing_scene_import_supported_units.json";
    char diagnostics[256];

    Layout_Init(&layout, 1.0f);
    Layout_Init(&imported, 1.0f);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){0.0f, 0.0f, 0.0f}) >= 0);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){1.5f, 2.0f, 0.5f}) >= 0);
    Layout_AddWall3(&layout, (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){1.5f, 2.0f, 0.5f});

    TEST_ASSERT(LineDrawingCanonicalScene_ExportLayoutToFileWithOptions(&layout,
                                                                        "scene_import_supported_units",
                                                                        path,
                                                                        &options));
    TEST_ASSERT(LineDrawingSceneImport_LoadLayoutFromAuthoringFile(&imported,
                                                                  path,
                                                                  diagnostics,
                                                                  sizeof(diagnostics)));
    TEST_ASSERT(imported.anchorCount == layout.anchorCount);
    TEST_ASSERT(imported.wallCount == layout.wallCount);
    TEST_ASSERT(imported.anchors[1].pos.y == layout.anchors[1].pos.y);
    {
        char* exported = LineDrawingCanonicalScene_ExportLayoutToString(&imported, "scaled_roundtrip");
        cJSON* root = exported ? cJSON_Parse(exported) : NULL;
        const cJSON* scale = root ? cJSON_GetObjectItemCaseSensitive(root, "world_scale") : NULL;
        TEST_ASSERT(cJSON_IsNumber(scale));
        TEST_ASSERT(fabs(scale->valuedouble - 1.25) < 1e-12);
        cJSON_Delete(root);
        free(exported);
    }

    remove(path);
    Layout_Free(&layout);
    Layout_Free(&imported);
    return true;
}

static bool test_scene_import_rejects_unsupported_authoring_unit_metadata(void) {
    Layout imported;
    char diagnostics[256];
    const char* path = "/tmp/line_drawing_scene_import_bad_units.json";
    const char* json =
        "{"
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"scene_id\":\"scene_bad_units\","
        "\"unit_system\":\"feet\","
        "\"conversion_policy\":\"explicit_only\","
        "\"world_scale\":1.0,"
        "\"extensions\":{\"line_drawing\":{\"layout_snapshot\":{"
        "\"version\":8,"
        "\"gridSize\":1,"
        "\"anchors\":[],"
        "\"walls\":[],"
        "\"objects3d\":[],"
        "\"scene3d\":{"
        "\"bounds\":{\"enabled\":false,\"clampOnEdit\":false,\"min\":{\"x\":-1,\"y\":-1,\"z\":-1},\"max\":{\"x\":1,\"y\":1,\"z\":1}},"
        "\"constructionPlane\":{\"mode\":\"axis_aligned\",\"axis\":\"xy\",\"offset\":0}"
        "}"
        "}}}"
        "}";

    Layout_Init(&imported, 1.0f);
    TEST_ASSERT(write_text_file(path, json));
    TEST_ASSERT(!LineDrawingSceneImport_LoadLayoutFromAuthoringFile(&imported,
                                                                   path,
                                                                   diagnostics,
                                                                   sizeof(diagnostics)));
    TEST_ASSERT(strstr(diagnostics, "unit_system=\"meters\"") != NULL);

    remove(path);
    Layout_Free(&imported);
    return true;
}

static bool test_scene_import_rejects_runtime_scene_file(void) {
    Layout imported;
    char diagnostics[256];
    const char* path = "/tmp/line_drawing_scene_import_runtime_scene.json";
    const char* json =
        "{"
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"scene_id\":\"scene_runtime_only\""
        "}";

    Layout_Init(&imported, 1.0f);
    TEST_ASSERT(write_text_file(path, json));
    TEST_ASSERT(!LineDrawingSceneImport_LoadLayoutFromAuthoringFile(&imported,
                                                                   path,
                                                                   diagnostics,
                                                                   sizeof(diagnostics)));
    TEST_ASSERT(strstr(diagnostics, "scene_runtime.json is compiled output") != NULL);

    remove(path);
    Layout_Free(&imported);
    return true;
}

static bool test_scene_import_normalizes_top_level_camera_light_and_path_records(void) {
    Layout imported;
    Layout reloaded;
    char diagnostics[256];
    char* layout_json = NULL;
    const char* path = "/tmp/line_drawing_scene_import_authoring_records.json";
    const char* json =
        "{"
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"unit_system\":\"meters\","
        "\"paths\":[{\"path_id\":\"path_loaded\",\"path_kind\":\"camera\","
        "\"curve_type\":\"linear\",\"camera_id\":\"camera_loaded\","
        "\"closed\":false,\"playback_mode\":\"loop\",\"duration_seconds\":8.0,"
        "\"normalized_distance\":0.25,\"playing\":true,"
        "\"control_points\":[{\"x\":0,\"y\":0,\"z\":2},{\"x\":10,\"y\":0,\"z\":2}]}],"
        "\"cameras\":[{\"camera_id\":\"camera_loaded\",\"label\":\"Loaded Camera\","
        "\"path_id\":\"path_loaded\",\"transform\":{\"position\":{\"x\":0,\"y\":0,\"z\":2}},"
        "\"orientation\":{\"mode\":\"look_at_target\",\"look_at_target\":{\"x\":5,\"y\":3,\"z\":2},"
        "\"fixed_forward\":{\"x\":0,\"y\":1,\"z\":0},\"roll_degrees\":5},"
        "\"vertical_fov_degrees\":60,\"near_clip\":0.2,\"far_clip\":500}],"
        "\"lights\":[{\"light_id\":\"light_loaded\",\"label\":\"Loaded Light\","
        "\"light_type\":\"spot\",\"path_id\":\"path_loaded\",\"enabled\":true,"
        "\"position_mode\":\"path_start\",\"transform\":{\"position\":{\"x\":1,\"y\":2,\"z\":3}},"
        "\"direction\":{\"x\":0,\"y\":0,\"z\":-1},\"aim_target\":{\"x\":1,\"y\":2,\"z\":0},"
        "\"color\":{\"x\":1,\"y\":0.5,\"z\":0.25},\"intensity\":4,\"radius\":0.5}]"
        "}";

    Layout_Init(&imported, 1.0f);
    Layout_Init(&reloaded, 1.0f);
    TEST_ASSERT(write_text_file(path, json));
    TEST_ASSERT(LineDrawingSceneImport_LoadLayoutFromAuthoringFile(&imported,
                                                                   path,
                                                                   diagnostics,
                                                                   sizeof(diagnostics)));
    TEST_ASSERT(imported.sceneAuthoring.path_count == 1u);
    TEST_ASSERT(imported.sceneAuthoring.camera_count == 1u);
    TEST_ASSERT(imported.sceneAuthoring.light_count == 1u);
    TEST_ASSERT(strcmp(imported.sceneAuthoring.paths[0].path_id, "path_loaded") == 0);
    TEST_ASSERT(strcmp(imported.sceneAuthoring.cameras[0].camera_id, "camera_loaded") == 0);
    TEST_ASSERT(strcmp(imported.sceneAuthoring.lights[0].light_id, "light_loaded") == 0);
    TEST_ASSERT(imported.sceneAuthoring.paths[0].playback_mode ==
                LINE_DRAWING_SCENE_PATH_PLAYBACK_LOOP);
    TEST_ASSERT(fabsf(imported.sceneAuthoring.paths[0].duration_seconds - 8.0f) < 0.001f);
    TEST_ASSERT(Layout_SceneAuthoringState_SetPathControlPoint(&imported.sceneAuthoring,
                                                               0u,
                                                               1u,
                                                               (Vec3){12.0f, 1.0f, 2.0f}));
    layout_json = Layout_SaveToString(&imported);
    TEST_ASSERT(layout_json != NULL);
    TEST_ASSERT(Layout_LoadFromString(&reloaded, layout_json));
    TEST_ASSERT(strcmp(reloaded.sceneAuthoring.cameras[0].camera_id, "camera_loaded") == 0);
    TEST_ASSERT(fabsf(reloaded.sceneAuthoring.paths[0].control_points[1].x - 12.0f) < 0.001f);
    free(layout_json);
    remove(path);
    Layout_Free(&imported);
    Layout_Free(&reloaded);
    return true;
}

bool scene_export_run_tests(void) {
    const TestCase cases[] = {
        { "scene_export_emits_authoring_and_runtime_files", test_scene_export_emits_authoring_and_runtime_files },
        { "scene_export_uses_parent_scene_name_for_authoring_hint",
          test_scene_export_uses_parent_scene_name_for_authoring_hint },
        { "scene_export_refuses_to_replace_existing_bundle",
          test_scene_export_refuses_to_replace_existing_bundle },
        { "scene_export_collects_and_verifies_mesh_dependencies",
          test_scene_export_collects_and_verifies_mesh_dependencies },
        { "scene_export_prefers_scene_owned_mesh_dependency",
          test_scene_export_prefers_scene_owned_mesh_dependency },
        { "scene_import_reconciles_mesh_metadata_without_rewriting_source",
          test_scene_import_reconciles_mesh_metadata_without_rewriting_source },
        { "scene_export_cleans_new_directory_on_authoring_failure",
          test_scene_export_cleans_new_directory_on_authoring_failure },
        { "scene_export_embeds_layout_snapshot_and_round_trips_import",
          test_scene_export_embeds_layout_snapshot_and_round_trips_import },
        { "scene_project_export_writes_project_metadata_scaffold",
          test_scene_project_export_writes_project_metadata_scaffold },
        { "scene_export_project_root_writes_scene_and_project_files",
          test_scene_export_project_root_writes_scene_and_project_files },
        { "scene_export_project_root_populates_mesh_manifest_sidecar",
          test_scene_export_project_root_populates_mesh_manifest_sidecar },
        { "scene_project_export_rejects_metadata_only_mesh_sidecar",
          test_scene_project_export_rejects_metadata_only_mesh_sidecar },
        { "scene_project_scaffold_keeps_authoring_as_editable_import",
          test_scene_project_scaffold_keeps_authoring_as_editable_import },
        { "scene_import_accepts_supported_authoring_unit_metadata",
          test_scene_import_accepts_supported_authoring_unit_metadata },
        { "scene_import_rejects_unsupported_authoring_unit_metadata",
          test_scene_import_rejects_unsupported_authoring_unit_metadata },
        { "scene_import_rejects_runtime_scene_file",
          test_scene_import_rejects_runtime_scene_file },
        { "scene_import_normalizes_top_level_camera_light_and_path_records",
          test_scene_import_normalizes_top_level_camera_light_and_path_records },
    };
    return run_test_cases("SceneExport", cases, sizeof(cases) / sizeof(cases[0]));
}
