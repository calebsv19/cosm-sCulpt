#include "Editor/object_face_extrude.h"

#include "Core/space_mode_adapter.h"
#include "Editor/object_face_sketch.h"
#include "Layout/scene/layout_object_faces.h"
#include "ObjectAuthoring/object_authoring_eval.h"

#include <SDL2/SDL.h>
#include <math.h>

typedef struct {
    float u0;
    float u1;
    float v0;
    float v1;
    float w0;
    float w1;
} FaceLocalPrismBox;

typedef struct {
    float face_width;
    float face_height;
    float face_depth;
} ObjectFaceRectPrismExtents;

static const float kObjectFaceExtrudeMinDepth = 1e-3f;
static const float kObjectFaceExtrudePreviewDepth = 1e-4f;

static float ObjectFaceExtrude_DefaultDepth(const GlobalState* state) {
    const float grid_step =
        (state && state->grid.gridSize > 0.0f) ? state->grid.gridSize : 1.0f;
    return fmaxf(grid_step, kObjectFaceExtrudeMinDepth * 2.0f);
}

static bool ObjectFaceExtrude_HasSketchTarget(const EditorState* editor) {
    return editor &&
           editor->objectFaceSketchHasRectangle &&
           editor->objectFaceSketchBodyId != 0u &&
           editor->objectFaceSketchFace != OBJECT3D_FACE_NONE &&
           (Editor_ObjectFaceSketchIsSelected(editor) ||
            (((editor->objectFaceExtrudeToolArmed ||
               editor->objectFaceExtrudeDragging ||
               editor->objectFaceExtrudeHasPreview) &&
              editor->selectedObjectAssetBodyId == editor->objectFaceSketchBodyId &&
              editor->selectedObjectAssetFace == editor->objectFaceSketchFace)));
}

static Vec2 ObjectFaceExtrude_SketchMinUV(const EditorState* editor) {
    return (Vec2){
        fminf(editor->objectFaceSketchStartUV.x, editor->objectFaceSketchCurrentUV.x),
        fminf(editor->objectFaceSketchStartUV.y, editor->objectFaceSketchCurrentUV.y)
    };
}

static Vec2 ObjectFaceExtrude_SketchMaxUV(const EditorState* editor) {
    return (Vec2){
        fmaxf(editor->objectFaceSketchStartUV.x, editor->objectFaceSketchCurrentUV.x),
        fmaxf(editor->objectFaceSketchStartUV.y, editor->objectFaceSketchCurrentUV.y)
    };
}

static bool ObjectFaceExtrude_ResolveRectPrismFaceExtents(const Object3D* object,
                                                          Object3DFaceKind face,
                                                          ObjectFaceRectPrismExtents* out_extents) {
    if (out_extents) {
        *out_extents = (ObjectFaceRectPrismExtents){0};
    }
    if (!object || !out_extents) return false;
    if (object->kind != OBJECT3D_KIND_RECT_PRISM) return false;

    switch (face) {
        case OBJECT3D_FACE_RECT_PRISM_NEG_N:
        case OBJECT3D_FACE_RECT_PRISM_POS_N:
            out_extents->face_width = object->rectPrism.width;
            out_extents->face_height = object->rectPrism.height;
            out_extents->face_depth = object->rectPrism.depth;
            return true;
        case OBJECT3D_FACE_RECT_PRISM_NEG_V:
        case OBJECT3D_FACE_RECT_PRISM_POS_V:
            out_extents->face_width = object->rectPrism.width;
            out_extents->face_height = object->rectPrism.depth;
            out_extents->face_depth = object->rectPrism.height;
            return true;
        case OBJECT3D_FACE_RECT_PRISM_NEG_U:
        case OBJECT3D_FACE_RECT_PRISM_POS_U:
            out_extents->face_width = object->rectPrism.height;
            out_extents->face_height = object->rectPrism.depth;
            out_extents->face_depth = object->rectPrism.width;
            return true;
        case OBJECT3D_FACE_NONE:
        case OBJECT3D_FACE_PLANE_SURFACE:
        default:
            return false;
    }
}

