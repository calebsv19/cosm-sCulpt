#include "UI/topbar/line_drawing_editor_topbar.h"

#include "Core/space_mode_adapter.h"
#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Input/input_editor_actions.h"
#include "Layout/layout.h"
#include "Layout/scene/layout_object_faces.h"
#include "UI/font_manager.h"
#include "UI/text_draw.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_visual_style.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    SDL_Rect pane_rect;
    SDL_Rect scene_button;
    SDL_Rect object_button;
    SDL_Rect primary_rect;
    SDL_Rect file_rect;
    SDL_Rect status_rect;
    SDL_Rect chips_row_rect;
    SDL_Rect mode_chip;
    SDL_Rect view_chip;
    SDL_Rect plane_chip;
    SDL_Rect cp_chip;
    SDL_Rect bounds_chip;
    SDL_Rect gizmo_chip;
    bool valid;
} LineDrawingEditorTopbarLayout;

typedef struct {
    SDL_Color fill;
    SDL_Color fill_alt;
    SDL_Color fill_active;
    SDL_Color border;
    SDL_Color divider;
    SDL_Color text;
    SDL_Color text_muted;
    SDL_Color accent;
} LineDrawingTopbarColors;

enum {
    TOPBAR_CHIP_GAP = 6,
    TOPBAR_MODE_CHIP_W = 112,
    TOPBAR_VIEW_CHIP_W = 86,
    TOPBAR_PLANE_CHIP_W = 118,
    TOPBAR_CP_CHIP_W = 150,
    TOPBAR_BOUNDS_CHIP_W = 142,
    TOPBAR_GIZMO_CHIP_W = 96
};

static bool Topbar_PointInRect(int x, int y, SDL_Rect rect) {
    return rect.w > 0 && rect.h > 0 &&
           x >= rect.x && x <= rect.x + rect.w &&
           y >= rect.y && y <= rect.y + rect.h;
}

static bool Topbar_ResolvePaneRect(SDL_Rect* out_rect) {
    const LineDrawingPaneHost* pane_host = NULL;
    CorePaneRect pane_rect = {0};

    if (!out_rect) return false;
    *out_rect = (SDL_Rect){0, 0, 0, 0};

    pane_host = Global_GetPaneHostConst();
    if (!pane_host || !pane_host->initialized) return false;
    if (!LineDrawingPaneHost_GetRectForRole(pane_host, LINE_DRAWING_PANE_ROLE_TOP_BAR, &pane_rect)) {
        return false;
    }

    *out_rect = (SDL_Rect){
        (int)pane_rect.x,
        (int)pane_rect.y,
        (int)pane_rect.width,
        (int)pane_rect.height
    };
    return out_rect->w > 0 && out_rect->h > 0;
}

