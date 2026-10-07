// src/Layout/layout.h
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "Math/math_util.h"
#include "Layout/scene/layout_scene_authoring.h"
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
    OBJECT3D_KIND_RECT_PRISM = 2,
    OBJECT3D_KIND_MESH_ASSET_INSTANCE = 3
} Object3DKind;

typedef enum {
    OBJECT3D_FACE_NONE = 0,
    OBJECT3D_FACE_PLANE_SURFACE = 1,
    OBJECT3D_FACE_RECT_PRISM_NEG_N = 2,
    OBJECT3D_FACE_RECT_PRISM_POS_N = 3,
    OBJECT3D_FACE_RECT_PRISM_NEG_V = 4,
    OBJECT3D_FACE_RECT_PRISM_POS_V = 5,
    OBJECT3D_FACE_RECT_PRISM_NEG_U = 6,
    OBJECT3D_FACE_RECT_PRISM_POS_U = 7
} Object3DFaceKind;

typedef struct {
    float width;
    float height;
    PlaneFrame3 frame;
    bool lockToConstructionPlane;
    bool lockToBounds;
} PlanePrimitive3D;

/* One local U/V rectangular opening, through N or a top-open pocket.
 * floor is retained material at -N; zero removes material through the board. */
typedef struct {
    bool enabled;
    float u, v, width, height, floor;
} LayoutPanelOpening;
typedef struct {
    float width;
    float height;
    float depth;
    LayoutPanelOpening opening;
    PlaneFrame3 frame;
    bool lockToConstructionPlane;
    bool lockToBounds;
} RectPrismPrimitive3D;

typedef struct {
    char assetId[64];
    char sourceAssetId[64];
    char runtimePath[512];
    Vec3 localBoundsMin;
    Vec3 localBoundsMax;
    size_t vertexCount;
    size_t triangleCount;
    bool lockToBounds;
} MeshAssetInstance3D;

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

#define LAYOUT_MAX_PROPERTIES 8
#define LAYOUT_MAX_ASSEMBLIES 32

typedef enum {
    LAYOUT_PROPERTY_TEXT, LAYOUT_PROPERTY_NUMBER, LAYOUT_PROPERTY_BOOL, LAYOUT_PROPERTY_LENGTH
} LayoutPropertyKind;
typedef struct {
    char key[48];
    LayoutPropertyKind kind;
    char text[128];
    double number; /* dimensionless number, boolean 0/1, or canonical meters */
} LayoutProperty;
typedef enum { LAYOUT_VOLUME_NONE, LAYOUT_VOLUME_KEEPOUT, LAYOUT_VOLUME_SERVICE } LayoutVolumeRole;
typedef struct {
    char label[96]; /* display only; empty falls back to the existing primitive label */
    char entity_type[64]; /* extensible category; empty means PhysicalObject */
    char parent_id[64]; /* assembly ID; transform hierarchy, never electrical/support connectivity */
    bool reference;
    LayoutVolumeRole volume_role; /* explicit reserved-space meaning, independent of display type */
    char volume_owner[64]; /* exempt object or assembly subtree; not transform parenting */
    size_t property_count;
    LayoutProperty properties[LAYOUT_MAX_PROPERTIES];
} LayoutEntityInfo;
typedef struct {
    char id[64];
    LayoutEntityInfo info;
    PlaneFrame3 frame; /* world rigid frame; local frames derive from the parent inverse */
} LayoutAssembly;
typedef struct {
    char entity_type[64];
    int designation; /* 0 all, 1 design, 2 reference */
    char assembly_id[64]; /* entire descendant subtree */
    char property_key[48];
    char property_value[128]; /* exact text or canonical scalar representation */
} LayoutEntityQuery;

#define LAYOUT_MAX_SAVED_VIEWS 16
#define LAYOUT_MAX_VIEW_QUERIES 8
typedef struct {
    char id[64], name[96];
    size_t query_count;
    LayoutEntityQuery queries[LAYOUT_MAX_VIEW_QUERIES]; /* OR of AND queries. */
} LayoutSavedView;

