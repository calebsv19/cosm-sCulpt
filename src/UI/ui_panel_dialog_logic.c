#include "UI/ui_panel_internal.h"

#include "Core/global_state.h"
#include "Editor/editor.h"

#include <SDL2/SDL.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const CoreUnitKind kDialogDisplayUnits[] = {
    CORE_UNIT_METER,
    CORE_UNIT_CENTIMETER,
    CORE_UNIT_MILLIMETER,
    CORE_UNIT_INCH,
    CORE_UNIT_FOOT
};

static bool UIPanelDialog_IsSupportedDisplayUnit(CoreUnitKind unit) {
    for (size_t i = 0; i < sizeof(kDialogDisplayUnits) / sizeof(kDialogDisplayUnits[0]); ++i) {
        if (kDialogDisplayUnits[i] == unit) return true;
    }
    return false;
}

static bool UIPanelDialog_ConvertWorldToDisplayWithUnit(CoreUnitKind unit,
                                                        double world_value,
                                                        double* out_display_value) {
    if (!UIPanelDialog_IsSupportedDisplayUnit(unit) || !out_display_value) return false;
    CoreResult r = core_units_world_to_unit(world_value, 1.0, unit, out_display_value);
    return r.code == CORE_OK;
}

static bool UIPanelDialog_ConvertDisplayToWorldWithUnit(CoreUnitKind unit,
                                                        double display_value,
                                                        double* out_world_value) {
    if (!UIPanelDialog_IsSupportedDisplayUnit(unit) || !out_world_value) return false;
    CoreResult r = core_units_unit_to_world(display_value, unit, 1.0, out_world_value);
    return r.code == CORE_OK;
}

static void SanitizeBuffer(char* buffer) {
    size_t len = strlen(buffer);
    while (len > 0 && isspace((unsigned char)buffer[len - 1])) {
        buffer[--len] = '\0';
    }
}

static void UIPanel_StopTextInputIfIdle(UIPanelState* ui) {
    if (!ui) return;
    if (!ui->saveDialog.active &&
        !ui->rootDialog.active &&
        !ui->prismDimensionDialog.active &&
        !ui->sceneBoundsDialog.active &&
        !ui->constructionPlaneDialog.active &&
        !ui->objectTransformDialog.active &&
        SDL_IsTextInputActive()) {
        SDL_StopTextInput();
    }
}

void UIPanel_CloseRootDialog(UIPanelState* ui) {
    if (!ui || !ui->rootDialog.active) return;
    ui->rootDialog.active = false;
    ui->rootDialog.target = UI_ROOT_TARGET_NONE;
    ui->rootDialog.buffer[0] = '\0';
    ui->rootDialog.length = 0;
    ui->rootDialog.cursor = 0;
    UIPanel_StopTextInputIfIdle(ui);
}

void UIPanel_CloseSaveDialog(UIPanelState* ui) {
    if (!ui || !ui->saveDialog.active) return;
    ui->saveDialog.active = false;
    ui->saveDialog.buffer[0] = '\0';
    ui->saveDialog.length = 0;
    ui->saveDialog.cursor = 0;
    UIPanel_StopTextInputIfIdle(ui);
}

void UIPanel_ClosePrismDimensionDialog(UIPanelState* ui) {
    if (!ui || !ui->prismDimensionDialog.active) return;
    ui->prismDimensionDialog.active = false;
    ui->prismDimensionDialog.target = UI_PRISM_DIMENSION_TARGET_NONE;
    ui->prismDimensionDialog.objectId = 0u;
    ui->prismDimensionDialog.buffer[0] = '\0';
    ui->prismDimensionDialog.length = 0;
    ui->prismDimensionDialog.cursor = 0;
    UIPanel_StopTextInputIfIdle(ui);
}

