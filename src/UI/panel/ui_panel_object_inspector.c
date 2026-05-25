#include "UI/ui_panel_object_inspector.h"

#include "Core/global_state.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/ui_panel_object_layout.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

static int UIPanelObjectInspector_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelObjectInspector_LineGap(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).section_gap;
}

static int UIPanelObjectInspector_PanelPad(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).pad_y;
}

enum {
    UI_OBJECT_INSPECTOR_SUMMARY_LINE_COUNT = 4,
    UI_OBJECT_INSPECTOR_DETAILS_LINE_COUNT = 7
};

static void UIPanelObjectInspector_DrawTextClipped(SDL_Renderer* renderer,
                                                   TTF_Font* font,
                                                   const char* text,
                                                   int x,
                                                   int y,
                                                   int max_width,
                                                   SDL_Color color) {
    UIPanelSummary_DrawTextClipped(renderer,
                                   font,
                                   text,
                                   x,
                                   y,
                                   max_width,
                                   UIPanelVisual_MakeMetrics(font).line_h,
                                   color);
}

static const char* UIPanelObjectInspector_KindLabel(Object3DKind kind) {
    switch (kind) {
        case OBJECT3D_KIND_PLANE: return "Plane";
        case OBJECT3D_KIND_RECT_PRISM: return "Rect Prism";
        case OBJECT3D_KIND_UNKNOWN:
        default: return "Unknown";
    }
}

static const char* UIPanelObjectInspector_CorePlaneLabel(CoreObjectPlane plane) {
    switch (plane) {
        case CORE_OBJECT_PLANE_YZ: return "YZ";
        case CORE_OBJECT_PLANE_XZ: return "XZ";
        case CORE_OBJECT_PLANE_XY:
        default: return "XY";
    }
}

static const char* UIPanelObjectInspector_DimensionalModeLabel(CoreObjectDimensionalMode mode) {
    switch (mode) {
        case CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED: return "Plane locked";
        case CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D:
        default: return "Full 3D";
    }
}

static void UIPanelObjectInspector_FormatDimension(float world_value,
                                                   char* out,
                                                   size_t out_size) {
    double display = 0.0;
    const char* symbol = UIPanel_GetDisplayUnitSymbol();
    if (!out || out_size == 0) return;
    if (UIPanel_ConvertWorldToDisplay((double)world_value, &display)) {
        snprintf(out, out_size, "%.2f%s", display, symbol);
    } else {
        snprintf(out, out_size, "%.2f", world_value);
    }
}

int UIPanel_ObjectInspectorReservedHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT || !Global_Get()) return 0;
    font_h = UIPanelObjectInspector_FontHeight();
    line_gap = UIPanelObjectInspector_LineGap();
    pad = UIPanelObjectInspector_PanelPad();
    return (pad * 2) +
           (font_h * UI_OBJECT_INSPECTOR_SUMMARY_LINE_COUNT) +
           (line_gap * (UI_OBJECT_INSPECTOR_SUMMARY_LINE_COUNT - 1));
}

int UIPanel_ObjectInspectorDetailsHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    if (!ui || ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT || !Global_Get()) return 0;
    font_h = UIPanelObjectInspector_FontHeight();
    line_gap = UIPanelObjectInspector_LineGap();
    pad = UIPanelObjectInspector_PanelPad();
    return (pad * 2) +
           (font_h * UI_OBJECT_INSPECTOR_DETAILS_LINE_COUNT) +
           (line_gap * (UI_OBJECT_INSPECTOR_DETAILS_LINE_COUNT - 1));
}

