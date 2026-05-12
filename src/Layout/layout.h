// src/Layout/layout.h
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "Math/math_util.h"
#include "core_object.h"

//        Anchor node in layout graph
// ======================================
typedef enum {
    ANCHOR_TYPE_CORNER = 0,
    ANCHOR_TYPE_CURVE  = 1
} AnchorType;

typedef struct {
    Vec3 pos;               // Position in world space
    int* connectedWalls;    // Dynamic array of wall indices
    int  connectionCount;

    bool lockAngle;         // Angle lock constraint enabled
    float angleDeg;         // Locked angle (if used)
    bool isDeleted;
    bool isPersistent;

    AnchorType type;        // Corner or smooth curve
    bool handlesLinked;     // When true, in/out handle angles mirror
    ViewPlaneAxis handleAxis; // Plane basis used to interpret handle polar angles
    float handleInLength;   // Polar length for incoming handle
    float handleInAngleDeg; // Angle in degrees relative to +X
    float handleOutLength;  // Polar length for outgoing handle
    float handleOutAngleDeg;
} Anchor;


//        Wall edge between two anchors
// ======================================
typedef struct {
    int anchorA;            // Index into Layout.anchors
    int anchorB;
    bool lockLength;        // Optional constraint
    bool isDeleted;
} Wall;

typedef struct {
    bool enabled;
    bool clampOnEdit;
    Vec3 min;
    Vec3 max;
} SceneBounds3D;

typedef enum {
    SCENE_BOUNDS_HANDLE_NONE = 0,
    SCENE_BOUNDS_HANDLE_MIN_X = 1,
    SCENE_BOUNDS_HANDLE_MAX_X = 2,
    SCENE_BOUNDS_HANDLE_MIN_Y = 3,
    SCENE_BOUNDS_HANDLE_MAX_Y = 4,
    SCENE_BOUNDS_HANDLE_MIN_Z = 5,
    SCENE_BOUNDS_HANDLE_MAX_Z = 6,
    SCENE_BOUNDS_HANDLE_CORNER_MIN_X_MIN_Y_MIN_Z = 7,
    SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MIN_Y_MIN_Z = 8,
    SCENE_BOUNDS_HANDLE_CORNER_MIN_X_MAX_Y_MIN_Z = 9,
    SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MAX_Y_MIN_Z = 10,
    SCENE_BOUNDS_HANDLE_CORNER_MIN_X_MIN_Y_MAX_Z = 11,
    SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MIN_Y_MAX_Z = 12,
    SCENE_BOUNDS_HANDLE_CORNER_MIN_X_MAX_Y_MAX_Z = 13,
    SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MAX_Y_MAX_Z = 14,
    SCENE_BOUNDS_HANDLE_EDGE_X_MIN_Y_MIN_Z = 15,
    SCENE_BOUNDS_HANDLE_EDGE_X_MAX_Y_MIN_Z = 16,
    SCENE_BOUNDS_HANDLE_EDGE_X_MIN_Y_MAX_Z = 17,
    SCENE_BOUNDS_HANDLE_EDGE_X_MAX_Y_MAX_Z = 18,
    SCENE_BOUNDS_HANDLE_EDGE_Y_MIN_X_MIN_Z = 19,
    SCENE_BOUNDS_HANDLE_EDGE_Y_MAX_X_MIN_Z = 20,
    SCENE_BOUNDS_HANDLE_EDGE_Y_MIN_X_MAX_Z = 21,
    SCENE_BOUNDS_HANDLE_EDGE_Y_MAX_X_MAX_Z = 22,
    SCENE_BOUNDS_HANDLE_EDGE_Z_MIN_X_MIN_Y = 23,
    SCENE_BOUNDS_HANDLE_EDGE_Z_MAX_X_MIN_Y = 24,
    SCENE_BOUNDS_HANDLE_EDGE_Z_MIN_X_MAX_Y = 25,
    SCENE_BOUNDS_HANDLE_EDGE_Z_MAX_X_MAX_Y = 26,
    SCENE_BOUNDS_HANDLE_CENTER = 27
} SceneBoundsHandleKind;

typedef enum {
    CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED = 0,
    CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME = 1
} ConstructionPlaneMode;

typedef struct {
    ConstructionPlaneMode mode;
    ViewPlane axisAligned;
    PlaneFrame3 customFrame;
} ConstructionPlane3D;

typedef struct {
    SceneBounds3D bounds;
    ConstructionPlane3D constructionPlane;
} Scene3DSettings;

typedef struct {
    Vec3 position;
    Vec3 rotationDeg;
    Vec3 scale;
} Transform3D;

