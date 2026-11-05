#include "test_framework.h"
#include "test_artifact_helpers.h"
#include "Layout/scene/layout_mesh_prepare_job.h"
#include "Layout/scene/layout_mesh_prepared_store.h"
#include "core_workers.h"
#include "core_pack.h"
#include <SDL2/SDL.h>
#include <dirent.h>
#include <errno.h>
#include <stdatomic.h>

/* Only the host runner links these adapters. No process-global allocator hook. */
static atomic_bool fail_job, fail_submit, fail_snapshot, fail_pack, fail_rename;
static atomic_int allocation_countdown, lod_countdown, write_countdown;
static atomic_int publication_gate; /* 1 before rename, 2 after rename */
static atomic_bool reached, release_gate, gate_timeout;
static const char *source = "tests/fixtures/agent_mesh_assets/asset_imported_tetrahedron_01.runtime.json";
static char root[1024], key[1024];

void *ld_proof_calloc(size_t n, size_t size) {
    return atomic_exchange(&fail_job, false) ? NULL : calloc(n, size);
}
bool ld_proof_submit(CoreWorkers *pool, CoreWorkerTaskFn fn, void *ctx) {
    return !atomic_exchange(&fail_submit, false) && core_workers_submit(pool, fn, ctx);
}
void *ld_proof_snapshot_malloc(size_t size) {
    return atomic_exchange(&fail_snapshot, false) ? NULL : malloc(size);
}
void *ld_proof_store_malloc(size_t size) {
    int n = atomic_load(&allocation_countdown);
    if (n > 0 && atomic_fetch_sub(&allocation_countdown, 1) == 1) return NULL;
    return malloc(size);
}
void *ld_proof_pack_realloc(void *ptr, size_t size) {
    return atomic_exchange(&fail_pack, false) ? NULL : core_realloc(ptr, size);
}
void *ld_proof_lod_calloc(size_t n, size_t size) {
    int remaining = atomic_load(&lod_countdown);
    if (remaining > 0 && atomic_fetch_sub(&lod_countdown, 1) == 1) return NULL;
    return calloc(n, size);
}
size_t ld_proof_write(const void *ptr, size_t size, size_t n, FILE *file) {
    int remaining = atomic_load(&write_countdown);
    if (remaining > 0 && atomic_fetch_sub(&write_countdown, 1) == 1) { errno = ENOSPC; return 0; }
    return fwrite(ptr, size, n, file);
}
static void barrier(void) {
    atomic_store(&reached, true);
    Uint64 until = SDL_GetTicks64() + 5000;
    while (!atomic_load(&release_gate) && SDL_GetTicks64() < until) SDL_Delay(1);
    if (!atomic_load(&release_gate)) atomic_store(&gate_timeout, true);
}
int ld_proof_rename(const char *from, const char *to) {
    if (atomic_load(&publication_gate) == 1) barrier();
    if (atomic_exchange(&fail_rename, false)) { errno = EIO; return -1; }
    int result = rename(from, to);
    if (atomic_load(&publication_gate) == 2) barrier();
    return result;
}
static bool done(LayoutMeshPrepareJob *job) {
    Uint64 until = SDL_GetTicks64() + 5000;
    while (!atomic_load_explicit(&job->done, memory_order_acquire) && SDL_GetTicks64() < until) SDL_Delay(1);
    return atomic_load_explicit(&job->done, memory_order_acquire);
}
static bool setup(void) {
    Layout_MeshPrepareShutdown();
    TEST_ASSERT(ld_test_artifact_temp_path(root, sizeof(root), "sculpt-failure-XXXXXX"));
    TEST_ASSERT(mkdtemp(root));
    TEST_ASSERT(!setenv("LINE_DRAWING_PREVIEW_STORE", root, 1));
    TEST_ASSERT(Layout_MeshPreparedStoreKey(source, key));
    return true;
}
static bool files(unsigned expected) {
    DIR *d = opendir(root); TEST_ASSERT(d); unsigned count = 0; struct dirent *e;
    while ((e = readdir(d))) if (e->d_name[0] != '.') count++;
    closedir(d); TEST_ASSERT(count == expected); return true;
}
static bool finish(void) {
    Layout_MeshPrepareShutdown();
    TEST_ASSERT(!Layout_MeshPreparePending() && !Layout_MeshPrepareReservedBytes());
    unlink(key); TEST_ASSERT(files(0)); TEST_ASSERT(!rmdir(root));
    unsetenv("LINE_DRAWING_PREVIEW_STORE"); return true;
}
static bool reusable(void) {
    for (unsigned i = 0; i < 8; i++) TEST_ASSERT(Layout_MeshPrepareSubmit(source, 99));
    TEST_ASSERT(!Layout_MeshPrepareSubmit(source, 99));
    Layout_MeshPrepareShutdown();
    TEST_ASSERT(!Layout_MeshPrepareReservedBytes()); return true;
}
static bool admission_and_input_failure(void) {
    TEST_ASSERT(setup());
    LayoutMeshPrepareJob *prior = Layout_MeshPrepareSubmit(source, 1);
    TEST_ASSERT(prior && done(prior) && prior->valid);
    atomic_store(&fail_job, true); TEST_ASSERT(!Layout_MeshPrepareSubmit(source, 2));
    atomic_store(&fail_submit, true); TEST_ASSERT(!Layout_MeshPrepareSubmit(source, 2));
    TEST_ASSERT(Layout_MeshPreparePending() == 1 && Layout_MeshPrepareReservedBytes() == 8u*1024u*1024u);
    atomic_store(&fail_snapshot, true);
    LayoutMeshPrepareJob *failed = Layout_MeshPrepareSubmit(source, 3);
    TEST_ASSERT(failed && done(failed) && !failed->valid);
    TEST_ASSERT(Layout_MeshPreparePoll() == failed); Layout_MeshPrepareFree(failed);
    TEST_ASSERT(Layout_MeshPreparePoll() == prior && prior->valid && prior->generation == 1);
    Layout_MeshPrepareFree(prior); TEST_ASSERT(!Layout_MeshPreparePoll());
    TEST_ASSERT(reusable()); return finish();
}
static bool result_allocation_failure(void) {
    TEST_ASSERT(setup());
    LayoutMeshPrepareJob *job = Layout_MeshPrepareSubmit(source, 1);
    TEST_ASSERT(job && done(job) && job->valid && job->delta.prepared_writes == 1);
    TEST_ASSERT(Layout_MeshPreparePoll() == job);
    for (int i = 1; i <= 8; i++) {
        CoreMeshPreviewLodMesh levels[4] = {0};
        atomic_store(&allocation_countdown, i);
        TEST_ASSERT(!Layout_MeshPreparedStoreLoad(key, levels));
        atomic_store(&allocation_countdown, 0);
        for (int j = 0; j < 4; j++) TEST_ASSERT(!levels[j].vertices && !levels[j].indices);
        TEST_ASSERT(job->valid && job->levels[3].vertices && Layout_MeshPrepareReservedBytes() == 8u*1024u*1024u);
    }
    /* The real worker falls back to rebuilding after a cache allocation refusal. */
    atomic_store(&allocation_countdown, 2); /* first is source key, second first vertex buffer */
    LayoutMeshPrepareJob *recovered = Layout_MeshPrepareSubmit(source, 2);
    TEST_ASSERT(recovered && done(recovered) && recovered->valid && recovered->delta.prepared_hits == 0);
    TEST_ASSERT(Layout_MeshPreparePoll() == recovered); Layout_MeshPrepareFree(recovered);
    Layout_MeshPrepareFree(job);
    /* A small tetrahedron uses two result allocations per tier, four tiers. */
    for (int i = 1; i <= 8; i++) {
        unlink(key); atomic_store(&lod_countdown, i);
        LayoutMeshPrepareJob *failed = Layout_MeshPrepareSubmit(source, 3);
        TEST_ASSERT(failed && done(failed) && !failed->valid);
        atomic_store(&lod_countdown, 0);
        TEST_ASSERT(Layout_MeshPreparePoll() == failed);
        for (int j = 0; j < 4; j++) TEST_ASSERT(!failed->levels[j].vertices && !failed->levels[j].indices);
        Layout_MeshPrepareFree(failed); TEST_ASSERT(files(0));
    }
    TEST_ASSERT(reusable()); return finish();
}
static bool publication_failure_and_cancel(void) {
    TEST_ASSERT(setup());
    for (int mode = 0; mode < 5; mode++) {
        bool cancel = mode == 2 || mode == 3;
        unlink(key);
        atomic_store(&fail_pack, mode == 0); atomic_store(&write_countdown, mode == 4 ? 3 : 0); atomic_store(&fail_rename, mode == 1);
        atomic_store(&publication_gate, cancel ? mode-1 : 0);
        atomic_store(&reached, false); atomic_store(&release_gate, false); atomic_store(&gate_timeout, false);
        LayoutMeshPrepareJob *job = Layout_MeshPrepareSubmit(source, (uint64_t)mode);
        TEST_ASSERT(job);
        if (cancel) {
            Uint64 until = SDL_GetTicks64() + 5000;
            while (!atomic_load(&reached) && SDL_GetTicks64() < until) SDL_Delay(1);
            TEST_ASSERT(atomic_load(&reached));
            TEST_ASSERT((access(key, F_OK) == 0) == (mode == 3));
            Layout_MeshPrepareCancelAll(); atomic_store(&release_gate, true);
        }
        TEST_ASSERT(done(job) && !atomic_load(&gate_timeout));
        atomic_store(&publication_gate, 0);
        TEST_ASSERT(Layout_MeshPreparePoll() == job);
        TEST_ASSERT(job->valid == (!cancel));
        TEST_ASSERT(job->delta.prepared_writes == (cancel ? 1u : 0u));
        CoreMeshPreviewLodMesh levels[4] = {0};
        TEST_ASSERT(Layout_MeshPreparedStoreLoad(key, levels) == cancel);
        for (int i = 0; i < 4; i++) core_mesh_preview_lod_mesh_free(&levels[i]);
        Layout_MeshPrepareFree(job); TEST_ASSERT(!Layout_MeshPreparePoll());
        TEST_ASSERT(files(cancel ? 1 : 0));
        /* Recovery consumes the same source identity without any stranded slot. */
        job = Layout_MeshPrepareSubmit(source, 100);
        TEST_ASSERT(job && done(job) && job->valid);
        TEST_ASSERT(job->delta.prepared_hits == (cancel ? 1u : 0u));
        TEST_ASSERT(Layout_MeshPreparePoll() == job); Layout_MeshPrepareFree(job);
    }
    TEST_ASSERT(reusable()); return finish();
}
bool mesh_failure_run_tests(void) {
    const TestCase cases[] = {
        {"admission_and_input_failure", admission_and_input_failure},
        {"result_allocation_failure", result_allocation_failure},
        {"publication_failure_and_cancel", publication_failure_and_cancel}
    };
    return run_test_cases("MeshFailure", cases, 3);
}
