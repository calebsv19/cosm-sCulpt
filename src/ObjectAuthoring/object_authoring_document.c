#include "ObjectAuthoring/object_authoring_document.h"

#include "Layout/scene/layout_object_faces.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ObjectAuthoringBodyKind ObjectAuthoring_BodyKindFromObjectKind(Object3DKind kind) {
    switch (kind) {
        case OBJECT3D_KIND_PLANE:
            return OBJECT_AUTHORING_BODY_KIND_PLANE_PRIMITIVE;
        case OBJECT3D_KIND_RECT_PRISM:
            return OBJECT_AUTHORING_BODY_KIND_RECT_PRISM_PRIMITIVE;
        case OBJECT3D_KIND_UNKNOWN:
        default:
            return OBJECT_AUTHORING_BODY_KIND_UNKNOWN;
    }
}

static ObjectAuthoringBody ObjectAuthoring_BodyFromObject(const Object3D* object) {
    if (!object) return (ObjectAuthoringBody){0};
    return (ObjectAuthoringBody){
        .bodyId = object->objectId,
        .sourceObjectId = object->objectId,
        .authoringKind = ObjectAuthoring_BodyKindFromObjectKind(object->kind),
        .sourceKind = object->kind,
        .transform = object->transform,
        .plane = object->plane,
        .rectPrism = object->rectPrism
    };
}

static bool ObjectAuthoring_EnsureOperationCapacity(ObjectAuthoringDocument* doc,
                                                    size_t required);
static bool ObjectAuthoringDocument_RebuildFacesForBody(ObjectAuthoringDocument* doc,
                                                        const ObjectAuthoringBody* body);
ObjectAuthoringVertexRef ObjectAuthoringVertexRef_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    uint32_t primitive_vertex_index);
ObjectAuthoringEdgeRef ObjectAuthoringEdgeRef_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    uint32_t primitive_edge_index);

static bool ObjectAuthoring_EnsureBodyCapacity(ObjectAuthoringDocument* doc,
                                               size_t required) {
    ObjectAuthoringBody* next = NULL;
    size_t next_capacity = 0u;
    if (!doc) return false;
    if (required <= doc->bodyCapacity) return true;
    next_capacity = doc->bodyCapacity > 0u ? doc->bodyCapacity : 4u;
    while (next_capacity < required) {
        next_capacity *= 2u;
    }
    next = (ObjectAuthoringBody*)realloc(doc->bodies,
                                         next_capacity * sizeof(*doc->bodies));
    if (!next) return false;
    doc->bodies = next;
    doc->bodyCapacity = next_capacity;
    return true;
}

static bool ObjectAuthoring_EnsureFaceCapacity(ObjectAuthoringDocument* doc,
                                               size_t required) {
    ObjectAuthoringFace* next = NULL;
    size_t next_capacity = 0u;
    if (!doc) return false;
    if (required <= doc->faceCapacity) return true;
    next_capacity = doc->faceCapacity > 0u ? doc->faceCapacity : 8u;
    while (next_capacity < required) {
        next_capacity *= 2u;
    }
    next = (ObjectAuthoringFace*)realloc(doc->faces,
                                         next_capacity * sizeof(*doc->faces));
    if (!next) return false;
    doc->faces = next;
    doc->faceCapacity = next_capacity;
    return true;
}

static bool ObjectAuthoring_EnsureVertexCapacity(ObjectAuthoringDocument* doc,
                                                 size_t required) {
    ObjectAuthoringVertex* next = NULL;
    size_t next_capacity = 0u;
    if (!doc) return false;
    if (required <= doc->vertexCapacity) return true;
    next_capacity = doc->vertexCapacity > 0u ? doc->vertexCapacity : 8u;
    while (next_capacity < required) {
        next_capacity *= 2u;
    }
    next = (ObjectAuthoringVertex*)realloc(
        doc->vertices,
        next_capacity * sizeof(*doc->vertices));
    if (!next) return false;
    doc->vertices = next;
    doc->vertexCapacity = next_capacity;
    return true;
}

static bool ObjectAuthoring_EnsureEdgeCapacity(ObjectAuthoringDocument* doc,
                                               size_t required) {
    ObjectAuthoringEdge* next = NULL;
    size_t next_capacity = 0u;
    if (!doc) return false;
    if (required <= doc->edgeCapacity) return true;
    next_capacity = doc->edgeCapacity > 0u ? doc->edgeCapacity : 12u;
    while (next_capacity < required) {
        next_capacity *= 2u;
    }
    next = (ObjectAuthoringEdge*)realloc(doc->edges,
                                         next_capacity * sizeof(*doc->edges));
    if (!next) return false;
    doc->edges = next;
    doc->edgeCapacity = next_capacity;
    return true;
}

static ObjectAuthoringOperation* ObjectAuthoringDocument_FindMutableOperation(
    ObjectAuthoringDocument* doc,
    ObjectAuthoringOperationId operation_id) {
    if (!doc || operation_id == 0u) return NULL;
    for (size_t i = 0; i < doc->operationCount; ++i) {
        if (doc->operations[i].operationId == operation_id) {
            return &doc->operations[i];
        }
    }
    return NULL;
}

static bool ObjectAuthoringDocument_AppendOperation(ObjectAuthoringDocument* doc,
                                                    ObjectAuthoringOperationKind kind,
                                                    ObjectAuthoringOperation** out_operation) {
    ObjectAuthoringOperation* operation = NULL;
    if (out_operation) *out_operation = NULL;
    if (!doc || kind == OBJECT_AUTHORING_OPERATION_NONE) return false;
    if (!ObjectAuthoring_EnsureOperationCapacity(doc, doc->operationCount + 1u)) {
        return false;
    }
    operation = &doc->operations[doc->operationCount++];
    memset(operation, 0, sizeof(*operation));
    operation->operationId = doc->nextOperationId++;
    operation->kind = kind;
    doc->selectedOperationId = operation->operationId;
    if (out_operation) *out_operation = operation;
    return true;
}

