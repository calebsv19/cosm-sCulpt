#include "Core/scene_collections_proof.h"
#include "Core/articulated_expansion_proof.h"
#include "Core/sink_support_v2_proof.h"
#include "Core/van_navigation_proof.h"
#include "Core/articulated_proof.h"
#include "Core/sink_support_proof.h"
#include "Core/header_controls_proof.h"
#include "Core/appearance_proof.h"
#include "Core/global_state.h"
#include "Core/camera_view.h"
#include "Layout/layout_json.h"
#include "Layout/layout_saved_views.h"
#include "Layout/scene/layout_mesh_solid_preview.h"
#include "Layout/scene/layout_mesh_preview_cache.h"
#include "Editor/scene_authoring_path_handles.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_shell.h"
#include "core_time.h"
#include "vk_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
static int compare(const void* a,const void* b) {double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);}
static bool frame_proof(GlobalState* s) {
    bool focus=getenv("LINE_DRAWING_NAVIGATION_PROOF_FOCUS_FIRST")!=NULL;
    if(focus && s->layout.objectStore.count)s->editor.selectedObject3DId=s->layout.objectStore.items[0].objectId;
    bool ok=CameraView_Frame(s,focus);
    s->editor.selectedObject3DId=0;return ok;
}
bool VanNavigationProof_Run(AppContext* app,const AppCallbacks* c,const char* path,const char* root) {
    if(getenv("LINE_DRAWING_SCENE_COLLECTIONS_PROOF"))return SceneCollectionsProof_Run(app,c,path,root);
    if(getenv("LINE_DRAWING_ARTICULATED_EXPANSION_PROOF"))return ArticulatedExpansionProof_Run(app,c,path,root);
    if(getenv("LINE_DRAWING_SINK_SUPPORT_V2_PROOF"))return SinkSupportV2Proof_Run(app,c,path,root);
    if(getenv("LINE_DRAWING_SINK_SUPPORT_PROOF"))return SinkSupportProof_Run(app,c,path,root);
    if(getenv("LINE_DRAWING_ARTICULATED_PROOF"))return ArticulatedProof_Run(app,c,path,root);
    GlobalState* s=Global_Get();
    const char* switches_env=getenv("LINE_DRAWING_NAVIGATION_PROOF_SWITCHES");
    int switches=switches_env?atoi(switches_env):0;double switch_worst_ms=0;
    if(switches<0 || switches>16)return false;
    for(int i=0;i<switches;++i) {
        CoreTimeNs switch_start=core_time_now_ns();
        if(!Layout_LoadFromFile(&s->layout,path))return false;
        Global_OnLayoutLoaded(path);
        if(!CameraView_Enter(s,false) || !frame_proof(s))return false;
        c->handleUpdate(app);if(!App_RenderOnce(app,c->handleRender))return false;
        double elapsed=core_time_diff_ns(core_time_now_ns(),switch_start)*1e-6;
        if(elapsed>switch_worst_ms)switch_worst_ms=elapsed;
        CameraView_Exit(s);
    }
    CoreTimeNs load_start=core_time_now_ns();
    if(!Layout_LoadFromFile(&s->layout,path))return false;
    double layout_load_ms=core_time_diff_ns(core_time_now_ns(),load_start)*1e-6;
    Global_OnLayoutLoaded(path);s->previewMode=LINE_DRAWING_PREVIEW_MODE_MATERIAL;
    UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_VIEW);
    if(!CameraView_Enter(s,false) || !frame_proof(s))return false;
    char* before=Layout_SaveToString(&s->layout);
    CoreTimeNs start=core_time_now_ns();c->handleUpdate(app);
    if(!App_RenderOnce(app,c->handleRender)) {free(before);return false;}
    double first_ms=core_time_diff_ns(core_time_now_ns(),start)*1e-6;
    double loading_frames[4096];size_t loading_count=0;size_t pending_peak=0;
    CoreTimeNs preparation_start=core_time_now_ns();
    while(Layout_MeshPreviewCacheRead().pending_jobs && loading_count<4096) {
        size_t pending=Layout_MeshPreviewCacheRead().pending_jobs;if(pending>pending_peak)pending_peak=pending;
        SDL_Event event={.type=SDL_MOUSEMOTION};event.motion.state=SDL_BUTTON_RMASK;event.motion.xrel=loading_count%2?1:-1;
        event.motion.x=(int)(s->cameraView.viewport[0]+s->cameraView.viewport[2]*.5f);
        event.motion.y=(int)(s->cameraView.viewport[1]+s->cameraView.viewport[3]*.5f);c->handleInput(app,&event);
        start=core_time_now_ns();c->handleUpdate(app);
        if(!App_RenderOnce(app,c->handleRender)) {free(before);return false;}
        loading_frames[loading_count++]=core_time_diff_ns(core_time_now_ns(),start)*1e-6;
        SDL_Delay(1);
    }
    double cold_ms=first_ms+core_time_diff_ns(core_time_now_ns(),preparation_start)*1e-6;
    LayoutMeshPreviewCacheStats cold=Layout_MeshPreviewCacheRead();
    /* Loading duration must not change the final camera used by parity runs. */
    if(!frame_proof(s)) {free(before);return false;}
    double durations[90];uint64_t raster=0,upload=0;
    for(size_t i=0;i<90;++i) {
        SDL_Event event={.type=SDL_MOUSEMOTION};
        event.motion.x=(int)(s->cameraView.viewport[0]+s->cameraView.viewport[2]*.5f);
        event.motion.y=(int)(s->cameraView.viewport[1]+s->cameraView.viewport[3]*.5f);
        event.motion.state=i%3==0?SDL_BUTTON_MMASK:SDL_BUTTON_RMASK;
        event.motion.xrel=i%2?2:-2;event.motion.yrel=i%2?1:-1;
        c->handleInput(app,&event);
        start=core_time_now_ns();c->handleUpdate(app);
        if(!App_RenderOnce(app,c->handleRender)) {free(before);return false;}
        durations[i]=core_time_diff_ns(core_time_now_ns(),start)*1e-6;
        LayoutMeshSolidPreviewFrameStats frame=Layout_MeshSolidPreviewReadStats();raster+=frame.raster_ns;upload+=frame.upload_ns;
    }
    LayoutMeshPreviewCacheStats warm=Layout_MeshPreviewCacheRead();
    LayoutMeshSolidPreviewFrameStats warm_rendering=Layout_MeshSolidPreviewReadStats();
    s->authoringGuides=true;
    (void)Layout_SceneAuthoringState_Select(&s->layout.sceneAuthoring,LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,0);
    bool guides_hidden_in_view=!SceneAuthoringPathHandles_ShouldShow(s);
    /* Selecting records is not navigation; restore native selection before comparison. */
    cJSON* original=cJSON_Parse(before);
    char* current=Layout_SaveToString(&s->layout);cJSON* after=cJSON_Parse(current);
    cJSON* old_author=cJSON_GetObjectItem(original,"sceneAuthoring");
    cJSON* new_author=cJSON_GetObjectItem(after,"sceneAuthoring");
    cJSON_DeleteItemFromObject(old_author,"selection");cJSON_DeleteItemFromObject(new_author,"selection");
    bool unchanged=original && after && cJSON_Compare(original,after,true);
    cJSON_Delete(original);cJSON_Delete(after);free(before);free(current);
    s->authoringGuides=false;Layout_SceneAuthoringState_ClearSelection(&s->layout.sceneAuthoring);
    char capture[4096];snprintf(capture,sizeof(capture),"%s/navigation.bmp",root);
    bool image=vk_renderer_request_capture(app->renderer,capture)==VK_SUCCESS && App_RenderOnce(app,c->handleRender);
    SDL_Delay(180);
    snprintf(capture,sizeof(capture),"%s/settled.bmp",root);
    image=image && vk_renderer_request_capture(app->renderer,capture)==VK_SUCCESS && App_RenderOnce(app,c->handleRender);
    bool settled_detail=!Layout_MeshSolidPreviewReadStats().interactiveQuality;
    bool camera_only=getenv("LINE_DRAWING_NAVIGATION_PROOF_CAMERA_ONLY")!=NULL;
    fprintf(stderr,"Native proof camera frames complete: warm p95 pending sort, instances %zu, planned %zu\n",s->layout.objectStore.count,warm_rendering.plannedTriangles);
    CameraView_Exit(s);
    s->spaceMode=SPACE_MODE_3D;s->freeViewCamera.enabled=true;
    s->freeViewCamera.yawDeg=-135;s->freeViewCamera.pitchDeg=-22;
    s->freeViewCamera.target=s->cameraView.orbit_target;
    s->grid.scale=30;
    s->grid.offsetX=-(s->cameraView.viewport[0]+s->cameraView.viewport[2]*.5f)/30;
    s->grid.offsetY=-(s->cameraView.viewport[1]+s->cameraView.viewport[3]*.5f)/30;
    (void)Layout_SceneAuthoringState_Select(&s->layout.sceneAuthoring,LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,0);
    s->authoringGuides=true;
    bool guides_on=SceneAuthoringPathHandles_ShouldShow(s);
    snprintf(capture,sizeof(capture),"%s/guides_on.bmp",root);
    if(!camera_only) {
        Global_FlagHitboxesDirty();c->handleUpdate(app);
        image=image && vk_renderer_request_capture(app->renderer,capture)==VK_SUCCESS && App_RenderOnce(app,c->handleRender);
    }
    s->authoringGuides=false;bool guides_off=!SceneAuthoringPathHandles_ShouldShow(s);
    snprintf(capture,sizeof(capture),"%s/guides_off.bmp",root);
    if(!camera_only) {
        Global_FlagHitboxesDirty();c->handleUpdate(app);
        image=image && vk_renderer_request_capture(app->renderer,capture)==VK_SUCCESS && App_RenderOnce(app,c->handleRender);
    }
    if(getenv("LINE_DRAWING_HEADER_CONTROLS_PROOF"))image=image && HeaderControlsProof_Run(app,c,root);
    if(getenv("LINE_DRAWING_APPEARANCE_PROOF"))image=image && AppearanceProof_Run(app,c,root);
    double mean=0;for(size_t i=0;i<90;++i)mean+=durations[i]/90;
    qsort(durations,90,sizeof(double),compare);
    struct rusage usage={0};(void)getrusage(RUSAGE_SELF,&usage);
