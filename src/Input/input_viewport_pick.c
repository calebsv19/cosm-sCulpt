#include "Input/input_viewport_pick.h"

#include <stdio.h>

static ViewportPickResult g_lastPickResult;
static int g_lastPickMouseX = 0;
static int g_lastPickMouseY = 0;
static bool g_lastPickValid = false;

static Hitbox ViewportPick_None(void) {
    return (Hitbox){ .type = HITBOX_NONE, .index = -1, .subIndex = -1 };
}

static bool ViewportPick_IsObjectEditSuppressedHit(HitboxType type) {
    return type == HITBOX_OBJECT3D ||
           type == HITBOX_OBJECT3D_GIZMO_AXIS ||
           type == HITBOX_OBJECT3D_PRISM_HANDLE ||
           type == HITBOX_OBJECT3D_PLANE_CORNER ||
           type == HITBOX_OBJECT3D_PLANE_EDGE ||
           type == HITBOX_SCENE_BOUNDS_HANDLE ||
           type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS;
}

static Hitbox ViewportPick_FilterObjectEditModeHit(const GlobalState* state,
                                                   int mouse_x,
                                                   int mouse_y,
                                                   Hitbox hit,
                                                   Hitbox* out_exact,
                                                   ViewportPickReason* out_reason) {
    Hitbox exact = ViewportPick_None();
    if (out_reason) *out_reason = VIEWPORT_PICK_REASON_RAW_HIT;
    if (out_exact) *out_exact = exact;

    if (!state ||
        state->workspaceMode != LINE_DRAWING_WORKSPACE_MODE_OBJECT ||
        !state->objectAuthoring.attached) {
        return hit;
    }

    if (state->editor.objectEditSelectionMode == OBJECT_EDIT_SELECTION_EDGE) {
        exact = HitboxSystem_GetHitAtOfType(mouse_x, mouse_y, HITBOX_OBJECT_TOPOLOGY_EDGE);
        if (out_exact) *out_exact = exact;
        if (exact.type != HITBOX_NONE) {
            if (out_reason) *out_reason = VIEWPORT_PICK_REASON_EXACT_TOPOLOGY_EDGE;
            return exact;
        }
        if (hit.type == HITBOX_OBJECT_TOPOLOGY_VERTEX ||
            ViewportPick_IsObjectEditSuppressedHit(hit.type)) {
            if (out_reason) *out_reason = VIEWPORT_PICK_REASON_FILTERED_BY_OBJECT_EDIT_MODE;
            return ViewportPick_None();
        }
    } else if (state->editor.objectEditSelectionMode == OBJECT_EDIT_SELECTION_VERTEX) {
        exact = HitboxSystem_GetHitAtOfType(mouse_x, mouse_y, HITBOX_OBJECT_TOPOLOGY_VERTEX);
        if (out_exact) *out_exact = exact;
        if (exact.type != HITBOX_NONE) {
            if (out_reason) *out_reason = VIEWPORT_PICK_REASON_EXACT_TOPOLOGY_VERTEX;
            return exact;
        }
        if (hit.type == HITBOX_OBJECT_TOPOLOGY_EDGE ||
            ViewportPick_IsObjectEditSuppressedHit(hit.type)) {
            if (out_reason) *out_reason = VIEWPORT_PICK_REASON_FILTERED_BY_OBJECT_EDIT_MODE;
            return ViewportPick_None();
        }
    }

    return hit;
}

static void ViewportPick_RecordLast(int mouse_x,
                                    int mouse_y,
                                    ViewportPickResult result) {
    g_lastPickMouseX = mouse_x;
    g_lastPickMouseY = mouse_y;
    g_lastPickResult = result;
    g_lastPickValid = true;
}

const ViewportPickResult* ViewportPick_LastResult(int* out_mouse_x,
                                                  int* out_mouse_y) {
    if (!g_lastPickValid) return NULL;
    if (out_mouse_x) *out_mouse_x = g_lastPickMouseX;
    if (out_mouse_y) *out_mouse_y = g_lastPickMouseY;
    return &g_lastPickResult;
}

const char* ViewportPick_HitboxTypeLabel(HitboxType type) {
    switch (type) {
        case HITBOX_NONE: return "None";
        case HITBOX_SCENE_BOUNDS_GIZMO_AXIS: return "BoundsAxis";
        case HITBOX_GIZMO_AXIS: return "Axis";
        case HITBOX_OBJECT3D_GIZMO_AXIS: return "ObjAxis";
        case HITBOX_OBJECT_FACE_SKETCH_HANDLE: return "SketchHandle";
        case HITBOX_OBJECT_FACE_SKETCH_BODY: return "SketchBody";
        case HITBOX_OBJECT3D_PRISM_HANDLE: return "PrismHandle";
        case HITBOX_OBJECT3D_PLANE_CORNER: return "PlaneCorner";
        case HITBOX_OBJECT3D_PLANE_EDGE: return "PlaneEdge";
        case HITBOX_OBJECT_TOPOLOGY_VERTEX: return "TopoVertex";
        case HITBOX_OBJECT_TOPOLOGY_EDGE: return "TopoEdge";
        case HITBOX_SCENE_BOUNDS_HANDLE: return "BoundsHandle";
        case HITBOX_WALL: return "Wall";
        case HITBOX_POINT: return "Point";
        case HITBOX_HANDLE: return "Handle";
        case HITBOX_OBJECT3D: return "Object";
        default: return "?";
    }
}

