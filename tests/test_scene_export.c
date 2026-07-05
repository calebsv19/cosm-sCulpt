#include "test_framework.h"

#include "Layout/layout.h"
#include "Tools/canonical_scene_export.h"
#include "Tools/scene_import.h"
#include "Tools/scene_export.h"
#include "Tools/scene_project_export.h"
#include "cjson/cJSON.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
    TEST_ASSERT(strstr(export_paths.scene_id, "scene_line_drawing_demo_room") != NULL);
    TEST_ASSERT(file_contains(export_paths.authoring_path, "\"scene_authoring_v1\""));
    TEST_ASSERT(file_contains(export_paths.runtime_path, "\"schema_variant\":\"scene_runtime_v1\""));

    Layout_Free(&layout);
    (void)unlink(export_paths.authoring_path);
    (void)unlink(export_paths.runtime_path);
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

    remove(export_paths.authoring_path);
    remove(export_paths.runtime_path);
    rmdir(export_paths.scene_dir);
    rmdir(root);
    Layout_Free(&layout);
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
    Transform3D transform;
    uint32_t object_id = 0u;
    Layout layout;
    LineDrawingSceneExportPaths export_paths;
    const char* runtime_json =
        "{"
        "\"schema_variant\":\"mesh_asset_runtime_v1\","
        "\"asset_id\":\"asset_project_mesh\","
        "\"source_asset_id\":\"source_project_mesh\","
        "\"vertex_count\":8,"
        "\"triangle_count\":12,"
        "\"local_bounds\":{"
            "\"min\":{\"x\":-0.5,\"y\":-1.0,\"z\":-1.5},"
            "\"max\":{\"x\":0.5,\"y\":1.0,\"z\":1.5}"
        "}"
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
    TEST_ASSERT(cJSON_GetObjectItem(object, "vertex_count")->valueint == 8);
    TEST_ASSERT(cJSON_GetObjectItem(object, "triangle_count")->valueint == 12);
    TEST_ASSERT(cJSON_IsFalse(cJSON_GetObjectItem(object, "physics_extension_present")));
    TEST_ASSERT(cJSON_IsFalse(cJSON_GetObjectItem(object, "ray_tracing_extension_present")));

    TEST_ASSERT(build_fixture_path(project_root,
                                   "assets/mesh_assets/asset_project_mesh.runtime.json",
                                   copied_runtime_path,
                                   sizeof(copied_runtime_path)));
    TEST_ASSERT(path_exists(copied_runtime_path));
    TEST_ASSERT(file_contains(copied_runtime_path, "\"asset_id\":\"asset_project_mesh\""));

    cJSON_Delete(manifest);
    free(manifest_text);
    Layout_Free(&layout);
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

bool scene_export_run_tests(void) {
    const TestCase cases[] = {
        { "scene_export_emits_authoring_and_runtime_files", test_scene_export_emits_authoring_and_runtime_files },
        { "scene_export_uses_parent_scene_name_for_authoring_hint",
          test_scene_export_uses_parent_scene_name_for_authoring_hint },
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
        { "scene_project_scaffold_keeps_authoring_as_editable_import",
          test_scene_project_scaffold_keeps_authoring_as_editable_import },
        { "scene_import_accepts_supported_authoring_unit_metadata",
          test_scene_import_accepts_supported_authoring_unit_metadata },
        { "scene_import_rejects_unsupported_authoring_unit_metadata",
          test_scene_import_rejects_unsupported_authoring_unit_metadata },
        { "scene_import_rejects_runtime_scene_file",
          test_scene_import_rejects_runtime_scene_file },
    };
    return run_test_cases("SceneExport", cases, sizeof(cases) / sizeof(cases[0]));
}
