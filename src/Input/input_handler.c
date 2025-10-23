// src/Input/input_handler.c
#include "input_handler.h"
#include "input_mouse.h"
#include "input_keyboard.h"   // ← NEW
#include "Core/global_state.h"
#include <SDL2/SDL.h>

//        Public input handler
// ======================================
void Input_Handle(AppContext *ctx, SDL_Event* event) {
    Global_RebuildHitboxesIfDirty();

    Input_MouseHandle(ctx, event);

    Input_KeyboardHandle(ctx, event);
}
