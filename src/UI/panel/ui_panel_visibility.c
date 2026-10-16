#include "UI/shared_special.h"
#include "UI/panel/ui_panel_visibility.h"
#include "UI/panel/ui_panel_collection_inspection.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/topbar/visibility_tree.h"
#include "Core/global_state.h"
#include "Layout/layout_scene_collections.h"
#include "Layout/layout_saved_views.h"
#include "Layout/layout_coverings.h"
#include "Layout/layout_guide_groups.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_parts.h"
#include "UI/font_manager.h"
#include "UI/text_draw.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

enum { TAB_SCENE=40,TAB_LAYERS,TAB_VIEWS, EDIT=44, NAME, NEW, RENAME, ADD, REMOVE, DELETE_GROUP, SAVE_VIEW, DELETE_VIEW, NEW_VIEW=60, LABEL=400 };
static bool open,edit_mode,resizing,select_all;
static int tab,scrolls[3],width=420,input;
static char search[3][128],name[96],selected[64],status[160];
/* Text commands borrow backing storage for the entire frame. */
static char row_labels[32][256],caption[128],search_label[160],name_label[128];
static SDL_Rect panel(void) {
    int w=width,sw=Global_GetScreenWidth(),h=Global_GetScreenHeight()-105;
    if(w>sw-24)w=sw-24;if(w<260)w=260;if(h<200)h=200;
    return (SDL_Rect){sw-w-10,85,w,h};
}
static int rows_visible(void) {int n=(panel().h-(edit_mode?215:190))/30;return n>32?32:n>0?n:0;}
static bool inside(int x,int y,SDL_Rect r) {return SDL_PointInRect(&(SDL_Point){x,y},&r);}
static size_t view_indices[LAYOUT_MAX_WORKING_VIEWS];
static bool matches(const char* value,const char* query) {
    if(!query[0])return true;for(;*value;++value){size_t n=0;while(query[n]&&value[n]&&tolower((unsigned char)query[n])==tolower((unsigned char)value[n]))++n;if(!query[n])return true;}return false;
}
size_t UIPanel_VisibilityCount(void) {
    LayoutObjectStore* s=&Global_Get()->layout.objectStore;
    if(tab==2){size_t n=0;for(size_t i=0;i<s->working_view_count;++i)if(matches(s->working_views[i].name,search[2])||matches(s->working_views[i].id,search[2]))view_indices[n++]=i;return n;}
    return tab==1?LDVisibilityTree_BuildLayers(s,search[tab]):LDVisibilityTree_BuildScene(s,search[tab]);
}
bool UIPanel_VisibilityRect(int action,SDL_Rect* out) {
    if(!out||!open)return false;SDL_Rect p=panel();int col=(p.w-24)/3;
    if(action>=TAB_SCENE&&action<=TAB_VIEWS)*out=(SDL_Rect){p.x+8+(action-TAB_SCENE)*(col+4),p.y+32,col,26};
    else if(action==LD_VIEW_ONLY)*out=(SDL_Rect){p.x+8,p.y+64,col,26};
    else if(action==LD_VIEW_RESTORE)*out=(SDL_Rect){p.x+12+col,p.y+64,col,26};
    else if(action==LD_VIEW_COVER_RESET)*out=(SDL_Rect){p.x+16+col*2,p.y+64,col,26};
    else if(action==LD_VIEW_SEARCH)*out=(SDL_Rect){p.x+8,p.y+96,p.w-76,26};
    else if(action==UI_VIS_INSPECT)*out=(SDL_Rect){p.x+150,p.y+p.h-56,140,26};
    else if(action==UI_VIS_REVEAL)*out=(SDL_Rect){p.x+8,p.y+p.h-56,135,26};
    else if(action==EDIT)*out=(SDL_Rect){p.x+p.w-62,p.y+96,54,26};
    else if(action>=LD_VIEW_ROW&&action<LD_VIEW_ROW+rows_visible())*out=(SDL_Rect){p.x+8,p.y+130+(action-LD_VIEW_ROW)*30,28,28};
    else if(action>=LD_VIEW_ROW_ONLY&&action<LD_VIEW_ROW_ONLY+rows_visible())*out=(SDL_Rect){p.x+p.w-60,p.y+130+(action-LD_VIEW_ROW_ONLY)*30,52,28};
    else if(action>=LD_VIEW_ROW_EXPAND&&action<LD_VIEW_ROW_EXPAND+rows_visible()) {
        LDVisibilityRow row;unsigned depth=0;if(tab!=2&&LDVisibilityTree_Row((size_t)scrolls[tab]+action-LD_VIEW_ROW_EXPAND,&row))depth=row.depth;
        *out=(SDL_Rect){p.x+40+(int)(depth>5?5:depth)*12,p.y+130+(action-LD_VIEW_ROW_EXPAND)*30,22,28};
    }else if(action>=LABEL&&action<LABEL+rows_visible()) {
        SDL_Rect arrow;UIPanel_VisibilityRect(LD_VIEW_ROW_EXPAND+action-LABEL,&arrow);
        *out=(SDL_Rect){arrow.x+24,arrow.y,p.x+p.w-66-arrow.x-24,28};
    }else if(action==NAME)*out=(SDL_Rect){p.x+8,p.y+p.h-82,p.w-16,24};
    else if(action>=NEW&&action<=DELETE_GROUP)*out=(SDL_Rect){p.x+8+(action-NEW)*(p.w-16)/5,p.y+p.h-54,(p.w-16)/5-3,24};
    else if(action==NEW_VIEW||action==SAVE_VIEW||action==DELETE_VIEW){int index=action==NEW_VIEW?0:action==SAVE_VIEW?1:2;*out=(SDL_Rect){p.x+8+index*(col+4),p.y+p.h-54,col,24};}
    else return false;return true;
}
void UIPanel_VisibilityReset(void) {UIPanel_CollectionInspectionClose();LDViewControls_Close();UIPanel_Get()->loadMenu.stlCollection[0]=0;UIPanel_Get()->loadMenu.stlPartRole=0;UIPanel_Get()->loadMenu.stlCollectionPicker=false;memset(search,0,sizeof(search));memset(scrolls,0,sizeof(scrolls));selected[0]=0;name[0]=0;tab=0;edit_mode=false;LDVisibilityTree_Reset();}
void UIPanel_VisibilityOpen(void) {open=true;status[0]=0;input=0;}
void UIPanel_VisibilityClose(void) {open=false;input=0;resizing=false;SDL_StopTextInput();}
static const char* selection(void) {
    GlobalState* g=Global_Get();const Object3D* o=Layout_ObjectStore_FindConst(&g->layout.objectStore,g->editor.selectedObject3DId);
    return o?o->coreMeta.object_id:UIPanel_Get()->parts.id;
}
static bool hit(int action,int x,int y) {SDL_Rect r;return UIPanel_VisibilityRect(action,&r)&&inside(x,y,r);}
static void edit_action(int action) {
    GlobalState* g=Global_Get();Layout* l=&g->layout;const LayoutSceneCollection* current=Layout_FindSceneCollection(&l->objectStore,selected);bool ok=false;
    if(action==NEW&&name[0]) {
        LayoutSceneCollection c={0};snprintf(c.name,96,"%s",name);if(current)snprintf(c.parent,64,"%s",current->id);
        ok=Layout_EditSceneCollection(l,&c,NULL,Layout_GeometryHistory,NULL);
        if(ok)snprintf(selected,64,"%s",l->objectStore.scene_collections[l->objectStore.scene_collection_count-1].id);
    }else if(action==RENAME&&current&&name[0]) {
        LayoutSceneCollection c=*current;snprintf(c.name,96,"%s",name);ok=Layout_EditSceneCollection(l,&c,NULL,Layout_GeometryHistory,NULL);
    }else if((action==ADD||action==REMOVE)&&current&&selection()[0])
        ok=Layout_AssignSceneCollection(l,current->id,selection(),action==ADD,Layout_GeometryHistory,NULL);
    else if(action==DELETE_GROUP&&current)ok=Layout_EditSceneCollection(l,NULL,current->id,Layout_GeometryHistory,NULL);
    else if(action==SAVE_VIEW&&name[0]) {
        LayoutWorkingView v={0};snprintf(v.name,96,"%s",name);
        if(tab==2&&selected[0])snprintf(v.id,64,"%s",selected);
        else {unsigned n=1;bool used;do{used=false;snprintf(v.id,64,"working_%u",n++);for(size_t i=0;i<l->objectStore.working_view_count;++i)if(!strcmp(v.id,l->objectStore.working_views[i].id))used=true;}while(used);}
        if(Layout_CaptureWorkingView(&l->objectStore,&v,g->authoringGuides))ok=Layout_SaveWorkingView(l,&v,NULL,Layout_GeometryHistory,NULL);
    }else if(action==DELETE_VIEW&&selected[0])ok=Layout_SaveWorkingView(l,NULL,selected,Layout_GeometryHistory,NULL);
    snprintf(status,sizeof(status),"%s",ok?"Document updated; Save to retain changes.":"Choose a group/name/selection. Remove members before deleting a group.");
    if(ok)Global_FlagHitboxesDirty();
}
bool UIPanel_VisibilityClick(int x,int y) {
    if(!open)return false;SDL_Rect p=panel();if(!inside(x,y,p))return false;
    if(x<p.x+5){resizing=true;return true;}
    if(y<p.y+28&&x>p.x+p.w-30){LDViewControls_Close();return true;}
    for(int i=0;i<3;++i)if(hit(TAB_SCENE+i,x,y)){tab=i;selected[0]=0;input=0;SDL_StopTextInput();return true;}
    if(hit(EDIT,x,y)){edit_mode=!edit_mode;input=0;SDL_StopTextInput();return true;}
    if(hit(LD_VIEW_SEARCH,x,y)){input=1;select_all=false;SDL_StartTextInput();return true;}
    if(edit_mode&&hit(NAME,x,y)){input=2;select_all=false;SDL_StartTextInput();return true;}
    if(edit_mode) {
        if(tab==2){if(hit(NEW_VIEW,x,y)){selected[0]=0;name[0]=0;input=2;SDL_StartTextInput();return true;}if(hit(SAVE_VIEW,x,y)){edit_action(SAVE_VIEW);return true;}if(hit(DELETE_VIEW,x,y)){edit_action(DELETE_VIEW);return true;}}
        else if(tab==0)for(int a=NEW;a<=DELETE_GROUP;++a)if(hit(a,x,y)){edit_action(a);return true;}
    }
    LayoutObjectStore* s=&Global_Get()->layout.objectStore;
    if(!edit_mode&&hit(UI_VIS_INSPECT,x,y)) {
        if(!UIPanel_BeginCollectionInspection(selected))snprintf(status,sizeof(status),"Select a Scene collection label to inspect its dimensions.");
        return true;
    }
    if(!edit_mode&&hit(UI_VIS_REVEAL,x,y)) {
        bool ok=Layout_RevealEntity(s,selected[0]?selected:selection());
        if(ok)Layout_VisibilityExplain(s,selected[0]?selected:selection(),status,sizeof(status));
        else snprintf(status,sizeof(status),"Select a scene group or covering to Reveal.");
        Global_FlagHitboxesDirty();return true;
    }
    if(hit(LD_VIEW_RESTORE,x,y)){LDVisibilityTree_Restore(s);Global_FlagHitboxesDirty();return true;}
    if(hit(LD_VIEW_COVER_RESET,x,y)){Layout_CoveringReset(s);Global_FlagHitboxesDirty();return true;}
    if(hit(LD_VIEW_ONLY,x,y)){if(selection()[0])(void)Layout_IsolateEntity(s,selection());Global_FlagHitboxesDirty();return true;}
    size_t count=UIPanel_VisibilityCount();int* scroll=&scrolls[tab];int max=(int)count-rows_visible();if(max<0)max=0;if(*scroll>max)*scroll=max;if(*scroll<0)*scroll=0;
    for(int r=0;r<rows_visible()&&(size_t)(*scroll+r)<count;++r) {
        if(tab==2) {
            const LayoutWorkingView* v=&s->working_views[view_indices[*scroll+r]];
            if(hit(LABEL+r,x,y)||hit(LD_VIEW_ROW_EXPAND+r,x,y)){snprintf(selected,64,"%s",v->id);snprintf(name,96,"%s",v->name);return true;}
            if(hit(LD_VIEW_ROW_ONLY+r,x,y)||hit(LD_VIEW_ROW+r,x,y)){(void)Layout_ApplyWorkingView(s,v->id,&Global_Get()->authoringGuides);Global_FlagHitboxesDirty();return true;}
            continue;
        }
        LDVisibilityRow row;LDVisibilityTree_Row((size_t)*scroll+r,&row);
        if(hit(LABEL+r,x,y)){snprintf(selected,64,"%s",row.id);snprintf(name,96,"%s",row.info->label);Layout_VisibilityExplain(s,row.id,status,sizeof(status));return true;}
        if(hit(LD_VIEW_ROW_EXPAND+r,x,y)){if(row.assembly)LDVisibilityTree_Expand(row.id);return true;}
        bool ok=true;
        if(hit(LD_VIEW_ROW+r,x,y))ok=LDVisibilityTree_Hide(s,row.id,LDVisibilityTree_State(s,row.id)==1);
        else if(hit(LD_VIEW_ROW_ONLY+r,x,y))ok=LDVisibilityTree_Only(s,row.id);
        else continue;
        if(ok){snprintf(selected,64,"%s",row.id);Layout_VisibilityExplain(s,row.id,status,sizeof(status));}
        if(!ok)snprintf(status,sizeof(status),"This group cannot be changed; inspect its membership.");Global_FlagHitboxesDirty();return true;
    }
    input=0;SDL_StopTextInput();return true;
}
bool UIPanel_VisibilityEvent(const SDL_Event* e) {
    if(!open||!e)return false;
    if(e->type==SDL_MOUSEBUTTONDOWN&&e->button.button==SDL_BUTTON_LEFT)return UIPanel_VisibilityClick(e->button.x,e->button.y);
    if(e->type==SDL_MOUSEBUTTONUP&&resizing){resizing=false;return true;}
    if(e->type==SDL_MOUSEMOTION&&resizing){width=Global_GetScreenWidth()-e->motion.x-10;if(width<300)width=300;if(width>650)width=650;return true;}
    if(e->type==SDL_MOUSEWHEEL){int x,y;SDL_GetMouseState(&x,&y);
#if SDL_VERSION_ATLEAST(2,26,0)
        x=e->wheel.mouseX;y=e->wheel.mouseY;
#endif
        if(!inside(x,y,panel()))return false;scrolls[tab]-=e->wheel.y*3;return true;}
    if(e->type==SDL_KEYDOWN&&e->key.keysym.sym==SDLK_ESCAPE){LDViewControls_Close();return true;}
    if(!input)return false;
    char* value=input==1?search[tab]:name;size_t cap=input==1?128:96;
    if(e->type==SDL_TEXTINPUT){if(select_all){value[0]=0;select_all=false;}size_t n=strlen(value),more=strlen(e->text.text);if(n+more<cap)memcpy(value+n,e->text.text,more+1);scrolls[tab]=0;return true;}
    if(e->type==SDL_KEYDOWN){if(e->key.keysym.sym==SDLK_a&&(e->key.keysym.mod&(KMOD_CTRL|KMOD_GUI))){select_all=true;return true;}if(e->key.keysym.sym==SDLK_BACKSPACE&&select_all){value[0]=0;select_all=false;return true;}size_t n=strlen(value);if(e->key.keysym.sym==SDLK_BACKSPACE&&n){do{--n;}while(n&&((unsigned char)value[n]&0xc0)==0x80);value[n]=0;scrolls[tab]=0;}if(e->key.keysym.sym==SDLK_RETURN){input=0;SDL_StopTextInput();}return true;}
    return e->type==SDL_KEYUP;
}
static void frame(SDL_Renderer* r,SDL_Rect box,bool active) {
    SDL_SetRenderDrawColor(r,active?57:32,active?78:39,active?103:49,255);SDL_RenderFillRect(r,&box);
    SDL_SetRenderDrawColor(r,active?125:70,active?179:83,active?219:99,255);SDL_RenderDrawRect(r,&box);
}
static void text(SDL_Renderer* r,SDL_Rect box,const char* value,SDL_Color color) {
    SDL_Rect old;SDL_bool clipped=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&old);SDL_Rect clip={box.x+3,box.y+1,box.w-6,box.h-2};SDL_RenderSetClipRect(r,&clip);
    (void)line_drawing_text_draw_utf8_at(r,FontManager_Get(FONT_DEFAULT),value,box.x+5,box.y+5,color);SDL_RenderSetClipRect(r,clipped?&old:NULL);
}
static void button(SDL_Renderer* r,int action,const char* label,bool active) {SDL_Rect box;if(UIPanel_VisibilityRect(action,&box)){frame(r,box,active);text(r,box,label,(SDL_Color){228,236,244,255});}}
static void disclosure(SDL_Renderer* r,SDL_Rect box,bool expanded) {
    frame(r,box,expanded);int cx=box.x+box.w/2,cy=box.y+box.h/2;
    SDL_SetRenderDrawColor(r,211,223,236,255);
    for(int i=0;i<6;++i)if(expanded)SDL_RenderDrawLine(r,cx-5+i,cy-2+i,cx+5-i,cy-2+i);else SDL_RenderDrawLine(r,cx-2+i,cy-5+i,cx-2+i,cy+5-i);
}
static void visibility(SDL_Renderer* r,SDL_Rect box,int state) {
    frame(r,box,state!=0);int cx=box.x+box.w/2,cy=box.y+box.h/2;
    SDL_SetRenderDrawColor(r,state?197:117,state?220:130,state?239:147,255);
    SDL_Point eye[]={{cx-9,cy},{cx-5,cy-5},{cx+5,cy-5},{cx+9,cy},{cx+5,cy+5},{cx-5,cy+5},{cx-9,cy}};
    for(int i=0;i<6;++i)SDL_RenderDrawLine(r,eye[i].x,eye[i].y,eye[i+1].x,eye[i+1].y);SDL_Rect pupil={cx-2,cy-2,5,5};SDL_RenderFillRect(r,&pupil);
    if(state==0)SDL_RenderDrawLine(r,cx-9,cy+8,cx+9,cy-8);
    if(state==2){SDL_SetRenderDrawColor(r,238,192,91,255);SDL_Rect mixed={box.x+2,box.y+box.h-4,box.w-4,2};SDL_RenderFillRect(r,&mixed);}
}
void UIPanel_VisibilityRender(SDL_Renderer* r) {
    if(!open||!r)return;SDL_Rect p=panel();frame(r,p,false);
    text(r,(SDL_Rect){p.x+6,p.y,p.w-38,28},"Scene visibility",(SDL_Color){233,240,248,255});
    SDL_SetRenderDrawColor(r,202,216,232,255);SDL_RenderDrawLine(r,p.x+p.w-22,p.y+9,p.x+p.w-12,p.y+19);SDL_RenderDrawLine(r,p.x+p.w-12,p.y+9,p.x+p.w-22,p.y+19);
    button(r,TAB_SCENE,"Scene",tab==0);button(r,TAB_LAYERS,"Layers",tab==1);button(r,TAB_VIEWS,"Views",tab==2);
    button(r,LD_VIEW_ONLY,"Only selection",false);button(r,LD_VIEW_RESTORE,"Restore",false);button(r,LD_VIEW_COVER_RESET,"Reset covers",false);
    snprintf(search_label,sizeof(search_label),"Search: %s",input==1?"":search[tab]);button(r,LD_VIEW_SEARCH,search_label,input==1);button(r,EDIT,"Edit",edit_mode);
    LayoutObjectStore* s=&Global_Get()->layout.objectStore;size_t count=UIPanel_VisibilityCount();int* scroll=&scrolls[tab];int max=(int)count-rows_visible();if(max<0)max=0;if(*scroll>max)*scroll=max;if(*scroll<0)*scroll=0;
    for(int i=0;i<rows_visible()&&(size_t)(*scroll+i)<count;++i) {
        SDL_Rect box;UIPanel_VisibilityRect(LD_VIEW_ROW+i,&box);
        if(tab==2){const LayoutWorkingView* v=&s->working_views[view_indices[*scroll+i]];visibility(r,box,1);snprintf(row_labels[i],256,"%s",v->name);button(r,LABEL+i,row_labels[i],!strcmp(selected,v->id));button(r,LD_VIEW_ROW_ONLY+i,"Apply",false);continue;}
        LDVisibilityRow row;LDVisibilityTree_Row((size_t)*scroll+i,&row);visibility(r,box,LDVisibilityTree_State(s,row.id));
        if(row.assembly){UIPanel_VisibilityRect(LD_VIEW_ROW_EXPAND+i,&box);disclosure(r,box,search[tab][0]||LDVisibilityTree_Expanded(row.id));}
        snprintf(row_labels[i],256,"%s",row.info->label[0]?row.info->label:row.id);button(r,LABEL+i,row_labels[i],!strcmp(selected,row.id));button(r,LD_VIEW_ROW_ONLY+i,"Only",false);
    }
    if(!edit_mode){button(r,UI_VIS_REVEAL,"Reveal selected",false);button(r,UI_VIS_INSPECT,"Inspect area",false);}
    if(edit_mode){snprintf(name_label,sizeof(name_label),"Name: %s",input==2?"":name);button(r,NAME,name_label,input==2);
        if(tab==2){button(r,NEW_VIEW,"New view",false);button(r,SAVE_VIEW,selected[0]?"Update view":"Save view",false);button(r,DELETE_VIEW,"Delete view",false);}
        else if(tab==0){button(r,NEW,"New child",false);button(r,RENAME,"Rename",false);button(r,ADD,"Add pick",false);button(r,REMOVE,"Remove",false);button(r,DELETE_GROUP,"Delete",false);}}
    snprintf(caption,sizeof(caption),"%s",status[0]?status:"Only keeps layer exclusions. Restore exits isolation.");
    text(r,(SDL_Rect){p.x+5,p.y+p.h-26,p.w-10,24},caption,(SDL_Color){171,187,204,255});
}

