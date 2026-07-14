#include "Input/input_mouse_internal.h"
#include "Input/input_viewport_pick.h"

#include "Core/space_mode_adapter.h"
#include "Core/workspace/line_drawing_object_workspace_view.h"
#include "Editor/object_face_extrude.h"
#include "Editor/object_face_sketch.h"
#include "Editor/object_face_sketch_edit.h"
#include "Editor/object3d_origin_pick.h"
#include "Editor/scene_authoring_path_handles.h"

#include "UI/ui_panel.h"

#include "Layout/hitbox_system.h"
#include "Layout/scene/layout_object_faces.h"

#include <SDL2/SDL.h>
#include <math.h>

static const float kObject3DOriginPickCaptureRadiusPx = 28.0f;

static SDL_Rect PaneRectToSDLRect(CorePaneRect rect) {
    SDL_Rect out = {0, 0, 0, 0};
    int x0 = (int)floorf(rect.x);
    int y0 = (int)floorf(rect.y);
    int x1 = (int)ceilf(rect.x + rect.width);
    int y1 = (int)ceilf(rect.y + rect.height);
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    out.x = x0;
    out.y = y0;
    out.w = x1 - x0;
    out.h = y1 - y0;
    return out;
}

static bool ResolvePaneRect(LineDrawingPaneRole role, SDL_Rect* out_rect) {
    const LineDrawingPaneHost* pane_host = NULL;
    CorePaneRect pane_rect = {0};
    if (!out_rect) return false;
    *out_rect = (SDL_Rect){0, 0, 0, 0};

    pane_host = Global_GetPaneHostConst();
    if (!pane_host || !pane_host->initialized) return false;
    if (!LineDrawingPaneHost_GetRectForRole(pane_host, role, &pane_rect)) return false;

    *out_rect = PaneRectToSDLRect(pane_rect);
    return out_rect->w > 0 && out_rect->h > 0;
}

static bool ResolveViewportRect(SDL_Rect* out_rect) {
    const LineDrawingPaneHost* pane_host = NULL;
    CorePaneRect pane_rect = {0};
    if (!out_rect) return false;
    *out_rect = (SDL_Rect){0, 0, 0, 0};

    pane_host = Global_GetPaneHostConst();
    if (!pane_host || !pane_host->initialized) return false;
    if (!LineDrawingPaneHost_GetViewportRect(pane_host, &pane_rect)) return false;

    *out_rect = PaneRectToSDLRect(pane_rect);
    return out_rect->w > 0 && out_rect->h > 0;
}

LineDrawingPaneHost* ResolvePaneHostMutable(void) {
    return Global_GetPaneHost();
}

PointerPaneLane ResolvePointerPaneLane(int x, int y) {
    SDL_Point point = { x, y };
    SDL_Rect top = {0, 0, 0, 0};
    SDL_Rect left = {0, 0, 0, 0};
    SDL_Rect right = {0, 0, 0, 0};
    SDL_Rect viewport = {0, 0, 0, 0};
    bool any_resolved = false;

    if (ResolveViewportRect(&viewport)) {
        any_resolved = true;
        if (SDL_PointInRect(&point, &viewport)) return POINTER_PANE_CENTER;
    }
    if (ResolvePaneRect(LINE_DRAWING_PANE_ROLE_TOP_BAR, &top)) {
        any_resolved = true;
        if (SDL_PointInRect(&point, &top)) return POINTER_PANE_TOP;
    }
    if (ResolvePaneRect(LINE_DRAWING_PANE_ROLE_LEFT_CONTROLS, &left)) {
        any_resolved = true;
        if (SDL_PointInRect(&point, &left)) return POINTER_PANE_LEFT;
    }
    if (ResolvePaneRect(LINE_DRAWING_PANE_ROLE_RIGHT_CONTROLS, &right)) {
        any_resolved = true;
        if (SDL_PointInRect(&point, &right)) return POINTER_PANE_RIGHT;
    }

    if (!any_resolved) {
        return POINTER_PANE_CENTER;
    }
    return POINTER_PANE_OUTSIDE;
}

