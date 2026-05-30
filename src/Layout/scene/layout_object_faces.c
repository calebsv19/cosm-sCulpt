#include "Layout/scene/layout_object_faces.h"

#include "Math/math_util.h"

#include <math.h>

typedef struct {
    Object3DFaceKind face;
    Vec3 corners3[4];
    Vec2 corners2[4];
    Vec3 normal;
    float depth;
} ObjectFaceQuad;

static Vec3 LayoutObjectFaces_ViewDirection(const SpaceViewContext* view_ctx) {
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

static float LayoutObjectFaces_SignedDepth(Vec3 point, const SpaceViewContext* view_ctx) {
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

static bool LayoutObjectFaces_PointInTriangle(Vec2 p, Vec2 a, Vec2 b, Vec2 c) {
    const float d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
    const float d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);
    const float d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);
    const bool has_neg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
    const bool has_pos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
    return !(has_neg && has_pos);
}

static bool LayoutObjectFaces_PointInQuad(Vec2 p, const Vec2 corners[4]) {
    return LayoutObjectFaces_PointInTriangle(p, corners[0], corners[1], corners[2]) ||
           LayoutObjectFaces_PointInTriangle(p, corners[0], corners[2], corners[3]);
}

static bool LayoutObjectFaces_BuildPlaneQuad(const Object3D* object,
                                             const SpaceViewContext* view_ctx,
                                             const Grid* grid,
                                             ObjectFaceQuad* out_quad) {
    Vec3 corners3[4];
    if (!object || !view_ctx || !grid || !out_quad) return false;
    if (!Layout_Object3D_ComputePlaneCorners(object, corners3)) return false;

    out_quad->face = OBJECT3D_FACE_PLANE_SURFACE;
    out_quad->normal = Vec3_Normalize(object->plane.frame.normal);
    out_quad->depth = LayoutObjectFaces_SignedDepth(object->transform.position, view_ctx);
    for (int i = 0; i < 4; ++i) {
        out_quad->corners3[i] = corners3[i];
        out_quad->corners2[i] =
            WorldToScreen(SpaceAdapter_ProjectToView(corners3[i], view_ctx), grid);
    }
    return true;
}

static size_t LayoutObjectFaces_BuildRectPrismQuads(const Object3D* object,
                                                    const SpaceViewContext* view_ctx,
                                                    const Grid* grid,
                                                    ObjectFaceQuad out_quads[6]) {
    static const int kFaceCorners[6][4] = {
        {0, 1, 2, 3},
        {4, 5, 6, 7},
        {0, 1, 5, 4},
        {3, 2, 6, 7},
        {0, 4, 7, 3},
        {1, 2, 6, 5}
    };
    static const Object3DFaceKind kFaces[6] = {
        OBJECT3D_FACE_RECT_PRISM_NEG_N,
        OBJECT3D_FACE_RECT_PRISM_POS_N,
        OBJECT3D_FACE_RECT_PRISM_NEG_V,
        OBJECT3D_FACE_RECT_PRISM_POS_V,
        OBJECT3D_FACE_RECT_PRISM_NEG_U,
        OBJECT3D_FACE_RECT_PRISM_POS_U
    };
    Vec3 corners3[8];
    Vec3 view_dir = {0};
    size_t count = 0u;

    if (!object || !view_ctx || !grid || !out_quads) return 0u;
    if (!Layout_Object3D_ComputeRectPrismCorners(object, corners3)) return 0u;

    view_dir = LayoutObjectFaces_ViewDirection(view_ctx);
    for (int face_index = 0; face_index < 6; ++face_index) {
        ObjectFaceQuad quad = {0};
        Vec3 edge_a = {0};
        Vec3 edge_b = {0};
        Vec3 center = {0};

        quad.face = kFaces[face_index];
        for (int i = 0; i < 4; ++i) {
            const Vec3 corner = corners3[kFaceCorners[face_index][i]];
            quad.corners3[i] = corner;
            quad.corners2[i] =
                WorldToScreen(SpaceAdapter_ProjectToView(corner, view_ctx), grid);
            center = Vec3_Add(center, Vec3_Scale(corner, 0.25f));
        }

        edge_a = Vec3_Sub(quad.corners3[1], quad.corners3[0]);
        edge_b = Vec3_Sub(quad.corners3[2], quad.corners3[1]);
        quad.normal = Vec3_Normalize(Vec3_Cross(edge_a, edge_b));
        if (Vec3_Dot(quad.normal, view_dir) >= -0.02f) continue;
        quad.depth = LayoutObjectFaces_SignedDepth(center, view_ctx);
        out_quads[count++] = quad;
    }

    return count;
}

