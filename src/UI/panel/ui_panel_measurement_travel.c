#include "UI/ui_panel_measurement.h"
#include "Core/global_state.h"
#include "Layout/layout_constraints.h"
#include "Editor/editor_reference_edit.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const LayoutConstraint* selected(void) {
    const LayoutObjectStore* s=&Global_Get()->layout.objectStore;
    int i=UIPanel_Get()->measurement.constraint_index;
    return i>=0 && (size_t)i<s->constraintCount ? &s->constraints[i] : NULL;
}
void UIPanel_TravelSelect(const LayoutConstraint* c) {
    if(!c || c->kind!=LAYOUT_CONSTRAINT_LINEAR_TRAVEL)return;
    UIPanelState* ui=UIPanel_Get();
    const double values[]={c->travel_min,c->travel_max,c->target};
    for(int i=0;i<3;++i) {
        double value=values[i];(void)core_units_convert(value,CORE_UNIT_METER,UIPanel_GetDisplayUnit(),&value);
        snprintf(ui->measurement.travel_text[i],64,"%.9g %s",value,UIPanel_GetDisplayUnitSymbol());
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
    snprintf(ui->measurement.placement_message,128,"%s",ok ? "Travel applied; attached geometry validated." : layout->geometryMessage);
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
    if(!c || c->kind!=LAYOUT_CONSTRAINT_LINEAR_TRAVEL || strcmp(c->id,ui->measurement.travel_drag_id)) {UIPanel_TravelEndDrag();return true;}
    SDL_Rect r=ui->measurement.travel_track;
    double t=fmax(0,fmin(1,(double)(e->motion.x-r.x-6)/(r.w-12)));
    (void)position(c->id,c->travel_min+(c->travel_max-c->travel_min)*t);
    return true;
}
void UIPanel_TravelAction(int action) {
    UIPanelState* ui=UIPanel_Get();
    UIPanel_MeasurementStopInput();
    const LayoutConstraint* saved=selected();
    if(action==MEASURE_TRAVEL) {
        ui->measurement.operation=3;
        if(saved && saved->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL)UIPanel_TravelSelect(saved);
        else {
            const Vec3 axes[]={{1,0,0},{0,1,0},{0,0,1}};
            LayoutMeasurementResult m=Layout_Measure(&Global_Get()->layout,&ui->measurement.refs[0],&ui->measurement.refs[1],LAYOUT_MEASURE_PROJECTED_DISTANCE,axes[ui->measurement.projection_axis]);
            double current=m.status==LAYOUT_MEASUREMENT_OK ? m.value : 0;
            snprintf(ui->measurement.travel_text[0],64,"%.9g m",current);
            snprintf(ui->measurement.travel_text[1],64,"%.9g m",current+1);
            snprintf(ui->measurement.travel_text[2],64,"%.9g m",current);
            snprintf(ui->measurement.placement_message,128,"Suggested limits span 1 m from the current pose. Edit before saving.");
        }
        return;
    }
    if(action>=MEASURE_TRAVEL_MIN && action<=MEASURE_TRAVEL_POSITION) {
        int i=action-MEASURE_TRAVEL_MIN;
        snprintf(ui->measurement.placement_text,64,"%s",ui->measurement.travel_text[i]);
        UIPanel_MeasurementStartValue(10+i);return;
    }
    if(action>=MEASURE_TRAVEL_TO_MIN && action<=MEASURE_TRAVEL_RESET) {
        if(saved && saved->kind==LAYOUT_CONSTRAINT_LINEAR_TRAVEL)
            (void)position(saved->id,action==MEASURE_TRAVEL_TO_MIN ? saved->travel_min : action==MEASURE_TRAVEL_TO_MAX ? saved->travel_max : saved->travel_home);
        return;
    }
    if(action!=MEASURE_TRAVEL_SAVE)return;
    double values[3];
    for(int i=0;i<3;++i)if(!Editor_ParseLength(ui->measurement.travel_text[i],UIPanel_GetDisplayUnit(),&values[i])) {
        snprintf(ui->measurement.placement_message,128,"Enter finite Min, Max and Position lengths, e.g. 900 mm.");return;
    }
    const Vec3 axes[]={{1,0,0},{0,1,0},{0,0,1}};
    LayoutConstraint c={.a=ui->measurement.refs[0],.b=ui->measurement.refs[1],.axis=axes[ui->measurement.projection_axis]};
    if(saved) {
        if(saved->kind!=LAYOUT_CONSTRAINT_LINEAR_TRAVEL || memcmp(&saved->a,&c.a,sizeof(c.a)) || memcmp(&saved->b,&c.b,sizeof(c.b)) ||
           (!ui->measurement.use_rule_axis && (saved->axis.x!=c.axis.x || saved->axis.y!=c.axis.y || saved->axis.z!=c.axis.z))) {
            snprintf(ui->measurement.placement_message,128,"Remove the existing rule before changing its type, references or rail axis.");return;
        }
        c=*saved;c.travel_min=values[0];c.travel_max=values[1];
    } else if(!Layout_InitLinearTravel(&Global_Get()->layout,&c,values[0],values[1])) {
        snprintf(ui->measurement.placement_message,128,"Choose two valid references and limits containing the current pose (the reset position).");return;
    }
    c.target=values[2];
    int index=ui->measurement.constraint_index;
    if(Layout_ConstraintEdit(&Global_Get()->layout,&c,NULL,Layout_GeometryHistory,NULL)) {
        if(index<0)index=(int)Global_Get()->layout.objectStore.constraintCount-1;
        UIPanel_MeasurementSelectRule(index);
        snprintf(ui->measurement.placement_message,128,"Travel saved. Drag the slider, or use Min, Max and Reset.");
    } else snprintf(ui->measurement.placement_message,128,"%s",Global_Get()->layout.geometryMessage);
}
