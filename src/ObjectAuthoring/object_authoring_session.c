#include "ObjectAuthoring/object_authoring_session.h"

#include <string.h>

void ObjectAuthoringSession_Init(ObjectAuthoringSession* session) {
    if (!session) return;
    memset(session, 0, sizeof(*session));
    ObjectAuthoringDocument_Init(&session->document);
}

void ObjectAuthoringSession_Free(ObjectAuthoringSession* session) {
    if (!session) return;
    ObjectAuthoringDocument_Free(&session->document);
    session->attached = false;
    session->sourceSceneObjectId = 0u;
}

void ObjectAuthoringSession_Clear(ObjectAuthoringSession* session) {
    if (!session) return;
    ObjectAuthoringDocument_Clear(&session->document);
    session->attached = false;
    session->sourceSceneObjectId = 0u;
}

bool ObjectAuthoringSession_Copy(ObjectAuthoringSession* dst,
                                 const ObjectAuthoringSession* src) {
    if (!dst || !src) return false;
    ObjectAuthoringSession_Clear(dst);
    if (!ObjectAuthoringDocument_Copy(&dst->document, &src->document)) {
        ObjectAuthoringSession_Clear(dst);
        return false;
    }
    dst->attached = src->attached;
    dst->sourceSceneObjectId = src->sourceSceneObjectId;
    return true;
}

bool ObjectAuthoringSession_ResetFromLayout(ObjectAuthoringSession* session,
                                            const Layout* layout,
                                            uint32_t source_scene_object_id) {
    if (!session || !layout) return false;
    ObjectAuthoringSession_Clear(session);
    session->sourceSceneObjectId = source_scene_object_id;
    session->attached = true;
    return ObjectAuthoringDocument_ResetFromLayoutAsCreateOperations(&session->document,
                                                                     layout);
}

bool ObjectAuthoringSession_MirrorBodiesFromLayout(ObjectAuthoringSession* session,
                                                   const Layout* layout) {
    if (!session || !layout) return false;
    if (!session->attached) {
        session->attached = true;
    }
    return ObjectAuthoringDocument_MirrorBodiesFromLayout(&session->document, layout);
}

bool ObjectAuthoringSession_SetSelection(ObjectAuthoringSession* session,
                                         ObjectAuthoringBodyId body_id,
                                         Object3DFaceKind face) {
    if (!session || !session->attached) return false;
    return ObjectAuthoringDocument_SetSelection(&session->document, body_id, face);
}

bool ObjectAuthoringSession_SetRectangleSketch(ObjectAuthoringSession* session,
                                               ObjectAuthoringBodyId body_id,
                                               Object3DFaceKind face,
                                               PlaneFrame3 frame,
                                               Vec2 min_uv,
                                               Vec2 max_uv,
                                               ObjectAuthoringSketchId* out_sketch_id) {
    if (!session || !session->attached) return false;
    return ObjectAuthoringDocument_SetRectangleSketch(&session->document,
                                                      body_id,
                                                      face,
                                                      frame,
                                                      min_uv,
                                                      max_uv,
                                                      out_sketch_id);
}

bool ObjectAuthoringSession_ClearActiveSketch(ObjectAuthoringSession* session) {
    if (!session || !session->attached) return false;
    return ObjectAuthoringDocument_ClearActiveSketch(&session->document);
}

bool ObjectAuthoringSession_RecordExtrude(ObjectAuthoringSession* session,
                                          ObjectAuthoringOperationKind kind,
                                          ObjectAuthoringBodyId body_id,
                                          Object3DFaceKind face,
                                          float depth,
                                          const uint32_t* result_body_ids,
                                          size_t result_body_count,
                                          ObjectAuthoringOperationId* out_operation_id) {
    const ObjectAuthoringSketch* sketch = NULL;
    ObjectAuthoringSketchId sketch_id = 0u;
    if (!session || !session->attached) return false;
    sketch = ObjectAuthoringDocument_ActiveSketch(&session->document);
    if (sketch) {
        sketch_id = sketch->sketchId;
    }
    return ObjectAuthoringDocument_RecordExtrude(&session->document,
                                                 kind,
                                                 body_id,
                                                 face,
                                                 sketch_id,
                                                 depth,
                                                 result_body_ids,
                                                 NULL,
                                                 result_body_count,
                                                 out_operation_id);
}

