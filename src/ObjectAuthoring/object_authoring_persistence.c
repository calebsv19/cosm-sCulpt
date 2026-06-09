#include "ObjectAuthoring/object_authoring_persistence.h"

#include "cjson/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int kObjectAuthoringSchemaVersion = 1;
static const char* kLineDrawingExtensionKey = "line_drawing";
static const char* kObjectAuthoringExtensionKey = "object_authoring_v1";

static void ObjectAuthoringPersistence_SetDiagnostics(char* diagnostics,
                                                      size_t diagnostics_size,
                                                      const char* message) {
    if (!diagnostics || diagnostics_size == 0u) return;
    snprintf(diagnostics, diagnostics_size, "%s", message ? message : "");
}

static char* ObjectAuthoringPersistence_ReadFile(const char* path) {
    FILE* f = NULL;
    long len = 0;
    char* text = NULL;
    if (!path || !path[0]) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    len = ftell(f);
    if (len < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    text = (char*)malloc((size_t)len + 1u);
    if (!text) {
        fclose(f);
        return NULL;
    }
    if (fread(text, 1u, (size_t)len, f) != (size_t)len) {
        free(text);
        fclose(f);
        return NULL;
    }
    text[len] = '\0';
    fclose(f);
    return text;
}

static bool ObjectAuthoringPersistence_WriteFile(const char* path, const char* text) {
    FILE* f = NULL;
    if (!path || !path[0] || !text) return false;
    f = fopen(path, "wb");
    if (!f) return false;
    if (fwrite(text, 1u, strlen(text), f) != strlen(text)) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

static char* ObjectAuthoringPersistence_PrintJson(const cJSON* root) {
    size_t capacity = 65536u;
    const size_t max_capacity = 4u * 1024u * 1024u;
    if (!root) return NULL;
    while (capacity <= max_capacity) {
        char* text = (char*)malloc(capacity);
        if (!text) return NULL;
        if (cJSON_PrintPreallocated((cJSON*)root, text, (int)capacity, cJSON_True)) {
            return text;
        }
        free(text);
        capacity *= 2u;
    }
    return NULL;
}

static cJSON* ObjectAuthoringPersistence_Vec2ToJson(Vec2 v) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) return NULL;
    cJSON_AddNumberToObject(obj, "x", v.x);
    cJSON_AddNumberToObject(obj, "y", v.y);
    return obj;
}

static bool ObjectAuthoringPersistence_Vec2FromJson(const cJSON* node, Vec2* out) {
    const cJSON* x = NULL;
    const cJSON* y = NULL;
    if (!cJSON_IsObject(node) || !out) return false;
    x = cJSON_GetObjectItemCaseSensitive(node, "x");
    y = cJSON_GetObjectItemCaseSensitive(node, "y");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y)) return false;
    out->x = (float)x->valuedouble;
    out->y = (float)y->valuedouble;
    return true;
}

static cJSON* ObjectAuthoringPersistence_Vec3ToJson(Vec3 v) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) return NULL;
    cJSON_AddNumberToObject(obj, "x", v.x);
    cJSON_AddNumberToObject(obj, "y", v.y);
    cJSON_AddNumberToObject(obj, "z", v.z);
    return obj;
}

static bool ObjectAuthoringPersistence_Vec3FromJson(const cJSON* node, Vec3* out) {
    const cJSON* x = NULL;
    const cJSON* y = NULL;
    const cJSON* z = NULL;
    if (!cJSON_IsObject(node) || !out) return false;
    x = cJSON_GetObjectItemCaseSensitive(node, "x");
    y = cJSON_GetObjectItemCaseSensitive(node, "y");
    z = cJSON_GetObjectItemCaseSensitive(node, "z");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(z)) return false;
    out->x = (float)x->valuedouble;
    out->y = (float)y->valuedouble;
    out->z = (float)z->valuedouble;
    return true;
}

