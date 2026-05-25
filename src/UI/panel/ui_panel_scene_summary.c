#include "UI/ui_panel_scene_summary.h"

#include "Core/global_state.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/ui_panel_scene_layout.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

static int UIPanelSceneSummary_FontHeight(void) {
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int h = 14;
    if (font) h = TTF_FontHeight(font);
    if (h < 12) h = 12;
    return h;
}

static int UIPanelSceneSummary_LineGap(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).section_gap;
}

static int UIPanelSceneSummary_PanelPad(void) {
    return UIPanelVisual_MakeMetrics(FontManager_Get(FONT_DEFAULT)).pad_y;
}

static void UIPanelSceneSummary_CountKinds(const LayoutObjectStore* store,
                                           size_t* out_total,
                                           size_t* out_planes,
                                           size_t* out_prisms) {
    size_t total = 0u;
    size_t planes = 0u;
    size_t prisms = 0u;
    if (store) {
        for (size_t i = 0; i < store->count; ++i) {
            const Object3D* object = &store->items[i];
            if (object->isDeleted || object->objectId == 0u) continue;
            ++total;
            if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
                ++prisms;
            } else if (object->kind == OBJECT3D_KIND_PLANE) {
                ++planes;
            }
        }
    }
    if (out_total) *out_total = total;
    if (out_planes) *out_planes = planes;
    if (out_prisms) *out_prisms = prisms;
}

static size_t UIPanelSceneSummary_CountLiveAnchors(const Layout* layout) {
    size_t total = 0u;
    if (!layout || !layout->anchors) return 0u;
    for (size_t i = 0; i < layout->anchorCount; ++i) {
        if (!layout->anchors[i].isDeleted) ++total;
    }
    return total;
}

static const char* UIPanelSceneSummary_KindLabel(Object3DKind kind) {
    switch (kind) {
        case OBJECT3D_KIND_RECT_PRISM: return "Prism";
        case OBJECT3D_KIND_PLANE: return "Plane";
        case OBJECT3D_KIND_UNKNOWN:
        default: return "Unknown";
    }
}

static void UIPanelSceneSummary_FormatDimension(float world_value,
                                                char* out,
                                                size_t out_size) {
    double display = 0.0;
    const char* symbol = UIPanel_GetDisplayUnitSymbol();
    if (!out || out_size == 0) return;
    if (UIPanel_ConvertWorldToDisplay((double)world_value, &display)) {
        snprintf(out, out_size, "%.1f%s", display, symbol);
    } else {
        snprintf(out, out_size, "%.1f", world_value);
    }
}

