#include "Layout/layout_json.h"
#include "Core/global_state.h"
#include "core_scene.h"
#include "cjson/cJSON.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char* LAYOUT_JSON_GENERATOR = "LineDrawing";

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

    if (schemaVersion > LAYOUT_JSON_SCHEMA_VERSION) {
        SDL_Log("[Layout JSON] Unsupported schema version %d (max %d)",
                schemaVersion, LAYOUT_JSON_SCHEMA_VERSION);
        return false;
    }

    Layout temp;
    Layout_Init(&temp, targetGrid);

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

    FILE* f = fopen(path, "w");
    if (!f) {
        free(out);
        return false;
    }

    fputs(out, f);
    fclose(f);
    free(out);
    return true;
}

bool Layout_LoadFromFile(Layout* layout, const char* path) {
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

    FILE* f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char* data = malloc((size_t)len + 1);
    if (!data) {
        fclose(f);
        return false;
    }

    fread(data, 1, (size_t)len, f);
    data[len] = '\0';
    fclose(f);

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
