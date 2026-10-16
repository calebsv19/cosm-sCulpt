#include "Layout/scene/layout_mesh_source_snapshot.h"
#include "Layout/scene/layout_mesh_prepare_job.h"
#include "Layout/scene/layout_mesh_prepared_store.h"
#include "core_workers.h"
#include "core_time.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#define JOB_LIMIT 8u
#define SOURCE_LIMIT (8u*1024u*1024u)
#define RESULT_LIMIT (8u*1024u*1024u)
static CoreWorkers pool;
static pthread_t thread[1];
static CoreWorkerTask tasks[JOB_LIMIT];
static LayoutMeshPrepareJob* jobs;
static size_t count;
static pthread_t owner_thread;
bool Layout_MeshSourceStampSame(const struct stat* a,const struct stat* b) {
#ifdef __APPLE__
    return a->st_size==b->st_size && a->st_ino==b->st_ino &&
        a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec && a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec;
#else
    return a->st_size==b->st_size && a->st_ino==b->st_ino &&
        a->st_mtim.tv_sec==b->st_mtim.tv_sec && a->st_mtim.tv_nsec==b->st_mtim.tv_nsec;
#endif
}
static size_t bytes(const CoreMeshPreviewLodMesh* m) {
    return m->vertex_count*sizeof(*m->vertices)+m->triangle_count*3*sizeof(*m->indices);
}
static bool canceled(LayoutMeshPrepareJob* j) {return atomic_load(&j->canceled);}
static void* prepare(void* data) {
    LayoutMeshPrepareJob* j=data;CoreTimeNs start=core_time_now_ns();
    j->delta.background_jobs=1;j->delta.ui_decode_calls=pthread_equal(owner_thread,pthread_self())?1:0;
    char key[1024],verified[1024],input[128]={0},checked[128]={0};bool keyed=false,loaded=false;struct stat stamp;size_t result_bytes=0;
    if(canceled(j) || stat(j->path,&j->stamp) || !S_ISREG(j->stamp.st_mode))goto complete;
    if(j->stamp.st_size>SOURCE_LIMIT) {j->delta.oversized_sources=1;goto complete;}
    if(!Layout_MeshSourceSnapshot(j->path,&j->stamp,&j->canceled,input))goto complete;
    keyed=Layout_MeshPreparedStoreKey(input,key);
    if(canceled(j))goto complete;
    j->valid=keyed && Layout_MeshPreparedStoreLoad(key,j->levels);
    loaded=j->valid;
    if(loaded)j->delta.prepared_hits=1;
    else {
        if(keyed)j->delta.prepared_rebuilds=1;
        CoreMeshAssetRuntimeDocument d;core_mesh_asset_runtime_document_init(&d);
        j->delta.loads=1;
        if(!canceled(j) && core_mesh_asset_runtime_document_load_file(input,&d).code==CORE_OK) {
            j->valid=true;
            for(int i=0;i<4 && j->valid;++i) {
                ++j->delta.lod_builds;
                j->valid=!canceled(j) && core_mesh_preview_build_lod_mesh(&d,Layout_MeshPreviewTriangleLimits[i],&j->levels[i]).code==CORE_OK &&
                    j->levels[i].triangle_count<=Layout_MeshPreviewTriangleLimits[i];
            }
        }
        core_mesh_asset_runtime_document_free(&d);
    }
    for(int i=0;i<4;++i)result_bytes+=bytes(&j->levels[i]);
    if(canceled(j) || stat(j->path,&stamp) || !Layout_MeshSourceStampSame(&j->stamp,&stamp) ||
       (keyed && (!Layout_MeshSourceSnapshot(j->path,&j->stamp,&j->canceled,checked) || !Layout_MeshPreparedStoreKey(checked,verified) || strcmp(key,verified))) ||
       result_bytes>RESULT_LIMIT)j->valid=false;
    if(j->valid && keyed && !loaded && !canceled(j) && Layout_MeshPreparedStoreSave(key,j->levels))j->delta.prepared_writes=1;
complete:
    if(input[0])unlink(input);
    if(checked[0])unlink(checked);
    if(canceled(j))j->valid=false;
    if(!j->valid) {for(int i=0;i<4;++i)core_mesh_preview_lod_mesh_free(&j->levels[i]);}
    j->delta.load_ns=core_time_diff_ns(core_time_now_ns(),start);
    atomic_store_explicit(&j->done,true,memory_order_release);return NULL;
}
LayoutMeshPrepareJob* Layout_MeshPrepareSubmit(const char* path,uint64_t generation) {
    if(count>=JOB_LIMIT || !path || strlen(path)>=512)return NULL;
    if(!pool.initialized) {owner_thread=pthread_self();if(!core_workers_init(&pool,thread,1,tasks,JOB_LIMIT,NULL))return NULL;}
    LayoutMeshPrepareJob* j=calloc(1,sizeof(*j));if(!j)return NULL;
    strcpy(j->path,path);j->generation=generation;atomic_init(&j->canceled,false);atomic_init(&j->done,false);
    if(!core_workers_submit(&pool,prepare,j)) {free(j);return NULL;}
    j->next=jobs;jobs=j;++count;return j;
}
LayoutMeshPrepareJob* Layout_MeshPreparePoll(void) {
    for(LayoutMeshPrepareJob** p=&jobs;*p;p=&(*p)->next)if(atomic_load_explicit(&(*p)->done,memory_order_acquire)) {
        LayoutMeshPrepareJob* j=*p;*p=j->next;--count;return j;
    }
    return NULL;
}
void Layout_MeshPrepareFree(LayoutMeshPrepareJob* j) {
    if(!j)return;for(int i=0;i<4;++i)core_mesh_preview_lod_mesh_free(&j->levels[i]);free(j);
}
void Layout_MeshPrepareCancelAll(void) {for(LayoutMeshPrepareJob* j=jobs;j;j=j->next)atomic_store(&j->canceled,true);}
size_t Layout_MeshPreparePending(void) {return count;}
void Layout_MeshPrepareShutdown(void) {
    Layout_MeshPrepareCancelAll();if(pool.initialized)core_workers_shutdown(&pool);
    while(jobs) {LayoutMeshPrepareJob* j=jobs;jobs=j->next;Layout_MeshPrepareFree(j);}count=0;
}
