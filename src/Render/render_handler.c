#include "render_handler.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"

#include "UI/ui_panel.h"
#include "UI/render_ui_panel.h"
#include "UI/info_overlay.h"
#include "UI/shared_theme_font_adapter.h"

#include "Layout/Grid/render_grid.h"
#include "Layout/layout.h"
#include "Layout/render_layout.h"     // ← NEW: Layout wall + anchor renderer
#include "Math/math_util.h"
#include "Render/vulkan_adapter.h"
#include "Editor/editor.h"
#include "Editor/render_editor.h"
#include "vk_renderer.h"
#include <SDL2/SDL.h>
#include <math.h>

static void DrawAxisArrow(SDL_Renderer* renderer,
                          const Grid* grid,
                          Vec2 from,
                          Vec2 to,
                          SDL_Color color,
                          Vec3 axisDirWorld,
                          const FreeViewCamera* camera) {
    Vec2 a = WorldToScreen(from, grid);
    Vec2 b = WorldToScreen(to, grid);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, (int)a.x, (int)a.y, (int)b.x, (int)b.y);

    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len <= 1.0f) return;

    float ux = dx / len;
    float uy = dy / len;
    float px = -uy;
    float py = ux;
    float foreshorten = 1.0f;
    if (camera) {
        Vec3 forward = FreeView_Forward(camera);
        float alongView = fabsf(Vec3_Dot(Vec3_Normalize(axisDirWorld), forward));
        foreshorten = 1.0f - alongView;
        if (foreshorten < 0.0f) foreshorten = 0.0f;
    }

    float headLen = (1.5f + 9.5f * foreshorten);
    float headW = (0.6f + 4.4f * foreshorten);
    Vec2 h1 = { b.x - ux * headLen + px * headW, b.y - uy * headLen + py * headW };
    Vec2 h2 = { b.x - ux * headLen - px * headW, b.y - uy * headLen - py * headW };
    SDL_RenderDrawLine(renderer, (int)b.x, (int)b.y, (int)h1.x, (int)h1.y);
    SDL_RenderDrawLine(renderer, (int)b.x, (int)b.y, (int)h2.x, (int)h2.y);
}

static void Render_FreeViewAxisGizmo(SDL_Renderer* renderer, const GlobalState* state) {
    if (!renderer || !state) return;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (!SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return;

    bool hasAnchors = false;
    Vec3 center = Layout_ComputeCentroid(&state->layout, &hasAnchors);
    if (!hasAnchors) center = viewCtx.camera.target;

    float axisLen = fmaxf(state->grid.gridSize * 4.0f, 2.0f);
    Vec3 xEnd = Vec3_Add(center, (Vec3){ axisLen, 0.0f, 0.0f });
    Vec3 yEnd = Vec3_Add(center, (Vec3){ 0.0f, axisLen, 0.0f });
    Vec3 zEnd = Vec3_Add(center, (Vec3){ 0.0f, 0.0f, axisLen });

    Vec2 c2 = SpaceAdapter_ProjectToView(center, &viewCtx);
    Vec2 x2 = SpaceAdapter_ProjectToView(xEnd, &viewCtx);
    Vec2 y2 = SpaceAdapter_ProjectToView(yEnd, &viewCtx);
    Vec2 z2 = SpaceAdapter_ProjectToView(zEnd, &viewCtx);

    DrawAxisArrow(renderer, &state->grid, c2, x2, (SDL_Color){ 255, 70, 70, 255 },
                  (Vec3){ 1.0f, 0.0f, 0.0f }, SpaceAdapter_Camera(&viewCtx));   // +X
    DrawAxisArrow(renderer, &state->grid, c2, y2, (SDL_Color){ 70, 255, 70, 255 },
                  (Vec3){ 0.0f, 1.0f, 0.0f }, SpaceAdapter_Camera(&viewCtx));   // +Y
    DrawAxisArrow(renderer, &state->grid, c2, z2, (SDL_Color){ 90, 160, 255, 255 },
                  (Vec3){ 0.0f, 0.0f, 1.0f }, SpaceAdapter_Camera(&viewCtx));  // +Z
}

void Render_Frame(AppContext* ctx) {
    static int logged_counts = 0;

    GlobalState* state = Global_Get();
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    Global_RebuildHitboxesIfDirty();
    Grid* grid = &state->grid;
    Layout* layout = &state->layout;
    EditorState* editor = &state->editor;

    VkRenderer* vk = (VkRenderer*)ctx->renderer;
    uint32_t before_draws = vk ? vk->draw_state.draw_call_count : 0;

    int w = Global_GetScreenWidth();
    int h = Global_GetScreenHeight();
    LineDrawing3dThemePalette palette = {0};
    SDL_Color background = {20, 20, 23, 255};
    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        background = palette.background_fill;
    }
    VulkanAdapter_Clear(ctx->renderer, w, h, background);

    // ─── Grid ────────────────────────────────────
    // Render_Grid(grid, ctx->renderer, w, h, GRID_DRAW_UNITS);
    if (!SpaceAdapter_IsFreeViewEnabled(&viewCtx)) {
        Render_Grid(grid, ctx->renderer, w, h, GRID_DRAW_UNITS | GRID_DRAW_FIVES);
    }
    // Render_Grid(grid, ctx->renderer, w, h, GRID_DRAW_UNITS | GRID_DRAW_FIVES | GRID_DRAW_TENS);
    if (!logged_counts && vk) {
        fprintf(stderr, "[LineDrawing] Grid draw calls: %u\n",
                vk->draw_state.draw_call_count - before_draws);
        before_draws = vk->draw_state.draw_call_count;
    }

    // ─── Layout Geometry (Walls + Anchors) ────────
    Layout_Render(layout, ctx);
    Render_FreeViewAxisGizmo(ctx->renderer, state);
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
