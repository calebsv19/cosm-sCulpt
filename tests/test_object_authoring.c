#include "test_layout_internal.h"

#include "Core/workspace/line_drawing_object_workspace_view.h"
#include "Editor/object_face_extrude.h"
#include "Editor/object_face_sketch.h"
#include "Editor/object_handle_gizmo.h"
#include "Input/input_mouse.h"
#include "Input/input_mouse_internal.h"
#include "Input/input_viewport_pick.h"
#include "Layout/asset/layout_object_asset_mesh_authoring.h"
#include "ObjectAuthoring/object_authoring_eval.h"
#include "ObjectAuthoring/object_authoring_mesh_compile.h"
#include "ObjectAuthoring/object_authoring_session.h"
#include "UI/ui_panel.h"

#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

static bool seed_object_authoring_state(GlobalState** out_state) {
    GlobalState* state = NULL;
    RectPrismPrimitiveCreateParams params;
    uint32_t object_id = 0u;
    bool adjusted = false;

    if (out_state) *out_state = NULL;
    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    params = (RectPrismPrimitiveCreateParams){
        .width = 4.0f,
        .height = 6.0f,
        .depth = 8.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 0.0f, 0.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = false,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&state->layout,
                                                &params,
                                                &object_id,
                                                &adjusted));
    state->editor.selectedObject3DId = object_id;
    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));
    TEST_ASSERT(LineDrawingObjectWorkspaceView_FocusFace(state,
                                                        object_id,
                                                        OBJECT3D_FACE_RECT_PRISM_POS_N));
    if (out_state) *out_state = state;
    return true;
}

static void shutdown_object_authoring_state(void) {
    ld_test_shutdown_runtime();
}

static void object_authoring_sketch_screen_point(const GlobalState* state,
                                                 const ObjectAuthoringSketch* sketch,
                                                 float u,
                                                 float v,
                                                 int* out_x,
                                                 int* out_y) {
    SpaceViewContext view_ctx = {0};
    const Vec3 axis_u = Vec3_Normalize(sketch->frame.axisU);
    const Vec3 axis_v = Vec3_Normalize(sketch->frame.axisV);
    const Vec3 world = Vec3_Add(sketch->frame.origin,
                                Vec3_Add(Vec3_Scale(axis_u, u),
                                         Vec3_Scale(axis_v, v)));
    Vec2 screen = {0};
    view_ctx = SpaceAdapter_BuildViewContext((GlobalState*)state);
    screen = WorldToScreen(SpaceAdapter_ProjectToView(world, &view_ctx), &state->grid);
    if (out_x) *out_x = (int)lroundf(screen.x);
    if (out_y) *out_y = (int)lroundf(screen.y);
}

static void object_authoring_world_screen_point(const GlobalState* state,
                                                Vec3 world,
                                                int* out_x,
                                                int* out_y) {
    SpaceViewContext view_ctx = SpaceAdapter_BuildViewContext((GlobalState*)state);
    Vec2 screen = WorldToScreen(SpaceAdapter_ProjectToView(world, &view_ctx), &state->grid);
    if (out_x) *out_x = (int)lroundf(screen.x);
    if (out_y) *out_y = (int)lroundf(screen.y);
}

static void object_authoring_click_left(int x, int y) {
    SDL_Event click = {0};
    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.type = SDL_MOUSEBUTTONDOWN;
    click.button.button = SDL_BUTTON_LEFT;
    click.button.x = x;
    click.button.y = y;
    click.button.clicks = 1;
    Input_MouseHandle(NULL, &click);
}

