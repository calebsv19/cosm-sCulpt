#include "Input/input_mouse_drag.h"

#include "Input/input_mouse_drag_shared.h"

#include "Core/space_mode_adapter.h"

#include "Layout/Grid/grid.h"

#include "Math/math_util.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <string.h>

bool draggingPan = false;
bool draggingHandle = false;
bool draggingAnchor = false;
bool draggingGizmo = false;
bool draggingObjectResize = false;
bool draggingObjectGizmo = false;
bool draggingObjectTranslate = false;
bool draggingObjectRotate = false;
bool draggingSelectionBox = false;
int draggingAnchorIndex = -1;
bool anchorDragCaptured = false;
bool dragPrecise = false;
int lastMx = 0;
int lastMy = 0;

ObjectResizeDragState objectResizeDrag = {
    .active = false,
    .objectId = 0u,
    .handle = PLANE_RESIZE_HANDLE_NONE,
    .historyCaptured = false
};

ObjectGizmoDragState objectGizmoDrag = {
    .active = false,
    .objectId = 0u,
    .handle = RECT_PRISM_RESIZE_HANDLE_NONE,
    .axisDirection = RECT_PRISM_AXIS_DIR_POS_U,
    .mouseStartScreen = { 0.0f, 0.0f },
    .handleStartWorld = { 0.0f, 0.0f, 0.0f },
    .worldUnitsPerPixel = 0.0f,
    .historyCaptured = false
};

ObjectTranslateDragState objectTranslateDrag = {
    .active = false,
    .objectId = 0u,
    .axis = GIZMO_AXIS_DIR_POS_X,
    .mouseStartScreen = { 0.0f, 0.0f },
    .centerStartWorld = { 0.0f, 0.0f, 0.0f },
    .worldUnitsPerPixel = 0.0f,
    .smooth = false,
    .historyCaptured = false
};

ObjectRotateDragState objectRotateDrag = {
    .active = false,
    .objectId = 0u,
    .axis = GIZMO_AXIS_DIR_POS_X,
    .mouseStartScreen = { 0.0f, 0.0f },
    .centerStartWorld = { 0.0f, 0.0f, 0.0f },
    .degreesPerPixel = 0.0f,
    .smooth = false,
    .historyCaptured = false,
    .baselineObject = {0}
};

void ResetObjectResizeDrag(EditorState* editor) {
    objectResizeDrag.active = false;
    objectResizeDrag.objectId = 0u;
    objectResizeDrag.handle = PLANE_RESIZE_HANDLE_NONE;
    objectResizeDrag.historyCaptured = false;
    if (!editor) return;
    editor->isResizingObject3D = false;
}

void ResetObjectGizmoDrag(EditorState* editor) {
    objectGizmoDrag.active = false;
    objectGizmoDrag.objectId = 0u;
    objectGizmoDrag.handle = RECT_PRISM_RESIZE_HANDLE_NONE;
    objectGizmoDrag.axisDirection = RECT_PRISM_AXIS_DIR_POS_U;
    objectGizmoDrag.mouseStartScreen = (Vec2){ 0.0f, 0.0f };
    objectGizmoDrag.handleStartWorld = (Vec3){ 0.0f, 0.0f, 0.0f };
    objectGizmoDrag.worldUnitsPerPixel = 0.0f;
    objectGizmoDrag.historyCaptured = false;
    if (!editor) return;
    editor->activeObject3DGizmoAxis = -1;
}

void ResetObjectTranslateDrag(EditorState* editor) {
    objectTranslateDrag.active = false;
    objectTranslateDrag.objectId = 0u;
    objectTranslateDrag.axis = GIZMO_AXIS_DIR_POS_X;
    objectTranslateDrag.mouseStartScreen = (Vec2){ 0.0f, 0.0f };
    objectTranslateDrag.centerStartWorld = (Vec3){ 0.0f, 0.0f, 0.0f };
    objectTranslateDrag.worldUnitsPerPixel = 0.0f;
    objectTranslateDrag.smooth = false;
    objectTranslateDrag.historyCaptured = false;
    if (!editor) return;
    editor->activeObject3DGizmoAxis = -1;
}

void ResetObjectRotateDrag(EditorState* editor) {
    objectRotateDrag.active = false;
    objectRotateDrag.objectId = 0u;
    objectRotateDrag.axis = GIZMO_AXIS_DIR_POS_X;
    objectRotateDrag.mouseStartScreen = (Vec2){ 0.0f, 0.0f };
    objectRotateDrag.centerStartWorld = (Vec3){ 0.0f, 0.0f, 0.0f };
    objectRotateDrag.degreesPerPixel = 0.0f;
    objectRotateDrag.smooth = false;
    objectRotateDrag.historyCaptured = false;
    memset(&objectRotateDrag.baselineObject, 0, sizeof(objectRotateDrag.baselineObject));
    if (!editor) return;
    editor->activeObject3DGizmoAxis = -1;
    editor->isRotatingObject3D = false;
}