typedef struct {
    uint32_t objectId;
    Object3DKind kind;
    Transform3D transform;
    CoreObject coreMeta;
    LayoutEntityInfo info;
    PlanePrimitive3D plane;
    RectPrismPrimitive3D rectPrism;
    MeshAssetInstance3D meshInstance;
    bool isDeleted;
} Object3D;

typedef enum LayoutReferenceKind {
    LAYOUT_REFERENCE_ORIGIN,
    LAYOUT_REFERENCE_AXIS_U,
    LAYOUT_REFERENCE_AXIS_V,
    LAYOUT_REFERENCE_AXIS_N,
    LAYOUT_REFERENCE_FACE
} LayoutReferenceKind;

/* Stable operand: no selection index, mesh triangle or cached world coordinate.
 * Primitive U/V/N use the authored frame, which already includes rotation. */
typedef struct LayoutGeometricReference {
    char entity_id[64];
    LayoutReferenceKind kind;
    Object3DFaceKind face;
    /* Physical offset from the named feature in the object's U/V/N basis.
     * Rotates with the object; resizing does not scale this meter-valued offset. */
    double local_offset_meters[3];
} LayoutGeometricReference;

#define LAYOUT_MAX_CONSTRAINTS 32

typedef enum LayoutConstraintKind {
    LAYOUT_CONSTRAINT_DISTANCE,
    LAYOUT_CONSTRAINT_COINCIDENT,
    LAYOUT_CONSTRAINT_PLANAR_MATE,
    LAYOUT_CONSTRAINT_LINEAR_TRAVEL,
    LAYOUT_CONSTRAINT_ANGULAR_TRAVEL
} LayoutConstraintKind;

typedef struct LayoutConstraint {
    char id[64];
    LayoutGeometricReference a;
    LayoutGeometricReference b;
    LayoutConstraintKind kind;
    Vec3 axis; /* world projection axis, or planar mate normal */
    double target; /* meters for distance/linear travel; degrees for mate/hinge */
    /* Linear travel is world-axis translation relative to A, with fixed B orientation. */
    double travel_min, travel_max, travel_home;
    double travel_offset[3]; /* perpendicular separation, meters */
    double travel_basis[3][3]; /* normalized B U/V/N at capture */
    double hinge_reference[3]; /* A direction at capture; hinge limits/home are degrees */
    char motion_assembly[64]; /* empty: B only; otherwise rigid design assembly driven by B */
} LayoutConstraint;

#define LAYOUT_MAX_RELATIONSHIPS 128

typedef enum {
    LAYOUT_RELATIONSHIP_ATTACHED_TO,
    LAYOUT_RELATIONSHIP_SUPPORTED_BY,
    LAYOUT_RELATIONSHIP_CONTAINED_BY
} LayoutRelationshipKind;

typedef struct {
    char id[64];
    char source[64];
    char target[64];
    LayoutRelationshipKind kind;
} LayoutRelationship;

#define LAYOUT_MAX_SPATIAL_RULES 32
typedef enum { LAYOUT_SPATIAL_NO_INTERSECTION, LAYOUT_SPATIAL_CLEARANCE } LayoutSpatialRuleKind;
typedef struct {
    char id[64], source[64], target[64];
    LayoutSpatialRuleKind kind;
    double clearance_meters;
} LayoutSpatialRule;

#define LAYOUT_MAX_MOTION_ENVELOPES 32
#define LAYOUT_MAX_MOTION_MEMBERS 64
typedef struct {
    char entity_id[64];
    Object3DKind kind;
    double size_meters[3], local_origin[3], local_basis[3][3];
} LayoutMotionMember;
typedef struct {
    uint32_t object_id, samples;
    char rule_id[64], input_digest[17], bounds_digest[17];
    double padding_meters;
    char assembly_id[64];
    size_t member_count;
    LayoutMotionMember members[LAYOUT_MAX_MOTION_MEMBERS];
} LayoutMotionEnvelope;