static bool ObjectAuthoring_EnsureSketchCapacity(ObjectAuthoringDocument* doc,
                                                 size_t required) {
    ObjectAuthoringSketch* next = NULL;
    size_t next_capacity = 0u;
    if (!doc) return false;
    if (required <= doc->sketchCapacity) return true;
    next_capacity = doc->sketchCapacity > 0u ? doc->sketchCapacity : 2u;
    while (next_capacity < required) {
        next_capacity *= 2u;
    }
    next = (ObjectAuthoringSketch*)realloc(doc->sketches,
                                           next_capacity * sizeof(*doc->sketches));
    if (!next) return false;
    doc->sketches = next;
    doc->sketchCapacity = next_capacity;
    return true;
}

static bool ObjectAuthoring_EnsureOperationCapacity(ObjectAuthoringDocument* doc,
                                                    size_t required) {
    ObjectAuthoringOperation* next = NULL;
    size_t next_capacity = 0u;
    if (!doc) return false;
    if (required <= doc->operationCapacity) return true;
    next_capacity = doc->operationCapacity > 0u ? doc->operationCapacity : 4u;
    while (next_capacity < required) {
        next_capacity *= 2u;
    }
    next = (ObjectAuthoringOperation*)realloc(
        doc->operations,
        next_capacity * sizeof(*doc->operations));
    if (!next) return false;
    doc->operations = next;
    doc->operationCapacity = next_capacity;
    return true;
}

void ObjectAuthoringDocument_Init(ObjectAuthoringDocument* doc) {
    if (!doc) return;
    memset(doc, 0, sizeof(*doc));
    doc->nextSketchId = 1u;
    doc->nextOperationId = 1u;
    doc->selectionKind = OBJECT_AUTHORING_SELECTION_NONE;
    doc->selectedFace.primitiveFace = OBJECT3D_FACE_NONE;
}

void ObjectAuthoringDocument_Free(ObjectAuthoringDocument* doc) {
    if (!doc) return;
    free(doc->bodies);
    free(doc->faces);
    free(doc->vertices);
    free(doc->edges);
    free(doc->sketches);
    free(doc->operations);
    ObjectAuthoringDocument_Init(doc);
}

void ObjectAuthoringDocument_Clear(ObjectAuthoringDocument* doc) {
    if (!doc) return;
    doc->bodyCount = 0u;
    doc->faceCount = 0u;
    doc->vertexCount = 0u;
    doc->edgeCount = 0u;
    doc->sketchCount = 0u;
    doc->operationCount = 0u;
    doc->nextSketchId = 1u;
    doc->nextOperationId = 1u;
    doc->selectionKind = OBJECT_AUTHORING_SELECTION_NONE;
    doc->selectedFace = ObjectAuthoringFaceRef_FromPrimitive(0u, OBJECT3D_FACE_NONE);
    doc->selectedVertex = ObjectAuthoringVertexRef_FromPrimitive(0u, 0u);
    doc->selectedEdge = ObjectAuthoringEdgeRef_FromPrimitive(0u, 0u);
    doc->selectedSketchId = 0u;
    doc->selectedOperationId = 0u;
}

bool ObjectAuthoringDocument_Copy(ObjectAuthoringDocument* dst,
                                  const ObjectAuthoringDocument* src) {
    if (!dst || !src) return false;
    ObjectAuthoringDocument_Clear(dst);
    if (!ObjectAuthoring_EnsureBodyCapacity(dst, src->bodyCount) ||
        !ObjectAuthoring_EnsureFaceCapacity(dst, src->faceCount) ||
        !ObjectAuthoring_EnsureVertexCapacity(dst, src->vertexCount) ||
        !ObjectAuthoring_EnsureEdgeCapacity(dst, src->edgeCount) ||
        !ObjectAuthoring_EnsureSketchCapacity(dst, src->sketchCount) ||
        !ObjectAuthoring_EnsureOperationCapacity(dst, src->operationCount)) {
        ObjectAuthoringDocument_Clear(dst);
        return false;
    }
    if (src->bodyCount > 0u) {
        memcpy(dst->bodies, src->bodies, src->bodyCount * sizeof(*src->bodies));
    }
    if (src->faceCount > 0u) {
        memcpy(dst->faces, src->faces, src->faceCount * sizeof(*src->faces));
    }
    if (src->vertexCount > 0u) {
        memcpy(dst->vertices, src->vertices, src->vertexCount * sizeof(*src->vertices));
    }
    if (src->edgeCount > 0u) {
        memcpy(dst->edges, src->edges, src->edgeCount * sizeof(*src->edges));
    }
    if (src->sketchCount > 0u) {
        memcpy(dst->sketches, src->sketches, src->sketchCount * sizeof(*src->sketches));
    }
    if (src->operationCount > 0u) {
        memcpy(dst->operations,
               src->operations,
               src->operationCount * sizeof(*src->operations));
    }
    dst->bodyCount = src->bodyCount;
    dst->faceCount = src->faceCount;
    dst->vertexCount = src->vertexCount;
    dst->edgeCount = src->edgeCount;
    dst->sketchCount = src->sketchCount;
    dst->operationCount = src->operationCount;
    dst->nextSketchId = src->nextSketchId;
    dst->nextOperationId = src->nextOperationId;
    dst->selectionKind = src->selectionKind;
    dst->selectedFace = src->selectedFace;
    dst->selectedVertex = src->selectedVertex;
    dst->selectedEdge = src->selectedEdge;
    dst->selectedSketchId = src->selectedSketchId;
    dst->selectedOperationId = src->selectedOperationId;
    return true;
}

ObjectAuthoringFaceId ObjectAuthoringFaceId_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    Object3DFaceKind face) {
    if (body_id == 0u || face == OBJECT3D_FACE_NONE) return 0u;
    return (ObjectAuthoringFaceId)(body_id * 16u + (uint32_t)face);
}

ObjectAuthoringVertexId ObjectAuthoringVertexId_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    uint32_t primitive_vertex_index) {
    if (body_id == 0u) return 0u;
    return (ObjectAuthoringVertexId)(body_id * 1024u + primitive_vertex_index + 1u);
}

ObjectAuthoringEdgeId ObjectAuthoringEdgeId_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    uint32_t primitive_edge_index) {
    if (body_id == 0u) return 0u;
    return (ObjectAuthoringEdgeId)(body_id * 1024u + 128u + primitive_edge_index + 1u);
}

