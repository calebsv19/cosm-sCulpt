#include "test_framework.h"

#include "Layout/layout.h"
#include "Tools/scene_import.h"
#include "Tools/scene_export.h"

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

static bool test_scene_export_emits_authoring_and_runtime_files(void) {
    Layout layout;
    LineDrawingSceneExportPaths export_paths;
    char diagnostics[256];

    Layout_Init(&layout, 1.0f);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){0.0f, 0.0f, 0.0f}) >= 0);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){2.0f, 1.0f, 0.0f}) >= 0);
    Layout_AddWall3(&layout, (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){2.0f, 1.0f, 0.0f});

    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToOutputRoot(&layout,
                                                                "tests/fixtures/demo room.json",
                                                                "tmp/test_scene_export",
                                                                &export_paths,
                                                                diagnostics,
                                                                sizeof(diagnostics)));
    TEST_ASSERT(strstr(export_paths.scene_dir, "tmp/test_scene_export/demo room") != NULL);
    TEST_ASSERT(strstr(export_paths.authoring_path, "tmp/test_scene_export/demo room/scene_authoring.json") != NULL);
    TEST_ASSERT(strstr(export_paths.runtime_path, "tmp/test_scene_export/demo room/scene_runtime.json") != NULL);
    TEST_ASSERT(strstr(export_paths.scene_id, "scene_line_drawing_demo_room") != NULL);
    TEST_ASSERT(file_contains(export_paths.authoring_path, "\"scene_authoring_v1\""));
    TEST_ASSERT(file_contains(export_paths.runtime_path, "\"schema_variant\":\"scene_runtime_v1\""));

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

bool scene_export_run_tests(void) {
    const TestCase cases[] = {
        { "scene_export_emits_authoring_and_runtime_files", test_scene_export_emits_authoring_and_runtime_files },
        { "scene_export_embeds_layout_snapshot_and_round_trips_import",
          test_scene_export_embeds_layout_snapshot_and_round_trips_import },
    };
    return run_test_cases("SceneExport", cases, sizeof(cases) / sizeof(cases[0]));
}
