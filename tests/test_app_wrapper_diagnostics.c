#include "test_framework.h"

#include "line_drawing/line_drawing_app_main.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static bool diagnostic_has_name(const char *name) {
    const size_t count = line_drawing_app_stage_diagnostics_count();
    for (size_t i = 0; i < count; ++i) {
        const LineDrawingAppStageDiagnostic *diag =
            line_drawing_app_stage_diagnostic_at(i);
        if (diag && diag->stage_name && strcmp(diag->stage_name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool test_stage_diagnostics_cover_wrapper_sequence(void) {
    TEST_ASSERT(line_drawing_app_stage_diagnostics_count() == 7u);
    TEST_ASSERT(diagnostic_has_name("bootstrap"));
    TEST_ASSERT(diagnostic_has_name("config_load"));
    TEST_ASSERT(diagnostic_has_name("state_seed"));
    TEST_ASSERT(diagnostic_has_name("subsystems_init"));
    TEST_ASSERT(diagnostic_has_name("runtime_start"));
    TEST_ASSERT(diagnostic_has_name("run_loop_handoff"));
    TEST_ASSERT(diagnostic_has_name("shutdown"));
    return true;
}

static bool test_config_adjacent_stages_do_not_claim_runtime_state_ownership(void) {
    const size_t count = line_drawing_app_stage_diagnostics_count();
    bool saw_config_load = false;
    bool saw_state_seed = false;

    for (size_t i = 0; i < count; ++i) {
        const LineDrawingAppStageDiagnostic *diag =
            line_drawing_app_stage_diagnostic_at(i);
        TEST_ASSERT(diag != NULL);
        TEST_ASSERT(diag->stage_name != NULL);
        TEST_ASSERT(diag->wrapper_role != NULL);
        TEST_ASSERT(diag->owner_note != NULL);
        TEST_ASSERT(!diag->owns_runtime_data_roots);
        TEST_ASSERT(!diag->owns_recent_contexts);
        TEST_ASSERT(!diag->owns_file_browser_restore);
        TEST_ASSERT(!diag->owns_package_runtime_setup);

        if (strcmp(diag->stage_name, "config_load") == 0) {
            saw_config_load = true;
            TEST_ASSERT(strstr(diag->owner_note, "Core") != NULL);
            TEST_ASSERT(strstr(diag->owner_note, "data") != NULL);
        } else if (strcmp(diag->stage_name, "state_seed") == 0) {
            saw_state_seed = true;
            TEST_ASSERT(strstr(diag->owner_note, "recent") != NULL);
            TEST_ASSERT(strstr(diag->owner_note, "File-browser") != NULL);
        }
    }

    TEST_ASSERT(saw_config_load);
    TEST_ASSERT(saw_state_seed);
    return true;
}

static bool test_stage_diagnostic_index_bounds(void) {
    TEST_ASSERT(line_drawing_app_stage_diagnostic_at(
                    line_drawing_app_stage_diagnostics_count()) == NULL);
    return true;
}

bool app_wrapper_diagnostics_run_tests(void) {
    const TestCase cases[] = {
        {"stage_diagnostics_cover_wrapper_sequence",
         test_stage_diagnostics_cover_wrapper_sequence},
        {"config_adjacent_stages_do_not_claim_runtime_state_ownership",
         test_config_adjacent_stages_do_not_claim_runtime_state_ownership},
        {"stage_diagnostic_index_bounds", test_stage_diagnostic_index_bounds},
    };
    return run_test_cases("AppWrapperDiagnostics",
                          cases,
                          sizeof(cases) / sizeof(cases[0]));
}
