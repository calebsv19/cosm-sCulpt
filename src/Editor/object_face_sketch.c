#include "Editor/object_face_extrude.h"
#include "Editor/object_face_sketch.h"

#include "Core/space_mode_adapter.h"
#include "Layout/scene/layout_object_faces.h"

#include <SDL2/SDL.h>
#include <math.h>

static const float kObjectFaceSketchPlaneSnapEpsilon = 1e-3f;

static bool ScreenToPlaneFrameWorld(int screen_x,
                                    int screen_y,
                                    const Grid* grid,
                                    const SpaceViewContext* view_ctx,
                                    const PlaneFrame3* frame,
                                    bool snap_to_grid,
                                    Vec3* out_world) {
    Vec2 view_pos = {0};
    Ray3 ray = {0};
    Plane3 plane = {0};

    if (!grid || !view_ctx || !frame || !out_world) return false;
    view_pos = snap_to_grid
        ? ScreenToSnappedWorld(screen_x, screen_y, grid)
        : ScreenToWorld(screen_x, screen_y, grid);

    ray = Ray3_FromPlaneViewPoint(view_pos, view_ctx->plane.axis);
    plane = Plane3_FromPointNormal(frame->origin, frame->normal);
    if (view_ctx->camera.enabled) {
        Vec3 right = FreeView_Right(&view_ctx->camera);
        Vec3 up = FreeView_Up(&view_ctx->camera);
        Vec3 forward = FreeView_Forward(&view_ctx->camera);
        ray.origin = Vec3_Add(view_ctx->camera.target,
                              Vec3_Add(Vec3_Scale(right, view_pos.x),
                                       Vec3_Scale(up, view_pos.y)));
        ray.direction = forward;

        // Face-focused free view targets the sketch plane directly, so clicks at
        // the face center can numerically land at t ~= 0. Keep marquee start
        // stable by projecting back onto the active face plane in that case.
        if (fabsf(Plane3_SignedDistance(plane, view_ctx->camera.target)) <=
            kObjectFaceSketchPlaneSnapEpsilon) {
            *out_world = Plane3_ProjectPoint(plane, ray.origin);
            return true;
        }
    }

    if (Ray3_IntersectPlane(ray, plane, NULL, out_world)) {
        return true;
    }

    if (fabsf(Plane3_SignedDistance(plane, ray.origin)) > kObjectFaceSketchPlaneSnapEpsilon) {
        return false;
    }
    *out_world = Plane3_ProjectPoint(plane, ray.origin);
    return true;
}

static bool ObjectFaceSketch_HasTarget(const EditorState* editor) {
    return editor &&
           editor->selectedObjectAssetBodyId != 0u &&
           editor->selectedObjectAssetFace != OBJECT3D_FACE_NONE;
}

static Vec2 ObjectFaceSketch_WorldToFaceUV(const PlaneFrame3* frame, Vec3 world) {
    Vec3 delta = Vec3_Sub(world, frame->origin);
    return (Vec2){
        .x = Vec3_Dot(delta, Vec3_Normalize(frame->axisU)),
        .y = Vec3_Dot(delta, Vec3_Normalize(frame->axisV))
    };
}

static ObjectAuthoringSession* ObjectFaceSketch_GlobalSessionForEditor(EditorState* editor) {
    GlobalState* state = Global_Get();
    if (!state || editor != &state->editor || !state->objectAuthoring.attached) {
        return NULL;
    }
    return &state->objectAuthoring;
}

static void ObjectFaceSketch_SyncCommittedRectangleToSession(EditorState* editor) {
    ObjectAuthoringSession* session = NULL;
    Vec2 min_uv = {0};
    Vec2 max_uv = {0};
    if (!editor || !editor->objectFaceSketchHasRectangle) return;
    session = ObjectFaceSketch_GlobalSessionForEditor(editor);
    if (!session) return;
    min_uv = (Vec2){
        fminf(editor->objectFaceSketchStartUV.x, editor->objectFaceSketchCurrentUV.x),
        fminf(editor->objectFaceSketchStartUV.y, editor->objectFaceSketchCurrentUV.y)
    };
    max_uv = (Vec2){
        fmaxf(editor->objectFaceSketchStartUV.x, editor->objectFaceSketchCurrentUV.x),
        fmaxf(editor->objectFaceSketchStartUV.y, editor->objectFaceSketchCurrentUV.y)
    };
    (void)ObjectAuthoringSession_SetRectangleSketch(session,
                                                    editor->objectFaceSketchBodyId,
                                                    editor->objectFaceSketchFace,
                                                    editor->objectFaceSketchFrame,
                                                    min_uv,
                                                    max_uv,
                                                    NULL);
}

