#include "hitbox_system.h"
#include "Editor/space_gizmo_drag.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>

#define MAX_HITBOXES 1024
static Hitbox hitboxes[MAX_HITBOXES];
static size_t hitboxCount = 0;

static int Hitbox_TypePriority(HitboxType type) {
    switch (type) {
        case HITBOX_GIZMO_AXIS: return 0;
        case HITBOX_HANDLE: return 1;
        case HITBOX_POINT: return 2;
        case HITBOX_WALL: return 3;
        case HITBOX_NONE:
        default: return 4;
    }
}

static float Hitbox_CenterDistanceSq(const Hitbox* h, int mx, int my) {
    float cx = (float)h->bounds.x + (float)h->bounds.w * 0.5f;
    float cy = (float)h->bounds.y + (float)h->bounds.h * 0.5f;
    float dx = (float)mx - cx;
    float dy = (float)my - cy;
    return dx * dx + dy * dy;
}

static void Hitbox_Add(HitboxType type,
                       int index,
                       int subIndex,
                       SDL_Rect bounds,
                       float depthDistance) {
    if (hitboxCount >= MAX_HITBOXES) return;
    hitboxes[hitboxCount++] = (Hitbox){
        .type = type,
        .index = index,
        .subIndex = subIndex,
        .bounds = bounds,
        .depthDistance = depthDistance
    };
}

static void Hitbox_AddSelectedAnchorGizmo(const Layout* layout,
                                          float scale,
                                          float offsetX,
                                          float offsetY,
                                          ViewPlane plane,
                                          const FreeViewCamera* camera,
                                          int selectedAnchorIndex,
                                          bool gizmoEnabled) {
    if (!layout || !gizmoEnabled) return;
    if (selectedAnchorIndex < 0 || (size_t)selectedAnchorIndex >= layout->anchorCount) return;

    const Anchor* anchor = &layout->anchors[selectedAnchorIndex];
    if (anchor->isDeleted) return;

    const float gridSize = layout->gridSize;
    const float axisWorldLen = fmaxf(gridSize * 2.0f, 1.0f);
    const int handleRadius = SDL_max(6, (int)(gridSize * scale * 0.13f));

    const Vec2 anchorView = Vec3_ProjectToView(anchor->pos, plane, camera);
    const Vec2 anchorScreen = {
        .x = (anchorView.x - offsetX) * gridSize * scale,
        .y = (anchorView.y - offsetY) * gridSize * scale
    };

    for (int dir = GIZMO_AXIS_DIR_POS_X; dir <= GIZMO_AXIS_DIR_NEG_Z; ++dir) {
        const Vec3 axisWorld = GizmoAxisDirection_WorldVector((GizmoAxisDirection)dir);
        const Vec3 tipWorld = Vec3_Add(anchor->pos, Vec3_Scale(axisWorld, axisWorldLen));
        const Vec2 tipView = Vec3_ProjectToView(tipWorld, plane, camera);
        const Vec2 tipScreen = {
            .x = (tipView.x - offsetX) * gridSize * scale,
            .y = (tipView.y - offsetY) * gridSize * scale
        };

        const float dx = tipScreen.x - anchorScreen.x;
        const float dy = tipScreen.y - anchorScreen.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 <= 9.0f) {
            continue;
        }

        const int cx = (int)tipScreen.x;
        const int cy = (int)tipScreen.y;
        Hitbox_Add(HITBOX_GIZMO_AXIS,
                   selectedAnchorIndex,
                   dir,
                   (SDL_Rect){ cx - handleRadius, cy - handleRadius, handleRadius * 2, handleRadius * 2 },
                   ViewPlane_AbsDistance(plane, anchor->pos));
    }
}