void UIPanel_CloseSceneBoundsDialog(UIPanelState* ui) {
    if (!ui || !ui->sceneBoundsDialog.active) return;
    ui->sceneBoundsDialog.active = false;
    ui->sceneBoundsDialog.target = UI_SCENE_BOUNDS_TARGET_NONE;
    ui->sceneBoundsDialog.buffer[0] = '\0';
    ui->sceneBoundsDialog.length = 0;
    ui->sceneBoundsDialog.cursor = 0;
    UIPanel_StopTextInputIfIdle(ui);
}

void UIPanel_CloseConstructionPlaneDialog(UIPanelState* ui) {
    if (!ui || !ui->constructionPlaneDialog.active) return;
    ui->constructionPlaneDialog.active = false;
    ui->constructionPlaneDialog.target = UI_CONSTRUCTION_PLANE_DIALOG_TARGET_NONE;
    ui->constructionPlaneDialog.buffer[0] = '\0';
    ui->constructionPlaneDialog.length = 0;
    ui->constructionPlaneDialog.cursor = 0;
    UIPanel_StopTextInputIfIdle(ui);
}

void UIPanel_CloseObjectTransformDialog(UIPanelState* ui) {
    if (!ui || !ui->objectTransformDialog.active) return;
    ui->objectTransformDialog.active = false;
    ui->objectTransformDialog.target = UI_OBJECT_TRANSFORM_DIALOG_TARGET_NONE;
    ui->objectTransformDialog.objectId = 0u;
    ui->objectTransformDialog.buffer[0] = '\0';
    ui->objectTransformDialog.length = 0;
    ui->objectTransformDialog.cursor = 0;
    UIPanel_StopTextInputIfIdle(ui);
}

static Object3D* UIPanel_FindEditableRectPrism(GlobalState* state, uint32_t objectId) {
    if (!state || objectId == 0u) return NULL;
    Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, objectId);
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM) return NULL;
    if (!Layout_ObjectStore_ValidateObject(object)) return NULL;
    return object;
}

static Object3D* UIPanel_FindEditableObject3D(GlobalState* state, uint32_t objectId) {
    if (!state || objectId == 0u) return NULL;
    Object3D* object = Layout_ObjectStore_Find(&state->layout.objectStore, objectId);
    if (!object) return NULL;
    if (object->kind != OBJECT3D_KIND_PLANE &&
        object->kind != OBJECT3D_KIND_RECT_PRISM) {
        return NULL;
    }
    if (!Layout_ObjectStore_ValidateObject(object)) return NULL;
    return object;
}

const char* UIPanel_PrismDimensionTargetLabel(UIPrismDimensionDialogTarget target) {
    switch (target) {
        case UI_PRISM_DIMENSION_TARGET_WIDTH: return "Width";
        case UI_PRISM_DIMENSION_TARGET_HEIGHT: return "Height";
        case UI_PRISM_DIMENSION_TARGET_DEPTH: return "Depth";
        case UI_PRISM_DIMENSION_TARGET_NONE:
        default: return "Dimension";
    }
}

const char* UIPanel_SceneBoundsTargetLabel(UISceneBoundsDialogTarget target) {
    switch (target) {
        case UI_SCENE_BOUNDS_TARGET_MIN: return "Scene Bounds Min";
        case UI_SCENE_BOUNDS_TARGET_MAX: return "Scene Bounds Max";
        case UI_SCENE_BOUNDS_TARGET_NONE:
        default: return "Scene Bounds";
    }
}

const char* UIPanel_ObjectTransformTargetLabel(UIObjectTransformDialogTarget target) {
    switch (target) {
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_POSITION: return "Object Position";
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_X: return "Rotate X";
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_Y: return "Rotate Y";
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_Z: return "Rotate Z";
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_NONE:
        default: return "Object Transform";
    }
}

static Vec3 UIPanel_ObjectTransformTargetAxis(UIObjectTransformDialogTarget target) {
    switch (target) {
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_X: return (Vec3){ 1.0f, 0.0f, 0.0f };
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_Y: return (Vec3){ 0.0f, 1.0f, 0.0f };
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_Z: return (Vec3){ 0.0f, 0.0f, 1.0f };
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_POSITION:
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_NONE:
        default: return (Vec3){ 0.0f, 0.0f, 0.0f };
    }
}

