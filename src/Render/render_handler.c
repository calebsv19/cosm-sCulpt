#include "render_handler.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"

#include "UI/ui_panel.h"
#include "UI/render_ui_panel.h"
#include "UI/info_overlay.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/workspace_authoring/line_drawing_workspace_authoring_overlay.h"

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

static SDL_Rect PaneRectToClipRect(CorePaneRect rect) {
    SDL_Rect out = {0, 0, 0, 0};
    int x0 = (int)floorf(rect.x);
    int y0 = (int)floorf(rect.y);
    int x1 = (int)ceilf(rect.x + rect.width);
    int y1 = (int)ceilf(rect.y + rect.height);
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    out.x = x0;
    out.y = y0;
    out.w = x1 - x0;
    out.h = y1 - y0;
    return out;
}

static bool ResolvePaneClipRect(const GlobalState* state,
                                LineDrawingPaneRole role,
                                SDL_Rect* out_clip) {
    const LineDrawingPaneHost* pane_host = NULL;
    CorePaneRect pane_rect = {0};
    if (!state || !out_clip) return false;
    pane_host = &state->paneHost;
    if (!pane_host->initialized) return false;
    if (!LineDrawingPaneHost_GetRectForRole(pane_host, role, &pane_rect)) return false;
    *out_clip = PaneRectToClipRect(pane_rect);
    return out_clip->w > 0 && out_clip->h > 0;
}

static void ResolvePaneChromeColors(SDL_Color* out_top_fill,
                                    SDL_Color* out_side_fill,
                                    SDL_Color* out_center_fill,
                                    SDL_Color* out_border,
                                    SDL_Color fallback_center) {
    LineDrawing3dThemePalette palette = {0};
    SDL_Color top_fill = (SDL_Color){ 28, 30, 35, 235 };
    SDL_Color side_fill = (SDL_Color){ 24, 27, 33, 245 };
    SDL_Color center_fill = fallback_center;
    SDL_Color border = (SDL_Color){ 74, 82, 96, 255 };

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        top_fill = palette.panel_fill;
        side_fill = palette.panel_fill;
        center_fill = palette.background_fill;
        border = palette.panel_border;
    }

    top_fill.a = 235;
    side_fill.a = 245;
    center_fill.a = 255;
    border.a = 255;

    if (out_top_fill) *out_top_fill = top_fill;
    if (out_side_fill) *out_side_fill = side_fill;
    if (out_center_fill) *out_center_fill = center_fill;
    if (out_border) *out_border = border;
}

static void FillPaneRect(SDL_Renderer* renderer, const SDL_Rect* rect, SDL_Color color) {
    if (!renderer || !rect || rect->w <= 0 || rect->h <= 0) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    (void)SDL_RenderFillRect(renderer, rect);
}

static void RenderPaneChromeBase(SDL_Renderer* renderer,
                                 const SDL_Rect* top_rect,
                                 const SDL_Rect* left_rect,
                                 const SDL_Rect* center_rect,
                                 const SDL_Rect* right_rect,
                                 bool has_top,
                                 bool has_left,
                                 bool has_center,
                                 bool has_right,
                                 SDL_Color top_fill,
                                 SDL_Color side_fill,
                                 SDL_Color center_fill) {
    if (!renderer) return;
    if (has_top) FillPaneRect(renderer, top_rect, top_fill);
    if (has_left) FillPaneRect(renderer, left_rect, side_fill);
    if (has_center) FillPaneRect(renderer, center_rect, center_fill);
    if (has_right) FillPaneRect(renderer, right_rect, side_fill);
}

static void RenderPaneChromeBorders(SDL_Renderer* renderer,
                                    const SDL_Rect* top_rect,
                                    const SDL_Rect* left_rect,
                                    const SDL_Rect* center_rect,
                                    const SDL_Rect* right_rect,
                                    bool has_top,
                                    bool has_left,
                                    bool has_center,
                                    bool has_right,
                                    SDL_Color border) {
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    if (has_top) (void)SDL_RenderDrawRect(renderer, top_rect);
    if (has_left) (void)SDL_RenderDrawRect(renderer, left_rect);
    if (has_center) (void)SDL_RenderDrawRect(renderer, center_rect);
    if (has_right) (void)SDL_RenderDrawRect(renderer, right_rect);

    if (has_top) {
        const int y = top_rect->y + top_rect->h - 1;
        int x1 = top_rect->x;
        int x2 = top_rect->x + top_rect->w - 1;
        (void)SDL_RenderDrawLine(renderer, x1, y, x2, y);
    }
    if (has_left && has_center) {
        const int x = left_rect->x + left_rect->w - 1;
        const int y1 = left_rect->y;
        const int y2 = left_rect->y + left_rect->h - 1;
        (void)SDL_RenderDrawLine(renderer, x, y1, x, y2);
    }
    if (has_right && has_center) {
        const int x = right_rect->x;
        const int y1 = right_rect->y;
        const int y2 = right_rect->y + right_rect->h - 1;
        (void)SDL_RenderDrawLine(renderer, x, y1, x, y2);
    }
}