static cJSON* ObjectAuthoringPersistence_FrameToJson(PlaneFrame3 frame) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) return NULL;
    cJSON_AddItemToObject(obj, "origin",
                          ObjectAuthoringPersistence_Vec3ToJson(frame.origin));
    cJSON_AddItemToObject(obj, "axis_u",
                          ObjectAuthoringPersistence_Vec3ToJson(frame.axisU));
    cJSON_AddItemToObject(obj, "axis_v",
                          ObjectAuthoringPersistence_Vec3ToJson(frame.axisV));
    cJSON_AddItemToObject(obj, "normal",
                          ObjectAuthoringPersistence_Vec3ToJson(frame.normal));
    return obj;
}

static bool ObjectAuthoringPersistence_FrameFromJson(const cJSON* node,
                                                     PlaneFrame3* out) {
    if (!cJSON_IsObject(node) || !out) return false;
    return ObjectAuthoringPersistence_Vec3FromJson(
               cJSON_GetObjectItemCaseSensitive(node, "origin"), &out->origin) &&
           ObjectAuthoringPersistence_Vec3FromJson(
               cJSON_GetObjectItemCaseSensitive(node, "axis_u"), &out->axisU) &&
           ObjectAuthoringPersistence_Vec3FromJson(
               cJSON_GetObjectItemCaseSensitive(node, "axis_v"), &out->axisV) &&
           ObjectAuthoringPersistence_Vec3FromJson(
               cJSON_GetObjectItemCaseSensitive(node, "normal"), &out->normal);
}

static cJSON* ObjectAuthoringPersistence_TransformToJson(Transform3D transform) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) return NULL;
    cJSON_AddItemToObject(obj, "position",
                          ObjectAuthoringPersistence_Vec3ToJson(transform.position));
    cJSON_AddItemToObject(obj, "rotation_deg",
                          ObjectAuthoringPersistence_Vec3ToJson(transform.rotationDeg));
    cJSON_AddItemToObject(obj, "scale",
                          ObjectAuthoringPersistence_Vec3ToJson(transform.scale));
    return obj;
}

static bool ObjectAuthoringPersistence_TransformFromJson(const cJSON* node,
                                                         Transform3D* out) {
    if (!cJSON_IsObject(node) || !out) return false;
    return ObjectAuthoringPersistence_Vec3FromJson(
               cJSON_GetObjectItemCaseSensitive(node, "position"), &out->position) &&
           ObjectAuthoringPersistence_Vec3FromJson(
               cJSON_GetObjectItemCaseSensitive(node, "rotation_deg"),
               &out->rotationDeg) &&
           ObjectAuthoringPersistence_Vec3FromJson(
               cJSON_GetObjectItemCaseSensitive(node, "scale"), &out->scale);
}

static cJSON* ObjectAuthoringPersistence_FaceRefToJson(ObjectAuthoringFaceRef face) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) return NULL;
    cJSON_AddNumberToObject(obj, "body_id", face.bodyId);
    cJSON_AddNumberToObject(obj, "face_id", face.faceId);
    cJSON_AddNumberToObject(obj, "primitive_face", face.primitiveFace);
    return obj;
}

static bool ObjectAuthoringPersistence_FaceRefFromJson(const cJSON* node,
                                                       ObjectAuthoringFaceRef* out) {
    const cJSON* body_id = NULL;
    const cJSON* face_id = NULL;
    const cJSON* primitive_face = NULL;
    if (!cJSON_IsObject(node) || !out) return false;
    body_id = cJSON_GetObjectItemCaseSensitive(node, "body_id");
    face_id = cJSON_GetObjectItemCaseSensitive(node, "face_id");
    primitive_face = cJSON_GetObjectItemCaseSensitive(node, "primitive_face");
    if (!cJSON_IsNumber(body_id) || !cJSON_IsNumber(primitive_face)) return false;
    out->bodyId = (ObjectAuthoringBodyId)body_id->valuedouble;
    out->primitiveFace = (Object3DFaceKind)primitive_face->valuedouble;
    out->faceId = cJSON_IsNumber(face_id)
        ? (ObjectAuthoringFaceId)face_id->valuedouble
        : ObjectAuthoringFaceId_FromPrimitive(out->bodyId, out->primitiveFace);
    return true;
}

