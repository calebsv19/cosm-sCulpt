// src/Layout/layout_render.c
#include "Layout/render_layout.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Editor/editor.h"
#include "Math/math_util.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdlib.h>

static float DepthVisualFactor(float absDistance) {
    const float kFadeStart = 0.0f;
    const float kFadeEnd = 8.0f;
    float t = (absDistance - kFadeStart) / (kFadeEnd - kFadeStart);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return 1.0f - (0.65f * t);
}

static Uint8 ApplyDepthToChannel(Uint8 base, float depthFactor) {
    float v = (float)base * depthFactor;
    if (v < 0.0f) v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    return (Uint8)v;
}

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

static void DrawFilledSquare(SDL_Renderer* renderer, int cx, int cy, int r) {
    SDL_Rect rect = { cx - r, cy - r, r * 2, r * 2 };
    SDL_RenderFillRect(renderer, &rect);
}

static bool Anchor_GetHandleForWall(const Layout* layout,
                                    int anchorIndex,
                                    const Anchor* anchor,
                                    int wallIndex,
                                    const SpaceViewContext* viewCtx,
                                    Vec2* outHandle) {
    if (!layout || !anchor || anchor->type != ANCHOR_TYPE_CURVE || !outHandle) return false;
    if (wallIndex < 0 || (size_t)wallIndex >= layout->wallCount) return false;

    const Wall* wall = &layout->walls[wallIndex];
    bool isAnchorA = (wall->anchorA == anchorIndex);

    float length = isAnchorA ? anchor->handleOutLength : anchor->handleInLength;
    float angle = isAnchorA ? anchor->handleOutAngleDeg : anchor->handleInAngleDeg;
    if (length <= 0.0001f) return false;

    Vec3 handleWorld = Vec3_HandleWorldPosition(anchor->pos, anchor->handleAxis, length, angle);
    *outHandle = SpaceAdapter_ProjectToView(handleWorld, viewCtx);
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

static void Layout_RenderSceneBounds3D(const Layout* layout, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    if (!layout || !renderer || !state) return;
    if (state->spaceMode != SPACE_MODE_3D) return;
    if (!layout->scene3d.bounds.enabled) return;
    if (!Layout_SceneBounds3D_IsValid(&layout->scene3d.bounds)) return;

    static const int kEdges[12][2] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0},
        {4, 5}, {5, 7}, {7, 6}, {6, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    const Grid* grid = &state->grid;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    ViewPlane plane = viewCtx.plane;
    Vec3 min = layout->scene3d.bounds.min;
    Vec3 max = layout->scene3d.bounds.max;
    Vec3 corners[8] = {
        { min.x, min.y, min.z }, { max.x, min.y, min.z },
        { min.x, max.y, min.z }, { max.x, max.y, min.z },
        { min.x, min.y, max.z }, { max.x, min.y, max.z },
        { min.x, max.y, max.z }, { max.x, max.y, max.z }
    };

    for (int e = 0; e < 12; ++e) {
        const Vec3 a3 = corners[kEdges[e][0]];
        const Vec3 b3 = corners[kEdges[e][1]];
        const float depthA = ViewPlane_AbsDistance(plane, a3);
        const float depthB = ViewPlane_AbsDistance(plane, b3);
        const float depthFactor = DepthVisualFactor((depthA + depthB) * 0.5f);
        const Vec2 a2 = WorldToScreen(SpaceAdapter_ProjectToView(a3, &viewCtx), grid);
        const Vec2 b2 = WorldToScreen(SpaceAdapter_ProjectToView(b3, &viewCtx), grid);

        SDL_SetRenderDrawColor(renderer,
                               ApplyDepthToChannel(90, depthFactor),
                               ApplyDepthToChannel(120, depthFactor),
                               ApplyDepthToChannel(200, depthFactor),
                               220);
        DrawLineWithThickness(renderer, (int)a2.x, (int)a2.y, (int)b2.x, (int)b2.y, 1);
    }
}

static void Layout_RenderObjects3D(const Layout* layout, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    if (!state) return;
    const Grid* grid = &state->grid;
    const EditorState* editor = &state->editor;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    ViewPlane plane = viewCtx.plane;
    const bool freeView = (state->spaceMode == SPACE_MODE_3D) &&
                          SpaceAdapter_IsFreeViewEnabled(&viewCtx);

    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        const bool isSelected = (editor->selectedObject3DId == object->objectId);
        const bool isHovered = (editor->hoveredObject3DId == object->objectId);
        const float depthFactor = DepthVisualFactor(ViewPlane_AbsDistance(plane, object->transform.position));

        if (object->isDeleted) continue;

        if (object->kind == OBJECT3D_KIND_PLANE) {
            Vec3 corners3[4];
            if (!Layout_Object3D_ComputePlaneCorners(object, corners3)) continue;

            Vec2 corners2[4];
            for (int c = 0; c < 4; ++c) {
                corners2[c] = WorldToScreen(SpaceAdapter_ProjectToView(corners3[c], &viewCtx), grid);
            }

            int thickness = 2;
            if (isSelected) {
                SDL_SetRenderDrawColor(renderer, 255, 220, 0, 255);
                thickness = 3;
            } else if (isHovered) {
                SDL_SetRenderDrawColor(renderer, 90, 220, 255, 255);
                thickness = 2;
            } else {
                SDL_SetRenderDrawColor(renderer,
                                       ApplyDepthToChannel(110, depthFactor),
                                       ApplyDepthToChannel(200, depthFactor),
                                       ApplyDepthToChannel(130, depthFactor),
                                       255);
                thickness = 2;
            }

            for (int edge = 0; edge < 4; ++edge) {
                const int next = (edge + 1) % 4;
                DrawLineWithThickness(renderer,
                                      (int)corners2[edge].x, (int)corners2[edge].y,
                                      (int)corners2[next].x, (int)corners2[next].y,
                                      thickness);
            }

            SDL_SetRenderDrawColor(renderer,
                                   ApplyDepthToChannel(90, depthFactor),
                                   ApplyDepthToChannel(170, depthFactor),
                                   ApplyDepthToChannel(110, depthFactor),
                                   isSelected ? 255 : 200);
            DrawLineWithThickness(renderer,
                                  (int)corners2[0].x, (int)corners2[0].y,
                                  (int)corners2[2].x, (int)corners2[2].y,
                                  1);
            DrawLineWithThickness(renderer,
                                  (int)corners2[1].x, (int)corners2[1].y,
                                  (int)corners2[3].x, (int)corners2[3].y,
                                  1);
            for (int c = 0; c < 4; ++c) {
                DrawFilledCircle(renderer, (int)corners2[c].x, (int)corners2[c].y, isSelected ? 3 : 2);
            }

            if (isSelected) {
                const int hoveredHandle =
                    (editor->hoveredObject3DId == object->objectId)
                    ? editor->hoveredObject3DResizeHandle
                    : PLANE_RESIZE_HANDLE_NONE;
                const int selectedHandle =
                    (editor->selectedObject3DId == object->objectId)
                    ? editor->selectedObject3DResizeHandle
                    : PLANE_RESIZE_HANDLE_NONE;
                const int activeHandle = (hoveredHandle != PLANE_RESIZE_HANDLE_NONE)
                    ? hoveredHandle
                    : selectedHandle;

                for (int c = 0; c < 4; ++c) {
                    const int handleKind = PLANE_RESIZE_HANDLE_CORNER_NEG_U_NEG_V + c;
                    const bool highlight = (activeHandle == handleKind);
                    SDL_SetRenderDrawColor(renderer,
                                           highlight ? 255 : 240,
                                           highlight ? 170 : 215,
                                           highlight ? 40 : 70,
                                           255);
                    DrawFilledSquare(renderer, (int)corners2[c].x, (int)corners2[c].y, highlight ? 4 : 3);
                }

                static const int kEdgeCornerA[4] = { 0, 1, 2, 3 };
                static const int kEdgeCornerB[4] = { 1, 2, 3, 0 };
                static const PlaneResizeHandleKind kEdgeHandle[4] = {
                    PLANE_RESIZE_HANDLE_EDGE_NEG_V,
                    PLANE_RESIZE_HANDLE_EDGE_POS_U,
                    PLANE_RESIZE_HANDLE_EDGE_POS_V,
                    PLANE_RESIZE_HANDLE_EDGE_NEG_U
                };
                for (int e = 0; e < 4; ++e) {
                    const Vec2 mid = {
                        .x = 0.5f * (corners2[kEdgeCornerA[e]].x + corners2[kEdgeCornerB[e]].x),
                        .y = 0.5f * (corners2[kEdgeCornerA[e]].y + corners2[kEdgeCornerB[e]].y)
                    };
                    const bool highlight = (activeHandle == (int)kEdgeHandle[e]);
                    SDL_SetRenderDrawColor(renderer,
                                           highlight ? 255 : 220,
                                           highlight ? 170 : 200,
                                           highlight ? 40 : 80,
                                           255);
                    DrawFilledSquare(renderer, (int)mid.x, (int)mid.y, highlight ? 4 : 3);
                }
            }
        } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            Vec3 corners3[8];
            if (!Layout_Object3D_ComputeRectPrismCorners(object, corners3)) continue;
            Vec2 corners2[8];
            for (int c = 0; c < 8; ++c) {
                corners2[c] = WorldToScreen(SpaceAdapter_ProjectToView(corners3[c], &viewCtx), grid);
            }

            int thickness = 2;
            if (isSelected) {
                SDL_SetRenderDrawColor(renderer, 255, 210, 80, 255);
                thickness = 3;
            } else if (isHovered) {
                SDL_SetRenderDrawColor(renderer, 90, 220, 255, 255);
                thickness = 2;
            } else {
                SDL_SetRenderDrawColor(renderer,
                                       ApplyDepthToChannel(120, depthFactor),
                                       ApplyDepthToChannel(165, depthFactor),
                                       ApplyDepthToChannel(215, depthFactor),
                                       255);
                thickness = 2;
            }

            static const int kRectEdges[12][2] = {
                {0, 1}, {1, 2}, {2, 3}, {3, 0},
                {4, 5}, {5, 6}, {6, 7}, {7, 4},
                {0, 4}, {1, 5}, {2, 6}, {3, 7}
            };
            for (int e = 0; e < 12; ++e) {
                const Vec2 a = corners2[kRectEdges[e][0]];
                const Vec2 b = corners2[kRectEdges[e][1]];
                DrawLineWithThickness(renderer,
                                      (int)a.x, (int)a.y,
                                      (int)b.x, (int)b.y,
                                      thickness);
            }

            SDL_SetRenderDrawColor(renderer,
                                   ApplyDepthToChannel(95, depthFactor),
                                   ApplyDepthToChannel(140, depthFactor),
                                   ApplyDepthToChannel(190, depthFactor),
                                   isSelected ? 255 : 210);
            DrawLineWithThickness(renderer, (int)corners2[0].x, (int)corners2[0].y,
                                  (int)corners2[2].x, (int)corners2[2].y, 1);
            DrawLineWithThickness(renderer, (int)corners2[1].x, (int)corners2[1].y,
                                  (int)corners2[3].x, (int)corners2[3].y, 1);
            DrawLineWithThickness(renderer, (int)corners2[4].x, (int)corners2[4].y,
                                  (int)corners2[6].x, (int)corners2[6].y, 1);
            DrawLineWithThickness(renderer, (int)corners2[5].x, (int)corners2[5].y,
                                  (int)corners2[7].x, (int)corners2[7].y, 1);

            if (isSelected) {
                if (freeView) {
                    const int hoveredHandle =
                        (editor->hoveredObject3DId == object->objectId)
                        ? editor->hoveredObject3DPrismHandle
                        : RECT_PRISM_RESIZE_HANDLE_NONE;
                    const int selectedHandle =
                        (editor->selectedObject3DId == object->objectId)
                        ? editor->selectedObject3DPrismHandle
                        : RECT_PRISM_RESIZE_HANDLE_NONE;
                    const int activeHandle = (hoveredHandle != RECT_PRISM_RESIZE_HANDLE_NONE)
                        ? hoveredHandle
                        : selectedHandle;

                    for (int c = 0; c < 8; ++c) {
                        const int handleKind = RECT_PRISM_RESIZE_HANDLE_CORNER_0 + c;
                        const bool highlight = (activeHandle == handleKind);
                        SDL_SetRenderDrawColor(renderer,
                                               highlight ? 255 : 240,
                                               highlight ? 170 : 215,
                                               highlight ? 40 : 70,
                                               255);
                        DrawFilledSquare(renderer, (int)corners2[c].x, (int)corners2[c].y, highlight ? 4 : 3);
                    }

                    for (int e = 0; e < 12; ++e) {
                        const Vec2 mid = {
                            .x = 0.5f * (corners2[kRectEdges[e][0]].x + corners2[kRectEdges[e][1]].x),
                            .y = 0.5f * (corners2[kRectEdges[e][0]].y + corners2[kRectEdges[e][1]].y)
                        };
                        const int handleKind = RECT_PRISM_RESIZE_HANDLE_EDGE_0 + e;
                        const bool highlight = (activeHandle == handleKind);
                        SDL_SetRenderDrawColor(renderer,
                                               highlight ? 255 : 220,
                                               highlight ? 170 : 200,
                                               highlight ? 40 : 80,
                                               255);
                        DrawFilledSquare(renderer, (int)mid.x, (int)mid.y, highlight ? 4 : 3);
                    }
                } else {
                    bool useTopFace = true;
                    (void)Layout_RectPrismSelectFaceForView(object, plane, SpaceAdapter_Camera(&viewCtx), &useTopFace);
                    const int baseIndex = useTopFace ? 4 : 0;
                    Vec2 face2[4] = {
                        corners2[baseIndex + 0],
                        corners2[baseIndex + 1],
                        corners2[baseIndex + 2],
                        corners2[baseIndex + 3]
                    };

                    const int hoveredHandle =
                        (editor->hoveredObject3DId == object->objectId)
                        ? editor->hoveredObject3DResizeHandle
                        : PLANE_RESIZE_HANDLE_NONE;
                    const int selectedHandle =
                        (editor->selectedObject3DId == object->objectId)
                        ? editor->selectedObject3DResizeHandle
                        : PLANE_RESIZE_HANDLE_NONE;
                    const int activeHandle = (hoveredHandle != PLANE_RESIZE_HANDLE_NONE)
                        ? hoveredHandle
                        : selectedHandle;

                    for (int c = 0; c < 4; ++c) {
                        const int handleKind = PLANE_RESIZE_HANDLE_CORNER_NEG_U_NEG_V + c;
                        const bool highlight = (activeHandle == handleKind);
                        SDL_SetRenderDrawColor(renderer,
                                               highlight ? 255 : 240,
                                               highlight ? 170 : 215,
                                               highlight ? 40 : 70,
                                               255);
                        DrawFilledSquare(renderer, (int)face2[c].x, (int)face2[c].y, highlight ? 4 : 3);
                    }

                    static const int kEdgeCornerA[4] = { 0, 1, 2, 3 };
                    static const int kEdgeCornerB[4] = { 1, 2, 3, 0 };
                    static const PlaneResizeHandleKind kEdgeHandle[4] = {
                        PLANE_RESIZE_HANDLE_EDGE_NEG_V,
                        PLANE_RESIZE_HANDLE_EDGE_POS_U,
                        PLANE_RESIZE_HANDLE_EDGE_POS_V,
                        PLANE_RESIZE_HANDLE_EDGE_NEG_U
                    };
                    for (int e = 0; e < 4; ++e) {
                        const Vec2 mid = {
                            .x = 0.5f * (face2[kEdgeCornerA[e]].x + face2[kEdgeCornerB[e]].x),
                            .y = 0.5f * (face2[kEdgeCornerA[e]].y + face2[kEdgeCornerB[e]].y)
                        };
                        const bool highlight = (activeHandle == (int)kEdgeHandle[e]);
                        SDL_SetRenderDrawColor(renderer,
                                               highlight ? 255 : 220,
                                               highlight ? 170 : 200,
                                               highlight ? 40 : 80,
                                               255);
                        DrawFilledSquare(renderer, (int)mid.x, (int)mid.y, highlight ? 4 : 3);
                    }
                }
            }
        }
    }
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
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    ViewPlane plane = viewCtx.plane;
    int selectedIndex = editor->selectedWallIndex;
    int hoveredIndex = editor->hoveredWallIndex;

    for (size_t i = 0; i < layout->wallCount; ++i) {
        Wall w = layout->walls[i];
        if (w.isDeleted) continue;
        Vec2 from = SpaceAdapter_ProjectToView(layout->anchors[w.anchorA].pos, &viewCtx);
        Vec2 to   = SpaceAdapter_ProjectToView(layout->anchors[w.anchorB].pos, &viewCtx);
        float depthA = ViewPlane_AbsDistance(plane, layout->anchors[w.anchorA].pos);
        float depthB = ViewPlane_AbsDistance(plane, layout->anchors[w.anchorB].pos);
        float depthFactor = DepthVisualFactor((depthA + depthB) * 0.5f);

        int thickness = 1;
        if ((int)i == selectedIndex) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);  // Yellow
            thickness = 3;
        } else if ((int)i == hoveredIndex) {
            SDL_SetRenderDrawColor(renderer, 180, 220, 255, 255);
            thickness = 2;
        } else {
            SDL_SetRenderDrawColor(renderer,
                                   ApplyDepthToChannel(100, depthFactor),
                                   ApplyDepthToChannel(100, depthFactor),
                                   ApplyDepthToChannel(220, depthFactor),
                                   255);
            thickness = (depthFactor > 0.75f) ? 2 : 1;
        }

        Vec2 handleA = from;
        Vec2 handleB = to;
        bool hasHandleA = Anchor_GetHandleForWall(layout, w.anchorA, &layout->anchors[w.anchorA], (int)i, &viewCtx, &handleA);
        bool hasHandleB = Anchor_GetHandleForWall(layout, w.anchorB, &layout->anchors[w.anchorB], (int)i, &viewCtx, &handleB);

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
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    ViewPlane plane = viewCtx.plane;

    for (size_t i = 0; i < layout->anchorCount; ++i) {
        const Anchor* anchor = &layout->anchors[i];
        if (anchor->isDeleted || anchor->type != ANCHOR_TYPE_CURVE) continue;
        float depthFactor = DepthVisualFactor(ViewPlane_AbsDistance(plane, anchor->pos));

        Vec2 anchorPos = SpaceAdapter_ProjectToView(anchor->pos, &viewCtx);
        Vec2 anchorScreen = WorldToScreen(anchorPos, grid);
        Vec2 handles[2] = {
            SpaceAdapter_ProjectToView(Vec3_HandleWorldPosition(anchor->pos, anchor->handleAxis,
                                                                anchor->handleInLength, anchor->handleInAngleDeg),
                                       &viewCtx),
            SpaceAdapter_ProjectToView(Vec3_HandleWorldPosition(anchor->pos, anchor->handleAxis,
                                                                anchor->handleOutLength, anchor->handleOutAngleDeg),
                                       &viewCtx)
        };

        for (int h = 0; h < 2; ++h) {
            Vec2 handlePos = handles[h];
            Vec2 handleScreen = WorldToScreen(handlePos, grid);

            SDL_SetRenderDrawColor(renderer,
                                   ApplyDepthToChannel(120, depthFactor),
                                   ApplyDepthToChannel(120, depthFactor),
                                   ApplyDepthToChannel(180, depthFactor),
                                   255);
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
                SDL_SetRenderDrawColor(renderer,
                                       ApplyDepthToChannel(150, depthFactor),
                                       ApplyDepthToChannel(150, depthFactor),
                                       ApplyDepthToChannel(220, depthFactor),
                                       255);
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
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    ViewPlane plane = viewCtx.plane;

    int r = ANCHOR_RENDER_RADIUS;

    int hoveredAnchor = editor->hoveredAnchorIndex;

    for (size_t i = 0; i < layout->anchorCount; ++i) {
        Anchor* anchor = &layout->anchors[i];
        if (anchor->isDeleted) continue;
        float depthFactor = DepthVisualFactor(ViewPlane_AbsDistance(plane, anchor->pos));
        Vec2 screen = WorldToScreen(SpaceAdapter_ProjectToView(anchor->pos, &viewCtx), grid);
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

        if (!isSelected && !isHovered && !isDragging) {
            rC = ApplyDepthToChannel(rC, depthFactor);
            gC = ApplyDepthToChannel(gC, depthFactor);
            bC = ApplyDepthToChannel(bC, depthFactor);
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

    Layout_RenderSceneBounds3D(layout, renderer);
    Layout_RenderObjects3D(layout, renderer);
    Layout_RenderWalls(layout, renderer);
    Layout_RenderHandles(layout, renderer);
    Layout_RenderAnchors(layout, renderer);
}
