#include "ObjectAuthoring/object_authoring_eval.h"

#include <stdio.h>
#include <string.h>

static void ObjectAuthoringEval_SetDiag(ObjectAuthoringEvalDiagnostics* diagnostics,
                                        ObjectAuthoringEvalStatus status,
                                        ObjectAuthoringOperationId operation_id,
                                        const char* message) {
    if (!diagnostics) return;
    diagnostics->status = status;
    diagnostics->failedOperationId = operation_id;
    diagnostics->message[0] = '\0';
    if (message) {
        snprintf(diagnostics->message, sizeof(diagnostics->message), "%s", message);
    }
}

static bool ObjectAuthoringEval_UpsertResultBodies(ObjectAuthoringDocument* evaluated,
                                                   const ObjectAuthoringOperation* operation) {
    if (!evaluated || !operation) return false;
    for (size_t i = 0; i < operation->resultBodyCount; ++i) {
        const ObjectAuthoringBody* body = &operation->resultBodies[i];
        if (body->bodyId == 0u) return false;
        if (!ObjectAuthoringDocument_UpsertBody(evaluated, body)) return false;
    }
    return true;
}

static bool ObjectAuthoringEval_RequireFaceRef(const ObjectAuthoringDocument* evaluated,
                                               const ObjectAuthoringOperation* operation,
                                               ObjectAuthoringEvalDiagnostics* diagnostics) {
    ObjectAuthoringFaceRefStatus status = OBJECT_AUTHORING_FACE_REF_STATUS_UNSET;
    char message[128];
    if (!evaluated || !operation) return false;
    status = ObjectAuthoringDocument_CheckFaceRef(evaluated, operation->faceRef);
    if (status == OBJECT_AUTHORING_FACE_REF_STATUS_OK) return true;
    snprintf(message,
             sizeof(message),
             "operation face ref %s",
             ObjectAuthoringFaceRefStatus_Label(status));
    ObjectAuthoringEval_SetDiag(
        diagnostics,
        status == OBJECT_AUTHORING_FACE_REF_STATUS_STALE_ADAPTER
            ? OBJECT_AUTHORING_EVAL_STALE_FACE_REF
            : OBJECT_AUTHORING_EVAL_MISSING_FACE,
        operation->operationId,
        message);
    return false;
}

bool ObjectAuthoring_EvaluateDocument(const ObjectAuthoringDocument* source,
                                      ObjectAuthoringDocument* evaluated,
                                      ObjectAuthoringEvalDiagnostics* diagnostics) {
    if (diagnostics) {
        memset(diagnostics, 0, sizeof(*diagnostics));
        diagnostics->status = OBJECT_AUTHORING_EVAL_OK;
    }
    if (!source || !evaluated) {
        ObjectAuthoringEval_SetDiag(diagnostics,
                                    OBJECT_AUTHORING_EVAL_INVALID_ARGUMENT,
                                    0u,
                                    "missing source or evaluated document");
        return false;
    }

    ObjectAuthoringDocument_Clear(evaluated);
    evaluated->nextSketchId = source->nextSketchId;
    evaluated->nextOperationId = source->nextOperationId;
    evaluated->selectedFace = source->selectedFace;
    evaluated->selectedSketchId = source->selectedSketchId;
    evaluated->selectedOperationId = source->selectedOperationId;

    for (size_t i = 0; i < source->operationCount; ++i) {
        const ObjectAuthoringOperation* operation = &source->operations[i];
        switch (operation->kind) {
            case OBJECT_AUTHORING_OPERATION_CREATE_PRIMITIVE:
                if (operation->bodySnapshot.bodyId == 0u ||
                    !ObjectAuthoringDocument_UpsertBody(evaluated,
                                                        &operation->bodySnapshot)) {
                    ObjectAuthoringEval_SetDiag(diagnostics,
                                                OBJECT_AUTHORING_EVAL_MISSING_BODY,
                                                operation->operationId,
                                                "create operation missing body snapshot");
                    return false;
                }
                break;
            case OBJECT_AUTHORING_OPERATION_SKETCH_RECTANGLE:
                if (!ObjectAuthoringEval_RequireFaceRef(evaluated,
                                                        operation,
                                                        diagnostics)) {
                    return false;
                }
                if (operation->sketchSnapshot.sketchId == 0u ||
                    !ObjectAuthoringDocument_UpsertSketchSnapshot(evaluated,
                                                                  &operation->sketchSnapshot)) {
                    ObjectAuthoringEval_SetDiag(diagnostics,
                                                OBJECT_AUTHORING_EVAL_MISSING_BODY,
                                                operation->operationId,
                                                "sketch operation cannot be replayed");
                    return false;
                }
                break;
            case OBJECT_AUTHORING_OPERATION_EXTRUDE_ADD:
                if (!ObjectAuthoringEval_RequireFaceRef(evaluated,
                                                        operation,
                                                        diagnostics)) {
                    return false;
                }
                if (operation->resultBodyCount == 0u ||
                    !ObjectAuthoringEval_UpsertResultBodies(evaluated, operation)) {
                    ObjectAuthoringEval_SetDiag(diagnostics,
                                                OBJECT_AUTHORING_EVAL_MISSING_RESULT,
                                                operation->operationId,
                                                "extrude add missing result body snapshots");
                    return false;
                }
                break;
            case OBJECT_AUTHORING_OPERATION_EXTRUDE_CUT:
                if (!ObjectAuthoringEval_RequireFaceRef(evaluated,
                                                        operation,
                                                        diagnostics)) {
                    return false;
                }
                if (!ObjectAuthoringDocument_RemoveBody(evaluated,
                                                        operation->faceRef.bodyId)) {
                    ObjectAuthoringEval_SetDiag(diagnostics,
                                                OBJECT_AUTHORING_EVAL_MISSING_BODY,
                                                operation->operationId,
                                                "extrude cut target body missing during replay");
                    return false;
                }
                if (!ObjectAuthoringEval_UpsertResultBodies(evaluated, operation)) {
                    ObjectAuthoringEval_SetDiag(diagnostics,
                                                OBJECT_AUTHORING_EVAL_MISSING_RESULT,
                                                operation->operationId,
                                                "extrude cut missing result body snapshots");
                    return false;
                }
                break;
            case OBJECT_AUTHORING_OPERATION_NONE:
            default:
                ObjectAuthoringEval_SetDiag(diagnostics,
                                            OBJECT_AUTHORING_EVAL_UNSUPPORTED_OPERATION,
                                            operation->operationId,
                                            "unsupported operation kind");
                return false;
        }
    }

    return true;
}

