#include "UI/ui_panel_measurement.h"
#include "Core/global_state.h"
#include "Layout/layout_constraints.h"
#include "Editor/editor_reference_edit.h"
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool moving_rule(const LayoutConstraint* c) {
    return c && (c->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL || c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL);
}
bool UIPanel_TravelParse(const char* text,bool angular,double* value) {
    if(!angular)return Editor_ParseLength(text,UIPanel_GetDisplayUnit(),value);
    char* end;double v=strtod(text,&end);
    if(end==text || !isfinite(v))return false;
    while(isspace((unsigned char)*end))++end;
    if(!strncmp(end,"deg",3))end+=3;
    while(isspace((unsigned char)*end))++end;
    if(*end)return false;
    *value=v;return true;
}
static void format_value(char* text,double value,bool angular) {
    if(!angular)(void)core_units_convert(value,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&value);
    snprintf(text,64,"%.9g %s",value,angular ? "deg" : UIPanel_GetDisplayUnitSymbol());
}
static const LayoutConstraint* selected(void) {
    const LayoutObjectStore* s=&Global_Get()->layout.objectStore;
    int i=UIPanel_Get()->measurement.constraint_index;
    return i>=0 && (size_t)i<s->constraintCount ? &s->constraints[i] : NULL;
}
void UIPanel_TravelSelect(const LayoutConstraint* c) {
    if(!moving_rule(c))return;
    UIPanelState* ui=UIPanel_Get();
    ui->measurement.observed_rule=*c;ui->measurement.observed_rule_valid=true;
    const double values[]={c->travel_min,c->travel_max,c->target};
    for(int i=0;i<3;++i) {
        format_value(ui->measurement.travel_text[i],values[i],c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL);
    }
}
void UIPanel_TravelStageInput(void) {
    UIPanelState* ui=UIPanel_Get();int field=ui->measurement.placing-10;
    if(field>=0 && field<3)snprintf(ui->measurement.travel_text[field],64,"%s",ui->measurement.placement_text);
}
void UIPanel_TravelEndDrag(void) {
    UIPanelState* ui=UIPanel_Get();
    if(ui->measurement.travel_dragging)Layout_EndGeometryGesture(&Global_Get()->layout);
    ui->measurement.travel_dragging=false;
    ui->measurement.travel_drag_id[0]=0;
}
static bool position(const char* id,double meters) {
    UIPanelState* ui=UIPanel_Get();Layout* layout=&Global_Get()->layout;
    bool ok=Layout_SetTravelPosition(layout,id,meters,Layout_GeometryHistory,NULL);
    snprintf(ui->measurement.placement_message,128,"%s",ok ? "Position updated." : layout->geometryMessage);
    if(ok)UIPanel_TravelSelect(selected());
    return ok;
}
bool UIPanel_TravelEvent(const SDL_Event* e) {
    UIPanelState* ui=UIPanel_Get();
    if(!ui->measurement.travel_dragging)return false;
    if(e->type==SDL_MOUSEBUTTONUP && e->button.button==SDL_BUTTON_LEFT) {UIPanel_TravelEndDrag();return true;}
    if(e->type==SDL_WINDOWEVENT && e->window.event==SDL_WINDOWEVENT_FOCUS_LOST) {UIPanel_TravelEndDrag();return false;}
    if(e->type==SDL_KEYDOWN) {UIPanel_TravelEndDrag();return false;}
    if(e->type!=SDL_MOUSEMOTION)return false;
    const LayoutConstraint* c=selected();
    if(!moving_rule(c) || strcmp(c->id,ui->measurement.travel_drag_id)) {UIPanel_TravelEndDrag();return true;}
    SDL_Rect r=ui->measurement.travel_track;
    double t=fmax(0,fmin(1,(double)(e->motion.x-r.x-6)/(r.w-12)));
    (void)position(c->id,c->travel_min+(c->travel_max-c->travel_min)*t);
    return true;
}
void UIPanel_TravelAction(int action) {
    UIPanelState* ui=UIPanel_Get();
    UIPanel_MeasurementStopInput();
    const LayoutConstraint* saved=selected();
    bool angular=ui->measurement.operation==4;
    if(action==MEASURE_TRAVEL || action==MEASURE_HINGE) {
        angular=action==MEASURE_HINGE;ui->measurement.operation=angular ? 4 : 3;
        if(moving_rule(saved))UIPanel_TravelSelect(saved);
        else {
            const Vec3 axes[]={{1,0,0},{0,1,0},{0,0,1}};
            LayoutMeasurementResult m=Layout_Measure(&Global_Get()->layout,&ui->measurement.refs[0],&ui->measurement.refs[1],angular ? LAYOUT_MEASURE_PLANAR_ANGLE : LAYOUT_MEASURE_PROJECTED_DISTANCE,axes[angular ? (ui->measurement.angle_plane+2)%3 : ui->measurement.projection_axis]);
            double current=m.status==LAYOUT_MEASUREMENT_OK ? m.value : 0;
            /* Match the solver tolerance without exposing float-storage noise. */
            if(fabs(current)<1e9)current=round(current*1e6)/1e6;
            double values[]={angular ? fmin(0,current) : current,angular ? fmax(110,current) : current+1,current};
            for(int i=0;i<3;++i) {
                format_value(ui->measurement.travel_text[i],values[i],angular);
            }
            ui->measurement.placement_message[0]=0;
        }
        return;
    }
    if(action>=MEASURE_TRAVEL_MIN && action<=MEASURE_TRAVEL_POSITION) {
        int i=action-MEASURE_TRAVEL_MIN;
        snprintf(ui->measurement.placement_text,64,"%s",ui->measurement.travel_text[i]);
        UIPanel_MeasurementStartValue(10+i);return;
    }
    if(action>=MEASURE_TRAVEL_TO_MIN && action<=MEASURE_TRAVEL_RESET) {
        if(moving_rule(saved)) {
            if(action==MEASURE_TRAVEL_RESET) {ui->measurement.refs[0]=saved->a;ui->measurement.refs[1]=saved->b;ui->measurement.use_rule_axis=true;}
            (void)position(saved->id,action==MEASURE_TRAVEL_TO_MIN ? saved->travel_min : action==MEASURE_TRAVEL_TO_MAX ? saved->travel_max : saved->travel_home);
        }
        return;
    }
    if(action!=MEASURE_TRAVEL_SAVE)return;
    double values[3];
    for(int i=0;i<3;++i)if(!UIPanel_TravelParse(ui->measurement.travel_text[i],angular,&values[i])) {
        snprintf(ui->measurement.placement_message,128,angular ? "Enter Min, Max and Position in degrees, e.g. 110 deg." : "Enter finite Min, Max and Position lengths, e.g. 900 mm.");return;
    }
    if(angular && (values[0]<=-180 || values[1]>180)) {snprintf(ui->measurement.placement_message,128,"Hinge limits must be greater than -180 and at most 180 deg.");return;}
    if(values[0]>values[1]) {snprintf(ui->measurement.placement_message,128,"Min must be less than or equal to Max.");return;}
    if(fabs(values[2]-values[0])<=1e-6)values[2]=values[0];
    if(fabs(values[2]-values[1])<=1e-6)values[2]=values[1];
    if(values[2]<values[0] || values[2]>values[1]) {snprintf(ui->measurement.placement_message,128,"Position must be between Min and Max.");return;}
    const Vec3 axes[]={{1,0,0},{0,1,0},{0,0,1}};
    LayoutConstraint c={.a=ui->measurement.refs[0],.b=ui->measurement.refs[1],.axis=axes[angular ? (ui->measurement.angle_plane+2)%3 : ui->measurement.projection_axis]};
    if(saved) {
        if(saved->kind!=(angular ? LAYOUT_CONSTRAINT_ANGULAR_TRAVEL : LAYOUT_CONSTRAINT_LINEAR_TRAVEL) || memcmp(&saved->a,&c.a,sizeof(c.a)) || memcmp(&saved->b,&c.b,sizeof(c.b)) ||
           (!ui->measurement.use_rule_axis && (saved->axis.x!=c.axis.x || saved->axis.y!=c.axis.y || saved->axis.z!=c.axis.z))) {
            snprintf(ui->measurement.placement_message,128,"Remove movement before changing its references, pivot or plane/axis.");return;
        }
        c=*saved;c.travel_min=values[0];c.travel_max=values[1];
    } else if(!(angular ? Layout_InitAngularTravel(&Global_Get()->layout,&c,values[0],values[1]) : Layout_InitLinearTravel(&Global_Get()->layout,&c,values[0],values[1]))) {
        snprintf(ui->measurement.placement_message,128,angular ? "Choose coplanar axes/faces and limits containing the current angle (Reset)." : "Choose two valid references and limits containing the current pose (the reset position).");return;
    }
    c.target=values[2];
    int index=ui->measurement.constraint_index;
    if(Layout_ConstraintEdit(&Global_Get()->layout,&c,NULL,Layout_GeometryHistory,NULL)) {
        if(index<0)index=(int)Global_Get()->layout.objectStore.constraintCount-1;
        UIPanel_MeasurementSelectRule(index);
        snprintf(ui->measurement.placement_message,128,angular ? "Hinge applied. Movement controls enabled." : "Travel applied. Movement controls enabled.");
    } else snprintf(ui->measurement.placement_message,128,"%s",Global_Get()->layout.geometryMessage);
}
