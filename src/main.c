// -----------------------------------------------------------------------------
// LineDrawing ‑ Phase‑1 Skeleton
// rudimentary "M‑0" implementation: SDL window + movable/zoomable grid
// -----------------------------------------------------------------------------
// This single file depends only on SDL2 and the provided sdl_app_framework.[ch].
// Later milestones will split responsibilities into grid.c, editor.c, layout.c …
// but for the first compile‑and‑run test we keep everything in one TU so that
// nothing else blocks progress.
// -----------------------------------------------------------------------------
#include <SDL2/SDL.h>
#include <math.h>
#include "SDLApp/sdl_app_framework.h"

// ----------------------------- Viewport state ---------------------------------
typedef struct {
    float offsetX;   // world units
    float offsetY;   // world units
    float scale;     // pixels  per world unit (zoom factor)
} Viewport;

static Viewport g_view = { .offsetX = 0.f, .offsetY = 0.f, .scale = 1.f };

// grid spacing expressed in world units.   1 == one logical grid cell.
// (At scale == 32, this draws a 32‑pixel grid.)
static const float GRID_STEP_WORLD = 1.0f;
static const float BASE_PIXELS_PER_UNIT = 32.f;   // zoom 1 ➜ 32 px per cell

// ----------------------------- helpers ----------------------------------------
static inline float toScreenX(float worldX) {
    return (worldX - g_view.offsetX) * g_view.scale * BASE_PIXELS_PER_UNIT;
}
static inline float toScreenY(float worldY) {
    return (worldY - g_view.offsetY) * g_view.scale * BASE_PIXELS_PER_UNIT;
}

// ----------------------------------------------------------------------------
// SDLApp callbacks
// ----------------------------------------------------------------------------
static void handleInput(AppContext* ctx) {
    const Uint8* keys = SDL_GetKeyboardState(NULL);

    // Pan with arrow keys (world‑space, independent of zoom)
    const float panSpeed = 5.0f * ctx->deltaTime / g_view.scale; // world units/s
    if (keys[SDL_SCANCODE_LEFT])  g_view.offsetX -= panSpeed;
    if (keys[SDL_SCANCODE_RIGHT]) g_view.offsetX += panSpeed;
    if (keys[SDL_SCANCODE_UP])    g_view.offsetY -= panSpeed;
    if (keys[SDL_SCANCODE_DOWN])  g_view.offsetY += panSpeed;

    // Zoom with +/- keys (’=’ is usually + without Shift)
    if (keys[SDL_SCANCODE_EQUALS])      g_view.scale *= 1.05f;   // zoom in
    if (keys[SDL_SCANCODE_MINUS])       g_view.scale *= 0.95f;   // zoom out

    // Clamp scale to sane range
    if (g_view.scale < 0.1f) g_view.scale = 0.1f;
    if (g_view.scale > 8.0f) g_view.scale = 8.0f;

    // Quick quit
    if (keys[SDL_SCANCODE_ESCAPE]) ctx->quit = true;
}

static void handleUpdate(AppContext* ctx) {
    (void)ctx; // Nothing to update yet – placeholder for future logic.
}

static void drawGrid(SDL_Renderer* rdr, int w, int h) {
    const float pixelsPerCell = g_view.scale * BASE_PIXELS_PER_UNIT * GRID_STEP_WORLD;

    // Find world coordinate of screen (0,0)
    const float worldLeft   = g_view.offsetX;
    const float worldTop    = g_view.offsetY;

    // Determine first vertical line X in pixels
    float firstX_px = fmodf(-worldLeft * pixelsPerCell, pixelsPerCell);
    if (firstX_px < 0) firstX_px += pixelsPerCell;

    // First horizontal line Y
    float firstY_px = fmodf(-worldTop * pixelsPerCell, pixelsPerCell);
    if (firstY_px < 0) firstY_px += pixelsPerCell;

    SDL_SetRenderDrawColor(rdr, 60, 60, 60, 255);

    // Vertical lines
    for (float x = firstX_px; x <= w; x += pixelsPerCell) {
        SDL_RenderDrawLine(rdr, (int)x, 0, (int)x, h);
    }
    // Horizontal lines
    for (float y = firstY_px; y <= h; y += pixelsPerCell) {
        SDL_RenderDrawLine(rdr, 0, (int)y, w, (int)y);
    }
}

static void handleRender(AppContext* ctx) {
    SDL_SetRenderDrawColor(ctx->renderer, 25, 25, 25, 255); // background
    SDL_RenderClear(ctx->renderer);

    int winW, winH;
    SDL_GetRendererOutputSize(ctx->renderer, &winW, &winH);
    drawGrid(ctx->renderer, winW, winH);

    SDL_RenderPresent(ctx->renderer);
}

// ----------------------------------------------------------------------------
// Entry point
// ----------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    AppContext app;
    if (!App_Init(&app, "LineDrawing – M0", 1280, 720, true)) {
        return 1;
    }

    AppCallbacks cbs = { .handleInput = handleInput,
                         .handleUpdate = handleUpdate,
                         .handleRender = handleRender };

    // Throttle to 60 fps until we have heavier logic.
    App_SetRenderMode(&app, RENDER_THROTTLED, 1.0f / 60.0f);

    App_Run(&app, &cbs);
    App_Shutdown(&app);
    return 0;
}

