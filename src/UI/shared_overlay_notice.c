#include "UI/font_manager.h"
#include "UI/shared_overlay.h"
#include "UI/shared_text.h"
#include "UI/shared_theme_font_adapter.h"
bool LDOverlay_NoticeRect(const GlobalState *s, SDL_Rect *out) {
    if (!s || !out || !s->layout.geometryMessage[0])
        return false;
    CorePaneRect viewport;
    if (!LineDrawingPaneHost_GetViewportRect(&s->paneHost, &viewport))
        return false;
    int width = (int)viewport.width - 24, height = (int)viewport.height - 24;
    TTF_Font *font = FontManager_Get(FONT_DEFAULT);
    int line = font ? TTF_FontHeight(font) : 16;
    if (width < 32 || height < line + 12)
        return false;
    int rows = LDText_CountLines(font, s->layout.geometryMessage, width - 12);
    if (rows < 1)
        rows = 1;
    if (rows > 3)
        rows = 3;
    int h = rows * (line + 2) + 12;
    if (h > height)
        h = height;
    *out = (SDL_Rect){(int)viewport.x + 12, (int)(viewport.y + viewport.height) - h - 12, width, h};
    return true;
}
void LDOverlay_NoticeDraw(SDL_Renderer *renderer, const GlobalState *s) {
    SDL_Rect box;
    if (!renderer || !LDOverlay_NoticeRect(s, &box))
        return;
    SDL_Rect old;
    SDL_bool clipped = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &old);
    if (clipped && !SDL_IntersectRect(&box, &old, &box))
        return;
    SDL_RenderSetClipRect(renderer, &box);
    LineDrawing3dThemePalette p = {0};
    (void)line_drawing3d_shared_theme_resolve_palette(&p);
    SDL_SetRenderDrawColor(renderer, p.panel_fill.r, p.panel_fill.g, p.panel_fill.b, 245);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, p.panel_border.r, p.panel_border.g, p.panel_border.b, 255);
    SDL_RenderDrawRect(renderer, &box);
    TTF_Font *font = FontManager_Get(FONT_DEFAULT);
    (void)LDText_LabelRows(renderer, font, s->layout.geometryMessage,
                           (SDL_Rect){box.x + 6, box.y + 6, box.w - 12, box.h - 12}, p.text_primary,
                           font ? TTF_FontHeight(font) + 2 : 18);
    SDL_RenderSetClipRect(renderer, clipped ? &old : NULL);
}