static bool RectPrismAxisAllowedForHandle(RectPrismResizeHandleKind handle,
                                          RectPrismAxisDirection axisDirection) {
    RectPrismHandleAxisMask mask = {0};
    if (!Layout_RectPrismResizeHandleAxisMask(handle, &mask)) return false;
    switch (Layout_RectPrismAxisDirection_Family(axisDirection)) {
        case 0: return mask.allowU;
        case 1: return mask.allowV;
        case 2: return mask.allowN;
        default: return false;
    }
}

static bool ScreenToPlaneFrameWorld(int screenX,
                                    int screenY,
                                    const Grid* grid,
                                    const SpaceViewContext* viewCtx,
                                    const PlaneFrame3* frame,
                                    bool snapToGrid,
                                    Vec3* outWorld) {
    if (!grid || !viewCtx || !frame || !outWorld) return false;

    Vec2 viewPos = snapToGrid
        ? ScreenToSnappedWorld(screenX, screenY, grid)
        : ScreenToWorld(screenX, screenY, grid);

    Ray3 ray = Ray3_FromPlaneViewPoint(viewPos, viewCtx->plane.axis);
    if (viewCtx->camera.enabled) {
        Vec3 right = FreeView_Right(&viewCtx->camera);
        Vec3 up = FreeView_Up(&viewCtx->camera);
        Vec3 forward = FreeView_Forward(&viewCtx->camera);
        ray.origin = Vec3_Add(viewCtx->camera.target,
                              Vec3_Add(Vec3_Scale(right, viewPos.x),
                                       Vec3_Scale(up, viewPos.y)));
        ray.direction = forward;
    }

    Plane3 plane = Plane3_FromPointNormal(frame->origin, frame->normal);
    return Ray3_IntersectPlane(ray, plane, NULL, outWorld);
}

bool BeginGizmoDragSession(GlobalState* state,
                           EditorState* editor,
                           int anchorIndex,
                           GizmoAxisDirection axis,
                           int mouseX,
                           int mouseY) {
    if (!state || !editor) return false;
    if (!GizmoAxisDirection_IsValid(axis)) return false;
    if (anchorIndex < 0 || (size_t)anchorIndex >= state->layout.anchorCount) return false;

    const Anchor* anchor = &state->layout.anchors[anchorIndex];
    if (anchor->isDeleted) return false;

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (state->spaceMode != SPACE_MODE_3D || !SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return false;

    const float axisWorldLen = fmaxf(state->grid.gridSize, 1e-4f);
    Vec3 axisWorldVec = GizmoAxisDirection_WorldVector(axis);
    Vec3 tipWorld = Vec3_Add(anchor->pos, Vec3_Scale(axisWorldVec, axisWorldLen));

    Vec2 anchorScreen = WorldToScreen(SpaceAdapter_ProjectToView(anchor->pos, &viewCtx), &state->grid);
    Vec2 tipScreen = WorldToScreen(SpaceAdapter_ProjectToView(tipWorld, &viewCtx), &state->grid);
    float axisPixels = Vec2_Distance(anchorScreen, tipScreen);
    if (axisPixels <= 1e-4f) return false;

    editor->gizmoDrag.active = true;
    editor->gizmoDrag.axis = axis;
    editor->gizmoDrag.anchorIndex = anchorIndex;
    editor->gizmoDrag.mouseStartScreen = (Vec2){ (float)mouseX, (float)mouseY };
    editor->gizmoDrag.primaryStartWorld = anchor->pos;
    editor->gizmoDrag.worldUnitsPerPixel = axisWorldLen / axisPixels;
    editor->gizmoDrag.smooth = (SDL_GetModState() & KMOD_SHIFT) != 0;
    editor->hoveredGizmoAxis = axis;
    editor->isDraggingAnchor = true;
    editor->isPreciseDrag = editor->gizmoDrag.smooth;

    Editor_BeginAnchorDrag(editor, &state->layout);
    if (editor->dragSnapshotCount == 0) {
        Editor_ResetGizmoDrag(editor);
        editor->isDraggingAnchor = false;
        editor->isPreciseDrag = false;
        return false;
    }

    Editor_HistoryCapture(editor, &state->layout);
    return true;
}

bool BeginObjectResizeDragSession(GlobalState* state,
                                  EditorState* editor,
                                  uint32_t objectId,
                                  PlaneResizeHandleKind handle) {
    if (!state || !editor) return false;
    if (objectId == 0u || handle == PLANE_RESIZE_HANDLE_NONE) return false;
    Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, objectId);
    if (!object) return false;
    if (object->kind != OBJECT3D_KIND_PLANE &&
        object->kind != OBJECT3D_KIND_RECT_PRISM) {
        return false;
    }
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    objectResizeDrag.active = true;
    objectResizeDrag.objectId = objectId;
    objectResizeDrag.handle = handle;
    objectResizeDrag.historyCaptured = false;
    editor->isResizingObject3D = true;
    editor->isRotatingObject3D = false;
    editor->selectedObject3DResizeHandle = (int)handle;
    editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->isPreciseDrag = (SDL_GetModState() & KMOD_ALT) != 0;
    return true;
}

