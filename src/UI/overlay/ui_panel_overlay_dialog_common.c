#include "UI/overlay/ui_panel_overlay_render_internal.h"

#include "UI/font_manager.h"
#include "UI/shared_text.h"
#include "UI/text_draw.h"
#include "Core/global_state.h"

#include <stdio.h>
#include <string.h>

void UIPanelOverlay_DrawText(SDL_Renderer* renderer,
                             const char* text,
                             int x,
                             int y,
                             SDL_Color color) {
    if (!text || !*text) return;
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;
    (void)LDText_Label(renderer,font,text,(SDL_Rect){x,y,Global_GetScreenWidth()-x-12,TTF_FontHeight(font)},color,false);
}

void UIPanelOverlay_DrawTextClipped(SDL_Renderer* renderer,
                                    const char* text,
                                    int x,
                                    int y,
                                    int max_width,
                                    SDL_Color color) {
    TTF_Font* font=FontManager_Get(FONT_DEFAULT);
    if(font)(void)LDText_Label(renderer,font,text,(SDL_Rect){x,y,max_width,TTF_FontHeight(font)},color,false);
}