static float UIPanel_ObjectRotationAxisValue(const Object3D* object,
                                             UIObjectTransformDialogTarget target) {
    if (!object) return 0.0f;
    switch (target) {
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_X: return object->transform.rotationDeg.x;
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_Y: return object->transform.rotationDeg.y;
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_ROTATION_Z: return object->transform.rotationDeg.z;
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_POSITION:
        case UI_OBJECT_TRANSFORM_DIALOG_TARGET_NONE:
        default: return 0.0f;
    }
}

static bool UIPanel_ParseVec3Text(const char* text, Vec3* out) {
    char temp[128];
    char* p = NULL;
    char* end = NULL;
    float values[3] = {0.0f, 0.0f, 0.0f};
    int parsed = 0;

    if (!text || !out) return false;
    snprintf(temp, sizeof(temp), "%s", text);
    for (size_t i = 0; temp[i] != '\0'; ++i) {
        if (temp[i] == ',' || temp[i] == ';') temp[i] = ' ';
    }

    p = temp;
    while (*p && parsed < 3) {
        while (*p && isspace((unsigned char)*p)) ++p;
        if (!*p) break;
        errno = 0;
        values[parsed] = strtof(p, &end);
        if (end == p || errno == ERANGE || !isfinite(values[parsed])) return false;
        parsed++;
        p = end;
    }
    while (*p && isspace((unsigned char)*p)) ++p;
    if (*p != '\0') return false;
    if (parsed != 3) return false;

    out->x = values[0];
    out->y = values[1];
    out->z = values[2];
    return true;
}

bool UIPanel_BeginPrismDimensionDialog(UIPrismDimensionDialogTarget target) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    Object3D* object = NULL;
    double worldValue = 0.0;
    double displayValue = 0.0;

    if (!state) return false;
    if (target == UI_PRISM_DIMENSION_TARGET_NONE) return false;
    object = UIPanel_FindEditableRectPrism(state, state->editor.selectedObject3DId);
    if (!object) {
        SDL_Log("[UI] Prism dimension edit blocked: select a valid rect prism first.");
        return false;
    }

    if (target == UI_PRISM_DIMENSION_TARGET_WIDTH) worldValue = (double)object->rectPrism.width;
    else if (target == UI_PRISM_DIMENSION_TARGET_HEIGHT) worldValue = (double)object->rectPrism.height;
    else worldValue = (double)object->rectPrism.depth;
    if (!UIPanelDialog_ConvertWorldToDisplayWithUnit(ui->displayUnit, worldValue, &displayValue)) {
        displayValue = worldValue;
    }

    ui->prismDimensionDialog.active = true;
    ui->prismDimensionDialog.target = target;
    ui->prismDimensionDialog.objectId = object->objectId;
    snprintf(ui->prismDimensionDialog.buffer,
             sizeof(ui->prismDimensionDialog.buffer),
             "%.4f",
             displayValue);
    SanitizeBuffer(ui->prismDimensionDialog.buffer);
    ui->prismDimensionDialog.length = strlen(ui->prismDimensionDialog.buffer);
    ui->prismDimensionDialog.cursor = ui->prismDimensionDialog.length;

    ui->loadMenu.open = false;
    UIPanel_CloseSaveDialog(ui);
    UIPanel_CloseRootDialog(ui);
    UIPanel_CloseSceneBoundsDialog(ui);
    UIPanel_CloseConstructionPlaneDialog(ui);
    UIPanel_CloseObjectTransformDialog(ui);
    if (!SDL_IsTextInputActive()) SDL_StartTextInput();
    return true;
}

static bool UIPanel_ParsePositiveFloat(const char* text, float* outValue) {
    char* end = NULL;
    float value = 0.0f;
    if (!text || !outValue) return false;

    errno = 0;
    value = strtof(text, &end);
    if (errno == ERANGE) return false;
    if (end == text) return false;
    while (*end && isspace((unsigned char)*end)) ++end;
    if (*end != '\0') return false;
    if (!isfinite(value)) return false;

    *outValue = value;
    return true;
}

