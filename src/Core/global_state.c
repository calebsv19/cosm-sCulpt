// src/Core/global_state.c
#include "Core/global_state.h"
#include "Layout/hitbox_system.h"
#include <stdlib.h>

static GlobalState* global = NULL;

GlobalState* Global_Get(void) {
    return global;
}

void Global_Init(int screenWidth, int screenHeight) {
    global = malloc(sizeof(GlobalState));
    global->screenWidth = screenWidth;
    global->screenHeight = screenHeight;

    Grid_init(&global->grid, 1.0f, screenWidth, screenHeight);
    global->grid.scale = 32.0f;

    Layout_Init(&global->layout, 1.0f);
    Editor_Init(&global->editor);
}


void Global_Shutdown(void) {
    Layout_Free(&global->layout);
    free(global);
    global = NULL;
}



void Global_TickSystems(AppContext* ctx) {
    (void)ctx;
    GlobalState* state = Global_Get();

    // Finalize any mark-delete logic
    Layout_CompactDeletedElements(&state->layout);

    // Rebuild hitboxes after deletion compaction
    HitboxSystem_Rebuild(&state->layout, state->grid.scale, state->grid.offsetX, state->grid.offsetY);

    // Future: add tick handlers for other systems here (UI, animations, etc.)
}


void Global_SetWindowSize(int w, int h) {
    global->screenWidth = w;
    global->screenHeight = h;
}

int Global_GetScreenWidth() { return global->screenWidth; }
int Global_GetScreenHeight() { return global->screenHeight; }
