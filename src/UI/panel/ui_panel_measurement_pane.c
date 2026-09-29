#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_right_scroll.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/font_manager.h"
#include "Core/global_state.h"
#include "Layout/layout_constraints.h"
#include "Layout/scene/layout_object_faces.h"
#include <stdio.h>
#include <string.h>

/* Rendering and hit testing share this layout, including disabled controls and
 * scroll offsets. No geometry or document state is changed by layout/render. */
typedef struct MeasurePane {
    SDL_Renderer* renderer;
    TTF_Font* font;
    UIPanelVisualPalette palette;
    SDL_Rect body;
    int y, h, click_x, click_y, hit, wanted;
    SDL_Rect found;
} MeasurePane;
static bool contains(SDL_Rect r,int x,int y) {
    return x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;
}
static void cell(MeasurePane* p,int action,const char* label,int column,int columns,bool enabled,bool selected) {
    int gap=5,w=(p->body.w-24-(columns-1)*gap)/columns;
    SDL_Rect rect={p->body.x+6+column*(w+gap),p->y,w,p->h-4};
    if (action==p->wanted) p->found=rect;
    if (action && enabled && contains(rect,p->click_x,p->click_y) && contains(p->body,p->click_x,p->click_y)) p->hit=action;
    if (!p->renderer) return;
    SDL_Color text=enabled ? p->palette.text_primary : (SDL_Color){130,135,145,255};
    if (enabled && action==MEASURE_OBJECT_A) text=(SDL_Color){100,210,255,255};
    if (enabled && action==MEASURE_OBJECT_B) text=(SDL_Color){255,190,90,255};
    if (action) {
        SDL_Color fill=selected ? p->palette.pane_border : p->palette.pane_fill;
        SDL_SetRenderDrawColor(p->renderer,fill.r,fill.g,fill.b,255);
        SDL_RenderFillRect(p->renderer,&rect);
        SDL_Color border=selected ? p->palette.accent : p->palette.pane_border;
        SDL_SetRenderDrawColor(p->renderer,border.r,border.g,border.b,255);
        SDL_RenderDrawRect(p->renderer,&rect);
    }
    if (p->font) UIPanelSummary_DrawTextClipped(p->renderer,p->font,label,rect.x+5,rect.y+3,rect.w-10,rect.h-4,text);
}
static void row(MeasurePane* p,int action,const char* label,bool enabled) {
    cell(p,action,label,0,1,enabled,action==MEASURE_VALUE && UIPanel_Get()->measurement.placing!=0); p->y+=p->h;
}
static const char* feature(const LayoutGeometricReference* ref) {
    switch(ref->kind) {
        case LAYOUT_REFERENCE_ORIGIN:return "Center";
        case LAYOUT_REFERENCE_AXIS_U:return "Local U axis";
        case LAYOUT_REFERENCE_AXIS_V:return "Local V axis";
        case LAYOUT_REFERENCE_AXIS_N:return "Local N axis";
        default:return Layout_Object3DFaceKind_Label(ref->face);
    }
}
static LayoutGeometricReference feature_at(LayoutGeometricReference ref,int index,bool plane) {
    ref.kind=index<4 ? (LayoutReferenceKind)index : LAYOUT_REFERENCE_FACE;
    ref.face=index<4 ? OBJECT3D_FACE_NONE : plane ? OBJECT3D_FACE_PLANE_SURFACE :
        (Object3DFaceKind)(OBJECT3D_FACE_RECT_PRISM_NEG_N+index-4);
    return ref;
}
static const Object3D* object(const char* id) {
    const LayoutObjectStore* s=&Global_Get()->layout.objectStore;
    for (size_t i=0;i<s->count;++i) if (!s->items[i].isDeleted && !strcmp(s->items[i].coreMeta.object_id,id)) return &s->items[i];
    return NULL;
}
static const LayoutConstraint* selected(void) {
    int index=UIPanel_Get()->measurement.constraint_index;
    const LayoutObjectStore* s=&Global_Get()->layout.objectStore;
    return index>=0 && (size_t)index<s->constraintCount ? &s->constraints[index] : NULL;
}
static void choices(MeasurePane* p,int chooser) {
    UIPanelState* ui=UIPanel_Get();
    if (ui->measurement.chooser!=chooser) return;
    const LayoutObjectStore* s=&Global_Get()->layout.objectStore;
    if (chooser<=2) {
        int found=0;
        for (size_t i=0;i<s->count;++i) {
            const Object3D* o=&s->items[i];
            if (o->isDeleted || (o->kind!=OBJECT3D_KIND_PLANE && o->kind!=OBJECT3D_KIND_RECT_PRISM)) continue;
            row(p,MEASURE_CHOICE_BASE+(int)i,o->coreMeta.object_id,true); ++found;
        }
        if (!found) row(p,0,"Create a plane or prism first",false);
    } else if (chooser<=4) {
        LayoutGeometricReference ref=ui->measurement.refs[chooser-3];
        const Object3D* o=object(ref.entity_id); if (!o) return;
        bool plane=o->kind==OBJECT3D_KIND_PLANE;
        for (int i=0;i<(plane?5:10);++i) {
            LayoutGeometricReference f=feature_at(ref,i,plane);
            row(p,MEASURE_CHOICE_BASE+i,feature(&f),true);
        }
    } else {
        for (size_t i=0;i<s->constraintCount;++i) {
            char text[160]; const LayoutConstraint* c=&s->constraints[i];
            snprintf(text,sizeof(text),"%s | %s",c->id,c->kind==LAYOUT_CONSTRAINT_DISTANCE ? "Distance" : c->kind==LAYOUT_CONSTRAINT_COINCIDENT ? "Join" : "Angle");
            row(p,MEASURE_CHOICE_BASE+(int)i,text,true);
        }
        if (!s->constraintCount) row(p,0,"No saved rules",false);
    }
}
static MeasurePane build(SDL_Renderer* renderer,int x,int y,int wanted) {
    UIPanelState* ui=UIPanel_Get();
    MeasurePane p={.renderer=renderer,.font=FontManager_Get(FONT_DEFAULT),.body=ui->rightBodyRect,.click_x=x,.click_y=y,.wanted=wanted};
    (void)UIPanelVisual_ResolvePalette(&p.palette);
    p.h=p.font ? TTF_FontHeight(p.font)+12 : 30;
    p.y=p.body.y+6-(int)UIPanel_RightScrollOffset(ui);
    bool has_primitive=false;
    const LayoutObjectStore* store=&Global_Get()->layout.objectStore;
    for(size_t i=0;i<store->count;++i) {
        const Object3D* o=&store->items[i];
        if(!o->isDeleted && (o->kind==OBJECT3D_KIND_PLANE || o->kind==OBJECT3D_KIND_RECT_PRISM))has_primitive=true;
    }
    if(!has_primitive) {
        row(&p,0,"Add objects to start measuring.",true);
        row(&p,0,"Works with planes and prisms.",true);
        row(&p,MEASURE_CREATE,"Create object",true);
        row(&p,MEASURE_FILE,"Open layout",true);
        return p;
    }
    char text[256];
    snprintf(text,sizeof(text),"Units: %s",UIPanel_GetDisplayUnitSymbol()); row(&p,MEASURE_UNITS,text,true);
    for (int i=0;i<2;++i) {
        const LayoutGeometricReference* ref=&ui->measurement.refs[i];
        snprintf(text,sizeof(text),"%c  %s  v",'A'+i,ref->entity_id[0] ? ref->entity_id : "Choose object");
        row(&p,i?MEASURE_OBJECT_B:MEASURE_OBJECT_A,text,true); choices(&p,i+1);
        snprintf(text,sizeof(text),"%s  v",feature(ref));
        cell(&p,i?MEASURE_FEATURE_B:MEASURE_FEATURE_A,text,0,2,object(ref->entity_id)!=NULL,false);
        cell(&p,i?MEASURE_PICK_B:MEASURE_PICK_A,ui->measurement.picking && ui->measurement.slot==i ? "Cancel pick" : "Pick in view",1,2,true,ui->measurement.picking && ui->measurement.slot==i); p.y+=p.h;
        choices(&p,i+3);
    }
    if (ui->measurement.picking) row(&p,0,"Click a reference marker in the view",true);
    const Vec3 axes[]={{1,0,0},{0,1,0},{0,0,1}};
    Vec3 axis=axes[ui->measurement.projection_axis],normal=axes[(ui->measurement.angle_plane+2)%3];
    const LayoutConstraint* rule=selected();
    if (rule && ui->measurement.use_rule_axis) {
        if (rule->kind==LAYOUT_CONSTRAINT_DISTANCE) axis=rule->axis;
        else if (rule->kind==LAYOUT_CONSTRAINT_PLANAR_MATE) normal=rule->axis;
    }
    const char* labels[]={"Distance","Along axis","Angle"};
    LayoutMeasurementResult measured[3];
    for (int i=0;i<3;++i) {
        measured[i]=Layout_Measure(&Global_Get()->layout,&ui->measurement.refs[0],&ui->measurement.refs[1],
            i==2 ? LAYOUT_MEASURE_PLANAR_ANGLE : i==1 ? LAYOUT_MEASURE_PROJECTED_DISTANCE : LAYOUT_MEASURE_POINT_DISTANCE,i==2 ? normal : axis);
        if (measured[i].status==LAYOUT_MEASUREMENT_OK) {
            double value=measured[i].value;
            if (i!=2) (void)core_units_convert(value,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&value);
            snprintf(text,sizeof(text),"%s: %.6g %s",labels[i],value,i==2 ? "deg" : UIPanel_GetDisplayUnitSymbol());
        } else snprintf(text,sizeof(text),"%s: %s",labels[i],i==2 ? "select coplanar axes/faces" : "choose valid references");
        row(&p,0,text,true);
    }
    row(&p,MEASURE_DETAILS,ui->measurement.details_open ? "More measurements  -" : "More measurements  +",true);
    if (ui->measurement.details_open) {
        const LayoutMeasurementKind kinds[]={LAYOUT_MEASURE_DIRECTION_ANGLE,LAYOUT_MEASURE_PARALLEL_PLANE_GAP};
        for(int i=0;i<2;++i) {
            LayoutMeasurementResult m=Layout_Measure(&Global_Get()->layout,&ui->measurement.refs[0],&ui->measurement.refs[1],kinds[i],axis);
            double value=m.value;
            if(i==1)(void)core_units_convert(value,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&value);
            if(m.status==LAYOUT_MEASUREMENT_OK)snprintf(text,sizeof(text),"%s: %.6g %s",i?"Plane gap":"Direction angle",value,i?UIPanel_GetDisplayUnitSymbol():"deg");
            else snprintf(text,sizeof(text),"%s",i?"Gap: select parallel faces":"Direction angle: select axes/faces");
            row(&p,0,text,true);
        }
    }
    for(int i=0;i<3;++i) cell(&p,MEASURE_AXIS_X+i,(const char*[]){"Axis X","Axis Y","Axis Z"}[i],i,3,true,!ui->measurement.use_rule_axis && ui->measurement.projection_axis==i);
    p.y+=p.h;
    for(int i=0;i<3;++i) cell(&p,MEASURE_PLANE_XY+i,(const char*[]){"Plane XY","Plane YZ","Plane ZX"}[i],i,3,true,!ui->measurement.use_rule_axis && ui->measurement.angle_plane==i);
    p.y+=p.h;
    if (rule && ui->measurement.use_rule_axis) {
        snprintf(text,sizeof(text),"Saved %s: %.3g, %.3g, %.3g",rule->kind==LAYOUT_CONSTRAINT_PLANAR_MATE ? "normal" : "axis",rule->axis.x,rule->axis.y,rule->axis.z);
        row(&p,0,text,true);
    }
    row(&p,MEASURE_OFFSETS,ui->measurement.offsets_open ? "Pivot offset  -" : "Pivot offset  +",true);
    if (ui->measurement.offsets_open) {
        cell(&p,MEASURE_SLOT_A,"Reference A",0,2,true,ui->measurement.slot==0);
        cell(&p,MEASURE_SLOT_B,"Reference B",1,2,true,ui->measurement.slot==1);p.y+=p.h;
        for(int i=0;i<3;++i) {
            double value=0;(void)core_units_convert(ui->measurement.refs[ui->measurement.slot].local_offset_meters[i],CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&value);
            snprintf(text,sizeof(text),"Local %c: %.6g %s", "UVN"[i],value,UIPanel_GetDisplayUnitSymbol());
            row(&p,MEASURE_OFFSET_U+i,text,true);
        }
        row(&p,MEASURE_RESET_OFFSET,"Reset offsets",true);
    }
    if (ui->measurement.placing>=7) {
        snprintf(text,sizeof(text),"Local %c [%s]: %s_","UVN"[ui->measurement.placing-7],UIPanel_GetDisplayUnitSymbol(),ui->measurement.placement_text);
        row(&p,MEASURE_VALUE,text,true);
        cell(&p,MEASURE_APPLY_OFFSET,"Set offset",0,2,true,false);cell(&p,MEASURE_CANCEL,"Cancel",1,2,true,false);p.y+=p.h;
    } else if (ui->measurement.placing==6) {
        row(&p,0,"Remove rule? Geometry stays in place.",true);
        cell(&p,MEASURE_SAVE,"Remove rule",0,2,true,false);cell(&p,MEASURE_CANCEL,"Cancel",1,2,true,false);p.y+=p.h;
    } else {
        row(&p,0,"A stays fixed; B moves",true);
        for(int i=0;i<3;++i) cell(&p,MEASURE_DISTANCE+i,(const char*[]){"Distance","Join","Angle"}[i],i,3,true,ui->measurement.operation==i);
        p.y+=p.h;
        if (ui->measurement.operation!=1) {
            snprintf(text,sizeof(text),"Target [%s]: %s%s",ui->measurement.operation==2 ? "deg" : UIPanel_GetDisplayUnitSymbol(),ui->measurement.placement_text[0]?ui->measurement.placement_text:"click to enter",ui->measurement.placing?"_":"");
            row(&p,MEASURE_VALUE,text,true);
        }
        bool valid=measured[ui->measurement.operation==2 ? 2 : ui->measurement.operation==0 ? 1 : 0].status==LAYOUT_MEASUREMENT_OK;
        bool different=strcmp(ui->measurement.refs[0].entity_id,ui->measurement.refs[1].entity_id)!=0;
        cell(&p,MEASURE_ONCE,ui->measurement.operation==1 ? "Join once" : "Move once",0,2,valid && different && ui->measurement.operation!=2,false);
        cell(&p,MEASURE_SAVE,rule?"Update rule":"Save rule",1,2,valid && different && Global_GetWorkspaceMode()==LINE_DRAWING_WORKSPACE_MODE_SCENE,false);p.y+=p.h;
        if (ui->measurement.placing) row(&p,MEASURE_CANCEL,"Cancel input",true);
        if (!different) row(&p,0,"Pick two different objects to move B",false);
        if (ui->measurement.operation==2) row(&p,0,"Angle also joins the two pivot points",true);
        if (Global_GetWorkspaceMode()!=LINE_DRAWING_WORKSPACE_MODE_SCENE) row(&p,0,"Saved rules require the Scene workspace",false);
    }
    snprintf(text,sizeof(text),"Rules: %s  v",rule?rule->id:"choose saved rule");row(&p,MEASURE_RULES,text,true);choices(&p,5);
    cell(&p,MEASURE_NEW,"New rule",0,2,true,false);cell(&p,MEASURE_REMOVE,"Remove",1,2,rule!=NULL,false);p.y+=p.h;
    if (rule) {
        LayoutConstraintFeedback f=Layout_ConstraintFeedback(&Global_Get()->layout,rule);
        row(&p,0,f.satisfied ? "Saved rule: satisfied" : "Saved rule: check references",f.satisfied);
    }
    const char* message=ui->measurement.placement_message;
    /* Wrap feedback to fit the pane instead of truncating the reason for refusal. */
    if (message[0]) {
        size_t length=strlen(message),start=0;
        int chars=(p.body.w-34)/(p.font ? TTF_FontHeight(p.font)/2+1 : 9);if(chars<12) chars=12;
        while(start<length) {
            size_t n=length-start;if(n>(size_t)chars)n=(size_t)chars;
            if(start+n<length) {size_t split=n;while(split && message[start+split]!=' ')--split;if(split)n=split;}
            snprintf(text,sizeof(text),"%.*s",(int)n,message+start);row(&p,0,text,true);start+=n;while(message[start]==' ')++start;
        }
    }
    return p;
}
void UIPanel_LayoutMeasurementPane(void) {
    UIPanelState* ui=UIPanel_Get();
    if(ui->activeRightTab!=UI_PANEL_RIGHT_TAB_MEASURE)return;
    MeasurePane p=build(NULL,-1,-1,0);
    UIPanel_RightScrollSetContentHeight(ui,(float)(p.y-ui->rightBodyRect.y)+UIPanel_RightScrollOffset(ui)+8);
}
bool UIPanel_MeasurementControlRect(int action,SDL_Rect* rect) {
    MeasurePane p=build(NULL,-1,-1,action);if(rect)*rect=p.found;return p.found.w>0;
}
void UIPanel_RenderMeasurement(SDL_Renderer* renderer) {
    UIPanelState* ui=UIPanel_Get();if(!renderer || ui->activeRightTab!=UI_PANEL_RIGHT_TAB_MEASURE)return;
    SDL_Rect old;bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&old);
    SDL_RenderSetClipRect(renderer,&ui->rightBodyRect);(void)build(renderer,-1,-1,0);SDL_RenderSetClipRect(renderer,clipped?&old:NULL);
}
bool UIPanel_MeasurementClick(int x,int y) {
    UIPanelState* ui=UIPanel_Get();if(!ui->measurement.active || ui->activeRightTab!=UI_PANEL_RIGHT_TAB_MEASURE)return false;
    if(!contains(ui->rightBodyRect,x,y)) {
        CorePaneRect vp;
        if(ui->measurement.picking && LineDrawingPaneHost_GetViewportRect(&Global_Get()->paneHost,&vp) &&
            x>=vp.x && y>=vp.y && x<vp.x+vp.width && y<vp.y+vp.height) {
            if(UIPanel_MeasurementPickAt(x,y)) ui->measurement.picking=false;
            snprintf(ui->measurement.placement_message,128,"%s",ui->measurement.pick_message);
            return true;
        }
        return false;
    }
    /* Leave the scrollbar gutter to the existing pane scroll handler. */
    if(x>=ui->rightBodyRect.x+ui->rightBodyRect.w-12)return false;
    MeasurePane p=build(NULL,x,y,0);int action=p.hit;
    if(!action)return true;
    int chooser=ui->measurement.chooser;
    if(action==MEASURE_CREATE || action==MEASURE_FILE) {
        if(action==MEASURE_CREATE)UIPanel_SetActiveRightTab(ui,UI_PANEL_RIGHT_TAB_CREATE);
        else UIPanel_SetActiveLeftTab(ui,UI_PANEL_LEFT_TAB_FILE);
        UIPanel_OnWindowResized(Global_GetScreenWidth(),Global_GetScreenHeight());
    } else if(action>=MEASURE_CHOICE_BASE) {
        int index=action-MEASURE_CHOICE_BASE;
        if(chooser<=2 && chooser>0) {
            LayoutGeometricReference ref={0};snprintf(ref.entity_id,64,"%s",Global_Get()->layout.objectStore.items[index].coreMeta.object_id);
            ui->measurement.refs[chooser-1]=ref;ui->measurement.constraint_index=-1;
        } else if(chooser<=4 && chooser>=3) {
            int slot=chooser-3;const Object3D* o=object(ui->measurement.refs[slot].entity_id);
            if(o)ui->measurement.refs[slot]=feature_at(ui->measurement.refs[slot],index,o->kind==OBJECT3D_KIND_PLANE);
            ui->measurement.constraint_index=-1;
        } else if(chooser==5)UIPanel_MeasurementSelectRule(index);
        ui->measurement.chooser=0;
    } else if(action==MEASURE_OBJECT_A || action==MEASURE_OBJECT_B || action==MEASURE_FEATURE_A || action==MEASURE_FEATURE_B || action==MEASURE_RULES) {
        int next=action==MEASURE_RULES ? 5 : action==MEASURE_OBJECT_A ? 1 : action==MEASURE_OBJECT_B ? 2 : action==MEASURE_FEATURE_A ? 3 : 4;
        UIPanel_MeasurementStopInput();ui->measurement.chooser=chooser==next?0:next;
    } else if(action==MEASURE_PICK_A || action==MEASURE_PICK_B) {
        int slot=action==MEASURE_PICK_A?0:1;bool cancel=ui->measurement.picking && ui->measurement.slot==slot;
        UIPanel_MeasurementStopInput();ui->measurement.slot=slot;ui->measurement.picking=!cancel;
    } else if(action==MEASURE_UNITS) {
        const CoreUnitKind units[]={CORE_UNIT_MILLIMETER,CORE_UNIT_CENTIMETER,CORE_UNIT_METER,CORE_UNIT_INCH,CORE_UNIT_FOOT};
        for(int i=0;i<5;++i)if(UIPanel_GetDisplayUnit()==units[i]){UIPanel_SetDisplayUnit(units[(i+1)%5]);break;}
    } else if(action>=MEASURE_AXIS_X && action<=MEASURE_AXIS_Z) {ui->measurement.projection_axis=action-MEASURE_AXIS_X;ui->measurement.use_rule_axis=false;}
    else if(action>=MEASURE_PLANE_XY && action<=MEASURE_PLANE_ZX) {ui->measurement.angle_plane=action-MEASURE_PLANE_XY;ui->measurement.use_rule_axis=false;}
    else if(action==MEASURE_DETAILS)ui->measurement.details_open=!ui->measurement.details_open;
    else if(action==MEASURE_OFFSETS)ui->measurement.offsets_open=!ui->measurement.offsets_open;
    else if(action==MEASURE_SLOT_A || action==MEASURE_SLOT_B)ui->measurement.slot=action==MEASURE_SLOT_A?0:1;
    else if(action>=MEASURE_OFFSET_U && action<=MEASURE_OFFSET_N) {
        int component=action-MEASURE_OFFSET_U;
        snprintf(ui->measurement.placement_text,64,"%.8g m",ui->measurement.refs[ui->measurement.slot].local_offset_meters[component]);
        UIPanel_MeasurementStartValue(7+component);
    } else if(action==MEASURE_RESET_OFFSET)memset(ui->measurement.refs[ui->measurement.slot].local_offset_meters,0,sizeof(ui->measurement.refs[0].local_offset_meters));
    else if(action>=MEASURE_DISTANCE && action<=MEASURE_ANGLE) {
        UIPanel_MeasurementStopInput();ui->measurement.operation=action-MEASURE_DISTANCE;ui->measurement.placement_text[0]=0;
    } else if(action==MEASURE_VALUE)UIPanel_MeasurementStartValue(ui->measurement.placing>=7 ? ui->measurement.placing : ui->measurement.operation==2?5:3);
    else if(action==MEASURE_APPLY_OFFSET) {
        UIPanel_MeasurementApplyButton(ui->measurement.placing);
        if(!ui->measurement.placing)ui->measurement.placement_text[0]=0;
    }
    else if(action==MEASURE_CANCEL)UIPanel_MeasurementStopInput();
    else if(action==MEASURE_NEW) {UIPanel_MeasurementStopInput();ui->measurement.constraint_index=-1;ui->measurement.use_rule_axis=false;ui->measurement.placement_message[0]=0;}
    else if(action==MEASURE_REMOVE) {UIPanel_MeasurementStopInput();ui->measurement.placing=6;}
    else if(action==MEASURE_ONCE || action==MEASURE_SAVE) {
        int mode=ui->measurement.placing==6?6:ui->measurement.operation==2?5:ui->measurement.operation==1?(action==MEASURE_ONCE?2:4):(action==MEASURE_ONCE?1:3);
        UIPanel_MeasurementApplyButton(mode);
    }
    UIPanel_LayoutMeasurementPane();return true;
}