static bool ObjectFaceSketch_BuildActiveSessionSketch(const GlobalState* state,
                                                      ObjectAuthoringSketch* out_sketch) {
    const ObjectAuthoringSketch* sketch = NULL;
    if (out_sketch) {
        *out_sketch = (ObjectAuthoringSketch){0};
    }
    if (!state || !state->objectAuthoring.attached) return false;
    sketch = ObjectAuthoringDocument_ActiveSketch(&state->objectAuthoring.document);
    if (!sketch) return false;
    if (out_sketch) {
        *out_sketch = *sketch;
    }
    return true;
}

static void ObjectFaceSketch_DrawHorizontalLine(SDL_Renderer* renderer,
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

static float ObjectFaceSketch_EdgeXAtY(Vec2 a, Vec2 b, float y) {
    const float dy = b.y - a.y;
    if (fabsf(dy) <= 1e-5f) return a.x;
    return a.x + ((y - a.y) * (b.x - a.x) / dy);
}

static void ObjectFaceSketch_FillFlatBottomTriangle(SDL_Renderer* renderer,
                                                    Vec2 top,
                                                    Vec2 left,
                                                    Vec2 right) {
    const int y_start = (int)ceilf(top.y);
    const int y_end = (int)floorf(SDL_max(left.y, right.y));
    for (int y = y_start; y <= y_end; ++y) {
        const float sample_y = (float)y + 0.5f;
        const float x0 = ObjectFaceSketch_EdgeXAtY(top, left, sample_y);
        const float x1 = ObjectFaceSketch_EdgeXAtY(top, right, sample_y);
        ObjectFaceSketch_DrawHorizontalLine(renderer, y, x0, x1);
    }
}

static void ObjectFaceSketch_FillFlatTopTriangle(SDL_Renderer* renderer,
                                                 Vec2 left,
                                                 Vec2 right,
                                                 Vec2 bottom) {
    const int y_start = (int)ceilf(SDL_min(left.y, right.y));
    const int y_end = (int)floorf(bottom.y);
    for (int y = y_start; y <= y_end; ++y) {
        const float sample_y = (float)y + 0.5f;
        const float x0 = ObjectFaceSketch_EdgeXAtY(left, bottom, sample_y);
        const float x1 = ObjectFaceSketch_EdgeXAtY(right, bottom, sample_y);
        ObjectFaceSketch_DrawHorizontalLine(renderer, y, x0, x1);
    }
}

static void ObjectFaceSketch_FillTriangle(SDL_Renderer* renderer, Vec2 p0, Vec2 p1, Vec2 p2) {
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
        ObjectFaceSketch_FillFlatBottomTriangle(renderer, points[0], points[1], points[2]);
        return;
    }
    if (fabsf(points[0].y - points[1].y) <= 1e-5f) {
        ObjectFaceSketch_FillFlatTopTriangle(renderer, points[0], points[1], points[2]);
        return;
    }

    {
        const float split_t = (points[1].y - points[0].y) / (points[2].y - points[0].y);
        const Vec2 split = {
            points[0].x + ((points[2].x - points[0].x) * split_t),
            points[1].y
        };
        ObjectFaceSketch_FillFlatBottomTriangle(renderer, points[0], points[1], split);
        ObjectFaceSketch_FillFlatTopTriangle(renderer, points[1], split, points[2]);
    }
}

static void ObjectFaceSketch_FaceUVToWorldCorners(PlaneFrame3 frame,
                                                  Vec2 min_uv,
                                                  Vec2 max_uv,
                                                  Vec3 out_corners[4]) {
    const Vec3 axis_u = Vec3_Normalize(frame.axisU);
    const Vec3 axis_v = Vec3_Normalize(frame.axisV);
    const Vec3 origin = frame.origin;

    out_corners[0] = Vec3_Add(origin, Vec3_Add(Vec3_Scale(axis_u, min_uv.x), Vec3_Scale(axis_v, min_uv.y)));
    out_corners[1] = Vec3_Add(origin, Vec3_Add(Vec3_Scale(axis_u, max_uv.x), Vec3_Scale(axis_v, min_uv.y)));
    out_corners[2] = Vec3_Add(origin, Vec3_Add(Vec3_Scale(axis_u, max_uv.x), Vec3_Scale(axis_v, max_uv.y)));
    out_corners[3] = Vec3_Add(origin, Vec3_Add(Vec3_Scale(axis_u, min_uv.x), Vec3_Scale(axis_v, max_uv.y)));
}