#define LAYOUT_MAX_ROUTES 32
#define LAYOUT_MAX_ROUTE_POINTS 64
#define LAYOUT_MAX_ROUTE_CORRIDORS 8
typedef struct {
    char circuit[64], power_domain[64];
    int voltage_class; /* 0 unknown, 1 DC, 2 signal, 3 AC */
    double nominal_volts, design_amps, area_mm2, return_meters, allowance_meters, fuse_amps;
    bool copper; /* false = material unknown; calculation currently supports copper only. */
} LayoutRouteElectrical;
typedef struct {
    char id[64];
    LayoutEntityInfo info; /* Cable/Pipe metadata; world-space route, no transform parent. */
    LayoutGeometricReference source, destination;
    size_t point_count;
    double points_meters[LAYOUT_MAX_ROUTE_POINTS][3]; /* Includes captured endpoint positions. */
    size_t corridor_count;
    char corridor_ids[LAYOUT_MAX_ROUTE_CORRIDORS][64];
    double radius_meters, clearance_meters, maximum_length_meters; /* zero = unspecified */
    LayoutRouteElectrical electrical;
} LayoutPhysicalRoute;

/* Bounded construction recipes; quantities are canonical meters in an assembly frame. */
#define LAYOUT_MAX_FURNITURE_UNITS 16
#define LAYOUT_MAX_FURNITURE_PARTS 16
typedef enum {
    LAYOUT_FURNITURE_BACK, LAYOUT_FURNITURE_END_REAR, LAYOUT_FURNITURE_END_FRONT,
    LAYOUT_FURNITURE_BOTTOM, LAYOUT_FURNITURE_TOP, LAYOUT_FURNITURE_DOOR,
    LAYOUT_FURNITURE_SHELF, LAYOUT_FURNITURE_WORKTOP, LAYOUT_FURNITURE_FIXTURE,
    LAYOUT_FURNITURE_RUN_RAIL, LAYOUT_FURNITURE_CUSHION
} LayoutFurnitureRole;
typedef struct {
    char entity_id[64];
    LayoutFurnitureRole role;
    double offset_m[3], span_m[3]; /* Fixed-size fixture anchor from wall/front/top. */
} LayoutFurniturePart;
typedef struct {
    char assembly_id[64];
    double center_m[3], size_m[3]; /* Local depth U, run V, height N. */
    double thickness_m[4]; /* Case, back, door, worktop. */
    double overhang_m[4]; /* Wall, aisle, rear end, front end. */
    bool sink_opening; /* First fixture is the provisional sink; top/worktop share its cutout. */
    double shelf_fraction;
    int back_sign;
    size_t part_count;
    LayoutFurniturePart parts[LAYOUT_MAX_FURNITURE_PARTS];
} LayoutFurnitureUnit;

#define LAYOUT_MAX_FURNITURE_CONTACTS 32
typedef enum { LAYOUT_FURNITURE_FOLLOW_MOVE, LAYOUT_FURNITURE_FOLLOW_FIT_RUN } LayoutFurnitureFollow;
/* Directed run-end plane contact. Gap is along the driver's outward run normal.
 * Units remain independent; this does not certify attachment or structural fit. */
typedef struct {
    char id[64], driver[64], follower[64];
    int driver_end, follower_end; /* -1 rear, +1 front in each unit's local V. */
    LayoutFurnitureFollow behavior;
    double gap_m;
    bool enabled;
} LayoutFurnitureContact;