static cJSON* ObjectAuthoringPersistence_BodyToJson(const ObjectAuthoringBody* body) {
    cJSON* obj = cJSON_CreateObject();
    cJSON* plane = NULL;
    cJSON* rect_prism = NULL;
    if (!obj || !body) return obj;
    cJSON_AddNumberToObject(obj, "body_id", body->bodyId);
    cJSON_AddNumberToObject(obj, "source_object_id", body->sourceObjectId);
    cJSON_AddNumberToObject(obj, "authoring_kind", body->authoringKind);
    cJSON_AddNumberToObject(obj, "source_kind", body->sourceKind);
    cJSON_AddItemToObject(obj, "transform",
                          ObjectAuthoringPersistence_TransformToJson(body->transform));

    plane = cJSON_CreateObject();
    cJSON_AddItemToObject(obj, "plane", plane);
    cJSON_AddNumberToObject(plane, "width", body->plane.width);
    cJSON_AddNumberToObject(plane, "height", body->plane.height);
    cJSON_AddItemToObject(plane, "frame",
                          ObjectAuthoringPersistence_FrameToJson(body->plane.frame));
    cJSON_AddBoolToObject(plane, "lock_to_construction_plane",
                          body->plane.lockToConstructionPlane);
    cJSON_AddBoolToObject(plane, "lock_to_bounds", body->plane.lockToBounds);

    rect_prism = cJSON_CreateObject();
    cJSON_AddItemToObject(obj, "rect_prism", rect_prism);
    cJSON_AddNumberToObject(rect_prism, "width", body->rectPrism.width);
    cJSON_AddNumberToObject(rect_prism, "height", body->rectPrism.height);
    cJSON_AddNumberToObject(rect_prism, "depth", body->rectPrism.depth);
    cJSON_AddItemToObject(rect_prism, "frame",
                          ObjectAuthoringPersistence_FrameToJson(body->rectPrism.frame));
    cJSON_AddBoolToObject(rect_prism, "lock_to_construction_plane",
                          body->rectPrism.lockToConstructionPlane);
    cJSON_AddBoolToObject(rect_prism, "lock_to_bounds", body->rectPrism.lockToBounds);
    return obj;
}

static bool ObjectAuthoringPersistence_BodyFromJson(const cJSON* node,
                                                    ObjectAuthoringBody* out) {
    const cJSON* plane = NULL;
    const cJSON* rect_prism = NULL;
    if (!cJSON_IsObject(node) || !out) return false;
    memset(out, 0, sizeof(*out));
    out->bodyId = (ObjectAuthoringBodyId)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(node, "body_id"));
    out->sourceObjectId = (uint32_t)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(node, "source_object_id"));
    out->authoringKind = (ObjectAuthoringBodyKind)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(node, "authoring_kind"));
    out->sourceKind = (Object3DKind)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(node, "source_kind"));
    if (out->bodyId == 0u || out->sourceKind == OBJECT3D_KIND_UNKNOWN) return false;
    if (!ObjectAuthoringPersistence_TransformFromJson(
            cJSON_GetObjectItemCaseSensitive(node, "transform"), &out->transform)) {
        return false;
    }

    plane = cJSON_GetObjectItemCaseSensitive(node, "plane");
    rect_prism = cJSON_GetObjectItemCaseSensitive(node, "rect_prism");
    if (cJSON_IsObject(plane)) {
        out->plane.width = (float)cJSON_GetNumberValue(
            cJSON_GetObjectItemCaseSensitive(plane, "width"));
        out->plane.height = (float)cJSON_GetNumberValue(
            cJSON_GetObjectItemCaseSensitive(plane, "height"));
        if (!ObjectAuthoringPersistence_FrameFromJson(
                cJSON_GetObjectItemCaseSensitive(plane, "frame"), &out->plane.frame)) {
            return false;
        }
        out->plane.lockToConstructionPlane =
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(plane,
                                                          "lock_to_construction_plane"));
        out->plane.lockToBounds =
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(plane, "lock_to_bounds"));
    }
    if (cJSON_IsObject(rect_prism)) {
        out->rectPrism.width = (float)cJSON_GetNumberValue(
            cJSON_GetObjectItemCaseSensitive(rect_prism, "width"));
        out->rectPrism.height = (float)cJSON_GetNumberValue(
            cJSON_GetObjectItemCaseSensitive(rect_prism, "height"));
        out->rectPrism.depth = (float)cJSON_GetNumberValue(
            cJSON_GetObjectItemCaseSensitive(rect_prism, "depth"));
        if (!ObjectAuthoringPersistence_FrameFromJson(
                cJSON_GetObjectItemCaseSensitive(rect_prism, "frame"),
                &out->rectPrism.frame)) {
            return false;
        }
        out->rectPrism.lockToConstructionPlane =
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(rect_prism,
                                                          "lock_to_construction_plane"));
        out->rectPrism.lockToBounds =
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(rect_prism, "lock_to_bounds"));
    }
    return true;
}