ObjectAuthoringFaceRef ObjectAuthoringFaceRef_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    Object3DFaceKind face) {
    return (ObjectAuthoringFaceRef){
        .bodyId = body_id,
        .faceId = ObjectAuthoringFaceId_FromPrimitive(body_id, face),
        .primitiveFace = face
    };
}

ObjectAuthoringVertexRef ObjectAuthoringVertexRef_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    uint32_t primitive_vertex_index) {
    return (ObjectAuthoringVertexRef){
        .bodyId = body_id,
        .vertexId = ObjectAuthoringVertexId_FromPrimitive(body_id, primitive_vertex_index),
        .primitiveVertexIndex = primitive_vertex_index
    };
}

ObjectAuthoringEdgeRef ObjectAuthoringEdgeRef_FromPrimitive(
    ObjectAuthoringBodyId body_id,
    uint32_t primitive_edge_index) {
    return (ObjectAuthoringEdgeRef){
        .bodyId = body_id,
        .edgeId = ObjectAuthoringEdgeId_FromPrimitive(body_id, primitive_edge_index),
        .primitiveEdgeIndex = primitive_edge_index
    };
}

bool ObjectAuthoringFaceRef_IsSet(ObjectAuthoringFaceRef ref) {
    return ref.bodyId != 0u && ref.faceId != 0u &&
           ref.primitiveFace != OBJECT3D_FACE_NONE;
}

bool ObjectAuthoringFaceRef_Matches(ObjectAuthoringFaceRef lhs,
                                    ObjectAuthoringFaceRef rhs) {
    if (lhs.faceId != 0u && rhs.faceId != 0u) {
        return lhs.faceId == rhs.faceId;
    }
    return lhs.bodyId == rhs.bodyId &&
           lhs.bodyId != 0u &&
           lhs.primitiveFace == rhs.primitiveFace &&
           lhs.primitiveFace != OBJECT3D_FACE_NONE;
}

ObjectAuthoringFaceRefStatus ObjectAuthoringDocument_CheckFaceRef(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringFaceRef ref) {
    const ObjectAuthoringFace* face = NULL;
    if (!doc || ref.bodyId == 0u || ref.primitiveFace == OBJECT3D_FACE_NONE) {
        return OBJECT_AUTHORING_FACE_REF_STATUS_UNSET;
    }
    if (!ObjectAuthoringDocument_FindBody(doc, ref.bodyId)) {
        return OBJECT_AUTHORING_FACE_REF_STATUS_MISSING_BODY;
    }
    if (ref.faceId != 0u) {
        face = ObjectAuthoringDocument_FindFace(doc, ref.faceId);
        if (!face) return OBJECT_AUTHORING_FACE_REF_STATUS_MISSING_FACE;
        if (face->ref.bodyId != ref.bodyId ||
            face->ref.primitiveFace != ref.primitiveFace) {
            return OBJECT_AUTHORING_FACE_REF_STATUS_STALE_ADAPTER;
        }
        return OBJECT_AUTHORING_FACE_REF_STATUS_OK;
    }
    return ObjectAuthoringDocument_FindFaceByPrimitive(doc,
                                                       ref.bodyId,
                                                       ref.primitiveFace)
               ? OBJECT_AUTHORING_FACE_REF_STATUS_OK
               : OBJECT_AUTHORING_FACE_REF_STATUS_MISSING_FACE;
}

const char* ObjectAuthoringFaceRefStatus_Label(ObjectAuthoringFaceRefStatus status) {
    switch (status) {
        case OBJECT_AUTHORING_FACE_REF_STATUS_OK:
            return "OK";
        case OBJECT_AUTHORING_FACE_REF_STATUS_UNSET:
            return "Unset";
        case OBJECT_AUTHORING_FACE_REF_STATUS_MISSING_BODY:
            return "Missing body";
        case OBJECT_AUTHORING_FACE_REF_STATUS_MISSING_FACE:
            return "Missing face";
        case OBJECT_AUTHORING_FACE_REF_STATUS_STALE_ADAPTER:
            return "Stale adapter";
        default:
            return "Unknown";
    }
}

static ObjectAuthoringFaceRole ObjectAuthoringFaceRole_FromPrimitive(
    Object3DFaceKind face) {
    switch (face) {
        case OBJECT3D_FACE_PLANE_SURFACE:
            return OBJECT_AUTHORING_FACE_ROLE_PLANE_SURFACE;
        case OBJECT3D_FACE_RECT_PRISM_NEG_N:
            return OBJECT_AUTHORING_FACE_ROLE_RECT_PRISM_NEG_N;
        case OBJECT3D_FACE_RECT_PRISM_POS_N:
            return OBJECT_AUTHORING_FACE_ROLE_RECT_PRISM_POS_N;
        case OBJECT3D_FACE_RECT_PRISM_NEG_V:
            return OBJECT_AUTHORING_FACE_ROLE_RECT_PRISM_NEG_V;
        case OBJECT3D_FACE_RECT_PRISM_POS_V:
            return OBJECT_AUTHORING_FACE_ROLE_RECT_PRISM_POS_V;
        case OBJECT3D_FACE_RECT_PRISM_NEG_U:
            return OBJECT_AUTHORING_FACE_ROLE_RECT_PRISM_NEG_U;
        case OBJECT3D_FACE_RECT_PRISM_POS_U:
            return OBJECT_AUTHORING_FACE_ROLE_RECT_PRISM_POS_U;
        case OBJECT3D_FACE_NONE:
        default:
            return OBJECT_AUTHORING_FACE_ROLE_UNKNOWN;
    }
}

