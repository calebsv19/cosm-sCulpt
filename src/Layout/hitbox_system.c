#include "hitbox_system.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>

#define MAX_HITBOXES 1024
static Hitbox hitboxes[MAX_HITBOXES];
static size_t hitboxCount = 0;


void HitboxSystem_Rebuild(const Layout* layout, float scale, float offsetX, float offsetY) {
    hitboxCount = 0;
    float gridSize = layout->gridSize;
    bool* anchorHasHitbox = NULL;
    if (layout->anchorCount > 0) {
        anchorHasHitbox = calloc(layout->anchorCount, sizeof(bool));
    }

    for (size_t i = 0; i < layout->wallCount; ++i) {
        Wall w = layout->walls[i];
        if (w.isDeleted) continue;
        int a = w.anchorA;
        int b = w.anchorB;

        // Safety check: prevent crash on invalid anchor indices
        if (a >= (int)layout->anchorCount || b >= (int)layout->anchorCount) {
            fprintf(stderr, "[HitboxSystem] Warning: Wall %zu references invalid anchor (%d or %d)\n", i, a, b);
            continue;
        }

        Anchor anchorA = layout->anchors[a];
        Anchor anchorB = layout->anchors[b];
        if (anchorA.isDeleted || anchorB.isDeleted) continue;

        Vec2 from = anchorA.pos;
        Vec2 to   = anchorB.pos;

        int x1 = (int)((from.x - offsetX) * gridSize * scale);
        int y1 = (int)((from.y - offsetY) * gridSize * scale);
        int x2 = (int)((to.x   - offsetX) * gridSize * scale);
        int y2 = (int)((to.y   - offsetY) * gridSize * scale);

        // ── Wall line as fat rectangle hitbox ──
        int minX = SDL_min(x1, x2) - 6;
        int maxX = SDL_max(x1, x2) + 6;
        int minY = SDL_min(y1, y2) - 6;
        int maxY = SDL_max(y1, y2) + 6;

        if (hitboxCount < MAX_HITBOXES) {
            hitboxes[hitboxCount++] = (Hitbox){
                .type = HITBOX_WALL,
                .index = (int)i,  // wall index
                .subIndex = -1,
                .bounds = { minX, minY, maxX - minX, maxY - minY }
            };
        }

        // ── Anchor points hitboxes (large circular area) ──
        int r = (int)(gridSize * scale * 0.15f);

        int anchorIDs[2] = { a, b };
        for (int j = 0; j < 2; ++j) {
            int anchorIndex = anchorIDs[j];
            Anchor anchor = layout->anchors[anchorIndex];
            if (anchor.isDeleted) continue;
            Vec2 pt = anchor.pos;

            int cx = (int)((pt.x - offsetX) * gridSize * scale);
            int cy = (int)((pt.y - offsetY) * gridSize * scale);

            if (hitboxCount < MAX_HITBOXES) {
                hitboxes[hitboxCount++] = (Hitbox){
                    .type = HITBOX_POINT,
                    .index = anchorIndex,  // anchor index, not wall
                    .subIndex = -1,
                    .bounds = { cx - r, cy - r, r * 2, r * 2 }
                };
            }
            if (anchorHasHitbox) anchorHasHitbox[anchorIndex] = true;
        }
    }

    if (anchorHasHitbox) {
        for (size_t i = 0; i < layout->anchorCount; ++i) {
            if (anchorHasHitbox[i]) continue;
            const Anchor* anchor = &layout->anchors[i];
            if (anchor->isDeleted) continue;

            Vec2 pt = anchor->pos;
            int r = (int)(gridSize * scale * 0.15f);
            int cx = (int)((pt.x - offsetX) * gridSize * scale);
            int cy = (int)((pt.y - offsetY) * gridSize * scale);

            if (hitboxCount < MAX_HITBOXES) {
                hitboxes[hitboxCount++] = (Hitbox){
                    .type = HITBOX_POINT,
                    .index = (int)i,
                    .subIndex = -1,
                    .bounds = { cx - r, cy - r, r * 2, r * 2 }
                };
            }
        }
    }

    for (size_t i = 0; i < layout->anchorCount; ++i) {
        const Anchor* anchor = &layout->anchors[i];
        if (anchor->isDeleted) continue;
        if (anchor->type != ANCHOR_TYPE_CURVE) continue;

        Vec2 handles[2] = {
            Vec2_HandleAbsolute(anchor->pos, anchor->handleInLength, anchor->handleInAngleDeg),
            Vec2_HandleAbsolute(anchor->pos, anchor->handleOutLength, anchor->handleOutAngleDeg)
        };

        for (int h = 0; h < 2; ++h) {
            Vec2 handlePos = handles[h];
            int hx = (int)((handlePos.x - offsetX) * gridSize * scale);
            int hy = (int)((handlePos.y - offsetY) * gridSize * scale);
            int hr = SDL_max(4, (int)(gridSize * scale * 0.1f));

            if (hitboxCount < MAX_HITBOXES) {
                hitboxes[hitboxCount++] = (Hitbox){
                    .type = HITBOX_HANDLE,
                    .index = (int)i,
                    .subIndex = h,
                    .bounds = { hx - hr, hy - hr, hr * 2, hr * 2 }
                };
            }
        }
    }

    free(anchorHasHitbox);
}


Hitbox HitboxSystem_GetHitAt(int mx, int my) {
    for (int i = (int)hitboxCount - 1; i >= 0; --i) {  // reversed
        SDL_Rect r = hitboxes[i].bounds;
        if (mx >= r.x && mx <= r.x + r.w &&
            my >= r.y && my <= r.y + r.h)
        {
            return hitboxes[i];
        }
    }

    return (Hitbox){ .type = HITBOX_NONE, .index = -1, .subIndex = -1 };
}