bool BeginObjectGizmoDragSession(GlobalState* state,
                                 EditorState* editor,
                                 uint32_t objectId,
                                 RectPrismResizeHandleKind handle,
                                 RectPrismAxisDirection axisDirection,
                                 int mouseX,
                                 int mouseY) {
    if (!state || !editor) return false;
    if (objectId == 0u || handle == RECT_PRISM_RESIZE_HANDLE_NONE) return false;
    if (!Layout_RectPrismAxisDirection_IsValid(axisDirection)) return false;
    if (!RectPrismAxisAllowedForHandle(handle, axisDirection)) return false;

    Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, objectId);
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (state->spaceMode != SPACE_MODE_3D || !SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return false;

    Vec3 handleWorld = {0};
    if (!Layout_RectPrismResizeHandleWorldPoint(object, handle, &handleWorld)) return false;
    Vec3 axisWorldVec = Layout_RectPrismAxisDirection_WorldVector(object, axisDirection);
    if (Vec3_Length(axisWorldVec) <= 1e-5f) return false;
    const float axisWorldLen = fmaxf(state->grid.gridSize * 2.0f, 1.0f);
    Vec3 tipWorld = Vec3_Add(handleWorld, Vec3_Scale(axisWorldVec, axisWorldLen));

    Vec2 startScreen = WorldToScreen(SpaceAdapter_ProjectToView(handleWorld, &viewCtx), &state->grid);
    Vec2 tipScreen = WorldToScreen(SpaceAdapter_ProjectToView(tipWorld, &viewCtx), &state->grid);
    float axisPixels = Vec2_Distance(startScreen, tipScreen);
    if (axisPixels <= 1e-4f) return false;

    objectGizmoDrag.active = true;
    objectGizmoDrag.objectId = objectId;
    objectGizmoDrag.handle = handle;
    objectGizmoDrag.axisDirection = axisDirection;
    objectGizmoDrag.mouseStartScreen = (Vec2){ (float)mouseX, (float)mouseY };
    objectGizmoDrag.handleStartWorld = handleWorld;
    objectGizmoDrag.worldUnitsPerPixel = axisWorldLen / axisPixels;
    objectGizmoDrag.historyCaptured = false;

    editor->isResizingObject3D = true;
    editor->isRotatingObject3D = false;
    editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->selectedObject3DPrismHandle = (int)handle;
    editor->activeObject3DGizmoAxis = (int)axisDirection;
    editor->isPreciseDrag = (SDL_GetModState() & KMOD_ALT) != 0;
    return true;
}

