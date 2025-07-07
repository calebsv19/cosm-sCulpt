// src/main.c
#include "SDLApp/sdl_app_framework.h"
#include "Layout/grid.h"
#include "Input/input_handler.h"
#include "Render/render_handler.h"
#include "Core/global_state.h"




static void handleInput(AppContext *ctx, SDL_Event* event) {
    Input_Handle(ctx, event);
}

static void handleUpdate(AppContext *ctx) {
    (void)ctx;
}

static void handleRender(AppContext *ctx) {
    Render_Frame(ctx);
}

int main(void) {
    AppContext app;
    if (!App_Init(&app, "LineDrawing", 1280, 720, true))
        return 1;

    // Initialize global program state (grid, layout, editor, etc.)
    Global_Init();

    AppCallbacks cbs = {
        .handleInput  = handleInput,
        .handleUpdate = handleUpdate,
        .handleRender = handleRender
    };
    App_SetRenderMode(&app, RENDER_THROTTLED, 1.0f / 60.0f);
    App_Run(&app, &cbs);

    Global_Shutdown();  // free layout memory, etc.
    App_Shutdown(&app);
    return 0;
}