typedef enum {
    OBJECT3D_KIND_UNKNOWN = 0,
    OBJECT3D_KIND_PLANE = 1,
    OBJECT3D_KIND_RECT_PRISM = 2
} Object3DKind;

typedef struct {
    float width;
    float height;
    PlaneFrame3 frame;
    bool lockToConstructionPlane;
    bool lockToBounds;
} PlanePrimitive3D;

typedef struct {
    float width;
    float height;
    float depth;
    PlaneFrame3 frame;
    bool lockToConstructionPlane;
    bool lockToBounds;
} RectPrismPrimitive3D;

typedef enum {
    PLANE_RESIZE_HANDLE_NONE = 0,
    PLANE_RESIZE_HANDLE_CORNER_NEG_U_NEG_V = 1,
    PLANE_RESIZE_HANDLE_CORNER_POS_U_NEG_V = 2,
    PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V = 3,
    PLANE_RESIZE_HANDLE_CORNER_NEG_U_POS_V = 4,
    PLANE_RESIZE_HANDLE_EDGE_NEG_V = 5,
    PLANE_RESIZE_HANDLE_EDGE_POS_U = 6,
    PLANE_RESIZE_HANDLE_EDGE_POS_V = 7,
    PLANE_RESIZE_HANDLE_EDGE_NEG_U = 8
} PlaneResizeHandleKind;

typedef enum {
    RECT_PRISM_AXIS_DIR_POS_U = 0,
    RECT_PRISM_AXIS_DIR_NEG_U = 1,
    RECT_PRISM_AXIS_DIR_POS_V = 2,
    RECT_PRISM_AXIS_DIR_NEG_V = 3,
    RECT_PRISM_AXIS_DIR_POS_N = 4,
    RECT_PRISM_AXIS_DIR_NEG_N = 5
} RectPrismAxisDirection;

typedef enum {
    RECT_PRISM_RESIZE_HANDLE_NONE = 0,
    RECT_PRISM_RESIZE_HANDLE_CORNER_0 = 1,
    RECT_PRISM_RESIZE_HANDLE_CORNER_1 = 2,
    RECT_PRISM_RESIZE_HANDLE_CORNER_2 = 3,
    RECT_PRISM_RESIZE_HANDLE_CORNER_3 = 4,
    RECT_PRISM_RESIZE_HANDLE_CORNER_4 = 5,
    RECT_PRISM_RESIZE_HANDLE_CORNER_5 = 6,
    RECT_PRISM_RESIZE_HANDLE_CORNER_6 = 7,
    RECT_PRISM_RESIZE_HANDLE_CORNER_7 = 8,
    RECT_PRISM_RESIZE_HANDLE_EDGE_0 = 9,
    RECT_PRISM_RESIZE_HANDLE_EDGE_1 = 10,
    RECT_PRISM_RESIZE_HANDLE_EDGE_2 = 11,
    RECT_PRISM_RESIZE_HANDLE_EDGE_3 = 12,
    RECT_PRISM_RESIZE_HANDLE_EDGE_4 = 13,
    RECT_PRISM_RESIZE_HANDLE_EDGE_5 = 14,
    RECT_PRISM_RESIZE_HANDLE_EDGE_6 = 15,
    RECT_PRISM_RESIZE_HANDLE_EDGE_7 = 16,
    RECT_PRISM_RESIZE_HANDLE_EDGE_8 = 17,
    RECT_PRISM_RESIZE_HANDLE_EDGE_9 = 18,
    RECT_PRISM_RESIZE_HANDLE_EDGE_10 = 19,
    RECT_PRISM_RESIZE_HANDLE_EDGE_11 = 20
} RectPrismResizeHandleKind;

typedef struct {
    bool allowU;
    bool allowV;
    bool allowN;
} RectPrismHandleAxisMask;

typedef struct {
    float width;
    float height;
    bool useExplicitFrame;
    PlaneFrame3 explicitFrame;
    bool lockToConstructionPlane;
    bool lockToBounds;
} PlanePrimitiveCreateParams;

typedef struct {
    float width;
    float height;
    float depth;
    bool useExplicitFrame;
    PlaneFrame3 explicitFrame;
    bool lockToConstructionPlane;
    bool lockToBounds;
} RectPrismPrimitiveCreateParams;

typedef struct {
    uint32_t objectId;
    Object3DKind kind;
    Transform3D transform;
    CoreObject coreMeta;
    PlanePrimitive3D plane;
    RectPrismPrimitive3D rectPrism;
    bool isDeleted;
} Object3D;

typedef struct {
    Object3D* items;
    size_t count;
    uint32_t nextObjectId;
} LayoutObjectStore;