static void RenderPaneSplitterHandle(SDL_Renderer* renderer,
                                     const GlobalState* state,
                                     SDL_Color border) {
    CorePaneRect splitter_rect = {0};
    SDL_Rect handle_rect = {0};
    SDL_Color handle_fill = border;
    bool hovered = false;
    bool active = false;

    if (!renderer || !state) return;
    if (!LineDrawingPaneHost_GetVisibleSplitter(&state->paneHost,
                                                &splitter_rect,
                                                &hovered,
                                                &active)) {
        return;
    }

    handle_rect = PaneRectToClipRect(splitter_rect);
    if (handle_rect.w <= 0 || handle_rect.h <= 0) return;

    if (active) {
        handle_fill = (SDL_Color){ 230, 178, 92, 228 };
    } else if (hovered) {
        handle_fill = (SDL_Color){ 214, 220, 232, 170 };
    } else {
        handle_fill.a = 128;
    }

    FillPaneRect(renderer, &handle_rect, handle_fill);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    (void)SDL_RenderDrawRect(renderer, &handle_rect);
}

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
    SDL_Rect center_clip = {0, 0, 0, 0};
    SDL_Rect top_clip = {0, 0, 0, 0};
    SDL_Rect left_clip = {0, 0, 0, 0};
    SDL_Rect right_clip = {0, 0, 0, 0};
    bool has_center_clip = false;
    bool has_top_clip = false;
    bool has_left_clip = false;
    bool has_right_clip = false;
    SDL_Color chrome_top_fill = {0};
    SDL_Color chrome_side_fill = {0};
    SDL_Color chrome_center_fill = {0};
    SDL_Color chrome_border = {0};

    if (!ctx || !derive_frame || !derive_frame->valid || !derive_frame->state) return;

    vk = (VkRenderer*)ctx->renderer;
    before_draws = derive_frame->vk_draw_calls_before;
    should_log = (logged_counts == 0);
    has_center_clip = ResolvePaneClipRect(derive_frame->state, LINE_DRAWING_PANE_ROLE_CENTER_CANVAS, &center_clip);
    has_top_clip = ResolvePaneClipRect(derive_frame->state, LINE_DRAWING_PANE_ROLE_TOP_BAR, &top_clip);
    has_left_clip = ResolvePaneClipRect(derive_frame->state, LINE_DRAWING_PANE_ROLE_LEFT_CONTROLS, &left_clip);
    has_right_clip = ResolvePaneClipRect(derive_frame->state, LINE_DRAWING_PANE_ROLE_RIGHT_CONTROLS, &right_clip);

    VulkanAdapter_Clear(ctx->renderer,
                        derive_frame->screen_width,
                        derive_frame->screen_height,
                        derive_frame->background);
    ResolvePaneChromeColors(&chrome_top_fill,
                            &chrome_side_fill,
                            &chrome_center_fill,
                            &chrome_border,
                            derive_frame->background);
    RenderPaneChromeBase(ctx->renderer,
                         &top_clip,
                         &left_clip,
                         &center_clip,
                         &right_clip,
                         has_top_clip,
                         has_left_clip,
                         has_center_clip,
                         has_right_clip,
                         chrome_top_fill,
                         chrome_side_fill,
                         chrome_center_fill);

    if (has_center_clip) {
        (void)SDL_RenderSetClipRect(ctx->renderer, &center_clip);
    }
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
    if (has_center_clip) {
        (void)SDL_RenderSetClipRect(ctx->renderer, NULL);
    }
    LogDrawCallDelta("Editor overlay", should_log, vk, &before_draws);

    if (has_top_clip) {
        (void)SDL_RenderSetClipRect(ctx->renderer, &top_clip);
    }
    Render_InfoOverlay(ctx->renderer);
    if (has_top_clip) {
        (void)SDL_RenderSetClipRect(ctx->renderer, NULL);
    }
    LogDrawCallDelta("Info overlay", should_log, vk, &before_draws);

    panel = UIPanel_Get();
    if (has_left_clip) {
        (void)SDL_RenderSetClipRect(ctx->renderer, &left_clip);
    }
    Render_UIPanelSide(panel, ctx->renderer, UI_PANEL_LEFT);
    Render_UIPanelRootSummary(panel, ctx->renderer);
    if (has_left_clip) {
        (void)SDL_RenderSetClipRect(ctx->renderer, NULL);
    }

    if (has_right_clip) {
        (void)SDL_RenderSetClipRect(ctx->renderer, &right_clip);
    }
    Render_UIPanelSide(panel, ctx->renderer, UI_PANEL_RIGHT);
    Render_UIPanelObjectSummary(panel, ctx->renderer);
    if (has_right_clip) {
        (void)SDL_RenderSetClipRect(ctx->renderer, NULL);
    }

    RenderPaneChromeBorders(ctx->renderer,
                            &top_clip,
                            &left_clip,
                            &center_clip,
                            &right_clip,
                            has_top_clip,
                            has_left_clip,
                            has_center_clip,
                            has_right_clip,
                            chrome_border);
    RenderPaneSplitterHandle(ctx->renderer, derive_frame->state, chrome_border);

    UIPanel_RenderOverlays(ctx->renderer);
    LineDrawingWorkspaceAuthoringOverlay_Draw(ctx->renderer, derive_frame->state);
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
