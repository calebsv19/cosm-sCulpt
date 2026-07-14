#include "UI/ui_panel_scene_authoring_inspector.h"
#include "Layout/scene/layout_scene_path_geometry.h"
#include "Layout/scene/layout_scene_path_traversal.h"
#include "Layout/scene/layout_scene_camera_authoring.h"
#include "Layout/scene/layout_scene_light_authoring.h"

#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Editor/scene_authoring_path_handles.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/ui_panel_object_layout.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

#include <SDL2/SDL.h>
#include <stdio.h>

enum {
    UI_SCENE_AUTHORING_INSPECTOR_SUMMARY_LINE_COUNT = 4,
    UI_SCENE_AUTHORING_INSPECTOR_DETAILS_LINE_COUNT = 8
};

static int UIPanelSceneAuthoringInspector_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelSceneAuthoringInspector_LineGap(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).section_gap;
}

static int UIPanelSceneAuthoringInspector_PanelPad(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).pad_y;
}

static bool UIPanelSceneAuthoringInspector_SelectedKind(
    LineDrawingSceneAuthoringSelectionKind* out_kind) {
    const GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringSelectionKind kind =
        LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE;
    if (state) kind = state->layout.sceneAuthoring.selected_kind;
    if (out_kind) *out_kind = kind;
    return kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT ||
           kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH ||
           kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL;
}

bool UIPanel_SceneAuthoringInspectorHasSelection(void) {
    return UIPanelSceneAuthoringInspector_SelectedKind(NULL);
}

int UIPanel_SceneAuthoringInspectorReservedHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT) return 0;
    if (!UIPanel_SceneAuthoringInspectorHasSelection()) return 0;
    font_h = UIPanelSceneAuthoringInspector_FontHeight();
    line_gap = UIPanelSceneAuthoringInspector_LineGap();
    pad = UIPanelSceneAuthoringInspector_PanelPad();
    return (pad * 2) +
           (font_h * UI_SCENE_AUTHORING_INSPECTOR_SUMMARY_LINE_COUNT) +
           (line_gap * (UI_SCENE_AUTHORING_INSPECTOR_SUMMARY_LINE_COUNT - 1));
}

int UIPanel_SceneAuthoringInspectorDetailsHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT) return 0;
    if (!UIPanel_SceneAuthoringInspectorHasSelection()) return 0;
    font_h = UIPanelSceneAuthoringInspector_FontHeight();
    line_gap = UIPanelSceneAuthoringInspector_LineGap();
    pad = UIPanelSceneAuthoringInspector_PanelPad();
    return (pad * 2) +
           (font_h * UI_SCENE_AUTHORING_INSPECTOR_DETAILS_LINE_COUNT) +
           (line_gap * (UI_SCENE_AUTHORING_INSPECTOR_DETAILS_LINE_COUNT - 1));
}

bool UIPanel_ToggleSceneAuthoringEditMode(void) {
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringSelectionKind kind =
        LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE;
    SceneAuthoringEditMode target = SCENE_AUTHORING_EDIT_MODE_NONE;
    if (!state) return false;
    kind = state->layout.sceneAuthoring.selected_kind;
    if (kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT) {
        target = SCENE_AUTHORING_EDIT_MODE_LIGHT;
    } else if (kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH) {
        target = SCENE_AUTHORING_EDIT_MODE_PATH;
    } else {
        return false;
    }
    if (state->editor.sceneAuthoringEditMode == target) {
        target = SCENE_AUTHORING_EDIT_MODE_NONE;
    }
    return Editor_SetSceneAuthoringEditMode(&state->editor, target);
}

bool UIPanel_ToggleSelectedSceneAuthoringLightEnabled(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (!Layout_SceneAuthoringState_ToggleSelectedLightEnabled(&state->layout.sceneAuthoring)) {
        return false;
    }
    Global_FlagLayoutChanged();
    return true;
}

bool UIPanel_CycleSelectedSceneAuthoringLightKind(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (!Layout_SceneAuthoringState_CycleSelectedLightKind(&state->layout.sceneAuthoring)) {
        return false;
    }
    Global_FlagLayoutChanged();
    return true;
}

bool UIPanel_CycleSelectedSceneAuthoringLightPath(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (!Layout_SceneAuthoringState_CycleSelectedLightPath(&state->layout.sceneAuthoring)) {
        return false;
    }
    Global_FlagLayoutChanged();
    return true;
}