bool ObjectAuthoringSession_RecordExtrudeFromLayout(ObjectAuthoringSession* session,
                                                    const Layout* layout,
                                                    ObjectAuthoringOperationKind kind,
                                                    ObjectAuthoringBodyId body_id,
                                                    Object3DFaceKind face,
                                                    float depth,
                                                    const uint32_t* result_body_ids,
                                                    size_t result_body_count,
                                                    ObjectAuthoringOperationId* out_operation_id) {
    ObjectAuthoringBody result_bodies[8];
    size_t capped_count = result_body_count;
    const ObjectAuthoringSketch* sketch = NULL;
    ObjectAuthoringSketchId sketch_id = 0u;
    if (!session || !session->attached || !layout) return false;
    if (capped_count > 8u) capped_count = 8u;
    if (capped_count > 0u && !result_body_ids) return false;
    for (size_t i = 0; i < capped_count; ++i) {
        const Object3D* object = NULL;
        memset(&result_bodies[i], 0, sizeof(result_bodies[i]));
        if (result_body_ids[i] == 0u) return false;
        object = Layout_ObjectStore_FindConst(&layout->objectStore, result_body_ids[i]);
        if (!object) return false;
        result_bodies[i] = (ObjectAuthoringBody){
            .bodyId = object->objectId,
            .sourceObjectId = object->objectId,
            .authoringKind = object->kind == OBJECT3D_KIND_PLANE
                ? OBJECT_AUTHORING_BODY_KIND_PLANE_PRIMITIVE
                : object->kind == OBJECT3D_KIND_RECT_PRISM
                    ? OBJECT_AUTHORING_BODY_KIND_RECT_PRISM_PRIMITIVE
                    : OBJECT_AUTHORING_BODY_KIND_UNKNOWN,
            .sourceKind = object->kind,
            .transform = object->transform,
            .plane = object->plane,
            .rectPrism = object->rectPrism
        };
    }
    sketch = ObjectAuthoringDocument_ActiveSketch(&session->document);
    if (sketch) {
        sketch_id = sketch->sketchId;
    }
    return ObjectAuthoringDocument_RecordExtrude(&session->document,
                                                 kind,
                                                 body_id,
                                                 face,
                                                 sketch_id,
                                                 depth,
                                                 result_body_ids,
                                                 result_bodies,
                                                 capped_count,
                                                 out_operation_id);
}

bool ObjectAuthoringSession_RecordExtrudeWithSnapshots(
    ObjectAuthoringSession* session,
    ObjectAuthoringOperationKind kind,
    ObjectAuthoringBodyId body_id,
    Object3DFaceKind face,
    float depth,
    const uint32_t* result_body_ids,
    const ObjectAuthoringBody* result_bodies,
    size_t result_body_count,
    ObjectAuthoringOperationId* out_operation_id) {
    const ObjectAuthoringSketch* sketch = NULL;
    ObjectAuthoringSketchId sketch_id = 0u;
    if (!session || !session->attached) return false;
    if (result_body_count > 0u && (!result_body_ids || !result_bodies)) return false;
    sketch = ObjectAuthoringDocument_ActiveSketch(&session->document);
    if (sketch) {
        sketch_id = sketch->sketchId;
    }
    return ObjectAuthoringDocument_RecordExtrude(&session->document,
                                                 kind,
                                                 body_id,
                                                 face,
                                                 sketch_id,
                                                 depth,
                                                 result_body_ids,
                                                 result_bodies,
                                                 result_body_count,
                                                 out_operation_id);
}
