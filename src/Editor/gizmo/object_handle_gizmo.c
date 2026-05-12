#include "Editor/object_handle_gizmo.h"

ObjectHandleGizmoTarget ObjectHandleGizmoTarget_None(void) {
    return (ObjectHandleGizmoTarget){
        .kind = OBJECT_HANDLE_GIZMO_TARGET_NONE,
        .objectId = 0u,
        .planeHandle = PLANE_RESIZE_HANDLE_NONE,
        .prismHandle = RECT_PRISM_RESIZE_HANDLE_NONE
    };
}

bool ObjectHandleGizmoTarget_FromPlane(uint32_t objectId,
                                       PlaneResizeHandleKind handle,
                                       ObjectHandleGizmoTarget* outTarget) {
    if (!outTarget || objectId == 0u || handle == PLANE_RESIZE_HANDLE_NONE) return false;
    *outTarget = (ObjectHandleGizmoTarget){
        .kind = OBJECT_HANDLE_GIZMO_TARGET_PLANE_RESIZE,
        .objectId = objectId,
        .planeHandle = handle,
        .prismHandle = RECT_PRISM_RESIZE_HANDLE_NONE
    };
    return true;
}

bool ObjectHandleGizmoTarget_FromPrism(uint32_t objectId,
                                       RectPrismResizeHandleKind handle,
                                       ObjectHandleGizmoTarget* outTarget) {
    if (!outTarget || objectId == 0u || handle == RECT_PRISM_RESIZE_HANDLE_NONE) return false;
    *outTarget = (ObjectHandleGizmoTarget){
        .kind = OBJECT_HANDLE_GIZMO_TARGET_PRISM_RESIZE,
        .objectId = objectId,
        .planeHandle = PLANE_RESIZE_HANDLE_NONE,
        .prismHandle = handle
    };
    return true;
}

bool ObjectHandleGizmoTarget_FromSelection(const Object3D* object,
                                           uint32_t objectId,
                                           PlaneResizeHandleKind selectedPlaneHandle,
                                           RectPrismResizeHandleKind selectedPrismHandle,
                                           ObjectHandleGizmoTarget* outTarget) {
    if (!object || !outTarget) return false;
    *outTarget = ObjectHandleGizmoTarget_None();

    if (object->kind == OBJECT3D_KIND_PLANE &&
        selectedPlaneHandle != PLANE_RESIZE_HANDLE_NONE &&
        selectedPrismHandle == RECT_PRISM_RESIZE_HANDLE_NONE) {
        return ObjectHandleGizmoTarget_FromPlane(objectId, selectedPlaneHandle, outTarget);
    }

    if (object->kind == OBJECT3D_KIND_RECT_PRISM &&
        selectedPrismHandle != RECT_PRISM_RESIZE_HANDLE_NONE) {
        return ObjectHandleGizmoTarget_FromPrism(objectId, selectedPrismHandle, outTarget);
    }

    return false;
}

bool ObjectHandleGizmoTarget_IsActive(const ObjectHandleGizmoTarget* target) {
    return target && target->kind != OBJECT_HANDLE_GIZMO_TARGET_NONE && target->objectId != 0u;
}

bool ObjectHandleGizmoTarget_ValidateForObject(const ObjectHandleGizmoTarget* target,
                                               const Object3D* object) {
    if (!ObjectHandleGizmoTarget_IsActive(target) || !object) return false;
    if (object->objectId != target->objectId) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    switch (target->kind) {
        case OBJECT_HANDLE_GIZMO_TARGET_PLANE_RESIZE:
            return object->kind == OBJECT3D_KIND_PLANE &&
                   target->planeHandle != PLANE_RESIZE_HANDLE_NONE;
        case OBJECT_HANDLE_GIZMO_TARGET_PRISM_RESIZE:
            return object->kind == OBJECT3D_KIND_RECT_PRISM &&
                   target->prismHandle != RECT_PRISM_RESIZE_HANDLE_NONE;
        case OBJECT_HANDLE_GIZMO_TARGET_NONE:
        default:
            return false;
    }
}

bool ObjectHandleGizmoTarget_AxisMask(const ObjectHandleGizmoTarget* target,
                                      RectPrismHandleAxisMask* outMask) {
    if (!ObjectHandleGizmoTarget_IsActive(target) || !outMask) return false;

    switch (target->kind) {
        case OBJECT_HANDLE_GIZMO_TARGET_PLANE_RESIZE:
            return Layout_PlaneResizeHandleAxisMask(target->planeHandle, outMask);
        case OBJECT_HANDLE_GIZMO_TARGET_PRISM_RESIZE:
            return Layout_RectPrismResizeHandleAxisMask(target->prismHandle, outMask);
        case OBJECT_HANDLE_GIZMO_TARGET_NONE:
        default:
            return false;
    }
}

