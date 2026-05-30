#include "Core/workspace/line_drawing_object_workspace_view.h"

#include "Core/line_drawing_pane_host.h"
#include "Core/space_mode_adapter.h"
#include "Editor/object_face_extrude.h"
#include "Editor/object_face_sketch.h"
#include "Layout/layout.h"
#include "Layout/scene/layout_object_faces.h"
#include "UI/ui_panel_shell.h"

#include <math.h>

static void ObjectWorkspaceView_GetViewportCenter(const GlobalState* state,
                                                  float* out_x,
                                                  float* out_y) {
    if (out_x) *out_x = 0.0f;
    if (out_y) *out_y = 0.0f;
    if (!state || !out_x || !out_y) return;

    *out_x = (float)state->screenWidth * 0.5f;
    *out_y = (float)state->screenHeight * 0.5f;
    (void)LineDrawingPaneHost_GetViewportCenter(&state->paneHost, out_x, out_y);
}

static void ObjectWorkspaceView_CenterGridOnProjectedPoint(GlobalState* state,
                                                           Vec2 projected_center) {
    float viewport_center_x = 0.0f;
    float viewport_center_y = 0.0f;
    float pixels_per_unit = 0.0f;

    if (!state) return;
    ObjectWorkspaceView_GetViewportCenter(state, &viewport_center_x, &viewport_center_y);
    pixels_per_unit = state->grid.gridSize * state->grid.scale;
    if (pixels_per_unit <= 0.0f) return;

    state->grid.offsetX = projected_center.x - (viewport_center_x / pixels_per_unit);
    state->grid.offsetY = projected_center.y - (viewport_center_y / pixels_per_unit);
}

static void ObjectWorkspaceView_SetCameraForward(FreeViewCamera* camera, Vec3 forward) {
    Vec3 dir = Vec3_Normalize(forward);
    const float z = fmaxf(-1.0f, fminf(1.0f, dir.z));
    if (!camera) return;

    camera->enabled = true;
    camera->yawDeg = RadToDeg(atan2f(dir.y, dir.x));
    camera->pitchDeg = RadToDeg(asinf(z));
    FreeView_NormalizeOrbitAngles(camera);
}

static bool ObjectWorkspaceView_HasCommittedSketchForFace(const GlobalState* state,
                                                          uint32_t object_id,
                                                          Object3DFaceKind face) {
    if (!state || object_id == 0u || face == OBJECT3D_FACE_NONE) return false;
    return state->editor.objectFaceSketchHasRectangle &&
           state->editor.objectFaceSketchBodyId == object_id &&
           state->editor.objectFaceSketchFace == face;
}

static bool ObjectWorkspaceView_HasCommittedSketchForObject(const GlobalState* state,
                                                            uint32_t object_id) {
    if (!state || object_id == 0u) return false;
    return state->editor.objectFaceSketchHasRectangle &&
           state->editor.objectFaceSketchBodyId == object_id;
}

static void ObjectWorkspaceView_ResetFaceAuthoringChrome(GlobalState* state) {
    if (!state) return;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    state->editor.selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
    state->editor.hoveredObject3DGizmoAxis = -1;
    state->editor.activeObject3DGizmoAxis = -1;
}