static void UIPanelObjectInspector_DrawSummaryCard(const UIPanelState* ui,
                                                   SDL_Renderer* renderer,
                                                   TTF_Font* font,
                                                   const Object3D* object,
                                                   SDL_Color label_color,
                                                   SDL_Color value_color,
                                                   SDL_Color accent_color,
                                                   SDL_Color fill_color,
                                                   SDL_Color border_color) {
    SDL_Rect panel = {0, 0, 0, 0};
    int font_h = UIPanelObjectInspector_FontHeight();
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    int line_gap = metrics.section_gap;
    int y = 0;
    char line_identity[160];
    char line_context[160];
    char line_selection[160];

    if (!UIPanel_GetObjectPaneRects(ui, &panel, NULL, NULL, NULL, NULL, NULL)) return;
    if (panel.h <= 0) return;

    UIPanelSummary_DrawCard(renderer, panel, fill_color, border_color, accent_color, metrics.accent_h);
    y = panel.y + metrics.pad_y;

    UIPanelSummary_DrawText(renderer, font, "Selected Object", panel.x + metrics.pad_x, y, label_color);
    y += font_h + line_gap;

    if (!object) {
        UIPanelObjectInspector_DrawTextClipped(renderer,
                                               font,
                                               "No object selected.",
                                               panel.x + metrics.pad_x,
                                               y,
                                               panel.w - (metrics.pad_x * 2),
                                               value_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer,
                                               font,
                                               "Pick from the Scene list or click an object origin in the viewport.",
                                               panel.x + metrics.pad_x,
                                               y,
                                               panel.w - (metrics.pad_x * 2),
                                               label_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer,
                                               font,
                                               "Use Create for new geometry, then return here for object-local edits.",
                                               panel.x + metrics.pad_x,
                                               y,
                                               panel.w - (metrics.pad_x * 2),
                                               label_color);
        return;
    }

    snprintf(line_identity,
             sizeof(line_identity),
             "#%u  %s   %s",
             object->objectId,
             UIPanelObjectInspector_KindLabel(object->kind),
             object->coreMeta.object_id);
    snprintf(line_context,
             sizeof(line_context),
             "%s   Locked plane %s",
             UIPanelObjectInspector_DimensionalModeLabel(object->coreMeta.dimensional_mode),
             UIPanelObjectInspector_CorePlaneLabel(object->coreMeta.locked_plane));
    snprintf(line_selection,
             sizeof(line_selection),
             "Use the sections below for typed edits, units, gizmo mode, and object actions.");

    UIPanelObjectInspector_DrawTextClipped(renderer, font, line_identity, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), accent_color);
    y += font_h + line_gap;
    UIPanelSummary_DrawDivider(renderer, panel, y - (line_gap / 2), metrics.pad_x, accent_color, 90);
    UIPanelObjectInspector_DrawTextClipped(renderer, font, line_context, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), value_color);
    y += font_h + line_gap;
    UIPanelObjectInspector_DrawTextClipped(renderer, font, line_selection, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), label_color);
}