static LineDrawingEditorTopbarLayout Topbar_ResolveLayout(void) {
    LineDrawingEditorTopbarLayout layout = {0};
    SDL_Rect pane_rect = {0, 0, 0, 0};
    const int pad = 10;
    const int gap = 6;
    const int button_w = 76;
    const int status_w = 172;
    int row_h = 24;
    int row1_y = 0;
    int row2_y = 0;
    int left = 0;
    int right = 0;

    if (!Topbar_ResolvePaneRect(&pane_rect)) {
        return layout;
    }

    row_h = (pane_rect.h - (pad * 2) - gap) / 2;
    if (row_h < 20) row_h = 20;
    if (row_h > 28) row_h = 28;
    row1_y = pane_rect.y + pad;
    row2_y = row1_y + row_h + gap;
    if (row2_y + row_h > pane_rect.y + pane_rect.h - 4) {
        row2_y = pane_rect.y + pane_rect.h - row_h - 4;
    }

    left = pane_rect.x + pad;
    right = pane_rect.x + pane_rect.w - pad;

    layout.pane_rect = pane_rect;
    layout.scene_button = (SDL_Rect){left, row1_y, button_w, row_h};
    layout.object_button = (SDL_Rect){left + button_w - 1, row1_y, button_w, row_h};
    layout.status_rect = (SDL_Rect){right - status_w, row1_y, status_w, row_h};
    if (layout.status_rect.x < layout.object_button.x + layout.object_button.w + gap) {
        layout.status_rect.x = right;
        layout.status_rect.w = 0;
    }
    layout.primary_rect = (SDL_Rect){
        layout.object_button.x + layout.object_button.w + gap,
        row1_y,
        layout.status_rect.x - (layout.object_button.x + layout.object_button.w + (gap * 2)),
        row_h
    };
    if (layout.primary_rect.w < 0) layout.primary_rect.w = 0;
    layout.file_rect = (SDL_Rect){left, row2_y, 230, row_h};
    if (layout.file_rect.w > pane_rect.w / 3) layout.file_rect.w = pane_rect.w / 3;
    if (layout.file_rect.w < 150) layout.file_rect.w = 150;
    layout.chips_row_rect = (SDL_Rect){
        layout.file_rect.x + layout.file_rect.w + gap,
        row2_y,
        right - (layout.file_rect.x + layout.file_rect.w + gap),
        row_h
    };
    if (layout.chips_row_rect.w < 0) layout.chips_row_rect.w = 0;
    {
        int chip_x = layout.chips_row_rect.x;
        const int chip_right = layout.chips_row_rect.x + layout.chips_row_rect.w;
        layout.mode_chip = (SDL_Rect){chip_x, row2_y, TOPBAR_MODE_CHIP_W, row_h};
        if (layout.mode_chip.x + layout.mode_chip.w > chip_right) {
            layout.mode_chip = (SDL_Rect){0, 0, 0, 0};
        }
        chip_x += TOPBAR_MODE_CHIP_W + TOPBAR_CHIP_GAP;
        layout.view_chip = (SDL_Rect){chip_x, row2_y, TOPBAR_VIEW_CHIP_W, row_h};
        if (layout.view_chip.x + layout.view_chip.w > chip_right) {
            layout.view_chip = (SDL_Rect){0, 0, 0, 0};
        }
        chip_x += TOPBAR_VIEW_CHIP_W + TOPBAR_CHIP_GAP;
        layout.plane_chip = (SDL_Rect){chip_x, row2_y, TOPBAR_PLANE_CHIP_W, row_h};
        if (layout.plane_chip.x + layout.plane_chip.w > chip_right) {
            layout.plane_chip = (SDL_Rect){0, 0, 0, 0};
        }
        chip_x += TOPBAR_PLANE_CHIP_W + TOPBAR_CHIP_GAP;
        layout.cp_chip = (SDL_Rect){chip_x, row2_y, TOPBAR_CP_CHIP_W, row_h};
        if (layout.cp_chip.x + layout.cp_chip.w > chip_right) {
            layout.cp_chip = (SDL_Rect){0, 0, 0, 0};
        }
        chip_x += TOPBAR_CP_CHIP_W + TOPBAR_CHIP_GAP;
        layout.bounds_chip = (SDL_Rect){chip_x, row2_y, TOPBAR_BOUNDS_CHIP_W, row_h};
        if (layout.bounds_chip.x + layout.bounds_chip.w > chip_right) {
            layout.bounds_chip = (SDL_Rect){0, 0, 0, 0};
        }
        chip_x += TOPBAR_BOUNDS_CHIP_W + TOPBAR_CHIP_GAP;
        layout.gizmo_chip = (SDL_Rect){chip_x, row2_y, TOPBAR_GIZMO_CHIP_W, row_h};
        if (layout.gizmo_chip.x + layout.gizmo_chip.w > chip_right) {
            layout.gizmo_chip = (SDL_Rect){0, 0, 0, 0};
        }
    }
    layout.valid = true;
    return layout;
}

static void Topbar_DrawTextClipped(SDL_Renderer* renderer,
                                   TTF_Font* font,
                                   const char* text,
                                   SDL_Rect clip_rect,
                                   int x,
                                   int y,
                                   SDL_Color color) {
    SDL_Rect previous_clip = {0, 0, 0, 0};
    SDL_bool had_clip = SDL_FALSE;
    if (!renderer || !font || !text || !text[0]) return;
    if (clip_rect.w > 0 && clip_rect.h > 0) {
        had_clip = SDL_RenderIsClipEnabled(renderer);
        if (had_clip) {
            (void)SDL_RenderGetClipRect(renderer, &previous_clip);
        }
        (void)SDL_RenderSetClipRect(renderer, &clip_rect);
    }
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
    if (clip_rect.w > 0 && clip_rect.h > 0) {
        (void)SDL_RenderSetClipRect(renderer, had_clip ? &previous_clip : NULL);
    }
}