bool BeginObjectTranslateDragSession(GlobalState* state,
                                     EditorState* editor,
                                     uint32_t objectId,
                                     GizmoAxisDirection axis,
                                     int mouseX,
                                     int mouseY) {
    if (!state || !editor) return false;
    if (objectId == 0u || !GizmoAxisDirection_IsValid(axis)) return false;

    Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, objectId);
    if (!object) return false;
    if (object->kind != OBJECT3D_KIND_PLANE &&
        object->kind != OBJECT3D_KIND_RECT_PRISM) {
        return false;
    }
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (state->spaceMode != SPACE_MODE_3D || !SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return false;

    const float axisWorldLen = fmaxf(state->grid.gridSize * 2.0f, 1.0f);
    Vec3 axisWorldVec = GizmoAxisDirection_WorldVector(axis);
    Vec3 tipWorld = Vec3_Add(object->transform.position, Vec3_Scale(axisWorldVec, axisWorldLen));

    Vec2 startScreen = WorldToScreen(SpaceAdapter_ProjectToView(object->transform.position, &viewCtx),
                                     &state->grid);
    Vec2 tipScreen = WorldToScreen(SpaceAdapter_ProjectToView(tipWorld, &viewCtx), &state->grid);
    float axisPixels = Vec2_Distance(startScreen, tipScreen);
    if (axisPixels <= 1e-4f) return false;

    objectTranslateDrag.active = true;
    objectTranslateDrag.objectId = objectId;
    objectTranslateDrag.axis = axis;
    objectTranslateDrag.mouseStartScreen = (Vec2){ (float)mouseX, (float)mouseY };
    objectTranslateDrag.centerStartWorld = object->transform.position;
    objectTranslateDrag.worldUnitsPerPixel = axisWorldLen / axisPixels;
    objectTranslateDrag.smooth = (SDL_GetModState() & KMOD_SHIFT) != 0;
    objectTranslateDrag.historyCaptured = false;

    editor->isResizingObject3D = false;
    editor->isRotatingObject3D = false;
    editor->activeObject3DGizmoAxis = (int)axis;
    editor->isPreciseDrag = objectTranslateDrag.smooth;
    return true;
}

bool BeginObjectRotateDragSession(GlobalState* state,
                                  EditorState* editor,
                                  uint32_t objectId,
                                  GizmoAxisDirection axis,
                                  int mouseX,
                                  int mouseY) {
    if (!state || !editor) return false;
    if (objectId == 0u || !GizmoAxisDirection_IsValid(axis)) return false;

    Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, objectId);
    if (!object) return false;
    if (object->kind != OBJECT3D_KIND_PLANE &&
        object->kind != OBJECT3D_KIND_RECT_PRISM) {
        return false;
    }
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (state->spaceMode != SPACE_MODE_3D || !SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return false;

    const float axisWorldLen = fmaxf(state->grid.gridSize * 2.0f, 1.0f);
    Vec3 axisWorldVec = GizmoAxisDirection_WorldVector(axis);
    Vec3 tipWorld = Vec3_Add(object->transform.position, Vec3_Scale(axisWorldVec, axisWorldLen));

    Vec2 startScreen = WorldToScreen(SpaceAdapter_ProjectToView(object->transform.position, &viewCtx),
                                     &state->grid);
    Vec2 tipScreen = WorldToScreen(SpaceAdapter_ProjectToView(tipWorld, &viewCtx), &state->grid);
    float axisPixels = Vec2_Distance(startScreen, tipScreen);
    if (axisPixels <= 1e-4f) return false;

    objectRotateDrag.active = true;
    objectRotateDrag.objectId = objectId;
    objectRotateDrag.axis = axis;
    objectRotateDrag.mouseStartScreen = (Vec2){ (float)mouseX, (float)mouseY };
    objectRotateDrag.centerStartWorld = object->transform.position;
    objectRotateDrag.degreesPerPixel = 180.0f / axisPixels;
    objectRotateDrag.smooth = (SDL_GetModState() & KMOD_SHIFT) != 0;
    objectRotateDrag.historyCaptured = false;
    objectRotateDrag.baselineObject = *object;

    editor->isResizingObject3D = false;
    editor->isRotatingObject3D = true;
    editor->activeObject3DGizmoAxis = (int)axis;
    editor->isPreciseDrag = objectRotateDrag.smooth;
    return true;
}

static void UpdateAnchorDragPosition(int mx, int my) {
    if (!draggingAnchor || draggingAnchorIndex < 0) return;

    GlobalState* state = Global_Get();
    if (!state) return;

    if ((size_t)draggingAnchorIndex >= state->layout.anchorCount) return;

    if (!anchorDragCaptured) {
        Editor_HistoryCapture(&state->editor, &state->layout);
        anchorDragCaptured = true;
    }

    bool precise = (SDL_GetModState() & KMOD_ALT) != 0;
    dragPrecise = precise;
    state->editor.isPreciseDrag = precise;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    Vec3 primaryPos = {0};
    if (!SpaceAdapter_ScreenToWorld(mx, my, &state->grid, &viewCtx, !precise, &primaryPos)) return;

    Editor_UpdateAnchorDrag(&state->editor, &state->layout, primaryPos);
}

