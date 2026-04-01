#include "line_drawing/line_drawing_app_main.h"

#include <string.h>

typedef struct LineDrawingLifecycleState {
    bool bootstrapped;
    bool config_loaded;
    bool state_seeded;
    bool subsystems_initialized;
    bool runtime_started;
    bool run_loop_completed;
    bool shutdown_completed;
    int exit_code;
} LineDrawingLifecycleState;

static LineDrawingLifecycleState g_line_drawing_lifecycle = {0};
static int g_line_drawing_launch_argc = 0;
static char **g_line_drawing_launch_argv = NULL;

__attribute__((weak)) int line_drawing_app_main_legacy(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 1;
}

static int line_drawing_default_legacy_entry(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 1;
}

static int (*g_line_drawing_legacy_entry)(int argc, char **argv) =
    line_drawing_default_legacy_entry;

bool line_drawing_app_bootstrap(void) {
    memset(&g_line_drawing_lifecycle, 0, sizeof(g_line_drawing_lifecycle));
    g_line_drawing_lifecycle.bootstrapped = true;
    return true;
}

bool line_drawing_app_config_load(void) {
    if (!g_line_drawing_lifecycle.bootstrapped) {
        return false;
    }
    g_line_drawing_lifecycle.config_loaded = true;
    return true;
}

bool line_drawing_app_state_seed(void) {
    if (!g_line_drawing_lifecycle.config_loaded) {
        return false;
    }
    g_line_drawing_lifecycle.state_seeded = true;
    return true;
}

bool line_drawing_app_subsystems_init(void) {
    if (!g_line_drawing_lifecycle.state_seeded) {
        return false;
    }
    g_line_drawing_lifecycle.subsystems_initialized = true;
    return true;
}

bool line_drawing_runtime_start(void) {
    if (!g_line_drawing_lifecycle.subsystems_initialized) {
        return false;
    }
    g_line_drawing_lifecycle.runtime_started = true;
    return true;
}

void line_drawing_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv)) {
    if (legacy_entry) {
        g_line_drawing_legacy_entry = legacy_entry;
    }
}

int line_drawing_app_run_loop(void) {
    if (!g_line_drawing_lifecycle.runtime_started) {
        return 1;
    }
    g_line_drawing_lifecycle.exit_code =
        g_line_drawing_legacy_entry(g_line_drawing_launch_argc, g_line_drawing_launch_argv);
    g_line_drawing_lifecycle.run_loop_completed = true;
    return g_line_drawing_lifecycle.exit_code;
}

void line_drawing_app_shutdown(void) {
    if (!g_line_drawing_lifecycle.bootstrapped) {
        return;
    }
    g_line_drawing_lifecycle.shutdown_completed = true;
}

int line_drawing_app_main(int argc, char **argv) {
    int exit_code = 1;

    g_line_drawing_launch_argc = argc;
    g_line_drawing_launch_argv = argv;
    line_drawing_app_set_legacy_entry(line_drawing_app_main_legacy);

    if (!line_drawing_app_bootstrap()) {
        return exit_code;
    }
    if (!line_drawing_app_config_load()) {
        line_drawing_app_shutdown();
        return exit_code;
    }
    if (!line_drawing_app_state_seed()) {
        line_drawing_app_shutdown();
        return exit_code;
    }
    if (!line_drawing_app_subsystems_init()) {
        line_drawing_app_shutdown();
        return exit_code;
    }
    if (!line_drawing_runtime_start()) {
        line_drawing_app_shutdown();
        return exit_code;
    }

    exit_code = line_drawing_app_run_loop();
    line_drawing_app_shutdown();
    return exit_code;
}
