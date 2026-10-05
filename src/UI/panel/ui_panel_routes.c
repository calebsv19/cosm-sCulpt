#include "UI/ui_panel_routes.h"
#include "Layout/layout_saved_views.h"
#include <stdlib.h>
#include <ctype.h>
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_right_scroll.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/font_manager.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Editor/editor_numeric_edit.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    SDL_Renderer* renderer; TTF_Font* font; UIPanelVisualPalette palette;
    SDL_Rect body, found; int y, h, x, click_y, hit, wanted;
} RoutePane;
static Layout* layout(void) { return &Global_Get()->layout; }
static bool inside(SDL_Rect r, int x, int y) { return x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h; }
static bool intermediate(void) {
    const UIPanelState* ui = UIPanel_Get();
    return ui->routes.point > 0 && ui->routes.point+1 < ui->routes.draft.point_count;
}
static void message(const char* text) { snprintf(UIPanel_Get()->routes.message,160,"%s",text); }
static void coordinate_text(void) {
    UIPanelState* ui = UIPanel_Get();
    if (ui->routes.point >= ui->routes.draft.point_count) ui->routes.point=0;
    for (int k=0;k<3;++k) {
        double value=ui->routes.draft.points_meters[ui->routes.point][k];
        (void)core_units_convert(value,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&value);
        snprintf(ui->routes.coordinates[k],64,"%.9g %s",value,UIPanel_GetDisplayUnitSymbol());
    }
}
static void design_text(void) {
    UIPanelState* ui=UIPanel_Get();LayoutPhysicalRoute* r=&ui->routes.draft;LayoutRouteElectrical* e=&r->electrical;
    const double values[]={r->radius_meters,r->clearance_meters,r->maximum_length_meters,e->nominal_volts,e->design_amps,e->area_mm2,e->return_meters,e->allowance_meters,e->fuse_amps};
    for (int i=0;i<9;++i) {
        ui->routes.design[i][0]=0;
        if (values[i]>0) snprintf(ui->routes.design[i],64,"%.9g%s",values[i],i<3 || i==6 || i==7?" m":"");
    }
    ui->routes.design[9][0]=0;
}
static char* input_buffer(int action,size_t* capacity) {
    UIPanelState* ui=UIPanel_Get();*capacity=64;
    if (action==ROUTES_NAME) {*capacity=96;return ui->routes.draft.info.label;}
    if (action==ROUTES_CIRCUIT)return ui->routes.draft.electrical.circuit;
    if (action==ROUTES_DOMAIN)return ui->routes.draft.electrical.power_domain;
    if (action>=ROUTES_X && action<=ROUTES_Z)return ui->routes.coordinates[action-ROUTES_X];
    if (action>=ROUTES_RADIUS && action<=ROUTES_AWG)return ui->routes.design[action-ROUTES_RADIUS];
    return NULL;
}
static bool apply_design(void) {
    UIPanelState* ui=UIPanel_Get();LayoutPhysicalRoute* r=&ui->routes.draft;LayoutRouteElectrical e=r->electrical;
    double values[9];
    for (int i=0;i<9;++i) {
        const char* text=ui->routes.design[i];values[i]=0;
        if (!text[0]) continue;
        bool valid;
        if (i<3 || i==6 || i==7) valid=Editor_ParseLength(text,UIPanel_GetDisplayUnit(),&values[i]);
        else {char* end;values[i]=strtod(text,&end);while(isspace((unsigned char)*end))++end;valid=end!=text && !*end;}
        if (!valid || !isfinite(values[i]) || values[i]<0 || values[i]>1e9) {message("Use finite nonnegative inputs; blank means unspecified. Draft not saved.");return false;}
    }
    if (ui->routes.design[9][0]) {
        char* end;double gauge=strtod(ui->routes.design[9],&end);while(isspace((unsigned char)*end))++end;
        if (*end || end==ui->routes.design[9] || !isfinite(gauge) || gauge<-3 || gauge>40 || gauge!=floor(gauge)) {message("AWG: integer 0..40; -1/-2/-3 mean 2/0, 3/0, 4/0.");return false;}
        values[5]=Layout_AWGArea((int)gauge);
    }
    r->radius_meters=values[0];r->clearance_meters=values[1];r->maximum_length_meters=values[2];
    e.nominal_volts=values[3];e.design_amps=values[4];e.area_mm2=values[5];e.return_meters=values[6];e.allowance_meters=values[7];e.fuse_amps=values[8];r->electrical=e;
    return true;
}
static bool apply_coordinates(void) {
    if (!intermediate()) return true;
    UIPanelState* ui=UIPanel_Get(); double point[3];
    for (int k=0;k<3;++k)
        if (!Editor_ParseLength(ui->routes.coordinates[k],UIPanel_GetDisplayUnit(),&point[k]) || fabs(point[k])>1e9) {
            message("Enter finite coordinates, e.g. 1250 mm. Route unchanged."); return false;
        }
    memcpy(ui->routes.draft.points_meters[ui->routes.point],point,sizeof(point));
    return true;
}
void UIPanel_RoutesStopInput(void) {
    UIPanelState* ui=UIPanel_Get();
    if (ui->routes.input) SDL_StopTextInput();
    ui->routes.input=0;ui->routes.chooser=0;ui->routes.picking=false;
}
static void choose(const LayoutPhysicalRoute* route) {
    UIPanelState* ui=UIPanel_Get(); UIPanel_RoutesStopInput();
    ui->routes.draft=ui->routes.observed=*route;
    snprintf(ui->routes.id,64,"%s",route->id);
    ui->routes.observed_valid=true;ui->routes.creating=false;
    ui->routes.endpoints_open=false;ui->routes.remove_pending=false;
    if (ui->routes.point>=route->point_count) ui->routes.point=route->point_count>2?1:0;
    coordinate_text();design_text();ui->routes.check_count=0;ui->routes.inspected_segment=0;
}
static void refresh(void) {
    UIPanelState* ui=UIPanel_Get();
    const LayoutPhysicalRoute* saved=Layout_FindRoute(&layout()->objectStore,ui->routes.id);
    if (saved && (!ui->routes.observed_valid || memcmp(saved,&ui->routes.observed,sizeof(*saved)))) choose(saved);
    else if (!saved && ui->routes.observed_valid) {
        UIPanel_RoutesStopInput();ui->routes.observed_valid=false;
        memset(&ui->routes.draft,0,sizeof(ui->routes.draft));message("Route removed from the document; Redo can restore it.");
    } else if (!ui->routes.id[0] && !ui->routes.creating && layout()->objectStore.route_count)
        choose(&layout()->objectStore.routes[0]);
}
static void new_route(void) {
    UIPanelState* ui=UIPanel_Get();UIPanel_RoutesStopInput();
    memset(&ui->routes,0,sizeof(ui->routes));ui->routes.creating=true;ui->routes.endpoints_open=true;
    ui->routes.draft.point_count=2;
    snprintf(ui->routes.draft.info.label,96,"New cable");snprintf(ui->routes.draft.info.entity_type,64,"Cable");
    coordinate_text();design_text();
}
static void cell(RoutePane* p,int action,const char* title,const char* value,int column,int columns,bool enabled,bool selected) {
    UIPanelState* ui=UIPanel_Get();int gap=6,w=(p->body.w-24-(columns-1)*gap)/columns;
    SDL_Rect r={p->body.x+6+column*(w+gap),p->y,w,p->h-5};
    if (value) {
        int label_width=w/4;
        if (p->renderer) UIPanelSummary_DrawTextClipped(p->renderer,p->font,title,r.x+2,r.y+5,label_width-4,r.h-4,p->palette.text_primary);
        r.x+=label_width;r.w-=label_width;title=value;
    }
    if (action==p->wanted) p->found=r;
    if (action && enabled && inside(r,p->x,p->click_y) && inside(p->body,p->x,p->click_y)) p->hit=action;
    if (!p->renderer) return;
    SDL_Color text=enabled?p->palette.text_primary:p->palette.text_muted;
    if (action) {
        int x,y;Uint32 buttons=SDL_GetMouseState(&x,&y);bool hover=enabled && inside(r,x,y) && inside(p->body,x,y);
        SDL_Color fill=value?UIPanelVisual_AdjustColor(p->palette.pane_fill,-8,0):UIPanelVisual_AdjustColor(p->palette.button_fill,12,0);
        if (action==ROUTES_SAVE && enabled) fill=UIPanelVisual_BlendColor(fill,p->palette.accent,100);
        if (selected || (hover && (buttons&SDL_BUTTON_LMASK))) fill=p->palette.button_fill_active;
        else if (hover && !value) fill=p->palette.button_fill_hover;
        if (!enabled) fill=UIPanelVisual_BlendColor(fill,p->palette.pane_fill,170);
        UIPanelVisual_DrawFrame(p->renderer,r,fill,hover?p->palette.accent:UIPanelVisual_AdjustColor(p->palette.button_border,30,0),0);
        if (selected || ui->routes.input==action) {
            SDL_SetRenderDrawColor(p->renderer,p->palette.accent.r,p->palette.accent.g,p->palette.accent.b,255);
            SDL_Rect line={r.x+3,r.y+r.h-4,r.w-6,2};SDL_RenderFillRect(p->renderer,&line);
        }
    }
    bool dropdown=action==ROUTES_SELECT || action==ROUTES_SOURCE || action==ROUTES_DESTINATION;
    UIPanelSummary_DrawTextClipped(p->renderer,p->font,title,r.x+7,r.y+5,r.w-(dropdown?30:14),r.h-6,text);
    if (dropdown) {
        int x=r.x+r.w-13,y=r.y+r.h/2;
        SDL_SetRenderDrawColor(p->renderer,text.r,text.g,text.b,255);
        SDL_RenderDrawLine(p->renderer,x-4,y-2,x,y+2);SDL_RenderDrawLine(p->renderer,x,y+2,x+4,y-2);
    }
}
static void row(RoutePane* p,int action,const char* text,bool enabled) {cell(p,action,text,NULL,0,1,enabled,false);p->y+=p->h;}
static void field(RoutePane* p,int action,const char* title,const char* value,bool enabled) {cell(p,action,title,value,0,1,enabled,false);p->y+=p->h;}
static void note(RoutePane* p,const char* text) {
    int lines=UIPanelSummary_CountWrappedLines(p->font,text,p->body.w-38);
    if (p->renderer) UIPanelSummary_DrawWrappedText(p->renderer,p->font,text,p->body.x+13,p->y+5,p->body.w-38,
        p->font?TTF_FontHeight(p->font):18,2,lines,p->palette.text_primary);
    p->y+=lines*(p->font?TTF_FontHeight(p->font)+2:20)+12;
}
static void endpoint_label(const LayoutGeometricReference* ref,char* out,size_t capacity) {
    const LayoutEntityInfo* info=Layout_EntityInfo(&layout()->objectStore,ref->entity_id);
    snprintf(out,capacity,"%s",info?(info->label[0]?info->label:ref->entity_id):"Choose object");
}
static bool endpoint_object(const Object3D* object) {
    return !object->isDeleted && (object->kind==OBJECT3D_KIND_PLANE || object->kind==OBJECT3D_KIND_RECT_PRISM);
}
static RoutePane build(SDL_Renderer* renderer,int x,int y,int wanted) {
    UIPanelState* ui=UIPanel_Get();RoutePane p={.renderer=renderer,.font=FontManager_Get(FONT_DEFAULT),
        .body=ui->rightBodyRect,.x=x,.click_y=y,.wanted=wanted};
    (void)UIPanelVisual_ResolvePalette(&p.palette);
    p.h=(p.font?TTF_FontHeight(p.font):18)+16;p.y=p.body.y+8-(int)UIPanel_RightScrollOffset(ui);
    if (Global_GetWorkspaceMode()!=LINE_DRAWING_WORKSPACE_MODE_SCENE) {note(&p,"Routes are edited in the Scene workspace.");return p;}
    char text[256];LayoutObjectStore* store=&layout()->objectStore;
    if (ui->routes.chooser) {
        row(&p,ROUTES_CANCEL,"Close choices",true);
        if (ui->routes.chooser==1) {
            for (size_t i=0;i<store->route_count;++i) row(&p,ROUTES_CHOICE_BASE+(int)i,store->routes[i].info.label,true);
            if (!store->route_count) note(&p,"No saved routes. Choose New route.");
        } else if(ui->routes.chooser==4) {
            note(&p,"Toggle regions for this draft, then Close choices and Save route. Coverage checks the union of selected boxes.");
            for(size_t i=0;i<store->count;++i) {
                const Object3D* o=&store->items[i];if(!Layout_FindCorridor(store,o->coreMeta.object_id))continue;
                bool selected=false;for(size_t j=0;j<ui->routes.draft.corridor_count;++j)if(!strcmp(o->coreMeta.object_id,ui->routes.draft.corridor_ids[j]))selected=true;
                snprintf(text,sizeof(text),"%s %s",selected?"[x]":"[ ]",o->info.label[0]?o->info.label:o->coreMeta.object_id);
                row(&p,ROUTES_CHOICE_BASE+(int)i,text,true);
            }
            row(&p,ROUTES_MARK_CORRIDOR,"Mark selected box as corridor",Global_Get()->editor.selectedObject3DId!=0);
            note(&p,"Create / size a box with existing Object controls, name it in Parts, then mark it here. This changes its semantic type.");
        } else {
            note(&p,ui->routes.chooser==2?"Choose source center":"Choose destination center");
            for (size_t i=0;i<store->count;++i) if (endpoint_object(&store->items[i])) {
                const Object3D* o=&store->items[i];snprintf(text,sizeof(text),"#%u %s",o->objectId,o->info.label[0]?o->info.label:o->coreMeta.object_id);
                row(&p,ROUTES_CHOICE_BASE+(int)i,text,true);
            }
        }
        return p;
    }
    cell(&p,ROUTES_SELECT,ui->routes.creating?"New route draft":ui->routes.id[0]?ui->routes.draft.info.label:"Choose route",NULL,0,2,true,false);
    cell(&p,ROUTES_NEW,"New route",NULL,1,2,true,false);p.y+=p.h;
    if (!ui->routes.creating && !ui->routes.observed_valid) {note(&p,"Create a Cable or Pipe route between two objects.");return p;}
    LayoutPhysicalRoute* draft=&ui->routes.draft;
    field(&p,ROUTES_NAME,"Name",draft->info.label,true);
    cell(&p,ROUTES_CABLE,"Cable",NULL,0,2,true,!strcmp(draft->info.entity_type,"Cable"));
    cell(&p,ROUTES_PIPE,"Pipe",NULL,1,2,true,!strcmp(draft->info.entity_type,"Pipe"));p.y+=p.h;
    double length=Layout_RouteLength(draft);(void)core_units_convert(length,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&length);
    snprintf(text,sizeof(text),"Length: %.6g %s%s",length,UIPanel_GetDisplayUnitSymbol(),ui->routes.creating || memcmp(draft,&ui->routes.observed,sizeof(*draft))?" (draft)":"");
    row(&p,0,text,true);
    bool ready=draft->source.entity_id[0] && draft->destination.entity_id[0];
    bool current=ready && Layout_RouteEndpointsCurrent(layout(),draft);
    note(&p,!ready?"Choose endpoints, then insert intermediate points.":current?"Endpoints match current object positions.":"Endpoints changed. Refresh their positions before trusting this route.");
    if (ready && !current) row(&p,ROUTES_REFRESH,"Refresh endpoints in draft",true);
    row(&p,ROUTES_ENDPOINTS,ui->routes.endpoints_open?"- Endpoints (object centers)":"+ Endpoints (object centers)",true);
    if (ui->routes.endpoints_open) {
        char name[96];endpoint_label(&draft->source,name,sizeof(name));snprintf(text,sizeof(text),"Source: %s",name);
        row(&p,ROUTES_SOURCE,text,true);row(&p,ROUTES_USE_SOURCE,"Use selected object as source",Global_Get()->editor.selectedObject3DId!=0);
        endpoint_label(&draft->destination,name,sizeof(name));snprintf(text,sizeof(text),"Destination: %s",name);
        row(&p,ROUTES_DESTINATION,text,true);row(&p,ROUTES_USE_DESTINATION,"Use selected object as destination",Global_Get()->editor.selectedObject3DId!=0);
    }
    if(ui->routes.observed_valid && !Layout_EntityShown(store,ui->routes.id))row(&p,ROUTES_SHOW,"Show route (clear view filters)",true);
    row(&p,ROUTES_CORRIDORS,ui->routes.corridors_open?"- Corridors / limits":"+ Corridors / limits",true);
    if(ui->routes.corridors_open) {
        snprintf(text,sizeof(text),"Assigned regions: %zu",draft->corridor_count);row(&p,ROUTES_CORRIDOR_CHOOSER,text,true);
        for(size_t i=0;i<draft->corridor_count;++i) {const Object3D* o=Layout_FindCorridor(store,draft->corridor_ids[i]);note(&p,o?(o->info.label[0]?o->info.label:draft->corridor_ids[i]):"Missing region");}
        field(&p,ROUTES_RADIUS,"Radius",ui->routes.design[0],true);field(&p,ROUTES_CLEARANCE,"Clearance",ui->routes.design[1],true);
        field(&p,ROUTES_MAX_LENGTH,"Max length",ui->routes.design[2],true);
    }
    row(&p,ROUTES_ELECTRICAL,ui->routes.electrical_open?"- Electrical":"+ Electrical",true);
    if(ui->routes.electrical_open) {
        snprintf(text,sizeof(text),"Class: %s",(const char*[]){"Unknown","DC power","Signal / CAN","AC power"}[draft->electrical.voltage_class]);row(&p,ROUTES_CLASS,text,true);
        field(&p,ROUTES_CIRCUIT,"Circuit",draft->electrical.circuit,true);field(&p,ROUTES_DOMAIN,"Domain",draft->electrical.power_domain,true);
        field(&p,ROUTES_VOLTS,"Volts (V)",ui->routes.design[3],true);field(&p,ROUTES_AMPS,"Load (A)",ui->routes.design[4],true);
        row(&p,ROUTES_COPPER,draft->electrical.copper?"Material: Copper (20 C)":"Material: Unknown",true);
        field(&p,ROUTES_AREA,"Area mm2",ui->routes.design[5],true);field(&p,ROUTES_AWG,"AWG",ui->routes.design[9],true);
        field(&p,ROUTES_RETURN,"Return",ui->routes.design[6],true);field(&p,ROUTES_ALLOWANCE,"Allowance",ui->routes.design[7],true);field(&p,ROUTES_FUSE,"Fuse (A)",ui->routes.design[8],true);
        note(&p,"Blank = unspecified. AWG converts to area on Save; edit area to override. Return length is explicit; allowance is outgoing extra cable.");
        double drop,percent;
        if(Layout_RouteVoltageDrop(draft,&drop,&percent)) {snprintf(text,sizeof(text),"Applied inputs estimate: %.3g V (%.3g%%)",drop,percent);note(&p,text);}
        else note(&p,"Drop: unknown. Needs DC voltage, load, copper area and return length.");
        note(&p,"Ideal copper at 20 C; excludes contacts, temperature, ampacity and fuse coordination. Signal routes do not use this calculation.");
        row(&p,ROUTES_SAVE,"Save route details",ready);
    }
    row(&p,ROUTES_CHECKS,ui->routes.checks_open?"- Checks":"+ Checks",true);
    if(ui->routes.checks_open) {
        row(&p,ROUTES_RUN_CHECKS,"Check current draft",ready);
        if(ui->routes.check_count)note(&p,"Results are a snapshot. Rerun after edits. Click an issue to inspect its segment / target.");
        size_t n=ui->routes.check_count<128?ui->routes.check_count:128;
        for(size_t i=0;i<n;++i) {
            const LayoutRouteCheck* check=&ui->routes.checks[i];snprintf(text,sizeof(text),"%s %s%s%zu",(const char*[]){"OK","Error","Review"}[check->severity],check->code,check->segment?" · segment ":" · route ",check->segment);
            row(&p,ROUTES_CHECK_BASE+(int)i,text,true);note(&p,check->message);
        }
        if(ui->routes.check_count>128)note(&p,"List truncated at 128; agent report includes total / truncation.");
    }
    p.y+=5;
    snprintf(text,sizeof(text),"Point %zu / %zu%s",ui->routes.point+1,draft->point_count,intermediate()?" · intermediate":ui->routes.point?" · destination":" · source");
    row(&p,0,text,true);
    cell(&p,ROUTES_PREVIOUS,"Previous",NULL,0,2,ui->routes.point>0,false);
    cell(&p,ROUTES_NEXT,"Next",NULL,1,2,ui->routes.point+1<draft->point_count,false);p.y+=p.h;
    for (int k=0;k<3;++k) field(&p,ROUTES_X+k,(const char*[]){"X","Y","Z"}[k],ui->routes.coordinates[k],intermediate());
    cell(&p,ROUTES_INSERT,"Insert point",NULL,0,2,ready && draft->point_count<LAYOUT_MAX_ROUTE_POINTS,false);
    cell(&p,ROUTES_DELETE_POINT,"Delete point",NULL,1,2,intermediate(),false);p.y+=p.h;
    if (intermediate()) {
        cell(&p,ROUTES_EARLIER,"Move earlier",NULL,0,2,ui->routes.point>1,false);
        cell(&p,ROUTES_LATER,"Move later",NULL,1,2,ui->routes.point+2<draft->point_count,false);p.y+=p.h;
        row(&p,ROUTES_PICK,ui->routes.picking?"Cancel point picking":"Pick on construction plane",true);
    }
    cell(&p,ROUTES_SAVE,"Save route",NULL,0,2,ready,false);cell(&p,ROUTES_CANCEL,"Cancel edits",NULL,1,2,true,false);p.y+=p.h;
    if (ui->routes.observed_valid) row(&p,ui->routes.remove_pending?ROUTES_CONFIRM_REMOVE:ROUTES_REMOVE,
        ui->routes.remove_pending?"Confirm delete route":"Delete route...",true);
    note(&p,ui->routes.picking?"Click the viewport to place this point on the current construction plane. Escape cancels.":"Coordinates use display units or a suffix. Save route commits the draft; Undo restores it.");
    if (ui->routes.message[0]) note(&p,ui->routes.message);
    return p;
}
void UIPanel_LayoutRoutes(void) {
    UIPanelState* ui=UIPanel_Get();if(ui->activeRightTab!=UI_PANEL_RIGHT_TAB_ROUTES)return;
    refresh();RoutePane p=build(NULL,-1,-1,0);
    UIPanel_RightScrollSetContentHeight(ui,(float)(p.y-ui->rightBodyRect.y)+UIPanel_RightScrollOffset(ui)+8);
}
void UIPanel_RenderRoutes(SDL_Renderer* renderer) {
    if (renderer && UIPanel_Get()->activeRightTab==UI_PANEL_RIGHT_TAB_ROUTES) (void)build(renderer,-1,-1,0);
}
bool UIPanel_RoutesControlRect(int action,SDL_Rect* rect) {
    if (!rect || UIPanel_Get()->activeRightTab!=UI_PANEL_RIGHT_TAB_ROUTES) return false;
    UIPanel_LayoutRoutes();RoutePane p=build(NULL,-1,-1,action);*rect=p.found;return rect->w>0 && rect->h>0;
}
static bool set_endpoint(int slot,const Object3D* object) {
    if (!object || !endpoint_object(object)) {message("Select a plane or prism endpoint.");return false;}
    if (!apply_coordinates()) return false;
    UIPanelState* ui=UIPanel_Get();LayoutPhysicalRoute* route=&ui->routes.draft;
    LayoutGeometricReference* ref=slot?&route->destination:&route->source;
    memset(ref,0,sizeof(*ref));snprintf(ref->entity_id,64,"%s",object->coreMeta.object_id);
    if (!Layout_RouteEndpoint(layout(),ref,route->points_meters[slot?route->point_count-1:0])) return false;
    coordinate_text();return true;
}
static bool action(int command) {
    UIPanelState* ui=UIPanel_Get();LayoutPhysicalRoute* route=&ui->routes.draft;
    int chooser=ui->routes.chooser;bool was_picking=ui->routes.picking;UIPanel_RoutesStopInput();
    if(command>=ROUTES_CHECK_BASE) {
        size_t i=(size_t)(command-ROUTES_CHECK_BASE);
        if(i<ui->routes.check_count && i<128 && apply_coordinates()) {
            const LayoutRouteCheck* check=&ui->routes.checks[i];if(check->segment && check->segment<route->point_count)ui->routes.point=ui->routes.inspected_segment=check->segment;
            for(size_t j=0;j<layout()->objectStore.count;++j)if(!strcmp(layout()->objectStore.items[j].coreMeta.object_id,check->target_id))Global_Get()->editor.selectedObject3DId=layout()->objectStore.items[j].objectId;
            Layout_ShowAllViews(&layout()->objectStore);coordinate_text();
        }
        return true;
    }
    if (command>=ROUTES_CHOICE_BASE) {
        size_t i=(size_t)(command-ROUTES_CHOICE_BASE);
        if (chooser==1 && i<layout()->objectStore.route_count) choose(&layout()->objectStore.routes[i]);
        else if(chooser==4 && i<layout()->objectStore.count) {
            const Object3D* o=&layout()->objectStore.items[i];bool removed=false;
            for(size_t j=0;j<route->corridor_count;++j)if(!strcmp(o->coreMeta.object_id,route->corridor_ids[j])) {
                memmove(route->corridor_ids[j],route->corridor_ids[j+1],(route->corridor_count-j-1)*64);--route->corridor_count;removed=true;break;
            }
            if(!removed && Layout_FindCorridor(&layout()->objectStore,o->coreMeta.object_id) && route->corridor_count<LAYOUT_MAX_ROUTE_CORRIDORS)
                snprintf(route->corridor_ids[route->corridor_count++],64,"%s",o->coreMeta.object_id);
            ui->routes.chooser=4;
        }
        else if ((chooser==2 || chooser==3) && i<layout()->objectStore.count) (void)set_endpoint(chooser==3,&layout()->objectStore.items[i]);
        return true;
    }
    if (command==ROUTES_NEW) new_route();
    else if (command==ROUTES_SELECT) ui->routes.chooser=1;
    else if (command==ROUTES_SOURCE || command==ROUTES_DESTINATION) ui->routes.chooser=command==ROUTES_SOURCE?2:3;
    else if (command==ROUTES_USE_SOURCE || command==ROUTES_USE_DESTINATION)
        (void)set_endpoint(command==ROUTES_USE_DESTINATION,Layout_ObjectStore_FindConst(&layout()->objectStore,Global_Get()->editor.selectedObject3DId));
    else if(command==ROUTES_CORRIDORS)ui->routes.corridors_open=!ui->routes.corridors_open;
    else if(command==ROUTES_ELECTRICAL)ui->routes.electrical_open=!ui->routes.electrical_open;
    else if(command==ROUTES_CHECKS)ui->routes.checks_open=!ui->routes.checks_open;
    else if(command==ROUTES_CLASS)route->electrical.voltage_class=(route->electrical.voltage_class+1)%4;
    else if(command==ROUTES_COPPER)route->electrical.copper=!route->electrical.copper;
    else if(command==ROUTES_SHOW) {Layout_ShowAllViews(&layout()->objectStore);Global_FlagHitboxesDirty();}
    else if(command==ROUTES_CORRIDOR_CHOOSER)ui->routes.chooser=4;
    else if(command==ROUTES_MARK_CORRIDOR) {
        bool ok=Layout_MarkCorridor(layout(),Global_Get()->editor.selectedObject3DId,Layout_GeometryHistory,NULL);
        message(ok?"Selected box is now a corridor. Name / dimensions remain editable in Parts / Object.":"Choose a design box without reserved-space or motion-envelope roles.");ui->routes.chooser=4;
    } else if(command==ROUTES_RUN_CHECKS) {
        if(apply_coordinates() && apply_design()) {ui->routes.check_count=Layout_CheckRoute(layout(),route,ui->routes.checks,128);message("Checks refreshed for current draft, including hidden obstacles.");}
    }
    else if (command==ROUTES_ENDPOINTS) ui->routes.endpoints_open=!ui->routes.endpoints_open;
    else if (command==ROUTES_CABLE || command==ROUTES_PIPE) snprintf(route->info.entity_type,64,"%s",command==ROUTES_CABLE?"Cable":"Pipe");
    else if (command==ROUTES_NAME || command==ROUTES_CIRCUIT || command==ROUTES_DOMAIN || (command>=ROUTES_RADIUS && command<=ROUTES_AWG) || (command>=ROUTES_X && command<=ROUTES_Z)) {
        if(command==ROUTES_AREA)ui->routes.design[9][0]=0;
        ui->routes.input=command;ui->routes.replace_text=true;SDL_StartTextInput();
    } else if (command==ROUTES_CANCEL) {
        if (!chooser) {
            const LayoutPhysicalRoute* saved=Layout_FindRoute(&layout()->objectStore,ui->routes.id);
            if (saved) choose(saved);else memset(&ui->routes,0,sizeof(ui->routes));
        }
    } else if (command==ROUTES_REFRESH) {
        if (apply_coordinates() && Layout_RouteEndpoint(layout(),&route->source,route->points_meters[0]) &&
            Layout_RouteEndpoint(layout(),&route->destination,route->points_meters[route->point_count-1])) {
            coordinate_text();message("Endpoint positions staged. Save route to commit.");
        }
    } else if (command==ROUTES_SAVE) {
        if (apply_coordinates() && apply_design()) {
            bool creating=!route->id[0];char id[64];snprintf(id,64,"%s",route->id);
            if (Layout_EditRoute(layout(),route,NULL,Layout_GeometryHistory,NULL)) {
                const LayoutPhysicalRoute* saved=creating?&layout()->objectStore.routes[layout()->objectStore.route_count-1]:Layout_FindRoute(&layout()->objectStore,id);
                choose(saved);message("Route saved. Save Layout writes the document to disk.");
            } else message(layout()->geometryMessage);
        }
    } else if (command==ROUTES_REMOVE) ui->routes.remove_pending=true;
    else if (command==ROUTES_CONFIRM_REMOVE) {
        if (Layout_EditRoute(layout(),NULL,ui->routes.id,Layout_GeometryHistory,NULL)) {memset(&ui->routes,0,sizeof(ui->routes));message("Route deleted; Undo restores it.");}
        else message(layout()->geometryMessage);
    } else if (apply_coordinates()) {
        size_t point=ui->routes.point;
        if (command==ROUTES_PREVIOUS && point) --ui->routes.point;
        else if (command==ROUTES_NEXT && point+1<route->point_count) ++ui->routes.point;
        else if (command==ROUTES_INSERT && route->point_count<LAYOUT_MAX_ROUTE_POINTS) {
            size_t i=point+1<route->point_count?point+1:route->point_count-1;
            memmove(route->points_meters[i+1],route->points_meters[i],(route->point_count-i)*sizeof(route->points_meters[0]));
            for (int k=0;k<3;++k) route->points_meters[i][k]=(route->points_meters[i-1][k]+route->points_meters[i+1][k])/2;
            ++route->point_count;ui->routes.point=i;
        } else if (command==ROUTES_DELETE_POINT && intermediate()) {
            memmove(route->points_meters[point],route->points_meters[point+1],(route->point_count-point-1)*sizeof(route->points_meters[0]));
            memset(route->points_meters[--route->point_count],0,sizeof(route->points_meters[0]));
            if (ui->routes.point>=route->point_count-1) ui->routes.point=route->point_count>2?route->point_count-2:0;
        } else if ((command==ROUTES_EARLIER && point>1) || (command==ROUTES_LATER && point+2<route->point_count)) {
            size_t other=command==ROUTES_EARLIER?point-1:point+1;double temporary[3];
            memcpy(temporary,route->points_meters[point],sizeof(temporary));
            memcpy(route->points_meters[point],route->points_meters[other],sizeof(temporary));
            memcpy(route->points_meters[other],temporary,sizeof(temporary));ui->routes.point=other;
        } else if (command==ROUTES_PICK && intermediate()) ui->routes.picking=!was_picking;
        coordinate_text();
    }
    return true;
}
static bool viewport_rect(SDL_Rect* rect) {
    CorePaneRect r;if(!LineDrawingPaneHost_GetViewportRect(&Global_Get()->paneHost,&r))return false;
    *rect=(SDL_Rect){r.x,r.y,r.width,r.height};return r.width>0 && r.height>0;
}
bool UIPanel_RoutesEvent(const SDL_Event* event) {
    UIPanelState* ui=UIPanel_Get();if(ui->activeRightTab!=UI_PANEL_RIGHT_TAB_ROUTES)return false;
    if (event->type==SDL_WINDOWEVENT && event->window.event==SDL_WINDOWEVENT_FOCUS_LOST) {UIPanel_RoutesStopInput();return false;}
    if (event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT) {
        if (inside(ui->rightBodyRect,event->button.x,event->button.y)) {
            refresh();RoutePane p=build(NULL,event->button.x,event->button.y,0);
            if (p.hit) (void)action(p.hit);
            UIPanel_LayoutRoutes();Global_FlagHitboxesDirty();return true;
        }
        SDL_Rect rect;
        if (ui->routes.picking && viewport_rect(&rect) && inside(rect,event->button.x,event->button.y)) {
            SpaceViewContext view=SpaceAdapter_BuildViewContext(Global_Get());Vec3 world;
            if (SpaceAdapter_ScreenToWorld(event->button.x,event->button.y,&Global_Get()->grid,&view,true,&world)) {
                double scale=Layout_WorldScale(layout());double point[3]={world.x*scale,world.y*scale,world.z*scale};
                memcpy(ui->routes.draft.points_meters[ui->routes.point],point,sizeof(point));coordinate_text();
                UIPanel_RoutesStopInput();message("Point staged on construction plane. Save route to commit.");
            } else message("No construction-plane intersection at this view. Choose an orthographic view.");
            return true;
        }
        if (ui->routes.input) UIPanel_RoutesStopInput();
    }
    if (event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_ESCAPE && (ui->routes.input || ui->routes.chooser || ui->routes.picking)) {UIPanel_RoutesStopInput();return true;}
    if (ui->routes.chooser && event->type==SDL_KEYDOWN)return true;
    if (!ui->routes.input)return false;
    size_t capacity;char* buffer=input_buffer(ui->routes.input,&capacity);
    if(!buffer)return false;
    if (event->type==SDL_KEYDOWN) {
        SDL_Keycode key=event->key.keysym.sym;
        if (key==SDLK_RETURN || key==SDLK_KP_ENTER) UIPanel_RoutesStopInput();
        else if (key==SDLK_a && (event->key.keysym.mod&(KMOD_CTRL|KMOD_GUI)))ui->routes.replace_text=true;
        else if (key==SDLK_BACKSPACE && buffer[0]) {
            if (ui->routes.replace_text) buffer[0]=0;
            else {size_t n=strlen(buffer)-1;while(n && ((unsigned char)buffer[n]&0xc0)==0x80)--n;buffer[n]=0;}
            ui->routes.replace_text=false;
        }
        return true;
    }
    if (event->type==SDL_TEXTINPUT) {
        if (ui->routes.replace_text)buffer[0]=0;
        size_t n=strlen(buffer),add=strlen(event->text.text);
        if(n+add<capacity)memcpy(buffer+n,event->text.text,add+1);else message("Text exceeds field capacity.");
        ui->routes.replace_text=false;return true;
    }
    return false;
}
static Vec2 project(const double point[3]) {
    GlobalState* state=Global_Get();double scale=Layout_WorldScale(&state->layout);
    Vec3 world={(float)(point[0]/scale),(float)(point[1]/scale),(float)(point[2]/scale)};
    SpaceViewContext view=SpaceAdapter_BuildViewContext(state);
    return WorldToScreen(SpaceAdapter_ProjectToView(world,&view),&state->grid);
}
static void draw_route(SDL_Renderer* renderer,const LayoutPhysicalRoute* route,bool selected) {
    if(route->point_count<2 || route->point_count>LAYOUT_MAX_ROUTE_POINTS)return;
    SDL_Color color=Layout_RouteEndpointsCurrent(layout(),route)?(SDL_Color){70,230,195,255}:(SDL_Color){255,185,70,255};
    if(Layout_RouteEndpointsCurrent(layout(),route) && route->electrical.voltage_class==1)
        color=route->electrical.nominal_volts>=20?(SDL_Color){255,165,60,255}:(SDL_Color){105,180,255,255};
    if(!selected){color.r/=2;color.g/=2;color.b/=2;}
    SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,color.a);
    for(size_t i=1;i<route->point_count;++i) {
        if(selected) {
            SDL_Color segment_color=UIPanel_Get()->routes.inspected_segment==i?(SDL_Color){255,95,80,255}:color;
            SDL_SetRenderDrawColor(renderer,segment_color.r,segment_color.g,segment_color.b,segment_color.a);
        }
        Vec2 a=project(route->points_meters[i-1]),b=project(route->points_meters[i]);
        if(isfinite(a.x) && isfinite(a.y) && isfinite(b.x) && isfinite(b.y)) {
            SDL_RenderDrawLine(renderer,a.x,a.y,b.x,b.y);
            if(selected)SDL_RenderDrawLine(renderer,a.x+1,a.y,b.x+1,b.y);
        }
    }
    if(selected)for(size_t i=0;i<route->point_count;++i) {
        Vec2 v=project(route->points_meters[i]);if(!isfinite(v.x) || !isfinite(v.y))continue;
        int size=i==UIPanel_Get()->routes.point?10:6;
        if (fabs(v.x)>1e7 || fabs(v.y)>1e7) continue;
        SDL_Rect r={(int)v.x-size/2,(int)v.y-size/2,size,size};SDL_RenderFillRect(renderer,&r);
    }
}
void UIPanel_RenderRouteViewport(SDL_Renderer* renderer) {
    if(!renderer || Global_GetWorkspaceMode()!=LINE_DRAWING_WORKSPACE_MODE_SCENE)return;
    SDL_Rect viewport;if(!viewport_rect(&viewport))return;
    SDL_Rect old;SDL_bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&old);
    SDL_RenderSetClipRect(renderer,&viewport);
    UIPanelState* ui=UIPanel_Get();bool active=ui->activeRightTab==UI_PANEL_RIGHT_TAB_ROUTES;
    for(size_t i=0;i<layout()->objectStore.route_count;++i) {
        const LayoutPhysicalRoute* route=&layout()->objectStore.routes[i];
        if(active && (ui->routes.observed_valid || ui->routes.creating) && !strcmp(route->id,ui->routes.id))continue;
        if(Layout_EntityShown(&layout()->objectStore,route->id))draw_route(renderer,route,false);
    }
    if(active && (ui->routes.creating || (ui->routes.observed_valid && Layout_EntityShown(&layout()->objectStore,ui->routes.id))))draw_route(renderer,&ui->routes.draft,true);
    SDL_RenderSetClipRect(renderer,clipped?&old:NULL);
}