static float ObjectFaceExtrude_ResolveDepthFromMouse(const GlobalState* state,
                                                     const EditorState* editor,
                                                     int mouse_x,
                                                     int mouse_y) {
    const Vec2 mouse_delta = {
        (float)mouse_x - editor->objectFaceExtrudeStartScreen.x,
        (float)mouse_y - editor->objectFaceExtrudeStartScreen.y
    };
    SpaceViewContext view_ctx = {0};
    Vec2 origin_screen = {0};
    Vec2 normal_screen = {0};
    Vec2 projected_delta = {0};
    float pixels_per_unit = 0.0f;
    float signed_units = 0.0f;
    const float fallback_pixels_per_unit =
        fmaxf(1.0f, state->grid.gridSize * fmaxf(state->grid.scale, 1e-3f));

    if (!state || !editor) return 0.0f;

    view_ctx = SpaceAdapter_BuildViewContext((GlobalState*)state);
    origin_screen = WorldToScreen(
        SpaceAdapter_ProjectToView(editor->objectFaceExtrudeFrame.origin, &view_ctx),
        &state->grid);
    normal_screen = WorldToScreen(
        SpaceAdapter_ProjectToView(
            Vec3_Add(editor->objectFaceExtrudeFrame.origin,
                     Vec3_Normalize(editor->objectFaceExtrudeFrame.normal)),
            &view_ctx),
        &state->grid);

    projected_delta = (Vec2){
        normal_screen.x - origin_screen.x,
        normal_screen.y - origin_screen.y
    };
    pixels_per_unit = sqrtf(projected_delta.x * projected_delta.x +
                            projected_delta.y * projected_delta.y);
    if (pixels_per_unit > 1e-3f) {
        const Vec2 dir = {
            projected_delta.x / pixels_per_unit,
            projected_delta.y / pixels_per_unit
        };
        signed_units = (mouse_delta.x * dir.x + mouse_delta.y * dir.y) / pixels_per_unit;
        return fabsf(signed_units);
    }

    return fabsf(mouse_delta.y) / fallback_pixels_per_unit;
}

static bool ObjectFaceExtrude_CreateRectPrism(GlobalState* state,
                                              PlaneFrame3 frame,
                                              float width,
                                              float height,
                                              float depth,
                                              uint32_t* out_object_id) {
    RectPrismPrimitiveCreateParams params;
    bool bounds_adjusted = false;
    if (out_object_id) *out_object_id = 0u;
    if (!state) return false;
    if (width <= kObjectFaceExtrudeMinDepth ||
        height <= kObjectFaceExtrudeMinDepth ||
        depth <= kObjectFaceExtrudeMinDepth) {
        return false;
    }

    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = width;
    params.height = height;
    params.depth = depth;
    params.useExplicitFrame = true;
    params.explicitFrame = frame;
    params.lockToConstructionPlane = false;
    params.lockToBounds = false;
    return Layout_CreateRectPrismPrimitive(&state->layout,
                                           &params,
                                           out_object_id,
                                           &bounds_adjusted);
}

static ObjectAuthoringBody ObjectFaceExtrude_BuildRectPrismBody(uint32_t body_id,
                                                                PlaneFrame3 frame,
                                                                float width,
                                                                float height,
                                                                float depth) {
    Transform3D transform = Layout_Transform3D_Default();
    transform.position = frame.origin;
    return (ObjectAuthoringBody){
        .bodyId = body_id,
        .sourceObjectId = body_id,
        .authoringKind = OBJECT_AUTHORING_BODY_KIND_RECT_PRISM_PRIMITIVE,
        .sourceKind = OBJECT3D_KIND_RECT_PRISM,
        .transform = transform,
        .rectPrism = {
            .width = width,
            .height = height,
            .depth = depth,
            .frame = frame,
            .lockToConstructionPlane = false,
            .lockToBounds = false
        }
    };
}

static bool ObjectFaceExtrude_ReplayAuthoringToLayout(GlobalState* state) {
    ObjectAuthoringDocument evaluated;
    ObjectAuthoringEvalDiagnostics diagnostics;
    bool ok = false;
    if (!state || !state->objectAuthoring.attached) return false;
    ObjectAuthoringDocument_Init(&evaluated);
    ok = ObjectAuthoring_EvaluateDocument(&state->objectAuthoring.document,
                                          &evaluated,
                                          &diagnostics) &&
         ObjectAuthoring_ApplyEvaluatedDocumentToLayout(&evaluated,
                                                        &state->layout,
                                                        &diagnostics) &&
         ObjectAuthoringSession_MirrorBodiesFromLayout(&state->objectAuthoring,
                                                       &state->layout);
    ObjectAuthoringDocument_Free(&evaluated);
    return ok;
}

