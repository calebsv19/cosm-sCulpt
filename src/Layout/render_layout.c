// src/Layout/layout_render.c
#include "Layout/render_layout.h"
#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Math/math_util.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdlib.h>

static void DrawLineWithThickness(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int thickness) {
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    if (thickness <= 1) return;

    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int half = thickness / 2;

    if (dx >= dy) {
        for (int offset = 1; offset <= half; ++offset) {
            SDL_RenderDrawLine(renderer, x1, y1 + offset, x2, y2 + offset);
            SDL_RenderDrawLine(renderer, x1, y1 - offset, x2, y2 - offset);
        }
    } else {
        for (int offset = 1; offset <= half; ++offset) {
            SDL_RenderDrawLine(renderer, x1 + offset, y1, x2 + offset, y2);
            SDL_RenderDrawLine(renderer, x1 - offset, y1, x2 - offset, y2);
        }
    }
}

static void DrawFilledCircle(SDL_Renderer* renderer, int cx, int cy, int r) {
    int r2 = r * r;
    for (int dy = -r; dy <= r; ++dy) {
        int dx = (int)sqrtf((float)(r2 - dy * dy));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static bool Anchor_GetHandleForWall(const Layout* layout,
                                    int anchorIndex,
                                    const Anchor* anchor,
                                    int wallIndex,
                                    Vec2* outHandle) {
    if (!layout || !anchor || anchor->type != ANCHOR_TYPE_CURVE || !outHandle) return false;
    if (wallIndex < 0 || (size_t)wallIndex >= layout->wallCount) return false;

    const Wall* wall = &layout->walls[wallIndex];
    bool isAnchorA = (wall->anchorA == anchorIndex);

    float length = isAnchorA ? anchor->handleOutLength : anchor->handleInLength;
    float angle = isAnchorA ? anchor->handleOutAngleDeg : anchor->handleInAngleDeg;
    if (length <= 0.0001f) return false;

    *outHandle = Vec2_HandleAbsolute(anchor->pos, length, angle);
    return true;
}

static Vec2 CubicEval(Vec2 p0, Vec2 c1, Vec2 c2, Vec2 p3, float t) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    Vec2 result = {0, 0};
    result.x = uuu * p0.x;
    result.y = uuu * p0.y;

    result.x += 3 * uu * t * c1.x;
    result.y += 3 * uu * t * c1.y;

    result.x += 3 * u * tt * c2.x;
    result.y += 3 * u * tt * c2.y;

    result.x += ttt * p3.x;
    result.y += ttt * p3.y;
    return result;
}

static void RenderBezier(SDL_Renderer* renderer, const Grid* grid,
                         Vec2 p0, Vec2 c1, Vec2 c2, Vec2 p3, int thickness) {
    const int segments = 32;
    Vec2 prevWorld = p0;
    Vec2 prevScreen = WorldToScreen(prevWorld, grid);

    for (int i = 1; i <= segments; ++i) {
        float t = (float)i / (float)segments;
        Vec2 worldPt = CubicEval(p0, c1, c2, p3, t);
        Vec2 screenPt = WorldToScreen(worldPt, grid);

        DrawLineWithThickness(renderer,
                              (int)prevScreen.x, (int)prevScreen.y,
                              (int)screenPt.x, (int)screenPt.y,
                              thickness);
        prevWorld = worldPt;
        prevScreen = screenPt;
    }
}

//        Draw all walls (selected = yellow, others = gray)
// ======================================
static void Layout_RenderWalls(const Layout* layout, SDL_Renderer* renderer){
    GlobalState* state = Global_Get();
    const Grid* grid = &state->grid;
    EditorState* editor = &state->editor;
    int selectedIndex = editor->selectedWallIndex;
    int hoveredIndex = editor->hoveredWallIndex;

    for (size_t i = 0; i < layout->wallCount; ++i) {
        Wall w = layout->walls[i];
        if (w.isDeleted) continue;
        Vec2 from = layout->anchors[w.anchorA].pos;
        Vec2 to   = layout->anchors[w.anchorB].pos;

        int thickness = 1;
        if ((int)i == selectedIndex) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);  // Yellow
            thickness = 3;
        } else if ((int)i == hoveredIndex) {
            SDL_SetRenderDrawColor(renderer, 180, 220, 255, 255);
            thickness = 2;
        } else {
            SDL_SetRenderDrawColor(renderer, 100, 100, 220, 255);  // Gray
        }

        Vec2 handleA = from;
        Vec2 handleB = to;
        bool hasHandleA = Anchor_GetHandleForWall(layout, w.anchorA, &layout->anchors[w.anchorA], (int)i, &handleA);
        bool hasHandleB = Anchor_GetHandleForWall(layout, w.anchorB, &layout->anchors[w.anchorB], (int)i, &handleB);

        if (hasHandleA || hasHandleB) {
            RenderBezier(renderer, grid, from, handleA, handleB, to, thickness);
        } else {
            Vec2 p1 = WorldToScreen(from, grid);
            Vec2 p2 = WorldToScreen(to, grid);
            DrawLineWithThickness(renderer,
                                  (int)p1.x, (int)p1.y,
                                  (int)p2.x, (int)p2.y,
                                  thickness);
        }
    }
}

