// src/Math/math_util.h
#pragma once

#include "Layout/Grid/grid.h"
#include <math.h>
#include <stdbool.h>


typedef struct { float x, y; } Vec2;
typedef struct { float x, y, z; } Vec3;
typedef struct { Vec3 origin, direction; } Ray3;
typedef struct { Vec3 normal; float d; } Plane3;
typedef struct { Vec3 origin, axisU, axisV, normal; } PlaneFrame3;
typedef enum {
    VIEW_PLANE_XY = 0,
    VIEW_PLANE_YZ = 1,
    VIEW_PLANE_XZ = 2
} ViewPlaneAxis;

typedef struct {
    ViewPlaneAxis axis;
    float offset;
} ViewPlane;
typedef struct {
    bool enabled;
    float yawDeg;
    float pitchDeg;
    Vec3 target;
} FreeViewCamera;

static inline float Vec2_Distance(Vec2 a, Vec2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx*dx + dy*dy);
}

static inline Vec3 Vec3_Add(Vec3 a, Vec3 b) {
    return (Vec3){ a.x + b.x, a.y + b.y, a.z + b.z };
}

static inline Vec3 Vec3_Sub(Vec3 a, Vec3 b) {
    return (Vec3){ a.x - b.x, a.y - b.y, a.z - b.z };
}

static inline Vec3 Vec3_Scale(Vec3 v, float s) {
    return (Vec3){ v.x * s, v.y * s, v.z * s };
}

static inline float Vec3_Dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline Vec3 Vec3_Cross(Vec3 a, Vec3 b) {
    return (Vec3){
        .x = a.y * b.z - a.z * b.y,
        .y = a.z * b.x - a.x * b.z,
        .z = a.x * b.y - a.y * b.x
    };
}

static inline float Vec3_Length(Vec3 v) {
    return sqrtf(Vec3_Dot(v, v));
}

static inline float Vec3_Distance(Vec3 a, Vec3 b) {
    return Vec3_Length(Vec3_Sub(a, b));
}

static inline Vec3 Vec3_Normalize(Vec3 v) {
    const float len = Vec3_Length(v);
    if (len <= 1e-6f) return (Vec3){0.0f, 0.0f, 0.0f};
    return Vec3_Scale(v, 1.0f / len);
}

static inline Vec2 Vec2_Snap(Vec2 pt, float gridSize) {
    pt.x = roundf(pt.x / gridSize) * gridSize;
    pt.y = roundf(pt.y / gridSize) * gridSize;
    return pt;
}

static inline float DegToRad(float deg) {
    return deg * (float)M_PI / 180.0f;
}

static inline float RadToDeg(float rad) {
    return rad * 180.0f / (float)M_PI;
}

static inline Vec2 Vec2_FromPolar(float length, float angleDeg) {
    float rad = DegToRad(angleDeg);
    return (Vec2){
        .x = cosf(rad) * length,
        .y = sinf(rad) * length
    };
}

static inline Vec2 Vec2_Add(Vec2 a, Vec2 b) {
    return (Vec2){ a.x + b.x, a.y + b.y };
}

static inline Vec2 Vec2_Sub(Vec2 a, Vec2 b) {
    return (Vec2){ a.x - b.x, a.y - b.y };
}

static inline Vec2 Vec2_HandleAbsolute(Vec2 anchorPos, float length, float angleDeg) {
    return Vec2_Add(anchorPos, Vec2_FromPolar(length, angleDeg));
}

static inline float Vec2_AngleDeg(Vec2 anchorPos, Vec2 targetPos) {
    return RadToDeg(atan2f(targetPos.y - anchorPos.y, targetPos.x - anchorPos.x));
}


static inline Vec2 WorldToScreen(Vec2 world, const Grid* grid) {
    return (Vec2){
        .x = (world.x - grid->offsetX) * grid->gridSize * grid->scale,
        .y = (world.y - grid->offsetY) * grid->gridSize * grid->scale
    };
}