static bool UIPanel_ParseScalarFloat(const char* text, float* outValue) {
    char* end = NULL;
    float value = 0.0f;
    if (!text || !outValue) return false;

    errno = 0;
    value = strtof(text, &end);
    if (errno == ERANGE) return false;
    if (end == text) return false;
    while (*end && isspace((unsigned char)*end)) ++end;
    if (*end != '\0') return false;
    if (!isfinite(value)) return false;

    *outValue = value;
    return true;
}

bool UIPanel_ApplyPrismDimensionDialog(UIPanelState* ui) {
    GlobalState* state = NULL;
    Object3D* object = NULL;
    float typedValue = 0.0f;
    double worldValue = 0.0;
    float nextWidth = 0.0f;
    float nextHeight = 0.0f;
    float nextDepth = 0.0f;
    bool boundsAdjusted = false;

    if (!ui || !ui->prismDimensionDialog.active) return false;
    state = Global_Get();
    if (!state) return false;
    object = UIPanel_FindEditableRectPrism(state, ui->prismDimensionDialog.objectId);
    if (!object) {
        SDL_Log("[UI] Prism dimension edit failed: selected prism is unavailable.");
        return false;
    }

    SanitizeBuffer(ui->prismDimensionDialog.buffer);
    ui->prismDimensionDialog.length = strlen(ui->prismDimensionDialog.buffer);
    if (ui->prismDimensionDialog.cursor > ui->prismDimensionDialog.length) {
        ui->prismDimensionDialog.cursor = ui->prismDimensionDialog.length;
    }
    if (!UIPanel_ParsePositiveFloat(ui->prismDimensionDialog.buffer, &typedValue)) {
        SDL_Log("[UI] Prism %s edit failed: value must be numeric.",
                UIPanel_PrismDimensionTargetLabel(ui->prismDimensionDialog.target));
        return false;
    }
    if (!UIPanelDialog_ConvertDisplayToWorldWithUnit(ui->displayUnit, (double)typedValue, &worldValue)) {
        SDL_Log("[UI] Prism %s edit failed: unit conversion rejected for '%s'.",
                UIPanel_PrismDimensionTargetLabel(ui->prismDimensionDialog.target),
                core_units_kind_symbol(ui->displayUnit));
        return false;
    }
    if (!isfinite(worldValue)) {
        SDL_Log("[UI] Prism %s edit failed: converted value is not finite.",
                UIPanel_PrismDimensionTargetLabel(ui->prismDimensionDialog.target));
        return false;
    }

    nextWidth = object->rectPrism.width;
    nextHeight = object->rectPrism.height;
    nextDepth = object->rectPrism.depth;
    switch (ui->prismDimensionDialog.target) {
        case UI_PRISM_DIMENSION_TARGET_WIDTH: nextWidth = (float)worldValue; break;
        case UI_PRISM_DIMENSION_TARGET_HEIGHT: nextHeight = (float)worldValue; break;
        case UI_PRISM_DIMENSION_TARGET_DEPTH: nextDepth = (float)worldValue; break;
        case UI_PRISM_DIMENSION_TARGET_NONE:
        default: return false;
    }

    Editor_HistoryCapture(&state->editor, &state->layout);
    if (!Layout_SetRectPrismDimensions(&state->layout,
                                       object->objectId,
                                       nextWidth,
                                       nextHeight,
                                       nextDepth,
                                       &boundsAdjusted)) {
        SDL_Log("[UI] Prism %s edit rejected by layout bounds/validation policy.",
                UIPanel_PrismDimensionTargetLabel(ui->prismDimensionDialog.target));
        return false;
    }

    state->editor.selectedObject3DId = object->objectId;
    Global_FlagHitboxesDirty();
    SDL_Log("[UI] Prism #%u %s updated to %.4f%s (%.4f m)%s",
            object->objectId,
            UIPanel_PrismDimensionTargetLabel(ui->prismDimensionDialog.target),
            typedValue,
            core_units_kind_symbol(ui->displayUnit),
            worldValue,
            boundsAdjusted ? " (bounds-adjusted)" : "");
    UIPanel_ClosePrismDimensionDialog(ui);
    return true;
}