static void Layout_RenderHandles(const Layout* layout, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    const Grid* grid = &state->grid;
    EditorState* editor = &state->editor;

    for (size_t i = 0; i < layout->anchorCount; ++i) {
        const Anchor* anchor = &layout->anchors[i];
        if (anchor->isDeleted || anchor->type != ANCHOR_TYPE_CURVE) continue;

        Vec2 anchorScreen = WorldToScreen(anchor->pos, grid);
        Vec2 handles[2] = {
            Vec2_HandleAbsolute(anchor->pos, anchor->handleInLength, anchor->handleInAngleDeg),
            Vec2_HandleAbsolute(anchor->pos, anchor->handleOutLength, anchor->handleOutAngleDeg)
        };

        for (int h = 0; h < 2; ++h) {
            Vec2 handlePos = handles[h];
            Vec2 handleScreen = WorldToScreen(handlePos, grid);

            SDL_SetRenderDrawColor(renderer, 120, 120, 180, 255);
            DrawLineWithThickness(renderer,
                                  (int)anchorScreen.x, (int)anchorScreen.y,
                                  (int)handleScreen.x, (int)handleScreen.y,
                                  1);

            bool isSelected = (editor->selectedHandleAnchor == (int)i &&
                               editor->selectedHandleComponent == h);
            bool isHovered = (editor->hoveredHandleAnchor == (int)i &&
                              editor->hoveredHandleComponent == h);

            if (isSelected) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            } else if (isHovered) {
                SDL_SetRenderDrawColor(renderer, 200, 220, 255, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 150, 150, 220, 255);
            }

            int radius = SDL_max(3, (int)(grid->gridSize * grid->scale * 0.08f));
            DrawFilledCircle(renderer, (int)handleScreen.x, (int)handleScreen.y, radius);
        }
    }
}


//        Draw wall anchor points (white dots at both ends)
// ======================================
static void Layout_RenderAnchors(const Layout* layout, SDL_Renderer* renderer){
    GlobalState* state = Global_Get();
    const Grid* grid = &state->grid;
    EditorState* editor = &state->editor;

    int r = ANCHOR_RENDER_RADIUS;

    int hoveredAnchor = editor->hoveredAnchorIndex;

    for (size_t i = 0; i < layout->anchorCount; ++i) {
        Anchor* anchor = &layout->anchors[i];
        if (anchor->isDeleted) continue;
        Vec2 screen = WorldToScreen(anchor->pos, grid);
        int cx = (int)screen.x;
        int cy = (int)screen.y;

        bool isSelected = Editor_IsAnchorSelected(editor, (int)i);
        bool isHovered = ((int)i == hoveredAnchor);
        bool isPinned = anchor->isPersistent;
        bool isDragging = (editor->isDraggingAnchor && editor->selectedAnchorIndex == (int)i);

        // Color logic
        Uint8 rC = 255, gC = 255, bC = 255; // Default white
        if (isPinned && isSelected) {
            rC = 255; gC = 100; bC = 100;
        } else if (isPinned) {
            rC = 255; gC = 50;  bC = 50;
        } else if (isDragging) {
            rC = 255; gC = 180; bC = 80;
        } else if (isSelected) {
            rC = 255; gC = 255; bC = 0;
        } else if (isHovered) {
            rC = 120; gC = 200; bC = 255;
        }

        SDL_SetRenderDrawColor(renderer, rC, gC, bC, 255);
        DrawFilledCircle(renderer, cx, cy, r);

        if (isSelected || isHovered || isDragging) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            int ringRadius = r + 2;
            int inner = r * r;
            int outer = ringRadius * ringRadius;
            for (int dy = -ringRadius; dy <= ringRadius; ++dy) {
                int outer_dx = (int)sqrtf((float)(outer - dy * dy));
                int inner_dx = 0;
                if (dy * dy < inner) {
                    inner_dx = (int)sqrtf((float)(inner - dy * dy));
                }
                if (inner_dx < outer_dx) {
                    SDL_RenderDrawLine(renderer,
                                       cx - outer_dx, cy + dy,
                                       cx - inner_dx, cy + dy);
                    SDL_RenderDrawLine(renderer,
                                       cx + inner_dx, cy + dy,
                                       cx + outer_dx, cy + dy);
                }
            }
        }
    }
}


//        Public: Main layout renderer
// ======================================
void Layout_Render(const Layout* layout, AppContext* ctx) {
    SDL_Renderer* renderer = ctx->renderer;

    Layout_RenderWalls(layout, renderer);
    Layout_RenderHandles(layout, renderer);
    Layout_RenderAnchors(layout, renderer);
}
