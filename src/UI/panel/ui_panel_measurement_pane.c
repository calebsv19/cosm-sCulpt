#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_right_scroll.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/font_manager.h"
#include "Core/global_state.h"
#include "Layout/layout_constraints.h"
#include "Editor/editor_reference_edit.h"
#include "Layout/scene/layout_object_faces.h"
#include <stdio.h>
#include <float.h>
#include <math.h>
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
static bool field_action(int action) {
    return action==MEASURE_VALUE || action==MEASURE_GRID || (action>=MEASURE_TRAVEL_MIN && action<=MEASURE_TRAVEL_POSITION) ||
        (action>=MEASURE_OFFSET_U && action<=MEASURE_OFFSET_N);
}
static bool dropdown_action(int action) {
    return action==MEASURE_TOOL || action==MEASURE_VIEW || action==MEASURE_UNITS || action==MEASURE_OBJECT_A || action==MEASURE_OBJECT_B ||
        action==MEASURE_FEATURE_A || action==MEASURE_FEATURE_B || action==MEASURE_RULES;
}
static void cell(MeasurePane* p,int action,const char* label,int column,int columns,bool enabled,bool selected) {
    int gap=6,w=(p->body.w-24-(columns-1)*gap)/columns;
    SDL_Rect rect={p->body.x+6+column*(w+gap),p->y,w,p->h-5};
    bool field=field_action(action);
    const char* value=field ? strstr(label,": ") : NULL;
    SDL_Color text=enabled ? p->palette.text_primary : p->palette.text_muted;
    if(value) {
        int label_width=w/3;
        if(p->renderer && p->font) {
            char title[48];snprintf(title,sizeof(title),"%.*s",(int)(value-label),label);
            UIPanelSummary_DrawTextClipped(p->renderer,p->font,title,rect.x+2,rect.y+5,label_width-4,rect.h-4,text);
        }
        rect.x+=label_width;rect.w-=label_width;label=value+2;
    }
    if (action==p->wanted) p->found=rect;
    if (action && enabled && contains(rect,p->click_x,p->click_y) && contains(p->body,p->click_x,p->click_y)) p->hit=action;
    if (!p->renderer) return;
    int mx,my;Uint32 buttons=SDL_GetMouseState(&mx,&my);
    bool hovered=action && enabled && contains(rect,mx,my) && contains(p->body,mx,my);
    bool pressed=hovered && (buttons & SDL_BUTTON_LMASK);
    UIPanelState* ui=UIPanel_Get();
    bool focused=(action==MEASURE_VALUE && ui->measurement.placing>0) ||
        (action>=MEASURE_TRAVEL_MIN && action<=MEASURE_TRAVEL_POSITION && ui->measurement.placing==10+action-MEASURE_TRAVEL_MIN) ||
        (action>=MEASURE_OFFSET_U && action<=MEASURE_OFFSET_N && ui->measurement.placing==7+action-MEASURE_OFFSET_U);
    bool primary=action==MEASURE_SAVE || action==MEASURE_TRAVEL_SAVE || action==MEASURE_APPLY_OFFSET;
    if (action) {
        SDL_Color fill=field ? UIPanelVisual_AdjustColor(p->palette.workspace_fill,-8,0) :
            UIPanelVisual_AdjustColor(p->palette.button_fill,12,0);
        if(primary && enabled)fill=UIPanelVisual_BlendColor(fill,p->palette.accent,120);
        if(selected || pressed)fill=p->palette.button_fill_active;
        else if(hovered && !field)fill=UIPanelVisual_AdjustColor(p->palette.button_fill_hover,18,0);
        if(!enabled)fill=UIPanelVisual_BlendColor(fill,p->palette.pane_fill,170);
        SDL_Color border=(focused || selected || hovered) ? p->palette.accent :
            UIPanelVisual_AdjustColor(p->palette.button_border,30,0);
        UIPanelVisual_DrawFrame(p->renderer,rect,fill,border,0);
        if(selected) {
            SDL_SetRenderDrawColor(p->renderer,border.r,border.g,border.b,255);
            SDL_Rect underline={rect.x+3,rect.y+rect.h-5,rect.w-6,3};SDL_RenderFillRect(p->renderer,&underline);
        }
    }
    if (enabled && action==MEASURE_OBJECT_A) text=(SDL_Color){100,210,255,255};
    if (enabled && action==MEASURE_OBJECT_B) text=(SDL_Color){255,190,90,255};
    int arrow=dropdown_action(action) ? 18 : 0;
    if (p->font) UIPanelSummary_DrawTextClipped(p->renderer,p->font,label,rect.x+7,rect.y+5,rect.w-14-arrow,rect.h-6,text);
    if(arrow) {
        int cx=rect.x+rect.w-12,cy=rect.y+rect.h/2;
        SDL_SetRenderDrawColor(p->renderer,text.r,text.g,text.b,255);
        SDL_RenderDrawLine(p->renderer,cx-4,cy-2,cx,cy+2);SDL_RenderDrawLine(p->renderer,cx,cy+2,cx+4,cy-2);
    }
}
static void row(MeasurePane* p,int action,const char* label,bool enabled) {
    cell(p,action,label,0,1,enabled,false);p->y+=p->h;
}
static void note(MeasurePane* p,const char* message) {
    char text[256];size_t length=strlen(message),start=0;
    int chars=(p->body.w-34)/(p->font ? TTF_FontHeight(p->font)/2+1 : 9);if(chars<12)chars=12;
    while(start<length) {
        size_t n=length-start;if(n>(size_t)chars)n=(size_t)chars;
        if(start+n<length){size_t split=n;while(split && message[start+split]!=' ')--split;if(split)n=split;}
        snprintf(text,sizeof(text),"%.*s",(int)n,message+start);row(p,0,text,true);
        start+=n;while(message[start]==' ')++start;
    }
}
static void object_label(const Object3D* o,char* out,size_t capacity) {
    if(o)snprintf(out,capacity,"#%u %s",o->objectId,o->kind==OBJECT3D_KIND_RECT_PRISM ? "Prism" : "Plane");
    else snprintf(out,capacity,"Choose object");
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
            char label[96];object_label(o,label,sizeof(label));
            row(p,MEASURE_CHOICE_BASE+(int)i,label,true); ++found;
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
    } else if(chooser==6) {
        const int actions[]={MEASURE_DISTANCE,MEASURE_JOIN,MEASURE_ANGLE,MEASURE_TRAVEL,MEASURE_HINGE};
        const char* names[]={"Distance","Join","Angle","Travel","Hinge"};
        for(int i=0;i<5;++i)row(p,actions[i],names[i],true);
    } else if(chooser==8) {
        const int actions[]={MEASURE_VIEW_TOP,MEASURE_VIEW_SIDE,MEASURE_VIEW_FRONT,MEASURE_VIEW_FREE};
        const char* names[]={"Top (XY)","Side (YZ)","Front (ZX)","Free view"};
        for(int i=0;i<4;++i)row(p,actions[i],names[i],true);
    } else if(chooser==7) {
        const char* names[]={"mm","cm","m","in","ft"};
        for(int i=0;i<5;++i)row(p,MEASURE_CHOICE_BASE+i,names[i],true);
    } else {
        for (size_t i=0;i<s->constraintCount;++i) {
            char text[160]; const LayoutConstraint* c=&s->constraints[i];
            snprintf(text,sizeof(text),"%s | %s",c->id,c->kind==LAYOUT_CONSTRAINT_DISTANCE ? "Distance" : c->kind==LAYOUT_CONSTRAINT_COINCIDENT ? "Join" : c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL ? "Travel" : c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL ? "Hinge" : "Angle");
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
    char text[256],name[96];
    int op=ui->measurement.operation;
    const char* tools[]={"Distance","Join","Angle","Travel","Hinge"};
    snprintf(text,sizeof(text),"Tool: %s",tools[op]);cell(&p,MEASURE_TOOL,text,0,2,true,false);
    snprintf(text,sizeof(text),"Units: %s",UIPanel_GetDisplayUnitSymbol());cell(&p,MEASURE_UNITS,text,1,2,true,false);p.y+=p.h;
    choices(&p,6);choices(&p,7);
    GlobalState* state=Global_Get();
    snprintf(text,sizeof(text),"View: %s",state->freeViewCamera.enabled ? "Free" : state->activePlane.axis==VIEW_PLANE_XY ? "Top (XY)" : state->activePlane.axis==VIEW_PLANE_YZ ? "Side (YZ)" : "Front (ZX)");
    row(&p,MEASURE_VIEW,text,true);choices(&p,8);p.y+=4;
    const LayoutConstraint* rule=selected();
    bool angular=op==4,angle=op==2 || angular;
    bool travel=op==3 || angular,active_travel=travel && rule && rule->kind==(angular ? LAYOUT_CONSTRAINT_ANGULAR_TRAVEL : LAYOUT_CONSTRAINT_LINEAR_TRAVEL);
    for(int i=0;i<2;++i) {
        const LayoutGeometricReference* ref=&ui->measurement.refs[i];
        row(&p,0,i ? "B  Moving object" : "A  Fixed reference",true);
        object_label(object(ref->entity_id),name,sizeof(name));
        cell(&p,i?MEASURE_OBJECT_B:MEASURE_OBJECT_A,name,0,2,true,false);
        cell(&p,i?MEASURE_PICK_B:MEASURE_PICK_A,ui->measurement.picking && ui->measurement.slot==i ? "Cancel pick" : "Pick in view",1,2,true,ui->measurement.picking && ui->measurement.slot==i);p.y+=p.h;
        choices(&p,i+1);
        if(angle) {
            snprintf(text,sizeof(text),"%c direction: %s",'A'+i,feature(ref));
            row(&p,i?MEASURE_FEATURE_B:MEASURE_FEATURE_A,text,object(ref->entity_id)!=NULL);choices(&p,i+3);
        }
    }
    if(ui->measurement.picking)note(&p,"Click a highlighted reference in the view.");
    const Vec3 axes[]={{1,0,0},{0,1,0},{0,0,1}};
    Vec3 axis=axes[ui->measurement.projection_axis],normal=axes[(ui->measurement.angle_plane+2)%3];
    if(rule && ui->measurement.use_rule_axis) {
        if(rule->kind==LAYOUT_CONSTRAINT_DISTANCE || rule->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL)axis=rule->axis;
        else if(rule->kind==LAYOUT_CONSTRAINT_PLANAR_MATE || rule->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL)normal=rule->axis;
    }
    if(op==0 || (travel && !angular)) {
        row(&p,0,travel ? "Travel direction (world)" : "Measure along (world)",true);
        for(int i=0;i<3;++i) {
            bool chosen=axis.x==axes[i].x && axis.y==axes[i].y && axis.z==axes[i].z;
            cell(&p,MEASURE_AXIS_X+i,(const char*[]){"X","Y","Z"}[i],i,3,!active_travel,chosen);
        }
        p.y+=p.h;
    } else if(angle) {
        row(&p,0,angular ? "Hinge plane" : "Angle plane",true);
        for(int i=0;i<3;++i)cell(&p,MEASURE_PLANE_XY+i,(const char*[]){"XY","YZ","ZX"}[i],i,3,!active_travel,
            normal.x==axes[(i+2)%3].x && normal.y==axes[(i+2)%3].y && normal.z==axes[(i+2)%3].z);
        p.y+=p.h;
    }
    bool different=strcmp(ui->measurement.refs[0].entity_id,ui->measurement.refs[1].entity_id)!=0;
    LayoutMeasurementResult measured=Layout_Measure(&Global_Get()->layout,&ui->measurement.refs[0],&ui->measurement.refs[1],
        angle ? LAYOUT_MEASURE_PLANAR_ANGLE : op==1 ? LAYOUT_MEASURE_POINT_DISTANCE : LAYOUT_MEASURE_PROJECTED_DISTANCE,angle?normal:axis);
    bool valid=measured.status==LAYOUT_MEASUREMENT_OK && different;
    bool scene=Global_GetWorkspaceMode()==LINE_DRAWING_WORKSPACE_MODE_SCENE;
    p.y+=5;
    if(angular)row(&p,MEASURE_PIVOT_EDITOR,"Edit pivot offsets...",true);
    if(ui->measurement.placing==6) {
        note(&p,"Remove constraint? Objects stay in place.");
        cell(&p,MEASURE_SAVE,"Remove",0,2,true,false);cell(&p,MEASURE_CANCEL,"Cancel",1,2,true,false);p.y+=p.h;
    } else if(travel) {
        for(int i=0;i<3;++i) {
            const char* value=ui->measurement.placing==10+i ? ui->measurement.placement_text : ui->measurement.travel_text[i];
            snprintf(text,sizeof(text),"%s: %s%s",(const char*[]){"Min","Max","Position"}[i],value,ui->measurement.placing==10+i?"|":"");
            row(&p,MEASURE_TRAVEL_MIN+i,text,true);
        }
        row(&p,MEASURE_TRAVEL_SAVE,active_travel ? "Apply changes" : angular ? "Create hinge" : "Create travel",valid && scene);
        if(!valid)note(&p,!different ? "Choose two different objects." : angular ? "Choose axes/faces in the hinge plane." : "Choose valid reference points.");
        if(ui->measurement.placement_message[0])note(&p,ui->measurement.placement_message);
        bool pending=false;
        if(active_travel) {
            pending=memcmp(&rule->a,&ui->measurement.refs[0],sizeof(rule->a)) || memcmp(&rule->b,&ui->measurement.refs[1],sizeof(rule->b));
            const double saved_values[]={rule->travel_min,rule->travel_max,rule->target};
            for(int i=0;i<3;++i) {
                const char* input=ui->measurement.placing==10+i ? ui->measurement.placement_text : ui->measurement.travel_text[i];
                double value;
                if(!UIPanel_TravelParse(input,angular,&value) || fabs(value-saved_values[i])>1e-6)pending=true;
            }
        }
        double current=active_travel ? rule->target : measured.value;
        if(!angular)(void)core_units_convert(current,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&current);
        if(pending)snprintf(text,sizeof(text),"Apply edits or Reset to move.");
        else snprintf(text,sizeof(text),active_travel ? "Move: %.6g %s" : angular ? "Move (create hinge to enable)" : "Move (create travel to enable)",current,angular ? "deg" : UIPanel_GetDisplayUnitSymbol());
        row(&p,0,text,true);
        bool movable=active_travel && !pending && rule->travel_max>rule->travel_min;
        SDL_Rect track={p.body.x+6,p.y,p.body.w-24,p.h-5};
        cell(&p,MEASURE_TRAVEL_SLIDER,"",0,1,movable,false);
        if(renderer) {
            double t=movable ? (rule->target-rule->travel_min)/(rule->travel_max-rule->travel_min) : 0.5;
            SDL_Color color=movable ? p.palette.accent : p.palette.text_muted;
            SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,255);
            SDL_Rect rail={track.x+6,track.y+track.h/2-2,track.w-12,4};SDL_RenderFillRect(renderer,&rail);
            SDL_Rect thumb={track.x+1+(int)(t*(track.w-12)),track.y+3,10,track.h-6};SDL_RenderFillRect(renderer,&thumb);
        }
        p.y+=p.h;
        cell(&p,MEASURE_TRAVEL_TO_MIN,"Min",0,3,active_travel && !pending,false);
        cell(&p,MEASURE_TRAVEL_TO_MAX,"Max",1,3,active_travel && !pending,false);
        cell(&p,MEASURE_TRAVEL_RESET,"Reset",2,3,active_travel,false);p.y+=p.h;
        if(angular)note(&p,"B rotates around the joined A/B pivot.");
        else {row(&p,0,"Position is measured from A along",true);row(&p,0,"the direction above; B does not rotate.",true);}
    } else if(ui->measurement.placing<7 || ui->measurement.placing>9) {
        if(measured.status==LAYOUT_MEASUREMENT_OK) {
            double value=measured.value;if(op!=2)(void)core_units_convert(value,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&value);
            snprintf(text,sizeof(text),"%s: %.6g %s",op==2?"Current angle":op==1?"Point gap":"Current distance",value,op==2?"deg":UIPanel_GetDisplayUnitSymbol());
        } else snprintf(text,sizeof(text),"%s",op==2 ? "Choose axes/faces in the angle plane." : "Choose valid reference points.");
        note(&p,text);
        if(op!=1) {
            snprintf(text,sizeof(text),"Target: %s%s",ui->measurement.placement_text[0]?ui->measurement.placement_text:"Enter value",ui->measurement.placing==3 || ui->measurement.placing==5 ? "|" : "");
            row(&p,MEASURE_VALUE,text,true);
        }
        row(&p,MEASURE_SAVE,rule ? "Apply changes" : op==1 ? "Create join" : op==2 ? "Create angle" : "Create distance",valid && scene);
        if(op!=2)row(&p,MEASURE_ONCE,op==1?"Join once (no constraint)":"Move once (no constraint)",valid);
        if(op==2)note(&p,"Joins the pivot points and sets the angle.");
        if(ui->measurement.placement_message[0])note(&p,ui->measurement.placement_message);
    }
    if(!scene)note(&p,"Create constraints in the Scene workspace.");
    p.y+=8;
    row(&p,MEASURE_ADVANCED,ui->measurement.advanced_open ? "- Advanced" : "+ Advanced",true);
    if(ui->measurement.advanced_open) {
        double grid=Global_Get()->layout.gridSize*Layout_WorldScale(&Global_Get()->layout);
        (void)core_units_convert(grid,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&grid);
        snprintf(text,sizeof(text),"Grid step: %.6g %s",grid,UIPanel_GetDisplayUnitSymbol());
        if(ui->measurement.placing==13)snprintf(text,sizeof(text),"Grid step: %s|",ui->measurement.placement_text);
        row(&p,MEASURE_GRID,text,true);
        if(ui->measurement.placing==13) {
            cell(&p,MEASURE_GRID_APPLY,"Apply grid",0,2,true,false);cell(&p,MEASURE_CANCEL,"Cancel",1,2,true,false);p.y+=p.h;
        }
        if(!angle)for(int i=0;i<2;++i) {
            snprintf(text,sizeof(text),"%c reference: %s",'A'+i,feature(&ui->measurement.refs[i]));
            row(&p,i?MEASURE_FEATURE_B:MEASURE_FEATURE_A,text,true);choices(&p,i+3);
        }
        if(rule) {
            snprintf(text,sizeof(text),"Saved axis: %.3g, %.3g, %.3g",rule->axis.x,rule->axis.y,rule->axis.z);row(&p,0,text,true);
            if(active_travel) {
                double home=rule->travel_home;if(!angular)(void)core_units_convert(home,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&home);
                snprintf(text,sizeof(text),"Reset position: %.6g %s",home,angular ? "deg" : UIPanel_GetDisplayUnitSymbol());row(&p,0,text,true);
                note(&p,"Remove movement before changing its plane/axis.");
            }
        }
        row(&p,MEASURE_OFFSETS,ui->measurement.offsets_open ? "- Pivot offsets" : "+ Pivot offsets",true);
        if(ui->measurement.offsets_open) {
            cell(&p,MEASURE_SLOT_A,"Reference A",0,2,true,ui->measurement.slot==0);
            cell(&p,MEASURE_SLOT_B,"Reference B",1,2,true,ui->measurement.slot==1);p.y+=p.h;
            for(int i=0;i<3;++i) {
                double value=0;(void)core_units_convert(ui->measurement.refs[ui->measurement.slot].local_offset_meters[i],CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&value);
                snprintf(text,sizeof(text),"Local %c: %.6g %s","UVN"[i],value,UIPanel_GetDisplayUnitSymbol());row(&p,MEASURE_OFFSET_U+i,text,true);
            }
            if(ui->measurement.placing>=7 && ui->measurement.placing<=9) {
                snprintf(text,sizeof(text),"Offset: %s|",ui->measurement.placement_text);row(&p,MEASURE_VALUE,text,true);
                cell(&p,MEASURE_APPLY_OFFSET,"Set offset",0,2,true,false);cell(&p,MEASURE_CANCEL,"Cancel",1,2,true,false);p.y+=p.h;
            }
            row(&p,MEASURE_RESET_OFFSET,"Reset offsets",true);
        }
        row(&p,MEASURE_DETAILS,ui->measurement.details_open ? "- Other measurements" : "+ Other measurements",true);
        if(ui->measurement.details_open) {
            const LayoutMeasurementKind kinds[]={LAYOUT_MEASURE_POINT_DISTANCE,LAYOUT_MEASURE_DIRECTION_ANGLE,LAYOUT_MEASURE_PARALLEL_PLANE_GAP};
            for(int i=0;i<3;++i) {
                LayoutMeasurementResult m=Layout_Measure(&Global_Get()->layout,&ui->measurement.refs[0],&ui->measurement.refs[1],kinds[i],axis);
                double v=m.value;if(i!=1)(void)core_units_convert(v,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&v);
                if(m.status==LAYOUT_MEASUREMENT_OK)snprintf(text,sizeof(text),"%s: %.6g %s",(const char*[]){"Point distance","Direction angle","Plane gap"}[i],v,i==1?"deg":UIPanel_GetDisplayUnitSymbol());
                else snprintf(text,sizeof(text),"%s: unavailable",(const char*[]){"Point distance","Direction angle","Plane gap"}[i]);
                note(&p,text);
            }
        }
    }
    snprintf(text,sizeof(text),"%c Saved constraints (%zu)",ui->measurement.rules_open?'-':'+',store->constraintCount);
    row(&p,MEASURE_SAVED,text,true);
    if(ui->measurement.rules_open) {
        snprintf(text,sizeof(text),"%s",rule?rule->id:"Choose constraint");row(&p,MEASURE_RULES,text,true);choices(&p,5);
        cell(&p,MEASURE_NEW,"New",0,2,true,false);cell(&p,MEASURE_REMOVE,"Remove",1,2,rule!=NULL,false);p.y+=p.h;
        if(rule)row(&p,0,Layout_ConstraintFeedback(&Global_Get()->layout,rule).satisfied ? "Constraint satisfied" : "Check constraint references",true);
    }
    return p;
}
void UIPanel_LayoutMeasurementPane(void) {
    UIPanelState* ui=UIPanel_Get();
    if(ui->activeRightTab!=UI_PANEL_RIGHT_TAB_MEASURE)return;
    if(ui->measurement.observed_rule_valid && ui->measurement.constraint_index>=0) {
        const LayoutObjectStore* store=&Global_Get()->layout.objectStore;int found=-1;
        for(size_t i=0;i<store->constraintCount;++i)if(!strcmp(store->constraints[i].id,ui->measurement.observed_rule.id))found=(int)i;
        if(found<0){UIPanel_MeasurementStopInput();ui->measurement.constraint_index=-1;ui->measurement.observed_rule_valid=false;}
        else if(memcmp(&store->constraints[found],&ui->measurement.observed_rule,sizeof(LayoutConstraint))) {
            UIPanel_MeasurementStopInput();UIPanel_MeasurementSelectRule(found);
            snprintf(ui->measurement.placement_message,128,"Controls refreshed from the saved constraint.");
        } else ui->measurement.constraint_index=found;
    }
    GlobalState* state=Global_Get();
    if(state->grid.gridSize!=state->layout.gridSize && isfinite(state->layout.gridSize) && state->layout.gridSize>0) {
        double pixels=state->grid.gridSize*state->grid.scale;state->grid.gridSize=state->layout.gridSize;
        state->grid.scale=(float)(pixels/state->grid.gridSize);Global_FlagGridChanged();Global_FlagHitboxesDirty();
    }
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
/* Preserve the physical value of pending fields when display units change. */
static bool change_units(CoreUnitKind next) {
    UIPanelState* ui=UIPanel_Get();UIPanel_MeasurementStopInput();
    double travel[3]={0},target=0;bool has_target=ui->measurement.placement_text[0] && ui->measurement.operation==0;
    if(ui->measurement.operation==3)for(int i=0;i<3;++i) {
        if(!Editor_ParseLength(ui->measurement.travel_text[i],UIPanel_GetDisplayUnit(),&travel[i])) {
            snprintf(ui->measurement.placement_message,128,"Correct the length fields before changing units.");return false;
        }
    }
    if(has_target && !Editor_ParseLength(ui->measurement.placement_text,UIPanel_GetDisplayUnit(),&target)) {
        snprintf(ui->measurement.placement_message,128,"Correct the target before changing units.");return false;
    }
    if(!UIPanel_SetDisplayUnit(next))return false;
    if(ui->measurement.operation==3)for(int i=0;i<3;++i) {
        (void)core_units_convert(travel[i],CORE_UNIT_METER,next,&travel[i]);
        snprintf(ui->measurement.travel_text[i],64,"%.9g %s",travel[i],UIPanel_GetDisplayUnitSymbol());
    }
    if(has_target) {
        (void)core_units_convert(target,CORE_UNIT_METER,next,&target);
        snprintf(ui->measurement.placement_text,64,"%.9g %s",target,UIPanel_GetDisplayUnitSymbol());
    }
    return true;
}
static void choose_tool(int operation) {
    UIPanelState* ui=UIPanel_Get();UIPanel_MeasurementStopInput();
    if(ui->measurement.operation==operation)return;
    ui->measurement.constraint_index=-1;ui->measurement.use_rule_axis=false;
    ui->measurement.operation=operation;ui->measurement.placement_text[0]=0;ui->measurement.placement_message[0]=0;
    if(operation==3)UIPanel_TravelAction(MEASURE_TRAVEL);
    if(operation==2 || operation==4)for(int i=0;i<2;++i) {
        if(ui->measurement.refs[i].kind==LAYOUT_REFERENCE_ORIGIN)ui->measurement.refs[i].kind=LAYOUT_REFERENCE_AXIS_U;
    }
    if(operation==4)UIPanel_TravelAction(MEASURE_HINGE);
    ui->rightScroll[UI_PANEL_RIGHT_TAB_MEASURE].scrollOffsetPx=0;
}
bool UIPanel_ApplyEngineeringGrid(const char* text) {
    GlobalState* state=Global_Get();double meters;
    if(!Editor_ParseLength(text,UIPanel_GetDisplayUnit(),&meters) || meters<=0) {
        snprintf(UIPanel_Get()->measurement.placement_message,128,"Grid step must be a positive length, e.g. 25 mm.");return false;
    }
    double world=meters/Layout_WorldScale(&state->layout);float step=(float)world;
    if(!isfinite(world) || world>FLT_MAX || step<=0 || fabs(step-world)>1e-7*world) {
        snprintf(UIPanel_Get()->measurement.placement_message,128,"Grid step exceeds numerical range.");return false;
    }
    double next_scale=state->grid.gridSize*state->grid.scale/step;
    if(!isfinite(next_scale) || next_scale>FLT_MAX || next_scale<FLT_MIN) {
        snprintf(UIPanel_Get()->measurement.placement_message,128,"Grid step exceeds viewport range.");return false;
    }
    if(step!=state->layout.gridSize) {
        if(!Layout_GeometryHistory(&state->layout,NULL))return false;
        /* Grid spacing changes presentation/snapping, not physical object scale. */
        double pixels=state->grid.gridSize*state->grid.scale;
        state->layout.gridSize=step;state->grid.gridSize=step;state->grid.scale=(float)(pixels/step);
        Global_FlagLayoutChanged();Global_FlagGridChanged();Global_FlagHitboxesDirty();
    }
    UIPanel_MeasurementStopInput();
    snprintf(UIPanel_Get()->measurement.placement_message,128,"Grid step applied.");return true;
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
    if(action==MEASURE_TOOL || action==MEASURE_UNITS || action==MEASURE_VIEW) {
        int next=action==MEASURE_TOOL ? 6 : action==MEASURE_VIEW ? 8 : 7;
        UIPanel_MeasurementStopInput();ui->measurement.chooser=chooser==next ? 0 : next;
    } else if(action>=MEASURE_VIEW_TOP && action<=MEASURE_VIEW_FREE) {
        UIPanel_MeasurementStopInput();
        GlobalState* state=Global_Get();state->freeViewCamera.enabled=action==MEASURE_VIEW_FREE;
        if(action!=MEASURE_VIEW_FREE)state->activePlane=(ViewPlane){.axis=action==MEASURE_VIEW_TOP ? VIEW_PLANE_XY : action==MEASURE_VIEW_SIDE ? VIEW_PLANE_YZ : VIEW_PLANE_XZ,.offset=0};
        ui->measurement.chooser=0;Global_FlagGridChanged();Global_FlagHitboxesDirty();
    } else if(action==MEASURE_PIVOT_EDITOR) {
        UIPanel_MeasurementStopInput();ui->measurement.advanced_open=true;ui->measurement.offsets_open=true;
        SDL_Rect pivot;if(UIPanel_MeasurementControlRect(MEASURE_OFFSETS,&pivot))
            ui->rightScroll[UI_PANEL_RIGHT_TAB_MEASURE].scrollOffsetPx+=pivot.y-ui->rightBodyRect.y;
    } else if(action==MEASURE_GRID) {
        double value=Global_Get()->layout.gridSize*Layout_WorldScale(&Global_Get()->layout);
        (void)core_units_convert(value,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&value);
        snprintf(ui->measurement.placement_text,64,"%.9g %s",value,UIPanel_GetDisplayUnitSymbol());UIPanel_MeasurementStartValue(13);
    } else if(action==MEASURE_GRID_APPLY) {
        (void)UIPanel_ApplyEngineeringGrid(ui->measurement.placement_text);
    } else if(action==MEASURE_ADVANCED) {
        UIPanel_MeasurementStopInput();ui->measurement.advanced_open=!ui->measurement.advanced_open;
    } else if(action==MEASURE_SAVED) {
        UIPanel_MeasurementStopInput();ui->measurement.rules_open=!ui->measurement.rules_open;
    } else if(action==MEASURE_TRAVEL)choose_tool(3);
    else if(action==MEASURE_HINGE)choose_tool(4);
    else if(action>=MEASURE_TRAVEL_MIN && action<=MEASURE_TRAVEL_SLIDER) {
        if(action==MEASURE_TRAVEL_SLIDER) {
            const LayoutConstraint* c=selected();
            if(c && UIPanel_MeasurementControlRect(action,&ui->measurement.travel_track)) {
                UIPanel_MeasurementStopInput();
                snprintf(ui->measurement.travel_drag_id,64,"%s",c->id);
                ui->measurement.travel_dragging=true;
                Layout_BeginGeometryGesture(&Global_Get()->layout);
                SDL_Event e={.type=SDL_MOUSEMOTION};e.motion.x=x;
                (void)UIPanel_TravelEvent(&e);
            }
        } else UIPanel_TravelAction(action);
    } else if(action==MEASURE_CREATE || action==MEASURE_FILE) {
        if(action==MEASURE_CREATE)UIPanel_SetActiveRightTab(ui,UI_PANEL_RIGHT_TAB_CREATE);
        else UIPanel_SetActiveLeftTab(ui,UI_PANEL_LEFT_TAB_FILE);
        UIPanel_OnWindowResized(Global_GetScreenWidth(),Global_GetScreenHeight());
    } else if(action>=MEASURE_CHOICE_BASE) {
        int index=action-MEASURE_CHOICE_BASE;
        if(chooser<=2 && chooser>0) {
            LayoutGeometricReference ref={0};snprintf(ref.entity_id,64,"%s",Global_Get()->layout.objectStore.items[index].coreMeta.object_id);
            if(ui->measurement.operation==4 || ui->measurement.operation==2)ref.kind=LAYOUT_REFERENCE_AXIS_U;
            ui->measurement.refs[chooser-1]=ref;ui->measurement.constraint_index=-1;
        } else if(chooser<=4 && chooser>=3) {
            int slot=chooser-3;const Object3D* o=object(ui->measurement.refs[slot].entity_id);
            if(o)ui->measurement.refs[slot]=feature_at(ui->measurement.refs[slot],index,o->kind==OBJECT3D_KIND_PLANE);
            ui->measurement.constraint_index=-1;
        } else if(chooser==5) {
            UIPanel_MeasurementStopInput();UIPanel_MeasurementSelectRule(index);
            ui->measurement.rules_open=false;ui->rightScroll[UI_PANEL_RIGHT_TAB_MEASURE].scrollOffsetPx=0;
        } else if(chooser==7) {
            const CoreUnitKind units[]={CORE_UNIT_MILLIMETER,CORE_UNIT_CENTIMETER,CORE_UNIT_METER,CORE_UNIT_INCH,CORE_UNIT_FOOT};
            if(index>=0 && index<5)(void)change_units(units[index]);
        }
        ui->measurement.chooser=0;
    } else if(action==MEASURE_OBJECT_A || action==MEASURE_OBJECT_B || action==MEASURE_FEATURE_A || action==MEASURE_FEATURE_B || action==MEASURE_RULES) {
        int next=action==MEASURE_RULES ? 5 : action==MEASURE_OBJECT_A ? 1 : action==MEASURE_OBJECT_B ? 2 : action==MEASURE_FEATURE_A ? 3 : 4;
        UIPanel_MeasurementStopInput();ui->measurement.chooser=chooser==next?0:next;
    } else if(action==MEASURE_PICK_A || action==MEASURE_PICK_B) {
        int slot=action==MEASURE_PICK_A?0:1;bool cancel=ui->measurement.picking && ui->measurement.slot==slot;
        UIPanel_MeasurementStopInput();ui->measurement.slot=slot;ui->measurement.picking=!cancel;
    } else if(action>=MEASURE_AXIS_X && action<=MEASURE_AXIS_Z) {
        ui->measurement.projection_axis=action-MEASURE_AXIS_X;ui->measurement.use_rule_axis=false;
        if(ui->measurement.operation==3 && !selected())UIPanel_TravelAction(MEASURE_TRAVEL);
    }
    else if(action>=MEASURE_PLANE_XY && action<=MEASURE_PLANE_ZX) {ui->measurement.angle_plane=action-MEASURE_PLANE_XY;ui->measurement.use_rule_axis=false;if(ui->measurement.operation==4 && !selected())UIPanel_TravelAction(MEASURE_HINGE);}
    else if(action==MEASURE_DETAILS)ui->measurement.details_open=!ui->measurement.details_open;
    else if(action==MEASURE_OFFSETS)ui->measurement.offsets_open=!ui->measurement.offsets_open;
    else if(action==MEASURE_SLOT_A || action==MEASURE_SLOT_B)ui->measurement.slot=action==MEASURE_SLOT_A?0:1;
    else if(action>=MEASURE_OFFSET_U && action<=MEASURE_OFFSET_N) {
        int component=action-MEASURE_OFFSET_U;
        snprintf(ui->measurement.placement_text,64,"%.8g m",ui->measurement.refs[ui->measurement.slot].local_offset_meters[component]);
        UIPanel_MeasurementStartValue(7+component);
    } else if(action==MEASURE_RESET_OFFSET)memset(ui->measurement.refs[ui->measurement.slot].local_offset_meters,0,sizeof(ui->measurement.refs[0].local_offset_meters));
    else if(action>=MEASURE_DISTANCE && action<=MEASURE_ANGLE) {
        choose_tool(action-MEASURE_DISTANCE);
    } else if(action==MEASURE_VALUE)UIPanel_MeasurementStartValue(ui->measurement.placing>=7 && ui->measurement.placing<=9 ? ui->measurement.placing : ui->measurement.operation==2?5:3);
    else if(action==MEASURE_APPLY_OFFSET) {
        UIPanel_MeasurementApplyButton(ui->measurement.placing);
        if(!ui->measurement.placing)ui->measurement.placement_text[0]=0;
    }
    else if(action==MEASURE_CANCEL)UIPanel_MeasurementStopInput();
    else if(action==MEASURE_NEW) {
        UIPanel_MeasurementStopInput();ui->measurement.constraint_index=-1;ui->measurement.use_rule_axis=false;ui->measurement.placement_message[0]=0;
        ui->measurement.rules_open=false;ui->rightScroll[UI_PANEL_RIGHT_TAB_MEASURE].scrollOffsetPx=0;
        if(ui->measurement.operation==3 || ui->measurement.operation==4)UIPanel_TravelAction(ui->measurement.operation==4 ? MEASURE_HINGE : MEASURE_TRAVEL);
    }
    else if(action==MEASURE_REMOVE) {UIPanel_MeasurementStopInput();ui->measurement.placing=6;}
    else if(action==MEASURE_ONCE || action==MEASURE_SAVE) {
        int mode=ui->measurement.placing==6?6:ui->measurement.operation==2?5:ui->measurement.operation==1?(action==MEASURE_ONCE?2:4):(action==MEASURE_ONCE?1:3);
        UIPanel_MeasurementApplyButton(mode);
    }
    UIPanel_LayoutMeasurementPane();return true;
}
