#include "input_editor_actions.h"

#include "Core/global_state.h"
#include "Core/line_drawing_pane_host.h"
#include "Core/space_mode_adapter.h"
#include "Core/viewport3d_bridge.h"
#include "Core/viewport_navigation_contract.h"
#include "Editor/editor.h"
#include "Layout/layout.h"
#include "Layout/scene/layout_object_faces.h"
#include "UI/ui_panel.h"

#include <SDL2/SDL.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

static bool InputEditorAction_ObjectWorldBounds(const Object3D* object,
                                                Vec3* out_min,
                                                Vec3* out_max) {
    Vec3 corners[8] = {0};
    size_t count = 0u;
    if (!object || object->isDeleted || !out_min || !out_max) return false;
    if (object->kind == OBJECT3D_KIND_PLANE) {
        if (!Layout_Object3D_ComputePlaneCorners(object, corners)) return false;
        count = 4u;
    } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
        if (!Layout_Object3D_ComputeRectPrismCorners(object, corners)) return false;
        count = 8u;
    } else if (object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
        if (!Layout_Object3D_ComputeMeshInstanceCorners(object, corners)) return false;
        count = 8u;
    } else {
        return false;
    }

    *out_min = corners[0];
    *out_max = corners[0];
    for (size_t i = 1u; i < count; ++i) {
        if (corners[i].x < out_min->x) out_min->x = corners[i].x;
        if (corners[i].y < out_min->y) out_min->y = corners[i].y;
        if (corners[i].z < out_min->z) out_min->z = corners[i].z;
        if (corners[i].x > out_max->x) out_max->x = corners[i].x;
        if (corners[i].y > out_max->y) out_max->y = corners[i].y;
        if (corners[i].z > out_max->z) out_max->z = corners[i].z;
    }
    return true;
}

static bool InputEditorAction_LayoutObjectWorldBounds(const Layout* layout,
                                                      uint32_t preferred_object_id,
                                                      Vec3* out_min,
                                                      Vec3* out_max) {
    bool found = false;
    if (!layout || !out_min || !out_max) return false;
    if (preferred_object_id != 0u) {
        const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore,
                                                              preferred_object_id);
        if (InputEditorAction_ObjectWorldBounds(object, out_min, out_max)) return true;
    }
    for (size_t i = 0u; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        Vec3 min = {0};
        Vec3 max = {0};
        if (!InputEditorAction_ObjectWorldBounds(object, &min, &max)) continue;
        if (!found) {
            *out_min = min;
            *out_max = max;
            found = true;
        } else {
            if (min.x < out_min->x) out_min->x = min.x;
            if (min.y < out_min->y) out_min->y = min.y;
            if (min.z < out_min->z) out_min->z = min.z;
            if (max.x > out_max->x) out_max->x = max.x;
            if (max.y > out_max->y) out_max->y = max.y;
            if (max.z > out_max->z) out_max->z = max.z;
        }
    }
    return found;
}

static Vec3 InputEditorAction_BoundsCenter(Vec3 min, Vec3 max) {
    return (Vec3){
        .x = (min.x + max.x) * 0.5f,
        .y = (min.y + max.y) * 0.5f,
        .z = (min.z + max.z) * 0.5f
    };
}

static bool InputEditorAction_GetViewportRect(const GlobalState* state, CorePaneRect* out_rect) {
    if (!state || !out_rect) return false;
    if (state->paneHost.initialized &&
        LineDrawingPaneHost_GetViewportRect(&state->paneHost, out_rect) &&
        out_rect->width > 1.0f &&
        out_rect->height > 1.0f) {
        return true;
    }
    *out_rect = (CorePaneRect){0.0f, 0.0f, (float)state->screenWidth, (float)state->screenHeight};
    return out_rect->width > 1.0f && out_rect->height > 1.0f;
}

