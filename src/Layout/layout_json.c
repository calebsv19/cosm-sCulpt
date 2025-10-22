#include "Layout/layout_json.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

bool Layout_SaveToFile(const Layout* layout, const char* path) {
    cJSON* root = cJSON_CreateObject();

    cJSON* anchors = cJSON_CreateArray();
    for (size_t i = 0; i < layout->anchorCount; ++i) {
        Anchor* a = &layout->anchors[i];
        if (a->isDeleted) continue;

        cJSON* anchor = cJSON_CreateObject();
        cJSON_AddNumberToObject(anchor, "x", a->pos.x);
        cJSON_AddNumberToObject(anchor, "y", a->pos.y);
        cJSON_AddBoolToObject(anchor, "persistent", a->isPersistent);
        cJSON_AddItemToArray(anchors, anchor);
    }
    cJSON_AddItemToObject(root, "anchors", anchors);

    cJSON* walls = cJSON_CreateArray();
    for (size_t i = 0; i < layout->wallCount; ++i) {
        Wall* w = &layout->walls[i];
        if (w->isDeleted) continue;

        cJSON* wall = cJSON_CreateObject();
        cJSON_AddNumberToObject(wall, "a", w->anchorA);
        cJSON_AddNumberToObject(wall, "b", w->anchorB);
        cJSON_AddItemToArray(walls, wall);
    }
    cJSON_AddItemToObject(root, "walls", walls);

    char* out = cJSON_Print(root);
    FILE* f = fopen(path, "w");
    if (!f) return false;

    fputs(out, f);
    fclose(f);

    cJSON_Delete(root);
    free(out);
    return true;
}

bool Layout_LoadFromFile(Layout* layout, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char* data = malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(data);
    free(data);
    if (!root) {
        SDL_Log("[Layout JSON] Failed to parse JSON from %s", path);
        return false;
    }

    Layout_Free(layout);
    Layout_Init(layout, 1.0f);  // Reset with grid size 1

    // ─── Load Anchors ───────────────────────────────
    cJSON* anchors = cJSON_GetObjectItem(root, "anchors");
    int numAnchors = cJSON_GetArraySize(anchors);

    for (int i = 0; i < numAnchors; ++i) {
        cJSON* a = cJSON_GetArrayItem(anchors, i);
        Vec2 pos = {
            .x = (float)cJSON_GetObjectItem(a, "x")->valuedouble,
            .y = (float)cJSON_GetObjectItem(a, "y")->valuedouble
        };
        int idx = Layout_AddAnchor(layout, pos);
        layout->anchors[idx].isPersistent = cJSON_IsTrue(cJSON_GetObjectItem(a, "persistent"));
    }

    // ─── Load Walls ────────────────────────────────
    cJSON* walls = cJSON_GetObjectItem(root, "walls");
    int numWalls = cJSON_GetArraySize(walls);

    for (int i = 0; i < numWalls; ++i) {
        cJSON* w = cJSON_GetArrayItem(walls, i);
        int a = cJSON_GetObjectItem(w, "a")->valueint;
        int b = cJSON_GetObjectItem(w, "b")->valueint;

        // Safety: make sure indices are valid
        if (a >= 0 && a < (int)layout->anchorCount &&
            b >= 0 && b < (int)layout->anchorCount) {
            Vec2 posA = layout->anchors[a].pos;
            Vec2 posB = layout->anchors[b].pos;
            Layout_AddWall(layout, posA, posB);
        } else {
            SDL_Log("[Layout JSON] Skipped wall (%d → %d): invalid index", a, b);
        }
    }

    cJSON_Delete(root);
    return true;
}

