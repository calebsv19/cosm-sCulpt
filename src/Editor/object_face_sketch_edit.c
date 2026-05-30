#include "Editor/object_face_sketch_edit.h"

#include "Core/space_mode_adapter.h"
#include "Editor/object_face_sketch.h"

#include <math.h>

static const float kObjectFaceSketchPlaneSnapEpsilon = 1e-3f;

static bool ObjectFaceSketchEdit_ScreenToFaceUV(const GlobalState* state,
                                                int screen_x,
                                                int screen_y,
                                                Vec2* out_uv) {
    SpaceViewContext view_ctx = {0};
    Vec2 view_pos = {0};
    Ray3 ray = {0};
    Plane3 plane = {0};
    Vec3 world = {0};
    const PlaneFrame3* frame = NULL;

    if (out_uv) *out_uv = (Vec2){0.0f, 0.0f};
    if (!state || !out_uv) return false;

    frame = &state->editor.objectFaceSketchFrame;
    view_ctx = SpaceAdapter_BuildViewContext((GlobalState*)state);
    view_pos = ScreenToWorld(screen_x, screen_y, &state->grid);
    ray = Ray3_FromPlaneViewPoint(view_pos, view_ctx.plane.axis);
    plane = Plane3_FromPointNormal(frame->origin, frame->normal);
    if (view_ctx.camera.enabled) {
        Vec3 right = FreeView_Right(&view_ctx.camera);
        Vec3 up = FreeView_Up(&view_ctx.camera);
        Vec3 forward = FreeView_Forward(&view_ctx.camera);
        ray.origin = Vec3_Add(view_ctx.camera.target,
                              Vec3_Add(Vec3_Scale(right, view_pos.x),
                                       Vec3_Scale(up, view_pos.y)));
        ray.direction = forward;

        // Face-focused free-view keeps the camera target on the sketch plane, so
        // screen-space drags should map directly through the target basis even
        // when the ray/plane intersection becomes numerically unstable at t ~= 0.
        if (fabsf(Plane3_SignedDistance(plane, view_ctx.camera.target)) <=
            kObjectFaceSketchPlaneSnapEpsilon) {
            world = Plane3_ProjectPoint(plane, ray.origin);
        } else if (!Ray3_IntersectPlane(ray, plane, NULL, &world)) {
            const float signed_distance = Plane3_SignedDistance(plane, ray.origin);
            if (fabsf(signed_distance) > kObjectFaceSketchPlaneSnapEpsilon) {
                return false;
            }
            world = Plane3_ProjectPoint(plane, ray.origin);
        }
    } else {
        if (!Ray3_IntersectPlane(ray, plane, NULL, &world)) {
            return false;
        }
    }

    {
        const Vec3 delta = Vec3_Sub(world, frame->origin);
        out_uv->x = Vec3_Dot(delta, Vec3_Normalize(frame->axisU));
        out_uv->y = Vec3_Dot(delta, Vec3_Normalize(frame->axisV));
    }
    return true;
}

static void ObjectFaceSketchEdit_ApplyDrag(EditorState* editor, Vec2 pointer_uv) {
    Vec2 min_uv = editor->objectFaceSketchEditStartMinUV;
    Vec2 max_uv = editor->objectFaceSketchEditStartMaxUV;
    const Vec2 delta = {
        pointer_uv.x - editor->objectFaceSketchEditStartUV.x,
        pointer_uv.y - editor->objectFaceSketchEditStartUV.y
    };

    switch ((ObjectFaceSketchHandleKind)editor->activeObjectFaceSketchHandle) {
        case OBJECT_FACE_SKETCH_HANDLE_BODY:
            min_uv.x += delta.x;
            min_uv.y += delta.y;
            max_uv.x += delta.x;
            max_uv.y += delta.y;
            break;
        case OBJECT_FACE_SKETCH_HANDLE_CORNER_MIN_U_MIN_V:
            min_uv = pointer_uv;
            break;
        case OBJECT_FACE_SKETCH_HANDLE_CORNER_POS_U_MIN_V:
            min_uv.y = pointer_uv.y;
            max_uv.x = pointer_uv.x;
            break;
        case OBJECT_FACE_SKETCH_HANDLE_CORNER_POS_U_POS_V:
            max_uv = pointer_uv;
            break;
        case OBJECT_FACE_SKETCH_HANDLE_CORNER_MIN_U_POS_V:
            min_uv.x = pointer_uv.x;
            max_uv.y = pointer_uv.y;
            break;
        case OBJECT_FACE_SKETCH_HANDLE_NONE:
        default:
            return;
    }

    Editor_ObjectFaceSketchSetRectangleUV(editor, min_uv, max_uv);
}

bool Editor_ObjectFaceSketchBeginEditDrag(GlobalState* state,
                                          ObjectFaceSketchHandleKind handle,
                                          int mouse_x,
                                          int mouse_y) {
    EditorState* editor = NULL;
    Vec2 pointer_uv = {0};

    if (!state) return false;
    editor = &state->editor;
    if (!Editor_ObjectFaceSketchHasCommittedRectangle(editor)) return false;
    if (handle == OBJECT_FACE_SKETCH_HANDLE_NONE) return false;
    if (!ObjectFaceSketchEdit_ScreenToFaceUV(state, mouse_x, mouse_y, &pointer_uv)) return false;

    Editor_ObjectFaceSketchGetRectangleUV(editor,
                                          &editor->objectFaceSketchEditStartMinUV,
                                          &editor->objectFaceSketchEditStartMaxUV);
    editor->objectFaceSketchEditStartUV = pointer_uv;
    (void)Editor_ObjectFaceSketchSelect(editor, handle);
    editor->activeObjectFaceSketchHandle = handle;
    editor->objectFaceSketchEditDragging = true;
    return true;
}

void Editor_ObjectFaceSketchUpdateEditDrag(GlobalState* state, int mouse_x, int mouse_y) {
    EditorState* editor = NULL;
    Vec2 pointer_uv = {0};

    if (!state) return;
    editor = &state->editor;
    if (!editor->objectFaceSketchEditDragging) return;
    if (!ObjectFaceSketchEdit_ScreenToFaceUV(state, mouse_x, mouse_y, &pointer_uv)) return;

    ObjectFaceSketchEdit_ApplyDrag(editor, pointer_uv);
    Global_FlagHitboxesDirty();
}

void Editor_ObjectFaceSketchEndEditDrag(GlobalState* state, int mouse_x, int mouse_y) {
    EditorState* editor = NULL;

    if (!state) return;
    editor = &state->editor;
    if (!editor->objectFaceSketchEditDragging) return;

    Editor_ObjectFaceSketchUpdateEditDrag(state, mouse_x, mouse_y);
    editor->objectFaceSketchEditDragging = false;
    editor->activeObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_NONE;

    if (!Editor_ObjectFaceSketchHasCommittedRectangle(editor)) {
        Editor_ObjectFaceSketchDeselect(editor);
    }
}