bool UIPanel_BeginSceneBoundsDialog(UISceneBoundsDialogTarget target) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    Vec3 worldValue = {0.0f, 0.0f, 0.0f};
    double dx = 0.0, dy = 0.0, dz = 0.0;

    if (!state) return false;
    if (target == UI_SCENE_BOUNDS_TARGET_NONE) return false;

    worldValue = (target == UI_SCENE_BOUNDS_TARGET_MIN)
        ? state->layout.scene3d.bounds.min
        : state->layout.scene3d.bounds.max;
    if (!UIPanelDialog_ConvertWorldToDisplayWithUnit(ui->displayUnit, (double)worldValue.x, &dx)) dx = worldValue.x;
    if (!UIPanelDialog_ConvertWorldToDisplayWithUnit(ui->displayUnit, (double)worldValue.y, &dy)) dy = worldValue.y;
    if (!UIPanelDialog_ConvertWorldToDisplayWithUnit(ui->displayUnit, (double)worldValue.z, &dz)) dz = worldValue.z;

    ui->sceneBoundsDialog.active = true;
    ui->sceneBoundsDialog.target = target;
    snprintf(ui->sceneBoundsDialog.buffer,
             sizeof(ui->sceneBoundsDialog.buffer),
             "%.4f, %.4f, %.4f",
             dx, dy, dz);
    SanitizeBuffer(ui->sceneBoundsDialog.buffer);
    ui->sceneBoundsDialog.length = strlen(ui->sceneBoundsDialog.buffer);
    ui->sceneBoundsDialog.cursor = ui->sceneBoundsDialog.length;

    ui->loadMenu.open = false;
    UIPanel_CloseSaveDialog(ui);
    UIPanel_CloseRootDialog(ui);
    UIPanel_ClosePrismDimensionDialog(ui);
    UIPanel_CloseConstructionPlaneDialog(ui);
    UIPanel_CloseObjectTransformDialog(ui);
    if (!SDL_IsTextInputActive()) SDL_StartTextInput();
    return true;
}

bool UIPanel_ApplySceneBoundsDialog(UIPanelState* ui) {
    GlobalState* state = NULL;
    SceneBounds3D next = {0};
    Vec3 typedDisplay = {0};
    double wx = 0.0, wy = 0.0, wz = 0.0;

    if (!ui || !ui->sceneBoundsDialog.active) return false;
    state = Global_Get();
    if (!state) return false;

    SanitizeBuffer(ui->sceneBoundsDialog.buffer);
    ui->sceneBoundsDialog.length = strlen(ui->sceneBoundsDialog.buffer);
    if (ui->sceneBoundsDialog.cursor > ui->sceneBoundsDialog.length) {
        ui->sceneBoundsDialog.cursor = ui->sceneBoundsDialog.length;
    }
    if (!UIPanel_ParseVec3Text(ui->sceneBoundsDialog.buffer, &typedDisplay)) {
        SDL_Log("[UI] %s edit failed: expected three numeric values (x, y, z).",
                UIPanel_SceneBoundsTargetLabel(ui->sceneBoundsDialog.target));
        return false;
    }
    if (!UIPanelDialog_ConvertDisplayToWorldWithUnit(ui->displayUnit, typedDisplay.x, &wx) ||
        !UIPanelDialog_ConvertDisplayToWorldWithUnit(ui->displayUnit, typedDisplay.y, &wy) ||
        !UIPanelDialog_ConvertDisplayToWorldWithUnit(ui->displayUnit, typedDisplay.z, &wz)) {
        SDL_Log("[UI] %s edit failed: unit conversion failed.",
                UIPanel_SceneBoundsTargetLabel(ui->sceneBoundsDialog.target));
        return false;
    }

    next = state->layout.scene3d.bounds;
    if (ui->sceneBoundsDialog.target == UI_SCENE_BOUNDS_TARGET_MIN) {
        next.min = (Vec3){ (float)wx, (float)wy, (float)wz };
    } else if (ui->sceneBoundsDialog.target == UI_SCENE_BOUNDS_TARGET_MAX) {
        next.max = (Vec3){ (float)wx, (float)wy, (float)wz };
    } else {
        return false;
    }

    if (!Layout_SceneBounds3D_IsValid(&next)) {
        SDL_Log("[UI] %s edit failed: bounds require min <= max on all axes.",
                UIPanel_SceneBoundsTargetLabel(ui->sceneBoundsDialog.target));
        return false;
    }

    Editor_HistoryCapture(&state->editor, &state->layout);
    state->layout.scene3d.bounds = next;
    Global_FlagLayoutChanged();
    SDL_Log("[UI] %s updated to (%.4f, %.4f, %.4f) %s",
            UIPanel_SceneBoundsTargetLabel(ui->sceneBoundsDialog.target),
            typedDisplay.x,
            typedDisplay.y,
            typedDisplay.z,
            UIPanel_GetDisplayUnitSymbol());

    UIPanel_CloseSceneBoundsDialog(ui);
    return true;
}