static void UpdateHandleDragPosition(int mx, int my) {
    GlobalState* state = Global_Get();
    EditorState* editor = &state->editor;
    if (editor->selectedHandleAnchor < 0 || editor->selectedHandleComponent < 0) return;
    if ((size_t)editor->selectedHandleAnchor >= state->layout.anchorCount) return;

    Anchor* anchor = &state->layout.anchors[editor->selectedHandleAnchor];
    if (anchor->isDeleted || anchor->type != ANCHOR_TYPE_CURVE) return;

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    Vec3 world3 = {0};
    if (!SpaceAdapter_ScreenToWorld(mx, my, &state->grid, &viewCtx, false, &world3)) return;
    Vec3 deltaWorld = Vec3_Sub(world3, anchor->pos);
    float length = 0.0f;
    float angle = 0.0f;
    anchor->handleAxis = SpaceAdapter_ActivePlaneAxis(&viewCtx);
    Vec3_HandlePolarFromWorldDelta(deltaWorld, anchor->handleAxis, &length, &angle);

    if (editor->selectedHandleComponent == 0) {
        anchor->handleInLength = length;
        anchor->handleInAngleDeg = angle;
        if (anchor->handlesLinked) {
            anchor->handleOutLength = length;
            anchor->handleOutAngleDeg = Angle_NormalizeDeg(angle + 180.0f);
        }
    } else {
        anchor->handleOutLength = length;
        anchor->handleOutAngleDeg = angle;
        if (anchor->handlesLinked) {
            anchor->handleInLength = length;
            anchor->handleInAngleDeg = Angle_NormalizeDeg(angle - 180.0f);
        }
    }

    Global_FlagLayoutChanged();
}

static void UpdateGizmoDragPosition(int mx, int my) {
    if (!draggingGizmo) return;

    GlobalState* state = Global_Get();
    if (!state) return;
    EditorState* editor = &state->editor;
    if (!editor->gizmoDrag.active) return;
    if (!GizmoAxisDirection_IsValid(editor->gizmoDrag.axis)) return;
    if (editor->gizmoDrag.anchorIndex < 0 ||
        (size_t)editor->gizmoDrag.anchorIndex >= state->layout.anchorCount) {
        return;
    }

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (state->spaceMode != SPACE_MODE_3D || !SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return;

    const float axisWorldLen = fmaxf(state->grid.gridSize, 1e-4f);
    Vec3 axisWorldVec = GizmoAxisDirection_WorldVector(editor->gizmoDrag.axis);
    Vec3 tipWorld = Vec3_Add(editor->gizmoDrag.primaryStartWorld, Vec3_Scale(axisWorldVec, axisWorldLen));

    Vec2 startScreen = WorldToScreen(SpaceAdapter_ProjectToView(editor->gizmoDrag.primaryStartWorld, &viewCtx),
                                     &state->grid);
    Vec2 tipScreen = WorldToScreen(SpaceAdapter_ProjectToView(tipWorld, &viewCtx), &state->grid);
    Vec2 axisScreenVector = Vec2_Sub(tipScreen, startScreen);
    float axisPixels = Vec2_Distance(startScreen, tipScreen);
    if (axisPixels > 1e-4f) {
        editor->gizmoDrag.worldUnitsPerPixel = axisWorldLen / axisPixels;
    }

    Vec2 mouseNow = { (float)mx, (float)my };
    float signedPixels = GizmoDrag_SignedPixelsAlongAxis(editor->gizmoDrag.mouseStartScreen,
                                                         mouseNow,
                                                         axisScreenVector);
    editor->gizmoDrag.smooth = (SDL_GetModState() & KMOD_SHIFT) != 0;
    editor->isPreciseDrag = editor->gizmoDrag.smooth;
    const float step = fmaxf(state->grid.gridSize, 1e-4f);
    float signedWorldDistance = GizmoDrag_ResolveDistance(signedPixels,
                                                          editor->gizmoDrag.worldUnitsPerPixel,
                                                          step,
                                                          editor->gizmoDrag.smooth);
    Vec3 primaryNewPos = GizmoDrag_ApplyAxisDistance(editor->gizmoDrag.primaryStartWorld,
                                                     editor->gizmoDrag.axis,
                                                     signedWorldDistance);
    Editor_UpdateAnchorDrag(editor, &state->layout, primaryNewPos);
}

static void UpdateObjectResizeDragPosition(int mx, int my) {
    if (!draggingObjectResize || !objectResizeDrag.active) return;

    GlobalState* state = Global_Get();
    if (!state) return;
    EditorState* editor = &state->editor;
    if (objectResizeDrag.objectId == 0u ||
        objectResizeDrag.handle == PLANE_RESIZE_HANDLE_NONE) {
        return;
    }

    Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, objectResizeDrag.objectId);
    if (!object) return;
    if (object->kind != OBJECT3D_KIND_PLANE &&
        object->kind != OBJECT3D_KIND_RECT_PRISM) {
        return;
    }

    if (!objectResizeDrag.historyCaptured) {
        Editor_HistoryCapture(editor, &state->layout);
        objectResizeDrag.historyCaptured = true;
    }

    const bool precise = (SDL_GetModState() & KMOD_ALT) != 0;
    editor->isPreciseDrag = precise;
    editor->isResizingObject3D = true;
    editor->isRotatingObject3D = false;
    editor->selectedObject3DResizeHandle = (int)objectResizeDrag.handle;
    editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;

    const PlaneFrame3* frame = (object->kind == OBJECT3D_KIND_PLANE)
        ? &object->plane.frame
        : &object->rectPrism.frame;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    Vec3 worldPoint = {0};
    if (!ScreenToPlaneFrameWorld(mx,
                                 my,
                                 &state->grid,
                                 &viewCtx,
                                 frame,
                                 !precise,
                                 &worldPoint)) {
        return;
    }

    PlaneResizeHandleKind resolvedHandle = objectResizeDrag.handle;
    if (object->kind == OBJECT3D_KIND_PLANE) {
        resolvedHandle =
            Layout_ResolvePlaneResizeHandleForDrag(object, objectResizeDrag.handle, worldPoint);
    } else {
        resolvedHandle =
            Layout_ResolveRectPrismResizeHandleForDrag(object, objectResizeDrag.handle, worldPoint);
    }
    if (resolvedHandle != objectResizeDrag.handle &&
        resolvedHandle != PLANE_RESIZE_HANDLE_NONE) {
        objectResizeDrag.handle = resolvedHandle;
        editor->selectedObject3DResizeHandle = (int)resolvedHandle;
    }

    if (object->kind == OBJECT3D_KIND_PLANE) {
        (void)Layout_ResizePlanePrimitiveFromHandle(&state->layout,
                                                    objectResizeDrag.objectId,
                                                    objectResizeDrag.handle,
                                                    worldPoint,
                                                    NULL);
    } else {
        (void)Layout_ResizeRectPrismPrimitiveFromHandle(&state->layout,
                                                        objectResizeDrag.objectId,
                                                        objectResizeDrag.handle,
                                                        worldPoint,
                                                        NULL);
    }
}

