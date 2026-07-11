#include "Layout/render_layout_surfaces.h"

#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Layout/scene/layout_object_faces.h"
#include "Math/math_util.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct {
    Object3DFaceKind face;
    Vec3 corners3[4];
    Vec2 corners2[4];
    Vec3 normal;
    float sort_depth;
    SDL_Color color;
} LayoutSurfaceQuad;

static Uint8 LayoutSurface_ClampColor(int value) {
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    return (Uint8)value;
}

static void LayoutSurface_DrawHorizontalLine(SDL_Renderer* renderer,
                                             int y,
                                             float x0,
                                             float x1) {
    int ix0 = 0;
    int ix1 = 0;
    if (!renderer) return;
    if (x0 > x1) {
        const float tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    ix0 = (int)lroundf(x0);
    ix1 = (int)lroundf(x1);
    SDL_RenderDrawLine(renderer, ix0, y, ix1, y);
}

static float LayoutSurface_EdgeXAtY(Vec2 a, Vec2 b, float y) {
    const float dy = b.y - a.y;
    if (fabsf(dy) <= 1e-5f) return a.x;
    return a.x + ((y - a.y) * (b.x - a.x) / dy);
}

static void LayoutSurface_FillFlatBottomTriangle(SDL_Renderer* renderer,
                                                 Vec2 top,
                                                 Vec2 left,
                                                 Vec2 right) {
    const int y_start = (int)ceilf(top.y);
    const int y_end = (int)floorf(SDL_max(left.y, right.y));
    for (int y = y_start; y <= y_end; ++y) {
        const float sample_y = (float)y + 0.5f;
        const float x0 = LayoutSurface_EdgeXAtY(top, left, sample_y);
        const float x1 = LayoutSurface_EdgeXAtY(top, right, sample_y);
        LayoutSurface_DrawHorizontalLine(renderer, y, x0, x1);
    }
}

static void LayoutSurface_FillFlatTopTriangle(SDL_Renderer* renderer,
                                              Vec2 left,
                                              Vec2 right,
                                              Vec2 bottom) {
    const int y_start = (int)ceilf(SDL_min(left.y, right.y));
    const int y_end = (int)floorf(bottom.y);
    for (int y = y_start; y <= y_end; ++y) {
        const float sample_y = (float)y + 0.5f;
        const float x0 = LayoutSurface_EdgeXAtY(left, bottom, sample_y);
        const float x1 = LayoutSurface_EdgeXAtY(right, bottom, sample_y);
        LayoutSurface_DrawHorizontalLine(renderer, y, x0, x1);
    }
}

static void LayoutSurface_FillTriangle(SDL_Renderer* renderer, Vec2 p0, Vec2 p1, Vec2 p2) {
    Vec2 points[3] = { p0, p1, p2 };
    if (!renderer) return;

    for (int i = 0; i < 3; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            if (points[j].y < points[i].y) {
                const Vec2 tmp = points[i];
                points[i] = points[j];
                points[j] = tmp;
            }
        }
    }

    if (fabsf(points[1].y - points[2].y) <= 1e-5f) {
        LayoutSurface_FillFlatBottomTriangle(renderer, points[0], points[1], points[2]);
        return;
    }
    if (fabsf(points[0].y - points[1].y) <= 1e-5f) {
        LayoutSurface_FillFlatTopTriangle(renderer, points[0], points[1], points[2]);
        return;
    }

    {
        const float split_t = (points[1].y - points[0].y) / (points[2].y - points[0].y);
        const Vec2 split = {
            points[0].x + ((points[2].x - points[0].x) * split_t),
            points[1].y
        };
        LayoutSurface_FillFlatBottomTriangle(renderer, points[0], points[1], split);
        LayoutSurface_FillFlatTopTriangle(renderer, points[1], split, points[2]);
    }
}

static float LayoutSurface_DepthVisualFactor(float abs_distance) {
    const float fade_start = 0.0f;
    const float fade_end = 8.0f;
    float t = (abs_distance - fade_start) / (fade_end - fade_start);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return 1.0f - (0.65f * t);
}

static SDL_Color LayoutSurface_ModulateColor(SDL_Color base,
                                             float depth_factor,
                                             float light_factor,
                                             Uint8 alpha) {
    const float shade = depth_factor * light_factor;
    return (SDL_Color){
        LayoutSurface_ClampColor((int)lroundf((float)base.r * shade)),
        LayoutSurface_ClampColor((int)lroundf((float)base.g * shade)),
        LayoutSurface_ClampColor((int)lroundf((float)base.b * shade)),
        alpha
    };
}

