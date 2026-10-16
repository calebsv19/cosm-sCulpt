#include "Layout/scene/layout_mesh_preview_cache.h"
#include "Layout/scene/layout_mesh_prepare_job.h"
#include "Layout/scene/layout_mesh_asset_path_resolver.h"
#include "core_time.h"
#include <stdlib.h>
#include <string.h>
#define DEFAULT_BUDGET (256u*1024u*1024u)
#define BUCKETS 1024u
typedef struct Asset {
    struct Asset *next, *hash_next;
    char path[512], resolved[512];
    struct stat stamp;
    bool valid, referenced;
    uint64_t used;
    size_t bytes;
    LayoutMeshPrepareJob* job;
    CoreMeshPreviewLodMesh levels[4];
} Asset;
static Asset *assets, *buckets[BUCKETS];
static uint64_t clock_tick, generation=1, reference_signature, checked_at;
static bool reference_valid;
static LayoutMeshPreviewCacheStats stats={.budget_bytes=DEFAULT_BUDGET};
static uint64_t hash(const char* path) {
    uint64_t h=1469598103934665603ull;
    for(const unsigned char* p=(const unsigned char*)path;*p;++p)h=(h^*p)*1099511628211ull;
    return h;
}
static Asset* find(const char* path) {
    for(Asset* a=buckets[hash(path)%BUCKETS];a;a=a->hash_next)if(!strcmp(a->path,path))return a;
    return NULL;
}
static size_t bytes(const CoreMeshPreviewLodMesh* m) {
    return m->vertex_count*sizeof(*m->vertices)+m->triangle_count*3*sizeof(*m->indices);
}
static void drop(Asset** link) {
    Asset* a=*link;*link=a->next;
    if(a->job)atomic_store(&a->job->canceled,true);
    Asset** h=&buckets[hash(a->path)%BUCKETS];while(*h && *h!=a)h=&(*h)->hash_next;
    if(*h)*h=a->hash_next;
    stats.resident_bytes-=a->bytes;--stats.resident_assets;
    for(int i=0;i<4;++i)core_mesh_preview_lod_mesh_free(&a->levels[i]);free(a);
}
static void trim(size_t incoming,const Asset* keep) {
    while(assets && (incoming>stats.budget_bytes || stats.resident_bytes>stats.budget_bytes-incoming)) {
        Asset** oldest=NULL;
        for(Asset** p=&assets;*p;p=&(*p)->next)if(*p!=keep && (!oldest || (*p)->used<(*oldest)->used))oldest=p;
        if(!oldest)break;drop(oldest);++stats.evictions;
    }
}
bool Layout_MeshPreviewCachePoll(void) {
    bool changed=false;LayoutMeshPrepareJob* j;
    while((j=Layout_MeshPreparePoll())) {
        Asset* a=NULL;for(Asset* p=assets;p;p=p->next)if(p->job==j) {a=p;break;}
        struct stat stamp;
        bool stale=!a || j->generation!=generation || atomic_load(&j->canceled) ||
            stat(j->path,&stamp) || !Layout_MeshSourceStampSame(&stamp,&j->stamp) ||
            (a && !Layout_MeshSourceStampSame(&a->stamp,&j->stamp));
        if(stale) {
            ++stats.stale_results;
            if(a) {a->job=NULL;Asset** link=&assets;while(*link && *link!=a)link=&(*link)->next;if(*link)drop(link);}
        } else {
            a->job=NULL;
            stats.loads+=j->delta.loads;stats.lod_builds+=j->delta.lod_builds;stats.load_ns+=j->delta.load_ns;
            stats.prepared_hits+=j->delta.prepared_hits;stats.prepared_writes+=j->delta.prepared_writes;
            stats.prepared_rebuilds+=j->delta.prepared_rebuilds;stats.oversized_sources+=j->delta.oversized_sources;
            stats.background_jobs+=j->delta.background_jobs;stats.ui_decode_calls+=j->delta.ui_decode_calls;
            size_t incoming=0;for(int i=0;i<4;++i)incoming+=bytes(&j->levels[i]);
            if(j->valid && incoming<=stats.budget_bytes-a->bytes) {
                trim(incoming,a);for(int i=0;i<4;++i) {a->levels[i]=j->levels[i];j->levels[i]=(CoreMeshPreviewLodMesh){0};}
                a->valid=true;a->bytes+=incoming;stats.resident_bytes+=incoming;
            }
        }
        Layout_MeshPrepareFree(j);changed=true;
    }
    return changed;
}
const CoreMeshPreviewLodMesh* Layout_MeshPreviewCacheGetTier(const char* path,unsigned tier) {
    if(tier>=4 || !path || !path[0] || strlen(path)>=512)return NULL;
    Asset* a=find(path);
    if(a) {++stats.hits;a->used=++clock_tick;return a->valid ? &a->levels[tier] : NULL;}
    ++stats.misses;
    if(sizeof(Asset)>stats.budget_bytes)return NULL;
    a=calloc(1,sizeof(*a));if(!a)return NULL;
    strcpy(a->path,path);a->used=++clock_tick;a->bytes=sizeof(*a);a->referenced=true;
    if(Layout_MeshAssetResolveRuntimePath(path,a->resolved,sizeof(a->resolved))==LAYOUT_MESH_PATH_MISSING || stat(a->resolved,&a->stamp)) {free(a);return NULL;}
    a->job=Layout_MeshPrepareSubmit(a->resolved,generation);
    if(!a->job) {free(a);++stats.queue_rejections;return NULL;}
    trim(a->bytes,NULL);a->next=assets;assets=a;
    unsigned bucket=(unsigned)(hash(path)%BUCKETS);a->hash_next=buckets[bucket];buckets[bucket]=a;
    stats.resident_bytes+=a->bytes;++stats.resident_assets;
    return NULL; /* Render a proxy immediately; no decode/hash/LOD work on this thread. */
}
const CoreMeshPreviewLodMesh* Layout_MeshPreviewCacheGet(const char* path,bool interactive) {return Layout_MeshPreviewCacheGetTier(path,interactive?2:3);}
bool Layout_MeshPreviewCacheCheckChanges(void) {
    bool changed=false;checked_at=core_time_now_ns();
    for(Asset** p=&assets;*p;) {
        struct stat stamp;++stats.source_checks;
        if(stat((*p)->resolved,&stamp) || !Layout_MeshSourceStampSame(&(*p)->stamp,&stamp)) {drop(p);changed=true;}
        else p=&(*p)->next;
    }
    return changed;
}
bool Layout_MeshPreviewCacheSync(const Layout* layout) {
    bool changed=Layout_MeshPreviewCachePoll();uint64_t signature=0;
    if(layout)for(size_t i=0;i<layout->objectStore.count;++i) {
        const Object3D* o=&layout->objectStore.items[i];
        if(!o->isDeleted && o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE)signature=signature*1099511628211ull+hash(o->meshInstance.runtimePath);
    }
    if(!reference_valid || signature!=reference_signature) {
        for(Asset* a=assets;a;a=a->next)a->referenced=false;
        if(layout)for(size_t i=0;i<layout->objectStore.count;++i) {
            const Object3D* o=&layout->objectStore.items[i];
            if(!o->isDeleted && o->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE) {Asset* a=find(o->meshInstance.runtimePath);if(a)a->referenced=true;}
        }
        for(Asset** p=&assets;*p;)if(!(*p)->referenced) {drop(p);changed=true;}else p=&(*p)->next;
        reference_signature=signature;reference_valid=true;
    }
    if(core_time_diff_ns(core_time_now_ns(),checked_at)>=500000000ull)changed=Layout_MeshPreviewCacheCheckChanges() || changed;
    return changed;
}
void Layout_MeshPreviewCacheReset(void) {
    Layout_MeshPrepareCancelAll();++generation;
    while(assets)drop(&assets);
    stats=(LayoutMeshPreviewCacheStats){.budget_bytes=DEFAULT_BUDGET};clock_tick=0;reference_valid=false;checked_at=0;
}
void Layout_MeshPreviewCacheShutdown(void) {Layout_MeshPreviewCacheReset();Layout_MeshPrepareShutdown();}
void Layout_MeshPreviewCacheSetBudget(size_t budget) {Layout_MeshPreviewCacheReset();stats.budget_bytes=budget;}
LayoutMeshPreviewCacheStats Layout_MeshPreviewCacheRead(void) {
    LayoutMeshPreviewCacheStats result=stats;result.generation=generation;result.pending_jobs=Layout_MeshPreparePending();
    result.pending_result_reservation_bytes=result.pending_jobs*8u*1024u*1024u;return result;
}