static LineDrawingTopbarColors Topbar_ResolveColors(void) {
    UIPanelVisualPalette panel_palette = {0};
    LineDrawingTopbarColors colors = {
        .fill = {28, 30, 35, 242},
        .fill_alt = {36, 40, 48, 232},
        .fill_active = {70, 86, 118, 242},
        .border = {86, 98, 116, 230},
        .divider = {70, 78, 92, 210},
        .text = {226, 232, 240, 255},
        .text_muted = {174, 184, 198, 255},
        .accent = {138, 168, 210, 255}
    };

    if (UIPanelVisual_ResolvePalette(&panel_palette)) {
        colors.fill = panel_palette.pane_fill;
        colors.fill.a = 255;
        colors.fill_alt = panel_palette.workspace_fill;
        colors.fill_alt.a = 255;
        colors.fill_active = panel_palette.button_fill_active;
        colors.fill_active.a = 255;
        colors.border = panel_palette.pane_border;
        colors.border.a = 230;
        colors.divider = panel_palette.pane_divider;
        colors.divider.a = 210;
        colors.text = panel_palette.text_primary;
        colors.text_muted = panel_palette.text_muted;
        colors.accent = panel_palette.accent;
    }
    colors.fill.a = 255;
    colors.fill_alt.a = 255;
    colors.fill_active.a = 255;
    return colors;
}

static void Topbar_DrawFrame(SDL_Renderer* renderer,
                             SDL_Rect rect,
                             SDL_Color fill,
                             SDL_Color border) {
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    (void)SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    (void)SDL_RenderDrawRect(renderer, &rect);
}

static void Topbar_DrawButton(SDL_Renderer* renderer,
                              TTF_Font* font,
                              SDL_Rect bounds,
                              const char* label,
                              bool active,
                              LineDrawingTopbarColors colors) {
    int text_w = 0;
    int text_h = 0;
    SDL_Color resolved_fill = active ? colors.fill_active : colors.fill_alt;
    SDL_Color resolved_text = active ? colors.text : colors.text_muted;
    SDL_Rect text_clip = {bounds.x + 5, bounds.y + 2, bounds.w - 10, bounds.h - 4};

    if (!renderer || !font || bounds.w <= 0 || bounds.h <= 0) return;
    Topbar_DrawFrame(renderer, bounds, resolved_fill, colors.border);
    if (active) {
        SDL_Rect accent = {bounds.x + 1, bounds.y + bounds.h - 3, bounds.w - 2, 2};
        SDL_SetRenderDrawColor(renderer, colors.accent.r, colors.accent.g, colors.accent.b, 235);
        (void)SDL_RenderFillRect(renderer, &accent);
    }

    if (!line_drawing_text_measure_utf8(renderer, font, label, &text_w, &text_h)) return;
    Topbar_DrawTextClipped(renderer,
                           font,
                           label,
                           text_clip,
                           bounds.x + (bounds.w - text_w) / 2,
                           bounds.y + (bounds.h - text_h) / 2,
                           resolved_text);
}

static void Topbar_DrawChip(SDL_Renderer* renderer,
                            TTF_Font* font,
                            SDL_Rect bounds,
                            const char* label,
                            const char* value,
                            bool active,
                            LineDrawingTopbarColors colors) {
    int font_h = font ? TTF_FontHeight(font) : 14;
    int label_w = 0;
    SDL_Rect text_clip = {bounds.x + 8, bounds.y + 2, bounds.w - 16, bounds.h - 4};
    SDL_Color fill = active ? colors.fill_active : colors.fill_alt;
    SDL_Color value_color = active ? colors.text : colors.text;
    char text[256] = {0};

    if (!renderer || !font || bounds.w <= 0 || bounds.h <= 0) return;
    if (label && label[0] && value && value[0]) {
        (void)snprintf(text, sizeof(text), "%s  %s", label, value);
    } else if (value && value[0]) {
        (void)snprintf(text, sizeof(text), "%s", value);
    } else if (label && label[0]) {
        (void)snprintf(text, sizeof(text), "%s", label);
    }
    if (!text[0]) return;

    Topbar_DrawFrame(renderer, bounds, fill, colors.border);
    if (label && label[0] && value && value[0] &&
        line_drawing_text_measure_utf8(renderer, font, label, &label_w, NULL)) {
        Topbar_DrawTextClipped(renderer,
                               font,
                               label,
                               text_clip,
                               bounds.x + 8,
                               bounds.y + (bounds.h - font_h) / 2,
                               colors.text_muted);
        Topbar_DrawTextClipped(renderer,
                               font,
                               value,
                               text_clip,
                               bounds.x + 8 + label_w + 10,
                               bounds.y + (bounds.h - font_h) / 2,
                               value_color);
        return;
    }
    Topbar_DrawTextClipped(renderer,
                           font,
                           text,
                           text_clip,
                           bounds.x + 8,
                           bounds.y + (bounds.h - font_h) / 2,
                           value_color);
}

