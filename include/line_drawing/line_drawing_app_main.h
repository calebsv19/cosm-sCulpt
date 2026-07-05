#ifndef LINE_DRAWING_LINE_DRAWING_APP_MAIN_H
#define LINE_DRAWING_LINE_DRAWING_APP_MAIN_H

#include <stdbool.h>
#include <stddef.h>

typedef enum LineDrawingAppDiagnosticStage {
    LINE_DRAWING_APP_DIAGNOSTIC_STAGE_BOOTSTRAP = 0,
    LINE_DRAWING_APP_DIAGNOSTIC_STAGE_CONFIG_LOAD,
    LINE_DRAWING_APP_DIAGNOSTIC_STAGE_STATE_SEED,
    LINE_DRAWING_APP_DIAGNOSTIC_STAGE_SUBSYSTEMS_INIT,
    LINE_DRAWING_APP_DIAGNOSTIC_STAGE_RUNTIME_START,
    LINE_DRAWING_APP_DIAGNOSTIC_STAGE_RUN_LOOP_HANDOFF,
    LINE_DRAWING_APP_DIAGNOSTIC_STAGE_SHUTDOWN
} LineDrawingAppDiagnosticStage;

typedef struct LineDrawingAppStageDiagnostic {
    LineDrawingAppDiagnosticStage stage;
    const char *stage_name;
    const char *wrapper_role;
    const char *owner_note;
    bool owns_runtime_data_roots;
    bool owns_recent_contexts;
    bool owns_file_browser_restore;
    bool owns_package_runtime_setup;
} LineDrawingAppStageDiagnostic;

bool line_drawing_app_bootstrap(void);
bool line_drawing_app_config_load(void);
bool line_drawing_app_state_seed(void);
bool line_drawing_app_subsystems_init(void);
bool line_drawing_runtime_start(void);
void line_drawing_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv));
int line_drawing_app_run_loop(void);
void line_drawing_app_shutdown(void);

size_t line_drawing_app_stage_diagnostics_count(void);
const LineDrawingAppStageDiagnostic *line_drawing_app_stage_diagnostic_at(size_t index);

int line_drawing_app_main(int argc, char **argv);
int line_drawing_app_main_legacy(int argc, char **argv);

#endif
