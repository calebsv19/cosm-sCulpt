// src/Input/input_mouse.c
#include "input_mouse.h"
#include "Input/input_mouse_internal.h"
#include "Input/input_mouse_drag.h"
#include "Input/input_mouse_drag_shared.h"
#include "Input/input_viewport_pick.h"
#include "Input/input_viewport_navigation.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Core/viewport_zoom.h"
#include "Core/workspace/line_drawing_object_workspace_view.h"
#include "Editor/editor.h"
#include "Editor/object_face_extrude.h"
#include "Editor/object_face_sketch.h"
#include "Editor/object_face_sketch_edit.h"
#include "Editor/scene_authoring_path_handles.h"
#include "ObjectAuthoring/object_authoring_document.h"

#include "UI/input_ui_panel.h"
#include "UI/object_workspace_viewport_hud.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_object_workspace_summary.h"
#include "UI/ui_panel_scene_list.h"
#include "UI/ui_panel_shell.h"

#include "Layout/Grid/grid.h"
#include "Math/math_util.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

static bool SelectObjectTopologyHit(GlobalState* state, Hitbox hit) {
    EditorState* editor = NULL;
    uint32_t body_id = 0u;
    bool selected_topology = false;
    const bool clicked_topology =
        hit.type == HITBOX_OBJECT_TOPOLOGY_VERTEX ||
        hit.type == HITBOX_OBJECT_TOPOLOGY_EDGE;
    const bool mode_allows_vertex =
        state && state->editor.objectEditSelectionMode == OBJECT_EDIT_SELECTION_VERTEX;
    const bool mode_allows_edge =
        state && state->editor.objectEditSelectionMode == OBJECT_EDIT_SELECTION_EDGE;
    const bool mode_matches_topology =
        (hit.type == HITBOX_OBJECT_TOPOLOGY_VERTEX && mode_allows_vertex) ||
        (hit.type == HITBOX_OBJECT_TOPOLOGY_EDGE && mode_allows_edge);

    if (!state ||
        !clicked_topology ||
        !mode_matches_topology ||
        state->workspaceMode != LINE_DRAWING_WORKSPACE_MODE_OBJECT ||
        !state->objectAuthoring.attached ||
        hit.index <= 0) {
        return false;
    }

    editor = &state->editor;
    body_id = (uint32_t)hit.index;
    Editor_ClearAnchorSelection(editor);
    Editor_ObjectFaceSketchDeselect(editor);
    Editor_ObjectFaceExtrudeClear(editor);
    editor->selectedObject3DId = body_id;
    editor->selectedObjectAssetBodyId = body_id;
    editor->selectedObjectAssetFace = OBJECT3D_FACE_NONE;
    editor->objectAuthoringMode = Editor_ObjectAuthoringIdleMode(editor);
    editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->selectedWallIndex = -1;
    editor->selectedHandleAnchor = -1;
    editor->selectedHandleComponent = -1;

    if (hit.type == HITBOX_OBJECT_TOPOLOGY_VERTEX) {
        selected_topology =
            ObjectAuthoringDocument_SetVertexSelection(&state->objectAuthoring.document,
                                                       body_id,
                                                       (uint32_t)hit.subIndex);
    } else {
        selected_topology =
            ObjectAuthoringDocument_SetEdgeSelection(&state->objectAuthoring.document,
                                                     body_id,
                                                     (uint32_t)hit.subIndex);
    }
    if (!selected_topology) {
        (void)ObjectAuthoringDocument_SetSelection(&state->objectAuthoring.document,
                                                   body_id,
                                                   OBJECT3D_FACE_NONE);
    }
    SyncObjectFaceSketchTarget(editor);
    Global_FlagHitboxesDirty();
    return selected_topology;
}

static bool SelectObjectTopologyAt(GlobalState* state, int mx, int my) {
    ViewportPickResult pick = {0};
    if (!InputMouse_ObjectEditTopologyModeActive(state)) return false;
    pick = ViewportPick_ResolveObjectWorkspaceHit(state, mx, my, true);
    return SelectObjectTopologyHit(state, pick.finalHit);
}

