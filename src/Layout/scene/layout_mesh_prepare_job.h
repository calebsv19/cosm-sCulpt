#pragma once
#include "Layout/scene/layout_mesh_preview_cache.h"
#include "Layout/scene/layout_mesh_prepared_store.h"
#include <stdatomic.h>
#include <sys/stat.h>
typedef struct LayoutMeshPrepareJob {
    struct LayoutMeshPrepareJob* next;
    char path[512];
    struct stat stamp;
    uint64_t generation;
    atomic_bool canceled, done;
    bool valid;
    CoreMeshPreviewLodMesh levels[4];
    LayoutMeshPreviewCacheStats delta;
} LayoutMeshPrepareJob;
/* Main-thread APIs; workers write their private job until done release-publication. */
LayoutMeshPrepareJob* Layout_MeshPrepareSubmit(const char* path, uint64_t generation);
LayoutMeshPrepareJob* Layout_MeshPreparePoll(void);
void Layout_MeshPrepareCancelAll(void);
void Layout_MeshPrepareFree(LayoutMeshPrepareJob* job);
size_t Layout_MeshPreparePending(void);
void Layout_MeshPrepareShutdown(void);
bool Layout_MeshSourceStampSame(const struct stat* a,const struct stat* b);
