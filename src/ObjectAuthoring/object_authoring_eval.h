#pragma once

#include "ObjectAuthoring/object_authoring_document.h"

#include <stdbool.h>

typedef enum {
    OBJECT_AUTHORING_EVAL_OK = 0,
    OBJECT_AUTHORING_EVAL_INVALID_ARGUMENT = 1,
    OBJECT_AUTHORING_EVAL_UNSUPPORTED_OPERATION = 2,
    OBJECT_AUTHORING_EVAL_MISSING_BODY = 3,
    OBJECT_AUTHORING_EVAL_MISSING_RESULT = 4,
    OBJECT_AUTHORING_EVAL_MISSING_FACE = 5,
    OBJECT_AUTHORING_EVAL_STALE_FACE_REF = 6
} ObjectAuthoringEvalStatus;

typedef struct {
    ObjectAuthoringEvalStatus status;
    ObjectAuthoringOperationId failedOperationId;
    char message[128];
} ObjectAuthoringEvalDiagnostics;

bool ObjectAuthoring_EvaluateDocument(const ObjectAuthoringDocument* source,
                                      ObjectAuthoringDocument* evaluated,
                                      ObjectAuthoringEvalDiagnostics* diagnostics);
bool ObjectAuthoring_ApplyEvaluatedDocumentToLayout(
    const ObjectAuthoringDocument* evaluated,
    Layout* layout,
    ObjectAuthoringEvalDiagnostics* diagnostics);