static cJSON* ObjectAuthoringPersistence_SketchToJson(
    const ObjectAuthoringSketch* sketch) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj || !sketch) return obj;
    cJSON_AddNumberToObject(obj, "sketch_id", sketch->sketchId);
    cJSON_AddItemToObject(obj, "face_ref",
                          ObjectAuthoringPersistence_FaceRefToJson(sketch->faceRef));
    cJSON_AddItemToObject(obj, "frame",
                          ObjectAuthoringPersistence_FrameToJson(sketch->frame));
    cJSON_AddItemToObject(obj, "min_uv",
                          ObjectAuthoringPersistence_Vec2ToJson(sketch->minUV));
    cJSON_AddItemToObject(obj, "max_uv",
                          ObjectAuthoringPersistence_Vec2ToJson(sketch->maxUV));
    cJSON_AddNumberToObject(obj, "operation_id", sketch->operationId);
    cJSON_AddBoolToObject(obj, "active", sketch->active);
    return obj;
}

static bool ObjectAuthoringPersistence_SketchFromJson(const cJSON* node,
                                                      ObjectAuthoringSketch* out) {
    if (!cJSON_IsObject(node) || !out) return false;
    memset(out, 0, sizeof(*out));
    out->sketchId = (ObjectAuthoringSketchId)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(node, "sketch_id"));
    out->operationId = (ObjectAuthoringOperationId)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(node, "operation_id"));
    out->active = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(node, "active"));
    return out->sketchId != 0u &&
           ObjectAuthoringPersistence_FaceRefFromJson(
               cJSON_GetObjectItemCaseSensitive(node, "face_ref"), &out->faceRef) &&
           ObjectAuthoringPersistence_FrameFromJson(
               cJSON_GetObjectItemCaseSensitive(node, "frame"), &out->frame) &&
           ObjectAuthoringPersistence_Vec2FromJson(
               cJSON_GetObjectItemCaseSensitive(node, "min_uv"), &out->minUV) &&
           ObjectAuthoringPersistence_Vec2FromJson(
               cJSON_GetObjectItemCaseSensitive(node, "max_uv"), &out->maxUV);
}

static cJSON* ObjectAuthoringPersistence_OperationToJson(
    const ObjectAuthoringOperation* operation) {
    cJSON* obj = cJSON_CreateObject();
    cJSON* ids = NULL;
    cJSON* bodies = NULL;
    if (!obj || !operation) return obj;
    cJSON_AddNumberToObject(obj, "operation_id", operation->operationId);
    cJSON_AddNumberToObject(obj, "kind", operation->kind);
    cJSON_AddItemToObject(obj, "face_ref",
                          ObjectAuthoringPersistence_FaceRefToJson(operation->faceRef));
    cJSON_AddNumberToObject(obj, "sketch_id", operation->sketchId);
    cJSON_AddItemToObject(obj, "body_snapshot",
                          ObjectAuthoringPersistence_BodyToJson(&operation->bodySnapshot));
    cJSON_AddItemToObject(obj, "sketch_snapshot",
                          ObjectAuthoringPersistence_SketchToJson(&operation->sketchSnapshot));
    cJSON_AddNumberToObject(obj, "depth", operation->depth);
    cJSON_AddNumberToObject(obj, "result_body_count", operation->resultBodyCount);
    ids = cJSON_CreateArray();
    bodies = cJSON_CreateArray();
    cJSON_AddItemToObject(obj, "result_body_ids", ids);
    cJSON_AddItemToObject(obj, "result_bodies", bodies);
    for (size_t i = 0; i < operation->resultBodyCount && i < 8u; ++i) {
        cJSON_AddItemToArray(ids, cJSON_CreateNumber(operation->resultBodyIds[i]));
        cJSON_AddItemToArray(bodies,
                             ObjectAuthoringPersistence_BodyToJson(&operation->resultBodies[i]));
    }
    return obj;
}

