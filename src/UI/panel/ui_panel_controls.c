#include "UI/ui_panel_internal.h"

#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Editor/object_face_extrude.h"

#include <SDL2/SDL.h>

static const CoreUnitKind kControlDisplayUnits[] = {
    CORE_UNIT_METER,
    CORE_UNIT_CENTIMETER,
    CORE_UNIT_MILLIMETER,
    CORE_UNIT_INCH,
    CORE_UNIT_FOOT
};

static bool UIPanelControls_IsSupportedDisplayUnit(CoreUnitKind unit) {
    for (size_t i = 0; i < sizeof(kControlDisplayUnits) / sizeof(kControlDisplayUnits[0]); ++i) {
        if (kControlDisplayUnits[i] == unit) return true;
    }
    return false;
}

bool UIPanel_BeginPrismWidthDialog(void) {
    return UIPanel_BeginPrismDimensionDialog(UI_PRISM_DIMENSION_TARGET_WIDTH);
}

bool UIPanel_BeginPrismHeightDialog(void) {
    return UIPanel_BeginPrismDimensionDialog(UI_PRISM_DIMENSION_TARGET_HEIGHT);
}

bool UIPanel_BeginPrismDepthDialog(void) {
    return UIPanel_BeginPrismDimensionDialog(UI_PRISM_DIMENSION_TARGET_DEPTH);
}

bool UIPanel_BeginSceneBoundsMinDialog(void) {
    return UIPanel_BeginSceneBoundsDialog(UI_SCENE_BOUNDS_TARGET_MIN);
}

bool UIPanel_BeginSceneBoundsMaxDialog(void) {
    return UIPanel_BeginSceneBoundsDialog(UI_SCENE_BOUNDS_TARGET_MAX);
}

bool UIPanel_SetConstructionPlaneAxis(ViewPlaneAxis axis) {
    GlobalState* state = Global_Get();
    ViewPlane plane = { .axis = VIEW_PLANE_XY, .offset = 0.0f };
    if (!state) return false;
    if (state->spaceMode != SPACE_MODE_3D) {
        SDL_Log("[UI] Construction plane controls require SPACE_MODE_3D.");
        return false;
    }
    plane = UIPanel_CurrentConstructionViewPlane(state);
    plane.axis = axis;
    state->activePlane = plane;
    Layout_ConstructionPlane3D_SetFromViewPlane(&state->layout.scene3d.constructionPlane, plane);
    Global_FlagHitboxesDirty();
    SDL_Log("[UI] Construction plane: %s (%s = %.2f)",
            UIPanel_ViewPlaneAxisLabel(plane.axis),
            UIPanel_ViewPlaneCoordinateLabel(plane.axis),
            plane.offset);
    return true;
}

bool UIPanel_AdjustConstructionPlaneOffset(float delta_world) {
    GlobalState* state = Global_Get();
    ViewPlane plane = { .axis = VIEW_PLANE_XY, .offset = 0.0f };
    if (!state) return false;
    if (state->spaceMode != SPACE_MODE_3D) {
        SDL_Log("[UI] Construction plane controls require SPACE_MODE_3D.");
        return false;
    }
    plane = UIPanel_CurrentConstructionViewPlane(state);
    plane.offset += delta_world;
    state->activePlane = plane;
    Layout_ConstructionPlane3D_SetFromViewPlane(&state->layout.scene3d.constructionPlane, plane);
    Global_FlagHitboxesDirty();
    SDL_Log("[UI] Construction plane offset: %s = %.2f",
            UIPanel_ViewPlaneCoordinateLabel(plane.axis),
            plane.offset);
    return true;
}

bool UIPanel_BeginConstructionPlaneOffsetDialog(void) {
    return UIPanel_BeginConstructionPlaneDialog(UI_CONSTRUCTION_PLANE_DIALOG_TARGET_OFFSET);
}

bool UIPanel_AdjustObjectExtrudeDepth(int direction) {
    GlobalState* state = Global_Get();
    float step = 0.0f;
    if (!state || direction == 0) return false;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return false;
    if (!state->editor.objectFaceExtrudeToolArmed) {
        SDL_Log("[UI] Extrude depth edit blocked: arm Extrude + or Extrude - first.");
        return false;
    }

    step = state->grid.gridSize > 0.0f ? state->grid.gridSize : 1.0f;
    if (direction < 0) step = -step;
    if (!Editor_ObjectFaceExtrudeAdjustDepth(state, step)) {
        SDL_Log("[UI] Extrude depth edit rejected.");
        return false;
    }
    SDL_Log("[UI] Extrude depth adjusted to %.4f", state->editor.objectFaceExtrudeDepth);
    return true;
}