static bool InputEditorAction_ResolveFreeViewFitScale(GlobalState* state,
                                                      Vec3 min,
                                                      Vec3 max,
                                                      float* out_scale) {
    CorePaneRect viewport = {0};
    SpaceViewContext view_ctx = {0};
    float min_x = FLT_MAX;
    float min_y = FLT_MAX;
    float max_x = -FLT_MAX;
    float max_y = -FLT_MAX;
    const Vec3 corners[8] = {
        { min.x, min.y, min.z }, { max.x, min.y, min.z },
        { min.x, max.y, min.z }, { max.x, max.y, min.z },
        { min.x, min.y, max.z }, { max.x, min.y, max.z },
        { min.x, max.y, max.z }, { max.x, max.y, max.z }
    };
    if (!state || !out_scale || state->grid.gridSize <= 0.0f) return false;
    if (!InputEditorAction_GetViewportRect(state, &viewport)) return false;
    view_ctx = SpaceAdapter_BuildViewContext(state);
    for (size_t i = 0u; i < 8u; ++i) {
        const Vec2 projected = SpaceAdapter_ProjectToView(corners[i], &view_ctx);
        if (projected.x < min_x) min_x = projected.x;
        if (projected.x > max_x) max_x = projected.x;
        if (projected.y < min_y) min_y = projected.y;
        if (projected.y > max_y) max_y = projected.y;
    }
    {
        const float span_x = max_x - min_x;
        const float span_y = max_y - min_y;
        const float usable_w = viewport.width * 0.84f;
        const float usable_h = viewport.height * 0.84f;
        float fit_scale = GRID_DEFAULT_MAX_SCALE;
        if (span_x > 0.0001f) {
            fit_scale = fminf(fit_scale, usable_w / (state->grid.gridSize * span_x));
        }
        if (span_y > 0.0001f) {
            fit_scale = fminf(fit_scale, usable_h / (state->grid.gridSize * span_y));
        }
        if (!isfinite(fit_scale) || fit_scale <= 0.0f) return false;
        if (fit_scale < 0.01f) fit_scale = 0.01f;
        if (fit_scale > GRID_DEFAULT_MAX_SCALE) fit_scale = GRID_DEFAULT_MAX_SCALE;
        *out_scale = fit_scale;
    }
    return true;
}

static bool InputEditorAction_FrameFreeViewCamera(GlobalState* state) {
    Vec3 min = {0};
    Vec3 max = {0};
    Vec3 target = {0};
    bool has_anchors = false;
    bool has_bounds = false;
    float fit_scale = 0.0f;
    CorePaneRect viewport = {0};
    CoreViewport3DCommand command = {0};
    FreeViewCamera next_camera;
    Grid next_grid;
    if (!state || !InputEditorAction_GetViewportRect(state, &viewport)) return false;
    has_bounds = InputEditorAction_LayoutObjectWorldBounds(&state->layout,
                                                           state->editor.selectedObject3DId,
                                                           &min,
                                                           &max);
    if (has_bounds) {
        target = InputEditorAction_BoundsCenter(min, max);
        if (!InputEditorAction_ResolveFreeViewFitScale(state, min, max, &fit_scale)) return false;
    } else {
        target = Layout_ComputeCentroid(&state->layout, &has_anchors);
        if (!has_anchors) target = state->freeViewCamera.target;
        fit_scale = state->grid.scale;
    }
    command.kind = CORE_VIEWPORT3D_COMMAND_FRAME;
    command.value.frame.target = (CoreViewport3DVec3d){
        (double)target.x, (double)target.y, (double)target.z
    };
    command.value.frame.scale_px_per_world_unit =
        (double)state->grid.gridSize * (double)fit_scale;
    next_camera = state->freeViewCamera;
    next_grid = state->grid;
    if (!LineDrawingViewport3DBridgeApply(
            &state->freeViewCamera,
            &state->grid,
            (double)viewport.x + (double)viewport.width * 0.5,
            (double)viewport.y + (double)viewport.height * 0.5,
            0.01,
            (double)GRID_DEFAULT_MAX_SCALE,
            &command,
            &next_camera,
            &next_grid)) return false;
    state->freeViewCamera = next_camera;
    state->grid = next_grid;
    Global_FlagHitboxesDirty();
    return true;
}

