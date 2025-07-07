// src/main.c – input split into mouse & keyboard pipelines

#include "SDLApp/sdl_app_framework.h"
#include "grid.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

static Grid g_grid;

static void handleMouseInput(AppContext *ctx) {
    int mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    static int lastMx = 0, lastMy = 0;
    static bool dragging = false;

    if (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
        if (dragging) {
            int dx = mx - lastMx;
            int dy = my - lastMy;
            Grid_pan(&g_grid, dx, dy);
        }
        dragging = true;
    } else {
        dragging = false;
    }

    lastMx = mx;
    lastMy = my;
}

static void handleKeyboardInput(AppContext *ctx) {
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    float panSpeed = 300.0f * ctx->deltaTime;  // pixels/sec
    int w, h;
    SDL_GetRendererOutputSize(ctx->renderer, &w, &h);

    if (keys[SDL_SCANCODE_LEFT])   Grid_pan (&g_grid,  panSpeed,   0);
    if (keys[SDL_SCANCODE_RIGHT])  Grid_pan (&g_grid, -panSpeed,   0);
    if (keys[SDL_SCANCODE_UP])     Grid_pan (&g_grid,    0,    panSpeed);
    if (keys[SDL_SCANCODE_DOWN])   Grid_pan (&g_grid,    0,   -panSpeed);

    if (keys[SDL_SCANCODE_EQUALS]) Grid_zoom(&g_grid, 1.05f, w/2.0f, h/2.0f);
    if (keys[SDL_SCANCODE_MINUS])  Grid_zoom(&g_grid, 0.95f, w/2.0f, h/2.0f);

    if (keys[SDL_SCANCODE_ESCAPE]) ctx->quit = true;
}

static void handleInput(AppContext *ctx) {
    handleMouseInput(ctx);
    handleKeyboardInput(ctx);
}


static void handleUpdate(AppContext *ctx) {
    (void)ctx;  // no state to update yet
}

static void handleRender(AppContext *ctx) {
    int w, h;
    SDL_GetRendererOutputSize(ctx->renderer, &w, &h);
    SDL_SetRenderDrawColor(ctx->renderer, 20, 20, 20, 255);
    SDL_RenderClear(ctx->renderer);
    Grid_draw(&g_grid, ctx->renderer, w, h);
    SDL_RenderPresent(ctx->renderer);
}

int main(void) {
    AppContext app;
    if (!App_Init(&app, "LineDrawing", 1280, 720, true))
        return 1;

    Grid_init(&g_grid, 1.0f);    // 1 world-unit per cell
    g_grid.scale = 32.0f;        // 32 pixels per world unit

    AppCallbacks cbs = {
        .handleInput  = handleInput,
        .handleUpdate = handleUpdate,
        .handleRender = handleRender
    };
    App_SetRenderMode(&app, RENDER_THROTTLED, 1.0f/60.0f);
    App_Run(&app, &cbs);
    App_Shutdown(&app);
    return 0;
}