static bool ObjectFaceExtrude_BoxToWorldFrame(const PlaneFrame3* face_frame,
                                              const FaceLocalPrismBox* box,
                                              PlaneFrame3* out_frame,
                                              float* out_width,
                                              float* out_height,
                                              float* out_depth) {
    const Vec3 axis_u = Vec3_Normalize(face_frame->axisU);
    const Vec3 axis_v = Vec3_Normalize(face_frame->axisV);
    const Vec3 axis_w = Vec3_Scale(Vec3_Normalize(face_frame->normal), -1.0f);
    const float width = box->u1 - box->u0;
    const float height = box->v1 - box->v0;
    const float depth = box->w1 - box->w0;
    const float uc = 0.5f * (box->u0 + box->u1);
    const float vc = 0.5f * (box->v0 + box->v1);
    const float wc = 0.5f * (box->w0 + box->w1);

    if (!face_frame || !box || !out_frame || !out_width || !out_height || !out_depth) return false;
    if (width <= kObjectFaceExtrudeMinDepth ||
        height <= kObjectFaceExtrudeMinDepth ||
        depth <= kObjectFaceExtrudeMinDepth) {
        return false;
    }

    *out_frame = *face_frame;
    out_frame->axisU = axis_u;
    out_frame->axisV = axis_v;
    out_frame->normal = axis_w;
    out_frame->origin = Vec3_Add(face_frame->origin,
                                 Vec3_Add(Vec3_Scale(axis_u, uc),
                                          Vec3_Add(Vec3_Scale(axis_v, vc),
                                                   Vec3_Scale(axis_w, wc))));
    *out_width = width;
    *out_height = height;
    *out_depth = depth;
    return true;
}

static bool ObjectFaceExtrude_CommitAdd(GlobalState* state) {
    EditorState* editor = NULL;
    Vec2 min_uv = {0};
    Vec2 max_uv = {0};
    Vec3 axis_u = {0};
    Vec3 axis_v = {0};
    Vec3 axis_n = {0};
    PlaneFrame3 frame = {0};
    Vec3 center = {0};
    uint32_t created_id = 0u;
    float width = 0.0f;
    float height = 0.0f;

    if (!state) return false;
    editor = &state->editor;
    if (editor->objectFaceExtrudeDepth <= kObjectFaceExtrudeMinDepth) return false;
    if (!ObjectFaceExtrude_HasSketchTarget(editor)) return false;

    min_uv = ObjectFaceExtrude_SketchMinUV(editor);
    max_uv = ObjectFaceExtrude_SketchMaxUV(editor);
    width = max_uv.x - min_uv.x;
    height = max_uv.y - min_uv.y;
    if (width <= kObjectFaceExtrudeMinDepth || height <= kObjectFaceExtrudeMinDepth) return false;

    axis_u = Vec3_Normalize(editor->objectFaceSketchFrame.axisU);
    axis_v = Vec3_Normalize(editor->objectFaceSketchFrame.axisV);
    axis_n = Vec3_Normalize(editor->objectFaceSketchFrame.normal);
    center = Vec3_Add(editor->objectFaceSketchFrame.origin,
                      Vec3_Add(Vec3_Scale(axis_u, 0.5f * (min_uv.x + max_uv.x)),
                               Vec3_Add(Vec3_Scale(axis_v, 0.5f * (min_uv.y + max_uv.y)),
                                        Vec3_Scale(axis_n, 0.5f * editor->objectFaceExtrudeDepth))));
    frame = editor->objectFaceSketchFrame;
    frame.axisU = axis_u;
    frame.axisV = axis_v;
    frame.normal = axis_n;
    frame.origin = center;

    if (state->objectAuthoring.attached) {
        ObjectAuthoringSession prior_session;
        ObjectAuthoringBody created_body;
        ObjectAuthoringSession_Init(&prior_session);
        if (!ObjectAuthoringSession_Copy(&prior_session, &state->objectAuthoring)) {
            ObjectAuthoringSession_Free(&prior_session);
            return false;
        }
        Editor_HistoryCapture(editor, &state->layout);
        created_id = state->layout.objectStore.nextObjectId;
        created_body = ObjectFaceExtrude_BuildRectPrismBody(created_id,
                                                            frame,
                                                            width,
                                                            height,
                                                            editor->objectFaceExtrudeDepth);
        if (!ObjectAuthoringSession_RecordExtrudeWithSnapshots(
                &state->objectAuthoring,
                OBJECT_AUTHORING_OPERATION_EXTRUDE_ADD,
                editor->objectFaceSketchBodyId,
                editor->objectFaceSketchFace,
                editor->objectFaceExtrudeDepth,
                &created_id,
                &created_body,
                1u,
                NULL) ||
            !ObjectFaceExtrude_ReplayAuthoringToLayout(state)) {
            (void)ObjectAuthoringSession_Copy(&state->objectAuthoring, &prior_session);
            ObjectAuthoringSession_Free(&prior_session);
            return false;
        }
        ObjectAuthoringSession_Free(&prior_session);
    } else {
        Editor_HistoryCapture(editor, &state->layout);
        if (!ObjectFaceExtrude_CreateRectPrism(state,
                                               frame,
                                               width,
                                               height,
                                               editor->objectFaceExtrudeDepth,
                                               &created_id)) {
            return false;
        }
    }

    editor->selectedObject3DId = editor->objectFaceSketchBodyId;
    editor->selectedObjectAssetBodyId = editor->objectFaceSketchBodyId;
    editor->selectedObjectAssetFace = editor->objectFaceSketchFace;
    (void)Editor_ObjectFaceSketchSelect(editor, OBJECT_FACE_SKETCH_HANDLE_BODY);
    Editor_ObjectFaceExtrudeClear(editor);
    Global_FlagLayoutChanged();
    Global_FlagHitboxesDirty();
    (void)created_id;
    return true;
}