static const char* Topbar_ObjectKindLabel(Object3DKind kind) {
    switch (kind) {
        case OBJECT3D_KIND_PLANE: return "Plane";
        case OBJECT3D_KIND_RECT_PRISM: return "Prism";
        case OBJECT3D_KIND_MESH_ASSET_INSTANCE: return "MeshAsset";
        case OBJECT3D_KIND_UNKNOWN:
        default: return "Unknown";
    }
}

static const char* Topbar_BaseName(const char* path) {
    const char* base = path ? strrchr(path, '/') : NULL;
    if (base && base[1]) return base + 1;
    if (path && path[0]) return path;
    return "(unsaved)";
}

static void Topbar_FormatPrimary(GlobalState* state, char* out, size_t out_size) {
    const Object3D* selected_object = NULL;
    const Object3D* hovered_object = NULL;
    EditorState* editor = NULL;
    Layout* layout = NULL;

    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!state) {
        (void)snprintf(out, out_size, "Sculpts");
        return;
    }

    editor = &state->editor;
    layout = &state->layout;
    if (editor->selectedObject3DId != 0u) {
        selected_object = Layout_ObjectStore_FindConst(&layout->objectStore,
                                                       editor->selectedObject3DId);
    }
    if (!selected_object && editor->hoveredObject3DId != 0u) {
        hovered_object = Layout_ObjectStore_FindConst(&layout->objectStore,
                                                     editor->hoveredObject3DId);
    }

    if (Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT) {
        const size_t body_count = Layout_ObjectStore_LiveCount(&layout->objectStore);
        const size_t sketch_count = state->objectAuthoring.attached
            ? state->objectAuthoring.document.sketchCount
            : 0u;
        const size_t op_count = state->objectAuthoring.attached
            ? state->objectAuthoring.document.operationCount
            : 0u;
        if (selected_object) {
            (void)snprintf(out,
                           out_size,
                           "Object Asset  Body #%u  %s  Face %s  Sketches %zu  Ops %zu",
                           editor->selectedObjectAssetBodyId != 0u
                               ? editor->selectedObjectAssetBodyId
                               : selected_object->objectId,
                           Topbar_ObjectKindLabel(selected_object->kind),
                           Layout_Object3DFaceKind_Label(editor->selectedObjectAssetFace),
                           sketch_count,
                           op_count);
        } else {
            (void)snprintf(out,
                           out_size,
                           "Object Asset  Bodies %zu  Sketches %zu  Ops %zu",
                           body_count,
                           sketch_count,
                           op_count);
        }
        return;
    }

    if (selected_object) {
        (void)snprintf(out,
                       out_size,
                       "Selected  Object #%u  %s  Pos %.2f, %.2f, %.2f",
                       selected_object->objectId,
                       Topbar_ObjectKindLabel(selected_object->kind),
                       selected_object->transform.position.x,
                       selected_object->transform.position.y,
                       selected_object->transform.position.z);
    } else if (hovered_object) {
        (void)snprintf(out,
                       out_size,
                       "Hover  Object #%u  %s  Pos %.2f, %.2f, %.2f",
                       hovered_object->objectId,
                       Topbar_ObjectKindLabel(hovered_object->kind),
                       hovered_object->transform.position.x,
                       hovered_object->transform.position.y,
                       hovered_object->transform.position.z);
    } else if (editor->selectedAnchorIndex >= 0 &&
               editor->selectedAnchorIndex < (int)layout->anchorCount) {
        Anchor* anchor = &layout->anchors[editor->selectedAnchorIndex];
        (void)snprintf(out,
                       out_size,
                       "Selected  Anchor #%d  Pos %.2f, %.2f, %.2f",
                       editor->selectedAnchorIndex,
                       anchor->pos.x,
                       anchor->pos.y,
                       anchor->pos.z);
    } else if (editor->selectedWallIndex >= 0 &&
               editor->selectedWallIndex < (int)layout->wallCount) {
        (void)snprintf(out, out_size, "Selected  Wall #%d", editor->selectedWallIndex);
    } else {
        (void)snprintf(out, out_size, "Sculpts  Scene authoring workspace");
    }
}