static void UpdateObjectGizmoDragPosition(int mx, int my) {
    if (!draggingObjectGizmo || !objectGizmoDrag.active) return;

    GlobalState* state = Global_Get();
    if (!state) return;
    EditorState* editor = &state->editor;
    if (objectGizmoDrag.objectId == 0u ||
        objectGizmoDrag.handle == RECT_PRISM_RESIZE_HANDLE_NONE ||
        !Layout_RectPrismAxisDirection_IsValid(objectGizmoDrag.axisDirection)) {
        return;
    }

    Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, objectGizmoDrag.objectId);
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM) return;
    if (!Layout_ObjectStore_ValidateObject(object)) return;

    if (!objectGizmoDrag.historyCaptured) {
        Editor_HistoryCapture(editor, &state->layout);
        objectGizmoDrag.historyCaptured = true;
    }

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (state->spaceMode != SPACE_MODE_3D || !SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return;

    Vec3 axisWorldVec = Layout_RectPrismAxisDirection_WorldVector(object, objectGizmoDrag.axisDirection);
    if (Vec3_Length(axisWorldVec) <= 1e-5f) return;

    const float axisWorldLen = fmaxf(state->grid.gridSize * 2.0f, 1.0f);
    Vec3 tipWorld = Vec3_Add(objectGizmoDrag.handleStartWorld, Vec3_Scale(axisWorldVec, axisWorldLen));
    Vec2 startScreen = WorldToScreen(SpaceAdapter_ProjectToView(objectGizmoDrag.handleStartWorld, &viewCtx),
                                     &state->grid);
    Vec2 tipScreen = WorldToScreen(SpaceAdapter_ProjectToView(tipWorld, &viewCtx), &state->grid);
    Vec2 axisScreenVector = Vec2_Sub(tipScreen, startScreen);
    float axisPixels = Vec2_Distance(startScreen, tipScreen);
    if (axisPixels > 1e-4f) {
        objectGizmoDrag.worldUnitsPerPixel = axisWorldLen / axisPixels;
    }

    const bool precise = (SDL_GetModState() & KMOD_ALT) != 0;
    editor->isPreciseDrag = precise;
    editor->isResizingObject3D = true;
    editor->isRotatingObject3D = false;
    editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->selectedObject3DPrismHandle = (int)objectGizmoDrag.handle;
    editor->activeObject3DGizmoAxis = (int)objectGizmoDrag.axisDirection;

    Vec2 mouseNow = { (float)mx, (float)my };
    float signedPixels = GizmoDrag_SignedPixelsAlongAxis(objectGizmoDrag.mouseStartScreen,
                                                         mouseNow,
                                                         axisScreenVector);
    const float step = fmaxf(state->grid.gridSize, 1e-4f);
    float signedWorldDistance = GizmoDrag_ResolveDistance(signedPixels,
                                                          objectGizmoDrag.worldUnitsPerPixel,
                                                          step,
                                                          precise);
    Vec3 dragPoint = Vec3_Add(objectGizmoDrag.handleStartWorld,
                              Vec3_Scale(axisWorldVec, signedWorldDistance));

    RectPrismResizeHandleKind resolvedHandle =
        Layout_ResolveRectPrismResizeHandleFor3DDrag(object, objectGizmoDrag.handle, dragPoint);
    if (resolvedHandle != objectGizmoDrag.handle &&
        resolvedHandle != RECT_PRISM_RESIZE_HANDLE_NONE) {
        objectGizmoDrag.handle = resolvedHandle;
        editor->selectedObject3DPrismHandle = (int)resolvedHandle;
    }
    (void)Layout_ResizeRectPrismFrom3DHandle(&state->layout,
                                             objectGizmoDrag.objectId,
                                             objectGizmoDrag.handle,
                                             dragPoint,
                                             NULL);
}

