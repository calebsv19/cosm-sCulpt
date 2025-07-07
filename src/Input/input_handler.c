// src/Input/input_handler.c
#include "input_handler.h"
#include "input_mouse.h"
#include "Core/global_state.h"
#include "Layout/grid.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

static void handleKeyboard(AppContext *ctx) {
    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;

    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    float panSpeed = 300.0f * ctx->deltaTime;
    int w, h;
    SDL_GetRendererOutputSize(ctx->renderer, &w, &h);

    if (keys[SDL_SCANCODE_LEFT])   Grid_pan(grid,  panSpeed,   0);
    if (keys[SDL_SCANCODE_RIGHT])  Grid_pan(grid, -panSpeed,   0);
    if (keys[SDL_SCANCODE_UP])     Grid_pan(grid,    0,    panSpeed);
    if (keys[SDL_SCANCODE_DOWN])   Grid_pan(grid,    0,   -panSpeed);

    if (keys[SDL_SCANCODE_EQUALS]) Grid_zoom(grid, 1.05f, w / 2.0f, h / 2.0f);
    if (keys[SDL_SCANCODE_MINUS])  Grid_zoom(grid, 0.95f, w / 2.0f, h / 2.0f);

    if (keys[SDL_SCANCODE_ESCAPE]) ctx->quit = true;
}

void Input_Handle(AppContext *ctx, SDL_Event* event) {
    Input_MouseHandle(ctx, event);  // Handles panning + editor click logic

    if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) {
        handleKeyboard(ctx);

        GlobalState* state = Global_Get();
        if (event->key.keysym.sym == SDLK_LSHIFT || event->key.keysym.sym == SDLK_RSHIFT) {
            bool held = (event->type == SDL_KEYDOWN);
            Editor_SetShiftHeld(&state->editor, held);
        }
    }
}

