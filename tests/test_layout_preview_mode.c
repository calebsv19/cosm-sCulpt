#include "test_layout_internal.h"

static bool test_preview_mode_defaults_to_wireframe(void) {
    ld_test_init_runtime();
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_WIREFRAME);
    TEST_ASSERT(strcmp(Global_GetPreviewModeLabel(Global_GetPreviewMode()), "Wireframe") == 0);
    TEST_ASSERT(strcmp(Global_GetPreviewModeExportValue(Global_GetPreviewMode()), "wireframe") == 0);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_preview_mode_cycles_wireframe_flat_material(void) {
    ld_test_init_runtime();
    TEST_ASSERT(Global_TogglePreviewMode());
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_FLAT);
    TEST_ASSERT(strcmp(Global_GetPreviewModeExportValue(Global_GetPreviewMode()), "flat") == 0);
    TEST_ASSERT(Global_TogglePreviewMode());
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_MATERIAL);
    TEST_ASSERT(strcmp(Global_GetPreviewModeExportValue(Global_GetPreviewMode()), "material") == 0);
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

bool test_layout_preview_mode_run_tests(void) {
    const TestCase cases[] = {
        {"PreviewModeDefaultsToWireframe", test_preview_mode_defaults_to_wireframe},
        {"PreviewModeCyclesWireframeFlatMaterial", test_preview_mode_cycles_wireframe_flat_material},
        {"PreviewModeRejectsInvalidValues", test_preview_mode_rejects_invalid_values},
    };
    return run_test_cases("LayoutPreviewMode", cases, sizeof(cases) / sizeof(cases[0]));
}
