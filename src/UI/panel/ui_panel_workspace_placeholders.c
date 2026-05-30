#include "UI/panel/ui_panel_workspace_placeholders.h"

#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/text_draw.h"

#include <SDL2/SDL_ttf.h>

static void Placeholder_DrawText(SDL_Renderer* renderer,
                                 TTF_Font* font,
                                 const char* text,
                                 int x,
                                 int y,
                                 SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

static void Placeholder_DrawCard(SDL_Renderer* renderer,
                                 SDL_Rect rect,
                                 const char* title,
                                 const char* line1,
                                 const char* line2,
                                 const char* line3) {
    TTF_Font* font = NULL;
    LineDrawing3dThemePalette palette = {0};
    SDL_Color fill = {32, 35, 42, 214};
    SDL_Color border = {90, 100, 116, 240};
    SDL_Color title_color = {232, 236, 244, 255};
    SDL_Color body_color = {186, 194, 208, 255};
    int line_y = 0;
    const int pad = 12;
    const int line_gap = 6;
    int text_h = 16;

    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
    font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;
    text_h = TTF_FontHeight(font);
    if (text_h < 14) text_h = 14;

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        fill = palette.panel_fill;
        border = palette.panel_border;
        title_color = palette.text_primary;
        body_color = palette.text_muted;
    }

    fill.a = 214;
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    (void)SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    (void)SDL_RenderDrawRect(renderer, &rect);

    line_y = rect.y + pad;
    Placeholder_DrawText(renderer, font, title, rect.x + pad, line_y, title_color);
    line_y += text_h + line_gap;
    Placeholder_DrawText(renderer, font, line1, rect.x + pad, line_y, body_color);
    line_y += text_h + line_gap;
    Placeholder_DrawText(renderer, font, line2, rect.x + pad, line_y, body_color);
    if (line3 && line3[0]) {
        line_y += text_h + line_gap;
        Placeholder_DrawText(renderer, font, line3, rect.x + pad, line_y, body_color);
    }
}

void Render_UIPanelObjectWorkspaceLeftPlaceholder(const UIPanelState* ui, SDL_Renderer* renderer) {
    if (!ui || !renderer) return;

    if (ui->activeLeftTab == UI_PANEL_LEFT_TAB_SCENE) {
        Placeholder_DrawCard(renderer,
                             ui->objectWorkspacePane.summaryRect,
                             "Object Workspace",
                             "Reusable mesh-asset authoring scaffold is active.",
                             "Closed profiles, extrude, inset, and face editing land next.",
                             "Use the top bar to switch back to Scene Workspace.");
        Placeholder_DrawCard(renderer,
                             ui->objectWorkspacePane.browserRect,
                             "Asset Browser",
                             "Saved object assets are not wired into this pane yet.",
                             "This area will become the local object structure and asset list.",
                             "");
    }
}

void Render_UIPanelObjectWorkspaceRightPlaceholder(const UIPanelState* ui, SDL_Renderer* renderer) {
    if (!ui || !renderer) return;

    if (ui->activeRightTab == UI_PANEL_RIGHT_TAB_CREATE) {
        Placeholder_DrawCard(renderer,
                             ui->createPane.summaryRect,
                             "Shape Tools",
                             "Shape-authoring controls are scaffolded but not active yet.",
                             "The first editing lane will focus on profile-to-mesh creation.",
                             "");
        Placeholder_DrawCard(renderer,
                             ui->createPane.workspaceRect,
                             "Near-Term Tool Slice",
                             "1. Draw a closed profile on a construction plane.",
                             "2. Extrude into a reusable mesh asset.",
                             "3. Add inset and face-move operations.");
    } else if (ui->activeRightTab == UI_PANEL_RIGHT_TAB_OBJECT) {
        Placeholder_DrawCard(renderer,
                             ui->objectPane.summaryRect,
                             "Asset Inspector",
                             "Object-asset metadata and save controls land in this tab.",
                             "Per-asset bounds, pivot, and compile hints will live here.",
                             "");
        Placeholder_DrawCard(renderer,
                             ui->objectPane.detailsRect,
                             "Headless Authoring Path",
                             "This lane will also host prompt-driven object creation hooks.",
                             "The same shared mesh asset contract will back manual and agent-built objects.",
                             "");
    }
}
