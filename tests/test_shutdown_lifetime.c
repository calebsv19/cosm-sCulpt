#include "test_framework.h"

#include "Core/global_state.h"
#include "UI/ui_panel.h"

#include "core_mesh_compile.h"

#include <SDL.h>

static int shutdown_lifetime_noop_thread(void* data) {
    (void)data;
    return 0;
}

static bool test_global_shutdown_is_noop_before_init(void) {
    Global_Shutdown();
    TEST_ASSERT(Global_Get() == NULL);
    return true;
}

static bool test_global_shutdown_is_idempotent_after_init(void) {
    Global_Init(800, 600);
    TEST_ASSERT(Global_Get() != NULL);

    Global_Shutdown();
    TEST_ASSERT(Global_Get() == NULL);

    Global_Shutdown();
    TEST_ASSERT(Global_Get() == NULL);
    return true;
}

static bool test_global_shutdown_waits_for_async_stl_import_worker(void) {
    UIPanelState* ui = NULL;

    Global_Init(800, 600);
    ui = UIPanel_Get();
    TEST_ASSERT(Global_Get() != NULL);
    TEST_ASSERT(ui != NULL);

    ui->loadMenu.asyncStlActive = true;
    SDL_AtomicSet(&ui->loadMenu.asyncStlComplete, 1);
    SDL_AtomicSet(&ui->loadMenu.asyncStlProgressPermille, 500);
    SDL_AtomicSet(&ui->loadMenu.asyncStlProgressStage,
                  (int)CORE_MESH_COMPILE_PROGRESS_STAGE_PARSING_STL);
    ui->loadMenu.asyncStlThread = SDL_CreateThread(shutdown_lifetime_noop_thread,
                                                   "ld-test-stl-worker",
                                                   NULL);
    TEST_ASSERT(ui->loadMenu.asyncStlThread != NULL);

    Global_Shutdown();
    TEST_ASSERT(Global_Get() == NULL);
    TEST_ASSERT(ui->loadMenu.asyncStlThread == NULL);
    TEST_ASSERT(!ui->loadMenu.asyncStlActive);
    TEST_ASSERT(SDL_AtomicGet(&ui->loadMenu.asyncStlComplete) == 0);
    TEST_ASSERT(SDL_AtomicGet(&ui->loadMenu.asyncStlProgressPermille) == 0);
    TEST_ASSERT(SDL_AtomicGet(&ui->loadMenu.asyncStlProgressStage) ==
                (int)CORE_MESH_COMPILE_PROGRESS_STAGE_UNKNOWN);
    return true;
}

bool shutdown_lifetime_run_tests(void) {
    const TestCase cases[] = {
        {"global shutdown is noop before init", test_global_shutdown_is_noop_before_init},
        {"global shutdown is idempotent after init", test_global_shutdown_is_idempotent_after_init},
        {"global shutdown waits for async stl import worker",
         test_global_shutdown_waits_for_async_stl_import_worker},
    };
    return run_test_cases("ShutdownLifetime",
                          cases,
                          sizeof(cases) / sizeof(cases[0]));
}