void ClearHoverState(EditorState* editor) {
    if (!editor) return;
    editor->hoveredAnchorIndex = -1;
    editor->hoveredWallIndex = -1;
    editor->hoveredHandleAnchor = -1;
    editor->hoveredHandleComponent = -1;
    editor->hoveredGizmoAxis = -1;
    editor->hoveredObject3DGizmoAxis = -1;
    editor->hoveredObject3DId = 0u;
    editor->hoveredObjectAssetBodyId = 0u;
    editor->hoveredObjectAssetFace = OBJECT3D_FACE_NONE;
    editor->hoveredObjectTopologyBodyId = 0u;
    editor->hoveredObjectTopologyVertexIndex = -1;
    editor->hoveredObjectTopologyEdgeIndex = -1;
    editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->hoveredSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
    editor->hoveredSceneBoundsGizmoAxis = -1;
    editor->hoveredObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_NONE;
    editor->hoveredSceneAuthoringGizmoPart = SCENE_AUTHORING_GIZMO_PART_NONE;
    editor->hoveredSceneAuthoringHandleKind = SCENE_AUTHORING_PATH_HANDLE_NONE;
    editor->hoveredSceneAuthoringGizmoAxis = -1;
    editor->hoveredSceneAuthoringPathElementKind = LINE_DRAWING_SCENE_PATH_ELEMENT_NONE;
    editor->hoveredSceneAuthoringPathIndex = -1;
    editor->hoveredSceneAuthoringControlPointIndex = -1;
    editor->hoveredSceneAuthoringPathSegmentIndex = -1;
}

bool InputMouse_ObjectModeEnabled(void) {
    return Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
}

bool InputMouse_ObjectEditTopologyModeActive(const GlobalState* state) {
    if (!state ||
        state->workspaceMode != LINE_DRAWING_WORKSPACE_MODE_OBJECT ||
        !state->objectAuthoring.attached) {
        return false;
    }
    return state->editor.objectEditSelectionMode == OBJECT_EDIT_SELECTION_EDGE ||
           state->editor.objectEditSelectionMode == OBJECT_EDIT_SELECTION_VERTEX;
}

bool InputMouse_IsObjectFaceAuthoringModal(const EditorState* editor) {
    return editor &&
           ((editor->objectAuthoringMode == OBJECT_AUTHORING_MODE_SKETCH_DRAW &&
             (editor->objectFaceSketchToolArmed || editor->objectFaceSketchDragging)) ||
            (editor->objectAuthoringMode == OBJECT_AUTHORING_MODE_OPERATION_PREVIEW &&
             (editor->objectFaceExtrudeToolArmed ||
              editor->objectFaceExtrudeDragging ||
              editor->objectFaceExtrudeHasPreview)) ||
            editor->objectFaceSketchEditDragging);
}

bool InputMouse_IsObjectFaceSketchDrawActive(const EditorState* editor) {
    return editor &&
           editor->objectAuthoringMode == OBJECT_AUTHORING_MODE_SKETCH_DRAW &&
           (editor->objectFaceSketchToolArmed || editor->objectFaceSketchDragging);
}

void ClearObjectAuthoringSelection(EditorState* editor) {
    if (!editor) return;
    Editor_ObjectFaceExtrudeClear(editor);
    Editor_ObjectFaceSketchDeselect(editor);
    editor->selectedObjectAssetBodyId = 0u;
    editor->selectedObjectAssetFace = OBJECT3D_FACE_NONE;
    editor->objectAuthoringMode = OBJECT_AUTHORING_MODE_NONE;
}

void SyncObjectFaceSketchTarget(EditorState* editor) {
    if (!editor) return;
    if (!editor->objectFaceSketchToolArmed &&
        !editor->objectFaceSketchDragging &&
        !editor->objectFaceSketchHasRectangle) {
        editor->objectAuthoringMode = Editor_ObjectAuthoringIdleMode(editor);
        return;
    }
    if (editor->selectedObjectAssetBodyId == editor->objectFaceSketchBodyId &&
        editor->selectedObjectAssetFace == editor->objectFaceSketchFace) {
        return;
    }
    Editor_ObjectFaceSketchClear(editor);
    Editor_ObjectFaceExtrudeClear(editor);
    editor->objectAuthoringMode = Editor_ObjectAuthoringIdleMode(editor);
}