bool LDInline_Visibility(LDInlineField* f) {if(!open || !input)return false;SDL_Rect box;if(!UIPanel_VisibilityRect(input==1?LD_VIEW_SEARCH:NAME,&box))return false;*f=(LDInlineField){input==1?search[tab]:name,input==1?sizeof(search[0]):sizeof(name),&select_all,LDInline_ValueRect(box,input==1?"Search:":"Name:"),NULL};return true;}

void LDSpecial_Visibility(void) {
    if(!open)return;
    SDL_Rect box,clip=panel();
    int fixed[]={TAB_SCENE,TAB_LAYERS,TAB_VIEWS,LD_VIEW_ONLY,LD_VIEW_RESTORE,LD_VIEW_COVER_RESET,LD_VIEW_SEARCH,EDIT};
    for(size_t i=0;i<sizeof(fixed)/sizeof(*fixed);++i)if(UIPanel_VisibilityRect(fixed[i],&box))LDSpecial_Record(27,fixed[i],"",box,&clip,true);
    size_t count=UIPanel_VisibilityCount();int max=(int)count-rows_visible();if(max<0)max=0;int scroll=scrolls[tab];if(scroll>max)scroll=max;if(scroll<0)scroll=0;
    for(int i=0;i<rows_visible() && (size_t)(scroll+i)<count;++i) {
        LDVisibilityRow row={0};const char* id;
        if(tab==2)id=Global_Get()->layout.objectStore.working_views[view_indices[scroll+i]].id;
        else {LDVisibilityTree_Row((size_t)(scroll+i),&row);id=row.id;}
        int actions[]={LD_VIEW_ROW+i,LD_VIEW_ROW_ONLY+i,LABEL+i,LD_VIEW_ROW_EXPAND+i};
        for(int k=0;k<4;++k)if((k!=3 || (tab!=2 && row.assembly)) && UIPanel_VisibilityRect(actions[k],&box))LDSpecial_Record(27,actions[k],id,box,&clip,true);
    }
    int actions[]={NAME,NEW,RENAME,ADD,REMOVE,DELETE_GROUP,NEW_VIEW,SAVE_VIEW,DELETE_VIEW,UI_VIS_REVEAL,UI_VIS_INSPECT};
    bool seen[64]={false};
    for(size_t i=0;i<sizeof(actions)/sizeof(*actions);++i){int a=actions[i];if(seen[a])continue;seen[a]=true;bool show=!edit_mode?(a==UI_VIS_REVEAL||a==UI_VIS_INSPECT):a==NAME||(tab==2?(a==NEW_VIEW||a==SAVE_VIEW||a==DELETE_VIEW):(tab==0&&a>=NEW&&a<=DELETE_GROUP));if(show&&UIPanel_VisibilityRect(a,&box))LDSpecial_Record(27,a,"",box,&clip,true);}
}

void LDInline_VisibilityChanged(void) {if(open && input==1)scrolls[tab]=0;}
