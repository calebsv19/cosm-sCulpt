#include "Core/line_drawing_engineering_proof.h"
#include "Core/global_state.h"
#include "Layout/layout_engineering.h"
#include "Layout/layout_relationships.h"
#include "UI/ui_panel_parts.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_spatial.h"
#include "UI/ui_panel_motion.h"
#include "UI/ui_panel_measurement.h"
#include "Layout/layout_motion.h"
#include <stdio.h>
#include <string.h>


static bool motion_proof(const char* mode) {
    GlobalState* state=Global_Get();Layout* l=&state->layout;bool travel=strstr(mode,"travel")!=NULL;
    uint32_t ids[]={l->objectStore.items[0].objectId,l->objectStore.items[1].objectId};
    for (int i=0;i<2;++i) {
        if (!Layout_SetRectPrismDimensions(l,ids[i],i?(travel?.4f:2):.2f,i?(travel?.8f:.08f):.2f,.2f,NULL) ||
            !Layout_SetObject3DPosition(l,ids[i],(Vec3){i?1:0,0,0},NULL)) return false;
        Object3D* o=Layout_ObjectStore_Find(&l->objectStore,ids[i]);
        snprintf(o->info.label,96,"%s",i?travel?"Lift bed":"Cabinet door":"Fixed mounting rail");
    }
    char group[64]={0};
    if (strstr(mode,"group")) {
        LayoutAssembly assembly={.frame={.origin={1,0,0},.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
        snprintf(assembly.info.label,96,"%s",travel?"Bed assembly":"Door assembly");snprintf(assembly.info.entity_type,64,"Assembly");
        if (!Layout_EditAssembly(l,&assembly,NULL,NULL,NULL)) return false;
        snprintf(group,64,"%s",l->objectStore.assemblies[0].id);
        LayoutEntityInfo info=l->objectStore.items[1].info;snprintf(info.parent_id,64,"%s",group);
        if (!Layout_SetEntityInfo(l,l->objectStore.items[1].coreMeta.object_id,&info,NULL,NULL)) return false;
        RectPrismPrimitiveCreateParams peer={.width=.15f,.height=.15f,.depth=.15f,.useExplicitFrame=true,
            .explicitFrame={.origin={1,travel?.8f:-.15f,0},.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
        uint32_t handle;if (!Layout_CreateRectPrismPrimitive(l,&peer,&handle,NULL)) return false;
        Object3D* part=Layout_ObjectStore_Find(&l->objectStore,handle);info=part->info;
        snprintf(info.label,96,"%s",travel?"Bed side rail":"Door handle");snprintf(info.parent_id,64,"%s",group);
        if (!Layout_SetEntityInfo(l,part->coreMeta.object_id,&info,NULL,NULL)) return false;
    }
    LayoutConstraint c={.axis={0,0,1}};snprintf(c.id,64,"door_hinge");snprintf(c.motion_assembly,64,"%s",group);
    snprintf(c.a.entity_id,64,"%s",l->objectStore.items[0].coreMeta.object_id);snprintf(c.b.entity_id,64,"%s",l->objectStore.items[1].coreMeta.object_id);
    if (travel) {
        c.axis=(Vec3){1,0,0};snprintf(c.id,64,"bed_travel");
        if (!Layout_InitLinearTravel(l,&c,1,3)) return false;
    } else {
        c.a.kind=c.b.kind=LAYOUT_REFERENCE_AXIS_U;c.b.local_offset_meters[0]=-1;
        if (!Layout_InitAngularTravel(l,&c,0,110)) return false;
    }
    if (!Layout_ConstraintEdit(l,&c,NULL,NULL,NULL)) return false;
    RectPrismPrimitiveCreateParams params={.width=.25f,.height=.25f,.depth=.5f,.useExplicitFrame=true,
        .explicitFrame={.origin={strstr(mode,"miss")?1.7f:travel?2:1,strstr(mode,"miss")?1.7f:travel?(group[0]?.8f:0):1,0},.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    uint32_t cabinet=0;if (!Layout_CreateRectPrismPrimitive(l,&params,&cabinet,NULL)) return false;
    snprintf(Layout_ObjectStore_Find(&l->objectStore,cabinet)->info.label,96,"Storage cabinet");
    UIPanelState* ui=UIPanel_Get();(void)UIPanel_SetDisplayUnit(CORE_UNIT_MILLIMETER);
    if (strstr(mode,"measure")) {
        UIPanel_BeginMeasurement();UIPanel_MeasurementSelectRule(0);ui->measurement.picking=false;
        UIPanel_SetActiveRightTab(ui,UI_PANEL_RIGHT_TAB_MEASURE);UIPanel_LayoutMeasurementPane();return true;
    }
    if (!Layout_GenerateMotionEnvelope(l,c.id,33,NULL,NULL)) return false;
    const LayoutMotionEnvelope* envelope=Layout_FindRuleEnvelope(&l->objectStore,c.id);
    if (strstr(mode,"stale")) {c.travel_max=90;if (!Layout_ConstraintEdit(l,&c,NULL,NULL,NULL)) return false;}
    UIPanel_SetActiveRightTab(ui,UI_PANEL_RIGHT_TAB_PARTS);ui->parts.mode=4;
    state->editor.selectedObject3DId=envelope->object_id;UIPanel_SpatialEnterVolumes();
    if (strstr(mode,"checks") || strstr(mode,"stale") || strstr(mode,"preview")) {ui->parts.mode=5;UIPanel_SpatialRunChecks();ui->spatial.selected_result=0;state->editor.selectedObject3DId=cabinet;}
    if (strstr(mode,"preview") && ui->spatial.result_count) (void)UIPanel_MotionInspectionClick(PARTS_MOTION_INSPECT,&ui->spatial.results[0]);
    UIPanel_LayoutParts();return true;
}

/* Disposable native-render fixture, reached only by the explicit artifact mode. */
bool LineDrawingEngineering_StageProof(const char* mode) {
    GlobalState* state=Global_Get();Layout* layout=&state->layout;
    if(layout->objectStore.count!=2)return false;
    if(!strncmp(mode,"parts-motion-",13))return motion_proof(mode);
    char parents[2][64];
    for(int i=0;i<2;++i) {
        LayoutAssembly a={.frame={.origin={i?1.5f:0,0,0},.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
        snprintf(a.info.label,96,"%s",i?"Water cabinet":"Van demo");snprintf(a.info.entity_type,64,"Assembly");
        if(i)snprintf(a.info.parent_id,64,"%s",parents[0]);
        if(!Layout_EditAssembly(layout,&a,NULL,NULL,NULL))return false;
        snprintf(parents[i],64,"%s",layout->objectStore.assemblies[i].id);
    }
    for(int i=0;i<2;++i) {
        Object3D* o=&layout->objectStore.items[i];LayoutEntityInfo info={.reference=i==0,.property_count=2};
        snprintf(info.label,96,"%s",i?"Cabinet side":"Reference rail");snprintf(info.entity_type,64,"%s",i?"Panel":"StructuralMember");snprintf(info.parent_id,64,"%s",parents[i]);
        info.properties[0]=(LayoutProperty){.kind=LAYOUT_PROPERTY_TEXT};snprintf(info.properties[0].key,48,"subsystem");snprintf(info.properties[0].text,128,"%s",i?"interior.cabinetry":"oem.structure");
        info.properties[1]=(LayoutProperty){.kind=LAYOUT_PROPERTY_LENGTH,.number=.0127};snprintf(info.properties[1].key,48,"thickness");
        if(!Layout_SetEntityInfo(layout,o->coreMeta.object_id,&info,NULL,NULL))return false;
    }
    if(!Layout_MoveAssembly(layout,parents[1],(double[]){.025,0,0},(Vec3){0,0,30},NULL,NULL))return false;
    (void)UIPanel_SetDisplayUnit(CORE_UNIT_MILLIMETER);
    if(!strcmp(mode,"parts-assembly")) {
        if(!UIPanel_PartsSelectEntity(parents[1]))return false;
        UIPanel_Get()->parts.movement_open=true;
        for(int k=0;k<3;++k)snprintf(UIPanel_Get()->parts.move[k],64,"0 mm");
        snprintf(UIPanel_Get()->parts.angle,64,"0");
    } else {
        state->editor.selectedObject3DId=layout->objectStore.items[1].objectId;
        if(!UIPanel_PartsSelectEntity(layout->objectStore.items[1].coreMeta.object_id))return false;
        UIPanel_Get()->parts.properties_open=true;
        if(!strcmp(mode,"parts-filter")) {
            UIPanel_Get()->parts.mode=2;UIPanel_Get()->parts.filter.designation=1;
            layout->objectStore.view_query=UIPanel_Get()->parts.filter;
        }
    }
    if (!strncmp(mode,"parts-links",11)) {
        for (int k=0;k<3;++k) {
            LayoutRelationship r={.kind=(LayoutRelationshipKind)k};
            snprintf(r.source,64,"%s",layout->objectStore.items[1].coreMeta.object_id);
            snprintf(r.target,64,"%s",k==2?parents[1]:layout->objectStore.items[0].coreMeta.object_id);
            if(!Layout_EditRelationship(layout,&r,NULL,NULL,NULL))return false;
        }
        UIPanelState* ui=UIPanel_Get();ui->parts.mode=3;ui->parts.properties_open=false;
        ui->parts.link=ui->parts.observed_link=layout->objectStore.relationships[1];ui->parts.link_observed=true;
        if(!strcmp(mode,"parts-links-incoming")) {
            state->editor.selectedObject3DId=layout->objectStore.items[0].objectId;
            memset(&ui->parts.link,0,sizeof(ui->parts.link));ui->parts.link_observed=false;
            snprintf(ui->parts.link.source,64,"%s",layout->objectStore.items[0].coreMeta.object_id);
        } else if(!strcmp(mode,"parts-links-choices"))ui->parts.chooser=6;
    }
    if(!strncmp(mode,"parts-volumes",13) || !strncmp(mode,"parts-checks",12)) {
        const Object3D* panel=&layout->objectStore.items[1];
        LayoutVolumeEdit v={.role=LAYOUT_VOLUME_SERVICE,.size_meters={1.2,.8,.5},
            .center_meters={panel->transform.position.x,panel->transform.position.y,panel->transform.position.z}};
        snprintf(v.name,96,"Fuse box service access");snprintf(v.owner,64,"%s",layout->objectStore.items[0].coreMeta.object_id);
        if(!Layout_EditVolume(layout,&v,false,NULL,NULL))return false;
        UIPanelState* ui=UIPanel_Get();ui->parts.properties_open=false;ui->parts.mode=4;
        state->editor.selectedObject3DId=layout->objectStore.items[2].objectId;UIPanel_SpatialEnterVolumes();
        if(!strncmp(mode,"parts-checks",12)) {
            ui->parts.mode=5;state->editor.selectedObject3DId=layout->objectStore.items[1].objectId;LayoutSpatialRule r={.kind=LAYOUT_SPATIAL_CLEARANCE,.clearance_meters=2.5};
            snprintf(r.source,64,"%s",layout->objectStore.items[0].coreMeta.object_id);snprintf(r.target,64,"%s",layout->objectStore.items[1].coreMeta.object_id);
            if(!Layout_EditSpatialRule(layout,&r,NULL,NULL,NULL))return false;
            UIPanel_SpatialRunChecks();ui->spatial.selected_result=0;
            if(!strcmp(mode,"parts-checks-rules")){ui->spatial.rules_open=true;ui->spatial.rule=layout->objectStore.spatial_rules[0];snprintf(ui->spatial.distance,64,"2500 mm");ui->spatial.selected_result=-1;}
        }
    }
    UIPanel_LayoutParts();return true;
}