typedef struct {
    Object3D* items;
    size_t count;
    uint32_t nextObjectId;
    LayoutConstraint constraints[LAYOUT_MAX_CONSTRAINTS];
    size_t constraintCount;
    uint32_t nextConstraintId;
    LayoutFurnitureUnit furniture[LAYOUT_MAX_FURNITURE_UNITS];
    size_t furniture_count;
    LayoutFurnitureContact furniture_contacts[LAYOUT_MAX_FURNITURE_CONTACTS];
    size_t furniture_contact_count;
    uint32_t next_furniture_contact_id;
    LayoutAssembly assemblies[LAYOUT_MAX_ASSEMBLIES];
    size_t assembly_count;
    uint32_t next_assembly_id;
    LayoutRelationship relationships[LAYOUT_MAX_RELATIONSHIPS];
    size_t relationship_count;
    uint32_t next_relationship_id;
    LayoutSpatialRule spatial_rules[LAYOUT_MAX_SPATIAL_RULES];
    size_t spatial_rule_count;
    uint32_t next_spatial_rule_id;
    LayoutMotionEnvelope motion_envelopes[LAYOUT_MAX_MOTION_ENVELOPES];
    size_t motion_envelope_count;
    LayoutPhysicalRoute routes[LAYOUT_MAX_ROUTES];
    size_t route_count;
    uint32_t next_route_id;
    LayoutEntityQuery view_query; /* transient viewport filter; never changes authored visibility */
    LayoutSavedView saved_views[LAYOUT_MAX_SAVED_VIEWS];
    size_t saved_view_count;
    char hidden_view_ids[LAYOUT_MAX_SAVED_VIEWS][64], isolated_view_id[64]; /* transient */
    size_t hidden_view_count;
} LayoutObjectStore;


//        Full layout graph
// ======================================
typedef struct {
    float gridSize;
    /* Physical meters per stored coordinate; new documents use 1.0. */
    double metersPerWorldUnit;
    Scene3DSettings scene3d;
    LineDrawingSceneAuthoringState sceneAuthoring;
    LayoutObjectStore objectStore;
    /* Transient mutation scope and human-readable last refusal; never serialized. */
    bool geometryEditActive;
    bool geometryGestureActive;
    bool geometryGestureCaptured;
    char geometryMessage[160];

    Anchor* anchors;
    size_t anchorCount;

    Wall* walls;
    size_t wallCount;
} Layout;


//        Public layout management
// ======================================
#define LINE_DRAWING_PHYSICAL_FRAME "right_handed_z_up_meters"
/* Zero-initialized legacy callers have the historical meter scale. */
static inline double Layout_WorldScale(const Layout* layout) {
    return layout && layout->metersPerWorldUnit != 0.0 ? layout->metersPerWorldUnit : 1.0;
}
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
bool Layout_CreateMeshAssetInstanceFromRuntimeAsset(Layout* layout,
                                                    const char* runtimeAssetPath,
                                                    const Transform3D* transform,
                                                    uint32_t* outObjectId,
                                                    char* diagnostics,
                                                    size_t diagnostics_size);
bool Layout_RefreshMeshAssetInstanceFromRuntimeAsset(Layout* layout,
                                                     uint32_t objectId,
                                                     bool* outChanged,
                                                     char* diagnostics,
                                                     size_t diagnostics_size);
bool Layout_RefreshMeshAssetInstancesFromRuntimeAsset(Layout* layout,
                                                      const char* runtimeAssetPath,
                                                      size_t* outRefreshedCount,
                                                      size_t* outChangedCount,
                                                      char* diagnostics,
                                                      size_t diagnostics_size);
bool Layout_ReconcileMeshAssetInstancesForScene(Layout* layout,
                                                const char* sceneAuthoringPath,
                                                size_t* outResolvedCount,
                                                size_t* outChangedCount,
                                                size_t* outUnresolvedCount,
                                                char* diagnostics,
                                                size_t diagnostics_size);
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
bool Layout_SetPlaneDimensions(Layout* layout,
                               uint32_t objectId,
                               float width,
                               float height,
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
bool Layout_ScaleObject3D(Layout* layout,
                          uint32_t objectId,
                          Vec3 scaleFactors,
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
Vec3 Layout_Transform3D_ApplyLocalPoint(Transform3D transform, Vec3 localPoint);
bool Layout_Object3D_ComputePlaneCorners(const Object3D* object, Vec3 outCorners[4]);
bool Layout_Object3D_ComputeRectPrismCorners(const Object3D* object, Vec3 outCorners[8]);
bool Layout_Object3D_ComputeMeshInstanceCorners(const Object3D* object, Vec3 outCorners[8]);
bool Layout_Object3D_ComputeVisualCenter(const Object3D* object, Vec3* outCenter);
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