bool InputEditorAction_FrameViewport(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (state->spaceMode == SPACE_MODE_3D && state->freeViewCamera.enabled) {
        return InputEditorAction_FrameFreeViewCamera(state);
    }
    {
        CorePaneRect viewport = {0};
        bool has_anchors = false;
        const Vec3 center = Layout_ComputeCentroid(&state->layout, &has_anchors);
        if (!has_anchors || !InputEditorAction_GetViewportRect(state, &viewport) ||
            state->grid.gridSize <= 0.0f || state->grid.scale <= 0.0f) {
            return false;
        }
        state->grid.offsetX = center.x -
                              (viewport.x + viewport.width * 0.5f) /
                                  (state->grid.gridSize * state->grid.scale);
        state->grid.offsetY = center.y -
                              (viewport.y + viewport.height * 0.5f) /
                                  (state->grid.gridSize * state->grid.scale);
        Global_FlagGridChanged();
        return true;
    }
}

static void InputEditorAction_InitializeFreeViewCamera(GlobalState* state) {
    if (!state) return;
    if (!InputEditorAction_FrameFreeViewCamera(state)) {
        Global_FlagHitboxesDirty();
    }
}

static const char* InputEditorAction_PlaneAxisLabel(ViewPlaneAxis axis) {
    switch (axis) {
        case VIEW_PLANE_YZ: return "YZ";
        case VIEW_PLANE_XZ: return "XZ";
        case VIEW_PLANE_XY:
        default: return "XY";
    }
}

static const char* InputEditorAction_PlaneCoordLabel(ViewPlaneAxis axis) {
    switch (axis) {
        case VIEW_PLANE_YZ: return "x";
        case VIEW_PLANE_XZ: return "y";
        case VIEW_PLANE_XY:
        default: return "z";
    }
}

static void InputEditorAction_SetActivePlane(GlobalState* state, ViewPlane plane) {
    if (!state) return;
    state->activePlane = plane;
    Layout_ConstructionPlane3D_SetFromViewPlane(&state->layout.scene3d.constructionPlane, plane);
    Global_FlagHitboxesDirty();
}

bool InputEditorAction_ToggleSpaceMode(bool persist) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (!Global_ToggleSpaceMode(persist)) return false;
    printf("[Editor] Space mode: %s\n", Global_GetSpaceModeLabel(state->spaceMode));
    return true;
}

bool InputEditorAction_ToggleFreeView(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;

    if (state->spaceMode != SPACE_MODE_3D) {
        if (!Global_SetSpaceMode(SPACE_MODE_3D, true)) return false;
    }

    state->freeViewCamera.enabled = !state->freeViewCamera.enabled;
    if (state->freeViewCamera.enabled) {
        InputEditorAction_InitializeFreeViewCamera(state);
    }
    Global_FlagHitboxesDirty();
    printf("[Editor] View mode: %s\n", state->freeViewCamera.enabled ? "FREE_VIEW" : "PLANE_VIEW");
    return true;
}

bool InputEditorAction_ToggleObjectGizmoMode(void) {
    if (!UIPanel_ToggleObjectGizmoRotateMode()) return false;
    printf("[Editor] Object gizmo mode: %s\n", UIPanel_ObjectGizmoModeLabel());
    return true;
}

bool InputEditorAction_CycleActivePlane(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (state->spaceMode != SPACE_MODE_3D) {
        printf("[Editor] Plane controls require SPACE_MODE_3D.\n");
        return false;
    }

    ViewPlane plane = state->activePlane;
    switch (plane.axis) {
        case VIEW_PLANE_XY:
            plane.axis = VIEW_PLANE_YZ;
            break;
        case VIEW_PLANE_YZ:
            plane.axis = VIEW_PLANE_XZ;
            break;
        case VIEW_PLANE_XZ:
        default:
            plane.axis = VIEW_PLANE_XY;
            break;
    }
    InputEditorAction_SetActivePlane(state, plane);
    printf("[Editor] Active plane: %s (%s = %.2f)\n",
           InputEditorAction_PlaneAxisLabel(state->activePlane.axis),
           InputEditorAction_PlaneCoordLabel(state->activePlane.axis),
           state->activePlane.offset);
    return true;
}

bool InputEditorAction_Undo(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (!Editor_Undo(&state->editor, &state->layout)) return false;
    Global_FlagHitboxesDirty();
    return true;
}

bool InputEditorAction_Redo(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (!Editor_Redo(&state->editor, &state->layout)) return false;
    Global_FlagHitboxesDirty();
    return true;
}
