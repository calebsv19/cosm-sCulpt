#include "Core/collection_inspection_proof.h"
#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Layout/layout_json.h"
#include "Layout/layout_collection_inspection.h"
#include "Layout/layout_scene_collections.h"
#include "Layout/layout_saved_views.h"
#include "UI/ui_panel_measurement.h"
#include "UI/panel/ui_panel_collection_inspection.h"
#include "UI/panel/ui_panel_visibility.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "vk_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool click(AppContext* app,const AppCallbacks* cb,int action,bool measurement) {
    LineDrawingEditorTopbar_UpdateControls();SDL_Rect box;
    if(!(measurement?UIPanel_MeasurementControlRect(action,&box):LDViewControls_Rect(action,&box)))return false;
    if(measurement){UIPanelState* ui=UIPanel_Get();ui->rightScroll[UI_PANEL_RIGHT_TAB_MEASURE].scrollOffsetPx+=box.y-ui->rightBodyRect.y-8;UIPanel_CollectionInspectionLayout();if(!UIPanel_MeasurementControlRect(action,&box))return false;}
    SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=box.x+box.w/2;e.button.y=box.y+box.h/2;cb->handleInput(app,&e);return true;
}
static bool name(AppContext* app,const AppCallbacks* cb,const char* text) {
    if(!click(app,cb,INSPECT_NAME,true))return false;SDL_Event e={.type=SDL_TEXTINPUT};snprintf(e.text.text,sizeof(e.text.text),"%s",text);cb->handleInput(app,&e);e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RETURN;cb->handleInput(app,&e);return true;
}
static char* geometry(const Layout* l) {
    char* saved=Layout_SaveToString(l);if(!saved)return NULL;cJSON* root=cJSON_Parse(saved);free(saved);if(!root)return NULL;
    cJSON_DeleteItemFromObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"file"),"schemaVersion");
    cJSON* eng=cJSON_GetObjectItemCaseSensitive(root,"engineering");cJSON_DeleteItemFromObjectCaseSensitive(eng,"measurements");
    cJSON* groups=cJSON_GetObjectItemCaseSensitive(eng,"sceneCollections");cJSON* row;cJSON_ArrayForEach(row,groups)cJSON_DeleteItemFromObjectCaseSensitive(row,"inspectionFrame");
    char* result=cJSON_PrintUnformatted(root);cJSON_Delete(root);return result;
}
static bool capture(AppContext* app,const AppCallbacks* cb,const char* root,const char* label) {
    UIPanel_Get()->rightScroll[UI_PANEL_RIGHT_TAB_MEASURE].scrollOffsetPx=0;
    for(int i=0;i<24;++i){cb->handleUpdate(app);if(!App_RenderOnce(app,cb->handleRender))return false;SDL_Delay(10);}
    char path[4096];snprintf(path,sizeof(path),"%s/%s.bmp",root,label);
    return vk_renderer_request_capture(app->renderer,path)==VK_SUCCESS&&App_RenderOnce(app,cb->handleRender);
}
bool CollectionInspectionProof_Run(AppContext* app,const AppCallbacks* cb,const char* root) {
    GlobalState* g=Global_Get();Layout* l=&g->layout;Editor_ClearHistory(&g->editor);char* original=geometry(l);char* before=Layout_SaveToString(l);
    bool ok=original&&before&&Layout_FindSceneCollection(&l->objectStore,"@scene.driver");
    Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_MATERIAL);
    (void)UIPanel_SetDisplayUnit(CORE_UNIT_METER);
    ok=ok&&click(app,cb,LD_VIEW_VISIBILITY,false)&&click(app,cb,UI_VIS_SCENE,false)&&click(app,cb,402,false)&&click(app,cb,UI_VIS_INSPECT,false);
    ok=ok&&UIPanel_CollectionInspectionActive()&&click(app,cb,INSPECT_FOCUS,true)&&click(app,cb,INSPECT_UNITS,true)&&click(app,cb,INSPECT_UNITS,true);
    char* after=Layout_SaveToString(l);bool readonly=before&&after&&!strcmp(before,after)&&Editor_UndoCount(&g->editor)==0;free(before);free(after);ok=ok&&readonly;
    size_t mesh_indices[2]={SIZE_MAX,SIZE_MAX};const Object3D* frame_pick=NULL;
    for(size_t i=0;i<l->objectStore.count;++i)if(Layout_InspectionMember(l,"@scene.driver",&l->objectStore.items[i])) {
        const Object3D* o=&l->objectStore.items[i];PlaneFrame3 frame;
        if(!frame_pick&&Layout_InspectionFrame(l,o->coreMeta.object_id,&frame)==LAYOUT_MEASUREMENT_OK)frame_pick=o;
        if(o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE){if(mesh_indices[0]==SIZE_MAX)mesh_indices[0]=i;else if(mesh_indices[1]==SIZE_MAX)mesh_indices[1]=i;}
    }
    if(frame_pick)g->editor.selectedObject3DId=frame_pick->objectId;else ok=false;
    ok=ok&&click(app,cb,INSPECT_FRAME_PICK,true)&&Editor_UndoCount(&g->editor)==1;
    const char* names[]={"Driver cabinet U extent","Driver cabinet V extent","Driver cabinet N extent"};
    for(int k=0;k<3&&ok;++k){ok=click(app,cb,INSPECT_NEW,true);for(int n=0;n<k&&ok;++n)ok=click(app,cb,INSPECT_KIND,true);ok=ok&&name(app,cb,names[k])&&click(app,cb,INSPECT_SAVE,true)&&l->objectStore.measurement_count==(size_t)k+1&&!strcmp(l->objectStore.measurements[k].name,names[k]);}
    ok=ok&&click(app,cb,INSPECT_NEW,true)&&mesh_indices[1]!=SIZE_MAX;
    for(int n=0;n<3&&ok;++n)ok=click(app,cb,INSPECT_KIND,true);
    for(int j=0;j<2&&ok;++j) {
        ok=click(app,cb,INSPECT_PART,true)&&click(app,cb,INSPECT_PART_BASE+(int)mesh_indices[j],true)&&
           click(app,cb,j?INSPECT_SET_B:INSPECT_SET_A,true)&&click(app,cb,j?INSPECT_FEATURE_B:INSPECT_FEATURE_A,true);
    }
    ok=ok&&name(app,cb,"Driver envelope datum gap")&&click(app,cb,INSPECT_SAVE,true)&&l->objectStore.measurement_count==4&&!strcmp(l->objectStore.measurements[3].name,"Driver envelope datum gap");
    ok=ok&&Editor_Undo(&g->editor,l)&&l->objectStore.measurement_count==3&&Editor_Redo(&g->editor,l)&&l->objectStore.measurement_count==4;
    char path[4096];snprintf(path,sizeof(path),"%s/driver_cabinet_inspection.layout.json",root);
    ok=ok&&access(path,F_OK)!=0&&Layout_SaveToFile(l,path)&&Layout_LoadFromFile(l,path);Global_OnLayoutLoaded(path);
    ok=ok&&l->objectStore.measurement_count==4&&UIPanel_BeginCollectionInspection("@scene.driver")&&click(app,cb,INSPECT_FOCUS,true)&&capture(app,cb,root,"driver_cabinet_inspection");
    ok=ok&&click(app,cb,INSPECT_SAVED,true)&&click(app,cb,INSPECT_SAVED_BASE+3,true)&&capture(app,cb,root,"saved_mesh_datums");
    char* final=geometry(l);bool unchanged=original&&final&&!strcmp(original,final);free(original);free(final);ok=ok&&unchanged;
    const LayoutSceneCollection* current=Layout_FindSceneCollection(&l->objectStore,"@scene.driver");
    LayoutCollectionInspection bounds=Layout_InspectCollection(l,"@scene.driver",current?current->inspection_frame:"");
    for(size_t i=0;i<l->objectStore.measurement_count;++i)if(Layout_EvaluateSavedMeasurement(l,&l->objectStore.measurements[i]).status!=LAYOUT_MEASUREMENT_OK)ok=false;
    snprintf(path,sizeof(path),"%s/collection_inspection.json",root);FILE* f=fopen(path,"wx");
    if(!f)ok=false;else {
        fprintf(f,"{\"passed\":%s,\"inspection_readonly\":%s,\"geometry_unchanged\":%s,\"physical_parts\":%zu,\"mesh_envelopes\":%zu,\"measurements\":[",ok?"true":"false",readonly?"true":"false",unchanged?"true":"false",bounds.parts,bounds.mesh_parts);
        for(size_t i=0;i<l->objectStore.measurement_count;++i){LayoutMeasurementResult v=Layout_EvaluateSavedMeasurement(l,&l->objectStore.measurements[i]);if(v.status!=LAYOUT_MEASUREMENT_OK)ok=false;fprintf(f,"%s{\"id\":\"%s\",\"value_m\":%.9g,\"valid\":%s}",i?",":"",l->objectStore.measurements[i].id,v.value,v.status==LAYOUT_MEASUREMENT_OK?"true":"false");}
        fprintf(f,"]}\n");fclose(f);
    }
    return ok;
}
