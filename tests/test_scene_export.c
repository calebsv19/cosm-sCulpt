#include "test_framework.h"

#include "Layout/layout.h"
#include "Tools/scene_export.h"

#include <stdio.h>
#include <string.h>

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

bool scene_export_run_tests(void) {
    const TestCase cases[] = {
        { "scene_export_emits_authoring_and_runtime_files", test_scene_export_emits_authoring_and_runtime_files },
    };
    return run_test_cases("SceneExport", cases, sizeof(cases) / sizeof(cases[0]));
}
