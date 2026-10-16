/* Window transitions end capture only; documents, drafts and undo stay owned
 * by their existing editor/UI subsystems. Keep the last admitted edit. */
#include "UI/shared_overlay.h"
#include "UI/shared_special.h"
#include "UI/shared_controls.h"
#include "UI/shared_text.h"
#include "Input/input_handler.h"
#include "Input/input_mouse_drag.h"
#include "Input/input_mouse_drag_shared.h"
#include "Input/input_viewport_navigation.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_camera_path.h"

void Input_CancelWindowCapture(void) {
    LDControls_CancelAll();
    LDOverlay_Cancel();
    UIPanel_SharedTextCancelCapture();
    LDInline_Cancel();
    GlobalState* state = Global_Get();
    EditorState* editor = &state->editor;
    InputViewportNavigation_ResetGesture();
    Editor_EndAnchorDrag(editor);
    Editor_ResetGizmoDrag(editor);
    ResetObjectResizeDrag(editor);
    ResetObjectGizmoDrag(editor);
    ResetObjectTranslateDrag(editor);
    ResetObjectRotateDrag(editor);
    ResetObjectScaleDrag(editor);
    ResetSceneBoundsGizmoDrag(editor);
    ResetSceneAuthoringPathHandleDrag(editor);
    draggingPan = draggingHandle = draggingAnchor = draggingGizmo = false;
    draggingObjectResize = draggingObjectGizmo = draggingObjectTranslate = false;
    draggingObjectRotate = draggingObjectScale = draggingSceneBoundsGizmo = false;
    draggingSceneAuthoringPathHandle = draggingSelectionBox = false;
    draggingAnchorIndex = -1;
    anchorDragCaptured = false;
    editor->selectionBoxActive = false;
    editor->objectFaceSketchDragging = false;
    editor->objectFaceSketchEditDragging = false;
    editor->objectFaceExtrudeDragging = false;
    LineDrawingPaneHost_EndSplitterDrag(&state->paneHost);
    UIPanelState* ui = UIPanel_Get();
    ui->sectionDragging = false;
    ui->sceneList.scrollbarDragging = false;
    ui->objectModelTree.operationScrollbarDragging = false;
    ui->loadMenu.scrollbarDragging = false;
    ui->measurement.travel_dragging = false;
    for (int i = 0; i < UI_PANEL_RIGHT_TAB_COUNT; ++i)
        ui->rightScroll[i].scrollbarDragging = false;
    UIPanel_CameraPathCancelCapture();
    SDL_CaptureMouse(SDL_FALSE);
    SDL_SetCursor(SDL_GetDefaultCursor());
}