void Editor_ObjectFaceSketchClear(EditorState* editor) {
    ObjectAuthoringSession* session = ObjectFaceSketch_GlobalSessionForEditor(editor);
    if (!editor) return;
    if (session) {
        (void)ObjectAuthoringSession_ClearActiveSketch(session);
    }
    editor->objectFaceSketchToolArmed = false;
    editor->objectFaceSketchDragging = false;
    editor->objectFaceSketchHasRectangle = false;
    editor->objectFaceSketchBodyId = 0u;
    editor->objectFaceSketchFace = OBJECT3D_FACE_NONE;
    editor->objectFaceSketchFrame = (PlaneFrame3){0};
    editor->objectFaceSketchStartUV = (Vec2){0.0f, 0.0f};
    editor->objectFaceSketchCurrentUV = (Vec2){0.0f, 0.0f};
    editor->hoveredObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_NONE;
    editor->selectedObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_NONE;
    editor->activeObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_NONE;
    editor->objectFaceSketchEditDragging = false;
    editor->objectFaceSketchEditStartUV = (Vec2){0.0f, 0.0f};
    editor->objectFaceSketchEditStartMinUV = (Vec2){0.0f, 0.0f};
    editor->objectFaceSketchEditStartMaxUV = (Vec2){0.0f, 0.0f};
    editor->objectAuthoringMode = Editor_ObjectAuthoringIdleMode(editor);
}

bool Editor_ObjectFaceSketchHasCommittedRectangle(const EditorState* editor) {
    return editor && editor->objectFaceSketchHasRectangle;
}

bool Editor_ObjectFaceSketchIsSelected(const EditorState* editor) {
    return editor &&
           editor->objectFaceSketchHasRectangle &&
           editor->objectAuthoringMode == OBJECT_AUTHORING_MODE_SKETCH_SELECT &&
           editor->selectedObjectFaceSketchHandle != OBJECT_FACE_SKETCH_HANDLE_NONE &&
           editor->selectedObjectAssetBodyId == editor->objectFaceSketchBodyId &&
           editor->selectedObjectAssetFace == editor->objectFaceSketchFace;
}

bool Editor_ObjectFaceSketchSelect(EditorState* editor, ObjectFaceSketchHandleKind handle) {
    if (!editor) return false;
    if (!editor->objectFaceSketchHasRectangle) return false;
    if (handle == OBJECT_FACE_SKETCH_HANDLE_NONE) {
        handle = OBJECT_FACE_SKETCH_HANDLE_BODY;
    }

    editor->selectedObject3DId = editor->objectFaceSketchBodyId;
    editor->selectedObjectAssetBodyId = editor->objectFaceSketchBodyId;
    editor->selectedObjectAssetFace = editor->objectFaceSketchFace;
    editor->selectedObjectFaceSketchHandle = handle;
    editor->activeObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_NONE;
    editor->objectAuthoringMode = OBJECT_AUTHORING_MODE_SKETCH_SELECT;
    return true;
}

void Editor_ObjectFaceSketchDeselect(EditorState* editor) {
    if (!editor) return;
    editor->selectedObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_NONE;
    editor->activeObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_NONE;
    if (editor->objectAuthoringMode == OBJECT_AUTHORING_MODE_SKETCH_SELECT) {
        editor->objectAuthoringMode = Editor_ObjectAuthoringIdleMode(editor);
    }
}

bool Editor_ObjectFaceSketchSyncFromAuthoring(GlobalState* state) {
    ObjectAuthoringSketch sketch = {0};
    if (!state) return false;
    if (!ObjectFaceSketch_BuildActiveSessionSketch(state, &sketch)) return false;

    state->editor.objectFaceSketchToolArmed = false;
    state->editor.objectFaceSketchDragging = false;
    state->editor.objectFaceSketchHasRectangle = true;
    state->editor.objectFaceSketchBodyId = sketch.faceRef.bodyId;
    state->editor.objectFaceSketchFace = sketch.faceRef.primitiveFace;
    state->editor.objectFaceSketchFrame = sketch.frame;
    state->editor.objectFaceSketchStartUV = sketch.minUV;
    state->editor.objectFaceSketchCurrentUV = sketch.maxUV;
    return true;
}

