#include "test_artifact_helpers.h"
#include "test_layout_internal.h"
#include "Core/editor_preferences.h"
#include "test_framework.h"

#include "Core/line_drawing_startup_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const LineDrawingStartupRootFallbackEntry* startup_config_find_entry(
    const LineDrawingStartupRootFallbackReport* report,
    LineDrawingStartupRootKind kind) {
    if (!report) return NULL;
    for (size_t i = 0u; i < report->count; ++i) {
        if (report->entries[i].kind == kind) return &report->entries[i];
    }
    return NULL;
}

static bool startup_config_make_dir(const char* path) {
    if (!path || !path[0]) return false;
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

static bool test_startup_config_reports_unset_roots(void) {
    LineDrawingDataPaths paths;
    LineDrawingStartupRootFallbackReport report = {0};
    const LineDrawingStartupRootFallbackEntry* input = NULL;
    const LineDrawingStartupRootFallbackEntry* output = NULL;

    memset(&paths, 0, sizeof(paths));
    TEST_ASSERT(LineDrawingStartupConfig_ApplyRootFallbacks(&paths, &report));
    TEST_ASSERT(report.count == LINE_DRAWING_STARTUP_ROOT_FALLBACK_CAP);
    TEST_ASSERT(report.changed);

    input = startup_config_find_entry(&report, LINE_DRAWING_STARTUP_ROOT_INPUT);
    output = startup_config_find_entry(&report, LINE_DRAWING_STARTUP_ROOT_OUTPUT);
    TEST_ASSERT(input != NULL);
    TEST_ASSERT(output != NULL);
    TEST_ASSERT(input->changed);
    TEST_ASSERT(input->reason == LINE_DRAWING_STARTUP_ROOT_FALLBACK_UNSET);
    TEST_ASSERT(strcmp(input->prior, "") == 0);
    TEST_ASSERT(strcmp(input->fallback, LineDrawingDataPaths_DefaultInputRoot()) == 0);
    TEST_ASSERT(strcmp(paths.input_root, LineDrawingDataPaths_DefaultInputRoot()) == 0);
    TEST_ASSERT(output->changed);
    TEST_ASSERT(output->reason == LINE_DRAWING_STARTUP_ROOT_FALLBACK_UNSET);
    TEST_ASSERT(strcmp(paths.output_root, LineDrawingDataPaths_DefaultOutputRoot()) == 0);
    return true;
}

static bool test_startup_config_reports_missing_roots(void) {
    LineDrawingDataPaths paths;
    LineDrawingStartupRootFallbackReport report = {0};
    const LineDrawingStartupRootFallbackEntry* layout = NULL;
    char missing_layout[PATH_MAX];
    TEST_ASSERT(ld_test_artifact_temp_path(missing_layout, sizeof(missing_layout), "ld_startup_config_missing_layout_root"));

    LineDrawingDataPaths_SetDefaults(&paths);
    snprintf(paths.layout_root, sizeof(paths.layout_root), "%s", missing_layout);
    (void)rmdir(missing_layout);

    TEST_ASSERT(LineDrawingStartupConfig_ApplyRootFallbacks(&paths, &report));
    TEST_ASSERT(report.changed);
    layout = startup_config_find_entry(&report, LINE_DRAWING_STARTUP_ROOT_LAYOUT);
    TEST_ASSERT(layout != NULL);
    TEST_ASSERT(layout->changed);
    TEST_ASSERT(layout->reason == LINE_DRAWING_STARTUP_ROOT_FALLBACK_MISSING);
    TEST_ASSERT(strcmp(layout->prior, missing_layout) == 0);
    TEST_ASSERT(strcmp(layout->fallback, LineDrawingDataPaths_DefaultLayoutRoot()) == 0);
    TEST_ASSERT(strcmp(paths.layout_root, LineDrawingDataPaths_DefaultLayoutRoot()) == 0);
    return true;
}

static bool test_startup_config_leaves_existing_roots_unchanged(void) {
    LineDrawingDataPaths paths;
    LineDrawingStartupRootFallbackReport report = {0};
    const LineDrawingStartupRootFallbackEntry* input = NULL;
    const LineDrawingStartupRootFallbackEntry* object_asset = NULL;
    char temp_root[PATH_MAX]; TEST_ASSERT(ld_test_artifact_temp_path(temp_root, sizeof(temp_root), "ld_startup_config_existing_XXXXXX"));
    char* root = NULL;

    root = mkdtemp(temp_root);
    TEST_ASSERT(root != NULL);

    snprintf(paths.input_root, sizeof(paths.input_root), "%s", root);
    snprintf(paths.output_root, sizeof(paths.output_root), "%s", root);
    snprintf(paths.layout_root, sizeof(paths.layout_root), "%s", root);
    snprintf(paths.object_asset_root, sizeof(paths.object_asset_root), "%s", root);

    TEST_ASSERT(startup_config_make_dir(root));
    TEST_ASSERT(LineDrawingStartupConfig_ApplyRootFallbacks(&paths, &report));
    TEST_ASSERT(report.count == LINE_DRAWING_STARTUP_ROOT_FALLBACK_CAP);
    TEST_ASSERT(!report.changed);

    input = startup_config_find_entry(&report, LINE_DRAWING_STARTUP_ROOT_INPUT);
    object_asset = startup_config_find_entry(&report, LINE_DRAWING_STARTUP_ROOT_OBJECT_ASSET);
    TEST_ASSERT(input != NULL);
    TEST_ASSERT(object_asset != NULL);
    TEST_ASSERT(!input->changed);
    TEST_ASSERT(input->reason == LINE_DRAWING_STARTUP_ROOT_FALLBACK_UNCHANGED);
    TEST_ASSERT(strcmp(input->prior, root) == 0);
    TEST_ASSERT(strcmp(paths.input_root, root) == 0);
    TEST_ASSERT(!object_asset->changed);
    TEST_ASSERT(object_asset->reason == LINE_DRAWING_STARTUP_ROOT_FALLBACK_UNCHANGED);
    TEST_ASSERT(strcmp(paths.object_asset_root, root) == 0);

    (void)rmdir(root);
    return true;
}

static bool test_editor_preferences_roundtrip_and_invalid(void) {
    ld_test_init_runtime();GlobalState* state=Global_Get();UIPanelState* ui=UIPanel_Get();
    char path[PATH_MAX];TEST_ASSERT(ld_test_artifact_temp_path(path, sizeof(path), "ld-editor-prefs-%ld.json",(long)getpid()));
    char* before=Layout_SaveToString(&state->layout);size_t history=Editor_UndoCount(&state->editor);
    ui->displayUnit=CORE_UNIT_MILLIMETER;state->activePlane.axis=VIEW_PLANE_YZ;
    state->freeViewCamera.enabled=true;state->freeViewCamera.yawDeg=67;state->freeViewCamera.pitchDeg=31;
    state->editor.viewportTool=VIEWPORT_TOOL_PAN;
    TEST_ASSERT(LineDrawingEditorPreferences_Save(path));
    ui->displayUnit=CORE_UNIT_FOOT;state->activePlane.axis=VIEW_PLANE_XY;state->freeViewCamera.enabled=false;state->editor.viewportTool=VIEWPORT_TOOL_SELECT;
    TEST_ASSERT(LineDrawingEditorPreferences_Load(path));
    TEST_ASSERT(ui->displayUnit==CORE_UNIT_MILLIMETER && state->activePlane.axis==VIEW_PLANE_YZ && state->freeViewCamera.enabled);
    TEST_ASSERT(state->editor.viewportTool==VIEWPORT_TOOL_PAN && state->freeViewCamera.yawDeg==67 && state->freeViewCamera.pitchDeg==31);
    state->editor.viewportTool=VIEWPORT_TOOL_LINE;TEST_ASSERT(LineDrawingEditorPreferences_Save(path));
    TEST_ASSERT(LineDrawingEditorPreferences_Load(path) && state->editor.viewportTool==VIEWPORT_TOOL_SELECT);
    FILE* file=fopen(path,"wb");TEST_ASSERT(file);fputs("{\"version\":1,\"displayUnit\":999}",file);fclose(file);
    TEST_ASSERT(!LineDrawingEditorPreferences_Load(path) && ui->displayUnit==CORE_UNIT_MILLIMETER);
    file=fopen(path,"wb");TEST_ASSERT(file);for(int i=0;i<3000;++i)fputc(' ',file);fclose(file);
    TEST_ASSERT(!LineDrawingEditorPreferences_Load(path));
    char* after=Layout_SaveToString(&state->layout);TEST_ASSERT(!strcmp(before,after) && history==Editor_UndoCount(&state->editor));
    free(before);free(after);unlink(path);ld_test_shutdown_runtime();return true;
}
static bool test_owned_temp_paths_are_checked(void) {
    const char* current = getenv("TMPDIR");
    char* saved = current ? strdup(current) : NULL;
    TEST_ASSERT(!current || saved);
    TEST_ASSERT(setenv("TMPDIR", "/owned/test root/", 1) == 0);
    char path[128];
    bool ok = ld_test_artifact_temp_path(path, sizeof(path), "fixture-%u", 7u) &&
        !strcmp(path, "/owned/test root/fixture-7");
    char short_path[8] = "before";
    ok = ok && !ld_test_artifact_temp_path(short_path, sizeof(short_path), "fixture") &&
        !short_path[0] && !ld_test_artifact_temp_path(path, sizeof(path), "../escape") &&
        !ld_test_artifact_temp_path(path, sizeof(path), "/absolute") &&
        !ld_test_artifact_temp_path(path, sizeof(path), "..");
    int restored = saved ? setenv("TMPDIR", saved, 1) : unsetenv("TMPDIR");
    free(saved);
    TEST_ASSERT(restored == 0 && ok);
    return true;
}

bool startup_config_run_tests(void) {
    const TestCase cases[] = {
        {"owned temporary paths are checked", test_owned_temp_paths_are_checked},
        {"editor preferences roundtrip and invalid", test_editor_preferences_roundtrip_and_invalid},
        {"reports unset roots", test_startup_config_reports_unset_roots},
        {"reports missing roots", test_startup_config_reports_missing_roots},
        {"leaves existing roots unchanged", test_startup_config_leaves_existing_roots_unchanged}
    };
    return run_test_cases("StartupConfig", cases, sizeof(cases) / sizeof(cases[0]));
}
