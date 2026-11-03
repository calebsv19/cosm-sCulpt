#include "UI/shared_text.h"
#include "UI/text_draw.h"
#include "UI/shared_theme_font_adapter.h"
#include "Core/global_state.h"
#include "kit_ui_native_text_sdl.h"
#include <math.h>
#include <string.h>

static uint64_t fingerprint(const char* text) {
    uint64_t h=UINT64_C(14695981039346656037);
    while (*text) { h^=(unsigned char)*text++; h*=UINT64_C(1099511628211); }
    return h;
}
void LDText_Bind(LDTextField* s, char* buffer, size_t capacity, size_t cursor) {
    if (s->edit.text!=buffer || s->edit.capacity!=capacity || s->fingerprint!=fingerprint(buffer) ||
        s->expected_cursor!=cursor) {
        memset(s,0,sizeof(*s));
        if (kit_ui_text_bind(&s->edit,buffer,capacity,KIT_UI_TEXT_SINGLE_LINE).code!=CORE_OK) return;
        if (kit_ui_text_position(&s->edit,cursor,cursor).code!=CORE_OK)
            (void)kit_ui_text_position(&s->edit,strlen(buffer),strlen(buffer));
    }
    s->fingerprint=fingerprint(buffer);s->expected_cursor=s->edit.cursor;
}
void LDText_CancelCapture(LDTextField* s) {
    s->dragging=false;s->ready=false;kit_ui_text_cancel_composition(&s->edit);
}
static CoreResult measure(void* user,const char* text,float* width) {
    LDTextField* s=user;int w=0;
    if (!s->font) {
        size_t scalars=0;for(const unsigned char* p=(const unsigned char*)text;*p;++p)if((*p&0xc0)!=0x80)++scalars;
        *width=(float)(scalars*8);return core_result_ok();
    }
    if (!line_drawing_text_measure_utf8(s->renderer,s->font,text,&w,NULL) &&
        line_drawing_text_size_utf8(s->font,text,&w,NULL)!=0)
        return (CoreResult){CORE_ERR_IO,"text measurement failed"};
    *width=(float)w;return core_result_ok();
}
KitUiTextEventResult LDText_Event(LDTextField* s,const SDL_Event* e) {
    KitUiTextEventResult result={.status={CORE_OK,NULL}};
    if (s->ready && e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT &&
        SDL_PointInRect(&(SDL_Point){e->button.x,e->button.y},&s->bounds)) {
        size_t at=0;
        if (kit_ui_text_presentation_hit(&s->view,e->button.x,e->button.y,measure,s,&at).code==CORE_OK) {
            kit_ui_text_cancel_composition(&s->edit);
            (void)kit_ui_text_position(&s->edit,at,(SDL_GetModState()&KMOD_SHIFT)?s->edit.anchor:at);
            s->dragging=true;
        }
        result.consumed=1;
    } else if (s->dragging && (e->type==SDL_MOUSEMOTION || e->type==SDL_MOUSEBUTTONUP)) {
        float x=e->type==SDL_MOUSEMOTION?e->motion.x:e->button.x;
        float y=e->type==SDL_MOUSEMOTION?e->motion.y:e->button.y;size_t at;
        if(s->ready && kit_ui_text_presentation_hit(&s->view,x,y,measure,s,&at).code==CORE_OK)
            (void)kit_ui_text_position(&s->edit,at,s->edit.anchor);
        if(e->type==SDL_MOUSEBUTTONUP)s->dragging=false;
        result.consumed=1;
    } else result=kit_ui_text_event_sdl(&s->edit,e);
    if(s->edit.text) {s->fingerprint=fingerprint(s->edit.text);s->expected_cursor=s->edit.cursor;}
    return result;
}
typedef struct Paint { LDTextField* field; SDL_Color ink; } Paint;
static CoreResult paint_text(void* user,KitRenderVec2 at,const char* text) {
    Paint* p=user;
    return line_drawing_text_draw_utf8_at(p->field->renderer,p->field->font,text,(int)at.x,(int)at.y,p->ink)
        ? core_result_ok() : (CoreResult){CORE_ERR_IO,"text drawing failed"};
}
static CoreResult paint_rect(void* user,KitRenderRect bounds,KitRenderColor c) {
    Paint* p=user;SDL_Rect rect={(int)bounds.x,(int)bounds.y,(int)ceilf(bounds.width),(int)ceilf(bounds.height)};
    SDL_SetRenderDrawColor(p->field->renderer,c.r,c.g,c.b,c.a);SDL_RenderFillRect(p->field->renderer,&rect);
    return core_result_ok();
}
static void paint(LDTextField* s,SDL_Color ink) {
    SDL_Rect old,clip=s->bounds;SDL_bool clipped=SDL_RenderIsClipEnabled(s->renderer);
    SDL_RenderGetClipRect(s->renderer,&old);
    if(clipped && !SDL_IntersectRect(&old,&clip,&clip))return;
#if USE_VULKAN
    float old_color[4];memcpy(old_color,s->renderer->draw_state.current_color,sizeof(old_color));
#else
    Uint8 red,green,blue,alpha;SDL_GetRenderDrawColor(s->renderer,&red,&green,&blue,&alpha);
    SDL_BlendMode old_blend;SDL_GetRenderDrawBlendMode(s->renderer,&old_blend);
    SDL_SetRenderDrawBlendMode(s->renderer,SDL_BLENDMODE_BLEND);
#endif
    SDL_RenderSetClipRect(s->renderer,&clip);
    LineDrawing3dThemePalette palette={0};(void)line_drawing3d_shared_theme_resolve_palette(&palette);
    KitUiTextPresentationColors colors={{palette.text_primary.r,palette.text_primary.g,palette.text_primary.b,75},
        {ink.r,ink.g,ink.b,ink.a},{ink.r,ink.g,ink.b,ink.a}};
    Paint context={s,ink};KitUiTextPainter painter={&context,paint_rect,paint_text};
    /* The external-text adapter rasterizes each run synchronously; no queued
       text pointer borrows this presentation after the call. */
    (void)kit_ui_text_presentation_paint(&s->view,&colors,&painter);
    SDL_RenderSetClipRect(s->renderer,clipped?&old:NULL);
#if USE_VULKAN
    vk_renderer_set_draw_color(s->renderer,old_color[0],old_color[1],old_color[2],old_color[3]);
#else
    SDL_SetRenderDrawBlendMode(s->renderer,old_blend);
    SDL_SetRenderDrawColor(s->renderer,red,green,blue,alpha);
#endif
}
/* Keep a bounded preedit visible beside its caret without changing edit bytes or hit-test space. */
static void reveal_preedit(KitUiTextPresentation* view) {
    if (!view->composition_bytes || view->count != 1) return;
    KitUiTextPresentationRow* row = &view->rows[0];
    float right = view->options.viewport.x + view->options.viewport.width;
    float shift = fmaxf(0, row->preedit.x + row->preedit.width - right);
    shift = fminf(shift, fmaxf(0, view->caret.x - view->options.viewport.x));
    row->origin.x -= shift;
    row->selection.x -= shift;
    row->preedit.x -= shift;
    view->caret.x -= shift;
    view->scroll_x += shift;
}