const char* Layout_Object3DFaceKind_Label(Object3DFaceKind face) {
    switch (face) {
        case OBJECT3D_FACE_PLANE_SURFACE: return "Surface";
        case OBJECT3D_FACE_RECT_PRISM_NEG_N: return "-N Face";
        case OBJECT3D_FACE_RECT_PRISM_POS_N: return "+N Face";
        case OBJECT3D_FACE_RECT_PRISM_NEG_V: return "-V Face";
        case OBJECT3D_FACE_RECT_PRISM_POS_V: return "+V Face";
        case OBJECT3D_FACE_RECT_PRISM_NEG_U: return "-U Face";
        case OBJECT3D_FACE_RECT_PRISM_POS_U: return "+U Face";
        case OBJECT3D_FACE_NONE:
        default: return "None";
    }
}

bool Layout_Object3DFaceKind_IsRectPrismFace(Object3DFaceKind face) {
    return face >= OBJECT3D_FACE_RECT_PRISM_NEG_N &&
           face <= OBJECT3D_FACE_RECT_PRISM_POS_U;
}

bool Layout_Object3DFaceKind_IsValidForObject(const Object3D* object,
                                              Object3DFaceKind face) {
    if (!object || face == OBJECT3D_FACE_NONE) return false;
    if (object->kind == OBJECT3D_KIND_PLANE) {
        return face == OBJECT3D_FACE_PLANE_SURFACE;
    }
    if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
        return Layout_Object3DFaceKind_IsRectPrismFace(face);
    }
    return false;
}

bool Layout_Object3DFace_GetFrame(const Object3D* object,
                                  Object3DFaceKind face,
                                  PlaneFrame3* out_frame) {
    Vec3 axis_u = {0};
    Vec3 axis_v = {0};
    Vec3 axis_n = {0};
    float half_u = 0.0f;
    float half_v = 0.0f;
    float half_n = 0.0f;

    if (out_frame) {
        *out_frame = (PlaneFrame3){0};
    }
    if (!object || !out_frame) return false;
    if (!Layout_Object3DFaceKind_IsValidForObject(object, face)) return false;

    if (object->kind == OBJECT3D_KIND_PLANE) {
        *out_frame = object->plane.frame;
        out_frame->origin = object->plane.frame.origin;
        return true;
    }

    axis_u = Vec3_Normalize(object->rectPrism.frame.axisU);
    axis_v = Vec3_Normalize(object->rectPrism.frame.axisV);
    axis_n = Vec3_Normalize(object->rectPrism.frame.normal);
    half_u = object->rectPrism.width * 0.5f;
    half_v = object->rectPrism.height * 0.5f;
    half_n = object->rectPrism.depth * 0.5f;

    switch (face) {
        case OBJECT3D_FACE_RECT_PRISM_NEG_N:
            out_frame->origin = Vec3_Add(object->rectPrism.frame.origin, Vec3_Scale(axis_n, -half_n));
            out_frame->axisU = axis_u;
            out_frame->axisV = axis_v;
            out_frame->normal = Vec3_Scale(axis_n, -1.0f);
            return true;
        case OBJECT3D_FACE_RECT_PRISM_POS_N:
            out_frame->origin = Vec3_Add(object->rectPrism.frame.origin, Vec3_Scale(axis_n, half_n));
            out_frame->axisU = axis_u;
            out_frame->axisV = axis_v;
            out_frame->normal = axis_n;
            return true;
        case OBJECT3D_FACE_RECT_PRISM_NEG_V:
            out_frame->origin = Vec3_Add(object->rectPrism.frame.origin, Vec3_Scale(axis_v, -half_v));
            out_frame->axisU = axis_u;
            out_frame->axisV = axis_n;
            out_frame->normal = Vec3_Scale(axis_v, -1.0f);
            return true;
        case OBJECT3D_FACE_RECT_PRISM_POS_V:
            out_frame->origin = Vec3_Add(object->rectPrism.frame.origin, Vec3_Scale(axis_v, half_v));
            out_frame->axisU = axis_u;
            out_frame->axisV = axis_n;
            out_frame->normal = axis_v;
            return true;
        case OBJECT3D_FACE_RECT_PRISM_NEG_U:
            out_frame->origin = Vec3_Add(object->rectPrism.frame.origin, Vec3_Scale(axis_u, -half_u));
            out_frame->axisU = axis_v;
            out_frame->axisV = axis_n;
            out_frame->normal = Vec3_Scale(axis_u, -1.0f);
            return true;
        case OBJECT3D_FACE_RECT_PRISM_POS_U:
            out_frame->origin = Vec3_Add(object->rectPrism.frame.origin, Vec3_Scale(axis_u, half_u));
            out_frame->axisU = axis_v;
            out_frame->axisV = axis_n;
            out_frame->normal = axis_u;
            return true;
        case OBJECT3D_FACE_NONE:
        case OBJECT3D_FACE_PLANE_SURFACE:
        default:
            return false;
    }
}