static void UIPanelObjectInspector_DrawDetailsCard(const UIPanelState* ui,
                                                   SDL_Renderer* renderer,
                                                   TTF_Font* font,
                                                   const Object3D* object,
                                                   SDL_Color label_color,
                                                   SDL_Color value_color,
                                                   SDL_Color accent_color,
                                                   SDL_Color fill_color,
                                                   SDL_Color border_color) {
    SDL_Rect panel = {0, 0, 0, 0};
    int font_h = UIPanelObjectInspector_FontHeight();
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    int line_gap = metrics.section_gap;
    int y = 0;

    if (!UIPanel_GetObjectPaneRects(ui, NULL, &panel, NULL, NULL, NULL, NULL)) return;
    if (panel.h <= 0) return;

    UIPanelSummary_DrawCard(renderer, panel, fill_color, border_color, accent_color, metrics.accent_h);
    y = panel.y + metrics.pad_y;

    UIPanelSummary_DrawText(renderer, font, "Inspector", panel.x + metrics.pad_x, y, label_color);
    y += font_h + line_gap;

    if (!object) {
        UIPanelObjectInspector_DrawTextClipped(renderer,
                                               font,
                                               "Nothing to inspect yet.",
                                               panel.x + metrics.pad_x,
                                               y,
                                               panel.w - (metrics.pad_x * 2),
                                               value_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer,
                                               font,
                                               "The Scene tab owns the object list.",
                                               panel.x + metrics.pad_x,
                                               y,
                                               panel.w - (metrics.pad_x * 2),
                                               label_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer,
                                               font,
                                               "Select from Scene or use Create, then this pane becomes the object editor.",
                                               panel.x + metrics.pad_x,
                                               y,
                                               panel.w - (metrics.pad_x * 2),
                                               label_color);
        return;
    }

    {
        char line_identity[160];
        char line_dims[160];
        char line_pos[160];
        char line_rot[160];
        char line_state[192];
        char line_editing[192];
        char w_text[32] = {0};
        char h_text[32] = {0};
        char d_text[32] = {0};
        const bool lock_plane = (object->kind == OBJECT3D_KIND_RECT_PRISM)
                                    ? object->rectPrism.lockToConstructionPlane
                                    : object->plane.lockToConstructionPlane;
        const bool lock_bounds = (object->kind == OBJECT3D_KIND_RECT_PRISM)
                                     ? object->rectPrism.lockToBounds
                                     : object->plane.lockToBounds;

        if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            UIPanelObjectInspector_FormatDimension(object->rectPrism.width, w_text, sizeof(w_text));
            UIPanelObjectInspector_FormatDimension(object->rectPrism.height, h_text, sizeof(h_text));
            UIPanelObjectInspector_FormatDimension(object->rectPrism.depth, d_text, sizeof(d_text));
        } else {
            UIPanelObjectInspector_FormatDimension(object->plane.width, w_text, sizeof(w_text));
            UIPanelObjectInspector_FormatDimension(object->plane.height, h_text, sizeof(h_text));
            snprintf(d_text, sizeof(d_text), "n/a");
        }

        snprintf(line_identity,
                 sizeof(line_identity),
                 "Identity  %s   Type %s   Plane %s",
                 object->coreMeta.object_id,
                 object->coreMeta.object_type[0] ? object->coreMeta.object_type : UIPanelObjectInspector_KindLabel(object->kind),
                 UIPanelObjectInspector_CorePlaneLabel(object->coreMeta.locked_plane));
        snprintf(line_dims,
                 sizeof(line_dims),
                 "Primitive  W %s   H %s   D %s",
                 w_text,
                 h_text,
                 d_text);
        snprintf(line_pos,
                 sizeof(line_pos),
                 "Position  %.2f, %.2f, %.2f",
                 object->transform.position.x,
                 object->transform.position.y,
                 object->transform.position.z);
        snprintf(line_rot,
                 sizeof(line_rot),
                 "Rotation  %.1f, %.1f, %.1f deg",
                 object->transform.rotationDeg.x,
                 object->transform.rotationDeg.y,
                 object->transform.rotationDeg.z);
        snprintf(line_state,
                 sizeof(line_state),
                 "State  Visible:%s  Locked:%s  Selectable:%s  Plane:%s  Bounds:%s",
                 object->coreMeta.flags.visible ? "On" : "Off",
                 object->coreMeta.flags.locked ? "On" : "Off",
                 object->coreMeta.flags.selectable ? "On" : "Off",
                 lock_plane ? "On" : "Off",
                 lock_bounds ? "On" : "Off");
        snprintf(line_editing,
                 sizeof(line_editing),
                 "Editing  %s   Gizmo:%s   Unit:%s",
                 UIPanelObjectInspector_DimensionalModeLabel(object->coreMeta.dimensional_mode),
                 Global_Get()->editor.object3DRotateMode ? "Rotate" : "Move",
                 UIPanel_GetDisplayUnitSymbol());

        UIPanelObjectInspector_DrawTextClipped(renderer, font, "Inspector", panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), accent_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer, font, line_identity, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), value_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer, font, line_pos, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), value_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer, font, line_rot, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), value_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer, font, line_dims, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), value_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer, font, line_state, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), label_color);
        y += font_h + line_gap;
        UIPanelObjectInspector_DrawTextClipped(renderer, font, line_editing, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), label_color);
    }
}

void Render_UIPanelObjectInspector(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    const Object3D* object = NULL;
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    SDL_Color fill_color = {20, 20, 24, 170};
    SDL_Color border_color = {90, 100, 115, 200};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualPalette palette = {0};

    if (!ui || !renderer || !state || !font) return;
    if (ui->activeRightTab != UI_PANEL_RIGHT_TAB_OBJECT) return;

    if (state->editor.selectedObject3DId != 0u) {
        object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                              state->editor.selectedObject3DId);
    }

    (void)UIPanelVisual_ResolvePalette(&palette);
    label_color = palette.text_muted;
    value_color = palette.text_primary;
    accent_color = palette.accent;
    fill_color = palette.pane_fill;
    fill_color.a = 170;
    border_color = palette.pane_border;
    border_color.a = 210;

    UIPanelObjectInspector_DrawSummaryCard(ui,
                                           renderer,
                                           font,
                                           object,
                                           label_color,
                                           value_color,
                                           accent_color,
                                           fill_color,
                                           border_color);
    UIPanelObjectInspector_DrawDetailsCard(ui,
                                           renderer,
                                           font,
                                           object,
                                           label_color,
                                           value_color,
                                           accent_color,
                                           fill_color,
                                           border_color);
}