static bool PreserveObjectWorkspaceBodyOnEmptyHit(GlobalState* state, uint32_t fallback_body_id) {
    EditorState* editor = NULL;
    uint32_t body_id = 0u;

    if (!state ||
        state->workspaceMode != LINE_DRAWING_WORKSPACE_MODE_OBJECT ||
        !state->objectAuthoring.attached) {
        return false;
    }

    editor = &state->editor;
    body_id = editor->selectedObjectAssetBodyId != 0u
        ? editor->selectedObjectAssetBodyId
        : editor->selectedObject3DId;
    if (body_id == 0u) body_id = fallback_body_id;
    if (body_id == 0u ||
        !Layout_ObjectStore_FindConst(&state->layout.objectStore, body_id)) {
        return false;
    }

    editor->selectedObject3DId = body_id;
    editor->selectedObjectAssetBodyId = body_id;
    editor->selectedObjectAssetFace = OBJECT3D_FACE_NONE;
    Editor_ObjectFaceExtrudeClear(editor);
    Editor_ObjectFaceSketchDeselect(editor);
    editor->objectAuthoringMode = Editor_ObjectAuthoringIdleMode(editor);
    (void)ObjectAuthoringDocument_SetSelection(&state->objectAuthoring.document,
                                               body_id,
                                               OBJECT3D_FACE_NONE);
    SyncObjectFaceSketchTarget(editor);
    return true;
}

// 		Scroll to zoom in/out
// ============================================================
static void HandleMouseWheel(AppContext* ctx, SDL_MouseWheelEvent* wheel) {
    GlobalState* state = Global_Get();
    int mx = 0;
    int my = 0;
    if (ctx && ctx->window) {
        SDL_GetMouseState(&mx, &my);
    } else {
        mx = Global_GetScreenWidth() / 2;
        my = Global_GetScreenHeight() / 2;
    }

    float delta = (wheel->preciseY != 0.0f) ? wheel->preciseY : (float)wheel->y;
    if (wheel->direction == SDL_MOUSEWHEEL_FLIPPED) {
        delta = -delta;
    }
    if (fabsf(delta) <= 0.0001f) return;
    if (UIPanel_HandleLoadMenuWheel(mx, my, delta)) return;
    if (UIPanel_HandleSceneListWheel(mx, my, delta)) return;
    if (UIPanel_ObjectWorkspaceHandleModelTreeWheel(mx, my, delta)) return;
    if (ResolvePointerPaneLane(mx, my) != POINTER_PANE_CENTER) return;

    // Exponential zoom for smoother high-precision wheel/trackpad input.
    float factor = powf(1.08f, delta);
    if (LineDrawingViewportZoom_Apply(state, factor, (float)mx, (float)my)) {
        Global_FlagGridChanged();
    }
}


