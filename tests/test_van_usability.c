#include "test_artifact_helpers.h"
#include "test_layout_internal.h"
#include "Core/camera_view.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "UI/ui_panel_camera.h"
#include "Input/input_handler.h"
#include "Editor/scene_authoring_path_handles.h"
#include "Layout/layout_saved_views.h"
#include "Layout/scene/layout_mesh_preview_cache.h"
#include "Layout/scene/layout_mesh_prepared_store.h"
#include "core_pack.h"
#include "Layout/scene/layout_mesh_view_plan.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
static const CoreMeshPreviewLodMesh* cache_wait(const char* path,bool interactive) {
    for(int i=0;i<5000;++i) {
        (void)Layout_MeshPreviewCachePoll();
        const CoreMeshPreviewLodMesh* mesh=Layout_MeshPreviewCacheGet(path,interactive);
        if(mesh || !Layout_MeshPreviewCacheRead().pending_jobs)return mesh;
        SDL_Delay(1);
    }
    return NULL;
}
static bool guides_and_navigation(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();Editor_ClearHistory(&s->editor);
    TEST_ASSERT(!s->authoringGuides);
    TEST_ASSERT(Layout_SceneAuthoringState_Select(&s->layout.sceneAuthoring,LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,0));
    TEST_ASSERT(!SceneAuthoringPathHandles_ShouldShow(s));s->authoringGuides=true;
    TEST_ASSERT(SceneAuthoringPathHandles_ShouldShow(s));
    LineDrawingSceneCameraPose p={.position={1,2,3},.forward={0,1,0},.up={0,0,1}};Vec3 near[4],far[4];
    TEST_ASSERT(SceneAuthoringCameraGuide_Corners(&p,90,2,near,far));
    for(int i=0;i<4;++i) {TEST_ASSERT(fabsf(near[i].y-3)<1e-6 && fabsf(far[i].y-6)<1e-6);TEST_ASSERT(fabsf(fabsf(near[i].x-1)-2)<1e-5);TEST_ASSERT(fabsf(fabsf(far[i].z-3)-4)<1e-5);}
    TEST_ASSERT(!SceneAuthoringCameraGuide_Corners(&p,180,2,near,far));
    RectPrismPrimitiveCreateParams prism;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&prism);uint32_t object;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&s->layout,&prism,&object,NULL));
    char* before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(CameraView_Enter(s,false));TEST_ASSERT(!SceneAuthoringPathHandles_ShouldShow(s));
    Vec3 eye=s->cameraView.eye;CameraView_Drag(s,20,8,false);TEST_ASSERT(Vec3_Distance(eye,s->cameraView.eye)>.001f);
    eye=s->cameraView.eye;CameraView_Drag(s,20,8,true);TEST_ASSERT(Vec3_Distance(eye,s->cameraView.eye)>.001f);
    float distance=Vec3_Distance(s->cameraView.eye,s->cameraView.orbit_target);CameraView_Dolly(s,2);
    TEST_ASSERT(Vec3_Distance(s->cameraView.eye,s->cameraView.orbit_target)<distance);
    TEST_ASSERT(CameraView_Frame(s,false));char* after=Layout_SaveToString(&s->layout);
    TEST_ASSERT(before && after && !strcmp(before,after) && Editor_UndoCount(&s->editor)==0);
    free(before);free(after);ld_test_shutdown_runtime();return true;
}
static bool visibility_subtree_and_history(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();Editor_ClearHistory(&s->editor);
    LayoutAssembly a={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};snprintf(a.info.label,96,"Visibility parent");snprintf(a.info.entity_type,64,"Assembly");
    TEST_ASSERT(Layout_EditAssembly(&s->layout,&a,NULL,NULL,NULL));
    char root[64];snprintf(root,64,"%s",s->layout.objectStore.assemblies[s->layout.objectStore.assembly_count-1].id);
    uint32_t ids[2];for(int i=0;i<2;++i) {
        RectPrismPrimitiveCreateParams p;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);TEST_ASSERT(Layout_CreateRectPrismPrimitive(&s->layout,&p,&ids[i],NULL));
    }
    Object3D* o=Layout_ObjectStore_Find(&s->layout.objectStore,ids[0]);LayoutEntityInfo info=o->info;snprintf(info.parent_id,64,"%s",root);
    char id[64];snprintf(id,64,"%s",o->coreMeta.object_id);TEST_ASSERT(Layout_SetEntityInfo(&s->layout,id,&info,NULL,NULL));
    UIPanel_PartsEnterMode(1);TEST_ASSERT(UIPanel_PartsSelectEntity(root));
    SDL_Rect control;TEST_ASSERT(UIPanel_PartsControlRect(PARTS_HIDE,&control));
    TEST_ASSERT(UIPanel_PartsClick(control.x+2,control.y+2));
    TEST_ASSERT(!Layout_EntityInspectionShown(&s->layout.objectStore,id));
    TEST_ASSERT(UIPanel_PartsControlRect(PARTS_RESTORE_VISIBILITY,&control));
    TEST_ASSERT(UIPanel_PartsClick(control.x+2,control.y+2));
    TEST_ASSERT(Layout_EntityInspectionShown(&s->layout.objectStore,id));
    char* before=Layout_SaveToString(&s->layout);
    TEST_ASSERT(Layout_SetEntityHidden(&s->layout.objectStore,root,true));TEST_ASSERT(!Layout_ObjectShown(&s->layout.objectStore,Layout_ObjectStore_Find(&s->layout.objectStore,ids[0])));
    TEST_ASSERT(Layout_ObjectShown(&s->layout.objectStore,Layout_ObjectStore_Find(&s->layout.objectStore,ids[1])));
    TEST_ASSERT(Layout_IsolateEntity(&s->layout.objectStore,root));TEST_ASSERT(Layout_ObjectShown(&s->layout.objectStore,Layout_ObjectStore_Find(&s->layout.objectStore,ids[0])));
    TEST_ASSERT(!Layout_ObjectShown(&s->layout.objectStore,Layout_ObjectStore_Find(&s->layout.objectStore,ids[1])));
    Layout_ShowAllViews(&s->layout.objectStore);char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(!strcmp(before,after));free(before);free(after);
    TEST_ASSERT(Layout_SetAuthoredVisibility(&s->layout,root,false,Layout_GeometryHistory,NULL));TEST_ASSERT(Editor_UndoCount(&s->editor)==1);
    TEST_ASSERT(!Layout_ObjectStore_Find(&s->layout.objectStore,ids[0])->coreMeta.flags.visible);
    TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));TEST_ASSERT(Layout_ObjectStore_Find(&s->layout.objectStore,ids[0])->coreMeta.flags.visible);
    TEST_ASSERT(!Layout_SetEntityHidden(&s->layout.objectStore,"missing",true));ld_test_shutdown_runtime();return true;
}
static bool cache_working_set_and_budget(void) {
    char root[PATH_MAX]; TEST_ASSERT(ld_test_artifact_temp_path(root, sizeof(root), "sculpt-cache-test-XXXXXX"));TEST_ASSERT(mkdtemp(root));
    char disabled[512];snprintf(disabled,sizeof(disabled),"%s/not-created/store",root);
    TEST_ASSERT(!setenv("LINE_DRAWING_PREVIEW_STORE",disabled,1));
    char* json=ld_test_read_text_file("tests/fixtures/agent_mesh_assets/asset_imported_tetrahedron_01.runtime.json");TEST_ASSERT(json);
    Layout l;Layout_Init(&l,1);char paths[12][512];
    for(int i=0;i<12;++i) {
        snprintf(paths[i],512,"%s/%d.json",root,i);FILE* f=fopen(paths[i],"w");TEST_ASSERT(f);TEST_ASSERT(fputs(json,f)>=0);TEST_ASSERT(!fclose(f));
        uint32_t id;Transform3D t=Layout_Transform3D_Default();TEST_ASSERT(Layout_CreateMeshAssetInstanceFromRuntimeAsset(&l,paths[i],&t,&id,NULL,0));
    }
    Layout_MeshPreviewCacheReset();TEST_ASSERT(!Layout_MeshPreviewCacheSync(&l));
    for(int i=0;i<12;++i)TEST_ASSERT(cache_wait(paths[i],true));
    LayoutMeshPreviewCacheStats cold=Layout_MeshPreviewCacheRead();TEST_ASSERT(cold.loads==12 && cold.lod_builds==48 && cold.resident_assets==12);
    for(int frame=0;frame<5;++frame) {TEST_ASSERT(!Layout_MeshPreviewCacheSync(&l));for(int i=0;i<12;++i)TEST_ASSERT(cache_wait(paths[i],false));}
    LayoutMeshPreviewCacheStats warm=Layout_MeshPreviewCacheRead();TEST_ASSERT(warm.loads==cold.loads && warm.lod_builds==cold.lod_builds && warm.hits>=60);
    FILE* f=fopen(paths[0],"a");TEST_ASSERT(f);fputs("\n",f);fclose(f);TEST_ASSERT(Layout_MeshPreviewCacheCheckChanges());TEST_ASSERT(cache_wait(paths[0],true));TEST_ASSERT(Layout_MeshPreviewCacheRead().loads==13);
    unlink(paths[1]);TEST_ASSERT(Layout_MeshPreviewCacheCheckChanges());TEST_ASSERT(!cache_wait(paths[1],true));
    Layout_MeshPreviewCacheSetBudget(cold.resident_bytes/12*3);
    for(int i=2;i<12;++i)TEST_ASSERT(cache_wait(paths[i],true));
    warm=Layout_MeshPreviewCacheRead();TEST_ASSERT(warm.resident_bytes<=warm.budget_bytes && warm.resident_assets<=3 && warm.evictions>0);
    Layout_MeshPreviewCacheSetBudget(1);TEST_ASSERT(!cache_wait(paths[2],true));
    TEST_ASSERT(Layout_MeshPreviewCacheRead().resident_bytes==0);
    Layout_MeshPreviewCacheSetBudget(cold.resident_bytes);TEST_ASSERT(cache_wait(paths[2],true));
    TEST_ASSERT(Layout_MeshPreviewCacheSync(NULL));TEST_ASSERT(Layout_MeshPreviewCacheRead().resident_assets==0);
    Layout_MeshPreviewCacheShutdown();Layout_Free(&l);for(int i=0;i<12;++i)unlink(paths[i]);rmdir(root);free(json);unsetenv("LINE_DRAWING_PREVIEW_STORE");return true;
}
static bool prepared_persistence_and_corruption(void) {
    char root[PATH_MAX]; TEST_ASSERT(ld_test_artifact_temp_path(root, sizeof(root), "sculpt-prepared-test-XXXXXX"));TEST_ASSERT(mkdtemp(root));
    char store[512],source[512],path[1024],replacement[1024];
    snprintf(store,sizeof(store),"%s/store",root);snprintf(source,sizeof(source),"%s/source.json",root);
    TEST_ASSERT(!setenv("LINE_DRAWING_PREVIEW_STORE",store,1));
    char* json=ld_test_read_text_file("tests/fixtures/agent_mesh_assets/asset_imported_tetrahedron_01.runtime.json");TEST_ASSERT(json);
    FILE* f=fopen(source,"w");TEST_ASSERT(f);fputs(json,f);fclose(f);
    Layout_MeshPreviewCacheReset();const CoreMeshPreviewLodMesh* first=cache_wait(source,false);TEST_ASSERT(first);
    size_t vertices=first->vertex_count,triangles=first->triangle_count;
    TEST_ASSERT(Layout_MeshPreviewCacheRead().loads==1 && Layout_MeshPreviewCacheRead().prepared_writes==1);
    TEST_ASSERT(Layout_MeshPreparedStoreKey(source,path));
    Layout_MeshPreviewCacheReset();first=cache_wait(source,false);TEST_ASSERT(first);
    TEST_ASSERT(first->vertex_count==vertices && first->triangle_count==triangles);
    TEST_ASSERT(Layout_MeshPreviewCacheRead().prepared_hits==1 && Layout_MeshPreviewCacheRead().loads==0 && Layout_MeshPreviewCacheRead().lod_builds==0);
    /* Version mismatch, corrupt geometry and malformed container each rebuild. */
    for(int i=0;i<3;++i) {
        CorePackReader r;CorePackChunkInfo c;
        TEST_ASSERT(core_pack_reader_open(path,&r).code==CORE_OK);
        TEST_ASSERT(core_pack_reader_find_chunk(&r,i==0?"META":"MIDX",0,&c).code==CORE_OK);core_pack_reader_close(&r);
        f=fopen(path,"r+b");TEST_ASSERT(f);
        if(i<2) {TEST_ASSERT(!fseek(f,(long)c.data_offset,SEEK_SET));uint32_t invalid=0xffffffff;TEST_ASSERT(fwrite(&invalid,sizeof(invalid),1,f)==1);}
        else {TEST_ASSERT(!ftruncate(fileno(f),2));}
        fclose(f);Layout_MeshPreviewCacheReset();TEST_ASSERT(cache_wait(source,false));
        TEST_ASSERT(Layout_MeshPreviewCacheRead().prepared_writes==1 && Layout_MeshPreviewCacheRead().loads==1);
    }
    f=fopen(source,"a");TEST_ASSERT(f);fputs("\n",f);fclose(f);
    TEST_ASSERT(Layout_MeshPreparedStoreKey(source,replacement) && strcmp(path,replacement));
    Layout_MeshPreviewCacheReset();TEST_ASSERT(cache_wait(source,true));TEST_ASSERT(Layout_MeshPreviewCacheRead().loads==1);
    f=fopen(source,"w");TEST_ASSERT(f);fputs("invalid runtime",f);fclose(f);
    Layout_MeshPreviewCacheReset();TEST_ASSERT(!cache_wait(source,true));TEST_ASSERT(Layout_MeshPreviewCacheRead().prepared_writes==0);
    f=fopen(source,"w");TEST_ASSERT(f);TEST_ASSERT(!ftruncate(fileno(f),65*1024*1024));fclose(f);
    Layout_MeshPreviewCacheReset();TEST_ASSERT(!cache_wait(source,true));
    TEST_ASSERT(Layout_MeshPreviewCacheRead().oversized_sources==1 && Layout_MeshPreviewCacheRead().loads==0);
    Layout_MeshPreviewCacheShutdown();unlink(source);free(json);
    DIR* d=opendir(store);TEST_ASSERT(d);struct dirent* e;
    while((e=readdir(d)))if(e->d_name[0]!='.') {char file[1024];snprintf(file,sizeof(file),"%s/%s",store,e->d_name);unlink(file);}
    closedir(d);rmdir(store);rmdir(root);unsetenv("LINE_DRAWING_PREVIEW_STORE");return true;
}
static bool asynchronous_pressure_and_scene_cancellation(void) {
    char root[PATH_MAX]; TEST_ASSERT(ld_test_artifact_temp_path(root, sizeof(root), "sculpt-async-test-XXXXXX"));TEST_ASSERT(mkdtemp(root));
    char disabled[512];snprintf(disabled,sizeof(disabled),"%s/missing/store",root);setenv("LINE_DRAWING_PREVIEW_STORE",disabled,1);
    char* json=ld_test_read_text_file("tests/fixtures/agent_mesh_assets/asset_imported_tetrahedron_01.runtime.json");TEST_ASSERT(json);
    Layout_MeshPreviewCacheShutdown();char paths[20][512];
    for(int i=0;i<20;++i) {
        snprintf(paths[i],512,"%s/%d.json",root,i);FILE* f=fopen(paths[i],"w");TEST_ASSERT(f);fputs(json,f);fclose(f);
        TEST_ASSERT(!Layout_MeshPreviewCacheGet(paths[i],true));
    }
    LayoutMeshPreviewCacheStats before=Layout_MeshPreviewCacheRead();
    TEST_ASSERT(before.pending_jobs==8 && before.queue_rejections==12 && before.loads==0);
    Layout_MeshPreviewCacheReset();TEST_ASSERT(Layout_MeshPreviewCacheRead().resident_assets==0);
    for(int i=0;i<5000 && Layout_MeshPreviewCacheRead().pending_jobs;++i) {Layout_MeshPreviewCachePoll();SDL_Delay(1);}
    LayoutMeshPreviewCacheStats after=Layout_MeshPreviewCacheRead();
    TEST_ASSERT(!after.pending_jobs && after.stale_results==8 && after.resident_assets==0 && after.loads==0);
    TEST_ASSERT(cache_wait(paths[0],true));after=Layout_MeshPreviewCacheRead();
    TEST_ASSERT(after.background_jobs==1 && after.ui_decode_calls==0 && after.loads==1);
    /* Replacement after submission cannot publish the previous source stamp. */
    Layout_MeshPreviewCacheReset();TEST_ASSERT(!Layout_MeshPreviewCacheGet(paths[1],true));
    FILE* f=fopen(paths[1],"a");TEST_ASSERT(f);fputs("\n",f);fclose(f);
    for(int i=0;i<5000 && Layout_MeshPreviewCacheRead().pending_jobs;++i) {Layout_MeshPreviewCachePoll();SDL_Delay(1);}
    TEST_ASSERT(Layout_MeshPreviewCacheRead().resident_assets==0 && Layout_MeshPreviewCacheRead().stale_results==1);
    TEST_ASSERT(cache_wait(paths[1],true));
    Layout_MeshPreviewCacheShutdown();for(int i=0;i<20;++i)unlink(paths[i]);rmdir(root);free(json);unsetenv("LINE_DRAWING_PREVIEW_STORE");return true;
}
static bool rapid_replacement_reset_shutdown(void) {
    char root[PATH_MAX]; TEST_ASSERT(ld_test_artifact_temp_path(root, sizeof(root), "sculpt-pass1-race-XXXXXX"));TEST_ASSERT(mkdtemp(root));
    char disabled[512],path[512];snprintf(disabled,512,"%s/missing/store",root);snprintf(path,512,"%s/mesh.json",root);
    setenv("LINE_DRAWING_PREVIEW_STORE",disabled,1);
    char* json=ld_test_read_text_file("tests/fixtures/agent_mesh_assets/asset_imported_tetrahedron_01.runtime.json");TEST_ASSERT(json);
    Layout_MeshPreviewCacheShutdown();
    for(int i=0;i<16;++i) {
        FILE* f=fopen(path,"w");TEST_ASSERT(f);fputs(json,f);fclose(f);
        TEST_ASSERT(!Layout_MeshPreviewCacheGet(path,true));
        Layout_MeshPreviewCacheReset(); /* old scene generation remains in flight */
        f=fopen(path,"a");TEST_ASSERT(f);fputs("\n",f);fclose(f);
        TEST_ASSERT(!Layout_MeshPreviewCacheGet(path,true));
        if(i%3==0)Layout_MeshPreviewCacheShutdown();
        else {TEST_ASSERT(Layout_MeshPreviewCacheSync(NULL));Layout_MeshPreviewCacheReset();}
        TEST_ASSERT(Layout_MeshPreviewCacheRead().resident_assets==0);
    }
    Layout_MeshPreviewCacheShutdown();TEST_ASSERT(!Layout_MeshPreviewCacheRead().pending_jobs);
    TEST_ASSERT(cache_wait(path,true));TEST_ASSERT(Layout_MeshPreviewCacheRead().resident_assets==1 && !Layout_MeshPreviewCacheRead().ui_decode_calls);
    Layout_MeshPreviewCacheShutdown();unlink(path);rmdir(root);free(json);unsetenv("LINE_DRAWING_PREVIEW_STORE");return true;
}
static bool conservative_bounds_lod_and_scene_budget(void) {
    SpaceViewContext v={.perspective={.enabled=true,.eye={0,0,0},.forward={0,1,0},.right={1,0,0},.up={0,0,1},
        .center={500,500},.focal=500,.tan_half_fov=1,.aspect=1,.near_clip=.01f,.far_clip=1000}};
    Grid grid={.scale=1};SDL_Rect clip={0,0,1000,1000};
    TEST_ASSERT(Layout_MeshBoundsVisible((Vec3){-1,5,-1},(Vec3){1,7,1},&v,&grid,clip));
    TEST_ASSERT(!Layout_MeshBoundsVisible((Vec3){20,5,-1},(Vec3){22,7,1},&v,&grid,clip));
    TEST_ASSERT(!Layout_MeshBoundsVisible((Vec3){-1,-7,-1},(Vec3){1,-5,1},&v,&grid,clip));
    TEST_ASSERT(Layout_MeshBoundsVisible((Vec3){-1,-1,-1},(Vec3){1,1,1},&v,&grid,clip));
    TEST_ASSERT(Layout_MeshBoundsVisible((Vec3){NAN,0,0},(Vec3){1,1,1},&v,&grid,clip));
    TEST_ASSERT(Layout_MeshScreenTier(33,0,true)==0 && Layout_MeshScreenTier(40,0,true)==1);
    TEST_ASSERT(Layout_MeshScreenTier(27,1,true)==1 && Layout_MeshScreenTier(24,1,true)==0);
    /* Rejection implies every face triangle of the conservative world box clips away. */
    const int faces[12][3]={{0,1,3},{0,3,2},{4,6,7},{4,7,5},{0,4,5},{0,5,1},{2,3,7},{2,7,6},{0,2,6},{0,6,4},{1,5,7},{1,7,3}};
    for(int n=0;n<300;++n) {
        Vec3 lo={(n%30)-15,(n%20)-10,(n%17)-8},hi=Vec3_Add(lo,(Vec3){.8f,1.2f,1.4f});
        if(Layout_MeshBoundsVisible(lo,hi,&v,&grid,clip))continue;
        Vec3 corners[8];for(int i=0;i<8;++i)corners[i]=(Vec3){i&1?hi.x:lo.x,i&2?hi.y:lo.y,i&4?hi.z:lo.z};
        for(int i=0;i<12;++i) {Vec3 out[12];TEST_ASSERT(!PerspectiveView_ClipTriangle(&v.perspective,corners[faces[i][0]],corners[faces[i][1]],corners[faces[i][2]],out));}
    }
    Layout l;Layout_Init(&l,1);uint32_t selected=0;
    for(int i=0;i<1000;++i) {
        Transform3D t=Layout_Transform3D_Default();t.position=(Vec3){(i%10)*.2f,10+(i/10)*.05f,0};
        t.rotationDeg.z=i%360;t.scale=(Vec3){i%2?2:1,.5f,1};uint32_t id;
        TEST_ASSERT(Layout_CreateMeshAssetInstanceFromRuntimeAsset(&l,"tests/fixtures/agent_mesh_assets/asset_imported_tetrahedron_01.runtime.json",&t,&id,NULL,0));
        Layout_ObjectStore_Find(&l.objectStore,id)->meshInstance.triangleCount=50000;
        if(!i)selected=id;
    }
    char* before=Layout_SaveToString(&l);
    const LayoutMeshViewPlan* p=Layout_MeshViewPlanBuild(&l,&v,&grid,clip,false,false,selected,120000);TEST_ASSERT(p);
    TEST_ASSERT(p->entries[0].tier==3 && p->planned_triangles<=120000 && !p->budget_overflow);
    p=Layout_MeshViewPlanBuild(&l,&v,&grid,clip,true,true,selected,120000);TEST_ASSERT(p && p->budget_overflow && p->entries[20].tier==3);
    char* after=Layout_SaveToString(&l);TEST_ASSERT(before && after && !strcmp(before,after));free(before);free(after);
    for(size_t i=0;i<l.objectStore.count;++i)l.objectStore.items[i].transform.position.y=-20;
    LayoutAssembly assembly={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    snprintf(assembly.info.label,sizeof(assembly.info.label),"Offscreen group");
    snprintf(assembly.info.entity_type,sizeof(assembly.info.entity_type),"Assembly");
    TEST_ASSERT(Layout_EditAssembly(&l,&assembly,NULL,NULL,NULL));
    for(size_t i=0;i<l.objectStore.count;++i)snprintf(l.objectStore.items[i].info.parent_id,64,"%s",l.objectStore.assemblies[0].id);
    p=Layout_MeshViewPlanBuild(&l,&v,&grid,clip,false,false,0,120000);TEST_ASSERT(p && p->culled_objects==1000 && p->planned_triangles==0);
    TEST_ASSERT(p->culled_assemblies==1);
    Layout_MeshViewPlanShutdown();Layout_Free(&l);return true;
}
static bool editor_mesh_hitbox_uses_native_transformed_bounds(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();Editor_ClearHistory(&s->editor);
    Transform3D transform=Layout_Transform3D_Default();
    transform.position=(Vec3){100,100,0};transform.rotationDeg.z=90;transform.scale.y=2;
    uint32_t id=0;
    TEST_ASSERT(Layout_CreateMeshAssetInstanceFromRuntimeAsset(&s->layout,
        "tests/fixtures/agent_mesh_assets/asset_imported_tetrahedron_01.runtime.json",&transform,&id,NULL,0));
    Object3D* object=Layout_ObjectStore_Find(&s->layout.objectStore,id);TEST_ASSERT(object);
    /* Native bounds deliberately differ from the runtime preview's unit tetra. */
    object->meshInstance.localBoundsMin=(Vec3){10,20,0};object->meshInstance.localBoundsMax=(Vec3){12,24,1};
    s->spaceMode=SPACE_MODE_3D;s->activePlane=(ViewPlane){.axis=VIEW_PLANE_XY};s->freeViewCamera.enabled=false;
    s->grid.scale=100;s->grid.offsetX=0;s->grid.offsetY=0;
    char* before=Layout_SaveToString(&s->layout);
    Vec2 screen=WorldToScreen((Vec2){56,111},&s->grid);
    Global_FlagHitboxesDirty();Global_RebuildHitboxesIfDirty();
    Hitbox hit=HitboxSystem_GetHitAtOfType((int)screen.x,(int)screen.y,HITBOX_OBJECT3D);
    TEST_ASSERT(hit.type==HITBOX_OBJECT3D && hit.index==(int)id);
    snprintf(object->meshInstance.runtimePath,sizeof(object->meshInstance.runtimePath),"/missing/native-bounds-are-authoritative.json");
    Global_FlagHitboxesDirty();Global_RebuildHitboxesIfDirty();
    hit=HitboxSystem_GetHitAtOfType((int)screen.x,(int)screen.y,HITBOX_OBJECT3D);
    TEST_ASSERT(hit.type==HITBOX_OBJECT3D && hit.index==(int)id);
    snprintf(object->meshInstance.runtimePath,sizeof(object->meshInstance.runtimePath),"tests/fixtures/agent_mesh_assets/asset_imported_tetrahedron_01.runtime.json");
    char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(before && after && !strcmp(before,after));
    TEST_ASSERT(Editor_UndoCount(&s->editor)==0);free(before);free(after);ld_test_shutdown_runtime();return true;
}
static bool header_click(int action) {
    LineDrawingEditorTopbar_UpdateControls();SDL_Rect r;
    if(!LDViewControls_Rect(action,&r))return false;
    SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=r.x+r.w/2;e.button.y=r.y+r.h/2;
    Input_Handle(NULL,&e);e.type=SDL_MOUSEBUTTONUP;Input_Handle(NULL,&e);return true;
}
static bool header_camera_visibility_controls(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();Editor_ClearHistory(&s->editor);LDViewControls_Close();
    Global_SetWindowSize(1280,720);UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_VIEW);
    uint32_t ids[2];for(int i=0;i<2;++i){RectPrismPrimitiveCreateParams p;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);TEST_ASSERT(Layout_CreateRectPrismPrimitive(&s->layout,&p,&ids[i],NULL));}
    LayoutAssembly assembly={.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
    snprintf(assembly.info.label,96,"Roof hardware");snprintf(assembly.info.entity_type,64,"Assembly");
    TEST_ASSERT(Layout_EditAssembly(&s->layout,&assembly,NULL,NULL,NULL));
    Object3D* child=Layout_ObjectStore_Find(&s->layout.objectStore,ids[0]);
    snprintf(child->info.parent_id,64,"%s",s->layout.objectStore.assemblies[0].id);
    snprintf(child->info.label,96,"Fastener");
    s->editor.selectedObject3DId=ids[0];
    char* before=Layout_SaveToString(&s->layout);
    for(int width=1280;width>=600;width-=340){
        Global_SetWindowSize(width,720);LineDrawingEditorTopbar_UpdateControls();SDL_Rect camera,vis,more;
        TEST_ASSERT(LDViewControls_Rect(LD_VIEW_CAMERA,&camera)&&LDViewControls_Rect(LD_VIEW_VISIBILITY,&vis)&&LDViewControls_Rect(LD_VIEW_MORE,&more));
        TEST_ASSERT(camera.x+camera.w<=vis.x && vis.x+vis.w<=more.x && more.x+more.w<=width-10);
        TEST_ASSERT(header_click(LD_VIEW_CAMERA)&&s->cameraView.active);
        SDL_Event key={.type=SDL_KEYDOWN};key.key.keysym.sym=SDLK_w;s->cameraView.focused=true;
        TEST_ASSERT(CameraView_HandleEvent(s,&key));Vec3 eye=s->cameraView.eye;TEST_ASSERT(CameraView_Step(s,.05f));TEST_ASSERT(Vec3_Distance(eye,s->cameraView.eye)>0);
        UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_VIEW);UIPanel_OnWindowResized(s->screenWidth,s->screenHeight);
        SDL_Rect sidebar;TEST_ASSERT(UIPanel_CameraControlRect(UI_CAMERA_EXIT,&sidebar));TEST_ASSERT(UIPanel_CameraClick(sidebar.x+2,sidebar.y+2));TEST_ASSERT(!s->cameraView.active);
        TEST_ASSERT(header_click(LD_VIEW_MORE));TEST_ASSERT(header_click(LD_VIEW_PLANE));TEST_ASSERT(!s->cameraView.active && !s->freeViewCamera.enabled);
        TEST_ASSERT(header_click(LD_VIEW_VISIBILITY));TEST_ASSERT(header_click(LD_VIEW_SEARCH));
        SDL_Event select={.type=SDL_KEYDOWN};select.key.keysym.sym=SDLK_a;select.key.keysym.mod=KMOD_CTRL;Input_Handle(NULL,&select);
        SDL_Event text={.type=SDL_TEXTINPUT};snprintf(text.text.text,sizeof(text.text.text),"roof");Input_Handle(NULL,&text);
        TEST_ASSERT(LDViewControls_MatchingCount()==2); /* assembly plus its child */
        TEST_ASSERT(header_click(LD_VIEW_ROW));TEST_ASSERT(!Layout_ObjectShown(&s->layout.objectStore,child));
        TEST_ASSERT(header_click(LD_VIEW_RESTORE));TEST_ASSERT(Layout_ObjectShown(&s->layout.objectStore,child));
        TEST_ASSERT(header_click(LD_VIEW_ROW_ONLY+1));TEST_ASSERT(Layout_ObjectShown(&s->layout.objectStore,child));
        TEST_ASSERT(!Layout_ObjectShown(&s->layout.objectStore,Layout_ObjectStore_Find(&s->layout.objectStore,ids[1])));
        TEST_ASSERT(header_click(LD_VIEW_RESTORE));LDViewControls_Close();
    }
    char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(before && after && !strcmp(before,after));free(after);TEST_ASSERT(Editor_UndoCount(&s->editor)==0);
    TEST_ASSERT(header_click(LD_VIEW_CAMERA));TEST_ASSERT(CameraView_Save(s,false));size_t saved=s->layout.sceneAuthoring.camera_count;
    TEST_ASSERT(saved>0);CameraView_Exit(s);TEST_ASSERT(header_click(LD_VIEW_MORE));TEST_ASSERT(header_click(LD_VIEW_SAVED));
    TEST_ASSERT(s->cameraView.active && s->cameraView.source_id[0]);CameraView_Exit(s);
    /* The camera save is the only deliberate authoring action in this test. */
    TEST_ASSERT(Editor_UndoCount(&s->editor)==1);
    free(before);LDViewControls_Close();ld_test_shutdown_runtime();return true;
}
static bool header_visibility_large_list_scroll(void) {
    ld_test_init_runtime();GlobalState* s=Global_Get();LDViewControls_Close();Global_SetWindowSize(800,600);
    uint32_t last=0;for(int i=0;i<750;++i){RectPrismPrimitiveCreateParams p;Layout_RectPrismPrimitiveCreateParams_SetDefaults(&p);TEST_ASSERT(Layout_CreateRectPrismPrimitive(&s->layout,&p,&last,NULL));}
    char* before=Layout_SaveToString(&s->layout);size_t undo=Editor_UndoCount(&s->editor);
    TEST_ASSERT(header_click(LD_VIEW_VISIBILITY));TEST_ASSERT(LDViewControls_MatchingCount()==1);TEST_ASSERT(header_click(LD_VIEW_ROW_EXPAND));TEST_ASSERT(LDViewControls_MatchingCount()==751);
    SDL_Event wheel={.type=SDL_MOUSEWHEEL};wheel.wheel.y=-10000;
#if SDL_VERSION_ATLEAST(2,26,0)
    SDL_Rect wheel_target;TEST_ASSERT(LDViewControls_Rect(LD_VIEW_SEARCH,&wheel_target));wheel.wheel.mouseX=wheel_target.x+5;wheel.wheel.mouseY=wheel_target.y+5;
#endif
    Input_Handle(NULL,&wheel);
    SDL_Rect r;int rows=0;while(LDViewControls_Rect(LD_VIEW_ROW+rows,&r))++rows;
    TEST_ASSERT(rows>0 && rows<32);TEST_ASSERT(header_click(LD_VIEW_ROW+rows-1));
    const Object3D* tail=Layout_ObjectStore_FindConst(&s->layout.objectStore,last);TEST_ASSERT(tail && !Layout_ObjectShown(&s->layout.objectStore,tail));
    TEST_ASSERT(header_click(LD_VIEW_RESTORE));TEST_ASSERT(Layout_ObjectShown(&s->layout.objectStore,tail));
    SDL_Event outside={.type=SDL_MOUSEBUTTONDOWN};outside.button.button=SDL_BUTTON_LEFT;outside.button.x=5;outside.button.y=599;
    TEST_ASSERT(!LDViewControls_Event(&outside));TEST_ASSERT(LDViewControls_Rect(LD_VIEW_RESTORE,&r));LDViewControls_Close();TEST_ASSERT(!LDViewControls_Rect(LD_VIEW_RESTORE,&r));
    char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(before && after && !strcmp(before,after) && undo==Editor_UndoCount(&s->editor));
    free(before);free(after);ld_test_shutdown_runtime();return true;
}
bool visibility_reveal_run_tests(void);
bool van_usability_run_tests(void) {
    const TestCase cases[]={{"rapid_replacement_reset_shutdown",rapid_replacement_reset_shutdown},{"header_visibility_large_list_scroll",header_visibility_large_list_scroll},{"header_camera_visibility_controls",header_camera_visibility_controls},{"editor_native_rotated_bounds",editor_mesh_hitbox_uses_native_transformed_bounds},{"guide_math_and_temporary_navigation",guides_and_navigation},{"visibility_subtree_undo",visibility_subtree_and_history},{"working_set_budget_invalidation_teardown",cache_working_set_and_budget},{"prepared_persistence_corruption_source_change",prepared_persistence_and_corruption},{"async_pressure_cancellation_replacement",asynchronous_pressure_and_scene_cancellation},{"conservative_bounds_hysteresis_scene_budget",conservative_bounds_lod_and_scene_budget}};
    bool ok=run_test_cases("VanUsability",cases,sizeof(cases)/sizeof(cases[0]));
    return visibility_reveal_run_tests() && ok;
}