bool UIPanel_BeginConstructionPlaneDialog(UIConstructionPlaneDialogTarget target) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    ViewPlane plane = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f };
    double displayOffset = 0.0;

    if (!state) return false;
    if (target != UI_CONSTRUCTION_PLANE_DIALOG_TARGET_OFFSET) return false;
    if (state->spaceMode != SPACE_MODE_3D) {
        SDL_Log("[UI] Construction plane controls require SPACE_MODE_3D.");
        return false;
    }

    plane = UIPanel_CurrentConstructionViewPlane(state);
    if (!UIPanelDialog_ConvertWorldToDisplayWithUnit(ui->displayUnit,
                                                     (double)plane.offset,
                                                     &displayOffset)) {
        displayOffset = (double)plane.offset;
    }

    ui->loadMenu.open = false;
    UIPanel_CloseSaveDialog(ui);
    UIPanel_CloseRootDialog(ui);
    UIPanel_ClosePrismDimensionDialog(ui);
    UIPanel_CloseSceneBoundsDialog(ui);
    UIPanel_CloseConstructionPlaneDialog(ui);
    UIPanel_CloseObjectTransformDialog(ui);

    ui->constructionPlaneDialog.active = true;
    ui->constructionPlaneDialog.target = target;
    snprintf(ui->constructionPlaneDialog.buffer,
             sizeof(ui->constructionPlaneDialog.buffer),
             "%.4f",
             displayOffset);
    SanitizeBuffer(ui->constructionPlaneDialog.buffer);
    ui->constructionPlaneDialog.length = strlen(ui->constructionPlaneDialog.buffer);
    ui->constructionPlaneDialog.cursor = ui->constructionPlaneDialog.length;
    if (!SDL_IsTextInputActive()) SDL_StartTextInput();
    return true;
}

