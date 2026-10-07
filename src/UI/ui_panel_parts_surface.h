#pragma once
/* Shared Parts form layout/hit/render surface. Domain workflows remain in modules. */
#include "UI/ui_panel_parts.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/font_manager.h"
#include <stdio.h>
#include <string.h>
typedef struct PartsPane {
    SDL_Renderer* renderer;TTF_Font* font;UIPanelVisualPalette palette;
    SDL_Rect body,found;int y,h,x,click_y,hit,wanted;
} PartsPane;
static inline bool contains(SDL_Rect r,int x,int y) {return x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;}
static inline bool is_field(int action) {
    return action==PARTS_VIEW_NAME || action==PARTS_NAME || action==PARTS_KEY || action==PARTS_VALUE ||
        (action>=PARTS_DX && action<=PARTS_ANGLE) || (action>=PARTS_VOLUME_NAME && action<=PARTS_VOLUME_Z) || action==PARTS_CHECK_DISTANCE;
}
static inline void cell(PartsPane* p,int action,const char* title,const char* value,int column,int columns,bool enabled,bool selected) {
    int gap=4,w=(p->body.w-24-(columns-1)*gap)/columns;
    SDL_Rect rect={p->body.x+6+column*(w+gap),p->y,w,p->h-3};
    if (UIPanel_Get()->parts.mode == 2 && action >= 8000 && action < 8200) {
        int total = p->body.w - 24;
        int only_w = 48;
        TTF_Font* font = p->font ? p->font : FontManager_Get(FONT_DEFAULT);
        if (font) (void)TTF_SizeUTF8(font, "Only", &only_w, NULL);
        only_w += 18;
        /* The name absorbs spare width; Only remains a compact action. */
        if (only_w > total / 2) only_w = total / 2;
        int label_w = total - only_w - gap;
        bool only = action >= 8100;
        rect.x = p->body.x + 6 + (only ? label_w + gap : 0);
        rect.w = only ? only_w : label_w;
    }
    if (value) {
        int label_width=w/3;
        if (p->renderer && p->font) UIPanelSummary_DrawTextClipped(p->renderer,p->font,title,rect.x+2,rect.y+3,label_width-4,rect.h-4,p->palette.text_primary);
        rect.x+=label_width;rect.w-=label_width;title=value;
    }
    if (action==p->wanted) p->found=rect;
    if (action && enabled && contains(rect,p->x,p->click_y) && contains(p->body,p->x,p->click_y)) p->hit=action;
    if (!p->renderer) return;
    SDL_Color text=enabled ? p->palette.text_primary : p->palette.text_muted;
    if (action) {
        int x,y;Uint32 buttons=SDL_GetMouseState(&x,&y);bool hover=enabled && contains(rect,x,y) && contains(p->body,x,y);
        SDL_Color fill=is_field(action) ? UIPanelVisual_AdjustColor(p->palette.pane_fill,-8,0) :
            selected || (hover && (buttons&SDL_BUTTON_LMASK)) ? p->palette.button_fill_active : hover ? p->palette.button_fill_hover : p->palette.button_fill;
        UIPanelVisual_DrawFrame(p->renderer,rect,fill,p->palette.button_border,0);
        if(selected || UIPanel_Get()->parts.input==action) {
            SDL_SetRenderDrawColor(p->renderer,p->palette.accent.r,p->palette.accent.g,p->palette.accent.b,255);
            SDL_Rect underline={rect.x+3,rect.y+rect.h-4,rect.w-6,2};SDL_RenderFillRect(p->renderer,&underline);
        }
    }
    bool chooser=action==PARTS_SELECT || action==PARTS_TYPE || action==PARTS_PARENT || action==PARTS_PROPERTY_KIND || action==PARTS_LINK_SOURCE || action==PARTS_LINK_TARGET || action==PARTS_LINK_TYPE || action==PARTS_VOLUME_SELECT || action==PARTS_VOLUME_ROLE || action==PARTS_VOLUME_OWNER || action==PARTS_CHECK_SOURCE || action==PARTS_CHECK_TARGET || action==PARTS_CHECK_KIND;
    if (p->font) UIPanelSummary_DrawTextClipped(p->renderer,p->font,title,rect.x+7,rect.y+3,rect.w-(chooser?30:14),rect.h-4,text);
    if (chooser) {
        int cx=rect.x+rect.w-13,cy=rect.y+rect.h/2;
        SDL_SetRenderDrawColor(p->renderer,text.r,text.g,text.b,255);
        SDL_RenderDrawLine(p->renderer,cx-4,cy-2,cx,cy+2);
        SDL_RenderDrawLine(p->renderer,cx,cy+2,cx+4,cy-2);
    }
}
static inline void row(PartsPane* p,int action,const char* text,bool enabled) {cell(p,action,text,NULL,0,1,enabled,false);p->y+=p->h;}
static inline void field(PartsPane* p,int action,const char* label,const char* value) {cell(p,action,label,value,0,1,true,false);p->y+=p->h;}
static inline void note(PartsPane* p,const char* text) {
    int max_width = p->body.w - 38;
    int old_height = p->h;
    TTF_Font* font = p->font ? p->font : FontManager_Get(FONT_DEFAULT);
    if (font) p->h = TTF_FontHeight(font) + 5;
    size_t start = 0, length = strlen(text);
    while (start < length) {
        size_t n = length - start;
        if (n > 255) n = 255;
        char line[256];
        for (;;) {
            snprintf(line, sizeof(line), "%.*s", (int)n, text + start);
            int width = (int)n * 9;
            if (font) (void)TTF_SizeUTF8(font, line, &width, NULL);
            if (width <= max_width || n == 1) break;
            --n;
        }
        if (start + n < length) {
            size_t k = n;
            while (k && text[start + k] != ' ') --k;
            if (k) n = k;
        }
        snprintf(line, sizeof(line), "%.*s", (int)n, text + start);
        row(p, 0, line, true);
        start += n;
        while (text[start] == ' ') ++start;
    }
    p->h = old_height;
}