#ifdef __APPLE__
    uint64_t peak_bytes=(uint64_t)usage.ru_maxrss;
#else
    uint64_t peak_bytes=(uint64_t)usage.ru_maxrss*1024;
#endif
    char report[4096];snprintf(report,sizeof(report),"%s/navigation.json",root);FILE* f=fopen(report,"wx");
    bool ok=unchanged && guides_hidden_in_view && guides_on && guides_off && settled_detail && image &&
        !warm.pending_jobs && warm.ui_decode_calls==0 && warm.loads==cold.loads && warm.lod_builds==cold.lod_builds;
    if(f) {
        fprintf(f,"{\"schema\":\"sculpt_navigation_proof_v1\",\"passed\":%s,\"objects\":%zu,\"frames\":90,\"cold_ms\":%.3f,\"warm_mean_ms\":%.3f,\"warm_p95_ms\":%.3f,\"raster_mean_ms\":%.3f,\"upload_mean_ms\":%.3f,\"resident_assets\":%zu,\"resident_bytes\":%zu,\"budget_bytes\":%zu,\"cold_loads\":%llu,\"warm_loads\":%llu,\"cold_lod_builds\":%llu,\"warm_lod_builds\":%llu,\"hits\":%llu,\"document_unchanged\":%s,\"guides_hidden_in_view\":%s,\"degraded_meshes\":%zu}\n",
            ok?"true":"false",s->layout.objectStore.count,cold_ms,mean,durations[85],raster/90.0*1e-6,upload/90.0*1e-6,warm.resident_assets,warm.resident_bytes,warm.budget_bytes,
            (unsigned long long)cold.loads,(unsigned long long)warm.loads,(unsigned long long)cold.lod_builds,(unsigned long long)warm.lod_builds,(unsigned long long)warm.hits,unchanged?"true":"false",guides_hidden_in_view?"true":"false",Layout_MeshSolidPreviewReadStats().degradedMeshes);
        if(fclose(f))ok=false;
        char extra[4096];snprintf(extra,sizeof(extra),"%s/preparation.json",root);
        FILE* metrics=fopen(extra,"wx");
        if(metrics) {
            qsort(loading_frames,loading_count,sizeof(double),compare);
            LayoutMeshSolidPreviewFrameStats rendering=warm_rendering;
            fprintf(metrics,"{\"prepared_hits\":%llu,\"prepared_writes\":%llu,\"prepared_rebuilds\":%llu,\"warm_p99_ms\":%.3f,\"warm_worst_ms\":%.3f,\"peak_process_resident_bytes\":%llu,\"settled_quality_restored\":%s,\"layout_load_ms\":%.3f,\"first_frame_ms\":%.3f,\"loading_frames\":%zu,\"loading_worst_ms\":%.3f,\"pending_jobs_peak\":%zu,\"ui_decode_calls\":%llu,\"background_jobs\":%llu,\"culled_objects\":%zu,\"culled_assemblies\":%zu,\"planned_triangles\":%zu,\"budget_overflow\":%s,\"raster_buffer_bytes\":%zu,\"texture_bytes_estimate\":%zu,\"planner_bytes\":%zu,\"source_checks\":%llu}\n",
                (unsigned long long)warm.prepared_hits,(unsigned long long)warm.prepared_writes,(unsigned long long)warm.prepared_rebuilds,
                durations[89],durations[89],(unsigned long long)peak_bytes,settled_detail?"true":"false",layout_load_ms,first_ms,
                loading_count,loading_count?loading_frames[loading_count-1]:0,pending_peak,(unsigned long long)warm.ui_decode_calls,
                (unsigned long long)warm.background_jobs,rendering.culledObjects,rendering.culledAssemblies,rendering.plannedTriangles,
                rendering.detailBudgetOverflow?"true":"false",rendering.rasterBufferBytes,rendering.textureBytesEstimate,
                rendering.plannerBytes,(unsigned long long)warm.source_checks);
            if(fclose(metrics))ok=false;
            snprintf(extra,sizeof(extra),"%s/lifecycle.json",root);metrics=fopen(extra,"wx");
            if(metrics) {
                int width=0,height=0;SDL_GetWindowSize(app->window,&width,&height);
                fprintf(metrics,"{\"switches\":%d,\"switch_worst_ms\":%.3f,\"camera_only\":%s,\"logical_window\":[%d,%d],\"viewport_logical\":[%.3f,%.3f,%.3f,%.3f]}\n",switches,switch_worst_ms,camera_only?"true":"false",width,height,s->cameraView.viewport[0],s->cameraView.viewport[1],s->cameraView.viewport[2],s->cameraView.viewport[3]);
                if(fclose(metrics))ok=false;
            } else ok=false;
        } else ok=false;
    } else ok=false;
    fprintf(stderr,"Native navigation: cold %.1f ms, warm mean %.1f / p95 %.1f ms, assets %zu, loads %llu -> %llu, preserved=%d\n",cold_ms,mean,durations[85],warm.resident_assets,(unsigned long long)cold.loads,(unsigned long long)warm.loads,unchanged);
    return ok;
}