static void Topbar_FormatPlane(GlobalState* state, char* out, size_t out_size) {
    SpaceViewContext view_ctx = {0};
    const char* plane_label = "XY";
    const char* coord_label = "z";
    if (!out || out_size == 0) return;
    if (!state) {
        (void)snprintf(out, out_size, "XY z=0.00");
        return;
    }
    view_ctx = SpaceAdapter_BuildViewContext(state);
    if (view_ctx.plane.axis == VIEW_PLANE_YZ) {
        plane_label = "YZ";
        coord_label = "x";
    } else if (view_ctx.plane.axis == VIEW_PLANE_XZ) {
        plane_label = "XZ";
        coord_label = "y";
    }
    (void)snprintf(out,
                   out_size,
                   "%s %s=%.2f",
                   plane_label,
                   coord_label,
                   view_ctx.plane.offset);
}

static void Topbar_FormatConstructionPlane(GlobalState* state, char* out, size_t out_size) {
    const ConstructionPlane3D* cp = NULL;
    const char* plane_label = "XY";
    const char* coord_label = "z";
    if (!out || out_size == 0) return;
    if (!state) {
        (void)snprintf(out, out_size, "Axis XY");
        return;
    }
    cp = &state->layout.scene3d.constructionPlane;
    if (cp->mode != CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED) {
        (void)snprintf(out, out_size, "Custom");
        return;
    }
    if (cp->axisAligned.axis == VIEW_PLANE_YZ) {
        plane_label = "YZ";
        coord_label = "x";
    } else if (cp->axisAligned.axis == VIEW_PLANE_XZ) {
        plane_label = "XZ";
        coord_label = "y";
    }
    (void)snprintf(out,
                   out_size,
                   "Axis %s %s=%.2f",
                   plane_label,
                   coord_label,
                   cp->axisAligned.offset);
}

static void Topbar_FormatBounds(GlobalState* state, char* out, size_t out_size) {
    const SceneBounds3D* bounds = NULL;
    if (!out || out_size == 0) return;
    if (!state) {
        (void)snprintf(out, out_size, "Off");
        return;
    }
    bounds = &state->layout.scene3d.bounds;
    (void)snprintf(out,
                   out_size,
                   "%s  Clamp %s",
                   bounds->enabled ? "On" : "Off",
                   bounds->clampOnEdit ? "On" : "Off");
}

static void Topbar_DrawStatusChips(SDL_Renderer* renderer,
                                   TTF_Font* font,
                                   LineDrawingEditorTopbarLayout layout,
                                   LineDrawingTopbarColors colors,
                                   GlobalState* state) {
    char file_value[128] = {0};
    char primary_value[256] = {0};
    char status_value[128] = {0};
    char plane_value[64] = {0};
    char cp_value[80] = {0};
    char bounds_value[80] = {0};
    char mode_value[48] = {0};
    char view_value[48] = {0};
    char gizmo_value[48] = {0};
    SDL_Rect chip = layout.chips_row_rect;
    int x = layout.chips_row_rect.x;
    const int gap = 6;
    const int h = layout.chips_row_rect.h;
    const char* file_name = Topbar_BaseName(state ? Global_GetCurrentConfigPath() : NULL);
    const bool dirty = state ? Global_IsLayoutDirty() : false;
    const bool object_mode =
        state && Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    const SpaceViewContext view_ctx = state ? SpaceAdapter_BuildViewContext(state) : (SpaceViewContext){0};
    const char* view_label = state && SpaceAdapter_IsFreeViewEnabled(&view_ctx) ? "Free" : "Plane";
    const size_t undo_count = state ? Editor_UndoCount(&state->editor) : 0u;
    const size_t redo_count = state ? Editor_RedoCount(&state->editor) : 0u;

    (void)snprintf(file_value, sizeof(file_value), "%s%s", file_name, dirty ? " *" : "");
    Topbar_FormatPrimary(state, primary_value, sizeof(primary_value));
    Topbar_FormatPlane(state, plane_value, sizeof(plane_value));
    Topbar_FormatConstructionPlane(state, cp_value, sizeof(cp_value));
    Topbar_FormatBounds(state, bounds_value, sizeof(bounds_value));
    (void)snprintf(mode_value,
                   sizeof(mode_value),
                   "%s / %s",
                   object_mode ? "Object" : "Scene",
                   state ? Global_GetSpaceModeLabel(state->spaceMode) : "3D");
    (void)snprintf(view_value, sizeof(view_value), "%s", view_label);
    (void)snprintf(gizmo_value,
                   sizeof(gizmo_value),
                   "%s",
                   state ? UIPanel_ObjectGizmoModeLabel() : "Move");
    (void)snprintf(status_value,
                   sizeof(status_value),
                   "%s  Undo %zu  Redo %zu",
                   dirty ? "Dirty" : "Saved",
                   undo_count,
                   redo_count);

    Topbar_DrawChip(renderer, font, layout.primary_rect, NULL, primary_value, false, colors);
    Topbar_DrawChip(renderer, font, layout.status_rect, NULL, status_value, dirty, colors);
    Topbar_DrawChip(renderer, font, layout.file_rect, "File", file_value, dirty, colors);

#define DRAW_NEXT_CHIP(width, label, value, active) \
    do { \
        chip = (SDL_Rect){x, layout.chips_row_rect.y, (width), h}; \
        if (chip.x + chip.w <= layout.chips_row_rect.x + layout.chips_row_rect.w) { \
            Topbar_DrawChip(renderer, font, chip, (label), (value), (active), colors); \
        } \
        x += (width) + TOPBAR_CHIP_GAP; \
    } while (0)

    (void)gap;
    DRAW_NEXT_CHIP(TOPBAR_MODE_CHIP_W,
                   "Mode",
                   mode_value,
                   state && state->spaceMode == SPACE_MODE_3D);
    DRAW_NEXT_CHIP(TOPBAR_VIEW_CHIP_W,
                   "View",
                   view_value,
                   state && SpaceAdapter_IsFreeViewEnabled(&view_ctx));
    DRAW_NEXT_CHIP(TOPBAR_PLANE_CHIP_W, "Plane", plane_value, false);
    DRAW_NEXT_CHIP(TOPBAR_CP_CHIP_W, "CP", cp_value, false);
    DRAW_NEXT_CHIP(TOPBAR_BOUNDS_CHIP_W,
                   "Bounds",
                   bounds_value,
                   state && state->layout.scene3d.bounds.enabled);
    DRAW_NEXT_CHIP(TOPBAR_GIZMO_CHIP_W,
                   "Gizmo",
                   gizmo_value,
                   state && (state->editor.object3DRotateMode || state->editor.object3DSizeMode));

#undef DRAW_NEXT_CHIP
}