static void UpdateObjectTranslateDragPosition(int mx, int my) {
    if (!draggingObjectTranslate || !objectTranslateDrag.active) return;

    GlobalState* state = Global_Get();
    if (!state) return;
    EditorState* editor = &state->editor;
    if (objectTranslateDrag.objectId == 0u ||
        !GizmoAxisDirection_IsValid(objectTranslateDrag.axis)) {
        return;
    }

    Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, objectTranslateDrag.objectId);
    if (!object) return;
    if (object->kind != OBJECT3D_KIND_PLANE &&
        object->kind != OBJECT3D_KIND_RECT_PRISM) {
        return;
    }
    if (!Layout_ObjectStore_ValidateObject(object)) return;

    if (!objectTranslateDrag.historyCaptured) {
        Editor_HistoryCapture(editor, &state->layout);
        objectTranslateDrag.historyCaptured = true;
    }

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (state->spaceMode != SPACE_MODE_3D || !SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return;

    const float axisWorldLen = fmaxf(state->grid.gridSize * 2.0f, 1.0f);
    Vec3 axisWorldVec = GizmoAxisDirection_WorldVector(objectTranslateDrag.axis);
    Vec3 tipWorld = Vec3_Add(objectTranslateDrag.centerStartWorld, Vec3_Scale(axisWorldVec, axisWorldLen));
    Vec2 startScreen = WorldToScreen(SpaceAdapter_ProjectToView(objectTranslateDrag.centerStartWorld, &viewCtx),
                                     &state->grid);
    Vec2 tipScreen = WorldToScreen(SpaceAdapter_ProjectToView(tipWorld, &viewCtx), &state->grid);
    Vec2 axisScreenVector = Vec2_Sub(tipScreen, startScreen);
    float axisPixels = Vec2_Distance(startScreen, tipScreen);
    if (axisPixels > 1e-4f) {
        objectTranslateDrag.worldUnitsPerPixel = axisWorldLen / axisPixels;
    }

    objectTranslateDrag.smooth = (SDL_GetModState() & KMOD_SHIFT) != 0;
    editor->isPreciseDrag = objectTranslateDrag.smooth;
    editor->isResizingObject3D = false;
    editor->isRotatingObject3D = false;
    editor->activeObject3DGizmoAxis = (int)objectTranslateDrag.axis;

    Vec2 mouseNow = { (float)mx, (float)my };
    float signedPixels = GizmoDrag_SignedPixelsAlongAxis(objectTranslateDrag.mouseStartScreen,
                                                         mouseNow,
                                                         axisScreenVector);
    const float step = fmaxf(state->grid.gridSize, 1e-4f);
    float signedWorldDistance = GizmoDrag_ResolveDistance(signedPixels,
                                                          objectTranslateDrag.worldUnitsPerPixel,
                                                          step,
                                                          objectTranslateDrag.smooth);
    Vec3 nextCenter = GizmoDrag_ApplyAxisDistance(objectTranslateDrag.centerStartWorld,
                                                  objectTranslateDrag.axis,
                                                  signedWorldDistance);

    (void)Layout_SetObject3DPosition(&state->layout,
                                     objectTranslateDrag.objectId,
                                     nextCenter,
                                     NULL);
}

