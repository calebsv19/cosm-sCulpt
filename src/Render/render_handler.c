#include "render_handler.h"
#include "Core/global_state.h"
#include "Layout/grid.h"
#include "Layout/layout.h"
#include "Editor/editor.h"
#include <SDL2/SDL.h>

void Render_Frame(AppContext* ctx) {
    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;
    Layout* layout = &state->layout;

    int w, h;
    SDL_GetRendererOutputSize(ctx->renderer, &w, &h);
    SDL_SetRenderDrawColor(ctx->renderer, 20, 20, 23, 255);
    SDL_RenderClear(ctx->renderer);

    // Draw grid
    Grid_draw(grid, ctx->renderer, w, h);

    // Draw committed walls
    SDL_SetRenderDrawColor(ctx->renderer, 200, 200, 200, 255);
    float scale = grid->scale;
    float gridSize = grid->gridSize;
    for (size_t i = 0; i < layout->wallCount; ++i) {
        Wall w = layout->walls[i];
        int x1 = (int)((w.from.x - grid->offsetX) * gridSize * scale);
        int y1 = (int)((w.from.y - grid->offsetY) * gridSize * scale);
        int x2 = (int)((w.to.x   - grid->offsetX) * gridSize * scale);
        int y2 = (int)((w.to.y   - grid->offsetY) * gridSize * scale);
        SDL_RenderDrawLine(ctx->renderer, x1, y1, x2, y2);
    }

    // Draw ghost wall (if in placing mode)
    Editor_Render(&state->editor, ctx);

    SDL_RenderPresent(ctx->renderer);
}

