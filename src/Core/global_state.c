// src/Core/global_state.c
#include "Core/global_state.h"
#include "Layout/hitbox_system.h"
#include "UI/ui_panel.h"
#include <stdlib.h>

static GlobalState* global = NULL;

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

    Grid_init(&global->grid, 1.0f, screenWidth, screenHeight);

    Layout_Init(&global->layout, 1.0f);
    Editor_Init(&global->editor);
    UIPanel_Init(screenWidth, screenHeight);
    Editor_HistoryCapture(&global->editor, &global->layout);
}


void Global_Shutdown(void) {
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
    HitboxSystem_Rebuild(&state->layout, state->grid.scale, state->grid.offsetX, state->grid.offsetY);
    state->hitboxDirty = false;
}