bool LineDrawingObjectWorkspaceView_EnterFreeView(GlobalState* state,
                                                  uint32_t object_id) {
    const Object3D* object = NULL;
    Vec3 focus_world = {0.0f, 0.0f, 0.0f};
    SpaceViewContext view_ctx = {0};
    const bool preserve_committed_sketch =
        ObjectWorkspaceView_HasCommittedSketchForObject(state, object_id);
    const float grid_size =
        (state && state->grid.gridSize > 0.0f) ? state->grid.gridSize : 1.0f;

    if (!state) return false;
    if (object_id != 0u) {
        object = Layout_ObjectStore_FindConst(&state->layout.objectStore, object_id);
        if (!object) return false;
    }

    if (object) {
        focus_world = object->transform.position;
        (void)Layout_FitSceneBounds3DToObject(&state->layout, object_id, grid_size);
    }

    Editor_ObjectFaceExtrudeClear(&state->editor);
    if (!preserve_committed_sketch) {
        Editor_ObjectFaceSketchClear(&state->editor);
    }
    ObjectWorkspaceView_ResetFaceAuthoringChrome(state);
    Grid_init(&state->grid, grid_size, state->screenWidth, state->screenHeight);
    state->activePlane = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f };
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = focus_world;
    state->editor.selectedObject3DId = object_id;
    state->editor.selectedObjectAssetBodyId = object_id;
    state->editor.selectedObjectAssetFace = OBJECT3D_FACE_NONE;
    state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_NONE;
    FreeView_NormalizeOrbitAngles(&state->freeViewCamera);

    view_ctx = SpaceAdapter_BuildViewContext(state);
    ObjectWorkspaceView_CenterGridOnProjectedPoint(state,
                                                   SpaceAdapter_ProjectToView(focus_world,
                                                                              &view_ctx));
    Global_FlagGridChanged();
    Global_FlagHitboxesDirty();
    return true;
}

bool LineDrawingObjectWorkspaceView_FocusFace(GlobalState* state,
                                              uint32_t object_id,
                                              Object3DFaceKind face) {
    const Object3D* object = NULL;
    PlaneFrame3 frame = {0};
    SpaceViewContext view_ctx = {0};
    Vec3 face_forward = {0.0f, 0.0f, 1.0f};
    const bool preserve_committed_sketch =
        ObjectWorkspaceView_HasCommittedSketchForFace(state, object_id, face);

    if (!state || object_id == 0u || face == OBJECT3D_FACE_NONE) return false;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore, object_id);
    if (!object) return false;
    if (!Layout_Object3DFace_GetFrame(object, face, &frame)) return false;

    Editor_ObjectFaceExtrudeClear(&state->editor);
    if (!preserve_committed_sketch) {
        Editor_ObjectFaceSketchClear(&state->editor);
    } else {
        state->editor.objectFaceSketchFrame = frame;
    }
    ObjectWorkspaceView_ResetFaceAuthoringChrome(state);
    state->editor.selectedObject3DId = object_id;
    state->editor.selectedObjectAssetBodyId = object_id;
    state->editor.selectedObjectAssetFace = face;
    state->editor.objectAuthoringMode = preserve_committed_sketch
        ? OBJECT_AUTHORING_MODE_SKETCH_SELECT
        : OBJECT_AUTHORING_MODE_FACE_SELECT;
    if (preserve_committed_sketch) {
        const ObjectFaceSketchHandleKind handle =
            state->editor.selectedObjectFaceSketchHandle != OBJECT_FACE_SKETCH_HANDLE_NONE
                ? (ObjectFaceSketchHandleKind)state->editor.selectedObjectFaceSketchHandle
                : OBJECT_FACE_SKETCH_HANDLE_BODY;
        (void)Editor_ObjectFaceSketchSelect(&state->editor, handle);
    }
    UIPanel_FocusObjectAuthoringTab(UIPanel_Get());
    state->layout.scene3d.constructionPlane.mode = CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME;
    state->layout.scene3d.constructionPlane.customFrame = frame;
    state->activePlane =
        Layout_ConstructionPlane3D_ToViewPlane(&state->layout.scene3d.constructionPlane);

    face_forward = Vec3_Scale(Vec3_Normalize(frame.normal), -1.0f);
    state->freeViewCamera.target = frame.origin;
    ObjectWorkspaceView_SetCameraForward(&state->freeViewCamera, face_forward);

    view_ctx = SpaceAdapter_BuildViewContext(state);
    ObjectWorkspaceView_CenterGridOnProjectedPoint(state,
                                                   SpaceAdapter_ProjectToView(frame.origin,
                                                                              &view_ctx));
    Global_FlagGridChanged();
    Global_FlagHitboxesDirty();
    return true;
}
