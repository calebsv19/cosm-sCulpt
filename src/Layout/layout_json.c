#include "Layout/layout_json.h"
#include "Core/global_state.h"
#include "core_io.h"
#include "core_scene.h"
#include "cjson/cJSON.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

static const char* LAYOUT_JSON_GENERATOR = "LineDrawing";

static bool Layout_PathIsPack(const char* path) {
    if (!path || !path[0]) return false;
    const char* dot = strrchr(path, '.');
    if (!dot) return false;
    return strcasecmp(dot, ".pack") == 0;
}

static const char* HandleAxis_ToString(ViewPlaneAxis axis) {
    switch (axis) {
        case VIEW_PLANE_YZ: return "yz";
        case VIEW_PLANE_XZ: return "xz";
        case VIEW_PLANE_XY:
        default: return "xy";
    }
}

static ViewPlaneAxis HandleAxis_FromJson(const cJSON* node) {
    if (!cJSON_IsString(node) || !node->valuestring) return VIEW_PLANE_XY;
    if (strcasecmp(node->valuestring, "yz") == 0) return VIEW_PLANE_YZ;
    if (strcasecmp(node->valuestring, "xz") == 0) return VIEW_PLANE_XZ;
    return VIEW_PLANE_XY;
}

static const char* ConstructionPlaneMode_ToString(ConstructionPlaneMode mode) {
    switch (mode) {
        case CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME: return "custom_frame";
        case CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED:
        default: return "axis_aligned";
    }
}

static ConstructionPlaneMode ConstructionPlaneMode_FromJson(const cJSON* node) {
    if (!cJSON_IsString(node) || !node->valuestring) {
        return CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED;
    }
    if (strcasecmp(node->valuestring, "custom_frame") == 0) {
        return CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME;
    }
    return CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED;
}

static const char* Object3DKind_ToString(Object3DKind kind) {
    switch (kind) {
        case OBJECT3D_KIND_PLANE: return "plane";
        case OBJECT3D_KIND_RECT_PRISM: return "rect_prism";
        case OBJECT3D_KIND_UNKNOWN:
        default: return "unknown";
    }
}

static Object3DKind Object3DKind_FromJson(const cJSON* node) {
    if (!cJSON_IsString(node) || !node->valuestring) return OBJECT3D_KIND_UNKNOWN;
    if (strcasecmp(node->valuestring, "plane") == 0) return OBJECT3D_KIND_PLANE;
    if (strcasecmp(node->valuestring, "rect_prism") == 0) return OBJECT3D_KIND_RECT_PRISM;
    return OBJECT3D_KIND_UNKNOWN;
}

static const char* CoreDimensionalMode_ToString(CoreObjectDimensionalMode mode) {
    switch (mode) {
        case CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D: return "full_3d";
        case CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED:
        default: return "plane_locked";
    }
}

static CoreObjectDimensionalMode CoreDimensionalMode_FromJson(const cJSON* node) {
    if (!cJSON_IsString(node) || !node->valuestring) return CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    if (strcasecmp(node->valuestring, "full_3d") == 0) return CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
    return CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
}

static const char* CorePlane_ToString(CoreObjectPlane plane) {
    switch (plane) {
        case CORE_OBJECT_PLANE_YZ: return "yz";
        case CORE_OBJECT_PLANE_XZ: return "xz";
        case CORE_OBJECT_PLANE_XY:
        default: return "xy";
    }
}

static CoreObjectPlane CorePlane_FromJson(const cJSON* node) {
    if (!cJSON_IsString(node) || !node->valuestring) return CORE_OBJECT_PLANE_XY;
    if (strcasecmp(node->valuestring, "yz") == 0) return CORE_OBJECT_PLANE_YZ;
    if (strcasecmp(node->valuestring, "xz") == 0) return CORE_OBJECT_PLANE_XZ;
    return CORE_OBJECT_PLANE_XY;
}

static cJSON* Vec3_ToJsonObject(Vec3 v) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) return NULL;
    cJSON_AddNumberToObject(obj, "x", v.x);
    cJSON_AddNumberToObject(obj, "y", v.y);
    cJSON_AddNumberToObject(obj, "z", v.z);
    return obj;
}

