#include "render_handler.h"
#include "Core/global_state.h"

#include "UI/ui_panel.h"
#include "UI/render_ui_panel.h"
#include "UI/info_overlay.h"

#include "Layout/Grid/render_grid.h"
#include "Layout/layout.h"
#include "Layout/render_layout.h"     // ← NEW: Layout wall + anchor renderer
#include "Render/vulkan_adapter.h"
#include "Editor/editor.h"
#include "Editor/render_editor.h"
#include "vk_renderer.h"
#include <SDL2/SDL.h>

void Render_Frame(AppContext* ctx) {
    static int logged_counts = 0;

    GlobalState* state = Global_Get();
    Global_RebuildHitboxesIfDirty();
    Grid* grid = &state->grid;
    Layout* layout = &state->layout;
    EditorState* editor = &state->editor;

    VkRenderer* vk = (VkRenderer*)ctx->renderer;
    uint32_t before_draws = vk ? vk->draw_state.draw_call_count : 0;

    int w = Global_GetScreenWidth();
    int h = Global_GetScreenHeight();
    VulkanAdapter_Clear(ctx->renderer, w, h, (SDL_Color){20, 20, 23, 255});

    // ─── Grid ────────────────────────────────────
    // Render_Grid(grid, ctx->renderer, w, h, GRID_DRAW_UNITS);
    Render_Grid(grid, ctx->renderer, w, h, GRID_DRAW_UNITS | GRID_DRAW_FIVES);
    // Render_Grid(grid, ctx->renderer, w, h, GRID_DRAW_UNITS | GRID_DRAW_FIVES | GRID_DRAW_TENS);
    if (!logged_counts && vk) {
        fprintf(stderr, "[LineDrawing] Grid draw calls: %u\n",
                vk->draw_state.draw_call_count - before_draws);
        before_draws = vk->draw_state.draw_call_count;
    }

    // ─── Layout Geometry (Walls + Anchors) ────────
    Layout_Render(layout, ctx);
    if (!logged_counts && vk) {
        fprintf(stderr, "[LineDrawing] Layout draw calls: %u\n",
                vk->draw_state.draw_call_count - before_draws);
        before_draws = vk->draw_state.draw_call_count;
    }

    // ─── Editor Overlay (Ghost Wall, Active Anchor) ─
    Render_Editor_Anchor(editor, ctx);
    Render_Editor_GhostWall(editor, ctx);
    Render_Editor_SelectionBox(editor, ctx);
    if (!logged_counts && vk) {
        fprintf(stderr, "[LineDrawing] Editor overlay draw calls: %u\n",
                vk->draw_state.draw_call_count - before_draws);
        before_draws = vk->draw_state.draw_call_count;
    }

    Render_InfoOverlay(ctx->renderer);
    if (!logged_counts && vk) {
        fprintf(stderr, "[LineDrawing] Info overlay draw calls: %u\n",
                vk->draw_state.draw_call_count - before_draws);
        before_draws = vk->draw_state.draw_call_count;
    }

    UIPanelState* panel = UIPanel_Get();
    Render_UIPanel(panel, ctx->renderer);
    UIPanel_RenderOverlays(ctx->renderer);
    if (!logged_counts && vk) {
        fprintf(stderr, "[LineDrawing] UI panel draw calls: %u\n",
                vk->draw_state.draw_call_count - before_draws);
        logged_counts = 1;
    }

}