void Editor_ObjectFaceSketchGetRectangleUV(const EditorState* editor,
                                           Vec2* out_min_uv,
                                           Vec2* out_max_uv) {
    const Vec2 min_uv = {
        fminf(editor->objectFaceSketchStartUV.x, editor->objectFaceSketchCurrentUV.x),
        fminf(editor->objectFaceSketchStartUV.y, editor->objectFaceSketchCurrentUV.y)
    };
    const Vec2 max_uv = {
        fmaxf(editor->objectFaceSketchStartUV.x, editor->objectFaceSketchCurrentUV.x),
        fmaxf(editor->objectFaceSketchStartUV.y, editor->objectFaceSketchCurrentUV.y)
    };

    if (out_min_uv) *out_min_uv = min_uv;
    if (out_max_uv) *out_max_uv = max_uv;
}

void Editor_ObjectFaceSketchSetRectangleUV(EditorState* editor,
                                           Vec2 min_uv,
                                           Vec2 max_uv) {
    if (!editor) return;
    editor->objectFaceSketchStartUV = (Vec2){
        fminf(min_uv.x, max_uv.x),
        fminf(min_uv.y, max_uv.y)
    };
    editor->objectFaceSketchCurrentUV = (Vec2){
        fmaxf(min_uv.x, max_uv.x),
        fmaxf(min_uv.y, max_uv.y)
    };
    editor->objectFaceSketchHasRectangle =
        fabsf(editor->objectFaceSketchCurrentUV.x - editor->objectFaceSketchStartUV.x) > 1e-3f &&
        fabsf(editor->objectFaceSketchCurrentUV.y - editor->objectFaceSketchStartUV.y) > 1e-3f;
    if (editor->objectFaceSketchHasRectangle) {
        ObjectFaceSketch_SyncCommittedRectangleToSession(editor);
    } else {
        ObjectAuthoringSession* session = ObjectFaceSketch_GlobalSessionForEditor(editor);
        if (session) {
            (void)ObjectAuthoringSession_ClearActiveSketch(session);
        }
    }
}

bool Editor_ObjectFaceSketchArmRectangle(GlobalState* state) {
    const Object3D* object = NULL;
    PlaneFrame3 frame = {0};
    if (!state) return false;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return false;
    if (!ObjectFaceSketch_HasTarget(&state->editor)) return false;

    object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                          state->editor.selectedObjectAssetBodyId);
    if (!Layout_Object3DFace_GetFrame(object,
                                      state->editor.selectedObjectAssetFace,
                                      &frame)) {
        return false;
    }

    Editor_ObjectFaceSketchClear(&state->editor);
    Editor_ObjectFaceExtrudeClear(&state->editor);
    state->editor.objectFaceSketchToolArmed = true;
    state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_SKETCH_DRAW;
    state->editor.objectFaceSketchBodyId = state->editor.selectedObjectAssetBodyId;
    state->editor.objectFaceSketchFace = state->editor.selectedObjectAssetFace;
    state->editor.objectFaceSketchFrame = frame;
    if (state->objectAuthoring.attached) {
        (void)ObjectAuthoringSession_SetSelection(&state->objectAuthoring,
                                                  state->editor.objectFaceSketchBodyId,
                                                  state->editor.objectFaceSketchFace);
    }
    return true;
}

bool Editor_ObjectFaceSketchHandleLeftMouseDown(GlobalState* state, int mouse_x, int mouse_y) {
    SpaceViewContext view_ctx = {0};
    Vec3 world = {0};
    if (!state) return false;
    if (!state->editor.objectFaceSketchToolArmed) return false;

    view_ctx = SpaceAdapter_BuildViewContext(state);
    if (!ScreenToPlaneFrameWorld(mouse_x,
                                 mouse_y,
                                 &state->grid,
                                 &view_ctx,
                                 &state->editor.objectFaceSketchFrame,
                                 false,
                                 &world)) {
        return false;
    }

    state->editor.objectFaceSketchStartUV =
        ObjectFaceSketch_WorldToFaceUV(&state->editor.objectFaceSketchFrame, world);
    state->editor.objectFaceSketchCurrentUV = state->editor.objectFaceSketchStartUV;
    state->editor.objectFaceSketchDragging = true;
    state->editor.objectFaceSketchHasRectangle = false;
    state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_SKETCH_DRAW;
    return true;
}