void LDText_Draw(LDTextField* s,SDL_Renderer* renderer,TTF_Font* font,SDL_Rect area,SDL_Color ink,bool active) {
    if(!s->edit.text || !renderer || !font || area.w<=0 || area.h<=0)return;
    s->renderer=renderer;s->font=font;s->bounds=area;
    int height=line_drawing_text_font_height(font);(void)line_drawing_text_measure_utf8(renderer,font,"Mg",NULL,&height);
    KitUiTextPresentationOptions options={{area.x,area.y,area.w,area.h},height,1,0,0,active,active};
    s->ready=kit_ui_text_presentation_build(&s->view,&s->edit,&options,measure,s).code==CORE_OK;
    if(s->ready) {
        reveal_preedit(&s->view);
        paint(s,ink);
        if(active && SDL_IsTextInputActive())
            (void)kit_ui_native_text_rect_sdl(SDL_GetKeyboardFocus(),&s->view,
                (float)Global_GetScreenWidth(),(float)Global_GetScreenHeight(),NULL);
    }
}
/* Preserve the host's word-boundary wrapping; the shared presentation still
   measures/scalar-wraps long words and owns clipping and row storage. */
static bool wrap_words(LDTextField* s,const char* text,int width,char* output,size_t capacity) {
    size_t used=0,line=0;bool has_word=false;
    while(*text) {
        while(*text==' ' || *text=='\t')++text;
        if(!*text)break;
        if(*text=='\n' || *text=='\r') {
            if(used+1>=capacity)return false;
            output[used++]='\n';if(*text++=='\r' && *text=='\n')++text;
            line=used;has_word=false;continue;
        }
        const char* word=text;while(*text && *text!=' ' && *text!='\t' && *text!='\n' && *text!='\r')++text;
        size_t length=(size_t)(text-word),separator=has_word?1:0;
        if(used+separator+length>=capacity)return false;
        size_t before=used;if(separator)output[used++]=' ';
        memcpy(output+used,word,length);used+=length;output[used]=0;float measured=0;
        if(measure(s,output+line,&measured).code!=CORE_OK)return false;
        if(has_word && measured>width) {output[before]='\n';line=before+1;}
        has_word=true;
    }
    output[used]=0;return true;
}
static bool label_build(LDTextField* s,TTF_Font* font,const char* text,SDL_Rect area,bool wrap,int advance) {
    if(!text || strlen(text)>=KIT_UI_TEXT_PRESENTATION_BYTES || area.w<=0 || area.h<=0)return false;
    s->font=font;s->bounds=area;char wrapped[KIT_UI_TEXT_PRESENTATION_BYTES];
    if(wrap) {if(!wrap_words(s,text,area.w,wrapped,sizeof(wrapped)))return false;text=wrapped;}
    if(kit_ui_text_bind(&s->edit,(char*)text,strlen(text)+1,0).code!=CORE_OK)return false;
    int height=font?line_drawing_text_font_height(font):14;if(font)(void)line_drawing_text_measure_utf8(s->renderer,font,"Mg",NULL,&height);
    KitUiTextPresentationOptions options={{area.x,area.y,area.w,area.h},advance>0?advance:height,1,0,wrap,0,0};
    bool accepted=kit_ui_text_presentation_build(&s->view,&s->edit,&options,measure,s).code==CORE_OK;
    s->edit.text=NULL; /* Presentation owns copied rows; wrapped storage is local. */
    return accepted;
}
int LDText_Label(SDL_Renderer* renderer,TTF_Font* font,const char* text,SDL_Rect area,SDL_Color ink,bool wrap) {
    LDTextField s={.renderer=renderer};
    if(!renderer || !font || !label_build(&s,font,text,area,wrap,0))return 0;
    paint(&s,ink);return (int)s.view.content_height;
}
int LDText_CountLines(TTF_Font* font,const char* text,int width) {
    LDTextField s={0};
    return label_build(&s,font,text,(SDL_Rect){0,0,width,32767},true,0)?(int)s.view.count:0;
}
int LDText_LabelRows(SDL_Renderer* renderer,TTF_Font* font,const char* text,SDL_Rect area,SDL_Color ink,int advance) {
    LDTextField s={.renderer=renderer};
    if(!renderer || !font || advance<=0 || !label_build(&s,font,text,area,true,advance))return 0;
    paint(&s,ink);return (int)s.view.content_height;
}