static bool test_object_authoring_session_mirrors_object_workspace_bodies(void) {
    GlobalState* state = NULL;
    const ObjectAuthoringFace* selected_face = NULL;

    TEST_ASSERT(seed_object_authoring_state(&state));
    TEST_ASSERT(state->objectAuthoring.attached);
    TEST_ASSERT(state->objectAuthoring.document.bodyCount ==
                Layout_ObjectStore_LiveCount(&state->layout.objectStore));
    TEST_ASSERT(state->objectAuthoring.document.faceCount == 6u);
    TEST_ASSERT(state->objectAuthoring.document.selectedFace.bodyId ==
                state->editor.selectedObjectAssetBodyId);
    TEST_ASSERT(state->objectAuthoring.document.selectedFace.primitiveFace ==
                state->editor.selectedObjectAssetFace);
    TEST_ASSERT(state->objectAuthoring.document.selectedFace.faceId ==
                ObjectAuthoringFaceId_FromPrimitive(
                    state->editor.selectedObjectAssetBodyId,
                    state->editor.selectedObjectAssetFace));
    selected_face = ObjectAuthoringDocument_FindFace(
        &state->objectAuthoring.document,
        state->objectAuthoring.document.selectedFace.faceId);
    TEST_ASSERT(selected_face != NULL);
    TEST_ASSERT(selected_face->ref.bodyId == state->editor.selectedObjectAssetBodyId);
    TEST_ASSERT(selected_face->ref.primitiveFace == state->editor.selectedObjectAssetFace);
    TEST_ASSERT(state->objectAuthoring.document.operationCount == 1u);
    TEST_ASSERT(state->objectAuthoring.document.operations[0].kind ==
                OBJECT_AUTHORING_OPERATION_CREATE_PRIMITIVE);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_face_ref_matching_prefers_stable_ids(void) {
    ObjectAuthoringFaceRef selected =
        ObjectAuthoringFaceRef_FromPrimitive(7u, OBJECT3D_FACE_RECT_PRISM_POS_N);
    ObjectAuthoringFaceRef stale_label = selected;
    ObjectAuthoringFaceRef legacy =
        ObjectAuthoringFaceRef_FromPrimitive(7u, OBJECT3D_FACE_RECT_PRISM_POS_N);
    ObjectAuthoringFaceRef different =
        ObjectAuthoringFaceRef_FromPrimitive(7u, OBJECT3D_FACE_RECT_PRISM_NEG_N);
    ObjectAuthoringFaceRef empty =
        ObjectAuthoringFaceRef_FromPrimitive(0u, OBJECT3D_FACE_NONE);

    stale_label.primitiveFace = OBJECT3D_FACE_RECT_PRISM_NEG_N;
    legacy.faceId = 0u;

    TEST_ASSERT(ObjectAuthoringFaceRef_IsSet(selected));
    TEST_ASSERT(!ObjectAuthoringFaceRef_IsSet(empty));
    TEST_ASSERT(ObjectAuthoringFaceRef_Matches(selected, stale_label));
    TEST_ASSERT(ObjectAuthoringFaceRef_Matches(selected, legacy));
    TEST_ASSERT(!ObjectAuthoringFaceRef_Matches(selected, different));
    TEST_ASSERT(!ObjectAuthoringFaceRef_Matches(selected, empty));
    return true;
}

static bool test_object_authoring_face_ref_diagnostics_classify_invalid_refs(void) {
    GlobalState* state = NULL;
    ObjectAuthoringFaceRef valid;
    ObjectAuthoringFaceRef stale_adapter;
    ObjectAuthoringFaceRef missing_body;
    ObjectAuthoringFaceRef missing_face;

    TEST_ASSERT(seed_object_authoring_state(&state));
    valid = state->objectAuthoring.document.selectedFace;
    stale_adapter = valid;
    stale_adapter.primitiveFace = OBJECT3D_FACE_RECT_PRISM_NEG_N;
    missing_body = valid;
    missing_body.bodyId = 9999u;
    missing_body.faceId = ObjectAuthoringFaceId_FromPrimitive(
        missing_body.bodyId,
        missing_body.primitiveFace);
    missing_face = valid;
    missing_face.faceId = 9999u;

    TEST_ASSERT(ObjectAuthoringDocument_CheckFaceRef(&state->objectAuthoring.document,
                                                     valid) ==
                OBJECT_AUTHORING_FACE_REF_STATUS_OK);
    TEST_ASSERT(ObjectAuthoringDocument_CheckFaceRef(&state->objectAuthoring.document,
                                                     stale_adapter) ==
                OBJECT_AUTHORING_FACE_REF_STATUS_STALE_ADAPTER);
    TEST_ASSERT(ObjectAuthoringDocument_CheckFaceRef(&state->objectAuthoring.document,
                                                     missing_body) ==
                OBJECT_AUTHORING_FACE_REF_STATUS_MISSING_BODY);
    TEST_ASSERT(ObjectAuthoringDocument_CheckFaceRef(&state->objectAuthoring.document,
                                                     missing_face) ==
                OBJECT_AUTHORING_FACE_REF_STATUS_MISSING_FACE);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_rebuilds_stable_vertex_edge_topology(void) {
    GlobalState* state = NULL;
    const ObjectAuthoringDocument* doc = NULL;
    const ObjectAuthoringFace* pos_face = NULL;
    const ObjectAuthoringEdge* bottom_edge = NULL;

    TEST_ASSERT(seed_object_authoring_state(&state));
    doc = &state->objectAuthoring.document;
    TEST_ASSERT(doc->vertexCount == 8u);
    TEST_ASSERT(doc->edgeCount == 12u);
    TEST_ASSERT(doc->faceCount == 6u);
    TEST_ASSERT(doc->vertices[0].vertexId ==
                ObjectAuthoringVertexId_FromPrimitive(
                    state->editor.selectedObjectAssetBodyId,
                    0u));
    TEST_ASSERT(doc->edges[0].edgeId ==
                ObjectAuthoringEdgeId_FromPrimitive(
                    state->editor.selectedObjectAssetBodyId,
                    0u));
    TEST_ASSERT(ld_test_nearly_equal(doc->vertices[0].position.x, -2.0f));
    TEST_ASSERT(ld_test_nearly_equal(doc->vertices[0].position.y, -3.0f));
    TEST_ASSERT(ld_test_nearly_equal(doc->vertices[0].position.z, -4.0f));

    pos_face = ObjectAuthoringDocument_FindFace(
        doc,
        ObjectAuthoringFaceId_FromPrimitive(state->editor.selectedObjectAssetBodyId,
                                            OBJECT3D_FACE_RECT_PRISM_POS_N));
    TEST_ASSERT(pos_face != NULL);
    TEST_ASSERT(pos_face->vertexCount == 4u);
    TEST_ASSERT(pos_face->edgeCount == 4u);
    TEST_ASSERT(pos_face->vertexIds[0] ==
                ObjectAuthoringVertexId_FromPrimitive(
                    state->editor.selectedObjectAssetBodyId,
                    4u));
    TEST_ASSERT(pos_face->edgeIds[0] ==
                ObjectAuthoringEdgeId_FromPrimitive(
                    state->editor.selectedObjectAssetBodyId,
                    4u));

    bottom_edge = ObjectAuthoringDocument_FindEdge(
        doc,
        ObjectAuthoringEdgeId_FromPrimitive(state->editor.selectedObjectAssetBodyId,
                                            0u));
    TEST_ASSERT(bottom_edge != NULL);
    TEST_ASSERT(bottom_edge->vertexIds[0] ==
                ObjectAuthoringVertexId_FromPrimitive(
                    state->editor.selectedObjectAssetBodyId,
                    0u));
    TEST_ASSERT(bottom_edge->vertexIds[1] ==
                ObjectAuthoringVertexId_FromPrimitive(
                    state->editor.selectedObjectAssetBodyId,
                    1u));
    TEST_ASSERT(bottom_edge->faceCount == 2u);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_selects_vertex_and_edge_topology_refs(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument* doc = NULL;
    ObjectAuthoringBodyId body_id = 0u;

    TEST_ASSERT(seed_object_authoring_state(&state));
    doc = &state->objectAuthoring.document;
    body_id = state->editor.selectedObjectAssetBodyId;

    TEST_ASSERT(ObjectAuthoringDocument_SetVertexSelection(doc, body_id, 4u));
    TEST_ASSERT(doc->selectionKind == OBJECT_AUTHORING_SELECTION_VERTEX);
    TEST_ASSERT(doc->selectedVertex.vertexId ==
                ObjectAuthoringVertexId_FromPrimitive(body_id, 4u));
    TEST_ASSERT(doc->selectedVertex.primitiveVertexIndex == 4u);
    TEST_ASSERT(doc->selectedFace.primitiveFace == OBJECT3D_FACE_NONE);
    TEST_ASSERT(doc->selectedEdge.edgeId == 0u);

    TEST_ASSERT(ObjectAuthoringDocument_SetEdgeSelection(doc, body_id, 9u));
    TEST_ASSERT(doc->selectionKind == OBJECT_AUTHORING_SELECTION_EDGE);
    TEST_ASSERT(doc->selectedEdge.edgeId ==
                ObjectAuthoringEdgeId_FromPrimitive(body_id, 9u));
    TEST_ASSERT(doc->selectedEdge.primitiveEdgeIndex == 9u);
    TEST_ASSERT(doc->selectedVertex.vertexId == 0u);

    TEST_ASSERT(!ObjectAuthoringDocument_SetVertexSelection(doc, body_id, 99u));
    TEST_ASSERT(!ObjectAuthoringDocument_SetEdgeSelection(doc, body_id, 99u));

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_topology_hitboxes_precede_object_body(void) {
    GlobalState* state = NULL;
    const ObjectAuthoringDocument* doc = NULL;
    const ObjectAuthoringVertex* vertex = NULL;
    const ObjectAuthoringEdge* edge = NULL;
    const ObjectAuthoringVertex* edge_a = NULL;
    const ObjectAuthoringVertex* edge_b = NULL;
    Hitbox hit = {0};
    Vec3 midpoint = {0};
    uint32_t body_id = 0u;
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    doc = &state->objectAuthoring.document;
    body_id = state->editor.selectedObjectAssetBodyId;

    vertex = ObjectAuthoringDocument_FindVertex(
        doc,
        ObjectAuthoringVertexId_FromPrimitive(body_id, 4u));
    TEST_ASSERT(vertex != NULL);
    object_authoring_world_screen_point(state, vertex->position, &x, &y);
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();
    hit = HitboxSystem_GetHitAt(x, y);
    TEST_ASSERT(hit.type == HITBOX_OBJECT_TOPOLOGY_VERTEX);
    TEST_ASSERT(hit.index == (int)body_id);
    TEST_ASSERT(hit.subIndex == 4);

    edge = ObjectAuthoringDocument_FindEdge(
        doc,
        ObjectAuthoringEdgeId_FromPrimitive(body_id, 4u));
    TEST_ASSERT(edge != NULL);
    edge_a = ObjectAuthoringDocument_FindVertex(doc, edge->vertexIds[0]);
    edge_b = ObjectAuthoringDocument_FindVertex(doc, edge->vertexIds[1]);
    TEST_ASSERT(edge_a != NULL);
    TEST_ASSERT(edge_b != NULL);
    midpoint = Vec3_Scale(Vec3_Add(edge_a->position, edge_b->position), 0.5f);
    object_authoring_world_screen_point(state, midpoint, &x, &y);
    TEST_ASSERT(ResolvePointerPaneLane(x, y) == POINTER_PANE_CENTER);
    hit = HitboxSystem_GetHitAt(x, y);
    TEST_ASSERT(hit.type == HITBOX_OBJECT_TOPOLOGY_EDGE);
    TEST_ASSERT(hit.index == (int)body_id);
    TEST_ASSERT(hit.subIndex == 4);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_topology_selection_resolves_gizmo_targets(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument* doc = NULL;
    const Object3D* object = NULL;
    ObjectHandleGizmoTarget target = ObjectHandleGizmoTarget_None();
    ObjectAuthoringBodyId body_id = 0u;

    TEST_ASSERT(seed_object_authoring_state(&state));
    doc = &state->objectAuthoring.document;
    body_id = state->editor.selectedObjectAssetBodyId;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore, body_id);
    TEST_ASSERT(object != NULL);

    TEST_ASSERT(ObjectAuthoringDocument_SetVertexSelection(doc, body_id, 4u));
    TEST_ASSERT(ObjectHandleGizmoTarget_FromAuthoringSelection(object, doc, &target));
    TEST_ASSERT(target.kind == OBJECT_HANDLE_GIZMO_TARGET_TOPOLOGY_VERTEX);
    TEST_ASSERT(target.objectId == body_id);
    TEST_ASSERT(target.vertexRef.vertexId == doc->selectedVertex.vertexId);
    TEST_ASSERT(ObjectHandleGizmoTarget_AxisAllowed(&target, RECT_PRISM_AXIS_DIR_POS_U));
    TEST_ASSERT(ObjectHandleGizmoTarget_AxisAllowed(&target, RECT_PRISM_AXIS_DIR_POS_V));
    TEST_ASSERT(ObjectHandleGizmoTarget_AxisAllowed(&target, RECT_PRISM_AXIS_DIR_POS_N));
    TEST_ASSERT(!ObjectHandleGizmoTarget_CanMutate(&target));

    TEST_ASSERT(ObjectAuthoringDocument_SetEdgeSelection(doc, body_id, 9u));
    TEST_ASSERT(ObjectHandleGizmoTarget_FromAuthoringSelection(object, doc, &target));
    TEST_ASSERT(target.kind == OBJECT_HANDLE_GIZMO_TARGET_TOPOLOGY_EDGE);
    TEST_ASSERT(target.objectId == body_id);
    TEST_ASSERT(target.edgeRef.edgeId == doc->selectedEdge.edgeId);
    TEST_ASSERT(ObjectHandleGizmoTarget_AxisAllowed(&target, RECT_PRISM_AXIS_DIR_NEG_U));
    TEST_ASSERT(ObjectHandleGizmoTarget_AxisAllowed(&target, RECT_PRISM_AXIS_DIR_NEG_V));
    TEST_ASSERT(ObjectHandleGizmoTarget_AxisAllowed(&target, RECT_PRISM_AXIS_DIR_NEG_N));
    TEST_ASSERT(!ObjectHandleGizmoTarget_CanMutate(&target));

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_topology_selection_emits_gizmo_axis_hitboxes(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument* doc = NULL;
    const Object3D* object = NULL;
    const ObjectAuthoringVertex* vertex = NULL;
    Hitbox hit = {0};
    Vec3 axis = {0};
    Vec3 tip = {0};
    Vec2 tip_screen = {0};
    ObjectAuthoringBodyId body_id = 0u;
    float axis_world_len = 0.0f;

    TEST_ASSERT(seed_object_authoring_state(&state));
    doc = &state->objectAuthoring.document;
    body_id = state->editor.selectedObjectAssetBodyId;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore, body_id);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(ObjectAuthoringDocument_SetVertexSelection(doc, body_id, 4u));
    vertex = ObjectAuthoringDocument_FindVertex(doc, doc->selectedVertex.vertexId);
    TEST_ASSERT(vertex != NULL);

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = object->transform.position;
    state->editor.selectedObject3DId = body_id;
    state->editor.selectedObjectAssetFace = OBJECT3D_FACE_NONE;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    axis_world_len = fmaxf(state->layout.gridSize * 2.0f, 1.0f);

    axis = Layout_RectPrismAxisDirection_WorldVector(object, RECT_PRISM_AXIS_DIR_POS_U);
    tip = Vec3_Add(vertex->position, Vec3_Scale(axis, axis_world_len));
    tip_screen = WorldToScreen(Vec3_ProjectToView(tip,
                                                  state->activePlane,
                                                  &state->freeViewCamera),
                               &state->grid);
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();
    hit = HitboxSystem_GetHitAt((int)tip_screen.x, (int)tip_screen.y);
    TEST_ASSERT(hit.type == HITBOX_OBJECT3D_GIZMO_AXIS);
    TEST_ASSERT(hit.index == (int)body_id);
    TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_POS_U);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_edge_mode_hover_targets_edge_not_face(void) {
    GlobalState* state = NULL;
    const ObjectAuthoringDocument* doc = NULL;
    const ObjectAuthoringEdge* edge = NULL;
    const ObjectAuthoringVertex* edge_a = NULL;
    const ObjectAuthoringVertex* edge_b = NULL;
    Vec3 midpoint = {0};
    uint32_t body_id = 0u;
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    doc = &state->objectAuthoring.document;
    body_id = state->editor.selectedObjectAssetBodyId;
    edge = ObjectAuthoringDocument_FindEdge(
        doc,
        ObjectAuthoringEdgeId_FromPrimitive(body_id, 4u));
    TEST_ASSERT(edge != NULL);
    edge_a = ObjectAuthoringDocument_FindVertex(doc, edge->vertexIds[0]);
    edge_b = ObjectAuthoringDocument_FindVertex(doc, edge->vertexIds[1]);
    TEST_ASSERT(edge_a != NULL);
    TEST_ASSERT(edge_b != NULL);
    midpoint = Vec3_Scale(Vec3_Add(edge_a->position, edge_b->position), 0.5f);
    object_authoring_world_screen_point(state, midpoint, &x, &y);

    state->editor.objectEditSelectionMode = OBJECT_EDIT_SELECTION_EDGE;
    Global_FlagHitboxesDirty();
    UpdateHover(x, y);
    TEST_ASSERT(state->editor.hoveredObjectTopologyBodyId == body_id);
    TEST_ASSERT(state->editor.hoveredObjectTopologyEdgeIndex == 4);
    TEST_ASSERT(state->editor.hoveredObjectTopologyVertexIndex == -1);
    TEST_ASSERT(state->editor.hoveredObjectAssetFace == OBJECT3D_FACE_NONE);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_vertex_mode_click_selects_vertex_and_gizmo_target(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument* doc = NULL;
    const Object3D* object = NULL;
    const ObjectAuthoringVertex* vertex = NULL;
    ObjectHandleGizmoTarget target = ObjectHandleGizmoTarget_None();
    uint32_t body_id = 0u;
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    doc = &state->objectAuthoring.document;
    body_id = state->editor.selectedObjectAssetBodyId;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore, body_id);
    TEST_ASSERT(object != NULL);
    vertex = ObjectAuthoringDocument_FindVertex(
        doc,
        ObjectAuthoringVertexId_FromPrimitive(body_id, 4u));
    TEST_ASSERT(vertex != NULL);
    object_authoring_world_screen_point(state, vertex->position, &x, &y);
    TEST_ASSERT(ResolvePointerPaneLane(x, y) == POINTER_PANE_CENTER);

    state->editor.objectEditSelectionMode = OBJECT_EDIT_SELECTION_VERTEX;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    Global_FlagHitboxesDirty();
    object_authoring_click_left(x, y);

    TEST_ASSERT(state->editor.selectedObject3DId == body_id);
    TEST_ASSERT(state->editor.selectedObjectAssetBodyId == body_id);
    TEST_ASSERT(state->editor.selectedObjectAssetFace == OBJECT3D_FACE_NONE);
    TEST_ASSERT(doc->selectionKind == OBJECT_AUTHORING_SELECTION_VERTEX);
    TEST_ASSERT(doc->selectedVertex.bodyId == body_id);
    TEST_ASSERT(doc->selectedVertex.primitiveVertexIndex == 4u);
    TEST_ASSERT(ObjectHandleGizmoTarget_FromAuthoringSelection(object, doc, &target));
    TEST_ASSERT(target.kind == OBJECT_HANDLE_GIZMO_TARGET_TOPOLOGY_VERTEX);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_edge_mode_click_selects_edge_and_gizmo_target(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument* doc = NULL;
    const Object3D* object = NULL;
    const ObjectAuthoringEdge* edge = NULL;
    const ObjectAuthoringVertex* edge_a = NULL;
    const ObjectAuthoringVertex* edge_b = NULL;
    ObjectHandleGizmoTarget target = ObjectHandleGizmoTarget_None();
    Vec3 midpoint = {0};
    uint32_t body_id = 0u;
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    doc = &state->objectAuthoring.document;
    body_id = state->editor.selectedObjectAssetBodyId;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore, body_id);
    TEST_ASSERT(object != NULL);
    edge = ObjectAuthoringDocument_FindEdge(
        doc,
        ObjectAuthoringEdgeId_FromPrimitive(body_id, 4u));
    TEST_ASSERT(edge != NULL);
    edge_a = ObjectAuthoringDocument_FindVertex(doc, edge->vertexIds[0]);
    edge_b = ObjectAuthoringDocument_FindVertex(doc, edge->vertexIds[1]);
    TEST_ASSERT(edge_a != NULL);
    TEST_ASSERT(edge_b != NULL);
    midpoint = Vec3_Scale(Vec3_Add(edge_a->position, edge_b->position), 0.5f);
    object_authoring_world_screen_point(state, midpoint, &x, &y);

    state->editor.objectEditSelectionMode = OBJECT_EDIT_SELECTION_EDGE;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    Global_FlagHitboxesDirty();
    object_authoring_click_left(x, y);

    TEST_ASSERT(state->editor.selectedObject3DId == body_id);
    TEST_ASSERT(state->editor.selectedObjectAssetBodyId == body_id);
    TEST_ASSERT(state->editor.selectedObjectAssetFace == OBJECT3D_FACE_NONE);
    TEST_ASSERT(doc->selectionKind == OBJECT_AUTHORING_SELECTION_EDGE);
    TEST_ASSERT(doc->selectedEdge.bodyId == body_id);
    TEST_ASSERT(doc->selectedEdge.primitiveEdgeIndex == 4u);
    TEST_ASSERT(ObjectHandleGizmoTarget_FromAuthoringSelection(object, doc, &target));
    TEST_ASSERT(target.kind == OBJECT_HANDLE_GIZMO_TARGET_TOPOLOGY_EDGE);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_free_view_edge_mode_click_selects_midpoint_handle(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument* doc = NULL;
    const Object3D* object = NULL;
    const ObjectAuthoringEdge* edge = NULL;
    const ObjectAuthoringVertex* edge_a = NULL;
    const ObjectAuthoringVertex* edge_b = NULL;
    ObjectHandleGizmoTarget target = ObjectHandleGizmoTarget_None();
    Hitbox edge_hit = {0};
    Vec3 midpoint = {0};
    uint32_t body_id = 0u;
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    doc = &state->objectAuthoring.document;
    body_id = state->editor.selectedObjectAssetBodyId;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore, body_id);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = object->transform.position;
    state->editor.objectEditSelectionMode = OBJECT_EDIT_SELECTION_EDGE;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;

    edge = ObjectAuthoringDocument_FindEdge(
        doc,
        ObjectAuthoringEdgeId_FromPrimitive(body_id, 4u));
    TEST_ASSERT(edge != NULL);
    edge_a = ObjectAuthoringDocument_FindVertex(doc, edge->vertexIds[0]);
    edge_b = ObjectAuthoringDocument_FindVertex(doc, edge->vertexIds[1]);
    TEST_ASSERT(edge_a != NULL);
    TEST_ASSERT(edge_b != NULL);
    midpoint = Vec3_Scale(Vec3_Add(edge_a->position, edge_b->position), 0.5f);
    object_authoring_world_screen_point(state, midpoint, &x, &y);

    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();
    edge_hit = HitboxSystem_GetHitAtOfType(x, y, HITBOX_OBJECT_TOPOLOGY_EDGE);
    TEST_ASSERT(edge_hit.type == HITBOX_OBJECT_TOPOLOGY_EDGE);
    TEST_ASSERT(edge_hit.index == (int)body_id);
    TEST_ASSERT(edge_hit.subIndex == 4);

    object_authoring_click_left(x, y);
    TEST_ASSERT(state->editor.selectedObject3DId == body_id);
    TEST_ASSERT(state->editor.selectedObjectAssetBodyId == body_id);
    TEST_ASSERT(state->editor.selectedObjectAssetFace == OBJECT3D_FACE_NONE);
    TEST_ASSERT(doc->selectionKind == OBJECT_AUTHORING_SELECTION_EDGE);
    TEST_ASSERT(doc->selectedEdge.bodyId == body_id);
    TEST_ASSERT(doc->selectedEdge.primitiveEdgeIndex == 4u);
    TEST_ASSERT(ObjectHandleGizmoTarget_FromAuthoringSelection(object, doc, &target));
    TEST_ASSERT(target.kind == OBJECT_HANDLE_GIZMO_TARGET_TOPOLOGY_EDGE);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_free_view_vertex_mode_click_selects_vertex_handle(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument* doc = NULL;
    const Object3D* object = NULL;
    const ObjectAuthoringVertex* vertex = NULL;
    ObjectHandleGizmoTarget target = ObjectHandleGizmoTarget_None();
    Hitbox vertex_hit = {0};
    uint32_t body_id = 0u;
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    doc = &state->objectAuthoring.document;
    body_id = state->editor.selectedObjectAssetBodyId;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore, body_id);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = object->transform.position;
    state->editor.objectEditSelectionMode = OBJECT_EDIT_SELECTION_VERTEX;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;

    vertex = ObjectAuthoringDocument_FindVertex(
        doc,
        ObjectAuthoringVertexId_FromPrimitive(body_id, 4u));
    TEST_ASSERT(vertex != NULL);
    object_authoring_world_screen_point(state, vertex->position, &x, &y);

    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();
    vertex_hit = HitboxSystem_GetHitAtOfType(x, y, HITBOX_OBJECT_TOPOLOGY_VERTEX);
    TEST_ASSERT(vertex_hit.type == HITBOX_OBJECT_TOPOLOGY_VERTEX);
    TEST_ASSERT(vertex_hit.index == (int)body_id);
    TEST_ASSERT(vertex_hit.subIndex == 4);

    object_authoring_click_left(x, y);
    TEST_ASSERT(state->editor.selectedObject3DId == body_id);
    TEST_ASSERT(state->editor.selectedObjectAssetBodyId == body_id);
    TEST_ASSERT(state->editor.selectedObjectAssetFace == OBJECT3D_FACE_NONE);
    TEST_ASSERT(doc->selectionKind == OBJECT_AUTHORING_SELECTION_VERTEX);
    TEST_ASSERT(doc->selectedVertex.bodyId == body_id);
    TEST_ASSERT(doc->selectedVertex.primitiveVertexIndex == 4u);
    TEST_ASSERT(ObjectHandleGizmoTarget_FromAuthoringSelection(object, doc, &target));
    TEST_ASSERT(target.kind == OBJECT_HANDLE_GIZMO_TARGET_TOPOLOGY_VERTEX);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_free_view_edge_mode_hover_targets_midpoint_handle(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument* doc = NULL;
    const Object3D* object = NULL;
    const ObjectAuthoringEdge* edge = NULL;
    const ObjectAuthoringVertex* edge_a = NULL;
    const ObjectAuthoringVertex* edge_b = NULL;
    Vec3 midpoint = {0};
    uint32_t body_id = 0u;
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    doc = &state->objectAuthoring.document;
    body_id = state->editor.selectedObjectAssetBodyId;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore, body_id);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = object->transform.position;
    state->editor.objectEditSelectionMode = OBJECT_EDIT_SELECTION_EDGE;

    edge = ObjectAuthoringDocument_FindEdge(
        doc,
        ObjectAuthoringEdgeId_FromPrimitive(body_id, 4u));
    TEST_ASSERT(edge != NULL);
    edge_a = ObjectAuthoringDocument_FindVertex(doc, edge->vertexIds[0]);
    edge_b = ObjectAuthoringDocument_FindVertex(doc, edge->vertexIds[1]);
    TEST_ASSERT(edge_a != NULL);
    TEST_ASSERT(edge_b != NULL);
    midpoint = Vec3_Scale(Vec3_Add(edge_a->position, edge_b->position), 0.5f);
    object_authoring_world_screen_point(state, midpoint, &x, &y);

    Global_FlagHitboxesDirty();
    UpdateHover(x, y);
    TEST_ASSERT(state->editor.hoveredObjectTopologyBodyId == body_id);
    TEST_ASSERT(state->editor.hoveredObjectTopologyEdgeIndex == 4);
    TEST_ASSERT(state->editor.hoveredObjectTopologyVertexIndex == -1);
    TEST_ASSERT(state->editor.hoveredObjectAssetFace == OBJECT3D_FACE_NONE);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_free_view_vertex_mode_hover_targets_vertex_handle(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument* doc = NULL;
    const Object3D* object = NULL;
    const ObjectAuthoringVertex* vertex = NULL;
    uint32_t body_id = 0u;
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    doc = &state->objectAuthoring.document;
    body_id = state->editor.selectedObjectAssetBodyId;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore, body_id);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = object->transform.position;
    state->editor.objectEditSelectionMode = OBJECT_EDIT_SELECTION_VERTEX;

    vertex = ObjectAuthoringDocument_FindVertex(
        doc,
        ObjectAuthoringVertexId_FromPrimitive(body_id, 4u));
    TEST_ASSERT(vertex != NULL);
    object_authoring_world_screen_point(state, vertex->position, &x, &y);

    Global_FlagHitboxesDirty();
    UpdateHover(x, y);
    TEST_ASSERT(state->editor.hoveredObjectTopologyBodyId == body_id);
    TEST_ASSERT(state->editor.hoveredObjectTopologyVertexIndex == 4);
    TEST_ASSERT(state->editor.hoveredObjectTopologyEdgeIndex == -1);
    TEST_ASSERT(state->editor.hoveredObjectAssetFace == OBJECT3D_FACE_NONE);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_viewport_pick_resolver_reports_topology_path(void) {
    GlobalState* state = NULL;
    const ObjectAuthoringDocument* doc = NULL;
    const Object3D* object = NULL;
    const ObjectAuthoringEdge* edge = NULL;
    const ObjectAuthoringVertex* edge_a = NULL;
    const ObjectAuthoringVertex* edge_b = NULL;
    const ObjectAuthoringVertex* vertex = NULL;
    ViewportPickResult edge_pick = {0};
    ViewportPickResult filtered_vertex_pick = {0};
    char pick_debug[320] = {0};
    Vec3 midpoint = {0};
    uint32_t body_id = 0u;
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    doc = &state->objectAuthoring.document;
    body_id = state->editor.selectedObjectAssetBodyId;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore, body_id);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = object->transform.position;
    state->editor.objectEditSelectionMode = OBJECT_EDIT_SELECTION_EDGE;

    edge = ObjectAuthoringDocument_FindEdge(
        doc,
        ObjectAuthoringEdgeId_FromPrimitive(body_id, 4u));
    TEST_ASSERT(edge != NULL);
    edge_a = ObjectAuthoringDocument_FindVertex(doc, edge->vertexIds[0]);
    edge_b = ObjectAuthoringDocument_FindVertex(doc, edge->vertexIds[1]);
    TEST_ASSERT(edge_a != NULL);
    TEST_ASSERT(edge_b != NULL);
    midpoint = Vec3_Scale(Vec3_Add(edge_a->position, edge_b->position), 0.5f);
    object_authoring_world_screen_point(state, midpoint, &x, &y);

    Global_FlagHitboxesDirty();
    edge_pick = ViewportPick_ResolveObjectWorkspaceHit(state, x, y, true);
    TEST_ASSERT(edge_pick.paneLane == POINTER_PANE_CENTER);
    TEST_ASSERT(edge_pick.objectEditTopologyModeActive);
    TEST_ASSERT(edge_pick.exactTopologyHit.type == HITBOX_OBJECT_TOPOLOGY_EDGE);
    TEST_ASSERT(edge_pick.finalHit.type == HITBOX_OBJECT_TOPOLOGY_EDGE);
    TEST_ASSERT(edge_pick.finalHit.index == (int)body_id);
    TEST_ASSERT(edge_pick.finalHit.subIndex == 4);
    TEST_ASSERT(edge_pick.reason == VIEWPORT_PICK_REASON_EXACT_TOPOLOGY_EDGE);
    TEST_ASSERT(ViewportPick_FormatLastDebug(pick_debug, sizeof(pick_debug), state));
    TEST_ASSERT(strstr(pick_debug, "pane:Center") != NULL);
    TEST_ASSERT(strstr(pick_debug, "final:TopoEdge") != NULL);
    TEST_ASSERT(strstr(pick_debug, "why:ExactEdge") != NULL);

    vertex = ObjectAuthoringDocument_FindVertex(
        doc,
        ObjectAuthoringVertexId_FromPrimitive(body_id, 4u));
    TEST_ASSERT(vertex != NULL);
    object_authoring_world_screen_point(state, vertex->position, &x, &y);

    Global_FlagHitboxesDirty();
    filtered_vertex_pick = ViewportPick_ResolveObjectWorkspaceHit(state, x, y, true);
    TEST_ASSERT(filtered_vertex_pick.rawHit.type == HITBOX_OBJECT_TOPOLOGY_VERTEX);
    TEST_ASSERT(filtered_vertex_pick.exactTopologyHit.type == HITBOX_NONE);
    TEST_ASSERT(filtered_vertex_pick.finalHit.type == HITBOX_NONE);
    TEST_ASSERT(filtered_vertex_pick.reason == VIEWPORT_PICK_REASON_FILTERED_BY_OBJECT_EDIT_MODE);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_topology_modes_suppress_body_handles_before_target(void) {
    GlobalState* state = NULL;
    const Object3D* object = NULL;
    Vec3 corners[8] = {0};
    Vec2 corner_screen = {0};
    Vec3 center_tip = {0};
    Vec2 center_tip_screen = {0};
    Hitbox prism_handle = {0};
    Hitbox center_gizmo = {0};
    uint32_t body_id = 0u;
    float axis_world_len = 0.0f;

    TEST_ASSERT(seed_object_authoring_state(&state));
    body_id = state->editor.selectedObjectAssetBodyId;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore, body_id);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = object->transform.position;
    state->editor.selectedObject3DId = body_id;
    state->editor.selectedObjectAssetFace = OBJECT3D_FACE_NONE;
    state->editor.objectEditSelectionMode = OBJECT_EDIT_SELECTION_EDGE;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    TEST_ASSERT(Layout_Object3D_ComputeRectPrismCorners(object, corners));

    axis_world_len = fmaxf(state->layout.gridSize * 2.0f, 1.0f);
    corner_screen = WorldToScreen(Vec3_ProjectToView(corners[0],
                                                     state->activePlane,
                                                     &state->freeViewCamera),
                                  &state->grid);
    center_tip = Vec3_Add(object->transform.position,
                          Vec3_Scale(GizmoAxisDirection_WorldVector(GIZMO_AXIS_DIR_POS_X),
                                     axis_world_len));
    center_tip_screen = WorldToScreen(Vec3_ProjectToView(center_tip,
                                                         state->activePlane,
                                                         &state->freeViewCamera),
                                      &state->grid);

    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();
    prism_handle = HitboxSystem_GetHitAtOfType((int)corner_screen.x,
                                               (int)corner_screen.y,
                                               HITBOX_OBJECT3D_PRISM_HANDLE);
    center_gizmo = HitboxSystem_GetHitAtOfType((int)center_tip_screen.x,
                                               (int)center_tip_screen.y,
                                               HITBOX_OBJECT3D_GIZMO_AXIS);
    TEST_ASSERT(prism_handle.type == HITBOX_NONE);
    TEST_ASSERT(center_gizmo.type == HITBOX_NONE);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_vertex_mode_suppresses_face_hover(void) {
    GlobalState* state = NULL;
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    object_authoring_world_screen_point(state, (Vec3){0.0f, 0.0f, 4.0f}, &x, &y);

    state->editor.objectEditSelectionMode = OBJECT_EDIT_SELECTION_VERTEX;
    Global_FlagHitboxesDirty();
    UpdateHover(x, y);
    TEST_ASSERT(state->editor.hoveredObjectTopologyBodyId == 0u);
    TEST_ASSERT(state->editor.hoveredObjectTopologyVertexIndex == -1);
    TEST_ASSERT(state->editor.hoveredObjectTopologyEdgeIndex == -1);
    TEST_ASSERT(state->editor.hoveredObjectAssetFace == OBJECT3D_FACE_NONE);
    TEST_ASSERT(state->editor.hoveredObjectAssetBodyId == 0u);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_vertex_mode_click_does_not_fall_back_to_body(void) {
    GlobalState* state = NULL;
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    object_authoring_world_screen_point(state, (Vec3){0.0f, 0.0f, 4.0f}, &x, &y);

    state->editor.objectEditSelectionMode = OBJECT_EDIT_SELECTION_VERTEX;
    state->editor.selectedObject3DId = 0u;
    state->editor.selectedObjectAssetBodyId = 0u;
    state->editor.selectedObjectAssetFace = OBJECT3D_FACE_NONE;
    (void)ObjectAuthoringDocument_SetSelection(&state->objectAuthoring.document,
                                               0u,
                                               OBJECT3D_FACE_NONE);
    Global_FlagHitboxesDirty();

    object_authoring_click_left(x, y);

    TEST_ASSERT(state->editor.selectedObject3DId == 0u);
    TEST_ASSERT(state->editor.selectedObjectAssetBodyId == 0u);
    TEST_ASSERT(state->editor.selectedObjectAssetFace == OBJECT3D_FACE_NONE);
    TEST_ASSERT(state->objectAuthoring.document.selectionKind == OBJECT_AUTHORING_SELECTION_NONE);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_vertex_mode_empty_click_preserves_active_body(void) {
    GlobalState* state = NULL;
    uint32_t body_id = 0u;
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    body_id = state->editor.selectedObjectAssetBodyId;
    TEST_ASSERT(body_id != 0u);
    object_authoring_world_screen_point(state, (Vec3){0.0f, 0.0f, 4.0f}, &x, &y);

    state->editor.objectEditSelectionMode = OBJECT_EDIT_SELECTION_VERTEX;
    Global_FlagHitboxesDirty();

    object_authoring_click_left(x, y);

    TEST_ASSERT(state->editor.selectedObject3DId == body_id);
    TEST_ASSERT(state->editor.selectedObjectAssetBodyId == body_id);
    TEST_ASSERT(state->editor.selectedObjectAssetFace == OBJECT3D_FACE_NONE);
    TEST_ASSERT(state->objectAuthoring.document.selectionKind == OBJECT_AUTHORING_SELECTION_BODY);
    TEST_ASSERT(state->objectAuthoring.document.selectedFace.bodyId == body_id);
    TEST_ASSERT(state->objectAuthoring.document.selectedVertex.vertexId == 0u);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_edge_mode_empty_click_preserves_active_body(void) {
    GlobalState* state = NULL;
    uint32_t body_id = 0u;
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    body_id = state->editor.selectedObjectAssetBodyId;
    TEST_ASSERT(body_id != 0u);
    object_authoring_world_screen_point(state, (Vec3){0.0f, 0.0f, 4.0f}, &x, &y);

    state->editor.objectEditSelectionMode = OBJECT_EDIT_SELECTION_EDGE;
    Global_FlagHitboxesDirty();

    object_authoring_click_left(x, y);

    TEST_ASSERT(state->editor.selectedObject3DId == body_id);
    TEST_ASSERT(state->editor.selectedObjectAssetBodyId == body_id);
    TEST_ASSERT(state->editor.selectedObjectAssetFace == OBJECT3D_FACE_NONE);
    TEST_ASSERT(state->objectAuthoring.document.selectionKind == OBJECT_AUTHORING_SELECTION_BODY);
    TEST_ASSERT(state->objectAuthoring.document.selectedFace.bodyId == body_id);
    TEST_ASSERT(state->objectAuthoring.document.selectedEdge.edgeId == 0u);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_rectangle_sketch_uses_session_document(void) {
    GlobalState* state = NULL;
    const ObjectAuthoringSketch* sketch = NULL;

    TEST_ASSERT(seed_object_authoring_state(&state));
    TEST_ASSERT(Editor_ObjectFaceSketchArmRectangle(state));
    state->editor.objectFaceSketchToolArmed = false;
    Editor_ObjectFaceSketchSetRectangleUV(&state->editor,
                                          (Vec2){ -1.25f, -0.75f },
                                          (Vec2){ 1.5f, 1.25f });
    sketch = ObjectAuthoringDocument_ActiveSketch(&state->objectAuthoring.document);
    TEST_ASSERT(sketch != NULL);
    TEST_ASSERT(sketch->faceRef.bodyId == state->editor.objectFaceSketchBodyId);
    TEST_ASSERT(sketch->faceRef.primitiveFace == state->editor.objectFaceSketchFace);
    TEST_ASSERT(sketch->faceRef.faceId ==
                ObjectAuthoringFaceId_FromPrimitive(sketch->faceRef.bodyId,
                                                    sketch->faceRef.primitiveFace));
    TEST_ASSERT(sketch->minUV.x == state->editor.objectFaceSketchStartUV.x);
    TEST_ASSERT(sketch->maxUV.y == state->editor.objectFaceSketchCurrentUV.y);
    TEST_ASSERT(state->objectAuthoring.document.operationCount == 2u);
    TEST_ASSERT(state->objectAuthoring.document.operations[1].kind ==
                OBJECT_AUTHORING_OPERATION_SKETCH_RECTANGLE);
    TEST_ASSERT(state->objectAuthoring.document.operations[1].sketchSnapshot.sketchId ==
                sketch->sketchId);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_extrude_records_operation_and_results(void) {
    GlobalState* state = NULL;
    const ObjectAuthoringOperation* op = NULL;
    size_t object_count_before = 0u;

    TEST_ASSERT(seed_object_authoring_state(&state));
    TEST_ASSERT(Editor_ObjectFaceSketchArmRectangle(state));
    state->editor.objectFaceSketchToolArmed = false;
    Editor_ObjectFaceSketchSetRectangleUV(&state->editor,
                                          (Vec2){ -1.0f, -1.0f },
                                          (Vec2){ 1.0f, 1.0f });
    state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_SKETCH_SELECT;
    state->editor.selectedObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_BODY;

    object_count_before = Layout_ObjectStore_LiveCount(&state->layout.objectStore);
    TEST_ASSERT(Editor_ObjectFaceExtrudeTrigger(state, OBJECT_FACE_EXTRUDE_MODE_ADD));
    TEST_ASSERT(Editor_ObjectFaceExtrudeTrigger(state, OBJECT_FACE_EXTRUDE_MODE_ADD));
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) ==
                object_count_before + 1u);
    TEST_ASSERT(state->objectAuthoring.document.operationCount == 3u);
    op = &state->objectAuthoring.document.operations[2];
    TEST_ASSERT(op->kind == OBJECT_AUTHORING_OPERATION_EXTRUDE_ADD);
    TEST_ASSERT(op->sketchId != 0u);
    TEST_ASSERT(op->faceRef.faceId ==
                ObjectAuthoringFaceId_FromPrimitive(op->faceRef.bodyId,
                                                    op->faceRef.primitiveFace));
    TEST_ASSERT(op->resultBodyCount == 1u);
    TEST_ASSERT(op->resultBodyIds[0] != 0u);
    TEST_ASSERT(op->resultBodies[0].bodyId == op->resultBodyIds[0]);
    TEST_ASSERT(state->objectAuthoring.document.bodyCount ==
                Layout_ObjectStore_LiveCount(&state->layout.objectStore));

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_replay_matches_extrude_add_output(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument evaluated;
    ObjectAuthoringEvalDiagnostics diagnostics;
    size_t object_count_before = 0u;

    ObjectAuthoringDocument_Init(&evaluated);
    TEST_ASSERT(seed_object_authoring_state(&state));
    TEST_ASSERT(Editor_ObjectFaceSketchArmRectangle(state));
    state->editor.objectFaceSketchToolArmed = false;
    Editor_ObjectFaceSketchSetRectangleUV(&state->editor,
                                          (Vec2){ -1.0f, -1.0f },
                                          (Vec2){ 1.0f, 1.0f });
    state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_SKETCH_SELECT;
    state->editor.selectedObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_BODY;

    object_count_before = Layout_ObjectStore_LiveCount(&state->layout.objectStore);
    TEST_ASSERT(Editor_ObjectFaceExtrudeTrigger(state, OBJECT_FACE_EXTRUDE_MODE_ADD));
    TEST_ASSERT(Editor_ObjectFaceExtrudeTrigger(state, OBJECT_FACE_EXTRUDE_MODE_ADD));
    TEST_ASSERT(ObjectAuthoring_EvaluateDocument(&state->objectAuthoring.document,
                                                 &evaluated,
                                                 &diagnostics));
    TEST_ASSERT(evaluated.bodyCount == object_count_before + 1u);
    TEST_ASSERT(evaluated.faceCount == 12u);
    TEST_ASSERT(evaluated.selectedFace.faceId ==
                state->objectAuthoring.document.selectedFace.faceId);
    TEST_ASSERT(evaluated.bodyCount == Layout_ObjectStore_LiveCount(&state->layout.objectStore));
    for (size_t i = 0; i < evaluated.bodyCount; ++i) {
        const Object3D* object =
            Layout_ObjectStore_FindConst(&state->layout.objectStore, evaluated.bodies[i].bodyId);
        TEST_ASSERT(object != NULL);
        TEST_ASSERT(object->kind == evaluated.bodies[i].sourceKind);
        if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.width,
                                             evaluated.bodies[i].rectPrism.width));
            TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.height,
                                             evaluated.bodies[i].rectPrism.height));
            TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.depth,
                                             evaluated.bodies[i].rectPrism.depth));
        }
    }

    ObjectAuthoringDocument_Free(&evaluated);
    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_stable_face_refs_survive_save_load(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument loaded_authoring;
    Layout loaded_layout;
    const ObjectAuthoringSketch* loaded_sketch = NULL;
    const ObjectAuthoringFace* loaded_face = NULL;
    bool has_authoring = false;
    char diagnostics[256] = {0};
    char root[256];
    char path[512];

    ObjectAuthoringDocument_Init(&loaded_authoring);
    Layout_Init(&loaded_layout, 1.0f);
    snprintf(root, sizeof(root), "/tmp/ld_object_authoring_face_ids_%ld", (long)getpid());
    TEST_ASSERT(mkdir(root, 0700) == 0 || errno == EEXIST);
    snprintf(path, sizeof(path), "%s/asset.json", root);

    TEST_ASSERT(seed_object_authoring_state(&state));
    TEST_ASSERT(Editor_ObjectFaceSketchArmRectangle(state));
    state->editor.objectFaceSketchToolArmed = false;
    Editor_ObjectFaceSketchSetRectangleUV(&state->editor,
                                          (Vec2){ -1.0f, -1.0f },
                                          (Vec2){ 1.0f, 1.0f });
    TEST_ASSERT(state->objectAuthoring.document.selectedFace.faceId != 0u);

    TEST_ASSERT(LayoutObjectAssetMeshAuthoring_SaveWithAuthoring(
        &state->layout,
        &state->objectAuthoring.document,
        path,
        diagnostics,
        sizeof(diagnostics)));
    TEST_ASSERT(LayoutObjectAssetMeshAuthoring_LoadWithAuthoring(&loaded_layout,
                                                                 &loaded_authoring,
                                                                 &has_authoring,
                                                                 path,
                                                                 diagnostics,
                                                                 sizeof(diagnostics)));
    TEST_ASSERT(has_authoring);
    TEST_ASSERT(loaded_authoring.faceCount == 6u);
    TEST_ASSERT(loaded_authoring.selectedFace.faceId ==
                state->objectAuthoring.document.selectedFace.faceId);
    loaded_sketch = ObjectAuthoringDocument_ActiveSketch(&loaded_authoring);
    TEST_ASSERT(loaded_sketch != NULL);
    TEST_ASSERT(loaded_sketch->faceRef.faceId ==
                state->objectAuthoring.document.selectedFace.faceId);
    loaded_face = ObjectAuthoringDocument_FindFace(&loaded_authoring,
                                                   loaded_sketch->faceRef.faceId);
    TEST_ASSERT(loaded_face != NULL);
    TEST_ASSERT(loaded_face->ref.bodyId == loaded_sketch->faceRef.bodyId);
    TEST_ASSERT(loaded_face->ref.primitiveFace == loaded_sketch->faceRef.primitiveFace);

    Layout_Free(&loaded_layout);
    ObjectAuthoringDocument_Free(&loaded_authoring);
    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_replay_matches_extrude_cut_output(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument evaluated;
    ObjectAuthoringEvalDiagnostics diagnostics;
    size_t object_count_before = 0u;

    ObjectAuthoringDocument_Init(&evaluated);
    TEST_ASSERT(seed_object_authoring_state(&state));
    TEST_ASSERT(Editor_ObjectFaceSketchArmRectangle(state));
    state->editor.objectFaceSketchToolArmed = false;
    Editor_ObjectFaceSketchSetRectangleUV(&state->editor,
                                          (Vec2){ -1.0f, -1.0f },
                                          (Vec2){ 1.0f, 1.0f });
    state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_SKETCH_SELECT;
    state->editor.selectedObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_BODY;

    object_count_before = Layout_ObjectStore_LiveCount(&state->layout.objectStore);
    TEST_ASSERT(Editor_ObjectFaceExtrudeTrigger(state, OBJECT_FACE_EXTRUDE_MODE_CUT));
    TEST_ASSERT(Editor_ObjectFaceExtrudeTrigger(state, OBJECT_FACE_EXTRUDE_MODE_CUT));
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) > object_count_before);
    TEST_ASSERT(ObjectAuthoring_EvaluateDocument(&state->objectAuthoring.document,
                                                 &evaluated,
                                                 &diagnostics));
    TEST_ASSERT(evaluated.bodyCount == Layout_ObjectStore_LiveCount(&state->layout.objectStore));
    for (size_t i = 0; i < evaluated.bodyCount; ++i) {
        const Object3D* object =
            Layout_ObjectStore_FindConst(&state->layout.objectStore, evaluated.bodies[i].bodyId);
        TEST_ASSERT(object != NULL);
        TEST_ASSERT(object->kind == evaluated.bodies[i].sourceKind);
        if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.width,
                                             evaluated.bodies[i].rectPrism.width));
            TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.height,
                                             evaluated.bodies[i].rectPrism.height));
            TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.depth,
                                             evaluated.bodies[i].rectPrism.depth));
        }
    }

    ObjectAuthoringDocument_Free(&evaluated);
    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_active_sketch_drives_hitboxes_without_editor_mirror(void) {
    GlobalState* state = NULL;
    const ObjectAuthoringSketch* sketch = NULL;
    Hitbox hit = {0};
    int x = 0;
    int y = 0;

    TEST_ASSERT(seed_object_authoring_state(&state));
    TEST_ASSERT(Editor_ObjectFaceSketchArmRectangle(state));
    state->editor.objectFaceSketchToolArmed = false;
    Editor_ObjectFaceSketchSetRectangleUV(&state->editor,
                                          (Vec2){ -1.0f, -1.0f },
                                          (Vec2){ 1.0f, 1.0f });
    sketch = ObjectAuthoringDocument_ActiveSketch(&state->objectAuthoring.document);
    TEST_ASSERT(sketch != NULL);

    state->editor.objectFaceSketchHasRectangle = false;
    state->editor.objectFaceSketchBodyId = 0u;
    state->editor.objectFaceSketchFace = OBJECT3D_FACE_NONE;
    state->editor.objectFaceSketchFrame = (PlaneFrame3){0};
    state->editor.objectFaceSketchStartUV = (Vec2){0.0f, 0.0f};
    state->editor.objectFaceSketchCurrentUV = (Vec2){0.0f, 0.0f};

    object_authoring_sketch_screen_point(state, sketch, 0.0f, 0.0f, &x, &y);
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();
    hit = HitboxSystem_GetHitAt(x, y);
    TEST_ASSERT(hit.type == HITBOX_OBJECT_FACE_SKETCH_BODY);
    TEST_ASSERT(hit.index == (int)sketch->faceRef.bodyId);

    TEST_ASSERT(Editor_ObjectFaceSketchSyncFromAuthoring(state));
    TEST_ASSERT(state->editor.objectFaceSketchHasRectangle);
    TEST_ASSERT(state->editor.objectFaceSketchBodyId == sketch->faceRef.bodyId);
    TEST_ASSERT(state->editor.objectFaceSketchFace == sketch->faceRef.primitiveFace);

    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_asset_save_load_preserves_operation_stack(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument loaded_authoring;
    Layout loaded_layout;
    bool has_authoring = false;
    char diagnostics[256] = {0};
    char root[256];
    char path[512];

    ObjectAuthoringDocument_Init(&loaded_authoring);
    Layout_Init(&loaded_layout, 1.0f);
    snprintf(root, sizeof(root), "/tmp/ld_object_authoring_asset_%ld", (long)getpid());
    TEST_ASSERT(mkdir(root, 0700) == 0 || errno == EEXIST);
    snprintf(path, sizeof(path), "%s/asset.json", root);

    TEST_ASSERT(seed_object_authoring_state(&state));
    TEST_ASSERT(Editor_ObjectFaceSketchArmRectangle(state));
    state->editor.objectFaceSketchToolArmed = false;
    Editor_ObjectFaceSketchSetRectangleUV(&state->editor,
                                          (Vec2){ -1.0f, -1.0f },
                                          (Vec2){ 1.0f, 1.0f });
    state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_SKETCH_SELECT;
    state->editor.selectedObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_BODY;
    TEST_ASSERT(Editor_ObjectFaceExtrudeTrigger(state, OBJECT_FACE_EXTRUDE_MODE_ADD));
    TEST_ASSERT(Editor_ObjectFaceExtrudeTrigger(state, OBJECT_FACE_EXTRUDE_MODE_ADD));

    TEST_ASSERT(LayoutObjectAssetMeshAuthoring_SaveWithAuthoring(
        &state->layout,
        &state->objectAuthoring.document,
        path,
        diagnostics,
        sizeof(diagnostics)));
    TEST_ASSERT(LayoutObjectAssetMeshAuthoring_LoadWithAuthoring(&loaded_layout,
                                                                 &loaded_authoring,
                                                                 &has_authoring,
                                                                 path,
                                                                 diagnostics,
                                                                 sizeof(diagnostics)));
    TEST_ASSERT(has_authoring);
    TEST_ASSERT(loaded_authoring.operationCount == state->objectAuthoring.document.operationCount);
    TEST_ASSERT(loaded_authoring.operationCount == 3u);
    TEST_ASSERT(loaded_authoring.operations[0].kind ==
                OBJECT_AUTHORING_OPERATION_CREATE_PRIMITIVE);
    TEST_ASSERT(loaded_authoring.operations[1].kind ==
                OBJECT_AUTHORING_OPERATION_SKETCH_RECTANGLE);
    TEST_ASSERT(loaded_authoring.operations[2].kind ==
                OBJECT_AUTHORING_OPERATION_EXTRUDE_ADD);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&loaded_layout.objectStore) ==
                Layout_ObjectStore_LiveCount(&state->layout.objectStore));

    Layout_Free(&loaded_layout);
    ObjectAuthoringDocument_Free(&loaded_authoring);
    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_replay_reports_missing_extrude_result(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument evaluated;
    ObjectAuthoringEvalDiagnostics diagnostics;

    ObjectAuthoringDocument_Init(&evaluated);
    TEST_ASSERT(seed_object_authoring_state(&state));
    TEST_ASSERT(ObjectAuthoringDocument_RecordExtrude(&state->objectAuthoring.document,
                                                      OBJECT_AUTHORING_OPERATION_EXTRUDE_ADD,
                                                      state->editor.selectedObjectAssetBodyId,
                                                      state->editor.selectedObjectAssetFace,
                                                      0u,
                                                      1.0f,
                                                      NULL,
                                                      NULL,
                                                      0u,
                                                      NULL));
    TEST_ASSERT(!ObjectAuthoring_EvaluateDocument(&state->objectAuthoring.document,
                                                  &evaluated,
                                                  &diagnostics));
    TEST_ASSERT(diagnostics.status == OBJECT_AUTHORING_EVAL_MISSING_RESULT);
    TEST_ASSERT(diagnostics.failedOperationId != 0u);

    ObjectAuthoringDocument_Free(&evaluated);
    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_replay_reports_stale_operation_face_ref(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument evaluated;
    ObjectAuthoringEvalDiagnostics diagnostics;
    ObjectAuthoringOperation* op = NULL;

    ObjectAuthoringDocument_Init(&evaluated);
    TEST_ASSERT(seed_object_authoring_state(&state));
    TEST_ASSERT(Editor_ObjectFaceSketchArmRectangle(state));
    state->editor.objectFaceSketchToolArmed = false;
    Editor_ObjectFaceSketchSetRectangleUV(&state->editor,
                                          (Vec2){ -1.0f, -1.0f },
                                          (Vec2){ 1.0f, 1.0f });
    state->editor.objectAuthoringMode = OBJECT_AUTHORING_MODE_SKETCH_SELECT;
    state->editor.selectedObjectFaceSketchHandle = OBJECT_FACE_SKETCH_HANDLE_BODY;
    TEST_ASSERT(Editor_ObjectFaceExtrudeTrigger(state, OBJECT_FACE_EXTRUDE_MODE_ADD));
    TEST_ASSERT(Editor_ObjectFaceExtrudeTrigger(state, OBJECT_FACE_EXTRUDE_MODE_ADD));
    TEST_ASSERT(state->objectAuthoring.document.operationCount >= 3u);
    op = &state->objectAuthoring.document.operations[2];
    op->faceRef.primitiveFace = OBJECT3D_FACE_RECT_PRISM_NEG_N;

    TEST_ASSERT(!ObjectAuthoring_EvaluateDocument(&state->objectAuthoring.document,
                                                  &evaluated,
                                                  &diagnostics));
    TEST_ASSERT(diagnostics.status == OBJECT_AUTHORING_EVAL_STALE_FACE_REF);
    TEST_ASSERT(diagnostics.failedOperationId == op->operationId);
    TEST_ASSERT(strstr(diagnostics.message, "Stale adapter") != NULL);

    ObjectAuthoringDocument_Free(&evaluated);
    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_runtime_mesh_compile_emits_face_surface_groups(void) {
    GlobalState* state = NULL;
    ObjectAuthoringRuntimeMesh runtime;
    char diagnostics[256] = {0};

    ObjectAuthoringRuntimeMesh_Init(&runtime);
    TEST_ASSERT(seed_object_authoring_state(&state));
    TEST_ASSERT(ObjectAuthoring_CompileRuntimeMesh(&state->objectAuthoring.document,
                                                   "asset_runtime_rect_prism",
                                                   "asset_authoring_rect_prism",
                                                   &runtime,
                                                   diagnostics,
                                                   sizeof(diagnostics)));
    TEST_ASSERT(ObjectAuthoringRuntimeMesh_Validate(&runtime,
                                                    diagnostics,
                                                    sizeof(diagnostics)));
    TEST_ASSERT(runtime.vertexCount == 8u);
    TEST_ASSERT(runtime.triangleCount == 12u);
    TEST_ASSERT(runtime.surfaceGroupCount == 6u);
    TEST_ASSERT(strcmp(runtime.contract.asset_id, "asset_runtime_rect_prism") == 0);
    TEST_ASSERT(strcmp(runtime.contract.source_asset_id, "asset_authoring_rect_prism") == 0);
    TEST_ASSERT(strcmp(runtime.surfaceGroups[0].groupId, "face_18") == 0);
    TEST_ASSERT(runtime.surfaceGroups[0].triangleStart == 0u);
    TEST_ASSERT(runtime.surfaceGroups[0].triangleCount == 2u);
    TEST_ASSERT(strcmp(runtime.triangles[0].surfaceGroupId,
                       runtime.surfaceGroups[0].groupId) == 0);
    TEST_ASSERT(runtime.contract.local_bounds.min.x == -2.0);
    TEST_ASSERT(runtime.contract.local_bounds.max.x == 2.0);
    TEST_ASSERT(runtime.contract.local_bounds.min.y == -3.0);
    TEST_ASSERT(runtime.contract.local_bounds.max.y == 3.0);
    TEST_ASSERT(runtime.contract.local_bounds.min.z == -4.0);
    TEST_ASSERT(runtime.contract.local_bounds.max.z == 4.0);

    ObjectAuthoringRuntimeMesh_Free(&runtime);
    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_saved_asset_compiles_to_runtime_mesh_file(void) {
    GlobalState* state = NULL;
    ObjectAuthoringDocument loaded_authoring;
    Layout loaded_layout;
    ObjectAuthoringRuntimeMesh runtime;
    bool has_authoring = false;
    char diagnostics[256] = {0};
    char root[256];
    char path[512];
    char runtime_path[512];

    ObjectAuthoringDocument_Init(&loaded_authoring);
    Layout_Init(&loaded_layout, 1.0f);
    ObjectAuthoringRuntimeMesh_Init(&runtime);
    snprintf(root, sizeof(root), "/tmp/ld_object_authoring_runtime_%ld", (long)getpid());
    TEST_ASSERT(mkdir(root, 0700) == 0 || errno == EEXIST);
    snprintf(path, sizeof(path), "%s/asset.json", root);
    snprintf(runtime_path, sizeof(runtime_path), "%s/asset.runtime.json", root);

    TEST_ASSERT(seed_object_authoring_state(&state));
    TEST_ASSERT(LayoutObjectAssetMeshAuthoring_SaveWithAuthoring(
        &state->layout,
        &state->objectAuthoring.document,
        path,
        diagnostics,
        sizeof(diagnostics)));
    TEST_ASSERT(LayoutObjectAssetMeshAuthoring_LoadWithAuthoring(&loaded_layout,
                                                                 &loaded_authoring,
                                                                 &has_authoring,
                                                                 path,
                                                                 diagnostics,
                                                                 sizeof(diagnostics)));
    TEST_ASSERT(has_authoring);
    TEST_ASSERT(ObjectAuthoring_CompileRuntimeMesh(&loaded_authoring,
                                                   "asset_runtime_from_saved",
                                                   "asset",
                                                   &runtime,
                                                   diagnostics,
                                                   sizeof(diagnostics)));
    TEST_ASSERT(ObjectAuthoringRuntimeMesh_SaveFile(&runtime,
                                                    runtime_path,
                                                    diagnostics,
                                                    sizeof(diagnostics)));

    ObjectAuthoringRuntimeMesh_Free(&runtime);
    Layout_Free(&loaded_layout);
    ObjectAuthoringDocument_Free(&loaded_authoring);
    shutdown_object_authoring_state();
    return true;
}

static bool test_object_authoring_ui_export_writes_runtime_mesh_sidecar(void) {
    GlobalState* state = NULL;
    struct stat st;
    char root[256];
    char asset_path[512];
    char runtime_path[512];

    snprintf(root, sizeof(root), "/tmp/ld_object_authoring_ui_export_%ld", (long)getpid());
    TEST_ASSERT(mkdir(root, 0700) == 0 || errno == EEXIST);
    snprintf(asset_path, sizeof(asset_path), "%s/ui_asset.json", root);
    snprintf(runtime_path, sizeof(runtime_path), "%s/ui_asset.runtime.json", root);

    TEST_ASSERT(seed_object_authoring_state(&state));
    TEST_ASSERT(Global_SetObjectAssetRoot(root, false));
    Global_OnObjectAssetLoaded(asset_path);
    TEST_ASSERT(UIPanel_ExportObjectRuntimeMesh());
    TEST_ASSERT(stat(runtime_path, &st) == 0);
    TEST_ASSERT(st.st_size > 0);
    TEST_ASSERT(strcmp(state->lastObjectRuntimeMeshPath, runtime_path) == 0);
    TEST_ASSERT(strstr(state->objectRuntimeMeshStatus, "Mesh exported") != NULL);

    shutdown_object_authoring_state();
    return true;
}

bool object_authoring_run_tests(void) {
    const TestCase cases[] = {
        {"session mirrors object workspace bodies",
         test_object_authoring_session_mirrors_object_workspace_bodies},
        {"face ref matching prefers stable ids",
         test_object_authoring_face_ref_matching_prefers_stable_ids},
        {"face ref diagnostics classify invalid refs",
         test_object_authoring_face_ref_diagnostics_classify_invalid_refs},
        {"rebuilds stable vertex edge topology",
         test_object_authoring_rebuilds_stable_vertex_edge_topology},
        {"selects vertex and edge topology refs",
         test_object_authoring_selects_vertex_and_edge_topology_refs},
        {"topology hitboxes precede object body",
         test_object_authoring_topology_hitboxes_precede_object_body},
        {"topology selection resolves gizmo targets",
         test_object_authoring_topology_selection_resolves_gizmo_targets},
        {"topology selection emits gizmo axis hitboxes",
         test_object_authoring_topology_selection_emits_gizmo_axis_hitboxes},
        {"edge mode hover targets edge not face",
         test_object_authoring_edge_mode_hover_targets_edge_not_face},
        {"vertex mode click selects vertex and gizmo target",
         test_object_authoring_vertex_mode_click_selects_vertex_and_gizmo_target},
        {"edge mode click selects edge and gizmo target",
         test_object_authoring_edge_mode_click_selects_edge_and_gizmo_target},
        {"free view edge mode click selects midpoint handle",
         test_object_authoring_free_view_edge_mode_click_selects_midpoint_handle},
        {"free view vertex mode click selects vertex handle",
         test_object_authoring_free_view_vertex_mode_click_selects_vertex_handle},
        {"free view edge mode hover targets midpoint handle",
         test_object_authoring_free_view_edge_mode_hover_targets_midpoint_handle},
        {"free view vertex mode hover targets vertex handle",
         test_object_authoring_free_view_vertex_mode_hover_targets_vertex_handle},
        {"viewport pick resolver reports topology path",
         test_object_authoring_viewport_pick_resolver_reports_topology_path},
        {"topology modes suppress body handles before target",
         test_object_authoring_topology_modes_suppress_body_handles_before_target},
        {"vertex mode suppresses face hover",
         test_object_authoring_vertex_mode_suppresses_face_hover},
        {"vertex mode click does not fall back to body",
         test_object_authoring_vertex_mode_click_does_not_fall_back_to_body},
        {"vertex mode empty click preserves active body",
         test_object_authoring_vertex_mode_empty_click_preserves_active_body},
        {"edge mode empty click preserves active body",
         test_object_authoring_edge_mode_empty_click_preserves_active_body},
        {"rectangle sketch uses session document",
         test_object_authoring_rectangle_sketch_uses_session_document},
        {"extrude records operation and results",
         test_object_authoring_extrude_records_operation_and_results},
        {"active sketch drives hitboxes without editor mirror",
         test_object_authoring_active_sketch_drives_hitboxes_without_editor_mirror},
        {"replay matches extrude add output",
         test_object_authoring_replay_matches_extrude_add_output},
        {"replay matches extrude cut output",
         test_object_authoring_replay_matches_extrude_cut_output},
        {"asset save load preserves operation stack",
         test_object_authoring_asset_save_load_preserves_operation_stack},
        {"stable face refs survive save load",
         test_object_authoring_stable_face_refs_survive_save_load},
        {"replay reports missing extrude result",
         test_object_authoring_replay_reports_missing_extrude_result},
        {"replay reports stale operation face ref",
         test_object_authoring_replay_reports_stale_operation_face_ref},
        {"runtime mesh compile emits face surface groups",
         test_object_authoring_runtime_mesh_compile_emits_face_surface_groups},
        {"saved asset compiles to runtime mesh file",
         test_object_authoring_saved_asset_compiles_to_runtime_mesh_file},
        {"ui export writes runtime mesh sidecar",
         test_object_authoring_ui_export_writes_runtime_mesh_sidecar},
    };
    return run_test_cases("object_authoring", cases, sizeof(cases) / sizeof(cases[0]));
}