static bool Vec3_FromJsonObject(const cJSON* node, Vec3* out) {
    if (!cJSON_IsObject(node) || !out) return false;
    const cJSON* x = cJSON_GetObjectItem(node, "x");
    const cJSON* y = cJSON_GetObjectItem(node, "y");
    const cJSON* z = cJSON_GetObjectItem(node, "z");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(z)) return false;
    out->x = (float)x->valuedouble;
    out->y = (float)y->valuedouble;
    out->z = (float)z->valuedouble;
    return true;
}

static cJSON* Transform3D_ToJsonObject(Transform3D t) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) return NULL;
    cJSON_AddItemToObject(obj, "position", Vec3_ToJsonObject(t.position));
    cJSON_AddItemToObject(obj, "rotationDeg", Vec3_ToJsonObject(t.rotationDeg));
    cJSON_AddItemToObject(obj, "scale", Vec3_ToJsonObject(t.scale));
    return obj;
}

static bool Transform3D_FromJsonObject(const cJSON* node, Transform3D* out) {
    if (!cJSON_IsObject(node) || !out) return false;
    Vec3 pos = {0};
    Vec3 rot = {0};
    Vec3 scale = {0};
    if (!Vec3_FromJsonObject(cJSON_GetObjectItem(node, "position"), &pos)) return false;
    if (!Vec3_FromJsonObject(cJSON_GetObjectItem(node, "rotationDeg"), &rot)) return false;
    if (!Vec3_FromJsonObject(cJSON_GetObjectItem(node, "scale"), &scale)) return false;
    out->position = pos;
    out->rotationDeg = rot;
    out->scale = scale;
    return true;
}

