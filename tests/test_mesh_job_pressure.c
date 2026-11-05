#include "test_framework.h"
#include "Layout/scene/layout_mesh_prepare_job.h"
#include "Layout/scene/layout_mesh_source_snapshot.h"
#include <SDL2/SDL.h>
#include <string.h>

static const char mesh[] = "tests/fixtures/agent_mesh_assets/asset_imported_tetrahedron_01.runtime.json";
static atomic_bool gate_enabled, saw_cancel, timed_out;
static atomic_uint snapshot_entries;

/* Only the host test build redirects the snapshot call here. Cancellation is
 * the release condition; the watchdog makes a missed cancellation a failure. */
bool ld_test_mesh_snapshot(const char *path, const struct stat *stamp,
                           const atomic_bool *cancel, char **output) {
    if (atomic_load(&gate_enabled)) {
        atomic_fetch_add(&snapshot_entries, 1);
        Uint64 until = SDL_GetTicks64() + 5000;
        while (!atomic_load(cancel) && SDL_GetTicks64() < until) SDL_Delay(1);
        if (atomic_load(cancel)) atomic_store(&saw_cancel, true);
        else atomic_store(&timed_out, true);
    }
    return Layout_MeshSourceSnapshot(path, stamp, cancel, output);
}

static bool wait_done(LayoutMeshPrepareJob *job) {
    Uint64 until = SDL_GetTicks64() + 5000;
    while (!atomic_load_explicit(&job->done, memory_order_acquire)) {
        if (SDL_GetTicks64() >= until) return false;
        SDL_Delay(1);
    }
    return true;
}

static bool cancel_after_publication(void) {
    LayoutMeshPrepareJob *jobs[8];
    for (unsigned i = 0; i < 8; i++) {
        jobs[i] = Layout_MeshPrepareSubmit(mesh, 7);
        TEST_ASSERT(jobs[i]);
    }
    TEST_ASSERT(!Layout_MeshPrepareSubmit(mesh, 7));
    for (unsigned i = 0; i < 8; i++) TEST_ASSERT(wait_done(jobs[i]) && jobs[i]->valid);
    Layout_MeshPrepareCancelAll();
    unsigned seen = 0, usable = 0;
    LayoutMeshPrepareJob *job;
    while ((job = Layout_MeshPreparePoll())) {
        TEST_ASSERT(atomic_load(&job->canceled));
        for (unsigned i = 0; i < 8; i++) if (jobs[i] == job) {
            TEST_ASSERT(!(seen & (1u << i))); seen |= 1u << i;
            jobs[i] = NULL;
        }
        usable += job->valid;
        Layout_MeshPrepareFree(job);
    }
    Layout_MeshPrepareShutdown();
    TEST_ASSERT(seen == 255);
    TEST_ASSERT_MSG(usable == 0, "canceled completions returned as valid geometry");
    return true;
}

/* One consumed job plus one published, one active, and five queued. */
static bool fill_around_held(uint64_t generation) {
    atomic_store(&gate_enabled, false);
    LayoutMeshPrepareJob *published = Layout_MeshPrepareSubmit(mesh, generation);
    TEST_ASSERT(published && wait_done(published) && published->valid);
    atomic_store(&snapshot_entries, 0);
    atomic_store(&saw_cancel, false);
    atomic_store(&timed_out, false);
    atomic_store(&gate_enabled, true);
    for (unsigned i = 0; i < 6; i++) TEST_ASSERT(Layout_MeshPrepareSubmit(mesh, generation));
    Uint64 until = SDL_GetTicks64() + 5000;
    while (!atomic_load(&snapshot_entries) && SDL_GetTicks64() < until) SDL_Delay(1);
    return atomic_load(&snapshot_entries) == 1 && Layout_MeshPreparePending() == 7;
}

static bool keep_result_across_shutdown(void) {
    LayoutMeshPrepareJob *first = Layout_MeshPrepareSubmit(mesh, 10);
    TEST_ASSERT(first && wait_done(first));
    LayoutMeshPrepareJob *held = Layout_MeshPreparePoll();
    TEST_ASSERT(held == first && held->valid);
    CoreMeshPreviewLodMesh *lod = &held->levels[3];
    TEST_ASSERT(lod->vertex_count && lod->triangle_count);
    CoreObjectVec3 vertex = lod->vertices[0];
    uint32_t index = lod->indices[0];

    TEST_ASSERT(fill_around_held(11));
    bool refused_before = !Layout_MeshPrepareSubmit(mesh, 11);
    Layout_MeshPrepareCancelAll();
    unsigned drained = 0;
    Uint64 until = SDL_GetTicks64() + 5000;
    while (Layout_MeshPreparePending() && SDL_GetTicks64() < until) {
        LayoutMeshPrepareJob *job = Layout_MeshPreparePoll();
        if (!job) { SDL_Delay(1); continue; }
        TEST_ASSERT(atomic_load(&job->canceled) && !job->valid);
        for (unsigned tier = 0; tier < 4; tier++) TEST_ASSERT(!job->levels[tier].vertices);
        Layout_MeshPrepareFree(job);
        drained++;
    }
    TEST_ASSERT(drained == 7 && atomic_load(&saw_cancel) && !atomic_load(&timed_out));
    TEST_ASSERT(held->valid && !atomic_load(&held->canceled));
    TEST_ASSERT(fill_around_held(12));
    Layout_MeshPrepareShutdown();
    atomic_store(&gate_enabled, false);
    TEST_ASSERT(atomic_load(&saw_cancel) && !atomic_load(&timed_out));
    TEST_ASSERT(atomic_load(&snapshot_entries) == 1 && !Layout_MeshPreparePending());
    TEST_ASSERT(held->valid && held->generation == 10 && !atomic_load(&held->canceled));
    TEST_ASSERT(!memcmp(&vertex, lod->vertices, sizeof(vertex)) && lod->indices[0] == index);
    size_t held_reservation = Layout_MeshPreviewCacheRead().pending_result_reservation_bytes;

    for (unsigned i = 0; i < 7; i++) TEST_ASSERT(Layout_MeshPrepareSubmit(mesh, 12));
    bool refused_after = !Layout_MeshPrepareSubmit(mesh, 12);
    TEST_ASSERT(!memcmp(&vertex, lod->vertices, sizeof(vertex)) && lod->indices[0] == index);
    Layout_MeshPrepareFree(held);
    bool reused = Layout_MeshPrepareSubmit(mesh, 12) != NULL;
    Layout_MeshPrepareShutdown();
    for (unsigned i = 0; i < 8; i++) TEST_ASSERT(Layout_MeshPrepareSubmit(mesh, 13));
    TEST_ASSERT(!Layout_MeshPrepareSubmit(mesh, 13));
    Layout_MeshPrepareShutdown();
    TEST_ASSERT_MSG(refused_before && refused_after && reused,
                    "held result must reserve one slot until released, including across restart");
    TEST_ASSERT(held_reservation == 8u * 1024u * 1024u);
    TEST_ASSERT(Layout_MeshPreviewCacheRead().pending_result_reservation_bytes == 0);
    return true;
}

bool mesh_job_pressure_run_tests(void) {
    const TestCase cases[] = {
        {"cancel_after_publication", cancel_after_publication},
        {"keep_result_across_shutdown", keep_result_across_shutdown}
    };
    return run_test_cases("MeshJobPressure", cases, 2);
}