static Vec3 LayoutSurface_ViewDirection(const SpaceViewContext* view_ctx) {
    if (!view_ctx) return (Vec3){ 0.0f, 0.0f, 1.0f };
    if (SpaceAdapter_IsFreeViewEnabled(view_ctx)) {
        return Vec3_Normalize(FreeView_Forward(&view_ctx->camera));
    }

    switch (view_ctx->plane.axis) {
        case VIEW_PLANE_YZ: return (Vec3){ 1.0f, 0.0f, 0.0f };
        case VIEW_PLANE_XZ: return (Vec3){ 0.0f, 1.0f, 0.0f };
        case VIEW_PLANE_XY:
        default: return (Vec3){ 0.0f, 0.0f, 1.0f };
    }
}

static float LayoutSurface_SignedViewDepth(Vec3 point, const SpaceViewContext* view_ctx) {
    if (!view_ctx) return point.z;
    if (SpaceAdapter_IsFreeViewEnabled(view_ctx)) {
        return Vec3_Dot(Vec3_Sub(point, view_ctx->camera.target),
                        FreeView_Forward(&view_ctx->camera));
    }

    switch (view_ctx->plane.axis) {
        case VIEW_PLANE_YZ: return point.x;
        case VIEW_PLANE_XZ: return point.y;
        case VIEW_PLANE_XY:
        default: return point.z;
    }
}

static float LayoutSurface_LightFactor(Vec3 normal, Vec3 view_dir) {
    const Vec3 light_dir = Vec3_Normalize((Vec3){ 0.42f, 0.36f, 0.83f });
    const float diffuse = fmaxf(0.0f, Vec3_Dot(Vec3_Normalize(normal), light_dir));
    const float rim = fmaxf(0.0f, -Vec3_Dot(Vec3_Normalize(normal), Vec3_Normalize(view_dir)));
    float factor = 0.48f + (0.38f * diffuse) + (0.16f * rim);
    if (factor < 0.32f) factor = 0.32f;
    if (factor > 1.08f) factor = 1.08f;
    return factor;
}

static void LayoutSurface_RenderQuad(SDL_Renderer* renderer, const LayoutSurfaceQuad* quad) {
    if (!renderer || !quad) return;
    SDL_SetRenderDrawColor(renderer,
                           quad->color.r,
                           quad->color.g,
                           quad->color.b,
                           quad->color.a);
    LayoutSurface_FillTriangle(renderer, quad->corners2[0], quad->corners2[1], quad->corners2[2]);
    LayoutSurface_FillTriangle(renderer, quad->corners2[0], quad->corners2[2], quad->corners2[3]);
}

static int LayoutSurface_CompareBackToFront(const void* lhs, const void* rhs) {
    const LayoutSurfaceQuad* a = (const LayoutSurfaceQuad*)lhs;
    const LayoutSurfaceQuad* b = (const LayoutSurfaceQuad*)rhs;
    if (a->sort_depth < b->sort_depth) return 1;
    if (a->sort_depth > b->sort_depth) return -1;
    return 0;
}

static void LayoutSurface_FillPlane(const Object3D* object,
                                    const SpaceViewContext* view_ctx,
                                    const Grid* grid,
                                    Vec3 view_dir,
                                    SDL_Color base_color,
                                    Uint8 alpha,
                                    SDL_Renderer* renderer) {
    Vec3 corners3[4];
    LayoutSurfaceQuad quad = {0};
    float depth_factor = 1.0f;
    float light_factor = 1.0f;

    if (!object || !view_ctx || !grid || !renderer) return;
    if (!Layout_Object3D_ComputePlaneCorners(object, corners3)) return;

    quad.face = OBJECT3D_FACE_PLANE_SURFACE;
    for (int i = 0; i < 4; ++i) {
        quad.corners3[i] = corners3[i];
        quad.corners2[i] = WorldToScreen(SpaceAdapter_ProjectToView(corners3[i], view_ctx), grid);
    }
    quad.normal = Vec3_Normalize(object->plane.frame.normal);
    depth_factor = LayoutSurface_DepthVisualFactor(ViewPlane_AbsDistance(view_ctx->plane,
                                                                         object->transform.position));
    light_factor = LayoutSurface_LightFactor(quad.normal, view_dir);
    if (Vec3_Dot(quad.normal, view_dir) > 0.0f) {
        light_factor *= 0.72f;
    }
    quad.color = LayoutSurface_ModulateColor(base_color, depth_factor, light_factor, alpha);
    LayoutSurface_RenderQuad(renderer, &quad);
}