static size_t ObjectFaceExtrude_BuildCutBoxes(const Object3D* target,
                                              Object3DFaceKind face,
                                              const EditorState* editor,
                                              FaceLocalPrismBox out_boxes[5]) {
    ObjectFaceRectPrismExtents extents = {0};
    Vec2 min_uv = {0};
    Vec2 max_uv = {0};
    float half_u = 0.0f;
    float half_v = 0.0f;
    float cut_depth = 0.0f;
    size_t count = 0u;

    if (!target || !editor || !out_boxes) return 0u;
    if (!ObjectFaceExtrude_ResolveRectPrismFaceExtents(target, face, &extents)) return 0u;

    min_uv = ObjectFaceExtrude_SketchMinUV(editor);
    max_uv = ObjectFaceExtrude_SketchMaxUV(editor);
    half_u = 0.5f * extents.face_width;
    half_v = 0.5f * extents.face_height;
    min_uv.x = fmaxf(-half_u, min_uv.x);
    min_uv.y = fmaxf(-half_v, min_uv.y);
    max_uv.x = fminf(half_u, max_uv.x);
    max_uv.y = fminf(half_v, max_uv.y);
    cut_depth = fminf(extents.face_depth, editor->objectFaceExtrudeDepth);

    if ((max_uv.x - min_uv.x) <= kObjectFaceExtrudeMinDepth ||
        (max_uv.y - min_uv.y) <= kObjectFaceExtrudeMinDepth ||
        cut_depth <= kObjectFaceExtrudeMinDepth) {
        return 0u;
    }

    if ((min_uv.x - (-half_u)) > kObjectFaceExtrudeMinDepth) {
        out_boxes[count++] = (FaceLocalPrismBox){
            .u0 = -half_u, .u1 = min_uv.x,
            .v0 = -half_v, .v1 = half_v,
            .w0 = 0.0f, .w1 = cut_depth
        };
    }
    if ((half_u - max_uv.x) > kObjectFaceExtrudeMinDepth) {
        out_boxes[count++] = (FaceLocalPrismBox){
            .u0 = max_uv.x, .u1 = half_u,
            .v0 = -half_v, .v1 = half_v,
            .w0 = 0.0f, .w1 = cut_depth
        };
    }
    if ((min_uv.y - (-half_v)) > kObjectFaceExtrudeMinDepth) {
        out_boxes[count++] = (FaceLocalPrismBox){
            .u0 = min_uv.x, .u1 = max_uv.x,
            .v0 = -half_v, .v1 = min_uv.y,
            .w0 = 0.0f, .w1 = cut_depth
        };
    }
    if ((half_v - max_uv.y) > kObjectFaceExtrudeMinDepth) {
        out_boxes[count++] = (FaceLocalPrismBox){
            .u0 = min_uv.x, .u1 = max_uv.x,
            .v0 = max_uv.y, .v1 = half_v,
            .w0 = 0.0f, .w1 = cut_depth
        };
    }
    if ((extents.face_depth - cut_depth) > kObjectFaceExtrudeMinDepth) {
        out_boxes[count++] = (FaceLocalPrismBox){
            .u0 = -half_u, .u1 = half_u,
            .v0 = -half_v, .v1 = half_v,
            .w0 = cut_depth, .w1 = extents.face_depth
        };
    }

    return count;
}

