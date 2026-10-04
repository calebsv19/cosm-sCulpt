#include "Core/editor_preferences.h"
#include "Core/global_state.h"
#include "UI/ui_panel.h"
#include "cjson/cJSON.h"
#include "core_io.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool number(const cJSON* root, const char* key, double* value) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble)) return false;
    *value = item->valuedouble;
    return true;
}
static bool valid_unit(double unit) {
    return unit == CORE_UNIT_MILLIMETER || unit == CORE_UNIT_CENTIMETER ||
        unit == CORE_UNIT_METER || unit == CORE_UNIT_INCH || unit == CORE_UNIT_FOOT;
}
bool LineDrawingEditorPreferences_Load(const char* path) {
    if (!path || !Global_Get()) return false;
    FILE* file = fopen(path, "rb");
    if (!file) return false;
    char text[2048]; size_t size = fread(text, 1, sizeof(text)-1, file);
    bool complete = !ferror(file) && fgetc(file) == EOF;
    fclose(file); text[size] = 0;
    if (!complete) return false;
    const char* end = NULL;
    cJSON* root = cJSON_ParseWithOpts(text, &end, false);
    if (end) while (isspace((unsigned char)*end)) ++end;
    if (!root || !end || *end) { cJSON_Delete(root); return false; }
    double version, unit, plane, tool, yaw, pitch;
    const cJSON* free_view = cJSON_GetObjectItemCaseSensitive(root, "freeView");
    bool ok = number(root,"version",&version) && version == 1 &&
        number(root,"displayUnit",&unit) && valid_unit(unit) &&
        number(root,"plane",&plane) && (plane == VIEW_PLANE_XY || plane == VIEW_PLANE_YZ || plane == VIEW_PLANE_XZ) &&
        number(root,"tool",&tool) && (tool == VIEWPORT_TOOL_SELECT || tool == VIEWPORT_TOOL_ORBIT || tool == VIEWPORT_TOOL_PAN) &&
        number(root,"yaw",&yaw) && fabs(yaw) <= 360000 &&
        number(root,"pitch",&pitch) && fabs(pitch) <= 89 && cJSON_IsBool(free_view);
    if (ok) {
        GlobalState* state = Global_Get();
        UIPanel_Get()->displayUnit = (CoreUnitKind)unit;
        state->activePlane = (ViewPlane){.axis=(ViewPlaneAxis)plane, .offset=0};
        state->freeViewCamera.enabled = cJSON_IsTrue(free_view);
        state->freeViewCamera.yawDeg = (float)yaw;
        state->freeViewCamera.pitchDeg = (float)pitch;
        state->editor.viewportTool = (ViewportTool)tool;
        if (state->spaceMode == SPACE_MODE_2D) {
            state->activePlane = (ViewPlane){.axis=VIEW_PLANE_XY};
            state->freeViewCamera.enabled = false;
            if (state->editor.viewportTool == VIEWPORT_TOOL_ORBIT) state->editor.viewportTool = VIEWPORT_TOOL_SELECT;
        }
        Global_FlagGridChanged(); Global_FlagHitboxesDirty();
    }
    cJSON_Delete(root);
    return ok;
}
bool LineDrawingEditorPreferences_Save(const char* path) {
    const GlobalState* state = Global_Get();
    if (!path || !path[0] || !state) return false;
    /* Never restore an armed drawing tool on launch. */
    int tool = state->editor.viewportTool == VIEWPORT_TOOL_LINE ? VIEWPORT_TOOL_SELECT : state->editor.viewportTool;
    if (!valid_unit(UIPanel_GetDisplayUnit()) || !isfinite(state->freeViewCamera.yawDeg) ||
        !isfinite(state->freeViewCamera.pitchDeg)) return false;
    cJSON* root = cJSON_CreateObject();
    bool ok = root && cJSON_AddNumberToObject(root,"version",1) &&
        cJSON_AddNumberToObject(root,"displayUnit",UIPanel_GetDisplayUnit()) &&
        cJSON_AddNumberToObject(root,"plane",state->activePlane.axis) &&
        cJSON_AddBoolToObject(root,"freeView",state->freeViewCamera.enabled) &&
        cJSON_AddNumberToObject(root,"tool",tool) &&
        cJSON_AddNumberToObject(root,"yaw",state->freeViewCamera.yawDeg) &&
        cJSON_AddNumberToObject(root,"pitch",state->freeViewCamera.pitchDeg);
    char* text = ok ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    if (!text) return false;
    ok = core_io_write_all_atomic(path, text, strlen(text)).code == CORE_OK;
    free(text); return ok;
}
