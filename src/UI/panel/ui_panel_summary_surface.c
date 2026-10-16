#include "Core/global_state.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/shared_text.h"

#include "UI/text_draw.h"

#include <string.h>

void UIPanelSummary_DrawText(SDL_Renderer* renderer,
                             TTF_Font* font,
                             const char* text,
                             int x,
                             int y,
                             SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) return;
    SDL_Rect clip={0,0,Global_GetScreenWidth(),Global_GetScreenHeight()};
    if(SDL_RenderIsClipEnabled(renderer))SDL_RenderGetClipRect(renderer,&clip);
    (void)LDText_Label(renderer,font,text,(SDL_Rect){x,y,clip.x+clip.w-x,TTF_FontHeight(font)},color,false);
}

void UIPanelSummary_DrawTextClipped(SDL_Renderer* renderer,TTF_Font* font,const char* text,
                                    int x,int y,int max_width,int clip_height,SDL_Color color) {
    (void)LDText_Label(renderer,font,text,(SDL_Rect){x,y,max_width,clip_height},color,false);
}
int UIPanelSummary_CountWrappedLines(TTF_Font* font,const char* text,int max_width) {
    return LDText_CountLines(font,text,max_width);
}
int UIPanelSummary_DrawWrappedText(SDL_Renderer* renderer,TTF_Font* font,const char* text,
    int x,int y,int max_width,int line_height,int line_gap,int max_lines,SDL_Color color) {
    if(!font || max_lines==0)return 0;
    if(line_height<=0)line_height=TTF_FontHeight(font);
    int count=LDText_CountLines(font,text,max_width);if(max_lines>0 && count>max_lines)count=max_lines;
    /* Shared rows use a measured line advance, including the host's spacing. */
    return LDText_LabelRows(renderer,font,text,(SDL_Rect){x,y,max_width,count*(line_height+line_gap)},color,line_height+line_gap)?count:0;
}

void UIPanelSummary_DrawCard(SDL_Renderer* renderer,
                             SDL_Rect rect,
                             SDL_Color fill_color,
                             SDL_Color border_color,
                             SDL_Color accent_color,
                             int accent_height) {
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
    UIPanelVisual_DrawFrame(renderer, rect, fill_color, border_color, 70);
    if (accent_height > 0) {
        UIPanelVisual_DrawAccentBand(renderer, rect, accent_color, 1, accent_height, 220);
    }
}

void UIPanelSummary_DrawDivider(SDL_Renderer* renderer,
                                SDL_Rect rect,
                                int y,
                                int inset_x,
                                SDL_Color color,
                                Uint8 alpha) {
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;
    if (inset_x < 0) inset_x = 0;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, alpha);
    SDL_RenderDrawLine(renderer,
                       rect.x + inset_x,
                       y,
                       rect.x + rect.w - inset_x,
                       y);
}