static bool ObjectFaceExtrude_CommitCut(GlobalState* state) {
    EditorState* editor = NULL;
    const Object3D* target = NULL;
    uint32_t target_body_id = 0u;
    Object3DFaceKind target_face = OBJECT3D_FACE_NONE;
    float target_depth = 0.0f;
    FaceLocalPrismBox boxes[5];
    PlaneFrame3 frames[5];
    uint32_t created_ids[5] = {0u};
    float widths[5] = {0};
    float heights[5] = {0};
    float depths[5] = {0};
    size_t box_count = 0u;

    if (!state) return false;
    editor = &state->editor;
    if (editor->objectFaceExtrudeDepth <= kObjectFaceExtrudeMinDepth) return false;
    if (!ObjectFaceExtrude_HasSketchTarget(editor)) return false;

    target = Layout_ObjectStore_FindConst(&state->layout.objectStore, editor->objectFaceSketchBodyId);
    if (!target || target->kind != OBJECT3D_KIND_RECT_PRISM) return false;
    target_body_id = editor->objectFaceSketchBodyId;
    target_face = editor->objectFaceSketchFace;
    target_depth = editor->objectFaceExtrudeDepth;

    box_count = ObjectFaceExtrude_BuildCutBoxes(target,
                                                editor->objectFaceSketchFace,
                                                editor,
                                                boxes);
    Editor_HistoryCapture(editor, &state->layout);
    for (size_t i = 0; i < box_count; ++i) {
        if (!ObjectFaceExtrude_BoxToWorldFrame(&editor->objectFaceSketchFrame,
                                               &boxes[i],
                                               &frames[i],
                                               &widths[i],
                                               &heights[i],
                                               &depths[i])) {
            return false;
        }
        if (state->objectAuthoring.attached) {
            created_ids[i] = state->layout.objectStore.nextObjectId + (uint32_t)i;
        } else if (!ObjectFaceExtrude_CreateRectPrism(state,
                                                      frames[i],
                                                      widths[i],
                                                      heights[i],
                                                      depths[i],
                                                      &created_ids[i])) {
            for (size_t created_index = 0; created_index < i; ++created_index) {
                if (created_ids[created_index] != 0u) {
                    (void)Layout_ObjectStore_Delete(&state->layout.objectStore,
                                                    created_ids[created_index]);
                }
            }
            return false;
        }
    }

    if (state->objectAuthoring.attached) {
        ObjectAuthoringBody result_bodies[5];
        ObjectAuthoringSession prior_session;
        ObjectAuthoringSession_Init(&prior_session);
        if (!ObjectAuthoringSession_Copy(&prior_session, &state->objectAuthoring)) {
            ObjectAuthoringSession_Free(&prior_session);
            return false;
        }
        for (size_t i = 0; i < box_count; ++i) {
            result_bodies[i] = ObjectFaceExtrude_BuildRectPrismBody(created_ids[i],
                                                                    frames[i],
                                                                    widths[i],
                                                                    heights[i],
                                                                    depths[i]);
        }
        if (!ObjectAuthoringSession_RecordExtrudeWithSnapshots(
                &state->objectAuthoring,
                OBJECT_AUTHORING_OPERATION_EXTRUDE_CUT,
                target_body_id,
                target_face,
                target_depth,
                created_ids,
                result_bodies,
                box_count,
                NULL) ||
            !ObjectFaceExtrude_ReplayAuthoringToLayout(state)) {
            (void)ObjectAuthoringSession_Copy(&state->objectAuthoring, &prior_session);
            ObjectAuthoringSession_Free(&prior_session);
            return false;
        }
        (void)ObjectAuthoringSession_ClearActiveSketch(&state->objectAuthoring);
        ObjectAuthoringSession_Free(&prior_session);
    } else {
        (void)Layout_ObjectStore_Delete(&state->layout.objectStore, target_body_id);
    }
    editor->selectedObject3DId = box_count > 0u ? created_ids[0] : 0u;
    editor->selectedObjectAssetBodyId = box_count > 0u ? created_ids[0] : 0u;
    editor->selectedObjectAssetFace = OBJECT3D_FACE_NONE;
    Editor_ObjectFaceSketchClear(editor);
    Editor_ObjectFaceExtrudeClear(editor);
    Global_FlagLayoutChanged();
    Global_FlagHitboxesDirty();
    return true;
}

void Editor_ObjectFaceExtrudeClear(EditorState* editor) {
    if (!editor) return;
    editor->objectFaceExtrudeToolArmed = false;
    editor->objectFaceExtrudeDragging = false;
    editor->objectFaceExtrudeHasPreview = false;
    editor->objectFaceExtrudeMode = OBJECT_FACE_EXTRUDE_MODE_NONE;
    editor->objectFaceExtrudeBodyId = 0u;
    editor->objectFaceExtrudeFace = OBJECT3D_FACE_NONE;
    editor->objectFaceExtrudeFrame = (PlaneFrame3){0};
    editor->objectFaceExtrudeStartScreen = (Vec2){0.0f, 0.0f};
    editor->objectFaceExtrudeDepth = 0.0f;
    if (editor->objectFaceSketchHasRectangle) {
        (void)Editor_ObjectFaceSketchSelect(editor, editor->selectedObjectFaceSketchHandle);
    } else {
        editor->objectAuthoringMode = Editor_ObjectAuthoringIdleMode(editor);
    }
}

