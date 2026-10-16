#include "test_layout_internal.h"
#include "Input/input_handler.h"
#include "Input/input_mouse_drag_shared.h"
#include "Input/input_viewport_navigation.h"
#include "UI/ui_panel.h"
#include "kit_ui_window_sdl.h"
#include <string.h>

static bool test_cancel_preserves_document_drafts_camera_history(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    UIPanelState* ui = UIPanel_Get();
    char* before = Layout_SaveToString(&state->layout);
    TEST_ASSERT(before);
    FreeViewCamera camera = state->freeViewCamera;
    size_t undo = Editor_UndoCount(&state->editor), redo = Editor_RedoCount(&state->editor);
    bool dirty = state->layoutDirtySinceSave;
    ui->saveDialog.active = true;
    strcpy(ui->saveDialog.buffer, "unsaved-window-draft");
    ui->saveDialog.length = ui->saveDialog.cursor = strlen(ui->saveDialog.buffer);
    strcpy(ui->measurement.travel_text[0], "0.275");
    ui->measurement.travel_dragging = true;
    ui->sectionDragging = ui->sceneList.scrollbarDragging = true;
    draggingPan = draggingAnchor = draggingSelectionBox = draggingObjectTranslate = true;
    state->editor.selectionBoxActive = true;
    Input_CancelWindowCapture();
    char* after = Layout_SaveToString(&state->layout);
    TEST_ASSERT(after && !strcmp(before, after));
    TEST_ASSERT(!memcmp(&camera, &state->freeViewCamera, sizeof(camera)));
    TEST_ASSERT(undo == Editor_UndoCount(&state->editor) && redo == Editor_RedoCount(&state->editor));
    TEST_ASSERT(dirty == state->layoutDirtySinceSave && ui->saveDialog.active);
    TEST_ASSERT(!strcmp(ui->saveDialog.buffer, "unsaved-window-draft") && ui->saveDialog.cursor == ui->saveDialog.length);
    TEST_ASSERT(!strcmp(ui->measurement.travel_text[0], "0.275"));
    TEST_ASSERT(!ui->measurement.travel_dragging && !ui->sectionDragging && !ui->sceneList.scrollbarDragging);
    TEST_ASSERT(!draggingPan && !draggingAnchor && !draggingSelectionBox && !draggingObjectTranslate);
    TEST_ASSERT(!state->editor.selectionBoxActive);
    Layout_FreeString(before); Layout_FreeString(after);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_logical_retina_and_outside_mapping(void) {
    KitUiWindowState window = {.logical_width=1280, .logical_height=720,
        .drawable_width=2560, .drawable_height=1440};
    int x, y;
    TEST_ASSERT(kit_ui_window_map_point_sdl(&window, 1280, 720, 640, 360, &x, &y));
    TEST_ASSERT(x == 640 && y == 360);
    TEST_ASSERT(kit_ui_window_map_point_sdl(&window, 1280, 720, -15, 740, &x, &y));
    TEST_ASSERT(x == -15 && y == 740);
    TEST_ASSERT(kit_ui_window_map_point_sdl(&window, 2560, 1440, 640, 360, &x, &y));
    TEST_ASSERT(x == 1280 && y == 720);
    window.logical_width = 0;
    TEST_ASSERT(!kit_ui_window_map_point_sdl(&window, 1280, 720, 0, 0, &x, &y));
    return true;
}

bool window_foundation_run_tests(void) {
    const TestCase cases[] = {
        {"CapturePreservesDocumentDraftCameraHistory", test_cancel_preserves_document_drafts_camera_history},
        {"RetinaLogicalAndOutsideMapping", test_logical_retina_and_outside_mapping}
    };
    return run_test_cases("WindowFoundation", cases, sizeof(cases)/sizeof(cases[0]));
}