static cJSON* Layout_CreateJson(const Layout* layout) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON* file = cJSON_CreateObject();
    if (!file) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "file", file);
    cJSON_AddNumberToObject(file, "schemaVersion", LAYOUT_JSON_SCHEMA_VERSION);
    cJSON_AddStringToObject(file, "generator", LAYOUT_JSON_GENERATOR);
    cJSON_AddNumberToObject(file, "gridSize", layout->gridSize);

    cJSON* scene3d = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "scene3d", scene3d);
    cJSON* bounds = cJSON_CreateObject();
    cJSON_AddItemToObject(scene3d, "bounds", bounds);
    cJSON_AddBoolToObject(bounds, "enabled", layout->scene3d.bounds.enabled);
    cJSON_AddBoolToObject(bounds, "clampOnEdit", layout->scene3d.bounds.clampOnEdit);
    cJSON_AddItemToObject(bounds, "min", Vec3_ToJsonObject(layout->scene3d.bounds.min));
    cJSON_AddItemToObject(bounds, "max", Vec3_ToJsonObject(layout->scene3d.bounds.max));

    cJSON* constructionPlane = cJSON_CreateObject();
    cJSON_AddItemToObject(scene3d, "constructionPlane", constructionPlane);
    cJSON_AddStringToObject(constructionPlane,
                            "mode",
                            ConstructionPlaneMode_ToString(layout->scene3d.constructionPlane.mode));
    cJSON_AddStringToObject(constructionPlane,
                            "axis",
                            HandleAxis_ToString(layout->scene3d.constructionPlane.axisAligned.axis));
    cJSON_AddNumberToObject(constructionPlane, "offset", layout->scene3d.constructionPlane.axisAligned.offset);
    cJSON* customFrame = cJSON_CreateObject();
    cJSON_AddItemToObject(constructionPlane, "customFrame", customFrame);
    cJSON_AddItemToObject(customFrame, "origin", Vec3_ToJsonObject(layout->scene3d.constructionPlane.customFrame.origin));
    cJSON_AddItemToObject(customFrame, "axisU", Vec3_ToJsonObject(layout->scene3d.constructionPlane.customFrame.axisU));
    cJSON_AddItemToObject(customFrame, "axisV", Vec3_ToJsonObject(layout->scene3d.constructionPlane.customFrame.axisV));
    cJSON_AddItemToObject(customFrame, "normal", Vec3_ToJsonObject(layout->scene3d.constructionPlane.customFrame.normal));

    cJSON* objects3d = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "objects3d", objects3d);
    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        if (object->isDeleted) continue;

        cJSON* node = cJSON_CreateObject();
        cJSON_AddItemToArray(objects3d, node);
        cJSON_AddNumberToObject(node, "id", (double)object->objectId);
        cJSON_AddStringToObject(node, "kind", Object3DKind_ToString(object->kind));
        cJSON_AddStringToObject(node, "objectType", object->coreMeta.object_type);
        cJSON_AddStringToObject(node, "dimensionalMode", CoreDimensionalMode_ToString(object->coreMeta.dimensional_mode));
        cJSON_AddStringToObject(node, "lockedPlane", CorePlane_ToString(object->coreMeta.locked_plane));
        cJSON_AddItemToObject(node, "transform", Transform3D_ToJsonObject(object->transform));

        if (object->kind == OBJECT3D_KIND_PLANE) {
            cJSON* plane = cJSON_CreateObject();
            cJSON_AddItemToObject(node, "plane", plane);
            cJSON_AddNumberToObject(plane, "width", object->plane.width);
            cJSON_AddNumberToObject(plane, "height", object->plane.height);
            cJSON_AddBoolToObject(plane, "lockToConstructionPlane", object->plane.lockToConstructionPlane);
            cJSON_AddBoolToObject(plane, "lockToBounds", object->plane.lockToBounds);
            cJSON* frame = cJSON_CreateObject();
            cJSON_AddItemToObject(plane, "frame", frame);
            cJSON_AddItemToObject(frame, "origin", Vec3_ToJsonObject(object->plane.frame.origin));
            cJSON_AddItemToObject(frame, "axisU", Vec3_ToJsonObject(object->plane.frame.axisU));
            cJSON_AddItemToObject(frame, "axisV", Vec3_ToJsonObject(object->plane.frame.axisV));
            cJSON_AddItemToObject(frame, "normal", Vec3_ToJsonObject(object->plane.frame.normal));
        } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            cJSON* rectPrism = cJSON_CreateObject();
            cJSON_AddItemToObject(node, "rectPrism", rectPrism);
            cJSON_AddNumberToObject(rectPrism, "width", object->rectPrism.width);
            cJSON_AddNumberToObject(rectPrism, "height", object->rectPrism.height);
            cJSON_AddNumberToObject(rectPrism, "depth", object->rectPrism.depth);
            cJSON_AddBoolToObject(rectPrism, "lockToConstructionPlane", object->rectPrism.lockToConstructionPlane);
            cJSON_AddBoolToObject(rectPrism, "lockToBounds", object->rectPrism.lockToBounds);
            cJSON* frame = cJSON_CreateObject();
            cJSON_AddItemToObject(rectPrism, "frame", frame);
            cJSON_AddItemToObject(frame, "origin", Vec3_ToJsonObject(object->rectPrism.frame.origin));
            cJSON_AddItemToObject(frame, "axisU", Vec3_ToJsonObject(object->rectPrism.frame.axisU));
            cJSON_AddItemToObject(frame, "axisV", Vec3_ToJsonObject(object->rectPrism.frame.axisV));
            cJSON_AddItemToObject(frame, "normal", Vec3_ToJsonObject(object->rectPrism.frame.normal));
        }
    }

    cJSON* anchors = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "anchors", anchors);
    for (size_t i = 0; i < layout->anchorCount; ++i) {
        const Anchor* a = &layout->anchors[i];
        if (a->isDeleted) continue;

        cJSON* anchor = cJSON_CreateObject();
        cJSON_AddNumberToObject(anchor, "x", a->pos.x);
        cJSON_AddNumberToObject(anchor, "y", a->pos.y);
        cJSON_AddNumberToObject(anchor, "z", a->pos.z);
        cJSON_AddBoolToObject(anchor, "persistent", a->isPersistent);
        cJSON_AddStringToObject(anchor, "type",
                                a->type == ANCHOR_TYPE_CURVE ? "curve" : "corner");
        cJSON_AddBoolToObject(anchor, "handlesLinked", a->handlesLinked);
        cJSON_AddStringToObject(anchor, "handleAxis", HandleAxis_ToString(a->handleAxis));
        cJSON_AddNumberToObject(anchor, "handleInLength", a->handleInLength);
        cJSON_AddNumberToObject(anchor, "handleInAngleDeg", a->handleInAngleDeg);
        cJSON_AddNumberToObject(anchor, "handleOutLength", a->handleOutLength);
        cJSON_AddNumberToObject(anchor, "handleOutAngleDeg", a->handleOutAngleDeg);
        cJSON_AddItemToArray(anchors, anchor);
    }

    cJSON* walls = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "walls", walls);
    for (size_t i = 0; i < layout->wallCount; ++i) {
        const Wall* w = &layout->walls[i];
        if (w->isDeleted) continue;

        cJSON* wall = cJSON_CreateObject();
        cJSON_AddNumberToObject(wall, "a", w->anchorA);
        cJSON_AddNumberToObject(wall, "b", w->anchorB);
        cJSON_AddItemToArray(walls, wall);
    }

    return root;
}