//        Left click: select point (priority) or wall
// ============================================================
static void HandleLeftMouseDown(SDL_MouseButtonEvent* btn) {
    PointerPaneLane pane_lane = POINTER_PANE_OUTSIDE;
    LineDrawingPaneHost* pane_host = ResolvePaneHostMutable();
    if (pane_host &&
        !UIPanel_IsSaveDialogActive() &&
        !UIPanel_IsRootDialogActive() &&
        !UIPanel_IsPrismDimensionDialogActive() &&
        !UIPanel_IsSceneBoundsDialogActive() &&
        LineDrawingPaneHost_BeginSplitterDrag(pane_host, (float)btn->x, (float)btn->y)) {
        return;
    }
    if (UIPanel_IsSaveDialogActive() ||
        UIPanel_IsRootDialogActive() ||
        UIPanel_IsPrismDimensionDialogActive() ||
        UIPanel_IsSceneBoundsDialogActive()) {
        (void)UIPanel_HandleClick(btn->x, btn->y);
        return;
    }

    pane_lane = ResolvePointerPaneLane(btn->x, btn->y);
    if (pane_lane == POINTER_PANE_TOP) {
        (void)LineDrawingEditorTopbar_HandleClick(btn->x, btn->y);
        return;
    }
    if (pane_lane == POINTER_PANE_LEFT || pane_lane == POINTER_PANE_RIGHT) {
        (void)UIPanel_HandleClick(btn->x, btn->y);
        return;
    }
    if (pane_lane == POINTER_PANE_OUTSIDE) {
        return;
    }

    {
        GlobalState* state = Global_Get();
        if (pane_lane == POINTER_PANE_CENTER &&
            state &&
            state->editor.primitivePlacementPreview != PRIMITIVE_PLACEMENT_PREVIEW_NONE) {
            const PrimitivePlacementPreviewKind preview = state->editor.primitivePlacementPreview;
            const bool placed =
                preview == PRIMITIVE_PLACEMENT_PREVIEW_PLANE
                    ? UIPanel_CreatePlanePrimitiveFromActiveContext(false)
                    : (preview == PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM
                           ? UIPanel_CreateRectPrismPrimitiveFromActiveContext(false)
                           : false);
            if (placed) {
                draggingHandle = false;
                draggingPan = false;
                draggingAnchor = false;
                draggingSelectionBox = false;
                Global_FlagHitboxesDirty();
                UpdateHover(btn->x, btn->y);
                return;
            }
        }
        if (pane_lane == POINTER_PANE_CENTER &&
            SelectObjectTopologyAt(state, btn->x, btn->y)) {
            UpdateHover(btn->x, btn->y);
            return;
        }
        if (pane_lane == POINTER_PANE_CENTER &&
            LineDrawingObjectWorkspaceViewportHud_HandleClick(state, btn->x, btn->y)) {
            SyncObjectFaceSketchTarget(&state->editor);
            UpdateHover(btn->x, btn->y);
            return;
        }
        if (pane_lane == POINTER_PANE_CENTER &&
            !InputMouse_ObjectEditTopologyModeActive(state) &&
            InputMouse_IsObjectFaceAuthoringModal(&state->editor)) {
            if (Editor_ObjectFaceExtrudeHandleLeftMouseDown(state, btn->x, btn->y)) {
                draggingHandle = false;
                draggingPan = false;
                draggingAnchor = false;
                draggingSelectionBox = false;
                Global_FlagHitboxesDirty();
                UpdateHover(btn->x, btn->y);
                return;
            }
            if (Editor_ObjectFaceSketchHandleLeftMouseDown(state, btn->x, btn->y)) {
                draggingHandle = false;
                draggingPan = false;
                draggingAnchor = false;
                draggingSelectionBox = false;
                Global_FlagHitboxesDirty();
                UpdateHover(btn->x, btn->y);
                return;
            }
            return;
        }
    }

    draggingPan = false;
    draggingHandle = false;
    draggingAnchor = false;
    draggingGizmo = false;
    draggingObjectResize = false;
    draggingObjectGizmo = false;
    draggingObjectTranslate = false;
    draggingObjectRotate = false;
    draggingObjectScale = false;
    draggingSceneBoundsGizmo = false;
    draggingSceneAuthoringPathHandle = false;
    draggingAnchorIndex = -1;
    anchorDragCaptured = false;
    lastMx = btn->x;
    lastMy = btn->y;
    dragPrecise = (SDL_GetModState() & KMOD_ALT) != 0;

    GlobalState* state = Global_Get();
    EditorState* editor = &state->editor;
    const bool topologyEditMode = InputMouse_ObjectEditTopologyModeActive(state);
    if (InputMouse_ObjectModeEnabled() &&
        !topologyEditMode &&
        Editor_ObjectFaceExtrudeHandleLeftMouseDown(state, btn->x, btn->y)) {
        draggingHandle = false;
        draggingPan = false;
        draggingAnchor = false;
        draggingSelectionBox = false;
        Global_FlagHitboxesDirty();
        UpdateHover(btn->x, btn->y);
        return;
    }
    if (InputMouse_ObjectModeEnabled() &&
        !topologyEditMode &&
        Editor_ObjectFaceSketchHandleLeftMouseDown(state, btn->x, btn->y)) {
        draggingHandle = false;
        draggingPan = false;
        draggingAnchor = false;
        draggingSelectionBox = false;
        Global_FlagHitboxesDirty();
        UpdateHover(btn->x, btn->y);
        return;
    }
    if (InputMouse_ObjectModeEnabled() &&
        InputMouse_IsObjectFaceSketchDrawActive(editor)) {
        draggingHandle = false;
        draggingPan = false;
        draggingAnchor = false;
        draggingSelectionBox = false;
        Global_FlagHitboxesDirty();
        UpdateHover(btn->x, btn->y);
        return;
    }

    Editor_ResetGizmoDrag(editor);
    ResetObjectResizeDrag(editor);
    ResetObjectGizmoDrag(editor);
    ResetObjectTranslateDrag(editor);
    ResetObjectRotateDrag(editor);
    ResetObjectScaleDrag(editor);
    ResetSceneBoundsGizmoDrag(editor);
    bool shiftSelect = (SDL_GetModState() & KMOD_SHIFT) != 0;
    bool startedGizmoDrag = false;
    bool startedObjectResize = false;
    bool startedObjectGizmoDrag = false;
    bool startedObjectTranslateDrag = false;
    bool startedObjectRotateDrag = false;
    bool startedObjectScaleDrag = false;
    bool startedSceneBoundsGizmoDrag = false;
    bool startedSceneAuthoringPathDrag = false;
    SceneAuthoringPathHandleRef sceneAuthoringHandle = SceneAuthoringPathHandleRef_None();
    SceneAuthoringGizmoPickResult sceneAuthoringPick = SceneAuthoringGizmoPickResult_None();

    if (shiftSelect &&
        SceneAuthoringPathHandles_InsertControlPointAtScreen(state,
                                                             editor,
                                                             btn->x,
                                                             btn->y,
                                                             &sceneAuthoringHandle)) {
        startedSceneAuthoringPathDrag =
            BeginSceneAuthoringPathHandleDragSession(state,
                                                     editor,
                                                     (SceneAuthoringGizmoPickResult){
                                                         .handle = sceneAuthoringHandle,
                                                         .part = SCENE_AUTHORING_GIZMO_PART_CENTER,
                                                         .axis = GIZMO_AXIS_DIR_POS_X
                                                     },
                                                     btn->x,
                                                     btn->y);
        draggingHandle = false;
        draggingPan = false;
        draggingSceneAuthoringPathHandle = startedSceneAuthoringPathDrag;
        Global_FlagHitboxesDirty();
        UpdateHover(btn->x, btn->y);
        return;
    }

    if (SceneAuthoringPathHandles_Pick(state, btn->x, btn->y, &sceneAuthoringPick)) {
        startedSceneAuthoringPathDrag =
            BeginSceneAuthoringPathHandleDragSession(state,
                                                     editor,
                                                     sceneAuthoringPick,
                                                     btn->x,
                                                     btn->y);
        draggingHandle = false;
        draggingPan = false;
        draggingSceneAuthoringPathHandle = startedSceneAuthoringPathDrag;
        Global_FlagHitboxesDirty();
        UpdateHover(btn->x, btn->y);
        return;
    }

    ViewportPickResult pick = ViewportPick_ResolveObjectWorkspaceHit(state,
                                                                      btn->x,
                                                                      btn->y,
                                                                      true);
    Hitbox hit = pick.finalHit;
    const SpaceViewContext orbit_view_ctx = SpaceAdapter_BuildViewContext(state);
    const bool reserve_alt_orbit =
        SpaceAdapter_IsFreeViewEnabled(&orbit_view_ctx) &&
        (SDL_GetModState() & KMOD_ALT) != 0 &&
        (hit.type == HITBOX_NONE || hit.type == HITBOX_OBJECT3D);

    bool clickedHandle = (hit.type == HITBOX_HANDLE);
    bool clickedGizmo = (hit.type == HITBOX_GIZMO_AXIS);
    bool clickedObjectGizmo = (hit.type == HITBOX_OBJECT3D_GIZMO_AXIS);
    bool clickedSketch = (hit.type == HITBOX_OBJECT_FACE_SKETCH_HANDLE ||
                          hit.type == HITBOX_OBJECT_FACE_SKETCH_BODY);
    bool clickedSceneBoundsGizmo = (hit.type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS);
    bool clickedSceneBoundsHandle = (hit.type == HITBOX_SCENE_BOUNDS_HANDLE);
    bool clickedPrismHandle = (hit.type == HITBOX_OBJECT3D_PRISM_HANDLE);
    bool clickedObjectResize = (hit.type == HITBOX_OBJECT3D_PLANE_CORNER ||
                                hit.type == HITBOX_OBJECT3D_PLANE_EDGE);
    bool clickedTopology = (hit.type == HITBOX_OBJECT_TOPOLOGY_VERTEX ||
                            hit.type == HITBOX_OBJECT_TOPOLOGY_EDGE);
    bool doubleClick = (!shiftSelect && btn->clicks >= 2);
    const bool object_mode = InputMouse_ObjectModeEnabled();

    if (reserve_alt_orbit) {
        draggingPan = false;
        Global_FlagHitboxesDirty();
        UpdateHover(btn->x, btn->y);
        return;
    }

    // Priority: anchor selection overrides wall
    if (hit.type == HITBOX_OBJECT_FACE_SKETCH_HANDLE ||
        hit.type == HITBOX_OBJECT_FACE_SKETCH_BODY) {
        Editor_ClearAnchorSelection(editor);
        (void)Editor_ObjectFaceSketchSelect(editor, (ObjectFaceSketchHandleKind)hit.subIndex);
        UIPanel_FocusObjectAuthoringTab(UIPanel_Get());
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        startedObjectResize = Editor_ObjectFaceSketchBeginEditDrag(
            state,
            (ObjectFaceSketchHandleKind)hit.subIndex,
            btn->x,
            btn->y);
    } else if (hit.type == HITBOX_POINT) {
        bool alreadySelected = Editor_IsAnchorSelected(editor, hit.index);
        if (doubleClick) {
            Editor_SelectAnchor(editor, hit.index, false);
        } else if (!alreadySelected) {
            Editor_SelectAnchor(editor, hit.index, shiftSelect);
        } else {
            editor->selectedAnchorIndex = hit.index;
        }
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        editor->selectedObject3DId = 0u;
        ClearObjectAuthoringSelection(editor);
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
        if (!doubleClick) {
            draggingAnchor = true;
            draggingAnchorIndex = hit.index;
            editor->isDraggingAnchor = true;
            editor->isPreciseDrag = dragPrecise;
            Editor_BeginAnchorDrag(editor, &state->layout);
        } else {
            draggingAnchor = false;
            editor->isDraggingAnchor = false;
        }
    } else if (hit.type == HITBOX_HANDLE) {
        Editor_SelectAnchor(editor, hit.index, false);
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = hit.index;
        editor->selectedHandleComponent = hit.subIndex;
        editor->selectedObject3DId = 0u;
        ClearObjectAuthoringSelection(editor);
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        Editor_HistoryCapture(editor, &state->layout);
    } else if (hit.type == HITBOX_GIZMO_AXIS) {
        Editor_SelectAnchor(editor, hit.index, false);
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        editor->selectedObject3DId = 0u;
        ClearObjectAuthoringSelection(editor);
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        startedGizmoDrag = BeginGizmoDragSession(state,
                                                 editor,
                                                 hit.index,
                                                 (GizmoAxisDirection)hit.subIndex,
                                                 btn->x,
                                                 btn->y);
    } else if (hit.type == HITBOX_OBJECT3D_PLANE_CORNER ||
               hit.type == HITBOX_OBJECT3D_PLANE_EDGE) {
        Editor_ClearAnchorSelection(editor);
        editor->selectedObject3DId = (uint32_t)hit.index;
        editor->selectedObjectAssetBodyId = object_mode ? (uint32_t)hit.index : 0u;
        editor->selectedObjectAssetFace = OBJECT3D_FACE_NONE;
        SyncObjectFaceSketchTarget(editor);
        editor->selectedObject3DResizeHandle = hit.subIndex;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, (uint32_t)hit.index);
        SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
        const bool freeView = (state->spaceMode == SPACE_MODE_3D) &&
                              SpaceAdapter_IsFreeViewEnabled(&viewCtx);
        if (object &&
            (object->kind == OBJECT3D_KIND_PLANE ||
             object->kind == OBJECT3D_KIND_RECT_PRISM) &&
            freeView) {
            startedObjectResize = false;
        } else {
            startedObjectResize = BeginObjectResizeDragSession(state,
                                                               editor,
                                                               (uint32_t)hit.index,
                                                               (PlaneResizeHandleKind)hit.subIndex);
        }
    } else if (hit.type == HITBOX_OBJECT3D_PRISM_HANDLE) {
        Editor_ClearAnchorSelection(editor);
        editor->selectedObject3DId = (uint32_t)hit.index;
        editor->selectedObjectAssetBodyId = object_mode ? (uint32_t)hit.index : 0u;
        editor->selectedObjectAssetFace = OBJECT3D_FACE_NONE;
        SyncObjectFaceSketchTarget(editor);
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = hit.subIndex;
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
    } else if (clickedTopology && object_mode && state->objectAuthoring.attached) {
        (void)SelectObjectTopologyHit(state, hit);
    } else if (hit.type == HITBOX_OBJECT3D_GIZMO_AXIS) {
        const int selectedPlaneHandle = editor->selectedObject3DResizeHandle;
        const int selectedPrismHandle = editor->selectedObject3DPrismHandle;
        Editor_ClearAnchorSelection(editor);
        editor->selectedObject3DId = (uint32_t)hit.index;
        editor->selectedObjectAssetBodyId = object_mode ? (uint32_t)hit.index : 0u;
        editor->selectedObjectAssetFace = OBJECT3D_FACE_NONE;
        SyncObjectFaceSketchTarget(editor);
        editor->selectedObject3DResizeHandle = selectedPlaneHandle;
        editor->selectedObject3DPrismHandle = selectedPrismHandle;
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, (uint32_t)hit.index);
        ObjectHandleGizmoTarget handleTarget = ObjectHandleGizmoTarget_None();
        if (ObjectHandleGizmoTarget_FromSelection(object,
                                                  (uint32_t)hit.index,
                                                  (PlaneResizeHandleKind)editor->selectedObject3DResizeHandle,
                                                  (RectPrismResizeHandleKind)editor->selectedObject3DPrismHandle,
                                                  &handleTarget) ||
            (object_mode &&
             state->objectAuthoring.attached &&
             ObjectHandleGizmoTarget_FromAuthoringSelection(
                 object,
                 &state->objectAuthoring.document,
                 &handleTarget))) {
            startedObjectGizmoDrag = BeginObjectHandleGizmoDragSession(state,
                                                                       editor,
                                                                       handleTarget,
                                                                       (RectPrismAxisDirection)hit.subIndex,
                                                                       btn->x,
                                                                       btn->y);
        } else {
            if (editor->object3DSizeMode) {
                startedObjectScaleDrag = BeginObjectScaleDragSession(state,
                                                                     editor,
                                                                     (uint32_t)hit.index,
                                                                     (GizmoAxisDirection)hit.subIndex,
                                                                     btn->x,
                                                                     btn->y);
            } else if (editor->object3DRotateMode) {
                startedObjectRotateDrag = BeginObjectRotateDragSession(state,
                                                                       editor,
                                                                       (uint32_t)hit.index,
                                                                       (GizmoAxisDirection)hit.subIndex,
                                                                       btn->x,
                                                                       btn->y);
            } else {
                startedObjectTranslateDrag = BeginObjectTranslateDragSession(state,
                                                                             editor,
                                                                             (uint32_t)hit.index,
                                                                             (GizmoAxisDirection)hit.subIndex,
                                                                             btn->x,
                                                                             btn->y);
            }
        }
    } else if (hit.type == HITBOX_SCENE_BOUNDS_HANDLE) {
        Editor_ClearAnchorSelection(editor);
        editor->selectedSceneBoundsHandle = hit.subIndex;
        editor->selectedObject3DId = 0u;
        ClearObjectAuthoringSelection(editor);
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
    } else if (hit.type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS) {
        const int selectedBoundsHandle = editor->selectedSceneBoundsHandle;
        Editor_ClearAnchorSelection(editor);
        editor->selectedSceneBoundsHandle = selectedBoundsHandle;
        editor->selectedObject3DId = 0u;
        ClearObjectAuthoringSelection(editor);
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        startedSceneBoundsGizmoDrag =
            BeginSceneBoundsGizmoDragSession(state,
                                             editor,
                                             (SceneBoundsHandleKind)selectedBoundsHandle,
                                             (RectPrismAxisDirection)hit.subIndex,
                                             btn->x,
                                             btn->y);
    } else if (hit.type == HITBOX_WALL) {
        editor->selectedWallIndex = hit.index;
        Editor_ClearAnchorSelection(editor);
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        editor->selectedObject3DId = 0u;
        ClearObjectAuthoringSelection(editor);
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    } else if (hit.type == HITBOX_OBJECT3D) {
        const Object3D* object =
            Layout_ObjectStore_FindConst(&state->layout.objectStore, (uint32_t)hit.index);
        Object3DFaceKind selected_face = OBJECT3D_FACE_NONE;
        if (object_mode &&
            editor->objectEditSelectionMode == OBJECT_EDIT_SELECTION_FACE) {
            selected_face = ResolveObjectAuthoringFaceForSelection(state, object, btn->x, btn->y);
        }
        Editor_ClearAnchorSelection(editor);
        Editor_ObjectFaceSketchDeselect(editor);
        editor->selectedObject3DId = (uint32_t)hit.index;
        editor->selectedObjectAssetBodyId = object_mode ? (uint32_t)hit.index : 0u;
        editor->selectedObjectAssetFace = object_mode ? selected_face : OBJECT3D_FACE_NONE;
        if (object_mode && state->objectAuthoring.attached) {
            (void)ObjectAuthoringDocument_SetSelection(&state->objectAuthoring.document,
                                                       (uint32_t)hit.index,
                                                       selected_face);
        }
        SyncObjectFaceSketchTarget(editor);
        editor->objectAuthoringMode =
            object_mode && editor->objectEditSelectionMode == OBJECT_EDIT_SELECTION_FACE
                ? Editor_ObjectAuthoringIdleMode(editor)
                : OBJECT_AUTHORING_MODE_NONE;
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->selectedWallIndex = -1;
        editor->selectedHandleAnchor = -1;
        editor->selectedHandleComponent = -1;
        if (doubleClick && object_mode && selected_face != OBJECT3D_FACE_NONE) {
            (void)LineDrawingObjectWorkspaceView_FocusFace(state,
                                                           (uint32_t)hit.index,
                                                           selected_face);
        }
    } else {
        if (shiftSelect) {
            editor->selectionBoxActive = true;
            editor->selectionBoxAdditive = true;
            draggingSelectionBox = true;
            editor->selectionBoxStart = ScreenToWorld(btn->x, btn->y, &state->grid);
            editor->selectionBoxEnd = editor->selectionBoxStart;
            lastMx = btn->x;
            lastMy = btn->y;
        } else {
            const bool same_face_committed_sketch =
                object_mode &&
                Editor_ObjectFaceSketchHasCommittedRectangle(editor) &&
                editor->selectedObjectAssetBodyId == editor->objectFaceSketchBodyId &&
                editor->selectedObjectAssetFace == editor->objectFaceSketchFace;
            const bool preserveCommittedSketchSelection =
                same_face_committed_sketch &&
                (Editor_ObjectFaceSketchIsSelected(editor) ||
                 editor->objectAuthoringMode == OBJECT_AUTHORING_MODE_SKETCH_SELECT);
            const uint32_t empty_hit_body_id = editor->selectedObjectAssetBodyId != 0u
                ? editor->selectedObjectAssetBodyId
                : editor->selectedObject3DId;
            editor->selectedWallIndex = -1;
            Editor_ClearAnchorSelection(editor);
            editor->selectedHandleAnchor = -1;
            editor->selectedHandleComponent = -1;
            if (preserveCommittedSketchSelection) {
                (void)Editor_ObjectFaceSketchSelect(editor,
                                                    (ObjectFaceSketchHandleKind)
                                                        editor->selectedObjectFaceSketchHandle);
                editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
                editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
            } else if (same_face_committed_sketch) {
                editor->selectedObject3DId = editor->selectedObjectAssetBodyId;
                Editor_ObjectFaceExtrudeClear(editor);
                Editor_ObjectFaceSketchDeselect(editor);
                editor->objectAuthoringMode = OBJECT_AUTHORING_MODE_FACE_SELECT;
                editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
                editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
            } else if (hit.type == HITBOX_NONE &&
                       PreserveObjectWorkspaceBodyOnEmptyHit(state, empty_hit_body_id)) {
                editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
                editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
            } else {
                editor->selectedObject3DId = 0u;
                Editor_ObjectFaceSketchDeselect(editor);
                ClearObjectAuthoringSelection(editor);
                editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
                editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
            }
        }
    }

    if (clickedSketch) {
        draggingHandle = false;
        draggingPan = false;
        draggingObjectResize = startedObjectResize;
    } else if (clickedHandle) {
        draggingHandle = true;
        draggingPan = false;
    } else if (clickedGizmo) {
        draggingHandle = false;
        draggingPan = false;
        draggingGizmo = startedGizmoDrag;
    } else if (clickedObjectGizmo) {
        draggingHandle = false;
        draggingPan = false;
        draggingObjectGizmo = startedObjectGizmoDrag;
        draggingObjectTranslate = startedObjectTranslateDrag;
        draggingObjectRotate = startedObjectRotateDrag;
        draggingObjectScale = startedObjectScaleDrag;
    } else if (clickedSceneBoundsGizmo) {
        draggingHandle = false;
        draggingPan = false;
        draggingSceneBoundsGizmo = startedSceneBoundsGizmoDrag;
    } else if (clickedSceneBoundsHandle) {
        draggingHandle = false;
        draggingPan = false;
        draggingSceneBoundsGizmo = false;
    } else if (clickedObjectResize) {
        draggingHandle = false;
        draggingPan = false;
        draggingObjectResize = startedObjectResize;
    } else if (clickedPrismHandle) {
        draggingHandle = false;
        draggingPan = false;
        draggingObjectResize = false;
    } else if (clickedTopology) {
        draggingHandle = false;
        draggingPan = false;
    } else if (draggingAnchor) {
        draggingHandle = false;
        draggingPan = false;
    } else if (draggingSelectionBox) {
        draggingHandle = false;
        draggingPan = false;
        editor->selectionBoxActive = true;
    } else {
        draggingHandle = false;
        draggingPan = !(object_mode && hit.type == HITBOX_OBJECT3D);
    }

    Global_FlagHitboxesDirty();
    UpdateHover(btn->x, btn->y);
}