static bool ObjectAuthoringPersistence_OperationFromJson(
    const cJSON* node,
    ObjectAuthoringOperation* out) {
    const cJSON* ids = NULL;
    const cJSON* bodies = NULL;
    if (!cJSON_IsObject(node) || !out) return false;
    memset(out, 0, sizeof(*out));
    out->operationId = (ObjectAuthoringOperationId)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(node, "operation_id"));
    out->kind = (ObjectAuthoringOperationKind)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(node, "kind"));
    out->sketchId = (ObjectAuthoringSketchId)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(node, "sketch_id"));
    out->depth = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(node, "depth"));
    out->resultBodyCount = (size_t)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(node, "result_body_count"));
    if (out->resultBodyCount > 8u) return false;
    if (out->operationId == 0u || out->kind == OBJECT_AUTHORING_OPERATION_NONE) {
        return false;
    }
    if (!ObjectAuthoringPersistence_FaceRefFromJson(
            cJSON_GetObjectItemCaseSensitive(node, "face_ref"), &out->faceRef)) {
        return false;
    }
    (void)ObjectAuthoringPersistence_BodyFromJson(
        cJSON_GetObjectItemCaseSensitive(node, "body_snapshot"), &out->bodySnapshot);
    (void)ObjectAuthoringPersistence_SketchFromJson(
        cJSON_GetObjectItemCaseSensitive(node, "sketch_snapshot"), &out->sketchSnapshot);
    ids = cJSON_GetObjectItemCaseSensitive(node, "result_body_ids");
    bodies = cJSON_GetObjectItemCaseSensitive(node, "result_bodies");
    if (!cJSON_IsArray(ids) || !cJSON_IsArray(bodies)) return false;
    for (size_t i = 0; i < out->resultBodyCount; ++i) {
        const cJSON* id = cJSON_GetArrayItem(ids, (int)i);
        const cJSON* body = cJSON_GetArrayItem(bodies, (int)i);
        if (!cJSON_IsNumber(id) ||
            !ObjectAuthoringPersistence_BodyFromJson(body, &out->resultBodies[i])) {
            return false;
        }
        out->resultBodyIds[i] = (uint32_t)id->valuedouble;
    }
    return true;
}

static cJSON* ObjectAuthoringPersistence_DocumentToJson(
    const ObjectAuthoringDocument* doc) {
    cJSON* root = cJSON_CreateObject();
    cJSON* bodies = cJSON_CreateArray();
    cJSON* sketches = cJSON_CreateArray();
    cJSON* operations = cJSON_CreateArray();
    if (!root || !bodies || !sketches || !operations || !doc) {
        cJSON_Delete(root);
        cJSON_Delete(bodies);
        cJSON_Delete(sketches);
        cJSON_Delete(operations);
        return NULL;
    }
    cJSON_AddNumberToObject(root, "schema_version", kObjectAuthoringSchemaVersion);
    cJSON_AddNumberToObject(root, "next_sketch_id", doc->nextSketchId);
    cJSON_AddNumberToObject(root, "next_operation_id", doc->nextOperationId);
    cJSON_AddItemToObject(root, "selected_face",
                          ObjectAuthoringPersistence_FaceRefToJson(doc->selectedFace));
    cJSON_AddNumberToObject(root, "selected_sketch_id", doc->selectedSketchId);
    cJSON_AddNumberToObject(root, "selected_operation_id", doc->selectedOperationId);
    cJSON_AddItemToObject(root, "bodies", bodies);
    cJSON_AddItemToObject(root, "sketches", sketches);
    cJSON_AddItemToObject(root, "operations", operations);
    for (size_t i = 0; i < doc->bodyCount; ++i) {
        cJSON_AddItemToArray(bodies,
                             ObjectAuthoringPersistence_BodyToJson(&doc->bodies[i]));
    }
    for (size_t i = 0; i < doc->sketchCount; ++i) {
        cJSON_AddItemToArray(sketches,
                             ObjectAuthoringPersistence_SketchToJson(&doc->sketches[i]));
    }
    for (size_t i = 0; i < doc->operationCount; ++i) {
        cJSON_AddItemToArray(operations,
                             ObjectAuthoringPersistence_OperationToJson(&doc->operations[i]));
    }
    return root;
}