static inline Vec2 ScreenToWorld(int screenX, int screenY, const Grid* grid) {
    return (Vec2){
        .x = (screenX / (grid->gridSize * grid->scale)) + grid->offsetX,
        .y = (screenY / (grid->gridSize * grid->scale)) + grid->offsetY
    };
}

static inline Vec2 ScreenToSnappedWorld(int screenX, int screenY, const Grid* grid) {
    Vec2 world = ScreenToWorld(screenX, screenY, grid);
    return Vec2_Snap(world, grid->gridSize);
}

static inline Vec3 Vec3_FromVec2(Vec2 v, float z) {
    return (Vec3){ v.x, v.y, z };
}

static inline Vec2 Vec2_FromVec3(Vec3 v) {
    return (Vec2){ v.x, v.y };
}

static inline Vec2 Vec3_ProjectToPlane(Vec3 v, ViewPlaneAxis axis) {
    switch (axis) {
        case VIEW_PLANE_YZ: return (Vec2){ v.y, v.z };
        case VIEW_PLANE_XZ: return (Vec2){ v.x, v.z };
        case VIEW_PLANE_XY:
        default: return (Vec2){ v.x, v.y };
    }
}

static inline Vec3 Vec3_FromPlaneCoords(Vec2 planePos, ViewPlaneAxis axis, float offset) {
    switch (axis) {
        case VIEW_PLANE_YZ: return (Vec3){ offset, planePos.x, planePos.y };
        case VIEW_PLANE_XZ: return (Vec3){ planePos.x, offset, planePos.y };
        case VIEW_PLANE_XY:
        default: return (Vec3){ planePos.x, planePos.y, offset };
    }
}

// Convert a plane-local vector into world-space delta.
static inline Vec3 Vec3_FromPlaneVector(Vec2 planeVec, ViewPlaneAxis axis) {
    return Vec3_FromPlaneCoords(planeVec, axis, 0.0f);
}

// Convert world-space delta into a plane-local vector.
static inline Vec2 Vec3_ToPlaneVector(Vec3 delta, ViewPlaneAxis axis) {
    return Vec3_ProjectToPlane(delta, axis);
}

// Build world-space handle delta from plane-local polar handle params.
static inline Vec3 Vec3_HandleOffsetFromPlanePolar(ViewPlaneAxis axis, float length, float angleDeg) {
    Vec2 planeOffset = Vec2_FromPolar(length, angleDeg);
    return Vec3_FromPlaneVector(planeOffset, axis);
}

// Build world-space handle position from plane-local polar handle params.
static inline Vec3 Vec3_HandleWorldPosition(Vec3 anchorPos,
                                            ViewPlaneAxis axis,
                                            float length,
                                            float angleDeg) {
    return Vec3_Add(anchorPos, Vec3_HandleOffsetFromPlanePolar(axis, length, angleDeg));
}

// Convert world-space delta to plane-local polar handle params.
static inline void Vec3_HandlePolarFromWorldDelta(Vec3 delta,
                                                  ViewPlaneAxis axis,
                                                  float* outLength,
                                                  float* outAngleDeg) {
    Vec2 planeDelta = Vec3_ToPlaneVector(delta, axis);
    if (outLength) {
        *outLength = sqrtf(planeDelta.x * planeDelta.x + planeDelta.y * planeDelta.y);
    }
    if (outAngleDeg) {
        *outAngleDeg = RadToDeg(atan2f(planeDelta.y, planeDelta.x));
    }
}

static inline Vec3 FreeView_Forward(const FreeViewCamera* camera) {
    if (!camera) return (Vec3){ 0.0f, 0.0f, 1.0f };
    float yaw = DegToRad(camera->yawDeg);
    float pitch = DegToRad(camera->pitchDeg);
    Vec3 f = {
        .x = cosf(yaw) * cosf(pitch),
        .y = sinf(yaw) * cosf(pitch),
        .z = sinf(pitch)
    };
    return Vec3_Normalize(f);
}