static bool Layout_ApplyJson(Layout* layout, const cJSON* root) {
    if (!cJSON_IsObject(root)) return false;

    float targetGrid = 1.0f;
    int schemaVersion = 0;

    const cJSON* file = cJSON_GetObjectItem(root, "file");
    if (cJSON_IsObject(file)) {
        const cJSON* versionNode = cJSON_GetObjectItem(file, "schemaVersion");
        if (cJSON_IsNumber(versionNode)) schemaVersion = versionNode->valueint;

        const cJSON* gridNode = cJSON_GetObjectItem(file, "gridSize");
        if (cJSON_IsNumber(gridNode)) targetGrid = (float)gridNode->valuedouble;
    }

    if (schemaVersion < LAYOUT_JSON_SCHEMA_VERSION_MIN_SUPPORTED ||
        schemaVersion > LAYOUT_JSON_SCHEMA_VERSION) {
        SDL_Log("[Layout JSON] Unsupported schema version %d (supported %d..%d)",
                schemaVersion,
                LAYOUT_JSON_SCHEMA_VERSION_MIN_SUPPORTED,
                LAYOUT_JSON_SCHEMA_VERSION);
        return false;
    }

    Layout temp;
    Layout_Init(&temp, targetGrid);
    if (schemaVersion >= LAYOUT_JSON_SCHEMA_VERSION_SCENE_BOUNDS) {
        const cJSON* scene3d = cJSON_GetObjectItem(root, "scene3d");
        const cJSON* bounds = cJSON_IsObject(scene3d) ? cJSON_GetObjectItem(scene3d, "bounds") : NULL;
        if (cJSON_IsObject(bounds)) {
            SceneBounds3D parsed = temp.scene3d.bounds;
            const cJSON* enabled = cJSON_GetObjectItem(bounds, "enabled");
            const cJSON* clampOnEdit = cJSON_GetObjectItem(bounds, "clampOnEdit");
            const cJSON* min = cJSON_GetObjectItem(bounds, "min");
            const cJSON* max = cJSON_GetObjectItem(bounds, "max");
            if (cJSON_IsBool(enabled)) parsed.enabled = cJSON_IsTrue(enabled);
            if (cJSON_IsBool(clampOnEdit)) parsed.clampOnEdit = cJSON_IsTrue(clampOnEdit);
            Vec3_FromJsonObject(min, &parsed.min);
            Vec3_FromJsonObject(max, &parsed.max);
            if (Layout_SceneBounds3D_IsValid(&parsed)) {
                temp.scene3d.bounds = parsed;
            } else {
                SDL_Log("[Layout JSON] Invalid scene3d.bounds ignored; defaults kept");
            }
        }

        if (schemaVersion >= LAYOUT_JSON_SCHEMA_VERSION_CONSTRUCTION_PLANE) {
            const cJSON* constructionPlane =
                cJSON_IsObject(scene3d) ? cJSON_GetObjectItem(scene3d, "constructionPlane") : NULL;
            if (cJSON_IsObject(constructionPlane)) {
                ConstructionPlane3D parsed = temp.scene3d.constructionPlane;
                parsed.mode = ConstructionPlaneMode_FromJson(cJSON_GetObjectItem(constructionPlane, "mode"));
                parsed.axisAligned.axis = HandleAxis_FromJson(cJSON_GetObjectItem(constructionPlane, "axis"));
                const cJSON* offset = cJSON_GetObjectItem(constructionPlane, "offset");
                if (cJSON_IsNumber(offset)) {
                    parsed.axisAligned.offset = (float)offset->valuedouble;
                }

                const cJSON* customFrame = cJSON_GetObjectItem(constructionPlane, "customFrame");
                if (cJSON_IsObject(customFrame)) {
                    Vec3_FromJsonObject(cJSON_GetObjectItem(customFrame, "origin"),
                                        &parsed.customFrame.origin);
                    Vec3_FromJsonObject(cJSON_GetObjectItem(customFrame, "axisU"),
                                        &parsed.customFrame.axisU);
                    Vec3_FromJsonObject(cJSON_GetObjectItem(customFrame, "axisV"),
                                        &parsed.customFrame.axisV);
                    Vec3_FromJsonObject(cJSON_GetObjectItem(customFrame, "normal"),
                                        &parsed.customFrame.normal);
                }

                if (Layout_ConstructionPlane3D_IsValid(&parsed)) {
                    temp.scene3d.constructionPlane = parsed;
                } else {
                    SDL_Log("[Layout JSON] Invalid scene3d.constructionPlane ignored; defaults kept");
                }
            }
        }
    }

    const cJSON* anchors = cJSON_GetObjectItem(root, "anchors");
    if (!anchors || !cJSON_IsArray(anchors)) {
        Layout_Free(&temp);
        return false;
    }

    const int numAnchors = cJSON_GetArraySize(anchors);
    for (int i = 0; i < numAnchors; ++i) {
        const cJSON* a = cJSON_GetArrayItem(anchors, i);
        if (!cJSON_IsObject(a)) continue;

        const cJSON* x = cJSON_GetObjectItem(a, "x");
        const cJSON* y = cJSON_GetObjectItem(a, "y");
        const cJSON* z = cJSON_GetObjectItem(a, "z");
        const cJSON* persistent = cJSON_GetObjectItem(a, "persistent");
        const cJSON* typeNode = cJSON_GetObjectItem(a, "type");
        const cJSON* curvedNode = cJSON_GetObjectItem(a, "curved"); // legacy bool
        const cJSON* handlesLinked = cJSON_GetObjectItem(a, "handlesLinked");
        const cJSON* handleAxis = cJSON_GetObjectItem(a, "handleAxis");
        const cJSON* handleInLength = cJSON_GetObjectItem(a, "handleInLength");
        const cJSON* handleInAngle = cJSON_GetObjectItem(a, "handleInAngleDeg");
        const cJSON* handleOutLength = cJSON_GetObjectItem(a, "handleOutLength");
        const cJSON* handleOutAngle = cJSON_GetObjectItem(a, "handleOutAngleDeg");
        if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y)) continue;

        Vec3 pos = {
            .x = (float)x->valuedouble,
            .y = (float)y->valuedouble,
            .z = 0.0f
        };
        if (cJSON_IsNumber(z)) {
            pos.z = (float)z->valuedouble;
        }

        int idx = Layout_AddAnchor3(&temp, pos);
        Anchor* anchor = &temp.anchors[idx];
        anchor->isPersistent = cJSON_IsTrue(persistent);

        AnchorType type = ANCHOR_TYPE_CORNER;
        if (cJSON_IsString(typeNode) && typeNode->valuestring) {
            if (strcasecmp(typeNode->valuestring, "curve") == 0) {
                type = ANCHOR_TYPE_CURVE;
            }
        } else if (cJSON_IsBool(curvedNode) && cJSON_IsTrue(curvedNode)) {
            type = ANCHOR_TYPE_CURVE;
        }
        anchor->type = type;

        if (cJSON_IsBool(handlesLinked)) {
            anchor->handlesLinked = cJSON_IsTrue(handlesLinked);
        }
        anchor->handleAxis = HandleAxis_FromJson(handleAxis);
        if (cJSON_IsNumber(handleInLength)) anchor->handleInLength = (float)handleInLength->valuedouble;
        if (cJSON_IsNumber(handleInAngle))  anchor->handleInAngleDeg = (float)handleInAngle->valuedouble;
        if (cJSON_IsNumber(handleOutLength)) anchor->handleOutLength = (float)handleOutLength->valuedouble;
        if (cJSON_IsNumber(handleOutAngle)) anchor->handleOutAngleDeg = (float)handleOutAngle->valuedouble;
    }

    const cJSON* walls = cJSON_GetObjectItem(root, "walls");
    if (!walls || !cJSON_IsArray(walls)) {
        Layout_Free(&temp);
        return false;
    }

    const int numWalls = cJSON_GetArraySize(walls);
    for (int i = 0; i < numWalls; ++i) {
        const cJSON* w = cJSON_GetArrayItem(walls, i);
        if (!cJSON_IsObject(w)) continue;

        const cJSON* a = cJSON_GetObjectItem(w, "a");
        const cJSON* b = cJSON_GetObjectItem(w, "b");
        if (!cJSON_IsNumber(a) || !cJSON_IsNumber(b)) continue;

        int idxA = a->valueint;
        int idxB = b->valueint;
        if (idxA >= 0 && idxA < (int)temp.anchorCount &&
            idxB >= 0 && idxB < (int)temp.anchorCount) {
            Vec3 posA = temp.anchors[idxA].pos;
            Vec3 posB = temp.anchors[idxB].pos;
            Layout_AddWall3(&temp, posA, posB);
        } else {
            SDL_Log("[Layout JSON] Skipped wall (%d → %d): invalid index", idxA, idxB);
        }
    }

    if (schemaVersion >= LAYOUT_JSON_SCHEMA_VERSION_OBJECT3D) {
        const cJSON* objects3d = cJSON_GetObjectItem(root, "objects3d");
        if (cJSON_IsArray(objects3d)) {
            const int count = cJSON_GetArraySize(objects3d);
            for (int i = 0; i < count; ++i) {
                const cJSON* node = cJSON_GetArrayItem(objects3d, i);
                if (!cJSON_IsObject(node)) continue;

                const Object3DKind kind = Object3DKind_FromJson(cJSON_GetObjectItem(node, "kind"));
                if (kind == OBJECT3D_KIND_UNKNOWN) continue;

                const cJSON* transformNode = cJSON_GetObjectItem(node, "transform");
                Transform3D transform = Layout_Transform3D_Default();
                if (cJSON_IsObject(transformNode)) {
                    (void)Transform3D_FromJsonObject(transformNode, &transform);
                }

                const cJSON* objectTypeNode = cJSON_GetObjectItem(node, "objectType");
                const char* objectType =
                    (cJSON_IsString(objectTypeNode) && objectTypeNode->valuestring)
                        ? objectTypeNode->valuestring
                        : NULL;

                const CoreObjectDimensionalMode dimensionalMode =
                    CoreDimensionalMode_FromJson(cJSON_GetObjectItem(node, "dimensionalMode"));
                const CoreObjectPlane lockedPlane =
                    CorePlane_FromJson(cJSON_GetObjectItem(node, "lockedPlane"));

                const uint32_t createdId = Layout_ObjectStore_Create(&temp.objectStore,
                                                                     kind,
                                                                     &transform,
                                                                     objectType,
                                                                     dimensionalMode,
                                                                     lockedPlane);
                if (createdId == 0u) continue;

                Object3D* object = Layout_ObjectStore_Find(&temp.objectStore, createdId);
                if (!object) continue;

                const cJSON* idNode = cJSON_GetObjectItem(node, "id");
                uint32_t parsedId = 0u;
                if (cJSON_IsNumber(idNode) && idNode->valuedouble > 0.0) {
                    parsedId = (uint32_t)idNode->valuedouble;
                }

                if (parsedId > 0u && parsedId != createdId) {
                    const Object3D* existing = Layout_ObjectStore_FindConst(&temp.objectStore, parsedId);
                    if (existing) {
                        (void)Layout_ObjectStore_Delete(&temp.objectStore, createdId);
                        continue;
                    }

                    object->objectId = parsedId;
                    {
                        char objectIdBuf[64];
                        snprintf(objectIdBuf, sizeof(objectIdBuf), "obj3d_%u", parsedId);
                        (void)core_object_set_identity(&object->coreMeta, objectIdBuf, object->coreMeta.object_type);
                    }
                }

                if (object->kind == OBJECT3D_KIND_PLANE) {
                    const cJSON* plane = cJSON_GetObjectItem(node, "plane");
                    if (cJSON_IsObject(plane)) {
                        const cJSON* width = cJSON_GetObjectItem(plane, "width");
                        const cJSON* height = cJSON_GetObjectItem(plane, "height");
                        const cJSON* lockToConstructionPlane = cJSON_GetObjectItem(plane, "lockToConstructionPlane");
                        const cJSON* lockToBounds = cJSON_GetObjectItem(plane, "lockToBounds");
                        const cJSON* frame = cJSON_GetObjectItem(plane, "frame");

                        if (cJSON_IsNumber(width) && width->valuedouble > 0.0) {
                            object->plane.width = (float)width->valuedouble;
                        }
                        if (cJSON_IsNumber(height) && height->valuedouble > 0.0) {
                            object->plane.height = (float)height->valuedouble;
                        }
                        if (cJSON_IsBool(lockToConstructionPlane)) {
                            object->plane.lockToConstructionPlane = cJSON_IsTrue(lockToConstructionPlane);
                        }
                        if (cJSON_IsBool(lockToBounds)) {
                            object->plane.lockToBounds = cJSON_IsTrue(lockToBounds);
                        }
                        if (cJSON_IsObject(frame)) {
                            Vec3_FromJsonObject(cJSON_GetObjectItem(frame, "origin"), &object->plane.frame.origin);
                            Vec3_FromJsonObject(cJSON_GetObjectItem(frame, "axisU"), &object->plane.frame.axisU);
                            Vec3_FromJsonObject(cJSON_GetObjectItem(frame, "axisV"), &object->plane.frame.axisV);
                            Vec3_FromJsonObject(cJSON_GetObjectItem(frame, "normal"), &object->plane.frame.normal);
                        }
                    }
                    object->plane.frame.origin = object->transform.position;
                } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
                    const cJSON* rectPrism = cJSON_GetObjectItem(node, "rectPrism");
                    if (cJSON_IsObject(rectPrism)) {
                        const cJSON* width = cJSON_GetObjectItem(rectPrism, "width");
                        const cJSON* height = cJSON_GetObjectItem(rectPrism, "height");
                        const cJSON* depth = cJSON_GetObjectItem(rectPrism, "depth");
                        const cJSON* lockToConstructionPlane =
                            cJSON_GetObjectItem(rectPrism, "lockToConstructionPlane");
                        const cJSON* lockToBounds = cJSON_GetObjectItem(rectPrism, "lockToBounds");
                        const cJSON* frame = cJSON_GetObjectItem(rectPrism, "frame");

                        if (cJSON_IsNumber(width) && width->valuedouble > 0.0) {
                            object->rectPrism.width = (float)width->valuedouble;
                        }
                        if (cJSON_IsNumber(height) && height->valuedouble > 0.0) {
                            object->rectPrism.height = (float)height->valuedouble;
                        }
                        if (cJSON_IsNumber(depth) && depth->valuedouble >= 0.0) {
                            object->rectPrism.depth = (float)depth->valuedouble;
                        }
                        if (cJSON_IsBool(lockToConstructionPlane)) {
                            object->rectPrism.lockToConstructionPlane = cJSON_IsTrue(lockToConstructionPlane);
                        }
                        if (cJSON_IsBool(lockToBounds)) {
                            object->rectPrism.lockToBounds = cJSON_IsTrue(lockToBounds);
                        }
                        if (cJSON_IsObject(frame)) {
                            Vec3_FromJsonObject(cJSON_GetObjectItem(frame, "origin"), &object->rectPrism.frame.origin);
                            Vec3_FromJsonObject(cJSON_GetObjectItem(frame, "axisU"), &object->rectPrism.frame.axisU);
                            Vec3_FromJsonObject(cJSON_GetObjectItem(frame, "axisV"), &object->rectPrism.frame.axisV);
                            Vec3_FromJsonObject(cJSON_GetObjectItem(frame, "normal"), &object->rectPrism.frame.normal);
                        }
                    }
                    object->rectPrism.frame.origin = object->transform.position;
                }

                if (!Layout_ObjectStore_ValidateObject(object)) {
                    (void)Layout_ObjectStore_Delete(&temp.objectStore, object->objectId);
                }
            }

            uint32_t maxId = 0u;
            for (size_t i = 0; i < temp.objectStore.count; ++i) {
                const Object3D* object = &temp.objectStore.items[i];
                if (object->isDeleted) continue;
                if (object->objectId > maxId) maxId = object->objectId;
            }
            temp.objectStore.nextObjectId = maxId + 1u;
        }
    }

    Layout_Free(layout);
    *layout = temp;
    temp.anchors = NULL;
    temp.walls = NULL;
    temp.anchorCount = 0;
    temp.wallCount = 0;
    return true;
}