bool UIPanel_CycleSelectedSceneAuthoringCameraPathKind(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (!Layout_SceneAuthoringState_CycleSelectedPathCurveType(&state->layout.sceneAuthoring)) {
        return false;
    }
    Global_FlagLayoutChanged();
    return true;
}

bool UIPanel_CycleSelectedSceneAuthoringTangentMode(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    return SceneAuthoringPathHandles_CycleSelectedTangentMode(state, &state->editor);
}

typedef bool (*SceneAuthoringMutationFn)(LineDrawingSceneAuthoringState* state);

static bool UIPanel_MutateSceneAuthoringState(SceneAuthoringMutationFn mutation) {
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState edited;
    if (!state || !mutation) return false;
    edited = state->layout.sceneAuthoring;
    if (!mutation(&edited)) return false;
    Editor_HistoryCapture(&state->editor, &state->layout);
    state->layout.sceneAuthoring = edited;
    Global_FlagLayoutChanged();
    return true;
}

bool UIPanel_CycleSelectedSceneAuthoringCameraOrientation(void) {
    return UIPanel_MutateSceneAuthoringState(
        Layout_SceneAuthoringState_CycleSelectedCameraOrientation);
}

bool UIPanel_CycleSelectedSceneAuthoringCameraRoll(void) {
    return UIPanel_MutateSceneAuthoringState(Layout_SceneAuthoringState_CycleSelectedCameraRoll);
}

bool UIPanel_CycleSelectedSceneAuthoringCameraFov(void) {
    return UIPanel_MutateSceneAuthoringState(Layout_SceneAuthoringState_CycleSelectedCameraFov);
}

bool UIPanel_CycleSelectedSceneAuthoringCameraClip(void) {
    return UIPanel_MutateSceneAuthoringState(Layout_SceneAuthoringState_CycleSelectedCameraClipPreset);
}

bool UIPanel_CycleSelectedSceneAuthoringLightPositionMode(void) {
    return UIPanel_MutateSceneAuthoringState(
        Layout_SceneAuthoringState_CycleSelectedLightPositionMode);
}

bool UIPanel_CycleSelectedSceneAuthoringLightColor(void) {
    return UIPanel_MutateSceneAuthoringState(Layout_SceneAuthoringState_CycleSelectedLightColor);
}

bool UIPanel_CycleSelectedSceneAuthoringLightIntensity(void) {
    return UIPanel_MutateSceneAuthoringState(Layout_SceneAuthoringState_CycleSelectedLightIntensity);
}

bool UIPanel_CycleSelectedSceneAuthoringLightSize(void) {
    return UIPanel_MutateSceneAuthoringState(Layout_SceneAuthoringState_CycleSelectedLightRadiusOrSize);
}

bool UIPanel_CycleSelectedSceneAuthoringLightCone(void) {
    return UIPanel_MutateSceneAuthoringState(Layout_SceneAuthoringState_CycleSelectedLightCone);
}

bool UIPanel_CycleSelectedSceneAuthoringLightFalloff(void) {
    return UIPanel_MutateSceneAuthoringState(Layout_SceneAuthoringState_CycleSelectedLightFalloff);
}

bool UIPanel_CycleSelectedSceneAuthoringMaterialColor(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (!Layout_SceneAuthoringState_CycleSelectedMaterialColor(&state->layout.sceneAuthoring)) {
        return false;
    }
    Global_FlagLayoutChanged();
    return true;
}

static LineDrawingScenePath* UIPanel_SelectedPath(LineDrawingSceneAuthoringState* state) {
    if (!state || state->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH ||
        state->selected_index >= state->path_count) return NULL;
    return &state->paths[state->selected_index];
}

static bool TogglePathPlayback(LineDrawingSceneAuthoringState* state) {
    LineDrawingScenePath* path = UIPanel_SelectedPath(state);
    if (!path) return false;
    path->playing = !path->playing;
    return true;
}

static bool AdvancePathScrub(LineDrawingSceneAuthoringState* state) {
    LineDrawingScenePath* path = UIPanel_SelectedPath(state);
    if (!path) return false;
    path->normalized_distance += 0.25f;
    if (path->normalized_distance > 1.0001f) path->normalized_distance = 0.0f;
    path->playing = false;
    return true;
}

static bool CyclePathPlaybackMode(LineDrawingSceneAuthoringState* state) {
    LineDrawingScenePath* path = UIPanel_SelectedPath(state);
    if (!path) return false;
    path->playback_mode = path->playback_mode == LINE_DRAWING_SCENE_PATH_PLAYBACK_ONCE
        ? LINE_DRAWING_SCENE_PATH_PLAYBACK_LOOP : LINE_DRAWING_SCENE_PATH_PLAYBACK_ONCE;
    return true;
}

