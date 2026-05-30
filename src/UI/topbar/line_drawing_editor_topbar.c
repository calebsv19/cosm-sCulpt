#include "UI/topbar/line_drawing_editor_topbar.h"

#include "Core/global_state.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/text_draw.h"
#include "UI/ui_panel.h"

#include <SDL2/SDL_ttf.h>

typedef struct {
    SDL_Rect scene_button;
    SDL_Rect object_button;
    SDL_Rect label_rect;
    bool valid;
} LineDrawingEditorTopbarLayout;

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
    const int pad = 12;
    const int gap = 8;
    const int button_w = 84;
    int button_h = 28;
    int buttons_y = 0;
    int right_edge = 0;

    if (!Topbar_ResolvePaneRect(&pane_rect)) {
        return layout;
    }

    button_h = pane_rect.h - (pad * 2);
    if (button_h < 24) button_h = 24;
    if (button_h > 32) button_h = 32;

    buttons_y = pane_rect.y + (pane_rect.h - button_h) / 2;
    right_edge = pane_rect.x + pane_rect.w - pad;

    layout.object_button = (SDL_Rect){
        right_edge - button_w,
        buttons_y,
        button_w,
        button_h
    };
    layout.scene_button = (SDL_Rect){
        layout.object_button.x - gap - button_w,
        buttons_y,
        button_w,
        button_h
    };
    layout.label_rect = (SDL_Rect){
        pane_rect.x + pad,
        buttons_y,
        layout.scene_button.x - (pane_rect.x + (pad * 2)),
        button_h
    };
    if (layout.label_rect.w < 0) layout.label_rect.w = 0;
    layout.valid = true;
    return layout;
}

static void Topbar_DrawText(SDL_Renderer* renderer,
                            TTF_Font* font,
                            const char* text,
                            int x,
                            int y,
                            SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

static void Topbar_DrawButton(SDL_Renderer* renderer,
                              TTF_Font* font,
                              SDL_Rect bounds,
                              const char* label,
                              bool active,
                              SDL_Color fill,
                              SDL_Color fill_active,
                              SDL_Color border,
                              SDL_Color text,
                              SDL_Color text_active) {
    int text_w = 0;
    int text_h = 0;
    SDL_Color resolved_fill = active ? fill_active : fill;
    SDL_Color resolved_text = active ? text_active : text;

    if (!renderer || !font || bounds.w <= 0 || bounds.h <= 0) return;
    SDL_SetRenderDrawColor(renderer,
                           resolved_fill.r, resolved_fill.g,
                           resolved_fill.b, resolved_fill.a);
    (void)SDL_RenderFillRect(renderer, &bounds);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    (void)SDL_RenderDrawRect(renderer, &bounds);

    if (!line_drawing_text_measure_utf8(renderer, font, label, &text_w, &text_h)) return;
    Topbar_DrawText(renderer,
                    font,
                    label,
                    bounds.x + (bounds.w - text_w) / 2,
                    bounds.y + (bounds.h - text_h) / 2,
                    resolved_text);
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
    return false;
}

void LineDrawingEditorTopbar_Render(SDL_Renderer* renderer) {
    LineDrawingEditorTopbarLayout layout = Topbar_ResolveLayout();
    TTF_Font* font = NULL;
    GlobalState* state = Global_Get();
    LineDrawing3dThemePalette palette = {0};
    SDL_Color text = {214, 220, 230, 255};
    SDL_Color text_active = {255, 255, 255, 255};
    SDL_Color fill = {44, 48, 57, 220};
    SDL_Color fill_active = {84, 112, 168, 238};
    SDL_Color border = {102, 112, 128, 255};
    SDL_Color label = {180, 188, 204, 255};
    const bool object_mode =
        state && Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    const char* workspace_label =
        state ? Global_GetWorkspaceModeLabel(Global_GetWorkspaceMode()) : "Scene Workspace";

    if (!renderer || !layout.valid) return;
    font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        text = palette.text_primary;
        text_active = palette.text_primary;
        label = palette.text_muted;
        fill = palette.button_fill;
        fill_active = palette.menu_highlight;
        border = palette.panel_border;
    }

    Topbar_DrawText(renderer,
                    font,
                    workspace_label,
                    layout.label_rect.x,
                    layout.label_rect.y + 4,
                    label);
    Topbar_DrawButton(renderer,
                      font,
                      layout.scene_button,
                      "Scene",
                      !object_mode,
                      fill,
                      fill_active,
                      border,
                      text,
                      text_active);
    Topbar_DrawButton(renderer,
                      font,
                      layout.object_button,
                      "Object",
                      object_mode,
                      fill,
                      fill_active,
                      border,
                      text,
                      text_active);
}
