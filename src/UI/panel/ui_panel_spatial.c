#include "UI/ui_panel_spatial.h"
#include "UI/ui_panel_parts_surface.h"
#include "Layout/layout_motion.h"
#include "UI/ui_panel_measurement.h"
#include "UI/ui_panel_motion.h"
#include "UI/ui_panel_shell.h"
#include "Core/space_mode_adapter.h"
#include <math.h>
#include "UI/ui_panel_right_scroll.h"
#include "Core/global_state.h"
#include "Editor/editor_numeric_edit.h"
#include <stdio.h>
#include <string.h>

static Layout* layout(void) {return &Global_Get()->layout;}
static void name(const char* id,char text[160]) {
    const LayoutEntityInfo* info=Layout_EntityInfo(&layout()->objectStore,id);
    snprintf(text,160,"%s",info && info->label[0]?info->label:id && id[0]?id:"Choose object or assembly");
}
static const Object3D* volume(void) {return Layout_ObjectStore_FindConst(&layout()->objectStore,UIPanel_Get()->spatial.volume.object_id);}
static void pick_volume(const Object3D* o) {
    UIPanelState* ui=UIPanel_Get();memset(&ui->spatial.volume,0,sizeof(ui->spatial.volume));
    ui->spatial.volume_observed=o!=NULL;ui->spatial.remove_pending=false;ui->parts.message[0]=0;
    double scale=Layout_WorldScale(layout());
    if(o) {
        ui->spatial.observed_volume=*o;ui->spatial.volume.object_id=o->objectId;ui->spatial.volume.role=o->info.volume_role;
        snprintf(ui->spatial.volume.name,96,"%s",o->info.label);snprintf(ui->spatial.volume.owner,64,"%s",o->info.volume_owner);
        const double size[]={o->rectPrism.width*scale,o->rectPrism.height*scale,o->rectPrism.depth*scale};
        const double center[]={o->transform.position.x*scale,o->transform.position.y*scale,o->transform.position.z*scale};
        for(int k=0;k<3;++k){snprintf(ui->spatial.size[k],64,"%.6g m",size[k]);snprintf(ui->spatial.center[k],64,"%.6g m",center[k]);}
        Global_Get()->editor.selectedObject3DId=o->objectId;
    } else {
        ui->spatial.volume.role=LAYOUT_VOLUME_KEEPOUT;
        for(int k=0;k<3;++k){snprintf(ui->spatial.size[k],64,"1 m");snprintf(ui->spatial.center[k],64,"0 m");}
    }
}
void UIPanel_SpatialEnterVolumes(void) {
    const Object3D* o=Layout_ObjectStore_FindConst(&layout()->objectStore,Global_Get()->editor.selectedObject3DId);
    if(o && o->info.volume_role)pick_volume(o);
    else if(!volume())pick_volume(NULL);
}
static void pick_rule(const LayoutSpatialRule* rule) {
    UIPanelState* ui=UIPanel_Get();ui->spatial.rule=ui->spatial.observed_rule=*rule;ui->spatial.rule_observed=true;ui->spatial.remove_pending=false;
    snprintf(ui->spatial.distance,64,"%.6g m",rule->clearance_meters);ui->parts.message[0]=0;
}
static uint64_t digest(void);
void UIPanel_SpatialRefresh(void) {
    UIPanelState* ui=UIPanel_Get();
    if(ui->spatial.preview_active && ui->spatial.checked_digest!=digest()) ui->spatial.preview_active=false;
    if(ui->parts.mode==4 && ui->spatial.volume_observed) {
        const Object3D* o=volume();
        if(!o || memcmp(o,&ui->spatial.observed_volume,sizeof(*o))) {
            pick_volume(o && o->info.volume_role?o:NULL);snprintf(ui->parts.message,160,"Volume refreshed from document.");
        }
    }
    if(ui->parts.mode==5 && ui->spatial.rule_observed) {
        const LayoutSpatialRule* r=Layout_FindSpatialRule(&layout()->objectStore,ui->spatial.rule.id);
        if(!r || memcmp(r,&ui->spatial.observed_rule,sizeof(*r))) {
            if(r)pick_rule(r);
            else {memset(&ui->spatial.rule,0,sizeof(ui->spatial.rule));ui->spatial.rule_observed=false;ui->spatial.remove_pending=false;}
            snprintf(ui->parts.message,160,"Checks refreshed from document.");
        }
    }
}
/* Hash only authored inputs. Filters/selections do not stale results; geometry,
 * metadata, ownership and undo do. Never interpret old results as current. */
