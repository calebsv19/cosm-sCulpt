// src/Core/global_state.c
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Layout/hitbox_system.h"
#include "UI/ui_panel.h"
#include "Layout/layout_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

static GlobalState* global = NULL;
static const char* k_space_mode_runtime_path = "data/runtime/space_mode.txt";
static const char* k_space_mode_legacy_path = "config/space_mode.txt";

static bool EnsureRuntimeDir(void) {
    if (mkdir("data", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    if (mkdir("data/runtime", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

const char* Global_GetSpaceModeLabel(SpaceMode mode) {
    return mode == SPACE_MODE_2D ? "2D" : "3D";
}

static bool SpaceMode_Parse(const char* text, SpaceMode* out_mode) {
    if (!text || !out_mode) return false;

    char normalized[16];
    size_t count = 0;
    for (const char* p = text; *p && count + 1 < sizeof(normalized); ++p) {
        unsigned char c = (unsigned char)*p;
        if (isspace(c)) continue;
        normalized[count++] = (char)tolower(c);
    }
    normalized[count] = '\0';

    if (strcmp(normalized, "2d") == 0) {
        *out_mode = SPACE_MODE_2D;
        return true;
    }
    if (strcmp(normalized, "3d") == 0) {
        *out_mode = SPACE_MODE_3D;
        return true;
    }
    return false;
}

SpaceMode Global_GetSpaceMode(void) {
    if (!global) return SPACE_MODE_3D;
    return global->spaceMode;
}

bool Global_SaveSpaceMode(void) {
    if (!global) return false;
    if (!EnsureRuntimeDir()) return false;
    FILE* fp = fopen(k_space_mode_runtime_path, "wb");
    if (!fp) return false;
    const char* mode_text = (global->spaceMode == SPACE_MODE_2D) ? "2d\n" : "3d\n";
    const size_t mode_len = strlen(mode_text);
    const bool ok = fwrite(mode_text, 1, mode_len, fp) == mode_len;
    fclose(fp);
    return ok;
}

bool Global_SetSpaceMode(SpaceMode mode, bool persist) {
    if (!global) return false;
    if (mode != SPACE_MODE_2D && mode != SPACE_MODE_3D) return false;

    global->spaceMode = mode;
    if (mode == SPACE_MODE_2D) {
        // 2D mode is always XY at z=0 with no free-view camera controls.
        global->activePlane.axis = VIEW_PLANE_XY;
        global->activePlane.offset = 0.0f;
        global->freeViewCamera.enabled = false;
    }

    Global_FlagHitboxesDirty();
    if (persist) {
        return Global_SaveSpaceMode();
    }
    return true;
}

bool Global_ToggleSpaceMode(bool persist) {
    if (!global) return false;
    SpaceMode next = (global->spaceMode == SPACE_MODE_2D) ? SPACE_MODE_3D : SPACE_MODE_2D;
    return Global_SetSpaceMode(next, persist);
}

bool Global_LoadSpaceMode(void) {
    if (!global) return false;
    FILE* fp = fopen(k_space_mode_runtime_path, "rb");
    if (!fp) {
        fp = fopen(k_space_mode_legacy_path, "rb");
    }
    if (!fp) return false;

    char buffer[32];
    const size_t count = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[count] = '\0';
    fclose(fp);

    SpaceMode loaded = SPACE_MODE_3D;
    if (!SpaceMode_Parse(buffer, &loaded)) return false;
    return Global_SetSpaceMode(loaded, false);
}

bool Global_IsCenterCrosshairEnabled(void) {
    if (!global) return false;
    return global->centerCrosshairEnabled;
}

bool Global_SetCenterCrosshairEnabled(bool enabled) {
    if (!global) return false;
    global->centerCrosshairEnabled = enabled;
    return true;
}

bool Global_ToggleCenterCrosshair(void) {
    if (!global) return false;
    global->centerCrosshairEnabled = !global->centerCrosshairEnabled;
    return true;
}

static void Global_UpdateSavedSnapshot(void) {
    if (!global) return;
    if (global->lastSavedSnapshot) {
        free(global->lastSavedSnapshot);
        global->lastSavedSnapshot = NULL;
    }
    global->lastSavedSnapshot = Layout_SaveToString(&global->layout);
}

static void Global_ProcessLayoutChanges(GlobalState* state) {
    if (!state || !state->layoutDirty) return;
    Layout_CompactDeletedElements(&state->layout);
    state->layoutDirty = false;
    state->hitboxDirty = true;
}

GlobalState* Global_Get(void) {
    return global;
}

void Global_Init(int screenWidth, int screenHeight) {
    global = malloc(sizeof(GlobalState));
    global->screenWidth = screenWidth;
    global->screenHeight = screenHeight;
    global->layoutDirty = true;
    global->hitboxDirty = true;
    global->spaceMode = SPACE_MODE_3D;
    global->activePlane = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f };
    global->freeViewCamera = (FreeViewCamera){
        .enabled = false,
        .yawDeg = 35.0f,
        .pitchDeg = 20.0f,
        .target = {0.0f, 0.0f, 0.0f}
    };
    global->centerCrosshairEnabled = true;
    global->layoutDirtySinceSave = false;
    global->lastSavedSnapshot = NULL;
    memset(global->currentConfigPath, 0, sizeof(global->currentConfigPath));
    strncpy(global->currentConfigPath, "config/layout_config.json", sizeof(global->currentConfigPath) - 1);

    Grid_init(&global->grid, 1.0f, screenWidth, screenHeight);

    Layout_Init(&global->layout, 1.0f);
    Editor_Init(&global->editor);
    UIPanel_Init(screenWidth, screenHeight);

    Global_SetSpaceMode(global->spaceMode, false);
    Global_LoadSpaceMode();
    Editor_HistoryCapture(&global->editor, &global->layout);
    Global_UpdateSavedSnapshot();
}


void Global_Shutdown(void) {
    if (global->lastSavedSnapshot) {
        free(global->lastSavedSnapshot);
        global->lastSavedSnapshot = NULL;
    }
    Editor_Free(&global->editor);
    Layout_Free(&global->layout);
    free(global);
    global = NULL;
}



void Global_TickSystems(AppContext* ctx) {
    (void)ctx;
    GlobalState* state = Global_Get();
    if (!state) return;

    Global_ProcessLayoutChanges(state);

    Global_RebuildHitboxesIfDirty();

    // Future: add tick handlers for other systems here (UI, animations, etc.)
}


void Global_SetWindowSize(int w, int h) {
    if (!global) return;
    global->screenWidth = w;
    global->screenHeight = h;
    Global_FlagGridChanged();
}

int Global_GetScreenWidth(void) { return global->screenWidth; }
int Global_GetScreenHeight(void) { return global->screenHeight; }

void Global_FlagLayoutChanged(void) {
    GlobalState* state = Global_Get();
    if (!state) return;
    state->layoutDirty = true;
    state->layoutDirtySinceSave = true;
    Global_FlagHitboxesDirty();
}

void Global_FlagGridChanged(void) {
    Global_FlagHitboxesDirty();
}

void Global_FlagHitboxesDirty(void) {
    GlobalState* state = Global_Get();
    if (!state) return;
    state->hitboxDirty = true;
}

void Global_RebuildHitboxesIfDirty(void) {
    GlobalState* state = Global_Get();
    if (!state) return;

    Global_ProcessLayoutChanges(state);

    if (!state->hitboxDirty) return;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    bool gizmoEnabled = (state->spaceMode == SPACE_MODE_3D) &&
                        SpaceAdapter_IsFreeViewEnabled(&viewCtx);
    HitboxSystem_Rebuild(&state->layout,
                         state->grid.scale,
                         state->grid.offsetX,
                         state->grid.offsetY,
                         viewCtx.plane,
                         SpaceAdapter_Camera(&viewCtx),
                         state->editor.selectedAnchorIndex,
                         gizmoEnabled);
    state->hitboxDirty = false;
}

void Global_OnLayoutSaved(const char* path) {
    GlobalState* state = Global_Get();
    if (!state) return;
    if (path && *path) {
        strncpy(state->currentConfigPath, path, sizeof(state->currentConfigPath) - 1);
        state->currentConfigPath[sizeof(state->currentConfigPath) - 1] = '\0';
    }
    state->layoutDirtySinceSave = false;
    state->layoutDirty = false;
    state->hitboxDirty = true;
    Global_UpdateSavedSnapshot();
}

void Global_OnLayoutLoaded(const char* path) {
    GlobalState* state = Global_Get();
    if (!state) return;
    if (path && *path) {
        strncpy(state->currentConfigPath, path, sizeof(state->currentConfigPath) - 1);
        state->currentConfigPath[sizeof(state->currentConfigPath) - 1] = '\0';
    }
    state->layoutDirtySinceSave = false;
    state->layoutDirty = false;
    state->hitboxDirty = true;
    Global_UpdateSavedSnapshot();
}

const char* Global_GetCurrentConfigPath(void) {
    GlobalState* state = Global_Get();
    if (!state) return NULL;
    return state->currentConfigPath;
}

bool Global_IsLayoutDirty(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    return state->layoutDirtySinceSave;
}