static size_t LayoutSurface_BuildBoxFaces(const Vec3 corners3[8],
                                          const SpaceViewContext* view_ctx,
                                          const Grid* grid,
                                          Vec3 view_dir,
                                          Vec3 depth_reference,
                                          SDL_Color base_color,
                                          Uint8 alpha,
                                          LayoutSurfaceQuad out_faces[6]) {
    static const int face_corners[6][4] = {
        {0, 1, 2, 3},
        {4, 5, 6, 7},
        {0, 1, 5, 4},
        {3, 2, 6, 7},
        {0, 4, 7, 3},
        {1, 2, 6, 5}
    };
    static const Object3DFaceKind face_kinds[6] = {
        OBJECT3D_FACE_RECT_PRISM_NEG_N,
        OBJECT3D_FACE_RECT_PRISM_POS_N,
        OBJECT3D_FACE_RECT_PRISM_NEG_V,
        OBJECT3D_FACE_RECT_PRISM_POS_V,
        OBJECT3D_FACE_RECT_PRISM_NEG_U,
        OBJECT3D_FACE_RECT_PRISM_POS_U
    };
    size_t face_count = 0u;
    float depth_factor = 1.0f;
    if (!corners3 || !view_ctx || !grid || !out_faces) return 0u;

    depth_factor =
        LayoutSurface_DepthVisualFactor(ViewPlane_AbsDistance(view_ctx->plane, depth_reference));
    for (int face_index = 0; face_index < 6; ++face_index) {
        LayoutSurfaceQuad quad = {0};
        Vec3 edge_a = {0};
        Vec3 edge_b = {0};
        Vec3 center = {0.0f, 0.0f, 0.0f};
        float light_factor = 1.0f;
        float view_alignment = 0.0f;

        quad.face = face_kinds[face_index];
        for (int corner_index = 0; corner_index < 4; ++corner_index) {
            const Vec3 corner3 = corners3[face_corners[face_index][corner_index]];
            quad.corners3[corner_index] = corner3;
            quad.corners2[corner_index] =
                WorldToScreen(SpaceAdapter_ProjectToView(corner3, view_ctx), grid);
            center = Vec3_Add(center, Vec3_Scale(corner3, 0.25f));
        }

        edge_a = Vec3_Sub(quad.corners3[1], quad.corners3[0]);
        edge_b = Vec3_Sub(quad.corners3[2], quad.corners3[1]);
        quad.normal = Vec3_Normalize(Vec3_Cross(edge_a, edge_b));
        quad.sort_depth = LayoutSurface_SignedViewDepth(center, view_ctx);
        view_alignment = Vec3_Dot(quad.normal, view_dir);
        light_factor = LayoutSurface_LightFactor(quad.normal, view_dir);
        if (view_alignment > 0.0f) {
            light_factor *= 0.72f;
        }
        quad.color = LayoutSurface_ModulateColor(base_color, depth_factor, light_factor, alpha);
        out_faces[face_count++] = quad;
    }

    return face_count;
}