bool Editor_ObjectFaceExtrudeArm(GlobalState* state, ObjectFaceExtrudeMode mode) {
    EditorState* editor = NULL;
    if (!state) return false;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return false;
    editor = &state->editor;
    if (!ObjectFaceExtrude_HasSketchTarget(editor)) return false;
    if (mode != OBJECT_FACE_EXTRUDE_MODE_ADD && mode != OBJECT_FACE_EXTRUDE_MODE_CUT) return false;
    if (mode == OBJECT_FACE_EXTRUDE_MODE_CUT) {
        const Object3D* target =
            Layout_ObjectStore_FindConst(&state->layout.objectStore, editor->objectFaceSketchBodyId);
        if (!target || target->kind != OBJECT3D_KIND_RECT_PRISM) return false;
    }

    Editor_ObjectFaceExtrudeClear(editor);
    editor->objectFaceExtrudeToolArmed = true;
    editor->objectAuthoringMode = OBJECT_AUTHORING_MODE_OPERATION_PREVIEW;
    editor->objectFaceExtrudeMode = mode;
    editor->objectFaceExtrudeBodyId = editor->objectFaceSketchBodyId;
    editor->objectFaceExtrudeFace = editor->objectFaceSketchFace;
    editor->objectFaceExtrudeFrame = editor->objectFaceSketchFrame;
    editor->objectFaceExtrudeDepth = ObjectFaceExtrude_DefaultDepth(state);
    editor->objectFaceExtrudeHasPreview =
        editor->objectFaceExtrudeDepth > kObjectFaceExtrudePreviewDepth;
    editor->selectedObject3DId = editor->objectFaceSketchBodyId;
    editor->selectedObjectAssetBodyId = editor->objectFaceSketchBodyId;
    editor->selectedObjectAssetFace = editor->objectFaceSketchFace;
    return true;
}

bool Editor_ObjectFaceExtrudeTrigger(GlobalState* state, ObjectFaceExtrudeMode mode) {
    EditorState* editor = NULL;
    bool same_active_mode = false;
    bool committed = false;

    if (!state) return false;
    editor = &state->editor;
    same_active_mode =
        editor->objectFaceExtrudeToolArmed &&
        !editor->objectFaceExtrudeDragging &&
        editor->objectFaceExtrudeMode == mode &&
        editor->objectFaceExtrudeDepth > kObjectFaceExtrudeMinDepth;

    if (same_active_mode) {
        if (mode == OBJECT_FACE_EXTRUDE_MODE_ADD) {
            committed = ObjectFaceExtrude_CommitAdd(state);
        } else if (mode == OBJECT_FACE_EXTRUDE_MODE_CUT) {
            committed = ObjectFaceExtrude_CommitCut(state);
        }
        if (!committed) {
            editor->objectFaceExtrudeHasPreview = false;
            editor->objectFaceExtrudeDepth = 0.0f;
        }
        return committed;
    }

    return Editor_ObjectFaceExtrudeArm(state, mode);
}

bool Editor_ObjectFaceExtrudeSetDepth(GlobalState* state, float depth) {
    EditorState* editor = NULL;
    if (!state) return false;
    editor = &state->editor;
    if (!editor->objectFaceExtrudeToolArmed) return false;
    if (editor->objectFaceExtrudeMode != OBJECT_FACE_EXTRUDE_MODE_ADD &&
        editor->objectFaceExtrudeMode != OBJECT_FACE_EXTRUDE_MODE_CUT) {
        return false;
    }
    if (!isfinite(depth) || depth <= kObjectFaceExtrudeMinDepth) return false;

    editor->objectFaceExtrudeDepth = depth;
    editor->objectFaceExtrudeHasPreview = depth > kObjectFaceExtrudePreviewDepth;
    editor->objectAuthoringMode = OBJECT_AUTHORING_MODE_OPERATION_PREVIEW;
    Global_FlagHitboxesDirty();
    return true;
}