//        Full layout graph
// ======================================
typedef struct {
    float gridSize;
    Scene3DSettings scene3d;
    LayoutObjectStore objectStore;

    Anchor* anchors;
    size_t anchorCount;

    Wall* walls;
    size_t wallCount;
} Layout;


//        Public layout management
// ======================================
void Layout_Init(Layout* layout, float gridSize);
void Layout_Free(Layout* layout);
void Layout_CompactDeletedElements(Layout* layout);
Vec3 Layout_ComputeCentroid(const Layout* layout, bool* outHasAnchors);
void Layout_Scene3DSettings_SetDefaults(Scene3DSettings* settings);
bool Layout_SceneBounds3D_IsValid(const SceneBounds3D* bounds);
bool Layout_SceneBounds3D_ClampPoint(const SceneBounds3D* bounds, Vec3* point, bool* outClamped);
bool Layout_SceneBoundsHandle_IsValid(SceneBoundsHandleKind handle);
bool Layout_SceneBoundsHandleAxisMask(SceneBoundsHandleKind handle,
                                      RectPrismHandleAxisMask* outMask);
Vec3 Layout_SceneBoundsAxisDirection_WorldVector(RectPrismAxisDirection direction);
bool Layout_SceneBoundsHandleWorldPoint(const SceneBounds3D* bounds,
                                        SceneBoundsHandleKind handle,
                                        Vec3* outPoint);
bool Layout_ResizeSceneBounds3DFromHandle(Layout* layout,
                                          SceneBoundsHandleKind handle,
                                          Vec3 draggedWorldPoint);
bool Layout_TranslateSceneBounds3D(Layout* layout, Vec3 delta);
bool Layout_FitSceneBounds3DToObject(Layout* layout,
                                     uint32_t objectId,
                                     float padding);
void Layout_ConstructionPlane3D_SetDefaults(ConstructionPlane3D* plane);
void Layout_ConstructionPlane3D_SetFromViewPlane(ConstructionPlane3D* plane, ViewPlane viewPlane);
bool Layout_ConstructionPlane3D_IsValid(const ConstructionPlane3D* plane);
ViewPlane Layout_ConstructionPlane3D_ToViewPlane(const ConstructionPlane3D* plane);
void Layout_ObjectStore_Init(LayoutObjectStore* store);
void Layout_ObjectStore_Free(LayoutObjectStore* store);
Transform3D Layout_Transform3D_Default(void);
void Layout_PlanePrimitiveCreateParams_SetDefaults(PlanePrimitiveCreateParams* params);
void Layout_RectPrismPrimitiveCreateParams_SetDefaults(RectPrismPrimitiveCreateParams* params);
uint32_t Layout_ObjectStore_Create(LayoutObjectStore* store,
                                   Object3DKind kind,
                                   const Transform3D* transform,
                                   const char* objectType,
                                   CoreObjectDimensionalMode dimensionalMode,
                                   CoreObjectPlane lockedPlane);
bool Layout_CreatePlanePrimitive(Layout* layout,
                                 const PlanePrimitiveCreateParams* params,
                                 uint32_t* outObjectId,
                                 bool* outBoundsAdjusted);
bool Layout_CreateRectPrismPrimitive(Layout* layout,
                                     const RectPrismPrimitiveCreateParams* params,
                                     uint32_t* outObjectId,
                                     bool* outBoundsAdjusted);
float Layout_PlanePrimitiveMinSize(void);
PlaneResizeHandleKind Layout_ResolvePlaneResizeHandleForDrag(const Object3D* object,
                                                             PlaneResizeHandleKind handle,
                                                             Vec3 draggedWorldPoint);
bool Layout_PlaneResizeHandleAxisMask(PlaneResizeHandleKind handle,
                                      RectPrismHandleAxisMask* outMask);
Vec3 Layout_PlaneAxisDirection_WorldVector(const Object3D* object,
                                           RectPrismAxisDirection direction);
bool Layout_PlaneResizeHandleWorldPoint(const Object3D* object,
                                        PlaneResizeHandleKind handle,
                                        Vec3* outPoint);
PlaneResizeHandleKind Layout_ResolveRectPrismResizeHandleForDrag(const Object3D* object,
                                                                 PlaneResizeHandleKind handle,
                                                                 Vec3 draggedWorldPoint);
bool Layout_ResizePlanePrimitiveFromHandle(Layout* layout,
                                           uint32_t objectId,
                                           PlaneResizeHandleKind handle,
                                           Vec3 draggedWorldPoint,
                                           bool* outBoundsAdjusted);
