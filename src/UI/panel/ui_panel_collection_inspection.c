#include "UI/text_draw.h"
#include "UI/shared_special.h"
#include "UI/panel/ui_panel_collection_inspection.h"
#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_right_scroll.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/font_manager.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Core/camera_view.h"
#include "Layout/layout_collection_inspection.h"
#include "Layout/layout_scene_collections.h"
#include "Core/construction_reference_export.h"
#include "Layout/layout_saved_views.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool active,input,replace;
static int chooser;
static char scope[64],part[64],message[160];
static LayoutSavedMeasurement draft;
/* Frame-long backing storage for queued text. Layout/hit-test passes never overwrite it. */
static char labels[96][256];static size_t label_count;
static const char* kinds[]={"Area extent U","Area extent V","Area extent N","Point distance A to B","Signed projection U","Signed projection V","Signed projection N"};
static const char* features[]={"Origin","Envelope min corner","Envelope center","Envelope max corner"};
typedef struct {SDL_Renderer* renderer;SDL_Rect body,found;int y,wanted,x,hit,y_click;UIPanelVisualPalette palette;} Pane;
static double display(double meters) {double v=meters;(void)core_units_convert(meters,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&v);return v;}
static bool inside(SDL_Rect r,int x,int y) {return SDL_PointInRect(&(SDL_Point){x,y},&r);}
static const char* part_label(const Object3D* o){return o?(o->info.label[0]?o->info.label:o->coreMeta.object_id):"Missing part";}
static void row(Pane* p,int action,const char* label,bool enabled) {
    TTF_Font* font=FontManager_Get(FONT_DEFAULT);int lines=UIPanelSummary_CountWrappedLines(font,label,p->body.w-36);
    if(lines<1)lines=1;if(lines>4)lines=4;int height=lines>1?lines*(font?line_drawing_text_font_height(font)+2:20)+12:28;
    SDL_Rect box={p->body.x+7,p->y,p->body.w-26,height};p->y+=height+4;
    if(action==p->wanted)p->found=box;
    if(action&&enabled&&inside(box,p->x,p->y_click)&&inside(p->body,p->x,p->y_click))p->hit=action;
    LDSpecial_Record(23,action,label,box,&p->body,enabled);
    if(!p->renderer||!SDL_HasIntersection(&box,&p->body))return;
    if(action)UIPanelVisual_DrawFrame(p->renderer,box,action==INSPECT_SAVE?p->palette.button_fill_active:p->palette.button_fill,p->palette.button_border,0);
    if(label_count<96){snprintf(labels[label_count],256,"%s",label);UIPanelSummary_DrawWrappedText(p->renderer,font,labels[label_count++],box.x+5,box.y+5,box.w-10,font?line_drawing_text_font_height(font):20,2,lines,enabled?p->palette.text_primary:p->palette.text_muted);}
}
static LayoutSavedMeasurement preview(const Layout* l) {
    LayoutSavedMeasurement m=draft;snprintf(m.collection,64,"%s",scope);
    if(!m.id[0]){snprintf(m.id,64,"preview");const LayoutSceneCollection* c=Layout_FindSceneCollection(&l->objectStore,scope);snprintf(m.frame_entity,64,"%s",c?c->inspection_frame:"");}
    if(!m.name[0])snprintf(m.name,96,"%s",kinds[m.kind]);return m;
}
static Pane build(SDL_Renderer* renderer,int wanted,int x,int y) {
    UIPanelState* ui=UIPanel_Get();Layout* l=&Global_Get()->layout;LayoutObjectStore* s=&l->objectStore;
    Pane p={.renderer=renderer,.body=ui->rightBodyRect,.wanted=wanted,.x=x,.y_click=y};(void)UIPanelVisual_ResolvePalette(&p.palette);
    p.y=p.body.y+8-(int)UIPanel_RightScrollOffset(ui);if(renderer)label_count=0;
    const LayoutSceneCollection* c=Layout_FindSceneCollection(s,scope);char text[256];
    snprintf(text,256,"Area: %s",c?c->name:"Missing collection");row(&p,INSPECT_AREA,text,true);
    if(chooser==1)for(size_t i=0;i<s->scene_collection_count;++i)row(&p,INSPECT_AREA_BASE+(int)i,s->scene_collections[i].name,true);
    row(&p,INSPECT_FOCUS,"Only + frame area (keeps layers)",c!=NULL);
    row(&p,INSPECT_EXPORT,"Export construction reference sheets",c!=NULL);
    row(&p,INSPECT_OPEN_REFERENCE,"Open last exported sheets",ConstructionReference_LastDirectory()[0]!=0);
    snprintf(text,256,"Units: %s",UIPanel_GetDisplayUnitSymbol());row(&p,INSPECT_UNITS,text,true);
    snprintf(text,256,"Frame: %s",c&&c->inspection_frame[0]?c->inspection_frame:"World X / Y / Z");row(&p,0,text,true);
    row(&p,INSPECT_FRAME_PICK,"Use viewport part frame",c!=NULL);row(&p,INSPECT_FRAME_PARTS,"Use Parts selection frame",c!=NULL);row(&p,INSPECT_FRAME_WORLD,"Use world frame",c!=NULL);
    LayoutCollectionInspection bounds=Layout_InspectCollection(l,scope,c?c->inspection_frame:"");
    snprintf(text,256,"%zu physical parts / %zu STL envelopes",bounds.parts,bounds.mesh_parts);row(&p,0,text,true);
    row(&p,0,bounds.message,bounds.status==LAYOUT_MEASUREMENT_OK);
    if(bounds.status==LAYOUT_MEASUREMENT_OK)for(int k=0;k<3;++k) {
        snprintf(text,256,"%c: %.5g to %.5g (%g %s)","UVN"[k],display(bounds.min_meters[k]),display(bounds.max_meters[k]),display(bounds.max_meters[k]-bounds.min_meters[k]),UIPanel_GetDisplayUnitSymbol());row(&p,0,text,true);
    }
    if(bounds.status==LAYOUT_MEASUREMENT_OK){snprintf(text,256,"World AABB: %.5g / %.5g / %.5g %s",display(bounds.world_max_meters[0]-bounds.world_min_meters[0]),display(bounds.world_max_meters[1]-bounds.world_min_meters[1]),display(bounds.world_max_meters[2]-bounds.world_min_meters[2]),UIPanel_GetDisplayUnitSymbol());row(&p,0,text,true);}
    const Object3D* o=Layout_InspectionObject(l,part);snprintf(text,256,"Part: %s",part_label(o));row(&p,INSPECT_PART,text,c!=NULL);
    if(chooser==2)for(size_t i=0;i<s->count;++i)if(Layout_InspectionMember(l,scope,&s->items[i]))row(&p,INSPECT_PART_BASE+(int)i,part_label(&s->items[i]),true);
    double size[3];if(o&&Layout_InspectionLocalSize(l,o,size)) {
        snprintf(text,256,"Local size: %.5g / %.5g / %.5g %s",display(size[0]),display(size[1]),display(size[2]),UIPanel_GetDisplayUnitSymbol());row(&p,0,text,true);
        row(&p,0,o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE?"STL envelope, instance scaled; no face-fit claim":"Authored primitive U / V / N size",true);
        Vec3 lo,hi;if(Layout_Object3D_ComputeWorldAABB(o,&lo,&hi)){double scale=Layout_WorldScale(l);snprintf(text,256,"Part world AABB: %.5g / %.5g / %.5g",display((hi.x-lo.x)*scale),display((hi.y-lo.y)*scale),display((hi.z-lo.z)*scale));row(&p,0,text,true);}
        LayoutInspectionDatum origin={0};snprintf(origin.entity_id,64,"%s",part);double point[3];
        if(bounds.status==LAYOUT_MEASUREMENT_OK&&Layout_InspectionDatumPoint(l,&origin,point)==LAYOUT_MEASUREMENT_OK){Vec3 axes[]={bounds.frame.axisU,bounds.frame.axisV,bounds.frame.normal};double position[3];double scale=Layout_WorldScale(l);for(int k=0;k<3;++k)position[k]=(point[0]-bounds.frame.origin.x*scale)*axes[k].x+(point[1]-bounds.frame.origin.y*scale)*axes[k].y+(point[2]-bounds.frame.origin.z*scale)*axes[k].z;snprintf(text,256,"Origin in frame: %.5g / %.5g / %.5g",display(position[0]),display(position[1]),display(position[2]));row(&p,0,text,true);}
    }
    row(&p,INSPECT_SET_A,"Use part as datum A",o&&Layout_InspectionMember(l,scope,o));row(&p,INSPECT_SET_B,"Use part as datum B",o&&Layout_InspectionMember(l,scope,o));
    snprintf(text,256,"A: %s",part_label(Layout_InspectionObject(l,draft.a.entity_id)));row(&p,0,text,true);
    snprintf(text,256,"A feature: %s",features[draft.a.feature]);row(&p,INSPECT_FEATURE_A,text,true);
    snprintf(text,256,"B: %s",part_label(Layout_InspectionObject(l,draft.b.entity_id)));row(&p,0,text,true);
    snprintf(text,256,"B feature: %s",features[draft.b.feature]);row(&p,INSPECT_FEATURE_B,text,true);
    snprintf(text,256,"Measure: %s",kinds[draft.kind]);row(&p,INSPECT_KIND,text,true);
    LayoutSavedMeasurement m=preview(l);LayoutMeasurementResult result=Layout_EvaluateSavedMeasurement(l,&m);
    if(result.status==LAYOUT_MEASUREMENT_OK)snprintf(text,256,"Current: %.5g %s",display(result.value),UIPanel_GetDisplayUnitSymbol());
    if(result.status!=LAYOUT_MEASUREMENT_OK)snprintf(text,256,"Current: %s",result.message);row(&p,0,text,result.status==LAYOUT_MEASUREMENT_OK);
    snprintf(text,256,"Measurement frame: %s",m.frame_entity[0]?m.frame_entity:"World X / Y / Z");row(&p,0,text,true);
    snprintf(text,256,"Name: %s",draft.name);row(&p,INSPECT_NAME,text,true);
    row(&p,INSPECT_SAVE,draft.id[0]?"Update saved measurement":"Save measurement",result.status==LAYOUT_MEASUREMENT_OK);
    row(&p,INSPECT_NEW,"New measurement",true);row(&p,INSPECT_DELETE,"Delete selected measurement",draft.id[0]!=0);
    row(&p,INSPECT_SAVED,"Saved measurements in this area",true);
    if(chooser==3)for(size_t i=0;i<s->measurement_count;++i)if(!strcmp(s->measurements[i].collection,scope)){LayoutMeasurementResult v=Layout_EvaluateSavedMeasurement(l,&s->measurements[i]);snprintf(text,256,"%s: %s",s->measurements[i].name,Layout_MeasurementStatusLabel(v.status));row(&p,INSPECT_SAVED_BASE+(int)i,text,true);}
    row(&p,0,message[0]?message:"Save edits with File Save. Measurements follow current pose.",true);
    if(ConstructionReference_LastDirectory()[0])row(&p,0,ConstructionReference_LastDirectory(),true);
    row(&p,INSPECT_BACK,"Return to reference / motion tools",true);
    return p;
}
bool UIPanel_CollectionInspectionActive(void){return active;}
bool UIPanel_CollectionInspectionHasInput(void){return active&&input&&UIPanel_Get()->activeRightTab==UI_PANEL_RIGHT_TAB_MEASURE;}
void UIPanel_CollectionInspectionStopInput(void){if(input)SDL_StopTextInput();input=false;}
void UIPanel_CollectionInspectionClose(void){UIPanel_CollectionInspectionStopInput();active=false;chooser=0;}
static void select_area(const char* id) {
    snprintf(scope,64,"%s",id);part[0]=0;memset(&draft,0,sizeof(draft));chooser=0;message[0]=0;
    Layout* l=&Global_Get()->layout;for(size_t i=0;i<l->objectStore.count;++i)if(Layout_InspectionMember(l,id,&l->objectStore.items[i])){snprintf(part,64,"%s",l->objectStore.items[i].coreMeta.object_id);break;}
    UIPanel_Get()->rightScroll[UI_PANEL_RIGHT_TAB_MEASURE].scrollOffsetPx=0;
}
bool UIPanel_BeginCollectionInspection(const char* id) {
    if(!Layout_FindSceneCollection(&Global_Get()->layout.objectStore,id))return false;
    UIPanel_BeginMeasurement();active=true;select_area(id);LDViewControls_Close();UIPanel_CollectionInspectionLayout();return true;
}
void UIPanel_CollectionInspectionLayout(void){if(UIPanel_Get()->activeRightTab!=UI_PANEL_RIGHT_TAB_MEASURE)return;Pane p=build(NULL,0,-1,-1);UIPanel_RightScrollSetContentHeight(UIPanel_Get(),(float)(p.y-p.body.y)+UIPanel_RightScrollOffset(UIPanel_Get())+8);}
void UIPanel_CollectionInspectionRender(SDL_Renderer* renderer){if(!renderer||UIPanel_Get()->activeRightTab!=UI_PANEL_RIGHT_TAB_MEASURE)return;SDL_Rect old;SDL_bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&old);SDL_RenderSetClipRect(renderer,&UIPanel_Get()->rightBodyRect);(void)build(renderer,0,-1,-1);SDL_RenderSetClipRect(renderer,clipped?&old:NULL);}
bool UIPanel_CollectionInspectionRect(int action,SDL_Rect* out){Pane p=build(NULL,action,-1,-1);if(out)*out=p.found;return active&&p.found.w>0;}
static void frame_action(int mode) {
    GlobalState* g=Global_Get();const LayoutSceneCollection* old=Layout_FindSceneCollection(&g->layout.objectStore,scope);if(!old)return;LayoutSceneCollection c=*old;
    const Object3D* pick=Layout_ObjectStore_FindConst(&g->layout.objectStore,g->editor.selectedObject3DId);const char* id=mode==0?"":mode==1?(pick?pick->coreMeta.object_id:""):UIPanel_Get()->parts.id;
    if(mode&&!id[0]){snprintf(message,160,"Select a part or assembly first.");return;}
    PlaneFrame3 frame;if(Layout_InspectionFrame(&g->layout,id,&frame)!=LAYOUT_MEASUREMENT_OK){snprintf(message,160,"Choose a valid primitive, STL or assembly frame first.");return;}
    snprintf(c.inspection_frame,64,"%s",id);bool ok=Layout_EditSceneCollection(&g->layout,&c,NULL,Layout_GeometryHistory,NULL);
    snprintf(message,160,"%s",ok?"Area frame updated; Save to retain. Saved measurements keep their captured frames.":"Frame update refused.");
}
bool UIPanel_CollectionInspectionClick(int x,int y) {
    if(!active)return false;
    if(!inside(UIPanel_Get()->rightBodyRect,x,y)){UIPanel_CollectionInspectionStopInput();return false;}
    if(x>=UIPanel_Get()->rightBodyRect.x+UIPanel_Get()->rightBodyRect.w-12)return false;
    Pane p=build(NULL,0,x,y);int action=p.hit;if(!action)return true;
    UIPanel_CollectionInspectionStopInput();Layout* l=&Global_Get()->layout;LayoutObjectStore* s=&l->objectStore;
    if(action>=INSPECT_SAVED_BASE){size_t i=(size_t)(action-INSPECT_SAVED_BASE);if(i<s->measurement_count)draft=s->measurements[i];chooser=0;}
    else if(action>=INSPECT_PART_BASE){size_t i=(size_t)(action-INSPECT_PART_BASE);if(i<s->count)snprintf(part,64,"%s",s->items[i].coreMeta.object_id);chooser=0;}
    else if(action>=INSPECT_AREA_BASE){size_t i=(size_t)(action-INSPECT_AREA_BASE);if(i<s->scene_collection_count)select_area(s->scene_collections[i].id);}
    else if(action==INSPECT_AREA||action==INSPECT_PART||action==INSPECT_SAVED){int next=action==INSPECT_AREA?1:action==INSPECT_PART?2:3;chooser=chooser==next?0:next;}
    else if(action==INSPECT_FOCUS) {
        const LayoutSceneCollection* c=Layout_FindSceneCollection(s,scope);LayoutCollectionInspection r=Layout_InspectCollection(l,scope,c?c->inspection_frame:"");
        if(r.status==LAYOUT_MEASUREMENT_OK){double scale=Layout_WorldScale(l);Vec3 lo={(float)(r.world_min_meters[0]/scale),(float)(r.world_min_meters[1]/scale),(float)(r.world_min_meters[2]/scale)},hi={(float)(r.world_max_meters[0]/scale),(float)(r.world_max_meters[1]/scale),(float)(r.world_max_meters[2]/scale)};
            if(CameraView_Enter(Global_Get(),false)&&Layout_IsolateEntity(s,scope))CameraView_FrameBounds(Global_Get(),lo,hi);
        }else snprintf(message,160,"%s",r.message);
    }else if(action==INSPECT_EXPORT){(void)ConstructionReference_Export(scope,message,sizeof(message));}
    else if(action==INSPECT_OPEN_REFERENCE){if(!ConstructionReference_OpenLast())snprintf(message,sizeof(message),"Could not open sheets. Open reference_sheets.html from File output root.");}
    else if(action==INSPECT_UNITS){CoreUnitKind units[]={CORE_UNIT_METER,CORE_UNIT_CENTIMETER,CORE_UNIT_MILLIMETER,CORE_UNIT_INCH,CORE_UNIT_FOOT};size_t i=0;while(i<5&&units[i]!=UIPanel_GetDisplayUnit())++i;(void)UIPanel_SetDisplayUnit(units[(i+1)%5]);}
    else if(action==INSPECT_FRAME_PICK||action==INSPECT_FRAME_WORLD||action==INSPECT_FRAME_PARTS)frame_action(action==INSPECT_FRAME_WORLD?0:action==INSPECT_FRAME_PICK?1:2);
    else if(action==INSPECT_SET_A||action==INSPECT_SET_B)snprintf((action==INSPECT_SET_A?draft.a.entity_id:draft.b.entity_id),64,"%s",part);
    else if(action==INSPECT_FEATURE_A)draft.a.feature=(draft.a.feature+1)%4;
    else if(action==INSPECT_FEATURE_B)draft.b.feature=(draft.b.feature+1)%4;
    else if(action==INSPECT_KIND)draft.kind=(draft.kind+1)%7;
    else if(action==INSPECT_NAME){input=true;replace=true;SDL_StartTextInput();}
    else if(action==INSPECT_NEW){memset(&draft,0,sizeof(draft));message[0]=0;}
    else if(action==INSPECT_SAVE) {
        LayoutSavedMeasurement m=preview(l);
        if(!draft.id[0]) {unsigned n=1;bool used;do{used=false;snprintf(m.id,64,"measurement_%u",n++);for(size_t i=0;i<s->measurement_count;++i)if(!strcmp(m.id,s->measurements[i].id))used=true;}while(used);}
        bool ok=Layout_SaveMeasurement(l,&m,NULL,Layout_GeometryHistory,NULL);if(ok)draft=m;
        snprintf(message,160,"%s",ok?"Measurement saved; File Save retains it. Undo removes this edit.":"Measurement could not be saved.");
    }else if(action==INSPECT_DELETE){bool ok=Layout_SaveMeasurement(l,NULL,draft.id,Layout_GeometryHistory,NULL);if(ok)memset(&draft,0,sizeof(draft));snprintf(message,160,"%s",ok?"Measurement removed; Undo restores it.":"Measurement no longer exists.");}
    else if(action==INSPECT_BACK){UIPanel_CollectionInspectionClose();UIPanel_BeginMeasurement();}
    UIPanel_CollectionInspectionLayout();return true;
}
bool UIPanel_CollectionInspectionText(const char* value) {
    if(!input||!value)return false;if(replace){draft.name[0]=0;replace=false;}size_t n=strlen(draft.name),more=strlen(value);if(n+more<96)memcpy(draft.name+n,value,more+1);return true;
}
bool UIPanel_CollectionInspectionKey(SDL_Keycode key) {
    if(!input)return false;
    if(key==SDLK_ESCAPE||key==SDLK_RETURN||key==SDLK_KP_ENTER)UIPanel_CollectionInspectionStopInput();
    else if(key==SDLK_BACKSPACE){if(replace){draft.name[0]=0;replace=false;}size_t n=strlen(draft.name);if(n){do{--n;}while(n&&((unsigned char)draft.name[n]&0xc0)==0x80);draft.name[n]=0;}}
    return true;
}
bool UIPanel_CollectionInspectionInputEvent(const SDL_Event* e) {
    if(!e||!UIPanel_CollectionInspectionHasInput())return false;
    if(e->type==SDL_TEXTINPUT)return UIPanel_CollectionInspectionText(e->text.text);
    if(e->type==SDL_KEYDOWN){if(e->key.keysym.sym==SDLK_a&&(e->key.keysym.mod&(KMOD_CTRL|KMOD_GUI))){replace=true;return true;}return UIPanel_CollectionInspectionKey(e->key.keysym.sym);}
    return e->type==SDL_KEYUP;
}

