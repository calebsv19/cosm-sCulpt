#include "test_layout_internal.h"
#include "Layout/layout_construction_reference.h"
#include "Layout/layout_scene_collections.h"
#include "Layout/layout_saved_views.h"
#include "Core/construction_reference_export.h"
#include "UI/panel/ui_panel_collection_inspection.h"
#include "UI/ui_panel_measurement.h"
#include <unistd.h>
#include <sys/stat.h>

static bool setup(void){ld_test_init_runtime();Layout* l=&Global_Get()->layout;RectPrismPrimitiveCreateParams p={.width=1,.height=2,.depth=3,.useExplicitFrame=true,.explicitFrame={.origin={10,20,30},.axisU={0,1,0},.axisV={-1,0,0},.normal={0,0,1}}};uint32_t id;
    if(!Layout_CreateRectPrismPrimitive(l,&p,&id,NULL))return false;Object3D* o=Layout_ObjectStore_Find(&l->objectStore,id);if(core_object_set_identity(&o->coreMeta,"panel_A","rect_prism_primitive").code!=CORE_OK)return false;
    snprintf(o->info.label,96,"<script>bad & label</script>");LayoutSceneCollection c={0};snprintf(c.id,64,"@scene.pilot");snprintf(c.name,96,"Driver cabinet");snprintf(c.inspection_frame,64,"panel_A");return Layout_EditSceneCollection(l,&c,NULL,NULL,NULL)&&Layout_AssignSceneCollection(l,c.id,"panel_A",true,NULL,NULL);
}
static bool analytic_model(void){TEST_ASSERT(setup());Layout* l=&Global_Get()->layout;char* before=Layout_SaveToString(l);cJSON* spec=Layout_ConstructionReference(l,"@scene.pilot");TEST_ASSERT(spec);
    cJSON* part=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(spec,"parts"),0);cJSON* size=cJSON_GetObjectItemCaseSensitive(part,"localSize_m");TEST_ASSERT(fabs(cJSON_GetArrayItem(size,0)->valuedouble-1)<1e-6);
    cJSON* max=cJSON_GetObjectItemCaseSensitive(spec,"frameMax_m");TEST_ASSERT(fabs(cJSON_GetArrayItem(max,0)->valuedouble-.5)<1e-6&&fabs(cJSON_GetArrayItem(max,1)->valuedouble-1)<1e-6);
    TEST_ASSERT(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(part,"cornersFrame_m"))==8);
    TEST_ASSERT(Layout_SetEntityHidden(&l->objectStore,"@scene.pilot",true));cJSON* hidden=Layout_ConstructionReference(l,"@scene.pilot");TEST_ASSERT(hidden&&cJSON_Compare(spec,hidden,true));cJSON_Delete(hidden);
    char* after=Layout_SaveToString(l);TEST_ASSERT(!strcmp(before,after));free(before);free(after);cJSON_Delete(spec);ld_test_shutdown_runtime();return true;}
static bool export_create_only_escape_snapshot(void){TEST_ASSERT(setup());Layout* l=&Global_Get()->layout;char dir[256];snprintf(dir,256,"/tmp/sculpt_reference_test_%ld",(long)getpid());char msg[160];char* before=Layout_SaveToString(l);
    TEST_ASSERT(Layout_ExportConstructionReference(l,"@scene.pilot","source<&",dir,msg,sizeof(msg)));TEST_ASSERT(!Layout_ExportConstructionReference(l,"@scene.pilot","",dir,msg,sizeof(msg)));
    char path[300];snprintf(path,300,"%s/reference_sheets.html",dir);FILE* f=fopen(path,"rb");TEST_ASSERT(f);char* html=calloc(100000,1);TEST_ASSERT(html);size_t n=fread(html,1,99999,f);html[n]=0;fclose(f);TEST_ASSERT(strstr(html,"&lt;script&gt;bad &amp; label&lt;/script&gt;")&&!strstr(html,"<script>"));free(html);
    Layout loaded={0};Layout_Init(&loaded,.05f);snprintf(path,300,"%s/source_snapshot.layout.json",dir);TEST_ASSERT(Layout_LoadFromFile(&loaded,path));char* readback=Layout_SaveToString(&loaded);TEST_ASSERT(readback&&!strcmp(before,readback));free(readback);Layout_Free(&loaded);
    char* after=Layout_SaveToString(l);TEST_ASSERT(!strcmp(before,after));free(before);free(after);
    const char* leaves[]={"reference_sheets.html","view_uv.svg","view_un.svg","view_vn.svg","specification.json","source_snapshot.layout.json","COMPLETE.txt"};for(size_t i=0;i<7;++i){snprintf(path,300,"%s/%s",dir,leaves[i]);TEST_ASSERT(!unlink(path));}TEST_ASSERT(!rmdir(dir));ld_test_shutdown_runtime();return true;}
static bool failures_and_invalid_measurement(void){TEST_ASSERT(setup());Layout* l=&Global_Get()->layout;TEST_ASSERT(!Layout_ConstructionReference(l,"absent"));Object3D* o=(Object3D*)Layout_InspectionObject(l,"panel_A");o->transform.scale.x=2;TEST_ASSERT(!Layout_ConstructionReference(l,"@scene.pilot"));o->transform.scale.x=1;
    LayoutSavedMeasurement m={.kind=3};snprintf(m.id,64,"broken");snprintf(m.name,96,"Missing datum");snprintf(m.collection,64,"@scene.pilot");snprintf(m.a.entity_id,64,"absent");snprintf(m.b.entity_id,64,"panel_A");l->objectStore.measurements[0]=m;l->objectStore.measurement_count=1;
    cJSON* spec=Layout_ConstructionReference(l,"@scene.pilot");TEST_ASSERT(spec);cJSON* value=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(spec,"measurements"),0);TEST_ASSERT(!cJSON_GetObjectItemCaseSensitive(value,"value_m"));cJSON_Delete(spec);
    char msg[160];TEST_ASSERT(!Layout_ExportConstructionReference(l,"@scene.pilot","","/missing-parent/sculpt-output",msg,sizeof(msg)));ld_test_shutdown_runtime();return true;}
bool construction_reference_run_tests(void){const TestCase cases[]={{"frame_projection_visibility_readonly",analytic_model},{"create_only_escape_snapshot",export_create_only_escape_snapshot},{"incomplete_invalid_measurement_refusal",failures_and_invalid_measurement}};return run_test_cases("ConstructionReference",cases,3);}