bool ObjectHandleGizmoTarget_AxisAllowed(const ObjectHandleGizmoTarget* target,
                                         RectPrismAxisDirection axisDirection) {
    RectPrismHandleAxisMask mask = {0};
    if (!Layout_RectPrismAxisDirection_IsValid(axisDirection)) return false;
    if (!ObjectHandleGizmoTarget_AxisMask(target, &mask)) return false;

    switch (Layout_RectPrismAxisDirection_Family(axisDirection)) {
        case 0: return mask.allowU;
        case 1: return mask.allowV;
        case 2: return mask.allowN;
        default: return false;
    }
}

Vec3 ObjectHandleGizmoTarget_AxisWorldVector(const ObjectHandleGizmoTarget* target,
                                             const Object3D* object,
                                             RectPrismAxisDirection axisDirection) {
    Vec3 zero = {0.0f, 0.0f, 0.0f};
    if (!ObjectHandleGizmoTarget_ValidateForObject(target, object)) return zero;

    switch (target->kind) {
        case OBJECT_HANDLE_GIZMO_TARGET_PLANE_RESIZE:
            return Layout_PlaneAxisDirection_WorldVector(object, axisDirection);
        case OBJECT_HANDLE_GIZMO_TARGET_PRISM_RESIZE:
            return Layout_RectPrismAxisDirection_WorldVector(object, axisDirection);
        case OBJECT_HANDLE_GIZMO_TARGET_NONE:
        default:
            return zero;
    }
}

bool ObjectHandleGizmoTarget_HandleWorldPoint(const ObjectHandleGizmoTarget* target,
                                              const Object3D* object,
                                              Vec3* outPoint) {
    if (!ObjectHandleGizmoTarget_ValidateForObject(target, object) || !outPoint) return false;

    switch (target->kind) {
        case OBJECT_HANDLE_GIZMO_TARGET_PLANE_RESIZE:
            return Layout_PlaneResizeHandleWorldPoint(object, target->planeHandle, outPoint);
        case OBJECT_HANDLE_GIZMO_TARGET_PRISM_RESIZE:
            return Layout_RectPrismResizeHandleWorldPoint(object, target->prismHandle, outPoint);
        case OBJECT_HANDLE_GIZMO_TARGET_NONE:
        default:
            return false;
    }
}

bool ObjectHandleGizmoTarget_ResolveForDrag(const Object3D* object,
                                            ObjectHandleGizmoTarget* target,
                                            Vec3 draggedWorldPoint) {
    if (!ObjectHandleGizmoTarget_ValidateForObject(target, object)) return false;

    switch (target->kind) {
        case OBJECT_HANDLE_GIZMO_TARGET_PLANE_RESIZE: {
            PlaneResizeHandleKind resolved =
                Layout_ResolvePlaneResizeHandleForDrag(object, target->planeHandle, draggedWorldPoint);
            if (resolved != PLANE_RESIZE_HANDLE_NONE) {
                target->planeHandle = resolved;
            }
            return true;
        }
        case OBJECT_HANDLE_GIZMO_TARGET_PRISM_RESIZE: {
            RectPrismResizeHandleKind resolved =
                Layout_ResolveRectPrismResizeHandleFor3DDrag(object,
                                                             target->prismHandle,
                                                             draggedWorldPoint);
            if (resolved != RECT_PRISM_RESIZE_HANDLE_NONE) {
                target->prismHandle = resolved;
            }
            return true;
        }
        case OBJECT_HANDLE_GIZMO_TARGET_NONE:
        default:
            return false;
    }
}

bool ObjectHandleGizmoTarget_ResizeFromDrag(Layout* layout,
                                            const ObjectHandleGizmoTarget* target,
                                            Vec3 draggedWorldPoint,
                                            bool* outBoundsAdjusted) {
    if (!layout || !ObjectHandleGizmoTarget_IsActive(target)) return false;

    switch (target->kind) {
        case OBJECT_HANDLE_GIZMO_TARGET_PLANE_RESIZE:
            return Layout_ResizePlanePrimitiveFromHandle(layout,
                                                         target->objectId,
                                                         target->planeHandle,
                                                         draggedWorldPoint,
                                                         outBoundsAdjusted);
        case OBJECT_HANDLE_GIZMO_TARGET_PRISM_RESIZE:
            return Layout_ResizeRectPrismFrom3DHandle(layout,
                                                      target->objectId,
                                                      target->prismHandle,
                                                      draggedWorldPoint,
                                                      outBoundsAdjusted);
        case OBJECT_HANDLE_GIZMO_TARGET_NONE:
        default:
            return false;
    }
}