static inline Vec3 FreeView_Right(const FreeViewCamera* camera) {
    if (!camera) return (Vec3){ 1.0f, 0.0f, 0.0f };
    float yaw = DegToRad(camera->yawDeg);
    // Non-singular right basis: independent of pitch, continuous through poles.
    return Vec3_Normalize((Vec3){
        .x = sinf(yaw),
        .y = -cosf(yaw),
        .z = 0.0f
    });
}

static inline Vec3 FreeView_Up(const FreeViewCamera* camera) {
    Vec3 right = FreeView_Right(camera);
    Vec3 forward = FreeView_Forward(camera);
    return Vec3_Normalize(Vec3_Cross(right, forward));
}

static inline Vec2 Vec3_ProjectToView(Vec3 v, ViewPlane plane, const FreeViewCamera* camera) {
    if (!camera || !camera->enabled) {
        return Vec3_ProjectToPlane(v, plane.axis);
    }
    Vec3 delta = Vec3_Sub(v, camera->target);
    Vec3 right = FreeView_Right(camera);
    Vec3 up = FreeView_Up(camera);
    return (Vec2){
        .x = Vec3_Dot(delta, right),
        .y = Vec3_Dot(delta, up)
    };
}

static inline Plane3 Plane3_FromPointNormal(Vec3 point, Vec3 normal) {
    Vec3 n = Vec3_Normalize(normal);
    return (Plane3){
        .normal = n,
        .d = -Vec3_Dot(n, point)
    };
}

static inline Plane3 Plane3_FromAxisX(float x) {
    return Plane3_FromPointNormal((Vec3){x, 0.0f, 0.0f}, (Vec3){1.0f, 0.0f, 0.0f});
}

static inline Plane3 Plane3_FromAxisY(float y) {
    return Plane3_FromPointNormal((Vec3){0.0f, y, 0.0f}, (Vec3){0.0f, 1.0f, 0.0f});
}

static inline Plane3 Plane3_FromAxisZ(float z) {
    return Plane3_FromPointNormal((Vec3){0.0f, 0.0f, z}, (Vec3){0.0f, 0.0f, 1.0f});
}

static inline Plane3 Plane3_FromViewPlane(ViewPlane plane) {
    switch (plane.axis) {
        case VIEW_PLANE_YZ: return Plane3_FromAxisX(plane.offset);
        case VIEW_PLANE_XZ: return Plane3_FromAxisY(plane.offset);
        case VIEW_PLANE_XY:
        default: return Plane3_FromAxisZ(plane.offset);
    }
}

static inline Ray3 Ray3_FromPlaneViewPoint(Vec2 planePos, ViewPlaneAxis axis) {
    switch (axis) {
        case VIEW_PLANE_YZ:
            return (Ray3){
                .origin = { 0.0f, planePos.x, planePos.y },
                .direction = { 1.0f, 0.0f, 0.0f }
            };
        case VIEW_PLANE_XZ:
            return (Ray3){
                .origin = { planePos.x, 0.0f, planePos.y },
                .direction = { 0.0f, 1.0f, 0.0f }
            };
        case VIEW_PLANE_XY:
        default:
            return (Ray3){
                .origin = { planePos.x, planePos.y, 0.0f },
                .direction = { 0.0f, 0.0f, 1.0f }
            };
    }
}

static inline float Plane3_SignedDistance(Plane3 plane, Vec3 point) {
    return Vec3_Dot(plane.normal, point) + plane.d;
}

static inline float ViewPlane_AbsDistance(ViewPlane plane, Vec3 point) {
    Plane3 p = Plane3_FromViewPlane(plane);
    return fabsf(Plane3_SignedDistance(p, point));
}

static inline Vec3 Plane3_ProjectPoint(Plane3 plane, Vec3 point) {
    float dist = Plane3_SignedDistance(plane, point);
    return Vec3_Sub(point, Vec3_Scale(plane.normal, dist));
}