Object3DFaceKind ResolveObjectAuthoringFaceAtPointer(const GlobalState* state,
                                                     const Object3D* object,
                                                     int mouse_x,
                                                     int mouse_y) {
    SpaceViewContext view_ctx = {0};
    Object3DFaceKind face = OBJECT3D_FACE_NONE;

    if (!state || !object) return OBJECT3D_FACE_NONE;
    view_ctx = SpaceAdapter_BuildViewContext(state);
    if (Layout_Object3D_PickVisibleFaceAtScreenPoint(object,
                                                     &view_ctx,
                                                     &state->grid,
                                                     mouse_x,
                                                     mouse_y,
                                                     &face)) {
        return face;
    }
    return OBJECT3D_FACE_NONE;
}

Hitbox ResolveViewportObjectBodyHit(const GlobalState* state, int mx, int my, Hitbox base_hit) {
    SpaceViewContext view_ctx = {0};

    if (!state) return base_hit;
    if (base_hit.type == HITBOX_OBJECT3D) return base_hit;
    view_ctx = SpaceAdapter_BuildViewContext(state);
    return Editor_ResolveObject3DBodyPick(&state->layout,
                                          &state->grid,
                                          &view_ctx,
                                          mx,
                                          my,
                                          base_hit,
                                          kObject3DOriginPickCaptureRadiusPx);
}

Object3DFaceKind ResolveObjectAuthoringFaceForSelection(const GlobalState* state,
                                                        const Object3D* object,
                                                        int mouse_x,
                                                        int mouse_y) {
    Object3DFaceKind face = OBJECT3D_FACE_NONE;
    SpaceViewContext view_ctx = {0};

    if (!state || !object) return OBJECT3D_FACE_NONE;
    face = ResolveObjectAuthoringFaceAtPointer(state, object, mouse_x, mouse_y);
    if (face != OBJECT3D_FACE_NONE) return face;
    if (state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_OBJECT &&
        state->editor.selectedObjectAssetBodyId == object->objectId &&
        state->editor.selectedObjectAssetFace != OBJECT3D_FACE_NONE &&
        Layout_Object3DFaceKind_IsValidForObject(object, state->editor.selectedObjectAssetFace)) {
        return state->editor.selectedObjectAssetFace;
    }

    view_ctx = SpaceAdapter_BuildViewContext(state);
    if (Layout_Object3D_DefaultAuthoringFaceForView(object, &view_ctx, &face)) {
        return face;
    }
    return OBJECT3D_FACE_NONE;
}

