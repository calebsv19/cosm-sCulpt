// src/Core/global_state.c
#include "Core/global_state.h"
#include "Layout/hitbox_system.h"
#include "UI/ui_panel.h"
#include "Layout/layout_json.h"
#include <stdlib.h>
#include <string.h>

static GlobalState* global = NULL;

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
    global->activePlane = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f };
    global->freeViewCamera = (FreeViewCamera){
        .enabled = false,
        .yawDeg = 35.0f,
        .pitchDeg = 20.0f,
        .target = {0.0f, 0.0f, 0.0f}
    };
    global->layoutDirtySinceSave = false;
    global->lastSavedSnapshot = NULL;
    memset(global->currentConfigPath, 0, sizeof(global->currentConfigPath));
    strncpy(global->currentConfigPath, "config/layout_config.json", sizeof(global->currentConfigPath) - 1);

    Grid_init(&global->grid, 1.0f, screenWidth, screenHeight);

    Layout_Init(&global->layout, 1.0f);
    Editor_Init(&global->editor);
    UIPanel_Init(screenWidth, screenHeight);
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
    HitboxSystem_Rebuild(&state->layout,
                         state->grid.scale,
                         state->grid.offsetX,
                         state->grid.offsetY,
                         state->activePlane,
                         &state->freeViewCamera);
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