static Object3D ObjectAuthoring_ObjectFromBody(const ObjectAuthoringBody* body) {
    Object3D object;
    memset(&object, 0, sizeof(object));
    if (!body) return object;
    object.objectId = body->bodyId;
    object.kind = body->sourceKind;
    object.transform = body->transform;
    object.plane = body->plane;
    object.rectPrism = body->rectPrism;
    object.isDeleted = false;
    core_object_init(&object.coreMeta);
    snprintf(object.coreMeta.object_id,
             sizeof(object.coreMeta.object_id),
             "body_%u",
             body->bodyId);
    snprintf(object.coreMeta.object_type,
             sizeof(object.coreMeta.object_type),
             "%s",
             body->sourceKind == OBJECT3D_KIND_PLANE ? "plane_primitive"
                                                     : "rect_prism_primitive");
    object.coreMeta.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
    object.coreMeta.locked_plane = CORE_OBJECT_PLANE_XY;
    object.coreMeta.transform.position = (CoreObjectVec3){
        .x = (double)body->transform.position.x,
        .y = (double)body->transform.position.y,
        .z = (double)body->transform.position.z
    };
    object.coreMeta.transform.rotation_deg = (CoreObjectVec3){
        .x = (double)body->transform.rotationDeg.x,
        .y = (double)body->transform.rotationDeg.y,
        .z = (double)body->transform.rotationDeg.z
    };
    object.coreMeta.transform.scale = (CoreObjectVec3){
        .x = (double)body->transform.scale.x,
        .y = (double)body->transform.scale.y,
        .z = (double)body->transform.scale.z
    };
    return object;
}

static bool ObjectAuthoringDocument_AppendVertex(ObjectAuthoringDocument* doc,
                                                 const ObjectAuthoringBody* body,
                                                 uint32_t primitive_vertex_index,
                                                 Vec3 position) {
    ObjectAuthoringVertex* vertex = NULL;
    if (!doc || !body || body->bodyId == 0u) return false;
    if (!ObjectAuthoring_EnsureVertexCapacity(doc, doc->vertexCount + 1u)) return false;
    vertex = &doc->vertices[doc->vertexCount++];
    memset(vertex, 0, sizeof(*vertex));
    vertex->ref = ObjectAuthoringVertexRef_FromPrimitive(body->bodyId,
                                                         primitive_vertex_index);
    vertex->vertexId = vertex->ref.vertexId;
    vertex->position = position;
    vertex->valid = vertex->vertexId != 0u;
    return vertex->valid;
}

static ObjectAuthoringEdge* ObjectAuthoringDocument_FindMutableEdge(
    ObjectAuthoringDocument* doc,
    ObjectAuthoringEdgeId edge_id) {
    if (!doc || edge_id == 0u) return NULL;
    for (size_t i = 0u; i < doc->edgeCount; ++i) {
        if (doc->edges[i].edgeId == edge_id) return &doc->edges[i];
    }
    return NULL;
}

static bool ObjectAuthoringDocument_AppendEdge(ObjectAuthoringDocument* doc,
                                               const ObjectAuthoringBody* body,
                                               uint32_t primitive_edge_index,
                                               uint32_t primitive_vertex_a,
                                               uint32_t primitive_vertex_b) {
    ObjectAuthoringEdge* edge = NULL;
    if (!doc || !body || body->bodyId == 0u) return false;
    if (!ObjectAuthoring_EnsureEdgeCapacity(doc, doc->edgeCount + 1u)) return false;
    edge = &doc->edges[doc->edgeCount++];
    memset(edge, 0, sizeof(*edge));
    edge->ref = ObjectAuthoringEdgeRef_FromPrimitive(body->bodyId,
                                                     primitive_edge_index);
    edge->edgeId = edge->ref.edgeId;
    edge->vertexIds[0] =
        ObjectAuthoringVertexId_FromPrimitive(body->bodyId, primitive_vertex_a);
    edge->vertexIds[1] =
        ObjectAuthoringVertexId_FromPrimitive(body->bodyId, primitive_vertex_b);
    edge->valid = edge->edgeId != 0u &&
                  edge->vertexIds[0] != 0u &&
                  edge->vertexIds[1] != 0u &&
                  edge->vertexIds[0] != edge->vertexIds[1];
    return edge->valid;
}

static void ObjectAuthoringDocument_LinkEdgeToFace(ObjectAuthoringDocument* doc,
                                                   ObjectAuthoringEdgeId edge_id,
                                                   ObjectAuthoringFaceId face_id) {
    ObjectAuthoringEdge* edge = ObjectAuthoringDocument_FindMutableEdge(doc, edge_id);
    if (!edge || face_id == 0u || edge->faceCount >= 2u) return;
    for (size_t i = 0u; i < edge->faceCount; ++i) {
        if (edge->faceIds[i] == face_id) return;
    }
    edge->faceIds[edge->faceCount++] = face_id;
}

static bool ObjectAuthoringDocument_AppendFace(ObjectAuthoringDocument* doc,
                                               const ObjectAuthoringBody* body,
                                               Object3DFaceKind face_kind,
                                               const uint32_t* primitive_vertices,
                                               const uint32_t* primitive_edges,
                                               size_t corner_count) {
    ObjectAuthoringFace* face = NULL;
    Object3D object;
    if (!doc || !body || face_kind == OBJECT3D_FACE_NONE ||
        !primitive_vertices || !primitive_edges || corner_count == 0u ||
        corner_count > 4u) {
        return false;
    }
    if (!ObjectAuthoring_EnsureFaceCapacity(doc, doc->faceCount + 1u)) return false;
    object = ObjectAuthoring_ObjectFromBody(body);
    if (!Layout_Object3DFaceKind_IsValidForObject(&object, face_kind)) return true;
    face = &doc->faces[doc->faceCount++];
    memset(face, 0, sizeof(*face));
    face->ref = ObjectAuthoringFaceRef_FromPrimitive(body->bodyId, face_kind);
    face->faceId = face->ref.faceId;
    face->role = ObjectAuthoringFaceRole_FromPrimitive(face_kind);
    face->valid = Layout_Object3DFace_GetFrame(&object, face_kind, &face->frame);
    face->vertexCount = corner_count;
    face->edgeCount = corner_count;
    for (size_t i = 0u; i < corner_count; ++i) {
        face->vertexIds[i] =
            ObjectAuthoringVertexId_FromPrimitive(body->bodyId, primitive_vertices[i]);
        face->edgeIds[i] =
            ObjectAuthoringEdgeId_FromPrimitive(body->bodyId, primitive_edges[i]);
        ObjectAuthoringDocument_LinkEdgeToFace(doc, face->edgeIds[i], face->faceId);
    }
    return true;
}