bool Editor_ObjectFaceExtrudeAdjustDepth(GlobalState* state, float delta_depth) {
    EditorState* editor = NULL;
    float next_depth = 0.0f;
    if (!state || !isfinite(delta_depth)) return false;
    editor = &state->editor;
    if (!editor->objectFaceExtrudeToolArmed) return false;

    next_depth = editor->objectFaceExtrudeDepth + delta_depth;
    if (next_depth <= kObjectFaceExtrudeMinDepth) {
        next_depth = kObjectFaceExtrudeMinDepth * 2.0f;
    }
    return Editor_ObjectFaceExtrudeSetDepth(state, next_depth);
}

bool Editor_ObjectFaceExtrudeHandleLeftMouseDown(GlobalState* state, int mouse_x, int mouse_y) {
    EditorState* editor = NULL;
    if (!state) return false;
    editor = &state->editor;
    if (!editor->objectFaceExtrudeToolArmed) return false;
    editor->objectFaceExtrudeDragging = true;
    editor->objectAuthoringMode = OBJECT_AUTHORING_MODE_OPERATION_PREVIEW;
    editor->objectFaceExtrudeHasPreview = false;
    editor->objectFaceExtrudeStartScreen = (Vec2){ (float)mouse_x, (float)mouse_y };
    editor->objectFaceExtrudeDepth = 0.0f;
    return true;
}

void Editor_ObjectFaceExtrudeHandleMouseMotion(GlobalState* state, int mouse_x, int mouse_y) {
    EditorState* editor = NULL;
    if (!state) return;
    editor = &state->editor;
    if (!editor->objectFaceExtrudeDragging) return;
    editor->objectFaceExtrudeDepth = ObjectFaceExtrude_ResolveDepthFromMouse(state,
                                                                              editor,
                                                                              mouse_x,
                                                                              mouse_y);
    editor->objectFaceExtrudeHasPreview =
        editor->objectFaceExtrudeDepth > kObjectFaceExtrudePreviewDepth;
}

void Editor_ObjectFaceExtrudeHandleLeftMouseUp(GlobalState* state, int mouse_x, int mouse_y) {
    EditorState* editor = NULL;
    bool committed = false;
    if (!state) return;
    editor = &state->editor;
    if (!editor->objectFaceExtrudeDragging) return;

    Editor_ObjectFaceExtrudeHandleMouseMotion(state, mouse_x, mouse_y);
    editor->objectFaceExtrudeDragging = false;
    if (editor->objectFaceExtrudeDepth <= kObjectFaceExtrudeMinDepth) {
        editor->objectFaceExtrudeHasPreview = false;
        return;
    }

    if (editor->objectFaceExtrudeMode == OBJECT_FACE_EXTRUDE_MODE_ADD) {
        committed = ObjectFaceExtrude_CommitAdd(state);
    } else if (editor->objectFaceExtrudeMode == OBJECT_FACE_EXTRUDE_MODE_CUT) {
        committed = ObjectFaceExtrude_CommitCut(state);
    }

    if (!committed) {
        editor->objectFaceExtrudeHasPreview = false;
        editor->objectFaceExtrudeDepth = 0.0f;
    }
}

static void ObjectFaceExtrude_BuildPreviewCorners(const PlaneFrame3* frame,
                                                  float width,
                                                  float height,
                                                  float depth,
                                                  Vec3 out_corners[8]) {
    const Vec3 center = frame->origin;
    const Vec3 axis_u = Vec3_Normalize(frame->axisU);
    const Vec3 axis_v = Vec3_Normalize(frame->axisV);
    const Vec3 axis_n = Vec3_Normalize(frame->normal);
    const Vec3 u = Vec3_Scale(axis_u, width * 0.5f);
    const Vec3 v = Vec3_Scale(axis_v, height * 0.5f);
    const Vec3 n = Vec3_Scale(axis_n, depth * 0.5f);

    out_corners[0] = Vec3_Add(center, Vec3_Add(Vec3_Add(Vec3_Scale(u, -1.0f), Vec3_Scale(v, -1.0f)), Vec3_Scale(n, -1.0f)));
    out_corners[1] = Vec3_Add(center, Vec3_Add(Vec3_Add(u, Vec3_Scale(v, -1.0f)), Vec3_Scale(n, -1.0f)));
    out_corners[2] = Vec3_Add(center, Vec3_Add(Vec3_Add(u, v), Vec3_Scale(n, -1.0f)));
    out_corners[3] = Vec3_Add(center, Vec3_Add(Vec3_Add(Vec3_Scale(u, -1.0f), v), Vec3_Scale(n, -1.0f)));
    out_corners[4] = Vec3_Add(center, Vec3_Add(Vec3_Add(Vec3_Scale(u, -1.0f), Vec3_Scale(v, -1.0f)), n));
    out_corners[5] = Vec3_Add(center, Vec3_Add(Vec3_Add(u, Vec3_Scale(v, -1.0f)), n));
    out_corners[6] = Vec3_Add(center, Vec3_Add(Vec3_Add(u, v), n));
    out_corners[7] = Vec3_Add(center, Vec3_Add(Vec3_Add(Vec3_Scale(u, -1.0f), v), n));
}