static bool CyclePathDuration(LineDrawingSceneAuthoringState* state) {
    LineDrawingScenePath* path = UIPanel_SelectedPath(state);
    if (!path) return false;
    path->duration_seconds = path->duration_seconds < 3.0f ? 5.0f
        : path->duration_seconds < 7.0f ? 10.0f : 2.0f;
    return true;
}

static bool TogglePathClosed(LineDrawingSceneAuthoringState* state) {
    LineDrawingScenePath* path = UIPanel_SelectedPath(state);
    if (!path) return false;
    path->closed = !path->closed;
    return true;
}

bool UIPanel_ToggleSelectedSceneAuthoringPathPlayback(void) {
    return UIPanel_MutateSceneAuthoringState(TogglePathPlayback);
}
bool UIPanel_AdvanceSelectedSceneAuthoringPathScrub(void) {
    return UIPanel_MutateSceneAuthoringState(AdvancePathScrub);
}
bool UIPanel_CycleSelectedSceneAuthoringPathPlaybackMode(void) {
    return UIPanel_MutateSceneAuthoringState(CyclePathPlaybackMode);
}
bool UIPanel_CycleSelectedSceneAuthoringPathDuration(void) {
    return UIPanel_MutateSceneAuthoringState(CyclePathDuration);
}
bool UIPanel_ToggleSelectedSceneAuthoringPathClosed(void) {
    return UIPanel_MutateSceneAuthoringState(TogglePathClosed);
}

static void UIPanelSceneAuthoringInspector_DrawLine(SDL_Renderer* renderer,
                                                    TTF_Font* font,
                                                    SDL_Rect panel,
                                                    int y,
                                                    const char* text,
                                                    SDL_Color color) {
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    UIPanelSummary_DrawTextClipped(renderer,
                                   font,
                                   text,
                                   panel.x + metrics.pad_x,
                                   y,
                                   panel.w - (metrics.pad_x * 2),
                                   metrics.line_h + 4,
                                   color);
}

static void UIPanelSceneAuthoringInspector_FormatSummary(const GlobalState* state,
                                                         char* line0,
                                                         size_t line0_size,
                                                         char* line1,
                                                         size_t line1_size,
                                                         char* line2,
                                                         size_t line2_size) {
    const LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
    if (authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT &&
        authoring->selected_index < authoring->light_count) {
        const LineDrawingSceneLight* light = &authoring->lights[authoring->selected_index];
        snprintf(line0, line0_size, "Light  %s", light->label);
        snprintf(line1, line1_size, "Id %s   Kind %s", light->light_id,
                 Layout_SceneLightKind_Label(light->kind));
        snprintf(line2, line2_size, "%s   %s   I %.1f",
                 Layout_SceneLightPositionMode_Name(light->position_mode),
                 light->enabled ? "Enabled" : "Disabled",
                 light->intensity);
    } else if (authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH &&
               authoring->selected_index < authoring->path_count) {
        const LineDrawingScenePath* path =
            &authoring->paths[authoring->selected_index];
        LineDrawingScenePathGeometry geometry = {0};
        LineDrawingScenePathTraversalTable traversal = {0};
        (void)Layout_ScenePathGeometry_Build(path, &geometry);
        (void)Layout_ScenePathTraversal_Build(path, &traversal);
        snprintf(line0, line0_size, "%s Path  %s",
                 Layout_ScenePathRole_Name(path->role), path->label);
        snprintf(line1, line1_size, "Id %s   Curve %s", path->path_id, path->curve_type);
        if (path->role == LINE_DRAWING_SCENE_PATH_ROLE_CAMERA) {
            const LineDrawingSceneCamera* camera =
                Layout_SceneAuthoringState_FindCameraForPathConst(authoring, path);
            snprintf(line2, line2_size, "Camera %s   %.1fu   %.0f%%",
                     camera ? Layout_SceneCameraOrientationMode_Name(camera->orientation_mode)
                            : "unbound",
                     traversal.total_distance,
                     path->normalized_distance * 100.0f);
        } else {
            snprintf(line2, line2_size, "Mode %s   %.1fu   %.0f%%",
                     Editor_SceneAuthoringEditModeLabel(state->editor.sceneAuthoringEditMode),
                     traversal.total_distance,
                     path->normalized_distance * 100.0f);
        }
    } else if (authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL &&
               authoring->selected_index < authoring->material_count) {
        const LineDrawingSceneMaterial* material =
            &authoring->materials[authoring->selected_index];
        snprintf(line0, line0_size, "Material  %s", material->label);
        snprintf(line1, line1_size, "Id %s", material->material_id);
        snprintf(line2, line2_size, "RGBA %.2f, %.2f, %.2f, %.2f",
                 material->rgba[0], material->rgba[1], material->rgba[2], material->rgba[3]);
    } else {
        snprintf(line0, line0_size, "Scene Authoring");
        snprintf(line1, line1_size, "Selection unavailable");
        snprintf(line2, line2_size, "Pick a scene-authoring row.");
    }
}

