// src/main.c
#include "Core/SDLApp/sdl_app_framework.h"
#include "Layout/Grid/grid.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"


#include "Input/input_handler.h"
#include "Render/render_handler.h"
#include "Core/global_state.h"




#define DEFAULT_WINDOW_WIDTH  1280
#define DEFAULT_WINDOW_HEIGHT 720


static void handleInput(AppContext *ctx, SDL_Event* event) {
    Input_Handle(ctx, event);
}


static void handleUpdate(AppContext *ctx) {
    Global_TickSystems(ctx);

}


static void handleRender(AppContext *ctx) {
    Render_Frame(ctx);
}

int main(void) {
    AppContext app;
    if (!App_Init(&app, "LineDrawing", DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, true))
        return 1;

    line_drawing3d_shared_theme_load_persisted();
    if (!FontManager_Init()) return 1;
    if (!FontManager_LoadFonts()) return 1;

    // Initialize global program state (grid, layout, editor, etc.)
    Global_Init(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);

    AppCallbacks cbs = {
        .handleInput  = handleInput,
        .handleUpdate = handleUpdate,
        .handleRender = handleRender
    };
    App_SetRenderMode(&app, RENDER_THROTTLED, 1.0f / 60.0f);
    App_Run(&app, &cbs);

    line_drawing3d_shared_theme_save_persisted();
    FontManager_Quit();
    Global_Shutdown();  // free layout memory, etc.
    App_Shutdown(&app);
    return 0;
}
