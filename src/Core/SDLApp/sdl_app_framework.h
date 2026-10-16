#pragma once
#include "vk_renderer.h"
#include "kit_ui_window_sdl.h"
#include "kit_ui_window_probe_sdl.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RENDER_ALWAYS,
    RENDER_THROTTLED
} RenderMode;

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    VkRenderer renderer_storage;
    KitUiWindowState window_state;
    KitUiWindowProbe window_probe;
    uint64_t presented_frames;
    bool window_block_pointer, window_resume_text;
    bool window_held_keys[SDL_NUM_SCANCODES], window_block_keys[SDL_NUM_SCANCODES];
    void* window_checkpoint;
    float deltaTime;
    bool quit;
    bool redraw_requested;
    void* userData;
    bool pending_swapchain_recreate;
    int pending_swapchain_width;
    int pending_swapchain_height;

    // Configurable render behavior
    RenderMode renderMode;
    float renderThreshold;      // maximum first-update delta after a settled wait
    float timeSinceLastRender;  // internal counter
} AppContext;


typedef struct {
    void (*handleInput)(AppContext* ctx, SDL_Event* event);   // SDL event input handling
    void (*handleUpdate)(AppContext* ctx);  // Per-wake update logic
    void (*handleRender)(AppContext* ctx);  // Render function
    /* -1 when settled; otherwise milliseconds until a timed visible update. */
    int (*nextUpdateDelayMs)(AppContext* ctx);
} AppCallbacks;



/**
 * Initializes SDL, creates window and renderer.
 * Returns false on failure. Sets up AppContext.
 */
bool App_Init(AppContext* ctx, const char* title, int width, int height, bool vsync);
void App_SetRenderMode(AppContext* ctx, RenderMode mode, float threshold);
bool App_RenderOnce(AppContext* ctx, void (*handleRender)(AppContext* ctx));


/**
 * Runs the main loop using the provided callback structure.
 */
void App_Run(AppContext* ctx, AppCallbacks* callbacks);

/**
 * Cleans up SDL resources (window, renderer) and shuts down SDL.
 */
void App_Shutdown(AppContext* ctx);

#ifdef __cplusplus
}
#endif