static bool ObjectAuthoringPersistence_DocumentFromJson(const cJSON* root,
                                                        ObjectAuthoringDocument* out) {
    ObjectAuthoringDocument temp;
    const cJSON* bodies = NULL;
    const cJSON* sketches = NULL;
    const cJSON* operations = NULL;
    if (!cJSON_IsObject(root) || !out) return false;
    if ((int)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(root,
                                                                   "schema_version")) !=
        kObjectAuthoringSchemaVersion) {
        return false;
    }

    ObjectAuthoringDocument_Init(&temp);
    bodies = cJSON_GetObjectItemCaseSensitive(root, "bodies");
    sketches = cJSON_GetObjectItemCaseSensitive(root, "sketches");
    operations = cJSON_GetObjectItemCaseSensitive(root, "operations");
    if (!cJSON_IsArray(bodies) || !cJSON_IsArray(sketches) ||
        !cJSON_IsArray(operations)) {
        ObjectAuthoringDocument_Free(&temp);
        return false;
    }
    for (int i = 0; i < cJSON_GetArraySize(bodies); ++i) {
        ObjectAuthoringBody body;
        if (!ObjectAuthoringPersistence_BodyFromJson(cJSON_GetArrayItem(bodies, i),
                                                     &body) ||
            !ObjectAuthoringDocument_UpsertBody(&temp, &body)) {
            ObjectAuthoringDocument_Free(&temp);
            return false;
        }
    }
    for (int i = 0; i < cJSON_GetArraySize(sketches); ++i) {
        ObjectAuthoringSketch sketch;
        if (!ObjectAuthoringPersistence_SketchFromJson(cJSON_GetArrayItem(sketches, i),
                                                       &sketch) ||
            !ObjectAuthoringDocument_AppendSketchSnapshot(&temp, &sketch)) {
            ObjectAuthoringDocument_Free(&temp);
            return false;
        }
    }
    for (int i = 0; i < cJSON_GetArraySize(operations); ++i) {
        ObjectAuthoringOperation operation;
        if (!ObjectAuthoringPersistence_OperationFromJson(cJSON_GetArrayItem(operations, i),
                                                          &operation) ||
            !ObjectAuthoringDocument_AppendOperationSnapshot(&temp, &operation)) {
            ObjectAuthoringDocument_Free(&temp);
            return false;
        }
    }
    temp.nextSketchId = (ObjectAuthoringSketchId)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(root, "next_sketch_id"));
    temp.nextOperationId = (ObjectAuthoringOperationId)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(root, "next_operation_id"));
    (void)ObjectAuthoringPersistence_FaceRefFromJson(
        cJSON_GetObjectItemCaseSensitive(root, "selected_face"), &temp.selectedFace);
    temp.selectedSketchId = (ObjectAuthoringSketchId)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(root, "selected_sketch_id"));
    temp.selectedOperationId = (ObjectAuthoringOperationId)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(root, "selected_operation_id"));

    ObjectAuthoringDocument_Clear(out);
    if (!ObjectAuthoringDocument_Copy(out, &temp)) {
        ObjectAuthoringDocument_Free(&temp);
        return false;
    }
    ObjectAuthoringDocument_Free(&temp);
    return true;
}

static cJSON* ObjectAuthoringPersistence_EnsureObject(cJSON* parent, const char* key) {
    cJSON* child = NULL;
    if (!parent || !key) return NULL;
    child = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (cJSON_IsObject(child)) return child;
    if (child) {
        cJSON_DeleteItemFromObjectCaseSensitive(parent, key);
    }
    child = cJSON_CreateObject();
    if (!child) return NULL;
    cJSON_AddItemToObject(parent, key, child);
    return child;
}

