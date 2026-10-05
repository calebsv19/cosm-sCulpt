#pragma once

#include "test_framework.h"

#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Layout/layout.h"
#include "Layout/layout_json.h"
#include "Layout/hitbox_system.h"
#include "Editor/editor.h"
#include "Math/math_util.h"
#include "Tools/canonical_scene_export.h"
#include "Tools/scene_import.h"
#include "Tools/shape_from_layout.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_parts.h"
#include "core_units.h"
#include "cjson/cJSON.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline bool ld_test_nearly_equal(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

static inline bool ld_test_vec3_nearly_equal(Vec3 a, Vec3 b) {
    return ld_test_nearly_equal(a.x, b.x) &&
           ld_test_nearly_equal(a.y, b.y) &&
           ld_test_nearly_equal(a.z, b.z);
}

static inline char* ld_test_read_text_file(const char* path) {
    FILE* f = NULL;
    long len = 0;
    char* buf = NULL;

    if (!path) return NULL;
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
    buf = (char*)malloc((size_t)len + 1u);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1u, (size_t)len, f) != (size_t)len) {
        fclose(f);
        free(buf);
        return NULL;
    }
    fclose(f);
    buf[len] = '\0';
    return buf;
}

static inline bool ld_test_write_text_file_basic(const char* path, const char* text) {
    FILE* f = NULL;
    size_t len = 0u;
    if (!path || !text) return false;
    f = fopen(path, "wb");
    if (!f) return false;
    len = strlen(text);
    if (fwrite(text, 1u, len, f) != len) {
        fclose(f);
        return false;
    }
    return fclose(f) == 0;
}

static inline bool ld_test_json_vec3_matches(cJSON* node, Vec3 expected) {
    cJSON* x = NULL;
    cJSON* y = NULL;
    cJSON* z = NULL;
    if (!cJSON_IsObject(node)) return false;
    x = cJSON_GetObjectItem(node, "x");
    y = cJSON_GetObjectItem(node, "y");
    z = cJSON_GetObjectItem(node, "z");
    return cJSON_IsNumber(x) &&
           cJSON_IsNumber(y) &&
           cJSON_IsNumber(z) &&
           ld_test_nearly_equal((float)x->valuedouble, expected.x) &&
           ld_test_nearly_equal((float)y->valuedouble, expected.y) &&
           ld_test_nearly_equal((float)z->valuedouble, expected.z);
}

static inline bool ld_test_json_bounds_matches(cJSON* node, SceneBounds3D expected) {
    return cJSON_IsObject(node) &&
           cJSON_IsBool(cJSON_GetObjectItem(node, "enabled")) &&
           cJSON_IsBool(cJSON_GetObjectItem(node, "clamp_on_edit")) &&
           cJSON_IsTrue(cJSON_GetObjectItem(node, "enabled")) == expected.enabled &&
           cJSON_IsTrue(cJSON_GetObjectItem(node, "clamp_on_edit")) == expected.clampOnEdit &&
           ld_test_json_vec3_matches(cJSON_GetObjectItem(node, "min"), expected.min) &&
           ld_test_json_vec3_matches(cJSON_GetObjectItem(node, "max"), expected.max);
}

static inline cJSON* ld_test_find_object_by_id(cJSON* objects, const char* object_id) {
    int count = 0;
    int i = 0;
    if (!cJSON_IsArray(objects) || !object_id) return NULL;
    count = cJSON_GetArraySize(objects);
    for (i = 0; i < count; ++i) {
        cJSON* obj = cJSON_GetArrayItem(objects, i);
        cJSON* id = cJSON_GetObjectItem(obj, "object_id");
        if (cJSON_IsString(id) && id->valuestring && strcmp(id->valuestring, object_id) == 0) {
            return obj;
        }
    }
    return NULL;
}

static inline void ld_test_init_runtime(void) {
    Global_Init(800, 600);
}

static inline void ld_test_shutdown_runtime(void) {
    Global_Shutdown();
}

static inline bool ld_test_context_part(int action) {
    int mode = action == PARTS_VOLUMES ? 4 : action == PARTS_CHECKS ? 5 : action - PARTS_OBJECTS;
    if (mode < 0 || mode > 5) return false;
    UIPanelState* ui = UIPanel_Get();
    UIPanel_SetActiveRightTab(ui, mode == 2 ? UI_PANEL_RIGHT_TAB_VIEW : UI_PANEL_RIGHT_TAB_OBJECT);
    UIPanel_OnWindowResized(Global_Get()->screenWidth, Global_Get()->screenHeight);
    SDL_Rect rect = ui->contextRect;
    SDL_Event event = {.type = SDL_MOUSEBUTTONDOWN};
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = rect.x + rect.w / 2;
    event.button.y = rect.y + rect.h / 2;
    if (!UIPanel_ContextEvent(&event) || !ui->contextMenuOpen) return false;
    int index = mode == 2 ? 1 : (const int[]){1, 2, 0, 3, 4, 5}[mode];
    event.button.y += (index + 1) * (rect.h + 2);
    return UIPanel_ContextEvent(&event) && ui->activeRightTab == UI_PANEL_RIGHT_TAB_PARTS && ui->parts.mode == mode;
}