bool Layout_ResizeRectPrismPrimitiveFromHandle(Layout* layout,
                                               uint32_t objectId,
                                               PlaneResizeHandleKind handle,
                                               Vec3 draggedWorldPoint,
                                               bool* outBoundsAdjusted);
bool Layout_ResizeRectPrismDepthFromFaceHandle(Layout* layout,
                                               uint32_t objectId,
                                               PlaneResizeHandleKind handle,
                                               bool useTopFace,
                                               Vec3 draggedWorldPoint,
                                               bool* outBoundsAdjusted);
bool Layout_SetRectPrismDimensions(Layout* layout,
                                   uint32_t objectId,
                                   float width,
                                   float height,
                                   float depth,
                                   bool* outBoundsAdjusted);
bool Layout_SetObject3DPosition(Layout* layout,
                                uint32_t objectId,
                                Vec3 position,
                                bool* outBoundsAdjusted);
bool Layout_RotateObject3D(Layout* layout,
                           uint32_t objectId,
                           Vec3 axisWorld,
                           float angleDeg,
                           const Object3D* baselineObject,
                           bool* outBoundsAdjusted);
bool Layout_RectPrismHandleAxisMask(PlaneResizeHandleKind handle,
                                    RectPrismHandleAxisMask* outMask);
bool Layout_RectPrismAxisDirection_IsValid(RectPrismAxisDirection direction);
int Layout_RectPrismAxisDirection_Family(RectPrismAxisDirection direction);
Vec3 Layout_RectPrismAxisDirection_WorldVector(const Object3D* object,
                                               RectPrismAxisDirection direction);
bool Layout_RectPrismResizeHandle_IsValid(RectPrismResizeHandleKind handle);
bool Layout_RectPrismResizeHandleAxisMask(RectPrismResizeHandleKind handle,
                                          RectPrismHandleAxisMask* outMask);
bool Layout_RectPrismResizeHandleWorldPoint(const Object3D* object,
                                            RectPrismResizeHandleKind handle,
                                            Vec3* outPoint);
RectPrismResizeHandleKind Layout_ResolveRectPrismResizeHandleFor3DDrag(
    const Object3D* object,
    RectPrismResizeHandleKind handle,
    Vec3 draggedWorldPoint);
bool Layout_ResizeRectPrismFrom3DHandle(Layout* layout,
                                        uint32_t objectId,
                                        RectPrismResizeHandleKind handle,
                                        Vec3 draggedWorldPoint,
                                        bool* outBoundsAdjusted);
bool Layout_RectPrismSelectFaceForView(const Object3D* object,
                                       ViewPlane plane,
                                       const FreeViewCamera* camera,
                                       bool* outUseTopFace);
bool Layout_RectPrismHandleWorldPoint(const Object3D* object,
                                      PlaneResizeHandleKind handle,
                                      bool useTopFace,
                                      Vec3* outPoint);
Object3D* Layout_ObjectStore_Find(LayoutObjectStore* store, uint32_t objectId);
const Object3D* Layout_ObjectStore_FindConst(const LayoutObjectStore* store, uint32_t objectId);
bool Layout_ObjectStore_Delete(LayoutObjectStore* store, uint32_t objectId);
bool Layout_ObjectStore_ValidateObject(const Object3D* object);
size_t Layout_ObjectStore_LiveCount(const LayoutObjectStore* store);
bool Layout_Object3D_ComputePlaneCorners(const Object3D* object, Vec3 outCorners[4]);
bool Layout_Object3D_ComputeRectPrismCorners(const Object3D* object, Vec3 outCorners[8]);
bool Layout_Object3D_ComputeWorldAABB(const Object3D* object, Vec3* outMin, Vec3* outMax);


//        Anchor management
// ======================================
int  Layout_AddAnchor(Layout* layout, Vec2 pos);
int  Layout_AddAnchor3(Layout* layout, Vec3 pos);
void Layout_RemoveAnchor(Layout* layout, int anchorIndex);
void Layout_MarkAnchorDeleted(Layout* layout, int anchorIndex);
bool Layout_SetAnchorType(Layout* layout, int anchorIndex, AnchorType type);
bool Layout_CanAnchorBecomeCurve(const Layout* layout, int anchorIndex);
bool Layout_SetHandlesLinked(Layout* layout, int anchorIndex, bool linked);


//        Wall management
// ======================================
void Layout_AddWall(Layout* layout, Vec2 from, Vec2 to);
void Layout_AddWall3(Layout* layout, Vec3 from, Vec3 to);
void Layout_RemoveWall(Layout* layout, int wallIndex);
void Layout_MarkWallDeleted(Layout* layout, int wallIndex);