static bool ObjectAuthoringDocument_RebuildFacesForBody(ObjectAuthoringDocument* doc,
                                                        const ObjectAuthoringBody* body) {
    static const uint32_t kPlaneFaceVertices[4] = {0u, 1u, 2u, 3u};
    static const uint32_t kPlaneFaceEdges[4] = {0u, 1u, 2u, 3u};
    static const uint32_t kPlaneEdges[4][2] = {
        {0u, 1u}, {1u, 2u}, {2u, 3u}, {3u, 0u}
    };
    static const uint32_t kRectPrismEdges[12][2] = {
        {0u, 1u}, {1u, 2u}, {2u, 3u}, {3u, 0u},
        {4u, 5u}, {5u, 6u}, {6u, 7u}, {7u, 4u},
        {0u, 4u}, {1u, 5u}, {2u, 6u}, {3u, 7u}
    };
    static const struct {
        Object3DFaceKind face;
        uint32_t vertices[4];
        uint32_t edges[4];
    } kRectPrismFaces[6] = {
        { OBJECT3D_FACE_RECT_PRISM_NEG_N, { 0u, 1u, 2u, 3u }, { 0u, 1u, 2u, 3u } },
        { OBJECT3D_FACE_RECT_PRISM_POS_N, { 4u, 5u, 6u, 7u }, { 4u, 5u, 6u, 7u } },
        { OBJECT3D_FACE_RECT_PRISM_NEG_V, { 0u, 1u, 5u, 4u }, { 0u, 9u, 4u, 8u } },
        { OBJECT3D_FACE_RECT_PRISM_POS_V, { 3u, 2u, 6u, 7u }, { 2u, 10u, 6u, 11u } },
        { OBJECT3D_FACE_RECT_PRISM_NEG_U, { 0u, 4u, 7u, 3u }, { 8u, 7u, 11u, 3u } },
        { OBJECT3D_FACE_RECT_PRISM_POS_U, { 1u, 2u, 6u, 5u }, { 1u, 10u, 5u, 9u } }
    };
    Object3D object;
    Vec3 corners[8];
    if (!doc || !body || body->bodyId == 0u) return false;
    object = ObjectAuthoring_ObjectFromBody(body);
    if (body->sourceKind == OBJECT3D_KIND_PLANE) {
        if (!Layout_Object3D_ComputePlaneCorners(&object, corners)) return false;
        for (uint32_t i = 0u; i < 4u; ++i) {
            if (!ObjectAuthoringDocument_AppendVertex(doc, body, i, corners[i])) {
                return false;
            }
        }
        for (uint32_t i = 0u; i < 4u; ++i) {
            if (!ObjectAuthoringDocument_AppendEdge(doc,
                                                    body,
                                                    i,
                                                    kPlaneEdges[i][0],
                                                    kPlaneEdges[i][1])) {
                return false;
            }
        }
        return ObjectAuthoringDocument_AppendFace(doc,
                                                  body,
                                                  OBJECT3D_FACE_PLANE_SURFACE,
                                                  kPlaneFaceVertices,
                                                  kPlaneFaceEdges,
                                                  4u);
    }
    if (body->sourceKind == OBJECT3D_KIND_RECT_PRISM) {
        if (!Layout_Object3D_ComputeRectPrismCorners(&object, corners)) return false;
        for (uint32_t i = 0u; i < 8u; ++i) {
            if (!ObjectAuthoringDocument_AppendVertex(doc, body, i, corners[i])) {
                return false;
            }
        }
        for (uint32_t i = 0u; i < 12u; ++i) {
            if (!ObjectAuthoringDocument_AppendEdge(doc,
                                                    body,
                                                    i,
                                                    kRectPrismEdges[i][0],
                                                    kRectPrismEdges[i][1])) {
                return false;
            }
        }
        for (size_t i = 0u; i < 6u; ++i) {
            if (!ObjectAuthoringDocument_AppendFace(doc,
                                                    body,
                                                    kRectPrismFaces[i].face,
                                                    kRectPrismFaces[i].vertices,
                                                    kRectPrismFaces[i].edges,
                                                    4u)) {
                return false;
            }
        }
    }
    return true;
}

bool ObjectAuthoringDocument_RebuildFaces(ObjectAuthoringDocument* doc) {
    if (!doc) return false;
    doc->vertexCount = 0u;
    doc->edgeCount = 0u;
    doc->faceCount = 0u;
    for (size_t i = 0; i < doc->bodyCount; ++i) {
        if (!ObjectAuthoringDocument_RebuildFacesForBody(doc, &doc->bodies[i])) {
            doc->faceCount = 0u;
            doc->vertexCount = 0u;
            doc->edgeCount = 0u;
            return false;
        }
    }
    if (doc->selectedFace.faceId != 0u &&
        !ObjectAuthoringDocument_FindFace(doc, doc->selectedFace.faceId)) {
        doc->selectedFace =
            ObjectAuthoringFaceRef_FromPrimitive(doc->selectedFace.bodyId,
                                                 doc->selectedFace.primitiveFace);
    }
    if (doc->selectedVertex.vertexId != 0u &&
        !ObjectAuthoringDocument_FindVertex(doc, doc->selectedVertex.vertexId)) {
        doc->selectedVertex = ObjectAuthoringVertexRef_FromPrimitive(0u, 0u);
        if (doc->selectionKind == OBJECT_AUTHORING_SELECTION_VERTEX) {
            doc->selectionKind = OBJECT_AUTHORING_SELECTION_NONE;
        }
    }
    if (doc->selectedEdge.edgeId != 0u &&
        !ObjectAuthoringDocument_FindEdge(doc, doc->selectedEdge.edgeId)) {
        doc->selectedEdge = ObjectAuthoringEdgeRef_FromPrimitive(0u, 0u);
        if (doc->selectionKind == OBJECT_AUTHORING_SELECTION_EDGE) {
            doc->selectionKind = OBJECT_AUTHORING_SELECTION_NONE;
        }
    }
    return true;
}

bool ObjectAuthoringDocument_RebuildTopology(ObjectAuthoringDocument* doc) {
    return ObjectAuthoringDocument_RebuildFaces(doc);
}

