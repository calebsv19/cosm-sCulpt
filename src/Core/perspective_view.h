#pragma once
#include "Math/math_util.h"

/* Runtime projection, independent of the orthographic editing camera and grid.
 * center/focal are expressed in virtual view coordinates for WorldToScreen. */
typedef struct {
    bool enabled;
    Vec3 eye, forward, right, up;
    Vec2 center;
    float focal, tan_half_fov, aspect, near_clip, far_clip;
} PerspectiveView;

static inline float PerspectiveView_Depth(const PerspectiveView *v, Vec3 p) {
    return Vec3_Dot(Vec3_Sub(p, v->eye), v->forward);
}
static inline Vec2 PerspectiveView_Project(const PerspectiveView *v, Vec3 p) {
    Vec3 d = Vec3_Sub(p, v->eye);
    float z = Vec3_Dot(d, v->forward);
    if (!isfinite(z) || z <= 0.0f)
        return (Vec2){NAN, NAN};
    return (Vec2){v->center.x + v->focal * Vec3_Dot(d, v->right) / z,
                  v->center.y - v->focal * Vec3_Dot(d, v->up) / z};
}
static inline Ray3 PerspectiveView_Ray(const PerspectiveView *v, Vec2 p) {
    return (Ray3){v->eye,
                  Vec3_Normalize(Vec3_Add(
                      v->forward, Vec3_Add(Vec3_Scale(v->right, (p.x - v->center.x) / v->focal),
                                           Vec3_Scale(v->up, -(p.y - v->center.y) / v->focal))))};
}
/* Clip before dividing by depth, including side planes to bound raster arithmetic.
 * A clipped triangle has at most nine vertices; buffers reserve twelve. */
static inline size_t PerspectiveView_ClipTriangle(const PerspectiveView *v, Vec3 a, Vec3 b, Vec3 c,
                                                  Vec3 out[12]) {
    Vec3 input[12] = {a, b, c}, output[12];
    size_t count = 3;
    float tx = v->tan_half_fov * v->aspect, ty = v->tan_half_fov;
    Vec3 normals[6] = {v->forward,
                       Vec3_Scale(v->forward, -1),
                       Vec3_Add(Vec3_Scale(v->forward, tx), v->right),
                       Vec3_Sub(Vec3_Scale(v->forward, tx), v->right),
                       Vec3_Add(Vec3_Scale(v->forward, ty), v->up),
                       Vec3_Sub(Vec3_Scale(v->forward, ty), v->up)};
    float offsets[6] = {-v->near_clip, v->far_clip, 0, 0, 0, 0};
    for (int plane = 0; plane < 6 && count; ++plane) {
        size_t n = 0;
        for (size_t i = 0; i < count; ++i) {
            Vec3 p = input[i], q = input[(i + 1) % count];
            float dp = Vec3_Dot(Vec3_Sub(p, v->eye), normals[plane]) + offsets[plane];
            float dq = Vec3_Dot(Vec3_Sub(q, v->eye), normals[plane]) + offsets[plane];
            if (dp >= 0 && n < 12)
                output[n++] = p;
            if ((dp >= 0) != (dq >= 0) && n < 12)
                output[n++] = Vec3_Add(p, Vec3_Scale(Vec3_Sub(q, p), dp / (dp - dq)));
        }
        count = n;
        for (size_t i = 0; i < n; ++i)
            input[i] = output[i];
    }
    for (size_t i = 0; i < count; ++i)
        out[i] = input[i];
    return count;
}
