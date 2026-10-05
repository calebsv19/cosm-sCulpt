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
    int gap=6,w=(p->body.w-24-(columns-1)*gap)/columns;
    SDL_Rect rect={p->body.x+6+column*(w+gap),p->y,w,p->h-5};
    if (value) {
        int label_width=w/3;
        if (p->renderer && p->font) UIPanelSummary_DrawTextClipped(p->renderer,p->font,title,rect.x+2,rect.y+5,label_width-4,rect.h-4,p->palette.text_primary);
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
    if (p->font) UIPanelSummary_DrawTextClipped(p->renderer,p->font,title,rect.x+7,rect.y+5,rect.w-(chooser?30:14),rect.h-6,text);
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
    int chars=(p->body.w-34)/(p->font ? TTF_FontHeight(p->font)/2+1 : 9);if (chars<12) chars=12;
    size_t start=0,length=strlen(text);
    while (start<length) {
        size_t n=length-start;if (n>(size_t)chars) n=(size_t)chars;
        if (start+n<length) {size_t k=n;while (k && text[start+k]!=' ') --k;if (k) n=k;}
        char line[256];snprintf(line,sizeof(line),"%.*s",(int)n,text+start);row(p,0,line,true);
        start+=n;while (text[start]==' ') ++start;
    }
}
