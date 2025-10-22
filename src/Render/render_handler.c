#include "render_handler.h"
#include "Core/global_state.h"

#include "UI/ui_panel.h"
#include "UI/render_ui_panel.h"

#include "Layout/Grid/render_grid.h"
#include "Layout/layout.h"
#include "Layout/render_layout.h"     // ← NEW: Layout wall + anchor renderer
#include "Editor/editor.h"
#include "Editor/render_editor.h"
#include <SDL2/SDL.h>

void Render_Frame(AppContext* ctx) {
    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;
    Layout* layout = &state->layout;
    EditorState* editor = &state->editor;

    int w, h;
    SDL_GetRendererOutputSize(ctx->renderer, &w, &h);
    SDL_SetRenderDrawColor(ctx->renderer, 20, 20, 23, 255); // Background
    SDL_RenderClear(ctx->renderer);

    // ─── Grid ────────────────────────────────────
    // Render_Grid(grid, ctx->renderer, w, h, GRID_DRAW_UNITS);
    Render_Grid(grid, ctx->renderer, w, h, GRID_DRAW_UNITS | GRID_DRAW_FIVES);
    // Render_Grid(grid, ctx->renderer, w, h, GRID_DRAW_UNITS | GRID_DRAW_FIVES | GRID_DRAW_TENS);

    // ─── Layout Geometry (Walls + Anchors) ────────
    Layout_Render(layout, ctx);

    // ─── Editor Overlay (Ghost Wall, Active Anchor) ─
    Render_Editor_Anchor(editor, ctx);
    Render_Editor_GhostWall(editor, ctx);


    UIPanelState* panel = UIPanel_Get();
    Render_UIPanel(panel, ctx->renderer);

    SDL_RenderPresent(ctx->renderer);
}