// 		Right click: place wall (snap to grid)
// ============================================================
static void HandleRightMouseDown(SDL_MouseButtonEvent* btn) {
    if (UIPanel_IsCapturingKeyboard()) {
        return;
    }
    if (ResolvePointerPaneLane(btn->x, btn->y) != POINTER_PANE_CENTER) {
        return;
    }
    GlobalState* state = Global_Get();
    if (state && (InputMouse_IsObjectFaceAuthoringModal(&state->editor) ||
                  state->editor.objectFaceSketchHasRectangle ||
                  state->editor.objectFaceExtrudeHasPreview)) {
        Editor_ObjectFaceSketchClear(&state->editor);
        Editor_ObjectFaceExtrudeClear(&state->editor);
        Global_FlagHitboxesDirty();
        UpdateHover(btn->x, btn->y);
        return;
    }
    Grid* grid = &state->grid;
    EditorState* editor = &state->editor;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    Vec3 world3 = {0};
    if (!SpaceAdapter_ScreenToWorld(btn->x, btn->y, grid, &viewCtx, true, &world3)) return;
    Editor_ClickAt(editor, world3);
}

// 		Public interface
// ============================================================
void Input_MouseHandle(AppContext *ctx, SDL_Event* event) {
    LineDrawingPaneHost* pane_host = ResolvePaneHostMutable();

    switch (event->type) {
        case SDL_MOUSEWHEEL:
            HandleMouseWheel(ctx, &event->wheel);
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (InputViewportNavigation_HandleMouseButton(&event->button))
                break;
            if (event->button.button == SDL_BUTTON_LEFT)
                HandleLeftMouseDown(&event->button);
            else if (event->button.button == SDL_BUTTON_RIGHT)
                HandleRightMouseDown(&event->button);
            break;

        case SDL_MOUSEBUTTONUP:
            if (InputViewportNavigation_HandleMouseButton(&event->button)) {
                break;
            }
            if (event->button.button == SDL_BUTTON_LEFT) {
                UIPanel_HandleSceneListMouseUp();
                UIPanel_ObjectWorkspaceHandleModelTreeMouseUp();
                if (pane_host && LineDrawingPaneHost_IsSplitterDragActive(pane_host)) {
                    LineDrawingPaneHost_EndSplitterDrag(pane_host);
                    LineDrawingPaneHost_UpdatePointer(pane_host,
                                                      (float)event->button.x,
                                                      (float)event->button.y);
                    UpdateHover(event->button.x, event->button.y);
                    break;
                }
                draggingPan = false;
                draggingHandle = false;
                draggingAnchor = false;
                draggingGizmo = false;
                draggingObjectResize = false;
                draggingObjectGizmo = false;
                draggingObjectTranslate = false;
                draggingObjectRotate = false;
                draggingObjectScale = false;
                draggingSceneBoundsGizmo = false;
                draggingSceneAuthoringPathHandle = false;
                draggingSelectionBox = false;
                draggingAnchorIndex = -1;
                anchorDragCaptured = false;
                GlobalState* state = Global_Get();
                if (state) {
                    Editor_ObjectFaceExtrudeHandleLeftMouseUp(state,
                                                              event->button.x,
                                                              event->button.y);
                    Editor_ObjectFaceSketchEndEditDrag(state,
                                                       event->button.x,
                                                       event->button.y);
                    Editor_ObjectFaceSketchHandleLeftMouseUp(state,
                                                            event->button.x,
                                                            event->button.y);
                    Global_FlagHitboxesDirty();
                    state->editor.isDraggingAnchor = false;
                    state->editor.isResizingObject3D = false;
                    state->editor.isResizingSceneBounds = false;
                    state->editor.isRotatingObject3D = false;
                    state->editor.isPreciseDrag = false;
                    Editor_EndAnchorDrag(&state->editor);
                    Editor_ResetGizmoDrag(&state->editor);
                    ResetObjectResizeDrag(&state->editor);
                    ResetObjectGizmoDrag(&state->editor);
                    ResetObjectTranslateDrag(&state->editor);
                    ResetObjectRotateDrag(&state->editor);
                    ResetObjectScaleDrag(&state->editor);
                    ResetSceneBoundsGizmoDrag(&state->editor);
                    ResetSceneAuthoringPathHandleDrag(&state->editor);
                    if (state->editor.selectionBoxActive) {
                        Vec2 start = state->editor.selectionBoxStart;
                        Vec2 end = state->editor.selectionBoxEnd;
                        Vec2 min = { fminf(start.x, end.x), fminf(start.y, end.y) };
                        Vec2 max = { fmaxf(start.x, end.x), fmaxf(start.y, end.y) };
                        Editor_SelectAnchorsInBox(&state->editor,
                                                  &state->layout,
                                                  min,
                                                  max,
                                                  state->editor.selectionBoxAdditive);
                        state->editor.selectionBoxActive = false;
                        state->editor.selectionBoxAdditive = false;
                    }
                }
            }
            break;

        case SDL_MOUSEMOTION:
            if (pane_host) {
                if (LineDrawingPaneHost_IsSplitterDragActive(pane_host)) {
                    (void)LineDrawingPaneHost_UpdateSplitterDrag(pane_host,
                                                                 (float)event->motion.x,
                                                                 (float)event->motion.y);
                    UpdateHover(event->motion.x, event->motion.y);
                    break;
                }
                LineDrawingPaneHost_UpdatePointer(pane_host,
                                                  (float)event->motion.x,
                                                  (float)event->motion.y);
            }
            if (InputViewportNavigation_HandleMouseMotion(&event->motion)) {
                UpdateHover(event->motion.x, event->motion.y);
                break;
            }
            {
                GlobalState* state = Global_Get();
                if (state) {
                    Editor_ObjectFaceExtrudeHandleMouseMotion(state,
                                                              event->motion.x,
                                                              event->motion.y);
                    Editor_ObjectFaceSketchUpdateEditDrag(state,
                                                          event->motion.x,
                                                          event->motion.y);
                    Editor_ObjectFaceSketchHandleMouseMotion(state,
                                                             event->motion.x,
                                                             event->motion.y);
                    if (InputMouse_IsObjectFaceAuthoringModal(&state->editor)) {
                        UpdateHover(event->motion.x, event->motion.y);
                        break;
                    }
                }
            }
            HandleMouseDrag(&event->motion);
            UpdateHover(event->motion.x, event->motion.y);
            break;
    }
}