bool ObjectAuthoringDocument_MirrorBodiesFromLayout(ObjectAuthoringDocument* doc,
                                                    const Layout* layout) {
    size_t live_count = 0u;
    size_t write_index = 0u;
    if (!doc || !layout) return false;

    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        if (!object->isDeleted && object->kind != OBJECT3D_KIND_UNKNOWN) {
            ++live_count;
        }
    }
    if (!ObjectAuthoring_EnsureBodyCapacity(doc, live_count)) return false;

    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        ObjectAuthoringBody* body = NULL;
        if (object->isDeleted || object->kind == OBJECT3D_KIND_UNKNOWN) continue;
        body = &doc->bodies[write_index++];
        *body = ObjectAuthoring_BodyFromObject(object);
    }
    doc->bodyCount = write_index;
    if (doc->selectedFace.bodyId != 0u &&
        !ObjectAuthoringDocument_FindBody(doc, doc->selectedFace.bodyId)) {
        doc->selectedFace = ObjectAuthoringFaceRef_FromPrimitive(0u, OBJECT3D_FACE_NONE);
    }
    return ObjectAuthoringDocument_RebuildFaces(doc);
}

bool ObjectAuthoringDocument_ResetFromLayoutAsCreateOperations(ObjectAuthoringDocument* doc,
                                                               const Layout* layout) {
    if (!doc || !layout) return false;
    ObjectAuthoringDocument_Clear(doc);
    if (!ObjectAuthoringDocument_MirrorBodiesFromLayout(doc, layout)) return false;
    for (size_t i = 0; i < doc->bodyCount; ++i) {
        ObjectAuthoringOperation* operation = NULL;
        if (!ObjectAuthoringDocument_AppendOperation(doc,
                                                     OBJECT_AUTHORING_OPERATION_CREATE_PRIMITIVE,
                                                     &operation)) {
            return false;
        }
        operation->bodySnapshot = doc->bodies[i];
        operation->faceRef =
            ObjectAuthoringFaceRef_FromPrimitive(doc->bodies[i].bodyId,
                                                 OBJECT3D_FACE_NONE);
        operation->resultBodyIds[0] = doc->bodies[i].bodyId;
        operation->resultBodies[0] = doc->bodies[i];
        operation->resultBodyCount = 1u;
    }
    return true;
}

const ObjectAuthoringBody* ObjectAuthoringDocument_FindBody(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringBodyId body_id) {
    if (!doc || body_id == 0u) return NULL;
    for (size_t i = 0; i < doc->bodyCount; ++i) {
        if (doc->bodies[i].bodyId == body_id) {
            return &doc->bodies[i];
        }
    }
    return NULL;
}

const ObjectAuthoringFace* ObjectAuthoringDocument_FindFace(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringFaceId face_id) {
    if (!doc || face_id == 0u) return NULL;
    for (size_t i = 0; i < doc->faceCount; ++i) {
        if (doc->faces[i].faceId == face_id) {
            return &doc->faces[i];
        }
    }
    return NULL;
}

const ObjectAuthoringVertex* ObjectAuthoringDocument_FindVertex(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringVertexId vertex_id) {
    if (!doc || vertex_id == 0u) return NULL;
    for (size_t i = 0u; i < doc->vertexCount; ++i) {
        if (doc->vertices[i].vertexId == vertex_id) {
            return &doc->vertices[i];
        }
    }
    return NULL;
}

const ObjectAuthoringEdge* ObjectAuthoringDocument_FindEdge(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringEdgeId edge_id) {
    if (!doc || edge_id == 0u) return NULL;
    for (size_t i = 0u; i < doc->edgeCount; ++i) {
        if (doc->edges[i].edgeId == edge_id) {
            return &doc->edges[i];
        }
    }
    return NULL;
}

const ObjectAuthoringFace* ObjectAuthoringDocument_FindFaceByPrimitive(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringBodyId body_id,
    Object3DFaceKind face) {
    return ObjectAuthoringDocument_FindFace(
        doc,
        ObjectAuthoringFaceId_FromPrimitive(body_id, face));
}

const ObjectAuthoringSketch* ObjectAuthoringDocument_FindSketch(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringSketchId sketch_id) {
    if (!doc || sketch_id == 0u) return NULL;
    for (size_t i = 0; i < doc->sketchCount; ++i) {
        if (doc->sketches[i].sketchId == sketch_id) {
            return &doc->sketches[i];
        }
    }
    return NULL;
}

const ObjectAuthoringOperation* ObjectAuthoringDocument_FindOperation(
    const ObjectAuthoringDocument* doc,
    ObjectAuthoringOperationId operation_id) {
    if (!doc || operation_id == 0u) return NULL;
    for (size_t i = 0; i < doc->operationCount; ++i) {
        if (doc->operations[i].operationId == operation_id) {
            return &doc->operations[i];
        }
    }
    return NULL;
}

bool ObjectAuthoringDocument_UpsertBody(ObjectAuthoringDocument* doc,
                                        const ObjectAuthoringBody* body) {
    if (!doc || !body || body->bodyId == 0u) return false;
    for (size_t i = 0; i < doc->bodyCount; ++i) {
        if (doc->bodies[i].bodyId == body->bodyId) {
            doc->bodies[i] = *body;
            return ObjectAuthoringDocument_RebuildFaces(doc);
        }
    }
    if (!ObjectAuthoring_EnsureBodyCapacity(doc, doc->bodyCount + 1u)) return false;
    doc->bodies[doc->bodyCount++] = *body;
    return ObjectAuthoringDocument_RebuildFaces(doc);
}

bool ObjectAuthoringDocument_RemoveBody(ObjectAuthoringDocument* doc,
                                        ObjectAuthoringBodyId body_id) {
    if (!doc || body_id == 0u) return false;
    for (size_t i = 0; i < doc->bodyCount; ++i) {
        if (doc->bodies[i].bodyId == body_id) {
            if (i + 1u < doc->bodyCount) {
                memmove(&doc->bodies[i],
                        &doc->bodies[i + 1u],
                        (doc->bodyCount - i - 1u) * sizeof(*doc->bodies));
            }
            --doc->bodyCount;
            if (doc->selectedFace.bodyId == body_id) {
                doc->selectedFace = ObjectAuthoringFaceRef_FromPrimitive(
                    0u,
                    OBJECT3D_FACE_NONE);
            }
            if (doc->selectedVertex.bodyId == body_id) {
                doc->selectedVertex = ObjectAuthoringVertexRef_FromPrimitive(0u, 0u);
            }
            if (doc->selectedEdge.bodyId == body_id) {
                doc->selectedEdge = ObjectAuthoringEdgeRef_FromPrimitive(0u, 0u);
            }
            if (doc->selectionKind != OBJECT_AUTHORING_SELECTION_NONE) {
                doc->selectionKind = OBJECT_AUTHORING_SELECTION_NONE;
            }
            return ObjectAuthoringDocument_RebuildFaces(doc);
        }
    }
    return false;
}