static inline bool Ray3_IntersectPlane(Ray3 ray, Plane3 plane, float* outT, Vec3* outPoint) {
    const float denom = Vec3_Dot(plane.normal, ray.direction);
    if (fabsf(denom) <= 1e-6f) return false;

    const float t = -(Vec3_Dot(plane.normal, ray.origin) + plane.d) / denom;
    if (t < 0.0f) return false;

    if (outT) *outT = t;
    if (outPoint) {
        *outPoint = Vec3_Add(ray.origin, Vec3_Scale(ray.direction, t));
    }
    return true;
}

static inline bool ScreenToPlaneWorld(int screenX,
                                      int screenY,
                                      const Grid* grid,
                                      ViewPlane plane,
                                      const FreeViewCamera* camera,
                                      bool snapped,
                                      Vec3* outWorld) {
    if (!grid || !outWorld) return false;
    Vec2 viewPos = snapped
        ? ScreenToSnappedWorld(screenX, screenY, grid)
        : ScreenToWorld(screenX, screenY, grid);
    Ray3 ray = Ray3_FromPlaneViewPoint(viewPos, plane.axis);
    if (camera && camera->enabled) {
        Vec3 right = FreeView_Right(camera);
        Vec3 up = FreeView_Up(camera);
        Vec3 forward = FreeView_Forward(camera);
        ray.origin = Vec3_Add(camera->target,
                              Vec3_Add(Vec3_Scale(right, viewPos.x),
                                       Vec3_Scale(up, viewPos.y)));
        ray.direction = forward;
    }
    Plane3 worldPlane = Plane3_FromViewPlane(plane);
    return Ray3_IntersectPlane(ray, worldPlane, NULL, outWorld);
}

static inline PlaneFrame3 PlaneFrame3_FromPlane(Plane3 plane, Vec3 origin) {
    Vec3 n = Vec3_Normalize(plane.normal);
    Vec3 ref = (fabsf(n.z) < 0.9f) ? (Vec3){0.0f, 0.0f, 1.0f} : (Vec3){1.0f, 0.0f, 0.0f};
    Vec3 u = Vec3_Normalize(Vec3_Cross(ref, n));
    Vec3 v = Vec3_Cross(n, u);
    return (PlaneFrame3){
        .origin = origin,
        .axisU = u,
        .axisV = v,
        .normal = n
    };
}

static inline Vec2 PlaneFrame3_ProjectPoint(const PlaneFrame3* frame, Vec3 point) {
    Vec3 local = Vec3_Sub(point, frame->origin);
    return (Vec2){
        .x = Vec3_Dot(local, frame->axisU),
        .y = Vec3_Dot(local, frame->axisV)
    };
}

static inline Vec3 PlaneFrame3_PointFromUV(const PlaneFrame3* frame, Vec2 uv) {
    Vec3 u = Vec3_Scale(frame->axisU, uv.x);
    Vec3 v = Vec3_Scale(frame->axisV, uv.y);
    return Vec3_Add(frame->origin, Vec3_Add(u, v));
}

static inline float Angle_NormalizeDeg(float angle) {
    float result = fmodf(angle, 360.0f);
    if (result < 0.0f) result += 360.0f;
    return result;
}

static inline float Angle_NormalizeSignedDeg(float angle) {
    float result = fmodf(angle, 360.0f);
    if (result > 180.0f) result -= 360.0f;
    if (result < -180.0f) result += 360.0f;
    return result;
}

// Normalize orbit camera angles with periodic wrapping only.
// Camera basis math is non-singular, so no pole-crossing yaw compensation is needed.
static inline void FreeView_NormalizeOrbitAngles(FreeViewCamera* camera) {
    if (!camera) return;
    camera->yawDeg = Angle_NormalizeDeg(camera->yawDeg);
    camera->pitchDeg = Angle_NormalizeSignedDeg(camera->pitchDeg);
}