bool ObjectAuthoringDocument_SaveExtensionToFile(const ObjectAuthoringDocument* doc,
                                                 const char* path,
                                                 char* diagnostics,
                                                 size_t diagnostics_size) {
    char* text = NULL;
    char* printed = NULL;
    cJSON* root = NULL;
    cJSON* extensions = NULL;
    cJSON* line_drawing = NULL;
    cJSON* object_authoring = NULL;
    bool ok = false;
    ObjectAuthoringPersistence_SetDiagnostics(diagnostics, diagnostics_size, "");
    if (!doc || !path || !path[0]) {
        ObjectAuthoringPersistence_SetDiagnostics(diagnostics,
                                                  diagnostics_size,
                                                  "invalid object authoring save arguments");
        return false;
    }

    text = ObjectAuthoringPersistence_ReadFile(path);
    root = text ? cJSON_Parse(text) : NULL;
    free(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        ObjectAuthoringPersistence_SetDiagnostics(diagnostics,
                                                  diagnostics_size,
                                                  "failed to reopen object asset JSON");
        return false;
    }
    extensions = ObjectAuthoringPersistence_EnsureObject(root, "extensions");
    line_drawing = ObjectAuthoringPersistence_EnsureObject(extensions,
                                                           kLineDrawingExtensionKey);
    object_authoring = ObjectAuthoringPersistence_DocumentToJson(doc);
    if (!extensions || !line_drawing || !object_authoring) {
        cJSON_Delete(root);
        ObjectAuthoringPersistence_SetDiagnostics(diagnostics,
                                                  diagnostics_size,
                                                  "failed to create object authoring JSON");
        return false;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(line_drawing, kObjectAuthoringExtensionKey);
    cJSON_AddItemToObject(line_drawing, kObjectAuthoringExtensionKey, object_authoring);
    printed = ObjectAuthoringPersistence_PrintJson(root);
    ok = printed && ObjectAuthoringPersistence_WriteFile(path, printed);
    free(printed);
    cJSON_Delete(root);
    if (!ok) {
        ObjectAuthoringPersistence_SetDiagnostics(diagnostics,
                                                  diagnostics_size,
                                                  "failed to write object authoring JSON");
    }
    return ok;
}

bool ObjectAuthoringDocument_LoadExtensionFromFile(ObjectAuthoringDocument* doc,
                                                   const char* path,
                                                   bool* out_found,
                                                   char* diagnostics,
                                                   size_t diagnostics_size) {
    char* text = NULL;
    cJSON* root = NULL;
    const cJSON* extensions = NULL;
    const cJSON* line_drawing = NULL;
    const cJSON* object_authoring = NULL;
    bool ok = false;
    if (out_found) *out_found = false;
    ObjectAuthoringPersistence_SetDiagnostics(diagnostics, diagnostics_size, "");
    if (!doc || !path || !path[0]) {
        ObjectAuthoringPersistence_SetDiagnostics(diagnostics,
                                                  diagnostics_size,
                                                  "invalid object authoring load arguments");
        return false;
    }
    text = ObjectAuthoringPersistence_ReadFile(path);
    root = text ? cJSON_Parse(text) : NULL;
    free(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        ObjectAuthoringPersistence_SetDiagnostics(diagnostics,
                                                  diagnostics_size,
                                                  "failed to parse object asset JSON");
        return false;
    }
    extensions = cJSON_GetObjectItemCaseSensitive(root, "extensions");
    line_drawing = cJSON_IsObject(extensions)
        ? cJSON_GetObjectItemCaseSensitive(extensions, kLineDrawingExtensionKey)
        : NULL;
    object_authoring = cJSON_IsObject(line_drawing)
        ? cJSON_GetObjectItemCaseSensitive(line_drawing, kObjectAuthoringExtensionKey)
        : NULL;
    if (!object_authoring) {
        cJSON_Delete(root);
        return true;
    }
    if (out_found) *out_found = true;
    ok = ObjectAuthoringPersistence_DocumentFromJson(object_authoring, doc);
    cJSON_Delete(root);
    if (!ok) {
        ObjectAuthoringPersistence_SetDiagnostics(diagnostics,
                                                  diagnostics_size,
                                                  "invalid object authoring extension");
    }
    return ok;
}