void Layout_RenderObjectSurfaces(const Layout* layout, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    SpaceViewContext view_ctx = {0};
    Vec3 view_dir = {0.0f, 0.0f, 1.0f};

    if (!layout || !renderer || !state) return;
    if (state->spaceMode != SPACE_MODE_3D) return;
    if (state->previewMode == LINE_DRAWING_PREVIEW_MODE_WIREFRAME) return;

    view_ctx = SpaceAdapter_BuildViewContext(state);
    view_dir = LayoutSurface_ViewDirection(&view_ctx);

    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        const bool is_selected = (state->editor.selectedObject3DId == object->objectId);
        const bool is_hovered = (state->editor.hoveredObject3DId == object->objectId);
        const Object3DFaceKind selected_face =
            (state->editor.selectedObjectAssetBodyId == object->objectId)
                ? state->editor.selectedObjectAssetFace
                : OBJECT3D_FACE_NONE;
        const Object3DFaceKind hovered_face =
            (state->editor.hoveredObjectAssetBodyId == object->objectId)
                ? state->editor.hoveredObjectAssetFace
                : OBJECT3D_FACE_NONE;
        SDL_Color base_color = (SDL_Color){ 96, 188, 126, 118 };
        Uint8 alpha = 108u;

        if (!Layout_ObjectStore_ValidateObject(object)) continue;

        if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            base_color = (SDL_Color){ 112, 158, 224, 122 };
        } else if (object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
            base_color = (SDL_Color){ 170, 132, 226, 82 };
            alpha = 76u;
        }
        if (is_hovered) {
            base_color = (SDL_Color){ 118, 214, 242, (Uint8)(base_color.a + 26u) };
            alpha = (Uint8)(alpha + 16u);
        }
        if (is_selected) {
            base_color = (SDL_Color){ 236, 184, 74, (Uint8)(base_color.a + 40u) };
            alpha = (Uint8)(alpha + 32u);
        }

        if (object->kind == OBJECT3D_KIND_PLANE) {
            LayoutSurface_FillPlane(object,
                                    &view_ctx,
                                    &state->grid,
                                    view_dir,
                                    (selected_face == OBJECT3D_FACE_PLANE_SURFACE)
                                        ? (SDL_Color){ 255, 204, 96, (Uint8)(base_color.a + 56u) }
                                        : ((hovered_face == OBJECT3D_FACE_PLANE_SURFACE)
                                               ? (SDL_Color){ 140, 224, 255, (Uint8)(base_color.a + 34u) }
                                               : base_color),
                                    (selected_face == OBJECT3D_FACE_PLANE_SURFACE)
                                        ? (Uint8)(alpha + 40u)
                                        : ((hovered_face == OBJECT3D_FACE_PLANE_SURFACE)
                                               ? (Uint8)(alpha + 20u)
                                               : alpha),
                                    renderer);
        } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            Vec3 corners3[8];
            LayoutSurfaceQuad faces[6];
            size_t face_count = 0u;
            if (!Layout_Object3D_ComputeRectPrismCorners(object, corners3)) continue;
            face_count = LayoutSurface_BuildBoxFaces(corners3,
                                                     &view_ctx,
                                                     &state->grid,
                                                     view_dir,
                                                     object->transform.position,
                                                     base_color,
                                                     alpha,
                                                     faces);
            qsort(faces, face_count, sizeof(faces[0]), LayoutSurface_CompareBackToFront);
            for (size_t face_index = 0; face_index < face_count; ++face_index) {
                if (faces[face_index].face == hovered_face) {
                    faces[face_index].color.r = LayoutSurface_ClampColor(faces[face_index].color.r + 24);
                    faces[face_index].color.g = LayoutSurface_ClampColor(faces[face_index].color.g + 26);
                    faces[face_index].color.b = LayoutSurface_ClampColor(faces[face_index].color.b + 34);
                    faces[face_index].color.a = (Uint8)SDL_min(255, faces[face_index].color.a + 18);
                }
                if (faces[face_index].face == selected_face) {
                    faces[face_index].color.r = LayoutSurface_ClampColor(faces[face_index].color.r + 58);
                    faces[face_index].color.g = LayoutSurface_ClampColor(faces[face_index].color.g + 34);
                    faces[face_index].color.b = LayoutSurface_ClampColor(faces[face_index].color.b + 12);
                    faces[face_index].color.a = (Uint8)SDL_min(255, faces[face_index].color.a + 36);
                }
                LayoutSurface_RenderQuad(renderer, &faces[face_index]);
            }
        } else if (object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
            Vec3 corners3[8];
            Vec3 visual_center = object->transform.position;
            LayoutSurfaceQuad faces[6];
            size_t face_count = 0u;
            if (!Layout_Object3D_ComputeMeshInstanceCorners(object, corners3)) continue;
            (void)Layout_Object3D_ComputeVisualCenter(object, &visual_center);
            face_count = LayoutSurface_BuildBoxFaces(corners3,
                                                     &view_ctx,
                                                     &state->grid,
                                                     view_dir,
                                                     visual_center,
                                                     base_color,
                                                     alpha,
                                                     faces);
            qsort(faces, face_count, sizeof(faces[0]), LayoutSurface_CompareBackToFront);
            for (size_t face_index = 0; face_index < face_count; ++face_index) {
                LayoutSurface_RenderQuad(renderer, &faces[face_index]);
            }
        }
    }
}