static const char* ObjectAuthoringEval_ObjectTypeForBody(
    const ObjectAuthoringBody* body) {
    if (!body) return "unknown_primitive";
    switch (body->sourceKind) {
        case OBJECT3D_KIND_PLANE:
            return "plane_primitive";
        case OBJECT3D_KIND_RECT_PRISM:
            return "rect_prism_primitive";
        case OBJECT3D_KIND_UNKNOWN:
        default:
            return "unknown_primitive";
    }
}

static bool ObjectAuthoringEval_ApplyBodyToLayout(Layout* layout,
                                                  const ObjectAuthoringBody* body) {
    Object3D* object = NULL;
    uint32_t created_id = 0u;
    if (!layout || !body || body->bodyId == 0u ||
        body->sourceKind == OBJECT3D_KIND_UNKNOWN) {
        return false;
    }

    layout->objectStore.nextObjectId = body->bodyId;
    created_id = Layout_ObjectStore_Create(&layout->objectStore,
                                           body->sourceKind,
                                           &body->transform,
                                           ObjectAuthoringEval_ObjectTypeForBody(body),
                                           CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                           CORE_OBJECT_PLANE_XY);
    if (created_id != body->bodyId) return false;

    object = Layout_ObjectStore_Find(&layout->objectStore, created_id);
    if (!object) return false;
    object->transform = body->transform;
    object->plane = body->plane;
    object->rectPrism = body->rectPrism;
    if (object->kind == OBJECT3D_KIND_PLANE) {
        object->plane.frame.origin = object->transform.position;
    } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
        object->rectPrism.frame.origin = object->transform.position;
    }
    return Layout_ObjectStore_ValidateObject(object);
}

bool ObjectAuthoring_ApplyEvaluatedDocumentToLayout(
    const ObjectAuthoringDocument* evaluated,
    Layout* layout,
    ObjectAuthoringEvalDiagnostics* diagnostics) {
    uint32_t next_body_id = 1u;
    if (!evaluated || !layout) {
        ObjectAuthoringEval_SetDiag(diagnostics,
                                    OBJECT_AUTHORING_EVAL_INVALID_ARGUMENT,
                                    0u,
                                    "missing evaluated document or layout");
        return false;
    }

    Layout_ObjectStore_Free(&layout->objectStore);
    Layout_ObjectStore_Init(&layout->objectStore);
    for (size_t i = 0; i < evaluated->bodyCount; ++i) {
        const ObjectAuthoringBody* body = &evaluated->bodies[i];
        if (!ObjectAuthoringEval_ApplyBodyToLayout(layout, body)) {
            ObjectAuthoringEval_SetDiag(diagnostics,
                                        OBJECT_AUTHORING_EVAL_MISSING_BODY,
                                        0u,
                                        "failed to apply evaluated body to layout");
            return false;
        }
        if (next_body_id <= body->bodyId) {
            next_body_id = body->bodyId + 1u;
        }
    }
    layout->objectStore.nextObjectId = next_body_id;
    ObjectAuthoringEval_SetDiag(diagnostics, OBJECT_AUTHORING_EVAL_OK, 0u, "");
    return true;
}
