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
    const char* missing_layout = "/tmp/ld_startup_config_missing_layout_root";

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
    char temp_root[] = "/tmp/ld_startup_config_existing_XXXXXX";
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

bool startup_config_run_tests(void) {
    const TestCase cases[] = {
        {"reports unset roots", test_startup_config_reports_unset_roots},
        {"reports missing roots", test_startup_config_reports_missing_roots},
        {"leaves existing roots unchanged", test_startup_config_leaves_existing_roots_unchanged}
    };
    return run_test_cases("StartupConfig", cases, sizeof(cases) / sizeof(cases[0]));
}