void Editor_ObjectFaceSketchHandleMouseMotion(GlobalState* state, int mouse_x, int mouse_y) {
    SpaceViewContext view_ctx = {0};
    Vec3 world = {0};
    if (!state || !state->editor.objectFaceSketchDragging) return;

    view_ctx = SpaceAdapter_BuildViewContext(state);
    if (!ScreenToPlaneFrameWorld(mouse_x,
                                 mouse_y,
                                 &state->grid,
                                 &view_ctx,
                                 &state->editor.objectFaceSketchFrame,
                                 false,
                                 &world)) {
        return;
    }

    state->editor.objectFaceSketchCurrentUV =
        ObjectFaceSketch_WorldToFaceUV(&state->editor.objectFaceSketchFrame, world);
}

void Editor_ObjectFaceSketchHandleLeftMouseUp(GlobalState* state, int mouse_x, int mouse_y) {
    const float min_extent = 1e-3f;
    if (!state || !state->editor.objectFaceSketchDragging) return;

    Editor_ObjectFaceSketchHandleMouseMotion(state, mouse_x, mouse_y);
    state->editor.objectFaceSketchDragging = false;
    state->editor.objectFaceSketchHasRectangle =
        fabsf(state->editor.objectFaceSketchCurrentUV.x - state->editor.objectFaceSketchStartUV.x) > min_extent &&
        fabsf(state->editor.objectFaceSketchCurrentUV.y - state->editor.objectFaceSketchStartUV.y) > min_extent;
    state->editor.objectFaceSketchToolArmed = false;
    if (state->editor.objectFaceSketchHasRectangle) {
        ObjectFaceSketch_SyncCommittedRectangleToSession(&state->editor);
        (void)Editor_ObjectFaceSketchSelect(&state->editor, OBJECT_FACE_SKETCH_HANDLE_BODY);
    } else {
        state->editor.objectAuthoringMode = Editor_ObjectAuthoringIdleMode(&state->editor);
    }
}

static void RenderObjectFaceSketchQuad(SDL_Renderer* renderer,
                                       const Grid* grid,
                                       const SpaceViewContext* view_ctx,
                                       const Vec3 corners[4],
                                       const EditorState* editor) {
    Vec2 pts[4];
    const bool body_selected = editor &&
        editor->selectedObjectFaceSketchHandle == OBJECT_FACE_SKETCH_HANDLE_BODY &&
        Editor_ObjectFaceSketchIsSelected(editor);
    const bool body_hovered = editor &&
        editor->hoveredObjectFaceSketchHandle == OBJECT_FACE_SKETCH_HANDLE_BODY;
    SDL_Color fill = {255, 210, 92, 72};
    SDL_Color border = {255, 210, 92, 235};
    if (!renderer || !grid || !view_ctx || !corners) return;
    for (int i = 0; i < 4; ++i) {
        pts[i] = WorldToScreen(SpaceAdapter_ProjectToView(corners[i], view_ctx), grid);
    }

    if (body_selected) {
        fill = (SDL_Color){96, 176, 255, 92};
        border = (SDL_Color){120, 208, 255, 255};
    } else if (body_hovered) {
        fill = (SDL_Color){255, 224, 132, 88};
        border = (SDL_Color){255, 232, 164, 255};
    }

    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    ObjectFaceSketch_FillTriangle(renderer, pts[0], pts[1], pts[2]);
    ObjectFaceSketch_FillTriangle(renderer, pts[0], pts[2], pts[3]);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    for (int i = 0; i < 4; ++i) {
        const Vec2 a = pts[i];
        const Vec2 b = pts[(i + 1) % 4];
        SDL_RenderDrawLine(renderer, (int)lroundf(a.x), (int)lroundf(a.y),
                           (int)lroundf(b.x), (int)lroundf(b.y));
    }

    if (body_selected || body_hovered) {
        const Vec2 mid01 = { 0.5f * (pts[0].x + pts[1].x), 0.5f * (pts[0].y + pts[1].y) };
        const Vec2 mid12 = { 0.5f * (pts[1].x + pts[2].x), 0.5f * (pts[1].y + pts[2].y) };
        const Vec2 mid23 = { 0.5f * (pts[2].x + pts[3].x), 0.5f * (pts[2].y + pts[3].y) };
        const Vec2 mid30 = { 0.5f * (pts[3].x + pts[0].x), 0.5f * (pts[3].y + pts[0].y) };
        const Vec2 center = {
            0.25f * (pts[0].x + pts[1].x + pts[2].x + pts[3].x),
            0.25f * (pts[0].y + pts[1].y + pts[2].y + pts[3].y)
        };
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
        SDL_RenderDrawLine(renderer, (int)lroundf(center.x), (int)lroundf(center.y),
                           (int)lroundf(mid01.x), (int)lroundf(mid01.y));
        SDL_RenderDrawLine(renderer, (int)lroundf(center.x), (int)lroundf(center.y),
                           (int)lroundf(mid12.x), (int)lroundf(mid12.y));
        SDL_RenderDrawLine(renderer, (int)lroundf(center.x), (int)lroundf(center.y),
                           (int)lroundf(mid23.x), (int)lroundf(mid23.y));
        SDL_RenderDrawLine(renderer, (int)lroundf(center.x), (int)lroundf(center.y),
                           (int)lroundf(mid30.x), (int)lroundf(mid30.y));
        {
            const int radius = body_selected ? 7 : 6;
            SDL_Rect center_rect = {
                (int)lroundf(center.x) - radius,
                (int)lroundf(center.y) - radius,
                radius * 2,
                radius * 2
            };
            SDL_Color center_fill = body_selected
                ? (SDL_Color){120, 208, 255, 255}
                : (SDL_Color){255, 232, 164, 248};
            SDL_SetRenderDrawColor(renderer,
                                   center_fill.r,
                                   center_fill.g,
                                   center_fill.b,
                                   center_fill.a);
            SDL_RenderFillRect(renderer, &center_rect);
            SDL_SetRenderDrawColor(renderer, 24, 26, 32, 255);
            SDL_RenderDrawRect(renderer, &center_rect);
        }
    }
}