static void ObjectFaceExtrude_DrawPreviewPrism(SDL_Renderer* renderer,
                                               const Grid* grid,
                                               const SpaceViewContext* view_ctx,
                                               const Vec3 corners[8],
                                               SDL_Color color) {
    static const int kEdges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    Vec2 pts[8];

    if (!renderer || !grid || !view_ctx || !corners) return;
    for (int i = 0; i < 8; ++i) {
        pts[i] = WorldToScreen(SpaceAdapter_ProjectToView(corners[i], view_ctx), grid);
    }

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int edge_index = 0; edge_index < 12; ++edge_index) {
        const Vec2 a = pts[kEdges[edge_index][0]];
        const Vec2 b = pts[kEdges[edge_index][1]];
        SDL_RenderDrawLine(renderer,
                           (int)lroundf(a.x), (int)lroundf(a.y),
                           (int)lroundf(b.x), (int)lroundf(b.y));
    }
}

void Render_EditorObjectFaceExtrude(EditorState* editor, AppContext* ctx) {
    GlobalState* state = Global_Get();
    Vec2 min_uv = {0};
    Vec2 max_uv = {0};
    PlaneFrame3 preview_frame = {0};
    Vec3 preview_corners[8];
    SpaceViewContext view_ctx = {0};
    float width = 0.0f;
    float height = 0.0f;
    float depth = editor->objectFaceExtrudeDepth;
    SDL_Color preview_color = {0};

    if (!editor || !ctx || !state) return;
    if (!editor->objectFaceExtrudeHasPreview && !editor->objectFaceExtrudeDragging) return;
    if (!ObjectFaceExtrude_HasSketchTarget(editor)) return;
    if (editor->objectFaceExtrudeDepth <= kObjectFaceExtrudePreviewDepth) return;

    min_uv = ObjectFaceExtrude_SketchMinUV(editor);
    max_uv = ObjectFaceExtrude_SketchMaxUV(editor);
    width = max_uv.x - min_uv.x;
    height = max_uv.y - min_uv.y;
    if (width <= kObjectFaceExtrudeMinDepth || height <= kObjectFaceExtrudeMinDepth) return;

    if (editor->objectFaceExtrudeMode == OBJECT_FACE_EXTRUDE_MODE_ADD) {
        const Vec3 axis_u = Vec3_Normalize(editor->objectFaceSketchFrame.axisU);
        const Vec3 axis_v = Vec3_Normalize(editor->objectFaceSketchFrame.axisV);
        const Vec3 axis_n = Vec3_Normalize(editor->objectFaceSketchFrame.normal);
        preview_frame = editor->objectFaceSketchFrame;
        preview_frame.origin = Vec3_Add(editor->objectFaceSketchFrame.origin,
                                        Vec3_Add(Vec3_Scale(axis_u, 0.5f * (min_uv.x + max_uv.x)),
                                                 Vec3_Add(Vec3_Scale(axis_v, 0.5f * (min_uv.y + max_uv.y)),
                                                          Vec3_Scale(axis_n, 0.5f * editor->objectFaceExtrudeDepth))));
        preview_frame.axisU = axis_u;
        preview_frame.axisV = axis_v;
        preview_frame.normal = axis_n;
        preview_color = (SDL_Color){255, 196, 88, 235};
    } else {
        FaceLocalPrismBox cut_box = {
            .u0 = min_uv.x, .u1 = max_uv.x,
            .v0 = min_uv.y, .v1 = max_uv.y,
            .w0 = 0.0f, .w1 = editor->objectFaceExtrudeDepth
        };
        if (!ObjectFaceExtrude_BoxToWorldFrame(&editor->objectFaceSketchFrame,
                                               &cut_box,
                                               &preview_frame,
                                               &width,
                                               &height,
                                               &depth)) {
            return;
        }
        preview_color = (SDL_Color){255, 104, 104, 235};
    }

    ObjectFaceExtrude_BuildPreviewCorners(&preview_frame,
                                          width,
                                          height,
                                          depth,
                                          preview_corners);
    view_ctx = SpaceAdapter_BuildViewContext(state);
    ObjectFaceExtrude_DrawPreviewPrism(ctx->renderer,
                                       &state->grid,
                                       &view_ctx,
                                       preview_corners,
                                       preview_color);
}