void HitboxSystem_Rebuild(const Layout* layout,
                         float scale,
                         float offsetX,
                         float offsetY,
                         ViewPlane plane,
                         const FreeViewCamera* camera,
                         int selectedAnchorIndex,
                         bool gizmoEnabled) {
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

        Vec2 from = Vec3_ProjectToView(anchorA.pos, plane, camera);
        Vec2 to   = Vec3_ProjectToView(anchorB.pos, plane, camera);

        int x1 = (int)((from.x - offsetX) * gridSize * scale);
        int y1 = (int)((from.y - offsetY) * gridSize * scale);
        int x2 = (int)((to.x   - offsetX) * gridSize * scale);
        int y2 = (int)((to.y   - offsetY) * gridSize * scale);

        // ── Wall line as fat rectangle hitbox ──
        int minX = SDL_min(x1, x2) - 6;
        int maxX = SDL_max(x1, x2) + 6;
        int minY = SDL_min(y1, y2) - 6;
        int maxY = SDL_max(y1, y2) + 6;

        float wallDepth = 0.5f * (ViewPlane_AbsDistance(plane, anchorA.pos) +
                                  ViewPlane_AbsDistance(plane, anchorB.pos));
        Hitbox_Add(HITBOX_WALL,
                   (int)i,
                   -1,
                   (SDL_Rect){ minX, minY, maxX - minX, maxY - minY },
                   wallDepth);

        // ── Anchor points hitboxes (large circular area) ──
        int r = (int)(gridSize * scale * 0.15f);

        int anchorIDs[2] = { a, b };
        for (int j = 0; j < 2; ++j) {
            int anchorIndex = anchorIDs[j];
            Anchor anchor = layout->anchors[anchorIndex];
            if (anchor.isDeleted) continue;
            Vec2 pt = Vec3_ProjectToView(anchor.pos, plane, camera);

            int cx = (int)((pt.x - offsetX) * gridSize * scale);
            int cy = (int)((pt.y - offsetY) * gridSize * scale);

            Hitbox_Add(HITBOX_POINT,
                       anchorIndex,
                       -1,
                       (SDL_Rect){ cx - r, cy - r, r * 2, r * 2 },
                       ViewPlane_AbsDistance(plane, anchor.pos));
            if (anchorHasHitbox) anchorHasHitbox[anchorIndex] = true;
        }
    }

    if (anchorHasHitbox) {
        for (size_t i = 0; i < layout->anchorCount; ++i) {
            if (anchorHasHitbox[i]) continue;
            const Anchor* anchor = &layout->anchors[i];
            if (anchor->isDeleted) continue;

            Vec2 pt = Vec3_ProjectToView(anchor->pos, plane, camera);
            int r = (int)(gridSize * scale * 0.15f);
            int cx = (int)((pt.x - offsetX) * gridSize * scale);
            int cy = (int)((pt.y - offsetY) * gridSize * scale);

            Hitbox_Add(HITBOX_POINT,
                       (int)i,
                       -1,
                       (SDL_Rect){ cx - r, cy - r, r * 2, r * 2 },
                       ViewPlane_AbsDistance(plane, anchor->pos));
        }
    }

    for (size_t i = 0; i < layout->anchorCount; ++i) {
        const Anchor* anchor = &layout->anchors[i];
        if (anchor->isDeleted) continue;
        if (anchor->type != ANCHOR_TYPE_CURVE) continue;

        Vec2 handles[2] = {
            Vec3_ProjectToView(Vec3_HandleWorldPosition(anchor->pos, anchor->handleAxis,
                                                        anchor->handleInLength, anchor->handleInAngleDeg),
                               plane, camera),
            Vec3_ProjectToView(Vec3_HandleWorldPosition(anchor->pos, anchor->handleAxis,
                                                        anchor->handleOutLength, anchor->handleOutAngleDeg),
                               plane, camera)
        };

        for (int h = 0; h < 2; ++h) {
            Vec2 handlePos = handles[h];
            int hx = (int)((handlePos.x - offsetX) * gridSize * scale);
            int hy = (int)((handlePos.y - offsetY) * gridSize * scale);
            int hr = SDL_max(4, (int)(gridSize * scale * 0.1f));

            Hitbox_Add(HITBOX_HANDLE,
                       (int)i,
                       h,
                       (SDL_Rect){ hx - hr, hy - hr, hr * 2, hr * 2 },
                       ViewPlane_AbsDistance(plane, anchor->pos));
        }
    }

    Hitbox_AddSelectedAnchorGizmo(layout,
                                  scale,
                                  offsetX,
                                  offsetY,
                                  plane,
                                  camera,
                                  selectedAnchorIndex,
                                  gizmoEnabled);

    free(anchorHasHitbox);
}


Hitbox HitboxSystem_GetHitAt(int mx, int my) {
    int bestIdx = -1;
    int bestTypePriority = 0;
    float bestDepth = 0.0f;
    float bestDist = 0.0f;

    for (int i = 0; i < (int)hitboxCount; ++i) {
        SDL_Rect r = hitboxes[i].bounds;
        if (mx >= r.x && mx <= r.x + r.w &&
            my >= r.y && my <= r.y + r.h) {
            int typePriority = Hitbox_TypePriority(hitboxes[i].type);
            float depth = hitboxes[i].depthDistance;
            float dist = Hitbox_CenterDistanceSq(&hitboxes[i], mx, my);

            if (bestIdx < 0) {
                bestIdx = i;
                bestTypePriority = typePriority;
                bestDepth = depth;
                bestDist = dist;
                continue;
            }

            if (typePriority < bestTypePriority) {
                bestIdx = i;
                bestTypePriority = typePriority;
                bestDepth = depth;
                bestDist = dist;
                continue;
            }
            if (typePriority > bestTypePriority) continue;

            if (depth + 1e-4f < bestDepth) {
                bestIdx = i;
                bestDepth = depth;
                bestDist = dist;
                continue;
            }
            if (bestDepth + 1e-4f < depth) continue;

            if (dist + 0.25f < bestDist) {
                bestIdx = i;
                bestDist = dist;
                continue;
            }
            if (bestDist + 0.25f < dist) continue;

            // Stable deterministic tie-breaker: prefer most recently inserted.
            bestIdx = i;
        }
    }

    if (bestIdx >= 0) return hitboxes[bestIdx];
    return (Hitbox){ .type = HITBOX_NONE, .index = -1, .subIndex = -1 };
}
