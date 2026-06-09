#pragma once

#include "Layout/layout.h"
#include "ObjectAuthoring/object_authoring_ids.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    OBJECT_AUTHORING_BODY_KIND_UNKNOWN = 0,
    OBJECT_AUTHORING_BODY_KIND_PLANE_PRIMITIVE = 1,
    OBJECT_AUTHORING_BODY_KIND_RECT_PRISM_PRIMITIVE = 2
} ObjectAuthoringBodyKind;

typedef enum {
    OBJECT_AUTHORING_OPERATION_NONE = 0,
    OBJECT_AUTHORING_OPERATION_CREATE_PRIMITIVE = 1,
    OBJECT_AUTHORING_OPERATION_SKETCH_RECTANGLE = 2,
    OBJECT_AUTHORING_OPERATION_EXTRUDE_ADD = 3,
    OBJECT_AUTHORING_OPERATION_EXTRUDE_CUT = 4
} ObjectAuthoringOperationKind;

typedef enum {
    OBJECT_AUTHORING_FACE_ROLE_UNKNOWN = 0,
    OBJECT_AUTHORING_FACE_ROLE_PLANE_SURFACE = 1,
    OBJECT_AUTHORING_FACE_ROLE_RECT_PRISM_NEG_N = 2,
    OBJECT_AUTHORING_FACE_ROLE_RECT_PRISM_POS_N = 3,
    OBJECT_AUTHORING_FACE_ROLE_RECT_PRISM_NEG_V = 4,
    OBJECT_AUTHORING_FACE_ROLE_RECT_PRISM_POS_V = 5,
    OBJECT_AUTHORING_FACE_ROLE_RECT_PRISM_NEG_U = 6,
    OBJECT_AUTHORING_FACE_ROLE_RECT_PRISM_POS_U = 7
} ObjectAuthoringFaceRole;

typedef struct {
    ObjectAuthoringBodyId bodyId;
    uint32_t sourceObjectId;
    ObjectAuthoringBodyKind authoringKind;
    Object3DKind sourceKind;
    Transform3D transform;
    PlanePrimitive3D plane;
    RectPrismPrimitive3D rectPrism;
} ObjectAuthoringBody;

typedef struct {
    ObjectAuthoringBodyId bodyId;
    ObjectAuthoringFaceId faceId;
    Object3DFaceKind primitiveFace;
} ObjectAuthoringFaceRef;

typedef struct {
    ObjectAuthoringBodyId bodyId;
    ObjectAuthoringVertexId vertexId;
    uint32_t primitiveVertexIndex;
} ObjectAuthoringVertexRef;

typedef struct {
    ObjectAuthoringBodyId bodyId;
    ObjectAuthoringEdgeId edgeId;
    uint32_t primitiveEdgeIndex;
} ObjectAuthoringEdgeRef;

typedef enum {
    OBJECT_AUTHORING_SELECTION_NONE = 0,
    OBJECT_AUTHORING_SELECTION_BODY = 1,
    OBJECT_AUTHORING_SELECTION_FACE = 2,
    OBJECT_AUTHORING_SELECTION_EDGE = 3,
    OBJECT_AUTHORING_SELECTION_VERTEX = 4
} ObjectAuthoringSelectionKind;

typedef struct {
    ObjectAuthoringVertexId vertexId;
    ObjectAuthoringVertexRef ref;
    Vec3 position;
    bool valid;
} ObjectAuthoringVertex;

typedef struct {
    ObjectAuthoringEdgeId edgeId;
    ObjectAuthoringEdgeRef ref;
    ObjectAuthoringVertexId vertexIds[2];
    ObjectAuthoringFaceId faceIds[2];
    size_t faceCount;
    bool valid;
} ObjectAuthoringEdge;

typedef struct {
    ObjectAuthoringFaceId faceId;
    ObjectAuthoringFaceRef ref;
    ObjectAuthoringFaceRole role;
    PlaneFrame3 frame;
    ObjectAuthoringVertexId vertexIds[4];
    ObjectAuthoringEdgeId edgeIds[4];
    size_t vertexCount;
    size_t edgeCount;
    bool valid;
} ObjectAuthoringFace;

