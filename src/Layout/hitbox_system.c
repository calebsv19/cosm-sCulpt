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
        case HITBOX_OBJECT3D_GIZMO_AXIS: return 0;
        case HITBOX_GIZMO_AXIS: return 1;
        case HITBOX_HANDLE: return 2;
        case HITBOX_OBJECT3D_PRISM_HANDLE: return 3;
        case HITBOX_OBJECT3D_PLANE_CORNER: return 4;
        case HITBOX_OBJECT3D_PLANE_EDGE: return 5;
        case HITBOX_POINT: return 6;
        case HITBOX_WALL: return 7;
        case HITBOX_OBJECT3D: return 8;
        case HITBOX_NONE:
        default: return 8;
    }
}

static float Hitbox_CenterDistanceSq(const Hitbox* h, int mx, int my) {
    float cx = (float)h->bounds.x + (float)h->bounds.w * 0.5f;
    float cy = (float)h->bounds.y + (float)h->bounds.h * 0.5f;
    float dx = (float)mx - cx;
    float dy = (float)my - cy;
    return dx * dx + dy * dy;
}

static float Hitbox_DepthDistance(ViewPlane plane,
                                  const FreeViewCamera* camera,
                                  Vec3 point) {
    if (camera && camera->enabled) {
        Vec3 forward = FreeView_Forward(camera);
        Vec3 delta = Vec3_Sub(point, camera->target);
        return fabsf(Vec3_Dot(delta, forward));
    }
    return ViewPlane_AbsDistance(plane, point);
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

static void Hitbox_AddObject3DCenterGizmoAxes(const Object3D* object,
                                              float gridSize,
                                              float scale,
                                              float offsetX,
                                              float offsetY,
                                              ViewPlane plane,
                                              const FreeViewCamera* camera) {
    if (!object) return;

    const float axisWorldLen = fmaxf(gridSize * 2.0f, 1.0f);
    const int gizmoRadius = SDL_max(6, (int)(gridSize * scale * 0.13f));
    const Vec3 center = object->transform.position;
    const Vec2 centerView = Vec3_ProjectToView(center, plane, camera);
    const float centerX = (centerView.x - offsetX) * gridSize * scale;
    const float centerY = (centerView.y - offsetY) * gridSize * scale;
    const float gizmoDepth = Hitbox_DepthDistance(plane, camera, center);

    for (int dir = GIZMO_AXIS_DIR_POS_X; dir <= GIZMO_AXIS_DIR_NEG_Z; ++dir) {
        const Vec3 axisWorld = GizmoAxisDirection_WorldVector((GizmoAxisDirection)dir);
        const Vec3 tipWorld = Vec3_Add(center, Vec3_Scale(axisWorld, axisWorldLen));
        const Vec2 tipView = Vec3_ProjectToView(tipWorld, plane, camera);
        const float tipX = (tipView.x - offsetX) * gridSize * scale;
        const float tipY = (tipView.y - offsetY) * gridSize * scale;
        const float dx = tipX - centerX;
        const float dy = tipY - centerY;
        if (dx * dx + dy * dy <= 9.0f) continue;

        Hitbox_Add(HITBOX_OBJECT3D_GIZMO_AXIS,
                   (int)object->objectId,
                   dir,
                   (SDL_Rect){ (int)tipX - gizmoRadius,
                               (int)tipY - gizmoRadius,
                               gizmoRadius * 2,
                               gizmoRadius * 2 },
                   gizmoDepth);
    }
}

static void Hitbox_AddObject3DPrimitives(const Layout* layout,
                                         float scale,
                                         float offsetX,
                                         float offsetY,
                                         ViewPlane plane,
                                         const FreeViewCamera* camera,
                                         uint32_t selectedObject3DId,
                                         int selectedObject3DResizeHandle,
                                         int selectedObject3DPrismHandle,
                                         bool gizmoEnabled) {
    if (!layout) return;
    const float gridSize = layout->gridSize;
    static const int kEdgeCornerA[4] = { 0, 1, 2, 3 };
    static const int kEdgeCornerB[4] = { 1, 2, 3, 0 };
    static const PlaneResizeHandleKind kEdgeHandle[4] = {
        PLANE_RESIZE_HANDLE_EDGE_NEG_V,
        PLANE_RESIZE_HANDLE_EDGE_POS_U,
        PLANE_RESIZE_HANDLE_EDGE_POS_V,
        PLANE_RESIZE_HANDLE_EDGE_NEG_U
    };
    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        if (object->isDeleted) continue;

        if (object->kind == OBJECT3D_KIND_PLANE) {
            Vec3 corners[4];
            if (!Layout_Object3D_ComputePlaneCorners(object, corners)) continue;

            float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
            for (int c = 0; c < 4; ++c) {
                Vec2 view = Vec3_ProjectToView(corners[c], plane, camera);
                const float sx = (view.x - offsetX) * gridSize * scale;
                const float sy = (view.y - offsetY) * gridSize * scale;
                if (c == 0) {
                    minX = maxX = sx;
                    minY = maxY = sy;
                } else {
                    if (sx < minX) minX = sx;
                    if (sx > maxX) maxX = sx;
                    if (sy < minY) minY = sy;
                    if (sy > maxY) maxY = sy;
                }
            }

            const int pad = 6;
            const int x = (int)floorf(minX) - pad;
            const int y = (int)floorf(minY) - pad;
            const int w = SDL_max(1, (int)ceilf(maxX - minX) + pad * 2);
            const int h = SDL_max(1, (int)ceilf(maxY - minY) + pad * 2);
            Hitbox_Add(HITBOX_OBJECT3D,
                       (int)object->objectId,
                       -1,
                       (SDL_Rect){ x, y, w, h },
                       Hitbox_DepthDistance(plane, camera, object->transform.position));

            if (object->objectId != selectedObject3DId) continue;

            const float handleDepth = Hitbox_DepthDistance(plane, camera, object->transform.position);
            const int cornerRadius = SDL_max(5, (int)(gridSize * scale * 0.12f));
            const int edgeRadius = SDL_max(4, (int)(gridSize * scale * 0.10f));
            for (int c = 0; c < 4; ++c) {
                Vec2 view = Vec3_ProjectToView(corners[c], plane, camera);
                const int sx = (int)((view.x - offsetX) * gridSize * scale);
                const int sy = (int)((view.y - offsetY) * gridSize * scale);
                Hitbox_Add(HITBOX_OBJECT3D_PLANE_CORNER,
                           (int)object->objectId,
                           PLANE_RESIZE_HANDLE_CORNER_NEG_U_NEG_V + c,
                           (SDL_Rect){ sx - cornerRadius, sy - cornerRadius, cornerRadius * 2, cornerRadius * 2 },
                           handleDepth);
            }

            for (int e = 0; e < 4; ++e) {
                Vec3 midpoint3 = Vec3_Scale(Vec3_Add(corners[kEdgeCornerA[e]], corners[kEdgeCornerB[e]]), 0.5f);
                Vec2 view = Vec3_ProjectToView(midpoint3, plane, camera);
                const int sx = (int)((view.x - offsetX) * gridSize * scale);
                const int sy = (int)((view.y - offsetY) * gridSize * scale);
                Hitbox_Add(HITBOX_OBJECT3D_PLANE_EDGE,
                           (int)object->objectId,
                           (int)kEdgeHandle[e],
                           (SDL_Rect){ sx - edgeRadius, sy - edgeRadius, edgeRadius * 2, edgeRadius * 2 },
                           handleDepth);
            }
            if (gizmoEnabled &&
                selectedObject3DResizeHandle == PLANE_RESIZE_HANDLE_NONE &&
                selectedObject3DPrismHandle == RECT_PRISM_RESIZE_HANDLE_NONE) {
                Hitbox_AddObject3DCenterGizmoAxes(object,
                                                  gridSize,
                                                  scale,
                                                  offsetX,
                                                  offsetY,
                                                  plane,
                                                  camera);
            }
        } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            Vec3 corners[8];
            if (!Layout_Object3D_ComputeRectPrismCorners(object, corners)) continue;

            float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
            for (int c = 0; c < 8; ++c) {
                Vec2 view = Vec3_ProjectToView(corners[c], plane, camera);
                const float sx = (view.x - offsetX) * gridSize * scale;
                const float sy = (view.y - offsetY) * gridSize * scale;
                if (c == 0) {
                    minX = maxX = sx;
                    minY = maxY = sy;
                } else {
                    if (sx < minX) minX = sx;
                    if (sx > maxX) maxX = sx;
                    if (sy < minY) minY = sy;
                    if (sy > maxY) maxY = sy;
                }
            }

            const int pad = 6;
            const int x = (int)floorf(minX) - pad;
            const int y = (int)floorf(minY) - pad;
            const int w = SDL_max(1, (int)ceilf(maxX - minX) + pad * 2);
            const int h = SDL_max(1, (int)ceilf(maxY - minY) + pad * 2);
            Hitbox_Add(HITBOX_OBJECT3D,
                       (int)object->objectId,
                       -1,
                       (SDL_Rect){ x, y, w, h },
                       Hitbox_DepthDistance(plane, camera, object->transform.position));

            if (object->objectId != selectedObject3DId) continue;

            const bool freeView = gizmoEnabled && camera && camera->enabled;
            if (freeView) {
                static const int kRectEdges[12][2] = {
                    {0, 1}, {1, 2}, {2, 3}, {3, 0},
                    {4, 5}, {5, 6}, {6, 7}, {7, 4},
                    {0, 4}, {1, 5}, {2, 6}, {3, 7}
                };
                const int cornerRadius = SDL_max(5, (int)(gridSize * scale * 0.12f));
                const int edgeRadius = SDL_max(4, (int)(gridSize * scale * 0.10f));
                for (int c = 0; c < 8; ++c) {
                    Vec2 view = Vec3_ProjectToView(corners[c], plane, camera);
                    const int sx = (int)((view.x - offsetX) * gridSize * scale);
                    const int sy = (int)((view.y - offsetY) * gridSize * scale);
                    Hitbox_Add(HITBOX_OBJECT3D_PRISM_HANDLE,
                               (int)object->objectId,
                               (int)(RECT_PRISM_RESIZE_HANDLE_CORNER_0 + c),
                               (SDL_Rect){ sx - cornerRadius, sy - cornerRadius, cornerRadius * 2, cornerRadius * 2 },
                               Hitbox_DepthDistance(plane, camera, corners[c]));
                }

                for (int e = 0; e < 12; ++e) {
                    Vec3 midpoint3 = Vec3_Scale(Vec3_Add(corners[kRectEdges[e][0]], corners[kRectEdges[e][1]]), 0.5f);
                    Vec2 view = Vec3_ProjectToView(midpoint3, plane, camera);
                    const int sx = (int)((view.x - offsetX) * gridSize * scale);
                    const int sy = (int)((view.y - offsetY) * gridSize * scale);
                    Hitbox_Add(HITBOX_OBJECT3D_PRISM_HANDLE,
                               (int)object->objectId,
                               (int)(RECT_PRISM_RESIZE_HANDLE_EDGE_0 + e),
                               (SDL_Rect){ sx - edgeRadius, sy - edgeRadius, edgeRadius * 2, edgeRadius * 2 },
                               Hitbox_DepthDistance(plane, camera, midpoint3));
                }
            } else {
                Vec3 bottomCenter = Vec3_Scale(Vec3_Add(Vec3_Add(corners[0], corners[1]),
                                                        Vec3_Add(corners[2], corners[3])), 0.25f);
                Vec3 topCenter = Vec3_Scale(Vec3_Add(Vec3_Add(corners[4], corners[5]),
                                                     Vec3_Add(corners[6], corners[7])), 0.25f);
                bool useTopFace = true;
                (void)Layout_RectPrismSelectFaceForView(object, plane, camera, &useTopFace);
                const int baseIndex = useTopFace ? 4 : 0;
                Vec3 faceCorners[4] = {
                    corners[baseIndex + 0],
                    corners[baseIndex + 1],
                    corners[baseIndex + 2],
                    corners[baseIndex + 3]
                };

                const float handleDepth = Hitbox_DepthDistance(plane, camera, useTopFace ? topCenter : bottomCenter);
                const int cornerRadius = SDL_max(5, (int)(gridSize * scale * 0.12f));
                const int edgeRadius = SDL_max(4, (int)(gridSize * scale * 0.10f));
                for (int c = 0; c < 4; ++c) {
                    Vec2 view = Vec3_ProjectToView(faceCorners[c], plane, camera);
                    const int sx = (int)((view.x - offsetX) * gridSize * scale);
                    const int sy = (int)((view.y - offsetY) * gridSize * scale);
                    Hitbox_Add(HITBOX_OBJECT3D_PLANE_CORNER,
                               (int)object->objectId,
                               PLANE_RESIZE_HANDLE_CORNER_NEG_U_NEG_V + c,
                               (SDL_Rect){ sx - cornerRadius, sy - cornerRadius, cornerRadius * 2, cornerRadius * 2 },
                               handleDepth);
                }

                for (int e = 0; e < 4; ++e) {
                    Vec3 midpoint3 = Vec3_Scale(Vec3_Add(faceCorners[kEdgeCornerA[e]], faceCorners[kEdgeCornerB[e]]), 0.5f);
                    Vec2 view = Vec3_ProjectToView(midpoint3, plane, camera);
                    const int sx = (int)((view.x - offsetX) * gridSize * scale);
                    const int sy = (int)((view.y - offsetY) * gridSize * scale);
                    Hitbox_Add(HITBOX_OBJECT3D_PLANE_EDGE,
                               (int)object->objectId,
                               (int)kEdgeHandle[e],
                               (SDL_Rect){ sx - edgeRadius, sy - edgeRadius, edgeRadius * 2, edgeRadius * 2 },
                               handleDepth);
                }
            }

            if (gizmoEnabled &&
                selectedObject3DResizeHandle == PLANE_RESIZE_HANDLE_NONE &&
                selectedObject3DPrismHandle == RECT_PRISM_RESIZE_HANDLE_NONE) {
                Hitbox_AddObject3DCenterGizmoAxes(object,
                                                  gridSize,
                                                  scale,
                                                  offsetX,
                                                  offsetY,
                                                  plane,
                                                  camera);
            }

            if (gizmoEnabled && selectedObject3DPrismHandle != RECT_PRISM_RESIZE_HANDLE_NONE) {
                RectPrismResizeHandleKind selectedHandle = (RectPrismResizeHandleKind)selectedObject3DPrismHandle;
                RectPrismHandleAxisMask axisMask = {0};
                Vec3 handleWorld = {0};
                if (Layout_RectPrismResizeHandleAxisMask(selectedHandle, &axisMask) &&
                    Layout_RectPrismResizeHandleWorldPoint(object, selectedHandle, &handleWorld)) {
                    const float axisWorldLen = fmaxf(gridSize * 2.0f, 1.0f);
                    const int gizmoRadius = SDL_max(6, (int)(gridSize * scale * 0.13f));
                    const Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
                    const Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
                    const Vec3 axisN = Vec3_Normalize(object->rectPrism.frame.normal);
                    const Vec3 basis[3] = { axisU, axisV, axisN };
                    const bool allowed[3] = {
                        axisMask.allowU,
                        axisMask.allowV,
                        axisMask.allowN
                    };

                    Vec2 baseView = Vec3_ProjectToView(handleWorld, plane, camera);
                    const float baseX = (baseView.x - offsetX) * gridSize * scale;
                    const float baseY = (baseView.y - offsetY) * gridSize * scale;
                    const float gizmoDepth = Hitbox_DepthDistance(plane, camera, handleWorld);
                    for (int axisFamily = 0; axisFamily < 3; ++axisFamily) {
                        if (!allowed[axisFamily]) continue;
                        for (int signPass = 0; signPass < 2; ++signPass) {
                            const float sign = (signPass == 0) ? 1.0f : -1.0f;
                            const int axisDir = axisFamily * 2 + signPass;
                            Vec3 tipWorld = Vec3_Add(handleWorld,
                                                     Vec3_Scale(basis[axisFamily], sign * axisWorldLen));
                            Vec2 tipView = Vec3_ProjectToView(tipWorld, plane, camera);
                            const float tipX = (tipView.x - offsetX) * gridSize * scale;
                            const float tipY = (tipView.y - offsetY) * gridSize * scale;
                            const float dx = tipX - baseX;
                            const float dy = tipY - baseY;
                            if (dx * dx + dy * dy <= 9.0f) continue;

                            Hitbox_Add(HITBOX_OBJECT3D_GIZMO_AXIS,
                                       (int)object->objectId,
                                       axisDir,
                                       (SDL_Rect){ (int)tipX - gizmoRadius,
                                                   (int)tipY - gizmoRadius,
                                                   gizmoRadius * 2,
                                                   gizmoRadius * 2 },
                                       gizmoDepth);
                        }
                    }
                }
            }
        }
    }
}

void HitboxSystem_Rebuild(const Layout* layout,
                         float scale,
                         float offsetX,
                         float offsetY,
                         ViewPlane plane,
                         const FreeViewCamera* camera,
                         int selectedAnchorIndex,
                         uint32_t selectedObject3DId,
                         int selectedObject3DResizeHandle,
                         int selectedObject3DPrismHandle,
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
    Hitbox_AddObject3DPrimitives(layout,
                                 scale,
                                 offsetX,
                                 offsetY,
                                 plane,
                                 camera,
                                 selectedObject3DId,
                                 selectedObject3DResizeHandle,
                                 selectedObject3DPrismHandle,
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
