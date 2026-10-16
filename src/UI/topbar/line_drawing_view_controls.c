#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/topbar/visibility_tree.h"
#include "UI/panel/ui_panel_visibility.h"
#include "Core/global_state.h"
#include "Core/camera_view.h"
#include "Input/input_editor_actions.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_parts.h"
#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_routes.h"
#include "UI/font_manager.h"
#include "UI/text_draw.h"

static SDL_Rect headers[4];
static int popup;
int LDViewControls_ActivePopup(void) { return popup; }
static bool inside(int x,int y,SDL_Rect r){return r.w>0&&r.h>0&&SDL_PointInRect(&(SDL_Point){x,y},&r);}
static SDL_Rect panel(void){
    SDL_Rect anchor=headers[popup==LD_VIEW_APPEARANCE?3:2];int width=Global_GetScreenWidth();
    int w=width<520?width-20:500,x=anchor.x;
    if(x+w>width-10)x=width-w-10;if(x<10)x=10;
    return (SDL_Rect){x,anchor.y+anchor.h+4,w,popup==LD_VIEW_APPEARANCE?4*28+68:11*28+44};
}
size_t LDViewControls_MatchingCount(void){return UIPanel_VisibilityCount();}
void LDViewControls_SetHeader(SDL_Rect camera,SDL_Rect visibility,SDL_Rect more,SDL_Rect appearance){headers[0]=camera;headers[1]=visibility;headers[2]=more;headers[3]=appearance;}
void LDViewControls_Close(void){UIPanel_VisibilityClose();popup=0;SDL_StopTextInput();}
bool LDViewControls_Rect(int action,SDL_Rect* out){
    if(!out)return false;
    if(action>=LD_VIEW_CAMERA&&action<=LD_VIEW_APPEARANCE){*out=headers[action-1];return out->w>0;}
    if(!popup)return false;
    if(popup==LD_VIEW_VISIBILITY)return UIPanel_VisibilityRect(action,out);
    SDL_Rect p=panel();
    if(popup==LD_VIEW_APPEARANCE&&action>=LD_VIEW_WIRE&&action<=LD_VIEW_EDGES)*out=(SDL_Rect){p.x+8,p.y+34+(action-LD_VIEW_WIRE)*28,p.w-16,26};
    else if(action>=LD_VIEW_EXPLORE&&action<=LD_VIEW_MATERIAL)*out=(SDL_Rect){p.x+8,p.y+34+(action-LD_VIEW_EXPLORE)*28,p.w-16,26};
    else return false;return true;
}
static void stop_inputs(void){UIPanel_MeasurementStopInput();UIPanel_PartsStopInput();UIPanel_RoutesStopInput();CameraView_ResetInput(Global_Get());}
static void action(int command){
    GlobalState* s=Global_Get();
    if(command>=LD_VIEW_WIRE&&command<=LD_VIEW_EDGES){if(command==LD_VIEW_EDGES){s->surfaceEdges=!s->surfaceEdges;Global_FlagGridChanged();}else(void)Global_SetPreviewMode((LineDrawingPreviewMode)(command-LD_VIEW_WIRE+LINE_DRAWING_PREVIEW_MODE_WIREFRAME));LDViewControls_Close();return;}
    stop_inputs();
    if(command==LD_VIEW_CAMERA){if(s->cameraView.active)CameraView_Exit(s);else(void)CameraView_Enter(s,false);}
    else if(command==LD_VIEW_EXPLORE)(void)CameraView_Enter(s,false);
    else if(command==LD_VIEW_SAVED){if(s->layout.sceneAuthoring.camera_count)(void)CameraView_Enter(s,true);}
    else if(command==LD_VIEW_EDITOR)CameraView_Exit(s);
    else if(command==LD_VIEW_PLANE){CameraView_Exit(s);if(s->freeViewCamera.enabled)(void)InputEditorAction_ToggleFreeView();}
    else if(command==LD_VIEW_ORIENTATION){CameraView_Exit(s);(void)InputEditorAction_ToggleFreeView();}
    else if(command==LD_VIEW_SPACE){CameraView_Exit(s);(void)InputEditorAction_ToggleSpaceMode(true);}
    else if(command==LD_VIEW_BOUNDS)(void)UIPanel_ToggleSceneBoundsEnabled();
    else if(command==LD_VIEW_GIZMO)(void)InputEditorAction_ToggleObjectGizmoMode();
    Global_FlagHitboxesDirty();LDViewControls_Close();
}
bool LDViewControls_Click(int x,int y){
    for(int i=0;i<4;++i)if(inside(x,y,headers[i])){
        if(i==0){action(LD_VIEW_CAMERA);return true;}
        int next=i+1;if(popup==next)LDViewControls_Close();else{LDViewControls_Close();stop_inputs();popup=next;if(popup==LD_VIEW_VISIBILITY)UIPanel_VisibilityOpen();}return true;
    }
    if(!popup)return false;
    if(popup==LD_VIEW_VISIBILITY)return UIPanel_VisibilityClick(x,y);
    if(!inside(x,y,panel())){LDViewControls_Close();return true;}
    SDL_Rect r;int first=popup==LD_VIEW_APPEARANCE?LD_VIEW_WIRE:LD_VIEW_EXPLORE,last=popup==LD_VIEW_APPEARANCE?LD_VIEW_EDGES:LD_VIEW_MATERIAL;
    for(int i=first;i<=last;++i)if(LDViewControls_Rect(i,&r)&&inside(x,y,r)){action(i);break;}return true;
}
bool LDViewControls_Event(const SDL_Event* e){
    if(!e)return false;
    if(UIPanel_IsSaveDialogActive()||UIPanel_IsRootDialogActive()||UIPanel_IsScenePropertyDialogActive()||UIPanel_IsSceneBoundsDialogActive()||UIPanel_IsPrismDimensionDialogActive()){if(popup)LDViewControls_Close();return false;}
    if(e->type==SDL_WINDOWEVENT&&e->window.event==SDL_WINDOWEVENT_FOCUS_LOST){LDViewControls_Close();return false;}
    if(popup && e->type==SDL_MOUSEBUTTONDOWN&&e->button.button==SDL_BUTTON_LEFT)return LDViewControls_Click(e->button.x,e->button.y);
    if(!popup)return false;
    if(popup==LD_VIEW_VISIBILITY)return UIPanel_VisibilityEvent(e);
    if(e->type==SDL_KEYDOWN&&e->key.keysym.sym==SDLK_ESCAPE){LDViewControls_Close();return true;}
    return e->type==SDL_KEYDOWN||e->type==SDL_KEYUP||e->type==SDL_MOUSEMOTION||e->type==SDL_MOUSEBUTTONUP||e->type==SDL_MOUSEWHEEL;
}
static void button(SDL_Renderer* r,SDL_Rect box,const char* label,bool active){
    if(box.w<=0)return;SDL_SetRenderDrawColor(r,active?65:37,active?83:41,active?113:48,255);SDL_RenderFillRect(r,&box);
    SDL_SetRenderDrawColor(r,90,104,125,255);SDL_RenderDrawRect(r,&box);
    SDL_Rect old;SDL_bool clipped=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&old);SDL_Rect clip={box.x+4,box.y+1,box.w-8,box.h-2};SDL_RenderSetClipRect(r,&clip);
    (void)line_drawing_text_draw_utf8_at(r,FontManager_Get(FONT_DEFAULT),label,box.x+6,box.y+4,(SDL_Color){230,236,245,255});SDL_RenderSetClipRect(r,clipped?&old:NULL);
}
void LDViewControls_RenderHeader(SDL_Renderer* r){
    GlobalState* s=Global_Get();button(r,headers[0],s->cameraView.active?(s->cameraView.source_id[0]?"Saved Camera ON":"Free Camera ON"):"Free Camera",s->cameraView.active);
    button(r,headers[1],"Visibility",popup==LD_VIEW_VISIBILITY);button(r,headers[2],"More",popup==LD_VIEW_MORE);
    button(r,headers[3],s->spaceMode==SPACE_MODE_2D?"2D lines":s->previewMode==LINE_DRAWING_PREVIEW_MODE_WIREFRAME?"Wireframe":s->previewMode==LINE_DRAWING_PREVIEW_MODE_FLAT?"Solid":s->previewMode==LINE_DRAWING_PREVIEW_MODE_MATERIAL?"Material":"Bounds",popup==LD_VIEW_APPEARANCE);
}
void LDViewControls_RenderPopup(SDL_Renderer* r){
    if(!popup||!r)return;
    if(popup==LD_VIEW_VISIBILITY){UIPanel_VisibilityRender(r);return;}
    SDL_Rect p=panel();SDL_SetRenderDrawColor(r,25,29,36,255);SDL_RenderFillRect(r,&p);SDL_SetRenderDrawColor(r,122,146,181,255);SDL_RenderDrawRect(r,&p);
    const char* title=popup==LD_VIEW_APPEARANCE?"Appearance":"Camera, editor & appearance";
    (void)line_drawing_text_draw_utf8_at(r,FontManager_Get(FONT_DEFAULT),title,p.x+10,p.y+8,(SDL_Color){230,236,245,255});
    static const char* choices[]={"Wireframe","Solid (3D)","Material (3D)","Subdued surface edges"};
    static const char* labels[]={"Explore / Free Camera","Enter selected saved camera","Return to editor","Return to plane editor","Toggle editor orientation","Toggle 2D / 3D","Toggle scene bounds","Cycle Gizmo","Appearance: Wire","Appearance: Solid","Appearance: Material"};
    int first=popup==LD_VIEW_APPEARANCE?LD_VIEW_WIRE:LD_VIEW_EXPLORE,last=popup==LD_VIEW_APPEARANCE?LD_VIEW_EDGES:LD_VIEW_MATERIAL;
    for(int i=first;i<=last;++i){SDL_Rect box;LDViewControls_Rect(i,&box);button(r,box,popup==LD_VIEW_APPEARANCE?choices[i-first]:labels[i-first],popup==LD_VIEW_APPEARANCE&&(i==LD_VIEW_EDGES?Global_Get()->surfaceEdges:Global_Get()->previewMode==(LineDrawingPreviewMode)(i-LD_VIEW_WIRE+1)));}
}
