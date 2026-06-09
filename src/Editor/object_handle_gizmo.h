#pragma once

#include "Layout/layout.h"
#include "ObjectAuthoring/object_authoring_document.h"

typedef enum {
    OBJECT_HANDLE_GIZMO_TARGET_NONE = 0,
    OBJECT_HANDLE_GIZMO_TARGET_PLANE_RESIZE = 1,
    OBJECT_HANDLE_GIZMO_TARGET_PRISM_RESIZE = 2,
    OBJECT_HANDLE_GIZMO_TARGET_TOPOLOGY_VERTEX = 3,
    OBJECT_HANDLE_GIZMO_TARGET_TOPOLOGY_EDGE = 4
} ObjectHandleGizmoTargetKind;

typedef struct {
    ObjectHandleGizmoTargetKind kind;
    uint32_t objectId;
    PlaneResizeHandleKind planeHandle;
    RectPrismResizeHandleKind prismHandle;
    ObjectAuthoringVertexRef vertexRef;
    ObjectAuthoringEdgeRef edgeRef;
    Vec3 topologyWorldPoint;
} ObjectHandleGizmoTarget;

ObjectHandleGizmoTarget ObjectHandleGizmoTarget_None(void);
bool ObjectHandleGizmoTarget_FromPlane(uint32_t objectId,
                                       PlaneResizeHandleKind handle,
                                       ObjectHandleGizmoTarget* outTarget);
bool ObjectHandleGizmoTarget_FromPrism(uint32_t objectId,
                                       RectPrismResizeHandleKind handle,
                                       ObjectHandleGizmoTarget* outTarget);
bool ObjectHandleGizmoTarget_FromSelection(const Object3D* object,
                                           uint32_t objectId,
                                           PlaneResizeHandleKind selectedPlaneHandle,
                                           RectPrismResizeHandleKind selectedPrismHandle,
                                           ObjectHandleGizmoTarget* outTarget);
bool ObjectHandleGizmoTarget_FromAuthoringSelection(const Object3D* object,
                                                    const ObjectAuthoringDocument* doc,
                                                    ObjectHandleGizmoTarget* outTarget);
bool ObjectHandleGizmoTarget_IsActive(const ObjectHandleGizmoTarget* target);
bool ObjectHandleGizmoTarget_IsTopology(const ObjectHandleGizmoTarget* target);
bool ObjectHandleGizmoTarget_CanMutate(const ObjectHandleGizmoTarget* target);
bool ObjectHandleGizmoTarget_ValidateForObject(const ObjectHandleGizmoTarget* target,
                                               const Object3D* object);
bool ObjectHandleGizmoTarget_AxisMask(const ObjectHandleGizmoTarget* target,
                                      RectPrismHandleAxisMask* outMask);
bool ObjectHandleGizmoTarget_AxisAllowed(const ObjectHandleGizmoTarget* target,
                                         RectPrismAxisDirection axisDirection);
Vec3 ObjectHandleGizmoTarget_AxisWorldVector(const ObjectHandleGizmoTarget* target,
                                             const Object3D* object,
                                             RectPrismAxisDirection axisDirection);
bool ObjectHandleGizmoTarget_HandleWorldPoint(const ObjectHandleGizmoTarget* target,
                                              const Object3D* object,
                                              Vec3* outPoint);
bool ObjectHandleGizmoTarget_ResolveForDrag(const Object3D* object,
                                            ObjectHandleGizmoTarget* target,
                                            Vec3 draggedWorldPoint);
bool ObjectHandleGizmoTarget_ResizeFromDrag(Layout* layout,
                                            const ObjectHandleGizmoTarget* target,
                                            Vec3 draggedWorldPoint,
                                            bool* outBoundsAdjusted);