bool ObjectAuthoringDocument_SetSelection(ObjectAuthoringDocument* doc,
                                          ObjectAuthoringBodyId body_id,
                                          Object3DFaceKind face) {
    if (!doc) return false;
    if (body_id == 0u || face == OBJECT3D_FACE_NONE) {
        doc->selectedFace = ObjectAuthoringFaceRef_FromPrimitive(body_id,
                                                                 OBJECT3D_FACE_NONE);
        doc->selectedVertex = ObjectAuthoringVertexRef_FromPrimitive(0u, 0u);
        doc->selectedEdge = ObjectAuthoringEdgeRef_FromPrimitive(0u, 0u);
        doc->selectionKind = body_id != 0u
            ? OBJECT_AUTHORING_SELECTION_BODY
            : OBJECT_AUTHORING_SELECTION_NONE;
        return true;
    }
    if (!ObjectAuthoringDocument_FindBody(doc, body_id)) return false;
    if (!ObjectAuthoringDocument_FindFaceByPrimitive(doc, body_id, face)) return false;
    doc->selectedFace = ObjectAuthoringFaceRef_FromPrimitive(body_id, face);
    doc->selectedVertex = ObjectAuthoringVertexRef_FromPrimitive(0u, 0u);
    doc->selectedEdge = ObjectAuthoringEdgeRef_FromPrimitive(0u, 0u);
    doc->selectionKind = OBJECT_AUTHORING_SELECTION_FACE;
    return true;
}

bool ObjectAuthoringDocument_SetVertexSelection(ObjectAuthoringDocument* doc,
                                                ObjectAuthoringBodyId body_id,
                                                uint32_t primitive_vertex_index) {
    ObjectAuthoringVertexRef ref;
    if (!doc || body_id == 0u) return false;
    ref = ObjectAuthoringVertexRef_FromPrimitive(body_id, primitive_vertex_index);
    if (!ObjectAuthoringDocument_FindBody(doc, body_id)) return false;
    if (!ObjectAuthoringDocument_FindVertex(doc, ref.vertexId)) return false;
    doc->selectedFace = ObjectAuthoringFaceRef_FromPrimitive(body_id, OBJECT3D_FACE_NONE);
    doc->selectedVertex = ref;
    doc->selectedEdge = ObjectAuthoringEdgeRef_FromPrimitive(0u, 0u);
    doc->selectionKind = OBJECT_AUTHORING_SELECTION_VERTEX;
    doc->selectedSketchId = 0u;
    return true;
}

bool ObjectAuthoringDocument_SetEdgeSelection(ObjectAuthoringDocument* doc,
                                              ObjectAuthoringBodyId body_id,
                                              uint32_t primitive_edge_index) {
    ObjectAuthoringEdgeRef ref;
    if (!doc || body_id == 0u) return false;
    ref = ObjectAuthoringEdgeRef_FromPrimitive(body_id, primitive_edge_index);
    if (!ObjectAuthoringDocument_FindBody(doc, body_id)) return false;
    if (!ObjectAuthoringDocument_FindEdge(doc, ref.edgeId)) return false;
    doc->selectedFace = ObjectAuthoringFaceRef_FromPrimitive(body_id, OBJECT3D_FACE_NONE);
    doc->selectedVertex = ObjectAuthoringVertexRef_FromPrimitive(0u, 0u);
    doc->selectedEdge = ref;
    doc->selectionKind = OBJECT_AUTHORING_SELECTION_EDGE;
    doc->selectedSketchId = 0u;
    return true;
}

static ObjectAuthoringSketch* ObjectAuthoringDocument_FindMutableSketch(
    ObjectAuthoringDocument* doc,
    ObjectAuthoringSketchId sketch_id) {
    if (!doc || sketch_id == 0u) return NULL;
    for (size_t i = 0; i < doc->sketchCount; ++i) {
        if (doc->sketches[i].sketchId == sketch_id) {
            return &doc->sketches[i];
        }
    }
    return NULL;
}

bool ObjectAuthoringDocument_SetRectangleSketch(ObjectAuthoringDocument* doc,
                                                ObjectAuthoringBodyId body_id,
                                                Object3DFaceKind face,
                                                PlaneFrame3 frame,
                                                Vec2 min_uv,
                                                Vec2 max_uv,
                                                ObjectAuthoringSketchId* out_sketch_id) {
    ObjectAuthoringSketch* sketch = NULL;
    if (out_sketch_id) *out_sketch_id = 0u;
    if (!doc || body_id == 0u || face == OBJECT3D_FACE_NONE) return false;
    if (!ObjectAuthoringDocument_FindBody(doc, body_id)) return false;

    sketch = ObjectAuthoringDocument_FindMutableSketch(doc, doc->selectedSketchId);
    if (!sketch) {
        if (!ObjectAuthoring_EnsureSketchCapacity(doc, doc->sketchCount + 1u)) return false;
        sketch = &doc->sketches[doc->sketchCount++];
        memset(sketch, 0, sizeof(*sketch));
        sketch->sketchId = doc->nextSketchId++;
    }
    *sketch = (ObjectAuthoringSketch){
        .sketchId = sketch->sketchId,
        .faceRef = ObjectAuthoringFaceRef_FromPrimitive(body_id, face),
        .frame = frame,
        .minUV = min_uv,
        .maxUV = max_uv,
        .operationId = sketch->operationId,
        .active = true
    };
    if (sketch->operationId != 0u) {
        ObjectAuthoringOperation* operation =
            ObjectAuthoringDocument_FindMutableOperation(doc, sketch->operationId);
        if (operation) {
            operation->faceRef = sketch->faceRef;
            operation->sketchId = sketch->sketchId;
            operation->sketchSnapshot = *sketch;
        }
    } else {
        ObjectAuthoringOperation* operation = NULL;
        if (!ObjectAuthoringDocument_AppendOperation(doc,
                                                     OBJECT_AUTHORING_OPERATION_SKETCH_RECTANGLE,
                                                     &operation)) {
            return false;
        }
        sketch->operationId = operation->operationId;
        operation->faceRef = sketch->faceRef;
        operation->sketchId = sketch->sketchId;
        operation->sketchSnapshot = *sketch;
    }
    doc->selectedFace = ObjectAuthoringFaceRef_FromPrimitive(body_id, face);
    doc->selectedSketchId = sketch->sketchId;
    if (out_sketch_id) *out_sketch_id = sketch->sketchId;
    return true;
}

