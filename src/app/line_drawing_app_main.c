#include "line_drawing/line_drawing_app_main.h"

#include <stdint.h>
#include <string.h>

typedef enum LineDrawingAppStage {
    LINE_DRAWING_APP_STAGE_INIT = 0,
    LINE_DRAWING_APP_STAGE_BOOTSTRAPPED,
    LINE_DRAWING_APP_STAGE_CONFIG_LOADED,
    LINE_DRAWING_APP_STAGE_STATE_SEEDED,
    LINE_DRAWING_APP_STAGE_SUBSYSTEMS_READY,
    LINE_DRAWING_APP_STAGE_RUNTIME_STARTED,
    LINE_DRAWING_APP_STAGE_LOOP_COMPLETED,
    LINE_DRAWING_APP_STAGE_SHUTDOWN_COMPLETED
} LineDrawingAppStage;

typedef struct LineDrawingLaunchArgs {
    int argc;
    char **argv;
} LineDrawingLaunchArgs;

typedef struct LineDrawingDispatchRequest {
    int argc;
    char **argv;
    int (*legacy_entry)(int argc, char **argv);
} LineDrawingDispatchRequest;

typedef struct LineDrawingDispatchOutcome {
    bool dispatched;
    bool used_legacy_entry;
    int exit_code;
} LineDrawingDispatchOutcome;

typedef struct LineDrawingDispatchSummary {
    uint32_t dispatch_count;
    bool dispatch_succeeded;
    int last_dispatch_exit_code;
} LineDrawingDispatchSummary;

typedef struct LineDrawingLifecycleOwnership {
    bool bootstrap_owned;
    bool config_owned;
    bool state_seed_owned;
    bool subsystems_owned;
    bool runtime_owned;
    bool dispatch_owned;
    bool shutdown_owned;
} LineDrawingLifecycleOwnership;

typedef struct LineDrawingAppContext {
    LineDrawingAppStage stage;
    int exit_code;
    int (*legacy_entry)(int argc, char **argv);
    bool (*runtime_dispatch)(const LineDrawingDispatchRequest *request,
                             LineDrawingDispatchOutcome *outcome);
    LineDrawingLaunchArgs launch_args;
    LineDrawingDispatchSummary dispatch_summary;
    LineDrawingLifecycleOwnership ownership;
} LineDrawingAppContext;

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

static bool line_drawing_default_runtime_dispatch(const LineDrawingDispatchRequest *request,
                                                  LineDrawingDispatchOutcome *outcome) {
    if (!request || !outcome || !request->legacy_entry) {
        return false;
    }
    outcome->dispatched = true;
    outcome->used_legacy_entry = true;
    outcome->exit_code = request->legacy_entry(request->argc, request->argv);
    return true;
}

static LineDrawingAppContext g_line_drawing_app_ctx = {
    .stage = LINE_DRAWING_APP_STAGE_INIT,
    .exit_code = 1,
    .legacy_entry = line_drawing_default_legacy_entry,
    .runtime_dispatch = line_drawing_default_runtime_dispatch
};

static bool line_drawing_app_transition_stage(LineDrawingAppContext *ctx,
                                              LineDrawingAppStage expected,
                                              LineDrawingAppStage next) {
    if (!ctx || ctx->stage != expected) {
        return false;
    }
    ctx->stage = next;
    return true;
}

