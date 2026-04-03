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
#include <string.h>

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

static void Render_ViewCenterCrosshair(SDL_Renderer* renderer, const GlobalState* state) {
    if (!renderer || !state || !state->centerCrosshairEnabled) return;

    const int cx = state->screenWidth / 2;
    const int cy = state->screenHeight / 2;
    const int half = 6;
    const int gap = 2;

    SDL_SetRenderDrawColor(renderer, 245, 245, 245, 200);
    SDL_RenderDrawLine(renderer, cx - half, cy, cx - gap, cy);
    SDL_RenderDrawLine(renderer, cx + gap, cy, cx + half, cy);
    SDL_RenderDrawLine(renderer, cx, cy - half, cx, cy - gap);
    SDL_RenderDrawLine(renderer, cx, cy + gap, cx, cy + half);

    SDL_SetRenderDrawColor(renderer, 255, 180, 80, 220);
    SDL_RenderDrawPoint(renderer, cx, cy);
}

static void LogDrawCallDelta(const char* label,
                             bool should_log,
                             VkRenderer* vk,
                             uint32_t* inout_before_draws) {
    if (!label || !should_log || !vk || !inout_before_draws) return;
    fprintf(stderr,
            "[LineDrawing] %s draw calls: %u\n",
            label,
            vk->draw_state.draw_call_count - *inout_before_draws);
    *inout_before_draws = vk->draw_state.draw_call_count;
}

void Render_DeriveFrame(const LineDrawingUpdateFrame* update_frame,
                        AppContext* ctx,
                        LineDrawingRenderDeriveFrame* out_derive_frame) {
    GlobalState* state = NULL;
    SpaceViewContext view_ctx = {0};
    LineDrawing3dThemePalette palette = {0};

    if (!out_derive_frame) return;
    memset(out_derive_frame, 0, sizeof(*out_derive_frame));
    if (!ctx) return;

    state = update_frame && update_frame->state_ready ? update_frame->state : Global_Get();
    if (!state) return;

    view_ctx = SpaceAdapter_BuildViewContext(state);

    out_derive_frame->valid = true;
    out_derive_frame->state = state;
    out_derive_frame->view_ctx = view_ctx;
    out_derive_frame->grid = &state->grid;
    out_derive_frame->layout = &state->layout;
    out_derive_frame->editor = &state->editor;
    out_derive_frame->screen_width = Global_GetScreenWidth();
    out_derive_frame->screen_height = Global_GetScreenHeight();
    out_derive_frame->background = (SDL_Color){20, 20, 23, 255};
    out_derive_frame->free_view_enabled = SpaceAdapter_IsFreeViewEnabled(&view_ctx);

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        out_derive_frame->background = palette.background_fill;
    }

    {
        VkRenderer* vk = (VkRenderer*)ctx->renderer;
        out_derive_frame->vk_draw_calls_before = vk ? vk->draw_state.draw_call_count : 0u;
    }
}

void Render_SubmitFrame(AppContext* ctx,
                        const LineDrawingRenderDeriveFrame* derive_frame) {
    static int logged_counts = 0;
    VkRenderer* vk = NULL;
    uint32_t before_draws = 0;
    bool should_log = false;
    UIPanelState* panel = NULL;

    if (!ctx || !derive_frame || !derive_frame->valid || !derive_frame->state) return;

    vk = (VkRenderer*)ctx->renderer;
    before_draws = derive_frame->vk_draw_calls_before;
    should_log = (logged_counts == 0);

    VulkanAdapter_Clear(ctx->renderer,
                        derive_frame->screen_width,
                        derive_frame->screen_height,
                        derive_frame->background);

    if (!derive_frame->free_view_enabled) {
        Render_Grid(derive_frame->grid,
                    ctx->renderer,
                    derive_frame->screen_width,
                    derive_frame->screen_height,
                    GRID_DRAW_UNITS | GRID_DRAW_FIVES);
    }
    LogDrawCallDelta("Grid", should_log, vk, &before_draws);

    Layout_Render(derive_frame->layout, ctx);
    Render_FreeViewAxisGizmo(ctx->renderer, derive_frame->state);
    LogDrawCallDelta("Layout", should_log, vk, &before_draws);

    Render_Editor_AxisGizmo(derive_frame->editor, ctx);
    Render_Editor_Anchor(derive_frame->editor, ctx);
    Render_Editor_GhostWall(derive_frame->editor, ctx);
    Render_Editor_SelectionBox(derive_frame->editor, ctx);
    Render_ViewCenterCrosshair(ctx->renderer, derive_frame->state);
    LogDrawCallDelta("Editor overlay", should_log, vk, &before_draws);

    Render_InfoOverlay(ctx->renderer);
    LogDrawCallDelta("Info overlay", should_log, vk, &before_draws);

    panel = UIPanel_Get();
    Render_UIPanel(panel, ctx->renderer);
    UIPanel_RenderOverlays(ctx->renderer);
    LogDrawCallDelta("UI panel", should_log, vk, &before_draws);

    if (should_log && vk) {
        logged_counts = 1;
    }
}

void Render_Frame(AppContext* ctx) {
    LineDrawingRenderDeriveFrame derive_frame;
    Render_DeriveFrame(NULL, ctx, &derive_frame);
    Render_SubmitFrame(ctx, &derive_frame);
}