bool Layout_Object3D_PickVisibleFaceAtScreenPoint(const Object3D* object,
                                                  const SpaceViewContext* view_ctx,
                                                  const Grid* grid,
                                                  int mouse_x,
                                                  int mouse_y,
                                                  Object3DFaceKind* out_face) {
    const Vec2 point = { (float)mouse_x, (float)mouse_y };
    Object3DFaceKind best_face = OBJECT3D_FACE_NONE;
    float best_depth = 0.0f;
    bool found = false;

    if (out_face) *out_face = OBJECT3D_FACE_NONE;
    if (!object || !view_ctx || !grid || !out_face) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    if (object->kind == OBJECT3D_KIND_PLANE) {
        ObjectFaceQuad quad = {0};
        Vec3 view_dir = LayoutObjectFaces_ViewDirection(view_ctx);
        if (!LayoutObjectFaces_BuildPlaneQuad(object, view_ctx, grid, &quad)) return false;
        if (Vec3_Dot(quad.normal, view_dir) > 0.0f) {
            quad.normal = Vec3_Scale(quad.normal, -1.0f);
        }
        if (!LayoutObjectFaces_PointInQuad(point, quad.corners2)) return false;
        *out_face = OBJECT3D_FACE_PLANE_SURFACE;
        return true;
    }

    if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
        ObjectFaceQuad quads[6];
        const size_t count = LayoutObjectFaces_BuildRectPrismQuads(object,
                                                                   view_ctx,
                                                                   grid,
                                                                   quads);
        for (size_t i = 0; i < count; ++i) {
            if (!LayoutObjectFaces_PointInQuad(point, quads[i].corners2)) continue;
            if (!found || quads[i].depth > best_depth) {
                found = true;
                best_depth = quads[i].depth;
                best_face = quads[i].face;
            }
        }
    }

    if (!found) return false;
    *out_face = best_face;
    return true;
}

bool Layout_Object3D_DefaultAuthoringFaceForView(const Object3D* object,
                                                 const SpaceViewContext* view_ctx,
                                                 Object3DFaceKind* out_face) {
    if (out_face) *out_face = OBJECT3D_FACE_NONE;
    if (!object || !view_ctx || !out_face) return false;
    if (!Layout_ObjectStore_ValidateObject(object)) return false;

    if (object->kind == OBJECT3D_KIND_PLANE) {
        *out_face = OBJECT3D_FACE_PLANE_SURFACE;
        return true;
    }

    if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
        Grid dummy_grid = {
            .gridSize = 1.0f,
            .scale = 1.0f,
            .offsetX = 0.0f,
            .offsetY = 0.0f
        };
        ObjectFaceQuad quads[6];
        const size_t count =
            LayoutObjectFaces_BuildRectPrismQuads(object, view_ctx, &dummy_grid, quads);
        bool found = false;
        float best_depth = 0.0f;
        Object3DFaceKind best_face = OBJECT3D_FACE_NONE;

        for (size_t i = 0; i < count; ++i) {
            if (!found || quads[i].depth > best_depth) {
                found = true;
                best_depth = quads[i].depth;
                best_face = quads[i].face;
            }
        }
        if (!found) return false;
        *out_face = best_face;
        return true;
    }

    return false;
}
