// src/Layout/layout_render.c
#include "Layout/render_layout.h"
#include "Core/global_state.h"
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

//        Draw all walls (selected = yellow, others = gray)
// ======================================
static void Layout_RenderWalls(const Layout* layout, SDL_Renderer* renderer){
    const Grid* grid = &Global_Get()->grid;  // Use actual grid state
    int selectedIndex = Global_Get()->editor.selectedWallIndex;

    for (size_t i = 0; i < layout->wallCount; ++i) {
        Wall w = layout->walls[i];
        if (w.isDeleted) continue;
        Vec2 from = layout->anchors[w.anchorA].pos;
        Vec2 to   = layout->anchors[w.anchorB].pos;

        int thickness = 1;
        if ((int)i == selectedIndex) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);  // Yellow
            thickness = 3;
        } else {
            SDL_SetRenderDrawColor(renderer, 100, 100, 220, 255);  // Gray
        }

        Vec2 p1 = WorldToScreen(from, grid);
        Vec2 p2 = WorldToScreen(to, grid);

        DrawLineWithThickness(renderer,
                              (int)p1.x, (int)p1.y,
                              (int)p2.x, (int)p2.y,
                              thickness);
    }
}


//        Draw wall anchor points (white dots at both ends)
// ======================================
static void Layout_RenderAnchors(const Layout* layout, SDL_Renderer* renderer){
    const Grid* grid = &Global_Get()->grid;

    int r = ANCHOR_RENDER_RADIUS;

    int selectedAnchor = Global_Get()->editor.selectedAnchorIndex;

    for (size_t i = 0; i < layout->anchorCount; ++i) {
        Anchor* anchor = &layout->anchors[i];
        if (anchor->isDeleted) continue;
        Vec2 screen = WorldToScreen(anchor->pos, grid);
        int cx = (int)screen.x;
        int cy = (int)screen.y;

        bool isSelected = ((int)i == selectedAnchor);
        bool isPinned = anchor->isPersistent;

        // Color logic
        Uint8 rC = 255, gC = 255, bC = 255; // Default white
        if (isPinned && isSelected) {
            rC = 255; gC = 100; bC = 100;
        } else if (isPinned) {
            rC = 255; gC = 50;  bC = 50;
        } else if (isSelected) {
            rC = 255; gC = 255; bC = 0;
        }

        SDL_SetRenderDrawColor(renderer, rC, gC, bC, 255);

        for (int dx = -r; dx <= r; ++dx) {
            for (int dy = -r; dy <= r; ++dy) {
                if (dx * dx + dy * dy <= r * r)
                    SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
            }
        }

        if (isSelected) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            int ringRadius = r + 2;
            int inner = r * r;
            int outer = ringRadius * ringRadius;
            for (int dx = -ringRadius; dx <= ringRadius; ++dx) {
                for (int dy = -ringRadius; dy <= ringRadius; ++dy) {
                    int dist = dx * dx + dy * dy;
                    if (dist <= outer && dist >= inner) {
                        SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
                    }
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
    Layout_RenderAnchors(layout, renderer);
}