bool UIPanel_BeginObjectPositionDialog(void) {
    return UIPanel_BeginObjectTransformDialog(UI_OBJECT_TRANSFORM_DIALOG_TARGET_POSITION);
}

bool UIPanel_BeginObjectRotationXDialog(void) {
    return UIPanel_BeginObjectTransformDialog(UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_X);
}

bool UIPanel_BeginObjectRotationYDialog(void) {
    return UIPanel_BeginObjectTransformDialog(UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_Y);
}

bool UIPanel_BeginObjectRotationZDialog(void) {
    return UIPanel_BeginObjectTransformDialog(UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_Z);
}

void UIPanel_CycleDisplayUnit(void) {
    UIPanelState* ui = UIPanel_Get();
    const size_t count = sizeof(kControlDisplayUnits) / sizeof(kControlDisplayUnits[0]);
    size_t currentIndex = 0u;
    for (size_t i = 0; i < count; ++i) {
        if (kControlDisplayUnits[i] == ui->displayUnit) {
            currentIndex = i;
            break;
        }
    }
    ui->displayUnit = kControlDisplayUnits[(currentIndex + 1u) % count];
    SDL_Log("[UI] Display units switched to %s (%s)",
            core_units_kind_name(ui->displayUnit),
            core_units_kind_symbol(ui->displayUnit));
}

bool UIPanel_SetDisplayUnit(CoreUnitKind unit) {
    UIPanelState* ui = UIPanel_Get();
    if (!UIPanelControls_IsSupportedDisplayUnit(unit)) return false;
    ui->displayUnit = unit;
    return true;
}

CoreUnitKind UIPanel_GetDisplayUnit(void) {
    return UIPanel_Get()->displayUnit;
}

const char* UIPanel_GetDisplayUnitSymbol(void) {
    const char* symbol = core_units_kind_symbol(UIPanel_Get()->displayUnit);
    return symbol ? symbol : "m";
}

bool UIPanel_ToggleObjectGizmoRotateMode(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (!state->editor.object3DRotateMode && !state->editor.object3DSizeMode) {
        state->editor.object3DRotateMode = true;
        state->editor.object3DSizeMode = false;
    } else if (state->editor.object3DRotateMode) {
        state->editor.object3DRotateMode = false;
        state->editor.object3DSizeMode = true;
    } else {
        state->editor.object3DRotateMode = false;
        state->editor.object3DSizeMode = false;
    }
    Global_FlagHitboxesDirty();
    SDL_Log("[UI] Object gizmo mode: %s",
            state->editor.object3DSizeMode ? "SIZE" :
            state->editor.object3DRotateMode ? "ROTATE" : "MOVE");
    return true;
}

bool UIPanel_IsObjectGizmoRotateMode(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    return state->editor.object3DRotateMode && !state->editor.object3DSizeMode;
}

bool UIPanel_IsObjectGizmoSizeMode(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    return state->editor.object3DSizeMode;
}

const char* UIPanel_ObjectGizmoModeLabel(void) {
    GlobalState* state = Global_Get();
    if (!state) return "Move";
    if (state->editor.object3DSizeMode) return "Size";
    if (state->editor.object3DRotateMode) return "Rotate";
    return "Move";
}

bool UIPanel_ToggleSceneBoundsEnabled(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    Editor_HistoryCapture(&state->editor, &state->layout);
    state->layout.scene3d.bounds.enabled = !state->layout.scene3d.bounds.enabled;
    Global_FlagLayoutChanged();
    SDL_Log("[UI] Scene bounds: %s",
            state->layout.scene3d.bounds.enabled ? "ENABLED" : "DISABLED");
    return true;
}

bool UIPanel_ToggleSceneBoundsClampOnEdit(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    Editor_HistoryCapture(&state->editor, &state->layout);
    state->layout.scene3d.bounds.clampOnEdit = !state->layout.scene3d.bounds.clampOnEdit;
    Global_FlagLayoutChanged();
    SDL_Log("[UI] Scene bounds clamp-on-edit: %s",
            state->layout.scene3d.bounds.clampOnEdit ? "ENABLED" : "DISABLED");
    return true;
}

bool UIPanel_ConvertWorldToDisplay(double world_value, double* out_display_value) {
    CoreResult r = core_units_world_to_unit(world_value,
                                            1.0,
                                            UIPanel_Get()->displayUnit,
                                            out_display_value);
    return out_display_value && r.code == CORE_OK;
}

bool UIPanel_ConvertDisplayToWorld(double display_value, double* out_world_value) {
    CoreResult r = core_units_unit_to_world(display_value,
                                            UIPanel_Get()->displayUnit,
                                            1.0,
                                            out_world_value);
    return out_world_value && r.code == CORE_OK;
}