typedef enum {
    OBJECT_AUTHORING_FACE_REF_STATUS_OK = 0,
    OBJECT_AUTHORING_FACE_REF_STATUS_UNSET = 1,
    OBJECT_AUTHORING_FACE_REF_STATUS_MISSING_BODY = 2,
    OBJECT_AUTHORING_FACE_REF_STATUS_MISSING_FACE = 3,
    OBJECT_AUTHORING_FACE_REF_STATUS_STALE_ADAPTER = 4
} ObjectAuthoringFaceRefStatus;

typedef struct {
    ObjectAuthoringSketchId sketchId;
    ObjectAuthoringFaceRef faceRef;
    PlaneFrame3 frame;
    Vec2 minUV;
    Vec2 maxUV;
    ObjectAuthoringOperationId operationId;
    bool active;
} ObjectAuthoringSketch;

typedef struct {
    ObjectAuthoringOperationId operationId;
    ObjectAuthoringOperationKind kind;
    ObjectAuthoringFaceRef faceRef;
    ObjectAuthoringSketchId sketchId;
    ObjectAuthoringBody bodySnapshot;
    ObjectAuthoringSketch sketchSnapshot;
    float depth;
    uint32_t resultBodyIds[8];
    ObjectAuthoringBody resultBodies[8];
    size_t resultBodyCount;
} ObjectAuthoringOperation;

typedef struct {
    ObjectAuthoringBody* bodies;
    size_t bodyCount;
    size_t bodyCapacity;

    ObjectAuthoringFace* faces;
    size_t faceCount;
    size_t faceCapacity;

    ObjectAuthoringVertex* vertices;
    size_t vertexCount;
    size_t vertexCapacity;

    ObjectAuthoringEdge* edges;
    size_t edgeCount;
    size_t edgeCapacity;

    ObjectAuthoringSketch* sketches;
    size_t sketchCount;
    size_t sketchCapacity;
    ObjectAuthoringSketchId nextSketchId;

    ObjectAuthoringOperation* operations;
    size_t operationCount;
    size_t operationCapacity;
    ObjectAuthoringOperationId nextOperationId;

    ObjectAuthoringSelectionKind selectionKind;
    ObjectAuthoringFaceRef selectedFace;
    ObjectAuthoringVertexRef selectedVertex;
    ObjectAuthoringEdgeRef selectedEdge;
    ObjectAuthoringSketchId selectedSketchId;
    ObjectAuthoringOperationId selectedOperationId;
} ObjectAuthoringDocument;

void ObjectAuthoringDocument_Init(ObjectAuthoringDocument* doc);
void ObjectAuthoringDocument_Free(ObjectAuthoringDocument* doc);
void ObjectAuthoringDocument_Clear(ObjectAuthoringDocument* doc);
bool ObjectAuthoringDocument_Copy(ObjectAuthoringDocument* dst,
                                  const ObjectAuthoringDocument* src);

ObjectAuthoringFaceId ObjectAuthoringFaceId_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    Object3DFaceKind face);
ObjectAuthoringVertexId ObjectAuthoringVertexId_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    uint32_t primitive_vertex_index);
ObjectAuthoringEdgeId ObjectAuthoringEdgeId_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    uint32_t primitive_edge_index);
ObjectAuthoringFaceRef ObjectAuthoringFaceRef_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    Object3DFaceKind face);
ObjectAuthoringVertexRef ObjectAuthoringVertexRef_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    uint32_t primitive_vertex_index);
ObjectAuthoringEdgeRef ObjectAuthoringEdgeRef_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    uint32_t primitive_edge_index);
bool ObjectAuthoringFaceRef_IsSet(ObjectAuthoringFaceRef ref);
bool ObjectAuthoringFaceRef_Matches(ObjectAuthoringFaceRef lhs,
                                    ObjectAuthoringFaceRef rhs);
