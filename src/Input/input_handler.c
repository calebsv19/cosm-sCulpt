// src/Input/input_handler.c
#include "input_handler.h"
#include "input_mouse.h"
#include "input_keyboard.h"   // ← NEW
#include "Core/global_state.h"
#include "Layout/hitbox_system.h"
#include <SDL2/SDL.h>

//        Public input handler
// ======================================
void Input_Handle(AppContext *ctx, SDL_Event* event) {
    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;
    Layout* layout = &state->layout;

    HitboxSystem_Rebuild(layout, grid->scale, grid->offsetX, grid->offsetY);

    Input_MouseHandle(ctx, event);

    Input_KeyboardHandle(ctx, event);

    HitboxSystem_Rebuild(layout, grid->scale, grid->offsetX, grid->offsetY);
}

