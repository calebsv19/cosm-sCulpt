#pragma once
#include "Layout/layout.h"
#include "core_mesh_preview.h"
/* App policy: dynamically retained indexed LODs, 256 MiB resident budget. */
typedef struct {
    size_t resident_bytes, resident_assets, budget_bytes;
    uint64_t hits, misses, loads, lod_builds, evictions, load_ns;
    uint64_t prepared_hits, prepared_writes, prepared_rebuilds, oversized_sources;
    uint64_t generation, background_jobs, ui_decode_calls, stale_results, queue_rejections, source_checks;
    size_t pending_jobs, pending_result_reservation_bytes;
} LayoutMeshPreviewCacheStats;
/* Borrow for immediate owner-thread use only, before any call that can evict,
 * reset or shut down the cache. These pointers are not polled job handles. */
const CoreMeshPreviewLodMesh* Layout_MeshPreviewCacheGetTier(const char* path, unsigned tier);
const CoreMeshPreviewLodMesh* Layout_MeshPreviewCacheGet(const char* path, bool interactive);
/* Drop unreferenced scene assets and detect replacement/removal, even at rest. */
bool Layout_MeshPreviewCacheSync(const Layout* layout);
bool Layout_MeshPreviewCachePoll(void);
bool Layout_MeshPreviewCacheCheckChanges(void);
void Layout_MeshPreviewCacheShutdown(void);
void Layout_MeshPreviewCacheReset(void);
LayoutMeshPreviewCacheStats Layout_MeshPreviewCacheRead(void);
/* Bounded test/diagnostic override; reset discards all cache data and counters. */
void Layout_MeshPreviewCacheSetBudget(size_t bytes);