static uint64_t digest(void) {
    const Layout* l=layout();uint64_t hash=UINT64_C(1469598103934665603);
    const void* blocks[]={&l->metersPerWorldUnit,&l->objectStore.count,l->objectStore.items,&l->objectStore.assembly_count,l->objectStore.assemblies,&l->objectStore.spatial_rule_count,l->objectStore.spatial_rules,&l->objectStore.constraintCount,l->objectStore.constraints,&l->objectStore.motion_envelope_count,l->objectStore.motion_envelopes};
    const size_t sizes[]={sizeof(l->metersPerWorldUnit),sizeof(l->objectStore.count),l->objectStore.count*sizeof(Object3D),sizeof(l->objectStore.assembly_count),sizeof(l->objectStore.assemblies),sizeof(l->objectStore.spatial_rule_count),sizeof(l->objectStore.spatial_rules),sizeof(l->objectStore.constraintCount),sizeof(l->objectStore.constraints),sizeof(l->objectStore.motion_envelope_count),sizeof(l->objectStore.motion_envelopes)};
    for(size_t i=0;i<sizeof(blocks)/sizeof(blocks[0]);++i){const unsigned char* p=blocks[i];for(size_t j=0;j<sizes[i];++j){hash^=p[j];hash*=UINT64_C(1099511628211);}}
    return hash;
}
void UIPanel_SpatialRunChecks(void) {
    UIPanelState* ui=UIPanel_Get();ui->spatial.total_count=Layout_CheckSpatial(layout(),ui->spatial.results,256);
    ui->spatial.result_count=ui->spatial.total_count>256?256:ui->spatial.total_count;
    ui->spatial.preview_active=false;ui->spatial.checked=true;ui->spatial.checked_digest=digest();ui->spatial.selected_result=-1;ui->parts.message[0]=0;
}
char* UIPanel_SpatialInputBuffer(size_t* capacity) {
    UIPanelState* ui=UIPanel_Get();int action=ui->parts.input;*capacity=64;
    if(action==PARTS_VOLUME_NAME){*capacity=96;return ui->spatial.volume.name;}
    if(action>=PARTS_VOLUME_W && action<=PARTS_VOLUME_D)return ui->spatial.size[action-PARTS_VOLUME_W];
    if(action>=PARTS_VOLUME_X && action<=PARTS_VOLUME_Z)return ui->spatial.center[action-PARTS_VOLUME_X];
    if(action==PARTS_CHECK_DISTANCE)return ui->spatial.distance;
    return NULL;
}
static void choices(PartsPane* p) {
    UIPanelState* ui=UIPanel_Get();LayoutObjectStore* store=&layout()->objectStore;
    row(p,PARTS_SPATIAL_CANCEL,"Close choices",true);
    int chooser=ui->parts.chooser;
    if(chooser==9){row(p,1000,"Keep-out",true);row(p,1001,"Service",true);return;}
    if(chooser==13){row(p,1000,"Must not intersect",true);row(p,1001,"Minimum clearance",true);return;}
    if(chooser==10)row(p,999,"No owner exemption",true);
    for(size_t i=0;i<store->count+store->assembly_count;++i) {
        const char* id;
        if(i<store->count){const Object3D* o=&store->items[i];if(o->isDeleted || (chooser==8 && !o->info.volume_role))continue;id=o->coreMeta.object_id;}
        else {if(chooser==8)continue;id=store->assemblies[i-store->count].id;}
        char text[256],label[160];name(id,label);snprintf(text,sizeof(text),"[%s] %.150s",id,label);row(p,1000+(int)i,text,true);
    }
}
static void volumes(PartsPane* p) {
    UIPanelState* ui=UIPanel_Get();char text[256],label[160];const Object3D* o=volume();
    name(o?o->coreMeta.object_id:"",label);snprintf(text,sizeof(text),"%s",o?label:"Choose saved volume");
    row(p,PARTS_VOLUME_SELECT,text,true);
    const LayoutMotionEnvelope* envelope=o?Layout_FindMotionEnvelope(&layout()->objectStore,o->objectId):NULL;
    if(envelope) {
        note(p,Layout_MotionEnvelopeCurrent(layout(),envelope)?"Motion envelope: current":"Motion envelope: STALE");
        name(o->info.volume_owner,label);snprintf(text,sizeof(text),"Moving part: %s",label);note(p,text);
        for(size_t i=0;i<layout()->objectStore.constraintCount;++i) {
            const LayoutConstraint* c=&layout()->objectStore.constraints[i];if(strcmp(c->id,envelope->rule_id))continue;
            snprintf(text,sizeof(text),"Range: %.4g to %.4g %s",c->travel_min,c->travel_max,c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL?"deg":"m");note(p,text);
        }
        snprintf(text,sizeof(text),"Size: %.3g x %.3g x %.3g m",o->rectPrism.width*Layout_WorldScale(layout()),o->rectPrism.height*Layout_WorldScale(layout()),o->rectPrism.depth*Layout_WorldScale(layout()));note(p,text);
        row(p,PARTS_VOLUME_POSITION,ui->spatial.position_open?"Details -":"Details +",true);
        if(ui->spatial.position_open) {
            snprintf(text,sizeof(text),"Movement: %s",envelope->rule_id);note(p,text);
            snprintf(text,sizeof(text),"%u poses; padding %.3g m",envelope->samples,envelope->padding_meters);note(p,text);
        }
        row(p,PARTS_ENVELOPE_REGENERATE,"Regenerate envelope",true);
        cell(p,PARTS_ENVELOPE_MOVEMENT,"Edit movement",NULL,0,2,true,false);
        cell(p,PARTS_ENVELOPE_CHECK,"Run checks",NULL,1,2,true,false);p->y+=p->h;
        cell(p,PARTS_VOLUME_NEW,"New volume",NULL,0,2,true,false);cell(p,PARTS_VOLUME_REMOVE,"Delete...",NULL,1,2,true,false);p->y+=p->h;
        if(ui->spatial.remove_pending){row(p,PARTS_VOLUME_CONFIRM_REMOVE,"Confirm delete envelope",true);row(p,PARTS_SPATIAL_CANCEL,"Cancel",true);}
        snprintf(text,sizeof(text),"Scope: %s",envelope->assembly_id[0]?"saved assembly":"B only");note(p,text);
        if(envelope->member_count){snprintf(text,sizeof(text),"%zu captured moving parts",envelope->member_count);note(p,text);}
        note(p,"Overview box; checks use member intervals.");return;
    }
    field(p,PARTS_VOLUME_NAME,"Name",ui->spatial.volume.name);
    row(p,PARTS_VOLUME_ROLE,ui->spatial.volume.role==LAYOUT_VOLUME_SERVICE?"Role: Service":"Role: Keep-out",true);
    field(p,PARTS_VOLUME_W,"Width U",ui->spatial.size[0]);field(p,PARTS_VOLUME_H,"Height V",ui->spatial.size[1]);field(p,PARTS_VOLUME_D,"Depth N",ui->spatial.size[2]);
    row(p,PARTS_VOLUME_POSITION,ui->spatial.position_open?"Position / owner −":"Position / owner +",true);
    if(ui->spatial.position_open) {
        field(p,PARTS_VOLUME_X,"World X",ui->spatial.center[0]);field(p,PARTS_VOLUME_Y,"World Y",ui->spatial.center[1]);field(p,PARTS_VOLUME_Z,"World Z",ui->spatial.center[2]);
        name(ui->spatial.volume.owner,label);snprintf(text,sizeof(text),"Owner: %s",ui->spatial.volume.owner[0]?label:"None");row(p,PARTS_VOLUME_OWNER,text,true);
        note(p,"Owner and its assembly members are exempt. This does not attach or parent the volume.");
    }
    row(p,PARTS_VOLUME_SAVE,o?"Update volume":"Create volume",true);
    cell(p,PARTS_VOLUME_NEW,"New volume",NULL,0,2,true,false);cell(p,PARTS_VOLUME_REMOVE,"Delete...",NULL,1,2,o!=NULL,false);p->y+=p->h;
    if(ui->spatial.remove_pending){row(p,PARTS_VOLUME_CONFIRM_REMOVE,"Confirm delete volume",true);row(p,PARTS_SPATIAL_CANCEL,"Cancel",true);}
    note(p,"Colored wire boxes reserve space. Dimensions accept mm, cm, m, in or ft.");
}
static void checks(PartsPane* p) {
    UIPanelState* ui=UIPanel_Get();char text[256],label[160];
    row(p,PARTS_CHECK_RUN,"Run checks",true);
    if(!ui->spatial.checked)note(p,"Checks inspect reserved spaces and saved rules. They do not move parts.");
    else if(ui->spatial.checked_digest!=digest())note(p,"Results are stale. Run checks again.");
    else if(!ui->spatial.total_count)note(p,"No reserved-space obstructions or saved check results.");
    else {
        size_t errors=0,warnings=0,passes=0;
        for(size_t i=0;i<ui->spatial.result_count;++i){LayoutSpatialSeverity s=ui->spatial.results[i].severity;if(s==LAYOUT_SPATIAL_ERROR)++errors;else if(s==LAYOUT_SPATIAL_WARNING)++warnings;else ++passes;}
        snprintf(text,sizeof(text),"%zu errors · %zu warnings",errors,warnings);note(p,text);
        if(passes){snprintf(text,sizeof(text),"%zu checks passed",passes);note(p,text);}
        if(ui->spatial.total_count>256){snprintf(text,sizeof(text),"Showing first 256 of %zu results. Refine the scene/rules for complete UI review.",ui->spatial.total_count);note(p,text);}
    }
    row(p,PARTS_CHECK_RULES,ui->spatial.rules_open?"Edit rules −":"Edit rules +",true);
    if(ui->spatial.rules_open) {
        name(ui->spatial.rule.source,label);snprintf(text,sizeof(text),"A: %s",label);row(p,PARTS_CHECK_SOURCE,text,true);
        row(p,PARTS_CHECK_KIND,ui->spatial.rule.kind==LAYOUT_SPATIAL_CLEARANCE?"Minimum clearance":"Must not intersect",true);
        name(ui->spatial.rule.target,label);snprintf(text,sizeof(text),"B: %s",label);row(p,PARTS_CHECK_TARGET,text,true);
        if(ui->spatial.rule.kind==LAYOUT_SPATIAL_CLEARANCE)field(p,PARTS_CHECK_DISTANCE,"Minimum",ui->spatial.distance);
        row(p,PARTS_CHECK_SAVE,ui->spatial.rule.id[0]?"Update check":"Create check",ui->spatial.rule.source[0] && ui->spatial.rule.target[0]);
        cell(p,PARTS_CHECK_NEW,"New check",NULL,0,2,true,false);cell(p,PARTS_CHECK_REMOVE,"Remove...",NULL,1,2,ui->spatial.rule.id[0]!=0,false);p->y+=p->h;
        if(ui->spatial.remove_pending){row(p,PARTS_CHECK_CONFIRM_REMOVE,"Confirm remove check",true);row(p,PARTS_SPATIAL_CANCEL,"Cancel",true);}
        for(size_t i=0;i<layout()->objectStore.spatial_rule_count;++i){const LayoutSpatialRule* r=&layout()->objectStore.spatial_rules[i];snprintf(text,sizeof(text),"%s: %.60s → %.60s",r->id,r->source,r->target);row(p,8000+(int)i,text,true);}
    }
    if(ui->spatial.checked_digest!=digest())return;
    if(ui->spatial.selected_result>=0 && (size_t)ui->spatial.selected_result<ui->spatial.result_count) {
        const LayoutSpatialResult* r=&ui->spatial.results[ui->spatial.selected_result];
        if(!ui->spatial.preview_active) {
            note(p,r->message);
            name(r->source,label);snprintf(text,sizeof(text),"A: %s [%s]",label,r->source);note(p,text);name(r->target,label);snprintf(text,sizeof(text),"B: %s [%s]",label,r->target);note(p,text);
            if(r->measurable){snprintf(text,sizeof(text),"%s gap: %.6g m",r->motion_intervals?"Interval bounds":r->approximate?"Bounds":"Surface",r->distance_meters);note(p,text);
                if(r->required_meters>0){snprintf(text,sizeof(text),"Minimum: %.6g m",r->required_meters);note(p,text);}}
            cell(p,PARTS_CHECK_SELECT_A,"Select A",NULL,0,2,true,false);cell(p,PARTS_CHECK_SELECT_B,"Select B",NULL,1,2,true,false);p->y+=p->h;
        }
        UIPanel_MotionInspectionBuild(p,r);
    }
    for(size_t i=0;i<ui->spatial.result_count;++i){const LayoutSpatialResult* r=&ui->spatial.results[i];char other[160];name(r->source,label);name(r->target,other);snprintf(text,sizeof(text),"%s: %.50s / %.50s",(const char*[]){"Pass","Error","Warning"}[r->severity],label,other);row(p,9000+(int)i,text,true);}
}
void UIPanel_SpatialBuild(PartsPane* p) {
    if(UIPanel_Get()->parts.chooser>=8)choices(p);
    else {if(UIPanel_Get()->parts.mode==4)volumes(p);else checks(p);if(UIPanel_Get()->parts.message[0])note(p,UIPanel_Get()->parts.message);}
}
static bool save_volume(bool remove) {
    UIPanelState* ui=UIPanel_Get();LayoutVolumeEdit v=ui->spatial.volume;
    if(!remove)for(int k=0;k<3;++k) {
        if(!Editor_ParseLength(ui->spatial.size[k],UIPanel_GetDisplayUnit(),&v.size_meters[k]) || !Editor_ParseLength(ui->spatial.center[k],UIPanel_GetDisplayUnit(),&v.center_meters[k])){snprintf(ui->parts.message,160,"Enter physical lengths such as 300 mm.");return false;}
        /* Keep the exact authored float when a rounded display field was untouched. */
        const Object3D* saved=volume();
        if(saved && ui->spatial.volume_observed) {
            double scale=Layout_WorldScale(layout());
            double sizes[]={saved->rectPrism.width*scale,saved->rectPrism.height*scale,saved->rectPrism.depth*scale};
            double centers[]={saved->transform.position.x*scale,saved->transform.position.y*scale,saved->transform.position.z*scale};
            char shown[64];snprintf(shown,64,"%.6g m",sizes[k]);if(!strcmp(shown,ui->spatial.size[k]))v.size_meters[k]=sizes[k];
            snprintf(shown,64,"%.6g m",centers[k]);if(!strcmp(shown,ui->spatial.center[k]))v.center_meters[k]=centers[k];
        }
    }
    bool creating=!v.object_id,ok=Layout_EditVolume(layout(),&v,remove,Layout_GeometryHistory,NULL);
    if(ok)pick_volume(remove?NULL:creating?&layout()->objectStore.items[layout()->objectStore.count-1]:Layout_ObjectStore_FindConst(&layout()->objectStore,v.object_id));
    snprintf(ui->parts.message,160,"%s",ok?remove?"Volume deleted.":"Volume saved.":layout()->geometryMessage);return ok;
}
static bool save_rule(void) {
    UIPanelState* ui=UIPanel_Get();LayoutSpatialRule r=ui->spatial.rule;
    if(r.kind==LAYOUT_SPATIAL_CLEARANCE && !Editor_ParseLength(ui->spatial.distance,UIPanel_GetDisplayUnit(),&r.clearance_meters)){snprintf(ui->parts.message,160,"Enter a clearance such as 50 mm.");return false;}
    if(r.kind==LAYOUT_SPATIAL_NO_INTERSECTION)r.clearance_meters=0;
    bool creating=!r.id[0],ok=Layout_EditSpatialRule(layout(),&r,NULL,Layout_GeometryHistory,NULL);
    if(ok)pick_rule(creating?&layout()->objectStore.spatial_rules[layout()->objectStore.spatial_rule_count-1]:Layout_FindSpatialRule(&layout()->objectStore,r.id));
    snprintf(ui->parts.message,160,"%s",ok?"Check saved. Run checks to see current results.":layout()->geometryMessage);return ok;
}
bool UIPanel_SpatialClick(int action,int chooser) {
    UIPanelState* ui=UIPanel_Get();LayoutObjectStore* s=&layout()->objectStore;
    if(chooser>=8 && action>=999) {
        int index=action-1000;
        if(chooser==9 && index>=0 && index<2)ui->spatial.volume.role=(LayoutVolumeRole)(index+1);
        else if(chooser==13 && index>=0 && index<2)ui->spatial.rule.kind=(LayoutSpatialRuleKind)index;
        else if(chooser==10 && action==999)ui->spatial.volume.owner[0]=0;
        else {
            const Object3D* o=index>=0 && (size_t)index<s->count?&s->items[index]:NULL;
            const char* id=o && !o->isDeleted?o->coreMeta.object_id:index>=0 && (size_t)index>=s->count && (size_t)index-s->count<s->assembly_count?s->assemblies[(size_t)index-s->count].id:NULL;
            if(id){if(chooser==8 && o && o->info.volume_role)pick_volume(o);else if(chooser==10)snprintf(ui->spatial.volume.owner,64,"%s",id);else if(chooser==11 || chooser==12)snprintf(chooser==11?ui->spatial.rule.source:ui->spatial.rule.target,64,"%s",id);}
        }
        ui->spatial.remove_pending=false;return true;
    }
    if(action>=PARTS_MOTION_INSPECT && action<=PARTS_MOTION_RANGE) {
        const LayoutSpatialResult* r=ui->spatial.selected_result>=0 && (size_t)ui->spatial.selected_result<ui->spatial.result_count && ui->spatial.checked_digest==digest()?&ui->spatial.results[ui->spatial.selected_result]:NULL;
        (void)UIPanel_MotionInspectionClick(action,r);
    } else if(action==PARTS_ENVELOPE_REGENERATE) {
        const LayoutMotionEnvelope* envelope=Layout_FindMotionEnvelope(s,ui->spatial.volume.object_id);
        char id[64];snprintf(id,64,"%s",envelope?envelope->rule_id:"");
        bool ok=envelope && Layout_GenerateMotionEnvelope(layout(),id,envelope->samples,Layout_GeometryHistory,NULL);
        if(ok)pick_volume(volume());
        snprintf(ui->parts.message,160,"%s",ok?"Envelope regenerated. Run checks for current results.":layout()->geometryMessage);
    } else if(action==PARTS_ENVELOPE_MOVEMENT) {
        const LayoutMotionEnvelope* envelope=Layout_FindMotionEnvelope(s,ui->spatial.volume.object_id);
        if(envelope) for(size_t i=0;i<s->constraintCount;++i) if(!strcmp(s->constraints[i].id,envelope->rule_id)) {
            const LayoutConstraint* c=&s->constraints[i];
            for(size_t j=0;j<s->count;++j)if(!s->items[j].isDeleted && !strcmp(s->items[j].coreMeta.object_id,c->b.entity_id))Global_Get()->editor.selectedObject3DId=s->items[j].objectId;
            UIPanel_SetActiveRightTab(ui,UI_PANEL_RIGHT_TAB_MEASURE);UIPanel_MeasurementSelectRule((int)i);
            ui->rightScroll[UI_PANEL_RIGHT_TAB_MEASURE].scrollOffsetPx=0;break;
        }
    } else if(action==PARTS_ENVELOPE_CHECK) {ui->parts.mode=5;UIPanel_SpatialRunChecks();ui->rightScroll[UI_PANEL_RIGHT_TAB_PARTS].scrollOffsetPx=0;}
    else if(action==PARTS_VOLUME_SELECT)ui->parts.chooser=8;
    else if(action==PARTS_VOLUME_ROLE)ui->parts.chooser=9;
    else if(action==PARTS_VOLUME_OWNER)ui->parts.chooser=10;
    else if(action==PARTS_CHECK_SOURCE)ui->parts.chooser=11;
    else if(action==PARTS_CHECK_TARGET)ui->parts.chooser=12;
    else if(action==PARTS_CHECK_KIND)ui->parts.chooser=13;
    else if(action==PARTS_VOLUME_POSITION)ui->spatial.position_open=!ui->spatial.position_open;
    else if(action==PARTS_VOLUME_NEW)pick_volume(NULL);
    else if(action==PARTS_VOLUME_SAVE)(void)save_volume(false);
    else if(action==PARTS_VOLUME_CONFIRM_REMOVE)(void)save_volume(true);
    else if(action==PARTS_VOLUME_REMOVE || action==PARTS_CHECK_REMOVE)ui->spatial.remove_pending=true;
    else if(action==PARTS_SPATIAL_CANCEL)ui->spatial.remove_pending=false;
    else if(action==PARTS_CHECK_RUN)UIPanel_SpatialRunChecks();
    else if(action==PARTS_CHECK_RULES)ui->spatial.rules_open=!ui->spatial.rules_open;
    else if(action==PARTS_CHECK_SAVE)(void)save_rule();
    else if(action==PARTS_CHECK_NEW){memset(&ui->spatial.rule,0,sizeof(ui->spatial.rule));ui->spatial.rule_observed=false;ui->spatial.remove_pending=false;snprintf(ui->spatial.distance,64,"50 mm");}
    else if(action==PARTS_CHECK_CONFIRM_REMOVE){bool ok=Layout_EditSpatialRule(layout(),NULL,ui->spatial.rule.id,Layout_GeometryHistory,NULL);if(ok){memset(&ui->spatial.rule,0,sizeof(ui->spatial.rule));ui->spatial.rule_observed=false;ui->spatial.remove_pending=false;}snprintf(ui->parts.message,160,"%s",ok?"Check removed.":layout()->geometryMessage);}
    else if(action==PARTS_CHECK_SELECT_A || action==PARTS_CHECK_SELECT_B) {
        if(ui->spatial.selected_result>=0 && (size_t)ui->spatial.selected_result<ui->spatial.result_count) {
            const LayoutSpatialResult* r=&ui->spatial.results[ui->spatial.selected_result];const char* id=action==PARTS_CHECK_SELECT_A?r->source:r->target;
            Global_Get()->editor.selectedObject3DId=0;
            for(size_t i=0;i<s->count;++i)if(!s->items[i].isDeleted && !strcmp(s->items[i].coreMeta.object_id,id))Global_Get()->editor.selectedObject3DId=s->items[i].objectId;
        }
    } else if(action>=9000 && (size_t)(action-9000)<ui->spatial.result_count){ui->spatial.preview_active=false;ui->spatial.selected_result=action-9000;}
    else if(action>=8000 && (size_t)(action-8000)<s->spatial_rule_count)pick_rule(&s->spatial_rules[action-8000]);
    if(ui->parts.chooser)ui->rightScroll[UI_PANEL_RIGHT_TAB_PARTS].scrollOffsetPx=0;
    Global_FlagHitboxesDirty();return true;
}