void UpdateHover(int mx, int my) {
    GlobalState* state = Global_Get();
    Hitbox hit = {0};
    if (!state) return;
    EditorState* editor = &state->editor;
    ViewportPickResult pick = {0};
    SceneAuthoringGizmoPickResult authoring_pick = SceneAuthoringGizmoPickResult_None();

    if (UIPanel_IsCapturingKeyboard()) {
        ClearHoverState(editor);
        return;
    }
    pick = ViewportPick_ResolveObjectWorkspaceHit(state, mx, my, true);
    if (pick.paneLane != POINTER_PANE_CENTER) {
        ClearHoverState(editor);
        return;
    }

    if (SceneAuthoringPathHandles_Pick(state, mx, my, &authoring_pick)) {
        ClearHoverState(editor);
        editor->hoveredSceneAuthoringGizmoPart = (int)authoring_pick.part;
        editor->hoveredSceneAuthoringHandleKind = (int)authoring_pick.handle.kind;
        editor->hoveredSceneAuthoringGizmoAxis =
            authoring_pick.part == SCENE_AUTHORING_GIZMO_PART_AXIS
                ? (int)authoring_pick.axis
                : -1;
        editor->hoveredSceneAuthoringPathElementKind =
            (int)authoring_pick.handle.element_kind;
        if (authoring_pick.handle.kind == SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT) {
            editor->hoveredSceneAuthoringPathIndex = (int)authoring_pick.handle.path_index;
            if (authoring_pick.handle.element_kind ==
                LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT) {
                editor->hoveredSceneAuthoringPathSegmentIndex =
                    (int)authoring_pick.handle.segment_index;
            } else {
                editor->hoveredSceneAuthoringControlPointIndex =
                    (int)authoring_pick.handle.control_index;
            }
        }
        return;
    }

    if (InputMouse_IsObjectFaceSketchDrawActive(editor)) {
        ClearHoverState(editor);
        editor->hoveredObject3DId = editor->objectFaceSketchBodyId;
        editor->hoveredObjectAssetBodyId = editor->objectFaceSketchBodyId;
        editor->hoveredObjectAssetFace = editor->objectFaceSketchFace;
        return;
    }

    hit = pick.finalHit;
    editor->hoveredSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
    editor->hoveredSceneBoundsGizmoAxis = -1;
    editor->hoveredObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_NONE;
    editor->hoveredObjectTopologyBodyId = 0u;
    editor->hoveredObjectTopologyVertexIndex = -1;
    editor->hoveredObjectTopologyEdgeIndex = -1;
    if (hit.type == HITBOX_OBJECT_FACE_SKETCH_HANDLE ||
        hit.type == HITBOX_OBJECT_FACE_SKETCH_BODY) {
        editor->hoveredObjectFaceSketchHandle = hit.subIndex;
        editor->hoveredObject3DId = (uint32_t)hit.index;
        editor->hoveredObjectAssetBodyId = (uint32_t)hit.index;
        editor->hoveredObjectAssetFace = editor->objectFaceSketchFace;
        editor->hoveredObject3DGizmoAxis = -1;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
    } else if (hit.type == HITBOX_POINT) {
        editor->hoveredAnchorIndex = hit.index;
        editor->hoveredWallIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
        editor->hoveredObject3DGizmoAxis = -1;
        editor->hoveredObject3DId = 0u;
        editor->hoveredObjectAssetBodyId = 0u;
        editor->hoveredObjectAssetFace = OBJECT3D_FACE_NONE;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    } else if (hit.type == HITBOX_HANDLE) {
        editor->hoveredHandleAnchor = hit.index;
        editor->hoveredHandleComponent = hit.subIndex;
        editor->hoveredAnchorIndex = hit.index;
        editor->hoveredWallIndex = -1;
        editor->hoveredGizmoAxis = -1;
        editor->hoveredObject3DGizmoAxis = -1;
        editor->hoveredObject3DId = 0u;
        editor->hoveredObjectAssetBodyId = 0u;
        editor->hoveredObjectAssetFace = OBJECT3D_FACE_NONE;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    } else if (hit.type == HITBOX_GIZMO_AXIS) {
        editor->hoveredGizmoAxis = hit.subIndex;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredAnchorIndex = hit.index;
        editor->hoveredWallIndex = -1;
        editor->hoveredObject3DGizmoAxis = -1;
        editor->hoveredObject3DId = 0u;
        editor->hoveredObjectAssetBodyId = 0u;
        editor->hoveredObjectAssetFace = OBJECT3D_FACE_NONE;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    } else if (hit.type == HITBOX_OBJECT3D_GIZMO_AXIS) {
        editor->hoveredObject3DId = (uint32_t)hit.index;
        editor->hoveredObjectAssetBodyId = 0u;
        editor->hoveredObjectAssetFace = OBJECT3D_FACE_NONE;
        editor->hoveredObject3DGizmoAxis = hit.subIndex;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
    } else if (hit.type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS) {
        editor->hoveredSceneBoundsGizmoAxis = hit.subIndex;
        editor->hoveredObject3DGizmoAxis = -1;
        editor->hoveredObject3DId = 0u;
        editor->hoveredObjectAssetBodyId = 0u;
        editor->hoveredObjectAssetFace = OBJECT3D_FACE_NONE;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
    } else if (hit.type == HITBOX_SCENE_BOUNDS_HANDLE) {
        editor->hoveredSceneBoundsHandle = hit.subIndex;
        editor->hoveredObject3DGizmoAxis = -1;
        editor->hoveredObject3DId = 0u;
        editor->hoveredObjectAssetBodyId = 0u;
        editor->hoveredObjectAssetFace = OBJECT3D_FACE_NONE;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
    } else if (hit.type == HITBOX_WALL) {
        editor->hoveredWallIndex = hit.index;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
        editor->hoveredObject3DGizmoAxis = -1;
        editor->hoveredObject3DId = 0u;
        editor->hoveredObjectAssetBodyId = 0u;
        editor->hoveredObjectAssetFace = OBJECT3D_FACE_NONE;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    } else if (hit.type == HITBOX_OBJECT3D_PRISM_HANDLE) {
        editor->hoveredObject3DId = (uint32_t)hit.index;
        editor->hoveredObjectAssetBodyId = 0u;
        editor->hoveredObjectAssetFace = OBJECT3D_FACE_NONE;
        editor->hoveredObject3DPrismHandle = hit.subIndex;
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
        editor->hoveredObject3DGizmoAxis = -1;
    } else if (hit.type == HITBOX_OBJECT_TOPOLOGY_VERTEX ||
               hit.type == HITBOX_OBJECT_TOPOLOGY_EDGE) {
        editor->hoveredObject3DId = (uint32_t)hit.index;
        editor->hoveredObjectAssetBodyId = (uint32_t)hit.index;
        editor->hoveredObjectAssetFace = OBJECT3D_FACE_NONE;
        editor->hoveredObjectTopologyBodyId = (uint32_t)hit.index;
        if (hit.type == HITBOX_OBJECT_TOPOLOGY_VERTEX) {
            editor->hoveredObjectTopologyVertexIndex = hit.subIndex;
            editor->hoveredObjectTopologyEdgeIndex = -1;
        } else {
            editor->hoveredObjectTopologyVertexIndex = -1;
            editor->hoveredObjectTopologyEdgeIndex = hit.subIndex;
        }
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
        editor->hoveredObject3DGizmoAxis = -1;
    } else if (hit.type == HITBOX_OBJECT3D_PLANE_CORNER ||
               hit.type == HITBOX_OBJECT3D_PLANE_EDGE) {
        editor->hoveredObject3DId = (uint32_t)hit.index;
        editor->hoveredObjectAssetBodyId = 0u;
        editor->hoveredObjectAssetFace = OBJECT3D_FACE_NONE;
        editor->hoveredObject3DResizeHandle = hit.subIndex;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
        editor->hoveredObject3DGizmoAxis = -1;
    } else if (hit.type == HITBOX_OBJECT3D) {
        editor->hoveredObject3DId = (uint32_t)hit.index;
        if (InputMouse_ObjectModeEnabled()) {
            const Object3D* object =
                Layout_ObjectStore_FindConst(&state->layout.objectStore, (uint32_t)hit.index);
            editor->hoveredObjectAssetBodyId = (uint32_t)hit.index;
            if (editor->objectEditSelectionMode == OBJECT_EDIT_SELECTION_FACE) {
                editor->hoveredObjectAssetFace =
                    ResolveObjectAuthoringFaceAtPointer(state, object, mx, my);
                if (editor->hoveredObjectAssetFace == OBJECT3D_FACE_NONE &&
                    editor->selectedObjectAssetBodyId == (uint32_t)hit.index &&
                    Layout_Object3DFaceKind_IsValidForObject(object,
                                                             editor->selectedObjectAssetFace)) {
                    editor->hoveredObjectAssetFace = editor->selectedObjectAssetFace;
                }
            } else {
                editor->hoveredObjectAssetFace = OBJECT3D_FACE_NONE;
            }
        } else {
            editor->hoveredObjectAssetBodyId = 0u;
            editor->hoveredObjectAssetFace = OBJECT3D_FACE_NONE;
        }
        editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        editor->hoveredWallIndex = -1;
        editor->hoveredAnchorIndex = -1;
        editor->hoveredHandleAnchor = -1;
        editor->hoveredHandleComponent = -1;
        editor->hoveredGizmoAxis = -1;
        editor->hoveredObject3DGizmoAxis = -1;
    } else {
        ClearHoverState(editor);
    }
}