static void RenderObjectFaceSketchHandles(SDL_Renderer* renderer,
                                          const Grid* grid,
                                          const SpaceViewContext* view_ctx,
                                          const Vec3 corners[4],
                                          const EditorState* editor) {
    if (!renderer || !grid || !view_ctx || !corners || !editor) return;

    for (int i = 0; i < 4; ++i) {
        const ObjectFaceSketchHandleKind handle =
            (ObjectFaceSketchHandleKind)(OBJECT_FACE_SKETCH_HANDLE_CORNER_MIN_U_MIN_V + i);
        const Vec2 screen = WorldToScreen(SpaceAdapter_ProjectToView(corners[i], view_ctx), grid);
        const bool hovered = editor->hoveredObjectFaceSketchHandle == (int)handle;
        const bool selected = editor->selectedObjectFaceSketchHandle == (int)handle;
        const bool active = editor->activeObjectFaceSketchHandle == (int)handle &&
                            editor->objectFaceSketchEditDragging;
        const int size = selected || active ? 7 : 6;
        SDL_Rect rect = {
            (int)lroundf(screen.x) - size,
            (int)lroundf(screen.y) - size,
            size * 2,
            size * 2
        };
        SDL_Color fill = active ? (SDL_Color){255, 214, 120, 255}
                                : selected ? (SDL_Color){144, 184, 255, 255}
                                           : hovered ? (SDL_Color){196, 220, 255, 255}
                                                     : (SDL_Color){160, 192, 255, 240};
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, 24, 26, 32, 255);
        SDL_RenderDrawRect(renderer, &rect);
    }
}

void Render_EditorObjectFaceSketch(EditorState* editor, AppContext* ctx) {
    GlobalState* state = Global_Get();
    SpaceViewContext view_ctx = {0};
    ObjectAuthoringSketch sketch = {0};
    PlaneFrame3 frame = {0};
    Vec2 min_uv = {0};
    Vec2 max_uv = {0};
    Vec3 corners[4];

    if (!editor || !ctx || !state) return;
    if (editor->objectFaceSketchDragging) {
        frame = editor->objectFaceSketchFrame;
        Editor_ObjectFaceSketchGetRectangleUV(editor, &min_uv, &max_uv);
    } else if (ObjectFaceSketch_BuildActiveSessionSketch(state, &sketch)) {
        frame = sketch.frame;
        min_uv = sketch.minUV;
        max_uv = sketch.maxUV;
    } else if (editor->objectFaceSketchHasRectangle) {
        frame = editor->objectFaceSketchFrame;
        Editor_ObjectFaceSketchGetRectangleUV(editor, &min_uv, &max_uv);
    } else {
        return;
    }

    view_ctx = SpaceAdapter_BuildViewContext(state);
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
#endif
    ObjectFaceSketch_FaceUVToWorldCorners(frame, min_uv, max_uv, corners);
    RenderObjectFaceSketchQuad(ctx->renderer, &state->grid, &view_ctx, corners, editor);
    RenderObjectFaceSketchHandles(ctx->renderer, &state->grid, &view_ctx, corners, editor);
}
