#include "test_layout_internal.h"
#include "UI/topbar/visibility_tree.h"
#include "UI/topbar/visibility_guide_groups.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/ui_panel_camera.h"
#include "Core/camera_view.h"
#include "Layout/layout_saved_views.h"
#include "Layout/scene/layout_preview_quality.h"
static bool click(int action){LineDrawingEditorTopbar_UpdateControls();SDL_Rect r;return LDViewControls_Rect(action,&r)&&LDViewControls_Click(r.x+2,r.y+2);}
static bool appearance_camera_pose_and_inspection(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();LDViewControls_Close();
    uint32_t id;RectPrismPrimitiveCreateParams p;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);TEST_ASSERT(Layout_CreateRectPrismPrimitive(&s->layout,&p,&id,NULL));
    TEST_ASSERT(CameraView_Enter(s,false));Vec3 eye=s->cameraView.eye;float yaw=s->cameraView.yaw,pitch=s->cameraView.pitch;
    char* before=Layout_SaveToString(&s->layout);size_t history=Editor_UndoCount(&s->editor);
    for(int w=1280;w>=600;w-=340){Global_SetWindowSize(w,720);for(int mode=LD_VIEW_WIRE;mode<=LD_VIEW_MATERIAL;++mode){TEST_ASSERT(click(LD_VIEW_APPEARANCE)&&click(mode));TEST_ASSERT(s->previewMode==(LineDrawingPreviewMode)(mode-LD_VIEW_WIRE+1));TEST_ASSERT(Vec3_Distance(eye,s->cameraView.eye)==0&&s->cameraView.yaw==yaw&&s->cameraView.pitch==pitch);}}
    TEST_ASSERT(Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_FLAT));UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_VIEW);UIPanel_OnWindowResized(s->screenWidth,s->screenHeight);
    SDL_Rect r;TEST_ASSERT(UIPanel_CameraControlRect(UI_CAMERA_INSPECTION,&r)&&UIPanel_CameraClick(r.x+2,r.y+2));TEST_ASSERT(s->previewMode==LINE_DRAWING_PREVIEW_MODE_FLAT);
    CameraView_Exit(s);s->spaceMode=SPACE_MODE_2D;TEST_ASSERT(click(LD_VIEW_APPEARANCE)&&click(LD_VIEW_MATERIAL));TEST_ASSERT(s->spaceMode==SPACE_MODE_3D);
    char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(before&&after&&!strcmp(before,after)&&history==Editor_UndoCount(&s->editor));free(before);free(after);ld_test_shutdown_runtime();return true;
}
static bool hierarchy_collapsed_search_ancestor(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();LayoutAssembly a={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};snprintf(a.info.label,96,"Exterior");snprintf(a.info.entity_type,64,"Assembly");TEST_ASSERT(Layout_EditAssembly(&s->layout,&a,NULL,NULL,NULL));
    char parent[64];snprintf(parent,64,"%s",s->layout.objectStore.assemblies[0].id);snprintf(a.info.parent_id,64,"%s",parent);snprintf(a.info.label,96,"Roof equipment");TEST_ASSERT(Layout_EditAssembly(&s->layout,&a,NULL,NULL,NULL));
    uint32_t id;RectPrismPrimitiveCreateParams p;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);TEST_ASSERT(Layout_CreateRectPrismPrimitive(&s->layout,&p,&id,NULL));Object3D* o=Layout_ObjectStore_Find(&s->layout.objectStore,id);snprintf(o->info.parent_id,64,"%s",s->layout.objectStore.assemblies[1].id);snprintf(o->info.label,96,"Fan bolt");
    LDVisibilityTree_Reset();TEST_ASSERT(LDVisibilityTree_Build(&s->layout.objectStore,"")==1);LDVisibilityTree_Expand(parent);TEST_ASSERT(LDVisibilityTree_Build(&s->layout.objectStore,"")==2);
    TEST_ASSERT(LDVisibilityTree_Build(&s->layout.objectStore,"bolt")==3);LDVisibilityRow row;TEST_ASSERT(LDVisibilityTree_Row(2,&row)&&row.depth==2&&!strcmp(row.id,o->coreMeta.object_id));
    TEST_ASSERT(LDVisibilityTree_Build(&s->layout.objectStore,"roof")==3);LDVisibilityTree_Expand(parent);TEST_ASSERT(LDVisibilityTree_Build(&s->layout.objectStore,"")==1);LDVisibilityTree_Reset();ld_test_shutdown_runtime();return true;
}
static bool quality_edges_preserve_depth_and_owner(void) {
    const uint64_t changed=2000000000ull;
    TEST_ASSERT(Layout_PreviewQuality(changed,changed)==LAYOUT_PREVIEW_MOVING);
    TEST_ASSERT(Layout_PreviewQuality(changed+449999999ull,changed)==LAYOUT_PREVIEW_MOVING);
    TEST_ASSERT(Layout_PreviewQuality(changed+450000000ull,changed)==LAYOUT_PREVIEW_BALANCED);
    TEST_ASSERT(Layout_PreviewQuality(changed+1100000000ull,changed)==LAYOUT_PREVIEW_DETAIL);
    TEST_ASSERT(Layout_PreviewRefinementDelayMs(changed,changed)==450);
    TEST_ASSERT(Layout_PreviewRefinementDelayMs(changed+450000000ull,changed)==650);
    TEST_ASSERT(Layout_PreviewRefinementDelayMs(changed+1100000000ull,changed)==-1);
    TEST_ASSERT(Layout_PreviewTriangleBudget(LAYOUT_PREVIEW_MOVING)==32000);
    TEST_ASSERT(Layout_PreviewRasterScale(1000,700,2,true)==.30f);
    float large=Layout_PreviewRasterScale(7680,4320,2,true);
    TEST_ASSERT(7680*4320*large*large<=180001);
    float scale=Layout_PreviewRasterScale(1000,700,2,false);TEST_ASSERT(scale>.75f&&ceilf(1000*scale)*ceilf(700*scale)<=900000);
    uint8_t rgba[100*4];float depth[100];int32_t owner[100];for(int i=0;i<100;++i){depth[i]=i%10<5?1:2;owner[i]=i%10<5?0:1;rgba[i*4]=100;rgba[i*4+1]=80;rgba[i*4+2]=50;rgba[i*4+3]=255;}
    float original_depth[100];int32_t original_owner[100];memcpy(original_depth,depth,sizeof(depth));memcpy(original_owner,owner,sizeof(owner));
    TEST_ASSERT(Layout_PreviewEdges(rgba,depth,owner,10,10,false,false,-1,-1)==0);
    TEST_ASSERT(Layout_PreviewEdges(rgba,depth,owner,10,10,false,false,0,-1)>0);TEST_ASSERT(rgba[(4*10+4)*4]==255);
    TEST_ASSERT(Layout_PreviewAntialias(rgba,owner,10,10));TEST_ASSERT(!memcmp(depth,original_depth,sizeof(depth))&&!memcmp(owner,original_owner,sizeof(owner)));return true;
}
static bool unassigned_area_group_controls(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();uint32_t ids[2];for(int i=0;i<2;++i){RectPrismPrimitiveCreateParams p;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);TEST_ASSERT(Layout_CreateRectPrismPrimitive(&s->layout,&p,&ids[i],NULL));}
    Object3D* o=Layout_ObjectStore_Find(&s->layout.objectStore,ids[0]);o->coreMeta.flags.visible=false;o->info.property_count=1;snprintf(o->info.properties[0].key,48,"subsystem");snprintf(o->info.properties[0].text,128,"Electrical service");o->info.properties[0].kind=LAYOUT_PROPERTY_TEXT;
    char* before=Layout_SaveToString(&s->layout);LDVisibilityTree_Reset();TEST_ASSERT(LDVisibilityTree_Build(&s->layout.objectStore,"")==2);LDVisibilityRow row;TEST_ASSERT(LDVisibilityTree_Row(0,&row)&&!strcmp(row.info->label,"Electrical service"));
    TEST_ASSERT(LDVisibilityTree_Hide(&s->layout.objectStore,row.id,false)&&Layout_ObjectShown(&s->layout.objectStore,o));TEST_ASSERT(LDVisibilityTree_Only(&s->layout.objectStore,row.id)&&!Layout_ObjectShown(&s->layout.objectStore,Layout_ObjectStore_Find(&s->layout.objectStore,ids[1])));
    Layout_RestoreEntityVisibility(&s->layout.objectStore);char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(before&&after&&!strcmp(before,after)&&!Layout_ObjectShown(&s->layout.objectStore,o));free(before);free(after);LDVisibilityTree_Reset();ld_test_shutdown_runtime();return true;
}
static bool actual_tree_compact_and_all_reachable(void) {
    const char* path=getenv("SCULPT_VISIBILITY_PRESET");if(!path)return true;ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(Layout_LoadFromFile(&s->layout,path));LDVisibilityTree_Reset();
    size_t initial=LDVisibilityTree_Build(&s->layout.objectStore,"");TEST_ASSERT(initial<=8);
    size_t total=initial;for(size_t row=0;row<total;++row){LDVisibilityRow item;TEST_ASSERT(LDVisibilityTree_Row(row,&item));if(item.assembly&&!LDVisibilityTree_Expanded(item.id)){LDVisibilityTree_Expand(item.id);total=LDVisibilityTree_Build(&s->layout.objectStore,"");}}
    for(size_t i=0;i<s->layout.objectStore.count;++i){const Object3D* o=&s->layout.objectStore.items[i];if(o->isDeleted)continue;bool found=false;for(size_t row=0;row<total;++row){LDVisibilityRow item;LDVisibilityTree_Row(row,&item);if(!strcmp(item.id,o->coreMeta.object_id)){found=true;break;}}TEST_ASSERT(found);}
    LDVisibilityTree_Reset();ld_test_shutdown_runtime();return true;
}
static bool functional_guides_include_owned_objects(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();LDVisibilityTree_Reset();
    LayoutAssembly a={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};snprintf(a.info.entity_type,64,"Assembly");TEST_ASSERT(Layout_EditAssembly(&s->layout,&a,NULL,NULL,NULL));
    const char* types[]={"MotionEnvelope","Walkway","KeepoutVolume","ServiceVolume","RoutingCorridor","PhysicalObject"};uint32_t ids[6];
    for(int i=0;i<6;++i){RectPrismPrimitiveCreateParams p;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);TEST_ASSERT(Layout_CreateRectPrismPrimitive(&s->layout,&p,&ids[i],NULL));Object3D* o=Layout_ObjectStore_Find(&s->layout.objectStore,ids[i]);snprintf(o->info.entity_type,64,"%s",types[i]);snprintf(o->info.parent_id,64,"%s",s->layout.objectStore.assemblies[0].id);o->info.property_count=1;snprintf(o->info.properties[0].key,48,"subsystem");snprintf(o->info.properties[0].text,128,"Physical systems");o->info.properties[0].kind=LAYOUT_PROPERTY_TEXT;TEST_ASSERT(LDVisibilityGuide_Category(&s->layout.objectStore,o)==(i<5?i+1:0));}
    char* before=Layout_SaveToString(&s->layout);size_t history=Editor_UndoCount(&s->editor);s->authoringGuides=true;
    LDVisibilityTree_Build(&s->layout.objectStore,"");TEST_ASSERT(LDVisibilityTree_Hide(&s->layout.objectStore,"@guides",true));
    for(int i=0;i<6;++i)TEST_ASSERT(Layout_ObjectShown(&s->layout.objectStore,Layout_ObjectStore_Find(&s->layout.objectStore,ids[i]))==(i==5));TEST_ASSERT(!s->authoringGuides);
    /* The physical collection must not reveal its assembly-owned guides. */
    TEST_ASSERT(LDVisibilityTree_Hide(&s->layout.objectStore,"@assemblies",false));
    for(int i=0;i<5;++i)TEST_ASSERT(!Layout_ObjectShown(&s->layout.objectStore,Layout_ObjectStore_Find(&s->layout.objectStore,ids[i])));
    TEST_ASSERT(LDVisibilityTree_State(&s->layout.objectStore,"@assemblies")==1);
    TEST_ASSERT(LDVisibilityTree_Hide(&s->layout.objectStore,"@guides",false));for(int i=0;i<6;++i)TEST_ASSERT(Layout_ObjectShown(&s->layout.objectStore,Layout_ObjectStore_Find(&s->layout.objectStore,ids[i])));
    TEST_ASSERT(LDVisibilityTree_Only(&s->layout.objectStore,"@guides"));TEST_ASSERT(!Layout_ObjectShown(&s->layout.objectStore,Layout_ObjectStore_Find(&s->layout.objectStore,ids[5])));LDVisibilityTree_Restore(&s->layout.objectStore);TEST_ASSERT(s->authoringGuides);
    size_t n=LDVisibilityTree_Build(&s->layout.objectStore,"Walkways");LDVisibilityRow row;bool walkway=false;for(size_t i=0;i<n;++i){LDVisibilityTree_Row(i,&row);if(!strcmp(row.id,Layout_ObjectStore_Find(&s->layout.objectStore,ids[1])->coreMeta.object_id)){walkway=true;TEST_ASSERT(row.depth==2);}}TEST_ASSERT(walkway);
    char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(before&&after&&!strcmp(before,after)&&history==Editor_UndoCount(&s->editor));free(before);free(after);LDVisibilityTree_Reset();ld_test_shutdown_runtime();return true;
}
static bool legacy_and_explicit_guide_membership(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();LDVisibilityTree_Reset();
    const char* names[]={"Fan 1 open envelope vertical marker","Fan 2 rear-clear and opening envelope edge",
        "Solar cable run reservation 25mm","Cable descent reservation to driver chase","Junction cable crosslink reservation",
        "Panel 1 cell division schematic","Rail top T-slot schematic","drawer_fixed attachment frame (native motion carrier)",
        "1515 panel-support crossmember envelope","sink hinge pin envelope low","MaxxFan 1 rear base extension / reference envelope","Unpainted cabinet"};
    const int expected[]={1,1,5,5,5,7,7,7,0,0,0,0};
    uint32_t ids[12];
    for(int i=0;i<12;++i){RectPrismPrimitiveCreateParams p;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);TEST_ASSERT(Layout_CreateRectPrismPrimitive(&s->layout,&p,&ids[i],NULL));Object3D* o=Layout_ObjectStore_Find(&s->layout.objectStore,ids[i]);snprintf(o->info.label,96,"%s",names[i]);o->info.entity_type[0]=0;TEST_ASSERT(LDVisibilityGuide_Category(&s->layout.objectStore,o)==expected[i]);}
    Object3D* o=Layout_ObjectStore_Find(&s->layout.objectStore,ids[11]);o->info.property_count=1;o->info.properties[0].kind=LAYOUT_PROPERTY_TEXT;snprintf(o->info.properties[0].key,48,"view_group");
    const char* groups[]={"motion","walkways","keepouts","service","corridors","guides","annotations"};
    for(int i=0;i<7;++i){snprintf(o->info.properties[0].text,128,"%s",groups[i]);TEST_ASSERT(LDVisibilityGuide_Category(&s->layout.objectStore,o)==i+1);}
    o->info.property_count=0;
    char* before=Layout_SaveToString(&s->layout);size_t history=Editor_UndoCount(&s->editor);
    LDVisibilityTree_Build(&s->layout.objectStore,"");TEST_ASSERT(LDVisibilityTree_Hide(&s->layout.objectStore,"@guides",true));
    for(int i=0;i<12;++i)TEST_ASSERT(Layout_ObjectShown(&s->layout.objectStore,Layout_ObjectStore_Find(&s->layout.objectStore,ids[i]))==(expected[i]==0));
    LDVisibilityTree_Restore(&s->layout.objectStore);
    char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(before&&after&&!strcmp(before,after)&&history==Editor_UndoCount(&s->editor));free(before);free(after);LDVisibilityTree_Reset();ld_test_shutdown_runtime();return true;
}
static bool guide_children_contiguous_and_stable(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();LDVisibilityTree_Reset();
    const char* types[]={"PhysicalObject","RoutingCorridor","PhysicalObject","MotionEnvelope","ServiceVolume"};
    for(int i=0;i<5;++i){RectPrismPrimitiveCreateParams p;uint32_t id;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);TEST_ASSERT(Layout_CreateRectPrismPrimitive(&s->layout,&p,&id,NULL));Object3D* o=Layout_ObjectStore_Find(&s->layout.objectStore,id);snprintf(o->info.entity_type,64,"%s",types[i]);}
    LDVisibilityTree_Expand("@guides");size_t n=LDVisibilityTree_Build(&s->layout.objectStore,"");bool physical=false;int category=0;
    for(size_t i=0;i<n;++i){LDVisibilityRow row;TEST_ASSERT(LDVisibilityTree_Row(i,&row));if(!strncmp(row.id,"@guide_",7)){TEST_ASSERT(!physical);int c=atoi(row.id+7);TEST_ASSERT(c>category);category=c;TEST_ASSERT(row.depth==1);}else if(!strncmp(row.id,"@area_",6))physical=true;}
    TEST_ASSERT(category==5&&physical);LDVisibilityTree_Reset();ld_test_shutdown_runtime();return true;
}
static bool actual_guides_hide_all_categories(void) {
    const char* path=getenv("SCULPT_VISIBILITY_PRESET");if(!path)return true;ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(Layout_LoadFromFile(&s->layout,path));LDVisibilityTree_Reset();LDVisibilityTree_Build(&s->layout.objectStore,"");
    size_t guides=0,parented=0;bool categories[8]={0};for(size_t i=0;i<s->layout.objectStore.count;++i){const Object3D* o=&s->layout.objectStore.items[i];int c=LDVisibilityGuide_Category(&s->layout.objectStore,o);if(c){++guides;categories[c]=true;if(o->info.parent_id[0])++parented;}}
    TEST_ASSERT(guides>=7&&parented>=5&&categories[1]&&categories[2]&&categories[3]&&categories[4]);char* before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(LDVisibilityTree_Hide(&s->layout.objectStore,"@guides",false));for(size_t i=0;i<s->layout.objectStore.count;++i)if(LDVisibilityGuide_Category(&s->layout.objectStore,&s->layout.objectStore.items[i]))TEST_ASSERT(Layout_ObjectShown(&s->layout.objectStore,&s->layout.objectStore.items[i]));
    TEST_ASSERT(LDVisibilityTree_Hide(&s->layout.objectStore,"@guides",true));for(size_t i=0;i<s->layout.objectStore.count;++i)if(LDVisibilityGuide_Category(&s->layout.objectStore,&s->layout.objectStore.items[i]))TEST_ASSERT(!Layout_ObjectShown(&s->layout.objectStore,&s->layout.objectStore.items[i]));
    LDVisibilityTree_Restore(&s->layout.objectStore);char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(before&&after&&!strcmp(before,after));free(before);free(after);LDVisibilityTree_Reset();ld_test_shutdown_runtime();return true;
}
bool appearance_tree_run_tests(void){const TestCase cases[]={{"guide_children_contiguous_and_stable",guide_children_contiguous_and_stable},{"legacy_and_explicit_guide_membership",legacy_and_explicit_guide_membership},{"functional_guides_include_owned_objects",functional_guides_include_owned_objects},{"actual_guides_hide_all_categories",actual_guides_hide_all_categories},{"actual_tree_compact_and_all_reachable",actual_tree_compact_and_all_reachable},{"unassigned_area_group_controls",unassigned_area_group_controls},{"appearance_camera_pose_and_inspection",appearance_camera_pose_and_inspection},{"hierarchy_collapsed_search_ancestor",hierarchy_collapsed_search_ancestor},{"quality_edges_preserve_depth_and_owner",quality_edges_preserve_depth_and_owner}};return run_test_cases("AppearanceTree",cases,sizeof(cases)/sizeof(cases[0]));}