bool UIPanel_BeginObjectTransformDialog(UIObjectTransformDialogTarget target) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    Object3D* object = NULL;
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;

    if (!state) return false;
    if (target == UI_OBJECT_TRANSFORM_DIALOG_TARGET_NONE) return false;

    object = UIPanel_FindEditableObject3D(state, state->editor.selectedObject3DId);
    if (!object) {
        SDL_Log("[UI] Object transform edit blocked: select a valid 3D object first.");
        return false;
    }

    ui->loadMenu.open = false;
    UIPanel_CloseSaveDialog(ui);
    UIPanel_CloseRootDialog(ui);
    UIPanel_ClosePrismDimensionDialog(ui);
    UIPanel_CloseSceneBoundsDialog(ui);
    UIPanel_CloseConstructionPlaneDialog(ui);
    UIPanel_CloseObjectTransformDialog(ui);

    ui->objectTransformDialog.active = true;
    ui->objectTransformDialog.target = target;
    ui->objectTransformDialog.objectId = object->objectId;

    if (target == UI_OBJECT_TRANSFORM_DIALOG_TARGET_POSITION) {
        if (!UIPanelDialog_ConvertWorldToDisplayWithUnit(ui->displayUnit, (double)object->transform.position.x, &dx)) {
            dx = object->transform.position.x;
        }
        if (!UIPanelDialog_ConvertWorldToDisplayWithUnit(ui->displayUnit, (double)object->transform.position.y, &dy)) {
            dy = object->transform.position.y;
        }
        if (!UIPanelDialog_ConvertWorldToDisplayWithUnit(ui->displayUnit, (double)object->transform.position.z, &dz)) {
            dz = object->transform.position.z;
        }
        snprintf(ui->objectTransformDialog.buffer,
                 sizeof(ui->objectTransformDialog.buffer),
                 "%.4f, %.4f, %.4f",
                 dx, dy, dz);
    } else {
        snprintf(ui->objectTransformDialog.buffer,
                 sizeof(ui->objectTransformDialog.buffer),
                 "%.4f",
                 UIPanel_ObjectRotationAxisValue(object, target));
    }
    SanitizeBuffer(ui->objectTransformDialog.buffer);
    ui->objectTransformDialog.length = strlen(ui->objectTransformDialog.buffer);
    ui->objectTransformDialog.cursor = ui->objectTransformDialog.length;
    if (!SDL_IsTextInputActive()) SDL_StartTextInput();
    return true;
}

bool UIPanel_ApplyConstructionPlaneDialog(UIPanelState* ui) {
    GlobalState* state = NULL;
    ViewPlane plane = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f };
    float typedValue = 0.0f;
    double worldOffset = 0.0;

    if (!ui || !ui->constructionPlaneDialog.active) return false;
    state = Global_Get();
    if (!state) return false;
    if (state->spaceMode != SPACE_MODE_3D) {
        SDL_Log("[UI] Construction plane controls require SPACE_MODE_3D.");
        return false;
    }

    SanitizeBuffer(ui->constructionPlaneDialog.buffer);
    ui->constructionPlaneDialog.length = strlen(ui->constructionPlaneDialog.buffer);
    if (ui->constructionPlaneDialog.cursor > ui->constructionPlaneDialog.length) {
        ui->constructionPlaneDialog.cursor = ui->constructionPlaneDialog.length;
    }
    if (!UIPanel_ParseScalarFloat(ui->constructionPlaneDialog.buffer, &typedValue)) {
        SDL_Log("[UI] Construction plane offset edit failed: value must be numeric.");
        return false;
    }
    if (!UIPanelDialog_ConvertDisplayToWorldWithUnit(ui->displayUnit, (double)typedValue, &worldOffset)) {
        SDL_Log("[UI] Construction plane offset edit failed: unit conversion failed.");
        return false;
    }
    if (!isfinite(worldOffset)) {
        SDL_Log("[UI] Construction plane offset edit failed: converted value is not finite.");
        return false;
    }

    plane = UIPanel_CurrentConstructionViewPlane(state);
    plane.offset = (float)worldOffset;
    state->activePlane = plane;
    Layout_ConstructionPlane3D_SetFromViewPlane(&state->layout.scene3d.constructionPlane, plane);
    Global_FlagHitboxesDirty();
    SDL_Log("[UI] Construction plane offset updated: %s = %.4f%s",
            UIPanel_ViewPlaneCoordinateLabel(plane.axis),
            typedValue,
            UIPanel_GetDisplayUnitSymbol());

    UIPanel_CloseConstructionPlaneDialog(ui);
    return true;
}

