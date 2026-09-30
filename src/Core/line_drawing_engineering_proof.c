#include "Core/line_drawing_engineering_proof.h"
#include "Core/global_state.h"
#include "Layout/layout_engineering.h"
#include "UI/ui_panel_parts.h"
#include "UI/ui_panel_shell.h"
#include <stdio.h>
#include <string.h>

/* Disposable native-render fixture, reached only by the explicit artifact mode. */
bool LineDrawingEngineering_StageProof(const char* mode) {
    GlobalState* state=Global_Get();Layout* layout=&state->layout;
    if(layout->objectStore.count!=2)return false;
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
    UIPanel_LayoutParts();return true;
}
