// src/Core/global_state.c
#include "Core/global_state.h"
#include <stdlib.h>

static GlobalState* global = NULL;

GlobalState* Global_Get(void) {
    return global;
}

void Global_Init(void) {
    global = malloc(sizeof(GlobalState));
    Grid_init(&global->grid, 1.0f);
    global->grid.scale = 32.0f;

    Layout_Init(&global->layout, 1.0f);
    Editor_Init(&global->editor);
}

void Global_Shutdown(void) {
    Layout_Free(&global->layout);
    free(global);
    global = NULL;
}

