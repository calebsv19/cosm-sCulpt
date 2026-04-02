// src/Render/render_editor.c
#include "Editor/render_editor.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Editor/space_gizmo_drag.h"
#include "Layout/Grid/grid.h"
#include "Math/math_util.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdlib.h>

static void DrawLineWithThickness(SDL_Renderer* renderer,
                                  int x1,
                                  int y1,
                                  int x2,
                                  int y2,
                                  int thickness) {
    if (thickness <= 1) {
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        return;
    }

    int half = thickness / 2;
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    if (dx >= dy) {
        for (int o = -half; o <= half; ++o) {
            SDL_RenderDrawLine(renderer, x1, y1 + o, x2, y2 + o);
        }
    } else {
        for (int o = -half; o <= half; ++o) {
            SDL_RenderDrawLine(renderer, x1 + o, y1, x2 + o, y2);
        }
    }
}

static void DrawFilledCircle(SDL_Renderer* renderer, int cx, int cy, int r) {
    if (r <= 0) return;
    int r2 = r * r;
    for (int dy = -r; dy <= r; ++dy) {
        int dx = (int)sqrtf((float)(r2 - dy * dy));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void DrawCircleRing(SDL_Renderer* renderer, int cx, int cy, int radius, int thickness) {
    if (radius <= 0 || thickness <= 0) return;
    const int innerR = SDL_max(0, radius - thickness);
    const int outerR = radius;
    const int inner2 = innerR * innerR;
    const int outer2 = outerR * outerR;
    for (int dy = -outerR; dy <= outerR; ++dy) {
        int y2 = dy * dy;
        if (y2 > outer2) continue;
        int outerDx = (int)sqrtf((float)(outer2 - y2));
        int innerDx = 0;
        if (y2 < inner2) innerDx = (int)sqrtf((float)(inner2 - y2));
        if (innerDx < outerDx) {
            SDL_RenderDrawLine(renderer, cx - outerDx, cy + dy, cx - innerDx, cy + dy);
            SDL_RenderDrawLine(renderer, cx + innerDx, cy + dy, cx + outerDx, cy + dy);
        }
    }
}

static SDL_Color GizmoAxis_BaseColor(GizmoAxisDirection dir) {
    switch (dir) {
        case GIZMO_AXIS_DIR_POS_X: return (SDL_Color){ 245, 85, 85, 255 };
        case GIZMO_AXIS_DIR_NEG_X: return (SDL_Color){ 170, 55, 55, 255 };
        case GIZMO_AXIS_DIR_POS_Y: return (SDL_Color){ 95, 235, 95, 255 };
        case GIZMO_AXIS_DIR_NEG_Y: return (SDL_Color){ 65, 165, 65, 255 };
        case GIZMO_AXIS_DIR_POS_Z: return (SDL_Color){ 105, 165, 245, 255 };
        case GIZMO_AXIS_DIR_NEG_Z: return (SDL_Color){ 70, 110, 185, 255 };
        default: return (SDL_Color){ 200, 200, 200, 255 };
    }
}

static SDL_Color GizmoAxis_ColorState(GizmoAxisDirection dir, bool hovered, bool active) {
    SDL_Color c = GizmoAxis_BaseColor(dir);
    if (hovered) {
        c.r = (Uint8)SDL_min(255, (int)c.r + 35);
        c.g = (Uint8)SDL_min(255, (int)c.g + 35);
        c.b = (Uint8)SDL_min(255, (int)c.b + 35);
    }
    if (active) {
        c.r = (Uint8)SDL_min(255, (int)c.r + 45);
        c.g = (Uint8)SDL_min(255, (int)c.g + 45);
        c.b = (Uint8)SDL_min(255, (int)c.b + 45);
    }
    return c;
}

// ─────────────────────────────────────────────
// High-level combined renderer
void Render_EditorOverlay(EditorState* editor, AppContext* ctx) {
    Render_Editor_AxisGizmo(editor, ctx);
    Render_Editor_Anchor(editor, ctx);
    Render_Editor_GhostWall(editor, ctx);
    // Future:
    // Render_Editor_SelectedWall(editor, ctx);
}

// ─────────────────────────────────────────────
// Draw the anchor point circle
void Render_Editor_Anchor(EditorState* editor, AppContext* ctx) {
    if (editor->mode != TOOL_PLACING_WALL) return;

    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);

    float gridSize = grid->gridSize;
    float scale = grid->scale;

    Vec2 from = SpaceAdapter_ProjectToView(editor->anchor, &viewCtx);

    int cx = (int)((from.x - grid->offsetX) * gridSize * scale);
    int cy = (int)((from.y - grid->offsetY) * gridSize * scale);
    int radius = (int)(gridSize * scale * 0.15f);

    SDL_SetRenderDrawColor(ctx->renderer, 255, 255, 100, 255);  // yellow
    int r2 = radius * radius;
    for (int dy = -radius; dy <= radius; ++dy) {
        int dx = (int)sqrtf((float)(r2 - dy * dy));
        SDL_RenderDrawLine(ctx->renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

// ─────────────────────────────────────────────
// Draw the ghost wall (anchor to mouse)
void Render_Editor_GhostWall(EditorState* editor, AppContext* ctx) {
    if (editor->mode != TOOL_PLACING_WALL) return;

    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);

    float gridSize = grid->gridSize;
    float scale = grid->scale;

    int mx, my;
    SDL_GetMouseState(&mx, &my);
    Vec3 toWorld = {0};
    if (!SpaceAdapter_ScreenToWorld(mx, my, grid, &viewCtx, true, &toWorld)) return;
    Vec2 to = SpaceAdapter_ProjectToView(toWorld, &viewCtx);

    Vec2 from = SpaceAdapter_ProjectToView(editor->anchor, &viewCtx);
    if (editor->shiftHeld) {
        float dx = fabsf(to.x - from.x);
        float dy = fabsf(to.y - from.y);
        if (dx > dy) to.y = from.y;
        else         to.x = from.x;
    }

    SDL_SetRenderDrawColor(ctx->renderer, 255, 100, 100, 255);  // red
    SDL_RenderDrawLine(ctx->renderer,
        (int)((from.x - grid->offsetX) * gridSize * scale),
        (int)((from.y - grid->offsetY) * gridSize * scale),
        (int)((to.x   - grid->offsetX) * gridSize * scale),
        (int)((to.y   - grid->offsetY) * gridSize * scale));
}

void Render_Editor_AxisGizmo(EditorState* editor, AppContext* ctx) {
    if (!editor || !ctx) return;

    GlobalState* state = Global_Get();
    if (!state) return;
    if (state->spaceMode != SPACE_MODE_3D) return;

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    if (!SpaceAdapter_IsFreeViewEnabled(&viewCtx)) return;

    const int selected = editor->selectedAnchorIndex;
    if (selected < 0 || (size_t)selected >= state->layout.anchorCount) return;
    const Anchor* anchor = &state->layout.anchors[selected];
    if (anchor->isDeleted) return;

    const Grid* grid = &state->grid;
    const float axisWorldLen = fmaxf(grid->gridSize * 2.0f, 1.0f);
    const int baseRadius = SDL_max(6, (int)(grid->gridSize * grid->scale * 0.13f));

    Vec2 anchorView = SpaceAdapter_ProjectToView(anchor->pos, &viewCtx);
    Vec2 anchorScreen = WorldToScreen(anchorView, grid);

    for (int dir = GIZMO_AXIS_DIR_POS_X; dir <= GIZMO_AXIS_DIR_NEG_Z; ++dir) {
        Vec3 axisDir = GizmoAxisDirection_WorldVector((GizmoAxisDirection)dir);
        Vec3 tipWorld = Vec3_Add(anchor->pos, Vec3_Scale(axisDir, axisWorldLen));
        Vec2 tipView = SpaceAdapter_ProjectToView(tipWorld, &viewCtx);
        Vec2 tipScreen = WorldToScreen(tipView, grid);

        float dx = tipScreen.x - anchorScreen.x;
        float dy = tipScreen.y - anchorScreen.y;
        float screenLen = sqrtf(dx * dx + dy * dy);
        if (screenLen <= 3.0f) continue;

        bool isHovered = (editor->hoveredGizmoAxis == dir);
        bool isActive = editor->gizmoDrag.active &&
                        editor->gizmoDrag.anchorIndex == selected &&
                        editor->gizmoDrag.axis == (GizmoAxisDirection)dir;
        SDL_Color axisColor = GizmoAxis_ColorState((GizmoAxisDirection)dir, isHovered, isActive);

        SDL_SetRenderDrawColor(ctx->renderer, axisColor.r, axisColor.g, axisColor.b, axisColor.a);
        DrawLineWithThickness(ctx->renderer,
                              (int)anchorScreen.x,
                              (int)anchorScreen.y,
                              (int)tipScreen.x,
                              (int)tipScreen.y,
                              (isHovered || isActive) ? 3 : 2);

        int handleRadius = baseRadius + (isHovered ? 1 : 0) + (isActive ? 1 : 0);
        DrawFilledCircle(ctx->renderer, (int)tipScreen.x, (int)tipScreen.y, handleRadius);

        SDL_Color ringColor = (SDL_Color){ 30, 30, 30, 255 };
        if (isHovered) ringColor = (SDL_Color){ 235, 235, 235, 255 };
        if (isActive) ringColor = (SDL_Color){ 255, 220, 120, 255 };
        SDL_SetRenderDrawColor(ctx->renderer, ringColor.r, ringColor.g, ringColor.b, ringColor.a);
        DrawCircleRing(ctx->renderer, (int)tipScreen.x, (int)tipScreen.y, handleRadius + 2, 2);
    }
}

void Render_Editor_SelectionBox(EditorState* editor, AppContext* ctx) {
    if (!editor->selectionBoxActive) return;

    GlobalState* state = Global_Get();
    const Grid* grid = &state->grid;

    Vec2 start = WorldToScreen(editor->selectionBoxStart, grid);
    Vec2 end = WorldToScreen(editor->selectionBoxEnd, grid);

    float minX = fminf(start.x, end.x);
    float minY = fminf(start.y, end.y);
    float maxX = fmaxf(start.x, end.x);
    float maxY = fmaxf(start.y, end.y);

    SDL_Rect rect = {
        .x = (int)minX,
        .y = (int)minY,
        .w = (int)(maxX - minX),
        .h = (int)(maxY - minY)
    };

    if (rect.w == 0 || rect.h == 0) return;

    SDL_SetRenderDrawColor(ctx->renderer, 80, 150, 255, 60);
    SDL_RenderFillRect(ctx->renderer, &rect);
    SDL_SetRenderDrawColor(ctx->renderer, 80, 150, 255, 200);
    SDL_RenderDrawRect(ctx->renderer, &rect);
}