static void UpdateObjectRotateDragPosition(int mx, int my) {
    if (!draggingObjectRotate || !objectRotateDrag.active) return;

    GlobalState* state = Global_Get();
    if (!state) return;
    EditorState* editor = &state->editor;
    if (objectRotateDrag.objectId == 0u ||
        !GizmoAxisDirection_IsValid(objectRotateDrag.axis)) {
        return;
    }

    Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, objectRotateDrag.objectId);
    if (!object) return;
    if (object->kind != OBJECT3D_KIND_PLANE &&
        object->kind != OBJECT3D_KIND_RECT_PRISM) {
        return;
    }
    if (!Layout_ObjectStore_ValidateObject(object)) return;

    if (!objectRotateDrag.historyCaptured) {
        Editor_HistoryCapture(editor, &state->layout);
        objectRotateDrag.historyCaptured = true;
    }

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (state->spaceMode != SPACE_MODE_3D || !SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return;

    const float axisWorldLen = fmaxf(state->grid.gridSize * 2.0f, 1.0f);
    Vec3 axisWorldVec = GizmoAxisDirection_WorldVector(objectRotateDrag.axis);
    Vec3 tipWorld = Vec3_Add(objectRotateDrag.centerStartWorld, Vec3_Scale(axisWorldVec, axisWorldLen));
    Vec2 startScreen = WorldToScreen(SpaceAdapter_ProjectToView(objectRotateDrag.centerStartWorld, &viewCtx),
                                     &state->grid);
    Vec2 tipScreen = WorldToScreen(SpaceAdapter_ProjectToView(tipWorld, &viewCtx), &state->grid);
    Vec2 axisScreenVector = Vec2_Sub(tipScreen, startScreen);
    float axisPixels = Vec2_Distance(startScreen, tipScreen);
    if (axisPixels > 1e-4f) {
        objectRotateDrag.degreesPerPixel = 180.0f / axisPixels;
    }

    objectRotateDrag.smooth = (SDL_GetModState() & KMOD_SHIFT) != 0;
    editor->isPreciseDrag = objectRotateDrag.smooth;
    editor->isResizingObject3D = false;
    editor->isRotatingObject3D = true;
    editor->activeObject3DGizmoAxis = (int)objectRotateDrag.axis;

    Vec2 mouseNow = { (float)mx, (float)my };
    float signedPixels = GizmoDrag_SignedPixelsAlongAxis(objectRotateDrag.mouseStartScreen,
                                                         mouseNow,
                                                         axisScreenVector);
    float angleDeg = signedPixels * objectRotateDrag.degreesPerPixel;
    if (!objectRotateDrag.smooth) {
        const float snapDeg = 15.0f;
        angleDeg = roundf(angleDeg / snapDeg) * snapDeg;
    }

    (void)Layout_RotateObject3D(&state->layout,
                                objectRotateDrag.objectId,
                                axisWorldVec,
                                angleDeg,
                                &objectRotateDrag.baselineObject,
                                NULL);
}

void HandleMouseDrag(const SDL_MouseMotionEvent* motion) {
    if (draggingObjectRotate) {
        UpdateObjectRotateDragPosition(motion->x, motion->y);
        return;
    }
    if (draggingObjectTranslate) {
        UpdateObjectTranslateDragPosition(motion->x, motion->y);
        return;
    }
    if (draggingObjectGizmo) {
        UpdateObjectGizmoDragPosition(motion->x, motion->y);
        return;
    }
    if (draggingObjectResize) {
        UpdateObjectResizeDragPosition(motion->x, motion->y);
        return;
    }
    if (draggingGizmo) {
        UpdateGizmoDragPosition(motion->x, motion->y);
        return;
    }
    if (draggingAnchor) {
        UpdateAnchorDragPosition(motion->x, motion->y);
        return;
    }
    if (draggingHandle) {
        UpdateHandleDragPosition(motion->x, motion->y);
        return;
    }
    if (draggingSelectionBox) {
        GlobalState* state = Global_Get();
        EditorState* editor = &state->editor;
        editor->selectionBoxEnd = ScreenToWorld(motion->x, motion->y, &state->grid);
        return;
    }
    if (!draggingPan) return;

    int dx = motion->x - lastMx;
    int dy = motion->y - lastMy;

    GlobalState* state = Global_Get();
    Grid_pan(&state->grid, -dx, -dy);
    Global_FlagGridChanged();

    lastMx = motion->x;
    lastMy = motion->y;
}