static bool line_drawing_app_bootstrap_ctx(LineDrawingAppContext *ctx) {
    int (*legacy_entry)(int argc, char **argv) = line_drawing_default_legacy_entry;
    bool (*runtime_dispatch)(const LineDrawingDispatchRequest *request,
                             LineDrawingDispatchOutcome *outcome) =
        line_drawing_default_runtime_dispatch;
    LineDrawingLaunchArgs launch_args = {0};

    if (!ctx) {
        return false;
    }
    if (ctx->legacy_entry) {
        legacy_entry = ctx->legacy_entry;
    }
    if (ctx->runtime_dispatch) {
        runtime_dispatch = ctx->runtime_dispatch;
    }
    launch_args = ctx->launch_args;

    memset(ctx, 0, sizeof(*ctx));
    ctx->legacy_entry = legacy_entry;
    ctx->runtime_dispatch = runtime_dispatch;
    ctx->launch_args = launch_args;
    ctx->exit_code = 1;

    if (!line_drawing_app_transition_stage(ctx,
                                           LINE_DRAWING_APP_STAGE_INIT,
                                           LINE_DRAWING_APP_STAGE_BOOTSTRAPPED)) {
        return false;
    }
    ctx->ownership.bootstrap_owned = true;
    return true;
}

static bool line_drawing_app_config_load_ctx(LineDrawingAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!line_drawing_app_transition_stage(ctx,
                                           LINE_DRAWING_APP_STAGE_BOOTSTRAPPED,
                                           LINE_DRAWING_APP_STAGE_CONFIG_LOADED)) {
        return false;
    }
    ctx->ownership.config_owned = true;
    return true;
}

static bool line_drawing_app_state_seed_ctx(LineDrawingAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!line_drawing_app_transition_stage(ctx,
                                           LINE_DRAWING_APP_STAGE_CONFIG_LOADED,
                                           LINE_DRAWING_APP_STAGE_STATE_SEEDED)) {
        return false;
    }
    ctx->ownership.state_seed_owned = true;
    return true;
}

static bool line_drawing_app_subsystems_init_ctx(LineDrawingAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!line_drawing_app_transition_stage(ctx,
                                           LINE_DRAWING_APP_STAGE_STATE_SEEDED,
                                           LINE_DRAWING_APP_STAGE_SUBSYSTEMS_READY)) {
        return false;
    }
    ctx->ownership.subsystems_owned = true;
    return true;
}

static bool line_drawing_runtime_start_ctx(LineDrawingAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!line_drawing_app_transition_stage(ctx,
                                           LINE_DRAWING_APP_STAGE_SUBSYSTEMS_READY,
                                           LINE_DRAWING_APP_STAGE_RUNTIME_STARTED)) {
        return false;
    }
    ctx->ownership.runtime_owned = true;
    return true;
}

void line_drawing_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv)) {
    if (legacy_entry) {
        g_line_drawing_app_ctx.legacy_entry = legacy_entry;
    }
}

static bool line_drawing_app_dispatch_prepare_ctx(LineDrawingAppContext *ctx,
                                                  LineDrawingDispatchRequest *request) {
    if (!ctx || !request || !ctx->legacy_entry || !ctx->runtime_dispatch) {
        return false;
    }
    if (ctx->stage != LINE_DRAWING_APP_STAGE_RUNTIME_STARTED) {
        return false;
    }
    memset(request, 0, sizeof(*request));
    request->argc = ctx->launch_args.argc;
    request->argv = ctx->launch_args.argv;
    request->legacy_entry = ctx->legacy_entry;
    ctx->dispatch_summary.dispatch_count = 1u;
    ctx->ownership.dispatch_owned = true;
    return true;
}

static bool line_drawing_app_dispatch_execute_ctx(
    LineDrawingAppContext *ctx,
    const LineDrawingDispatchRequest *request,
    LineDrawingDispatchOutcome *outcome) {
    if (!ctx || !request || !outcome || !ctx->runtime_dispatch) {
        return false;
    }
    memset(outcome, 0, sizeof(*outcome));
    return ctx->runtime_dispatch(request, outcome) && outcome->dispatched;
}

