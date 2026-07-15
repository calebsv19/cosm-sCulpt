#include "test_layout_internal.h"
#include "Layout/scene/layout_mesh_asset_path_resolver.h"

#include <sys/stat.h>
#include <unistd.h>

static bool test_preview_mode_defaults_to_wireframe(void) {
    ld_test_init_runtime();
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_WIREFRAME);
    TEST_ASSERT(strcmp(Global_GetPreviewModeLabel(Global_GetPreviewMode()), "Wireframe") == 0);
    TEST_ASSERT(strcmp(Global_GetPreviewModeExportValue(Global_GetPreviewMode()), "wireframe") == 0);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_preview_mode_cycles_wireframe_flat_material_bounds(void) {
    ld_test_init_runtime();
    TEST_ASSERT(Global_TogglePreviewMode());
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_FLAT);
    TEST_ASSERT(strcmp(Global_GetPreviewModeExportValue(Global_GetPreviewMode()), "flat") == 0);
    TEST_ASSERT(Global_TogglePreviewMode());
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_MATERIAL);
    TEST_ASSERT(strcmp(Global_GetPreviewModeExportValue(Global_GetPreviewMode()), "material") == 0);
    TEST_ASSERT(Global_TogglePreviewMode());
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_BOUNDS);
    TEST_ASSERT(strcmp(Global_GetPreviewModeLabel(Global_GetPreviewMode()), "Bounds") == 0);
    TEST_ASSERT(strcmp(Global_GetPreviewModeExportValue(Global_GetPreviewMode()), "bounds") == 0);
    TEST_ASSERT(Global_TogglePreviewMode());
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_WIREFRAME);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_preview_mode_rejects_invalid_values(void) {
    ld_test_init_runtime();
    TEST_ASSERT(!Global_SetPreviewMode((LineDrawingPreviewMode)99));
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_WIREFRAME);
    TEST_ASSERT(Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_FLAT));
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_FLAT);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_mesh_path_resolver_recovers_legacy_desktop_library(void) {
    char root[256];
    char desktop[320];
    char stls[384];
    char library[448];
    char stored[512];
    char actual[512];
    char resolved[512];
    FILE* file = NULL;
    snprintf(root, sizeof(root), "/tmp/ld_mesh_path_%ld", (long)getpid());
    snprintf(desktop, sizeof(desktop), "%s/Desktop", root);
    snprintf(stls, sizeof(stls), "%s/stls", desktop);
    snprintf(library, sizeof(library), "%s/Legacy_Library", stls);
    snprintf(stored, sizeof(stored), "%s/Legacy_Library/skull.runtime.json", desktop);
    snprintf(actual, sizeof(actual), "%s/skull.runtime.json", library);
    TEST_ASSERT(mkdir(root, 0700) == 0);
    TEST_ASSERT(mkdir(desktop, 0700) == 0);
    TEST_ASSERT(mkdir(stls, 0700) == 0);
    TEST_ASSERT(mkdir(library, 0700) == 0);
    file = fopen(actual, "wb");
    TEST_ASSERT(file != NULL);
    TEST_ASSERT(fputs("{}", file) >= 0);
    fclose(file);

    ld_test_init_runtime();
    TEST_ASSERT(Layout_MeshAssetResolveRuntimePath(stored, resolved, sizeof(resolved)) ==
                LAYOUT_MESH_PATH_RELOCATED);
    TEST_ASSERT(strcmp(resolved, actual) == 0);
    ld_test_shutdown_runtime();
    return true;
}

bool test_layout_preview_mode_run_tests(void) {
    const TestCase cases[] = {
        {"PreviewModeDefaultsToWireframe", test_preview_mode_defaults_to_wireframe},
        {"PreviewModeCyclesWireframeFlatMaterialBounds", test_preview_mode_cycles_wireframe_flat_material_bounds},
        {"PreviewModeRejectsInvalidValues", test_preview_mode_rejects_invalid_values},
        {"MeshPathResolverRecoversLegacyDesktopLibrary", test_mesh_path_resolver_recovers_legacy_desktop_library},
    };
    return run_test_cases("LayoutPreviewMode", cases, sizeof(cases) / sizeof(cases[0]));
}
