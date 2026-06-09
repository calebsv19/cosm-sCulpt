#pragma once

#include "Layout/layout.h"
#include "ObjectAuthoring/object_authoring_document.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    ObjectAuthoringDocument document;
    bool attached;
    uint32_t sourceSceneObjectId;
} ObjectAuthoringSession;

void ObjectAuthoringSession_Init(ObjectAuthoringSession* session);
void ObjectAuthoringSession_Free(ObjectAuthoringSession* session);
void ObjectAuthoringSession_Clear(ObjectAuthoringSession* session);
bool ObjectAuthoringSession_Copy(ObjectAuthoringSession* dst,
                                 const ObjectAuthoringSession* src);

bool ObjectAuthoringSession_ResetFromLayout(ObjectAuthoringSession* session,
                                            const Layout* layout,
                                            uint32_t source_scene_object_id);
bool ObjectAuthoringSession_MirrorBodiesFromLayout(ObjectAuthoringSession* session,
                                                   const Layout* layout);
bool ObjectAuthoringSession_SetSelection(ObjectAuthoringSession* session,
                                         ObjectAuthoringBodyId body_id,
                                         Object3DFaceKind face);
bool ObjectAuthoringSession_SetRectangleSketch(ObjectAuthoringSession* session,
                                               ObjectAuthoringBodyId body_id,
                                               Object3DFaceKind face,
                                               PlaneFrame3 frame,
                                               Vec2 min_uv,
                                               Vec2 max_uv,
                                               ObjectAuthoringSketchId* out_sketch_id);
bool ObjectAuthoringSession_ClearActiveSketch(ObjectAuthoringSession* session);
bool ObjectAuthoringSession_RecordExtrude(ObjectAuthoringSession* session,
                                          ObjectAuthoringOperationKind kind,
                                          ObjectAuthoringBodyId body_id,
                                          Object3DFaceKind face,
                                          float depth,
                                          const uint32_t* result_body_ids,
                                          size_t result_body_count,
                                          ObjectAuthoringOperationId* out_operation_id);
bool ObjectAuthoringSession_RecordExtrudeFromLayout(ObjectAuthoringSession* session,
                                                    const Layout* layout,
                                                    ObjectAuthoringOperationKind kind,
                                                    ObjectAuthoringBodyId body_id,
                                                    Object3DFaceKind face,
                                                    float depth,
                                                    const uint32_t* result_body_ids,
                                                    size_t result_body_count,
                                                    ObjectAuthoringOperationId* out_operation_id);
bool ObjectAuthoringSession_RecordExtrudeWithSnapshots(
    ObjectAuthoringSession* session,
    ObjectAuthoringOperationKind kind,
    ObjectAuthoringBodyId body_id,
    Object3DFaceKind face,
    float depth,
    const uint32_t* result_body_ids,
    const ObjectAuthoringBody* result_bodies,
    size_t result_body_count,
    ObjectAuthoringOperationId* out_operation_id);