static int line_drawing_app_dispatch_finalize_ctx(LineDrawingAppContext *ctx,
                                                  const LineDrawingDispatchOutcome *outcome) {
    if (!ctx || !outcome) {
        return 1;
    }
    ctx->dispatch_summary.dispatch_succeeded = true;
    ctx->dispatch_summary.last_dispatch_exit_code = outcome->exit_code;
    ctx->exit_code = outcome->exit_code;
    if (!line_drawing_app_transition_stage(ctx,
                                           LINE_DRAWING_APP_STAGE_RUNTIME_STARTED,
                                           LINE_DRAWING_APP_STAGE_LOOP_COMPLETED)) {
        return 1;
    }
    return ctx->exit_code;
}

static int line_drawing_app_run_loop_ctx(LineDrawingAppContext *ctx) {
    LineDrawingDispatchRequest request = {0};
    LineDrawingDispatchOutcome outcome = {0};
    if (!line_drawing_app_dispatch_prepare_ctx(ctx, &request)) {
        return 1;
    }
    if (!line_drawing_app_dispatch_execute_ctx(ctx, &request, &outcome)) {
        ctx->dispatch_summary.dispatch_succeeded = false;
        ctx->dispatch_summary.last_dispatch_exit_code = 1;
        return 1;
    }
    return line_drawing_app_dispatch_finalize_ctx(ctx, &outcome);
}

static void line_drawing_app_shutdown_ctx(LineDrawingAppContext *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->stage == LINE_DRAWING_APP_STAGE_SHUTDOWN_COMPLETED) {
        return;
    }
    ctx->ownership.runtime_owned = false;
    ctx->ownership.dispatch_owned = false;
    ctx->ownership.subsystems_owned = false;
    ctx->ownership.state_seed_owned = false;
    ctx->ownership.config_owned = false;
    ctx->ownership.bootstrap_owned = false;
    ctx->ownership.shutdown_owned = true;
    ctx->stage = LINE_DRAWING_APP_STAGE_SHUTDOWN_COMPLETED;
}

bool line_drawing_app_bootstrap(void) {
    return line_drawing_app_bootstrap_ctx(&g_line_drawing_app_ctx);
}

bool line_drawing_app_config_load(void) {
    return line_drawing_app_config_load_ctx(&g_line_drawing_app_ctx);
}

bool line_drawing_app_state_seed(void) {
    return line_drawing_app_state_seed_ctx(&g_line_drawing_app_ctx);
}

bool line_drawing_app_subsystems_init(void) {
    return line_drawing_app_subsystems_init_ctx(&g_line_drawing_app_ctx);
}

bool line_drawing_runtime_start(void) {
    return line_drawing_runtime_start_ctx(&g_line_drawing_app_ctx);
}

int line_drawing_app_run_loop(void) {
    return line_drawing_app_run_loop_ctx(&g_line_drawing_app_ctx);
}

void line_drawing_app_shutdown(void) {
    line_drawing_app_shutdown_ctx(&g_line_drawing_app_ctx);
}

int line_drawing_app_main(int argc, char **argv) {
    int exit_code = 1;

    g_line_drawing_app_ctx.launch_args.argc = argc;
    g_line_drawing_app_ctx.launch_args.argv = argv;
    line_drawing_app_set_legacy_entry(line_drawing_app_main_legacy);

    if (!line_drawing_app_bootstrap_ctx(&g_line_drawing_app_ctx)) {
        return exit_code;
    }
    if (!line_drawing_app_config_load_ctx(&g_line_drawing_app_ctx)) {
        goto shutdown;
    }
    if (!line_drawing_app_state_seed_ctx(&g_line_drawing_app_ctx)) {
        goto shutdown;
    }
    if (!line_drawing_app_subsystems_init_ctx(&g_line_drawing_app_ctx)) {
        goto shutdown;
    }
    if (!line_drawing_runtime_start_ctx(&g_line_drawing_app_ctx)) {
        goto shutdown;
    }
    exit_code = line_drawing_app_run_loop_ctx(&g_line_drawing_app_ctx);

shutdown:
    line_drawing_app_shutdown_ctx(&g_line_drawing_app_ctx);
    return exit_code;
}