int UIPanel_SceneSummaryReservedHeight(const UIPanelState* ui) {
    int font_h = 0;
    int line_gap = 0;
    int pad = 0;
    const int lines = 6;
    if (!ui || ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return 0;
    font_h = UIPanelSceneSummary_FontHeight();
    line_gap = UIPanelSceneSummary_LineGap();
    pad = UIPanelSceneSummary_PanelPad();
    return (pad * 2) + (font_h * lines) + (line_gap * (lines - 1));
}

void Render_UIPanelSceneSummary(const UIPanelState* ui, SDL_Renderer* renderer) {
    GlobalState* state = Global_Get();
    const Object3D* object = NULL;
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    SDL_Color fill_color = {20, 20, 24, 170};
    SDL_Color border_color = {90, 100, 115, 210};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    UIPanelVisualPalette palette = {0};
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);
    SDL_Rect panel = {0, 0, 0, 0};
    char line_counts[160];
    char line_selected[160];
    char line_context[192];
    char line_locks[192];
    int font_h = 0;
    int line_gap = 0;
    int panel_pad = 0;
    int y = 0;
    size_t total = 0u;
    size_t planes = 0u;
    size_t prisms = 0u;
    size_t anchors = 0u;
    size_t walls = 0u;

    if (!ui || !renderer || !font || !state) return;
    if (ui->activeLeftTab != UI_PANEL_LEFT_TAB_SCENE) return;
    if (!UIPanel_GetScenePaneRects(ui, &panel, NULL, NULL, NULL)) return;
    if (panel.h <= 0) return;

    (void)UIPanelVisual_ResolvePalette(&palette);
    label_color = palette.text_muted;
    value_color = palette.text_primary;
    accent_color = palette.accent;
    fill_color = palette.pane_fill;
    fill_color.a = 170;
    border_color = palette.pane_border;
    border_color.a = 210;
    UIPanelSummary_DrawCard(renderer, panel, fill_color, border_color, accent_color, metrics.accent_h);

    UIPanelSceneSummary_CountKinds(&state->layout.objectStore, &total, &planes, &prisms);
    anchors = UIPanelSceneSummary_CountLiveAnchors(&state->layout);
    walls = state->layout.wallCount;
    if (state->editor.selectedObject3DId != 0u) {
        object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                              state->editor.selectedObject3DId);
    }

    font_h = UIPanelSceneSummary_FontHeight();
    line_gap = UIPanelSceneSummary_LineGap();
    panel_pad = metrics.pad_y;
    y = panel.y + panel_pad;

    UIPanelSummary_DrawText(renderer, font, "Scene", panel.x + metrics.pad_x, y, label_color);
    y += font_h + line_gap;

    snprintf(line_counts,
             sizeof(line_counts),
             "Objects  %zu total   %zu planes   %zu prisms",
             total,
             planes,
             prisms);
    UIPanelSummary_DrawTextClipped(renderer, font, line_counts, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, accent_color);
    y += font_h + line_gap;

    UIPanelSummary_DrawDivider(renderer, panel, y - (line_gap / 2), metrics.pad_x, accent_color, 90);

    if (!object) {
        snprintf(line_selected, sizeof(line_selected), "Selection  none");
        snprintf(line_context,
                 sizeof(line_context),
                 "Graph  %zu anchors   %zu walls",
                 anchors,
                 walls);
        snprintf(line_locks,
                 sizeof(line_locks),
                 "Pick from the list or click an object origin.");
    } else {
        char w_text[32] = {0};
        char h_text[32] = {0};
        char d_text[32] = {0};
        snprintf(line_selected,
                 sizeof(line_selected),
                 "Selection  #%u  %s",
                 object->objectId,
                 UIPanelSceneSummary_KindLabel(object->kind));
        if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            UIPanelSceneSummary_FormatDimension(object->rectPrism.width, w_text, sizeof(w_text));
            UIPanelSceneSummary_FormatDimension(object->rectPrism.height, h_text, sizeof(h_text));
            UIPanelSceneSummary_FormatDimension(object->rectPrism.depth, d_text, sizeof(d_text));
        } else {
            UIPanelSceneSummary_FormatDimension(object->plane.width, w_text, sizeof(w_text));
            UIPanelSceneSummary_FormatDimension(object->plane.height, h_text, sizeof(h_text));
            snprintf(d_text, sizeof(d_text), "n/a");
        }
        snprintf(line_context,
                 sizeof(line_context),
                 "Graph  %zu anchors   %zu walls",
                 anchors,
                 walls);
        snprintf(line_locks,
                 sizeof(line_locks),
                 "Center  %.1f, %.1f, %.1f   Size %s x %s x %s",
                 object->transform.position.x,
                 object->transform.position.y,
                 object->transform.position.z,
                 w_text,
                 h_text,
                 d_text);
    }

    UIPanelSummary_DrawTextClipped(renderer, font, line_selected, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, value_color);
    y += font_h + line_gap;
    UIPanelSummary_DrawTextClipped(renderer, font, line_context, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, value_color);
    y += font_h + line_gap;
    if (object) {
        char line_lock_state[192];
        bool lock_plane = false;
        bool lock_bounds = false;
        if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            lock_plane = object->rectPrism.lockToConstructionPlane;
            lock_bounds = object->rectPrism.lockToBounds;
        } else {
            lock_plane = object->plane.lockToConstructionPlane;
            lock_bounds = object->plane.lockToBounds;
        }
        snprintf(line_lock_state,
                 sizeof(line_lock_state),
                 "Locks  Plane:%s  Bounds:%s   Details stay below.",
                 lock_plane ? "On" : "Off",
                 lock_bounds ? "On" : "Off");
        UIPanelSummary_DrawTextClipped(renderer, font, line_locks, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, value_color);
        y += font_h + line_gap;
        UIPanelSummary_DrawTextClipped(renderer, font, line_lock_state, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, label_color);
        return;
    }

    UIPanelSummary_DrawTextClipped(renderer, font, line_locks, panel.x + metrics.pad_x, y, panel.w - (metrics.pad_x * 2), font_h + 4, label_color);
}