bool UIPanel_ApplyObjectTransformDialog(UIPanelState* ui) {
    GlobalState* state = NULL;
    Object3D* object = NULL;
    bool boundsAdjusted = false;

    if (!ui || !ui->objectTransformDialog.active) return false;
    state = Global_Get();
    if (!state) return false;
    object = UIPanel_FindEditableObject3D(state, ui->objectTransformDialog.objectId);
    if (!object) {
        SDL_Log("[UI] Object transform edit failed: selected object is unavailable.");
        return false;
    }

    SanitizeBuffer(ui->objectTransformDialog.buffer);
    ui->objectTransformDialog.length = strlen(ui->objectTransformDialog.buffer);
    if (ui->objectTransformDialog.cursor > ui->objectTransformDialog.length) {
        ui->objectTransformDialog.cursor = ui->objectTransformDialog.length;
    }

    Editor_HistoryCapture(&state->editor, &state->layout);
    if (ui->objectTransformDialog.target == UI_OBJECT_TRANSFORM_DIALOG_TARGET_POSITION) {
        Vec3 typedDisplay = {0};
        Vec3 nextPosition = {0};
        double wx = 0.0, wy = 0.0, wz = 0.0;

        if (!UIPanel_ParseVec3Text(ui->objectTransformDialog.buffer, &typedDisplay)) {
            SDL_Log("[UI] Object position edit failed: expected three numeric values (x, y, z).");
            return false;
        }
        if (!UIPanelDialog_ConvertDisplayToWorldWithUnit(ui->displayUnit, typedDisplay.x, &wx) ||
            !UIPanelDialog_ConvertDisplayToWorldWithUnit(ui->displayUnit, typedDisplay.y, &wy) ||
            !UIPanelDialog_ConvertDisplayToWorldWithUnit(ui->displayUnit, typedDisplay.z, &wz)) {
            SDL_Log("[UI] Object position edit failed: unit conversion failed.");
            return false;
        }

        nextPosition = (Vec3){ (float)wx, (float)wy, (float)wz };
        if (!Layout_SetObject3DPosition(&state->layout,
                                        object->objectId,
                                        nextPosition,
                                        &boundsAdjusted)) {
            SDL_Log("[UI] Object position edit rejected by layout bounds/validation policy.");
            return false;
        }

        state->editor.selectedObject3DId = object->objectId;
        Global_FlagHitboxesDirty();
        SDL_Log("[UI] Object #%u position updated to (%.4f, %.4f, %.4f)%s",
                object->objectId,
                typedDisplay.x,
                typedDisplay.y,
                typedDisplay.z,
                boundsAdjusted ? " (bounds-adjusted)" : "");
    } else {
        float typedDegrees = 0.0f;
        const float currentDegrees = UIPanel_ObjectRotationAxisValue(object, ui->objectTransformDialog.target);
        Vec3 axisWorld = UIPanel_ObjectTransformTargetAxis(ui->objectTransformDialog.target);

        if (!UIPanel_ParseScalarFloat(ui->objectTransformDialog.buffer, &typedDegrees)) {
            SDL_Log("[UI] %s edit failed: value must be numeric.",
                    UIPanel_ObjectTransformTargetLabel(ui->objectTransformDialog.target));
            return false;
        }

        {
            const float delta = typedDegrees - currentDegrees;
            if (fabsf(delta) > 0.0001f) {
                if (!Layout_RotateObject3D(&state->layout,
                                           object->objectId,
                                           axisWorld,
                                           delta,
                                           NULL,
                                           &boundsAdjusted)) {
                    SDL_Log("[UI] %s edit rejected by layout bounds/validation policy.",
                            UIPanel_ObjectTransformTargetLabel(ui->objectTransformDialog.target));
                    return false;
                }
            }
        }

        state->editor.selectedObject3DId = object->objectId;
        Global_FlagHitboxesDirty();
        SDL_Log("[UI] Object #%u %s updated to %.4f deg%s",
                object->objectId,
                UIPanel_ObjectTransformTargetLabel(ui->objectTransformDialog.target),
                typedDegrees,
                boundsAdjusted ? " (bounds-adjusted)" : "");
    }

    UIPanel_CloseObjectTransformDialog(ui);
    return true;
}