bool LineDrawingEditorTopbar_HandleClick(int mouse_x, int mouse_y) {
    LineDrawingEditorTopbarLayout layout = Topbar_ResolveLayout();

    if (!layout.valid) return false;
    if (Topbar_PointInRect(mouse_x, mouse_y, layout.scene_button)) {
        if (Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_SCENE)) {
            UIPanel_ResetTransientUiState();
        }
        return true;
    }
    if (Topbar_PointInRect(mouse_x, mouse_y, layout.object_button)) {
        if (Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT)) {
            UIPanel_ResetTransientUiState();
        }
        return true;
    }
    if (Topbar_PointInRect(mouse_x, mouse_y, layout.mode_chip)) {
        UIPanel_ResetTransientUiState();
        (void)InputEditorAction_ToggleSpaceMode(true);
        return true;
    }
    if (Topbar_PointInRect(mouse_x, mouse_y, layout.view_chip)) {
        UIPanel_ResetTransientUiState();
        (void)InputEditorAction_ToggleFreeView();
        return true;
    }
    if (Topbar_PointInRect(mouse_x, mouse_y, layout.bounds_chip)) {
        UIPanel_ResetTransientUiState();
        (void)UIPanel_ToggleSceneBoundsEnabled();
        return true;
    }
    if (Topbar_PointInRect(mouse_x, mouse_y, layout.gizmo_chip)) {
        UIPanel_ResetTransientUiState();
        (void)InputEditorAction_ToggleObjectGizmoMode();
        return true;
    }
    return false;
}

void LineDrawingEditorTopbar_Render(SDL_Renderer* renderer) {
    LineDrawingEditorTopbarLayout layout = Topbar_ResolveLayout();
    TTF_Font* font = NULL;
    GlobalState* state = Global_Get();
    LineDrawingTopbarColors colors = Topbar_ResolveColors();
    const bool object_mode =
        state && Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;

    if (!renderer || !layout.valid) return;
    font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;

    Topbar_DrawFrame(renderer, layout.pane_rect, colors.fill, colors.divider);
    Topbar_DrawButton(renderer,
                      font,
                      layout.scene_button,
                      "Scene",
                      !object_mode,
                      colors);
    Topbar_DrawButton(renderer,
                      font,
                      layout.object_button,
                      "Object",
                      object_mode,
                      colors);
    Topbar_DrawStatusChips(renderer, font, layout, colors, state);
}