/* Read-only annotations follow the same visibility/filter and clipping policy
 * as existing constraint markers. These are bounds labels, not collision proof. */
void UIPanel_RenderMotionViewport(SDL_Renderer* renderer) {
    GlobalState* state=Global_Get();CorePaneRect rect;TTF_Font* font=FontManager_Get(FONT_DEFAULT);
    if (!renderer || !font || !LineDrawingPaneHost_GetViewportRect(&state->paneHost,&rect)) return;
    SDL_Rect old;bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&old);
    SDL_Rect clip={(int)rect.x,(int)rect.y,(int)rect.width,(int)rect.height};SDL_RenderSetClipRect(renderer,&clip);
    int line_height=TTF_FontHeight(font)+8;
    int visible=0;
    for (size_t i=0;i<state->layout.objectStore.motion_envelope_count;++i) {
        const LayoutMotionEnvelope* envelope=&state->layout.objectStore.motion_envelopes[i];
        const Object3D* o=Layout_ObjectStore_FindConst(&state->layout.objectStore,envelope->object_id);
        if (o && Layout_ObjectShown(&state->layout.objectStore,o)) ++visible;
    }
    if (!visible) {SDL_RenderSetClipRect(renderer,clipped?&old:NULL);return;}
    int capacity=(clip.h/3)/line_height;
    if(capacity<1)capacity=1;
    int shown=visible<capacity?visible:capacity;
    int width=clip.w-24;if(width>560)width=560;
    int y=clip.y+clip.h-12-shown*line_height;
    int row_index=0;
    for (size_t i=0;i<state->layout.objectStore.motion_envelope_count && row_index<shown;++i) {
        const LayoutMotionEnvelope* envelope=&state->layout.objectStore.motion_envelopes[i];
        const Object3D* o=Layout_ObjectStore_FindConst(&state->layout.objectStore,envelope->object_id);
        if (!o || !Layout_ObjectShown(&state->layout.objectStore,o)) continue;
        bool current=Layout_MotionEnvelopeCurrent(&state->layout,envelope);
        char text[160];
        snprintf(text,sizeof(text),"Motion %zu: %s | %s",i+1,envelope->rule_id,
            current?"overview bounds":"STALE - regenerate");
        SDL_Rect banner={clip.x+12,y+row_index*line_height,width,line_height};
        SDL_SetRenderDrawColor(renderer,20,24,30,235);SDL_RenderFillRect(renderer,&banner);
        UIPanelSummary_DrawTextClipped(renderer,font,text,banner.x+6,banner.y+3,
            banner.w-12,line_height-6,current?(SDL_Color){185,150,255,255}:(SDL_Color){255,165,90,255});
        ++row_index;
    }
    SDL_RenderSetClipRect(renderer,clipped?&old:NULL);
}