const char* ViewportPick_PaneLaneLabel(PointerPaneLane lane) {
    switch (lane) {
        case POINTER_PANE_TOP: return "Top";
        case POINTER_PANE_LEFT: return "Left";
        case POINTER_PANE_RIGHT: return "Right";
        case POINTER_PANE_CENTER: return "Center";
        case POINTER_PANE_OUTSIDE: return "Outside";
        default: return "?";
    }
}

const char* ViewportPick_ReasonLabel(ViewportPickReason reason) {
    switch (reason) {
        case VIEWPORT_PICK_REASON_NONE: return "None";
        case VIEWPORT_PICK_REASON_OUTSIDE_CENTER_PANE: return "OutsideCenter";
        case VIEWPORT_PICK_REASON_RAW_HIT: return "Raw";
        case VIEWPORT_PICK_REASON_EXACT_TOPOLOGY_EDGE: return "ExactEdge";
        case VIEWPORT_PICK_REASON_EXACT_TOPOLOGY_VERTEX: return "ExactVertex";
        case VIEWPORT_PICK_REASON_FILTERED_BY_OBJECT_EDIT_MODE: return "ModeFiltered";
        case VIEWPORT_PICK_REASON_BODY_FALLBACK: return "BodyFallback";
        default: return "?";
    }
}

bool ViewportPick_FormatLastDebug(char* out,
                                  size_t out_size,
                                  const GlobalState* state) {
    int mouse_x = 0;
    int mouse_y = 0;
    const ViewportPickResult* pick = NULL;
    if (!out || out_size == 0u) return false;
    out[0] = '\0';
    pick = ViewportPick_LastResult(&mouse_x, &mouse_y);
    if (!pick) return false;
    snprintf(out,
             out_size,
             "Pick x%d y%d pane:%s raw:%s(%d,%d) exact:%s(%d,%d) final:%s(%d,%d) why:%s attach:%s topo:%s",
             mouse_x,
             mouse_y,
             ViewportPick_PaneLaneLabel(pick->paneLane),
             ViewportPick_HitboxTypeLabel(pick->rawHit.type),
             pick->rawHit.index,
             pick->rawHit.subIndex,
             ViewportPick_HitboxTypeLabel(pick->exactTopologyHit.type),
             pick->exactTopologyHit.index,
             pick->exactTopologyHit.subIndex,
             ViewportPick_HitboxTypeLabel(pick->finalHit.type),
             pick->finalHit.index,
             pick->finalHit.subIndex,
             ViewportPick_ReasonLabel(pick->reason),
             state && state->objectAuthoring.attached ? "Y" : "N",
             pick->objectEditTopologyModeActive ? "Y" : "N");
    return true;
}

ViewportPickResult ViewportPick_ResolveObjectWorkspaceHit(GlobalState* state,
                                                          int mouse_x,
                                                          int mouse_y,
                                                          bool require_center_pane) {
    ViewportPickResult result = {
        .paneLane = POINTER_PANE_OUTSIDE,
        .rawHit = { .type = HITBOX_NONE, .index = -1, .subIndex = -1 },
        .exactTopologyHit = { .type = HITBOX_NONE, .index = -1, .subIndex = -1 },
        .filteredHit = { .type = HITBOX_NONE, .index = -1, .subIndex = -1 },
        .bodyFallbackHit = { .type = HITBOX_NONE, .index = -1, .subIndex = -1 },
        .finalHit = { .type = HITBOX_NONE, .index = -1, .subIndex = -1 },
        .reason = VIEWPORT_PICK_REASON_NONE,
        .objectEditTopologyModeActive = false
    };
    ViewportPickReason filter_reason = VIEWPORT_PICK_REASON_RAW_HIT;

    if (!state) {
        ViewportPick_RecordLast(mouse_x, mouse_y, result);
        return result;
    }

    result.paneLane = ResolvePointerPaneLane(mouse_x, mouse_y);
    result.objectEditTopologyModeActive = InputMouse_ObjectEditTopologyModeActive(state);
    if (require_center_pane && result.paneLane != POINTER_PANE_CENTER) {
        result.reason = VIEWPORT_PICK_REASON_OUTSIDE_CENTER_PANE;
        ViewportPick_RecordLast(mouse_x, mouse_y, result);
        return result;
    }

    Global_RebuildHitboxesIfDirty();
    result.rawHit = HitboxSystem_GetHitAt(mouse_x, mouse_y);
    result.filteredHit = ViewportPick_FilterObjectEditModeHit(state,
                                                              mouse_x,
                                                              mouse_y,
                                                              result.rawHit,
                                                              &result.exactTopologyHit,
                                                              &filter_reason);
    result.finalHit = result.filteredHit;
    result.reason = filter_reason;

    if (!result.objectEditTopologyModeActive) {
        result.bodyFallbackHit = ResolveViewportObjectBodyHit(state,
                                                              mouse_x,
                                                              mouse_y,
                                                              result.filteredHit);
        if (result.bodyFallbackHit.type != result.filteredHit.type ||
            result.bodyFallbackHit.index != result.filteredHit.index ||
            result.bodyFallbackHit.subIndex != result.filteredHit.subIndex) {
            result.reason = VIEWPORT_PICK_REASON_BODY_FALLBACK;
        }
        result.finalHit = result.bodyFallbackHit;
    }

    ViewportPick_RecordLast(mouse_x, mouse_y, result);
    return result;
}
