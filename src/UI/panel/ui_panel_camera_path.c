#include "UI/ui_panel_camera_path.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/font_manager.h"
#include "Core/global_state.h"
#include "Core/camera_path.h"
#include "Layout/scene/layout_camera_poses.h"
#include "Layout/scene/layout_scene_path_edit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool shown,actions,editing,replace,dragging;
static char text[64];
static int row_height(void) {
    TTF_Font *font=FontManager_Get(FONT_DEFAULT);
    return (font ? TTF_FontHeight(font) : 16)+8;
}
static int columns(void) {
    TTF_Font *font=FontManager_Get(FONT_DEFAULT);
    int width=60,height=0;
    if(font)(void)TTF_SizeUTF8(font,"Add after",&width,&height);
    return UIPanel_Get()->rightBodyRect.w>=2*(width+8)+16 ? 2 : 1;
}
static bool expanded(void) {
    const GlobalState *s=Global_Get();
    return s->cameraView.active || shown ||
        (s->layout.sceneAuthoring.selected_kind==LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH &&
         s->layout.sceneAuthoring.selected_index<s->layout.sceneAuthoring.path_count &&
         s->layout.sceneAuthoring.paths[s->layout.sceneAuthoring.selected_index].role==LINE_DRAWING_SCENE_PATH_ROLE_CAMERA);
}
static int rows(void) {
    if(!expanded())return 0;
    const GlobalState *s=Global_Get(); const LineDrawingScenePath *p=CameraPath_Selected(s);
    if(!p) return s->cameraView.active ? 1 : 0;
    size_t n=Layout_ScenePathEdit_AnchorCount(p);
    int pair=columns()==1 ? 2 : 1;
    return 1+(int)(n<4 ? n : 4)+pair*3+1+pair+1+1+(actions ? pair*3 : 0);
}
int UIPanel_CameraPathHeight(void) { return rows()*row_height()+ (rows() ? 8 : 0); }
static size_t first_point(const GlobalState *s) {
    size_t a=CameraPath_SelectedAnchor(s);
    return a>2 ? a-2 : 0;
}
static SDL_Rect full_row(int row,int y) {
    SDL_Rect b=UIPanel_Get()->viewPane.summaryRect;
    return (SDL_Rect){b.x+4,y+4+row*row_height(),b.w-12,row_height()-3};
}
bool UIPanel_CameraPathRect(int action,SDL_Rect *out,int y) {
    const GlobalState *s=Global_Get(); const LineDrawingScenePath *p=CameraPath_Selected(s);
    if(!out) return false;
    *out=(SDL_Rect){0};
    if(action==UI_PATH_SHOW && !s->cameraView.active && p) {
        *out=full_row(2,UIPanel_Get()->viewPane.summaryRect.y);return true;
    }
    if(!expanded())return false;
    if(!p) { if(action==UI_PATH_NEW && s->cameraView.active) *out=full_row(0,y); return out->w>0; }
    size_t n=Layout_ScenePathEdit_AnchorCount(p),first=first_point(s);
    int points=(int)(n<4 ? n : 4),pair=columns()==1 ? 2 : 1;
    int row=1+points,col=0,count=columns();
    if(action>=UI_PATH_POINT_BASE && action<UI_PATH_POINT_BASE+16) {
        size_t a=(size_t)(action-UI_PATH_POINT_BASE);
        if(a<first || a>=first+(size_t)points || a>=n)return false;
        row=1+(int)(a-first);count=1;
    } else switch(action) {
    case UI_PATH_PREVIOUS: break;
    case UI_PATH_NEXT: col=1; break;
    case UI_PATH_VIEW: row+=pair; break;
    case UI_PATH_REVERT: row+=pair;col=1;break;
    case UI_PATH_APPLY: row+=pair*2;break;
    case UI_PATH_ADD: row+=pair*2;col=1;break;
    case UI_PATH_NEW: row+=pair*3;count=1;break;
    case UI_PATH_PLAY: row+=pair*3+1;break;
    case UI_PATH_LOOP: row+=pair*3+1;col=1;break;
    case UI_PATH_SCRUB: row+=pair*4+1;count=1;break;
    case UI_PATH_ACTIONS: row+=pair*4+2;count=1;break;
    default:
        if(!actions)return false;
        row+=pair*4+3;
        switch(action) {
        case UI_PATH_SECONDS:break;
        case UI_PATH_CLOSED:col=1;break;
        case UI_PATH_EARLIER:row+=pair;break;
        case UI_PATH_LATER:row+=pair;col=1;break;
        case UI_PATH_REMOVE:row+=pair*2;count=1;break;
        default:return false;
        }
    }
    if(count==1 && col) { row+=col;col=0; }
    SDL_Rect r=full_row(row,y);
    if(count==2) { r.w=(r.w-4)/2;r.x+=col*(r.w+4); }
    *out=r; return r.w>0;
}
static bool contains(SDL_Rect r,int x,int y) { return r.w>0 && r.h>0 && x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h; }
static void paint(SDL_Renderer *renderer,SDL_Rect r,const char *label,bool selected,bool enabled) {
    UIPanelVisualPalette p={0}; (void)UIPanelVisual_ResolvePalette(&p);
    UIPanelVisual_DrawFrame(renderer,r,selected ? p.button_fill_active : p.button_fill,selected ? p.accent : p.button_border,0);
    TTF_Font *font=FontManager_Get(FONT_DEFAULT);
    if(font) UIPanelSummary_DrawTextClipped(renderer,font,label,r.x+4,r.y+3,r.w-8,r.h-4,enabled ? p.text_primary : p.text_muted);
}
void UIPanel_CameraPathRender(SDL_Renderer *renderer,int y) {
    GlobalState *s=Global_Get(); const LineDrawingScenePath *p=CameraPath_Selected(s);
    SDL_Rect r; char label[160];
    if(UIPanel_CameraPathRect(UI_PATH_SHOW,&r,y))paint(renderer,r,expanded() ? "Path poses: open" : "Path poses",shown,true);
    if(!expanded())return;
    if(!p) { if(UIPanel_CameraPathRect(UI_PATH_NEW,&r,y)) paint(renderer,r,"New path from view",false,true);return; }
    size_t a=CameraPath_SelectedAnchor(s),n=Layout_ScenePathEdit_AnchorCount(p);
    snprintf(label,sizeof(label),"Path: %s",p->label); paint(renderer,full_row(0,y),label,false,true);
    for(size_t i=0;i<n;++i) if(UIPanel_CameraPathRect(UI_PATH_POINT_BASE+(int)i,&r,y)) {
        Vec3 point=p->control_points[CameraPoses_Control(p,i)];
        double meters=s->layout.metersPerWorldUnit;
        snprintf(label,sizeof(label),"Point %zu  (%.2g, %.2g, %.2g m)",i+1,point.x*meters,point.y*meters,point.z*meters);
        paint(renderer,r,label,i==a,true);
    }
    const char *names[]={"View","Revert","Apply","Add after","New path from view",
        "< Point", "Point >", s->cameraView.path_playing ? "Pause" : "Play",
        p->playback_mode==LINE_DRAWING_SCENE_PATH_PLAYBACK_LOOP ? "Loop" : "Once","",
        actions ? "Point actions: open" : "Point actions","", "Earlier", "Later", "Remove point",
        p->closed ? "Closed" : "Open"};
    for(int action=UI_PATH_VIEW;action<=UI_PATH_CLOSED;++action) {
        if(!UIPanel_CameraPathRect(action,&r,y)) continue;
        bool on=s->cameraView.active;
        bool enabled=(action==UI_PATH_APPLY || action==UI_PATH_ADD || action==UI_PATH_NEW) ? on :
            action==UI_PATH_PLAY ? n>1 : action==UI_PATH_REMOVE ? n>1 :
            action==UI_PATH_EARLIER || action==UI_PATH_PREVIOUS ? a>0 :
            action==UI_PATH_LATER || action==UI_PATH_NEXT ? a+1<n : true;
        const char *name=names[action-UI_PATH_VIEW];
        if(action==UI_PATH_SECONDS) {
            if(editing)snprintf(label,sizeof(label),"Leg seconds: %s",text);
            if(!editing)snprintf(label,sizeof(label),"Leg seconds: %.3g",p->key_count ? p->keys[a].seconds : 2.f);
            name=label;
        }
        if(action==UI_PATH_SCRUB) {
            float duration=CameraPoses_Duration(p),time=!strcmp(p->path_id,s->cameraView.path_source) ? s->cameraView.path_time : 0;
            snprintf(label,sizeof(label),"Scrub %.2f / %.2f s",time,duration);name=label;
            paint(renderer,r,name,false,true);
            float t=duration>0 ? time/duration : 0;
            SDL_SetRenderDrawColor(renderer,95,175,230,255);
            SDL_RenderDrawLine(renderer,r.x,r.y+r.h-2,r.x+(int)(r.w*t),r.y+r.h-2);continue;
        }
        paint(renderer,r,name,action==UI_PATH_ACTIONS && actions,enabled);
    }
}
static void refresh(void) { GlobalState *s=Global_Get();Global_FlagHitboxesDirty();UIPanel_OnWindowResized(s->screenWidth,s->screenHeight); }
void UIPanel_CameraPathReset(void) { editing=false;dragging=false;SDL_StopTextInput(); }
bool UIPanel_CameraPathCapturing(void) { return editing; }
static bool scrub(int x,int y) {
    SDL_Rect r; const LineDrawingScenePath *p=CameraPath_Selected(Global_Get());
    if(!p || !UIPanel_CameraPathRect(UI_PATH_SCRUB,&r,y))return false;
    float t=fmaxf(0,fminf(1,(float)(x-r.x)/r.w));
    return CameraPath_Scrub(Global_Get(),t*CameraPoses_Duration(p));
}
bool UIPanel_CameraPathClick(int x,int y,int base) {
    if(!contains(UIPanel_Get()->rightBodyRect,x,y))return false;
    GlobalState *s=Global_Get(); int action=0; SDL_Rect r;
    for(int i=UI_PATH_SHOW;i<=UI_PATH_CLOSED;++i) if(UIPanel_CameraPathRect(i,&r,base) && contains(r,x,y)) {action=i;break;}
    for(int i=0;i<16 && !action;++i) if(UIPanel_CameraPathRect(UI_PATH_POINT_BASE+i,&r,base) && contains(r,x,y))action=UI_PATH_POINT_BASE+i;
    if(!action)return false;
    UIPanel_CameraPathReset(); CameraView_ResetInput(s);
    size_t a=CameraPath_SelectedAnchor(s);
    if(action>=UI_PATH_POINT_BASE) (void)CameraPath_SelectPoint(s,(size_t)(action-UI_PATH_POINT_BASE));
    else switch(action) {
    case UI_PATH_SHOW:shown=!shown;break;
    case UI_PATH_PREVIOUS: if(a)(void)CameraPath_SelectPoint(s,a-1);break;
    case UI_PATH_NEXT: (void)CameraPath_SelectPoint(s,a+1);break;
    case UI_PATH_VIEW: (void)CameraPath_ViewPoint(s,a);break;
    case UI_PATH_REVERT: s->cameraView.path_modified=false; (void)CameraPath_ViewPoint(s,a);break;
    case UI_PATH_APPLY:(void)CameraPath_Apply(s);break;
    case UI_PATH_ADD:(void)CameraPath_Add(s,false);break;
    case UI_PATH_NEW:(void)CameraPath_Add(s,true);break;
    case UI_PATH_PLAY:(void)CameraPath_TogglePlay(s);break;
    case UI_PATH_SCRUB:(void)scrub(x,base);dragging=true;break;
    case UI_PATH_ACTIONS:actions=!actions;break;
    case UI_PATH_EARLIER:(void)CameraPath_Move(s,-1);break;
    case UI_PATH_LATER:(void)CameraPath_Move(s,1);break;
    case UI_PATH_REMOVE:(void)CameraPath_Remove(s);break;
    case UI_PATH_SECONDS: {
        const LineDrawingScenePath *p=CameraPath_Selected(s);
        editing=true;replace=true;snprintf(text,sizeof(text),"%.5g",p->key_count ? p->keys[a].seconds : 2.f);SDL_StartTextInput();break;
    }
    case UI_PATH_LOOP: case UI_PATH_CLOSED: {
        LineDrawingScenePath *p=(LineDrawingScenePath *)CameraPath_Selected(s);
        if(p && !s->cameraView.path_modified && Editor_TryHistoryCapture(&s->editor,&s->layout)) {
            if(action==UI_PATH_LOOP)p->playback_mode=p->playback_mode==LINE_DRAWING_SCENE_PATH_PLAYBACK_ONCE ? LINE_DRAWING_SCENE_PATH_PLAYBACK_LOOP : LINE_DRAWING_SCENE_PATH_PLAYBACK_ONCE;
            else p->closed=!p->closed;
            CameraPath_Stop(s); Global_FlagLayoutChanged(); s->cameraView.path_signature=0;
        }
        break;
    }
    default:break;
    }
    refresh();return true;
}
bool UIPanel_CameraPathEvent(const SDL_Event *e,int y) {
    if(dragging && e->type==SDL_MOUSEMOTION) { (void)scrub(e->motion.x,y);refresh();return true; }
    if(dragging && e->type==SDL_MOUSEBUTTONUP) { dragging=false;return true; }
    if(e->type==SDL_WINDOWEVENT && e->window.event==SDL_WINDOWEVENT_FOCUS_LOST) UIPanel_CameraPathReset();
    if(!editing)return false;
    if(e->type==SDL_TEXTINPUT) {
        if(replace) {text[0]=0;replace=false;}
        size_t n=strlen(text);snprintf(text+n,sizeof(text)-n,"%s",e->text.text);return true;
    }
    if(e->type==SDL_KEYDOWN) {
        if(e->key.keysym.sym==SDLK_ESCAPE)UIPanel_CameraPathReset();
        else if(e->key.keysym.sym==SDLK_RETURN || e->key.keysym.sym==SDLK_KP_ENTER) {
            char *end;float n=strtof(text,&end);
            if(end!=text && !*end && CameraPath_SetSeconds(Global_Get(),n))UIPanel_CameraPathReset();
        } else if(e->key.keysym.sym==SDLK_a && e->key.keysym.mod&(KMOD_CTRL|KMOD_GUI))replace=true;
        else if(e->key.keysym.sym==SDLK_BACKSPACE) {size_t n=strlen(text);if(replace)text[0]=0;else if(n)text[n-1]=0;replace=false;}
        refresh();return true;
    }
    return false;
}