bool Layout_SaveToFile(const Layout* layout, const char* path) {
    cJSON* root = Layout_CreateJson(layout);
    if (!root) return false;

    char* out = cJSON_PrintBuffered(root, 1024, cJSON_True);
    cJSON_Delete(root);
    if (!out) return false;

    CoreResult write_result = core_io_write_all(path, out, strlen(out));
    free(out);
    return write_result.code == CORE_OK;
}

bool Layout_LoadFromFile(Layout* layout, const char* path) {
    if (Layout_PathIsPack(path)) {
        SDL_Log("[Layout JSON] Runtime import policy is JSON-only; .pack is diagnostics-tooling only (%s)", path);
        return false;
    }

    if (core_scene_path_is_scene_bundle(path)) {
        CoreSceneBundleInfo bundle = {0};
        CoreResult r = core_scene_bundle_resolve(path, &bundle);
        if (r.code != CORE_OK) {
            SDL_Log("[Layout JSON] Failed to resolve scene bundle '%s' (%s)", path, r.message);
            return false;
        }
        SDL_Log("[Layout JSON] Scene bundle resolved to %s", bundle.fluid_source_path);
        SDL_Log("[Layout JSON] Scene bundle sources are not layout JSON; load a config/*.json file instead.");
        return false;
    }

    CoreBuffer file_data = {0};
    CoreResult read_result = core_io_read_all(path, &file_data);
    if (read_result.code != CORE_OK) return false;

    char* data = malloc(file_data.size + 1);
    if (!data) {
        core_io_buffer_free(&file_data);
        return false;
    }
    memcpy(data, file_data.data, file_data.size);
    data[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    bool ok = Layout_LoadFromString(layout, data);
    free(data);
    return ok;
}

char* Layout_SaveToString(const Layout* layout) {
    cJSON* root = Layout_CreateJson(layout);
    if (!root) return NULL;
    char* out = cJSON_PrintBuffered(root, 512, cJSON_False);
    cJSON_Delete(root);
    return out;
}

bool Layout_LoadFromString(Layout* layout, const char* jsonData) {
    if (!jsonData) return false;

    cJSON* root = cJSON_Parse(jsonData);
    if (!root) {
        SDL_Log("[Layout JSON] Failed to parse layout JSON string");
        return false;
    }

    bool ok = Layout_ApplyJson(layout, root);
    cJSON_Delete(root);

    if (ok) {
        Global_FlagLayoutChanged();
    }
    return ok;
}