ObjectAuthoringFaceRefStatus ObjectAuthoringDocument_CheckFaceRef(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringFaceRef ref);
const char* ObjectAuthoringFaceRefStatus_Label(ObjectAuthoringFaceRefStatus status);
bool ObjectAuthoringDocument_RebuildFaces(ObjectAuthoringDocument* doc);
bool ObjectAuthoringDocument_RebuildTopology(ObjectAuthoringDocument* doc);

bool ObjectAuthoringDocument_MirrorBodiesFromLayout(ObjectAuthoringDocument* doc,
                                                    const Layout* layout);
bool ObjectAuthoringDocument_ResetFromLayoutAsCreateOperations(ObjectAuthoringDocument* doc,
                                                               const Layout* layout);
const ObjectAuthoringBody* ObjectAuthoringDocument_FindBody(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringBodyId body_id);
const ObjectAuthoringFace* ObjectAuthoringDocument_FindFace(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringFaceId face_id);
const ObjectAuthoringVertex* ObjectAuthoringDocument_FindVertex(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringVertexId vertex_id);
const ObjectAuthoringEdge* ObjectAuthoringDocument_FindEdge(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringEdgeId edge_id);
const ObjectAuthoringFace* ObjectAuthoringDocument_FindFaceByPrimitive(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringBodyId body_id,
    Object3DFaceKind face);
const ObjectAuthoringSketch* ObjectAuthoringDocument_FindSketch(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringSketchId sketch_id);
const ObjectAuthoringOperation* ObjectAuthoringDocument_FindOperation(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringOperationId operation_id);
bool ObjectAuthoringDocument_UpsertBody(ObjectAuthoringDocument* doc,
                                        const ObjectAuthoringBody* body);
bool ObjectAuthoringDocument_RemoveBody(ObjectAuthoringDocument* doc,
                                        ObjectAuthoringBodyId body_id);
bool ObjectAuthoringDocument_SetSelection(ObjectAuthoringDocument* doc,
                                          ObjectAuthoringBodyId body_id,
                                          Object3DFaceKind face);
bool ObjectAuthoringDocument_SetVertexSelection(ObjectAuthoringDocument* doc,
                                                ObjectAuthoringBodyId body_id,
                                                uint32_t primitive_vertex_index);
bool ObjectAuthoringDocument_SetEdgeSelection(ObjectAuthoringDocument* doc,
                                              ObjectAuthoringBodyId body_id,
                                              uint32_t primitive_edge_index);

bool ObjectAuthoringDocument_SetRectangleSketch(ObjectAuthoringDocument* doc,
                                                ObjectAuthoringBodyId body_id,
                                                Object3DFaceKind face,
                                                PlaneFrame3 frame,
                                                Vec2 min_uv,
                                                Vec2 max_uv,
                                                ObjectAuthoringSketchId* out_sketch_id);
bool ObjectAuthoringDocument_UpsertSketchSnapshot(ObjectAuthoringDocument* doc,
                                                  const ObjectAuthoringSketch* sketch);
bool ObjectAuthoringDocument_AppendSketchSnapshot(ObjectAuthoringDocument* doc,
                                                  const ObjectAuthoringSketch* sketch);
bool ObjectAuthoringDocument_ClearActiveSketch(ObjectAuthoringDocument* doc);
const ObjectAuthoringSketch* ObjectAuthoringDocument_ActiveSketch(
    const ObjectAuthoringDocument* doc);

bool ObjectAuthoringDocument_RecordExtrude(ObjectAuthoringDocument* doc,
                                           ObjectAuthoringOperationKind kind,
                                           ObjectAuthoringBodyId body_id,
                                           Object3DFaceKind face,
                                           ObjectAuthoringSketchId sketch_id,
                                           float depth,
                                           const uint32_t* result_body_ids,
                                           const ObjectAuthoringBody* result_bodies,
                                           size_t result_body_count,
                                           ObjectAuthoringOperationId* out_operation_id);
bool ObjectAuthoringDocument_AppendOperationSnapshot(
    ObjectAuthoringDocument* doc,
    const ObjectAuthoringOperation* operation);