bool ObjectAuthoringDocument_UpsertSketchSnapshot(ObjectAuthoringDocument* doc,
                                                  const ObjectAuthoringSketch* sketch) {
    ObjectAuthoringSketch* dst = NULL;
    if (!doc || !sketch || sketch->sketchId == 0u) return false;
    if (!ObjectAuthoringDocument_FindBody(doc, sketch->faceRef.bodyId)) return false;
    dst = ObjectAuthoringDocument_FindMutableSketch(doc, sketch->sketchId);
    if (!dst) {
        if (!ObjectAuthoring_EnsureSketchCapacity(doc, doc->sketchCount + 1u)) return false;
        dst = &doc->sketches[doc->sketchCount++];
    }
    *dst = *sketch;
    if (doc->nextSketchId <= sketch->sketchId) {
        doc->nextSketchId = sketch->sketchId + 1u;
    }
    if (sketch->active) {
        doc->selectedSketchId = sketch->sketchId;
        doc->selectedFace = sketch->faceRef;
    }
    return true;
}

bool ObjectAuthoringDocument_AppendSketchSnapshot(ObjectAuthoringDocument* doc,
                                                  const ObjectAuthoringSketch* sketch) {
    ObjectAuthoringSketch* dst = NULL;
    if (!doc || !sketch || sketch->sketchId == 0u) return false;
    if (!ObjectAuthoring_EnsureSketchCapacity(doc, doc->sketchCount + 1u)) return false;
    dst = &doc->sketches[doc->sketchCount++];
    *dst = *sketch;
    if (doc->nextSketchId <= sketch->sketchId) {
        doc->nextSketchId = sketch->sketchId + 1u;
    }
    if (sketch->active) {
        doc->selectedSketchId = sketch->sketchId;
        doc->selectedFace = sketch->faceRef;
    }
    return true;
}

bool ObjectAuthoringDocument_ClearActiveSketch(ObjectAuthoringDocument* doc) {
    ObjectAuthoringSketch* sketch = NULL;
    if (!doc) return false;
    sketch = ObjectAuthoringDocument_FindMutableSketch(doc, doc->selectedSketchId);
    if (sketch) {
        sketch->active = false;
    }
    doc->selectedSketchId = 0u;
    return true;
}

const ObjectAuthoringSketch* ObjectAuthoringDocument_ActiveSketch(
    const ObjectAuthoringDocument* doc) {
    if (!doc || doc->selectedSketchId == 0u) return NULL;
    for (size_t i = 0; i < doc->sketchCount; ++i) {
        if (doc->sketches[i].sketchId == doc->selectedSketchId &&
            doc->sketches[i].active) {
            return &doc->sketches[i];
        }
    }
    return NULL;
}

bool ObjectAuthoringDocument_RecordExtrude(ObjectAuthoringDocument* doc,
                                           ObjectAuthoringOperationKind kind,
                                           ObjectAuthoringBodyId body_id,
                                           Object3DFaceKind face,
                                           ObjectAuthoringSketchId sketch_id,
                                           float depth,
                                           const uint32_t* result_body_ids,
                                           const ObjectAuthoringBody* result_bodies,
                                           size_t result_body_count,
                                           ObjectAuthoringOperationId* out_operation_id) {
    ObjectAuthoringOperation* operation = NULL;
    if (out_operation_id) *out_operation_id = 0u;
    if (!doc || body_id == 0u || face == OBJECT3D_FACE_NONE) return false;
    if (kind != OBJECT_AUTHORING_OPERATION_EXTRUDE_ADD &&
        kind != OBJECT_AUTHORING_OPERATION_EXTRUDE_CUT) {
        return false;
    }
    if (!ObjectAuthoringDocument_AppendOperation(doc, kind, &operation)) {
        return false;
    }

    operation->faceRef = ObjectAuthoringFaceRef_FromPrimitive(body_id, face);
    operation->sketchId = sketch_id;
    operation->depth = depth;
    {
        const ObjectAuthoringSketch* sketch = ObjectAuthoringDocument_ActiveSketch(doc);
        if (sketch && sketch->sketchId == sketch_id) {
            operation->sketchSnapshot = *sketch;
        }
    }
    operation->resultBodyCount = result_body_count;
    if (operation->resultBodyCount > 8u) {
        operation->resultBodyCount = 8u;
    }
    for (size_t i = 0; i < operation->resultBodyCount; ++i) {
        operation->resultBodyIds[i] = result_body_ids ? result_body_ids[i] : 0u;
        if (result_bodies) {
            operation->resultBodies[i] = result_bodies[i];
        }
    }
    if (out_operation_id) *out_operation_id = operation->operationId;
    return true;
}

bool ObjectAuthoringDocument_AppendOperationSnapshot(
    ObjectAuthoringDocument* doc,
    const ObjectAuthoringOperation* operation) {
    ObjectAuthoringOperation* dst = NULL;
    if (!doc || !operation || operation->operationId == 0u ||
        operation->kind == OBJECT_AUTHORING_OPERATION_NONE) {
        return false;
    }
    if (!ObjectAuthoring_EnsureOperationCapacity(doc, doc->operationCount + 1u)) {
        return false;
    }
    dst = &doc->operations[doc->operationCount++];
    *dst = *operation;
    if (dst->resultBodyCount > 8u) {
        dst->resultBodyCount = 8u;
    }
    if (doc->nextOperationId <= dst->operationId) {
        doc->nextOperationId = dst->operationId + 1u;
    }
    doc->selectedOperationId = dst->operationId;
    return true;
}