void UIPanel_CollectionInspectionViewport(SDL_Renderer* renderer) {
    if(!active||!renderer||UIPanel_Get()->activeRightTab!=UI_PANEL_RIGHT_TAB_MEASURE)return;
    GlobalState* g=Global_Get();LayoutSavedMeasurement m=preview(&g->layout);
    LayoutMeasurementResult value=Layout_EvaluateSavedMeasurement(&g->layout,&m);
    if(value.status!=LAYOUT_MEASUREMENT_OK)return;
    double p[2][3];
    if(m.kind<3) {
        LayoutCollectionInspection bounds=Layout_InspectCollection(&g->layout,scope,m.frame_entity);
        if(bounds.status!=LAYOUT_MEASUREMENT_OK)return;
        Vec3 axes[]={bounds.frame.axisU,bounds.frame.axisV,bounds.frame.normal};double scale=Layout_WorldScale(&g->layout);
        for(int j=0;j<2;++j) {
            p[j][0]=bounds.frame.origin.x*scale;p[j][1]=bounds.frame.origin.y*scale;p[j][2]=bounds.frame.origin.z*scale;
            for(int k=0;k<3;++k){double v=j&&k==m.kind?bounds.max_meters[k]:bounds.min_meters[k];p[j][0]+=v*axes[k].x;p[j][1]+=v*axes[k].y;p[j][2]+=v*axes[k].z;}
        }
    }else if(Layout_InspectionDatumPoint(&g->layout,&m.a,p[0])!=LAYOUT_MEASUREMENT_OK||Layout_InspectionDatumPoint(&g->layout,&m.b,p[1])!=LAYOUT_MEASUREMENT_OK)return;
    SpaceViewContext view=SpaceAdapter_BuildViewContext(g);CorePaneRect vp;
    if(!LineDrawingPaneHost_GetViewportRect(&g->paneHost,&vp))return;
    SDL_Rect clip={(int)vp.x,(int)vp.y,(int)vp.width,(int)vp.height},old;SDL_bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&old);SDL_RenderSetClipRect(renderer,&clip);
    Vec2 pixels[2];bool visible[2];double scale=Layout_WorldScale(&g->layout);
    for(int j=0;j<2;++j) {
        Vec3 point={(float)(p[j][0]/scale),(float)(p[j][1]/scale),(float)(p[j][2]/scale)};
        pixels[j]=WorldToScreen(SpaceAdapter_ProjectToView(point,&view),&g->grid);
        visible[j]=isfinite(pixels[j].x)&&isfinite(pixels[j].y)&&pixels[j].x>=clip.x&&pixels[j].x<clip.x+clip.w&&pixels[j].y>=clip.y&&pixels[j].y<clip.y+clip.h;
        if(visible[j]){SDL_Color color=j?(SDL_Color){255,190,90,255}:(SDL_Color){100,210,255,255};SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,255);SDL_Rect marker={(int)pixels[j].x-5,(int)pixels[j].y-5,11,11};SDL_RenderDrawRect(renderer,&marker);UIPanelSummary_DrawTextClipped(renderer,FontManager_Get(FONT_DEFAULT),j?"B":"A",marker.x+13,marker.y,24,24,color);}
    }
    if(visible[0]&&visible[1]){SDL_SetRenderDrawColor(renderer,150,215,235,255);SDL_RenderDrawLine(renderer,(int)pixels[0].x,(int)pixels[0].y,(int)pixels[1].x,(int)pixels[1].y);}
    static char banner[256];snprintf(banner,256,"%s: %.5g %s | envelope reference, current pose",m.name,display(value.value),UIPanel_GetDisplayUnitSymbol());
    SDL_Rect box={clip.x+8,clip.y+8,clip.w-16,30};SDL_SetRenderDrawColor(renderer,25,30,38,240);SDL_RenderFillRect(renderer,&box);UIPanelSummary_DrawTextClipped(renderer,FontManager_Get(FONT_DEFAULT),banner,box.x+6,box.y+5,box.w-12,24,(SDL_Color){220,235,245,255});SDL_RenderSetClipRect(renderer,clipped?&old:NULL);
}

void LDSpecial_Inspection(void) {if(active && UIPanel_Get()->activeRightTab==UI_PANEL_RIGHT_TAB_MEASURE)(void)build(NULL,0,-1,-1);}
bool LDInline_Inspection(LDInlineField* f) {
    if(!UIPanel_CollectionInspectionHasInput())return false;
    SDL_Rect box;if(!UIPanel_CollectionInspectionRect(INSPECT_NAME,&box))return false;
    *f=(LDInlineField){draft.name,sizeof(draft.name),&replace,box,NULL};return true;
}