void Render_UIPanelSceneAuthoringInspector(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualPalette palette = {0};
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    SDL_Color fill_color = {20, 20, 24, 170};
    SDL_Color details_fill = {16, 18, 22, 190};
    SDL_Color border_color = {90, 100, 115, 210};
    SDL_Color accent_color = {140, 170, 210, 255};
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Rect summary = {0, 0, 0, 0};
    SDL_Rect details = {0, 0, 0, 0};
    int font_h = 0;
    int line_gap = 0;
    int y = 0;
    char line0[192] = {0};
    char line1[192] = {0};
    char line2[192] = {0};

    if (!ui || !renderer || !state || !font) return;
    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT) return;
    if (!UIPanel_SceneAuthoringInspectorHasSelection()) return;
    if (!UIPanel_GetObjectPaneRects(ui, &summary, &details, NULL, NULL, NULL, NULL)) return;

    (void)UIPanelVisual_ResolvePalette(&palette);
    fill_color = palette.pane_fill;
    fill_color.a = 170;
    details_fill = palette.workspace_fill;
    border_color = palette.pane_border;
    border_color.a = 210;
    accent_color = palette.accent;
    label_color = palette.text_muted;
    value_color = palette.text_primary;
    font_h = UIPanelSceneAuthoringInspector_FontHeight();
    line_gap = UIPanelSceneAuthoringInspector_LineGap();

    UIPanelSceneAuthoringInspector_FormatSummary(state,
                                                 line0,
                                                 sizeof(line0),
                                                 line1,
                                                 sizeof(line1),
                                                 line2,
                                                 sizeof(line2));

    if (summary.h > 0) {
        UIPanelSummary_DrawCard(renderer, summary, fill_color, border_color, accent_color, metrics.accent_h);
        y = summary.y + metrics.pad_y;
        UIPanelSummary_DrawText(renderer, font, "Scene Authoring", summary.x + metrics.pad_x, y, label_color);
        y += font_h + line_gap;
        UIPanelSceneAuthoringInspector_DrawLine(renderer, font, summary, y, line0, accent_color);
        y += font_h + line_gap;
        UIPanelSceneAuthoringInspector_DrawLine(renderer, font, summary, y, line1, value_color);
        y += font_h + line_gap;
        UIPanelSceneAuthoringInspector_DrawLine(renderer, font, summary, y, line2, label_color);
    }

    if (details.h > 0) {
        char detail0[192] = {0};
        char detail1[192] = {0};
        char detail2[192] = {0};
        const LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
        UIPanelSummary_DrawCard(renderer, details, details_fill, border_color, accent_color, metrics.accent_h);
        y = details.y + metrics.pad_y;
        UIPanelSummary_DrawText(renderer, font, "Properties", details.x + metrics.pad_x, y, label_color);
        y += font_h + line_gap;

        if (authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT &&
            authoring->selected_index < authoring->light_count) {
            const LineDrawingSceneLight* light = &authoring->lights[authoring->selected_index];
            const LineDrawingScenePath* light_path =
                Layout_SceneAuthoringState_FindPathByIdConst(authoring, light->path_id);
            const Vec3 effective_position =
                Layout_SceneLight_EffectivePosition(light, light_path);
            snprintf(detail0, sizeof(detail0), "Position %.2f %.2f %.2f   %s",
                     effective_position.x, effective_position.y, effective_position.z,
                     Layout_SceneLightPositionMode_Name(light->position_mode));
            snprintf(detail1, sizeof(detail1), "RGB %.2f %.2f %.2f   I %.1f   %s",
                     light->color_rgb[0], light->color_rgb[1], light->color_rgb[2],
                     light->intensity, Layout_SceneLightFalloff_Name(light->falloff));
            if (state->editor.selectedSceneAuthoringLightPosition) {
                snprintf(detail2, sizeof(detail2), "Selected  light position handle");
            } else if (state->editor.selectedSceneAuthoringLightAim) {
                snprintf(detail2, sizeof(detail2), "Selected  light aim handle");
            } else if (state->editor.selectedSceneAuthoringControlPointIndex >= 0) {
                const size_t path_index =
                    (size_t)state->editor.selectedSceneAuthoringPathIndex;
                if (state->editor.selectedSceneAuthoringPathIndex >= 0 &&
                    path_index < authoring->path_count) {
                    const LineDrawingScenePathElementRef element =
                        Layout_ScenePathEdit_ElementForControl(
                            &authoring->paths[path_index],
                            (size_t)state->editor.selectedSceneAuthoringControlPointIndex);
                    snprintf(detail2, sizeof(detail2), "Selected  %s   Tangent %s",
                             Layout_ScenePathEdit_ElementKindName(element.kind),
                             Layout_ScenePathTangentMode_Name(
                                 Layout_ScenePathEdit_AnchorMode(
                                     &authoring->paths[path_index], element.anchor_index)));
                } else {
                    snprintf(detail2, sizeof(detail2), "Selected  path handle");
                }
            } else {
                snprintf(detail2, sizeof(detail2), "Path  %s",
                         light->path_id[0] ? light->path_id : "none");
            }
        } else if (authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH &&
                   authoring->selected_index < authoring->path_count) {
            const LineDrawingScenePath* path =
                &authoring->paths[authoring->selected_index];
            const LineDrawingSceneCamera* camera =
                Layout_SceneAuthoringState_FindCameraForPathConst(authoring, path);
            if (camera) {
                snprintf(detail0, sizeof(detail0), "Orientation %s   Roll %.0f deg",
                         Layout_SceneCameraOrientationMode_Name(camera->orientation_mode),
                         camera->roll_degrees);
                snprintf(detail1, sizeof(detail1), "FOV %.0f deg   Clip %.2g / %.0f",
                         camera->vertical_fov_degrees,
                         camera->near_clip,
                         camera->far_clip);
            } else {
                snprintf(detail0, sizeof(detail0), "Camera  unbound");
                snprintf(detail1, sizeof(detail1), "Role  %s",
                         Layout_ScenePathRole_Name(path->role));
            }
            if (state->editor.selectedSceneAuthoringCameraAim) {
                snprintf(detail2, sizeof(detail2), "Selected  camera aim handle");
            } else if (state->editor.selectedSceneAuthoringPathIndex ==
                           (int)authoring->selected_index &&
                state->editor.selectedSceneAuthoringControlPointIndex >= 0) {
                const LineDrawingScenePathElementRef element =
                    Layout_ScenePathEdit_ElementForControl(
                        path,
                        (size_t)state->editor.selectedSceneAuthoringControlPointIndex);
                snprintf(detail2,
                         sizeof(detail2),
                         "Selected  %s %zu   Tangent %s",
                         Layout_ScenePathEdit_ElementKindName(element.kind),
                         element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR
                             ? element.anchor_index + 1u : element.control_index + 1u,
                         Layout_ScenePathTangentMode_Name(
                             Layout_ScenePathEdit_AnchorMode(path, element.anchor_index)));
            } else if (state->editor.selectedSceneAuthoringPathIndex ==
                           (int)authoring->selected_index &&
                       state->editor.selectedSceneAuthoringPathElementKind ==
                           LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT) {
                snprintf(detail2, sizeof(detail2), "Selected  segment %d",
                         state->editor.selectedSceneAuthoringPathSegmentIndex + 1);
            } else {
                snprintf(detail2, sizeof(detail2), "Controls  Edit mode %s",
                         state->editor.sceneAuthoringEditMode == SCENE_AUTHORING_EDIT_MODE_PATH
                             ? "active"
                             : "off");
            }
        } else {
            snprintf(detail0, sizeof(detail0), "Material palette entry");
            snprintf(detail1, sizeof(detail1), "Color shown in the selected palette slot.");
            snprintf(detail2, sizeof(detail2), "Renderer stack  downstream material owner.");
        }
        UIPanelSceneAuthoringInspector_DrawLine(renderer, font, details, y, detail0, value_color);
        y += font_h + line_gap;
        UIPanelSceneAuthoringInspector_DrawLine(renderer, font, details, y, detail1, value_color);
        y += font_h + line_gap;
        UIPanelSceneAuthoringInspector_DrawLine(renderer, font, details, y, detail2, label_color);
    }
}
