#include "line_drawing/line_drawing_app_main.h"

#include <stdio.h>
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

typedef struct LineDrawingRuntimeLoopRequest {
    const LineDrawingDispatchRequest *dispatch_request;
    bool (*runtime_dispatch)(const LineDrawingDispatchRequest *request,
                             LineDrawingDispatchOutcome *outcome);
} LineDrawingRuntimeLoopRequest;

typedef struct LineDrawingRuntimeLoopOutcome {
    bool dispatched;
    bool used_legacy_entry;
    int exit_code;
} LineDrawingRuntimeLoopOutcome;

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
    bool run_loop_handoff_owned;
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
    int wrapper_error;
} LineDrawingAppContext;

typedef enum LineDrawingWrapperError {
    LINE_DRAWING_WRAP_OK = 0,
    LINE_DRAWING_WRAP_BOOTSTRAP_FAILED = 1,
    LINE_DRAWING_WRAP_CONFIG_LOAD_FAILED = 2,
    LINE_DRAWING_WRAP_STATE_SEED_FAILED = 3,
    LINE_DRAWING_WRAP_SUBSYSTEMS_INIT_FAILED = 4,
    LINE_DRAWING_WRAP_RUNTIME_START_FAILED = 5,
    LINE_DRAWING_WRAP_DISPATCH_PREPARE_FAILED = 6,
    LINE_DRAWING_WRAP_DISPATCH_EXECUTE_FAILED = 7,
    LINE_DRAWING_WRAP_DISPATCH_FINALIZE_FAILED = 8,
    LINE_DRAWING_WRAP_RUN_LOOP_HANDOFF_FAILED = 9
} LineDrawingWrapperError;

typedef struct LineDrawingRunLoopHandoffRequest {
    LineDrawingAppContext *ctx;
    LineDrawingDispatchRequest dispatch_request;
} LineDrawingRunLoopHandoffRequest;

typedef struct LineDrawingRunLoopHandoffOutcome {
    bool dispatched;
    bool used_legacy_entry;
    int dispatch_exit_code;
    int wrapper_exit_code;
} LineDrawingRunLoopHandoffOutcome;

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

static void line_drawing_log_stage_error(const char *fn_name,
                                         LineDrawingAppStage expected,
                                         LineDrawingAppStage actual) {
    fprintf(stderr,
            "line_drawing: lifecycle stage order violation at %s (expected=%d actual=%d)\n",
            fn_name ? fn_name : "unknown",
            (int)expected,
            (int)actual);
}

static void line_drawing_log_wrapper_error(LineDrawingWrapperError code,
                                           const char *fn_name,
                                           LineDrawingAppStage stage,
                                           const char *reason) {
    fprintf(stderr,
            "line_drawing: wrapper error code=%d fn=%s stage=%d reason=%s\n",
            (int)code,
            fn_name ? fn_name : "unknown",
            (int)stage,
            reason ? reason : "unspecified");
}

static bool line_drawing_app_transition_stage(LineDrawingAppContext *ctx,
                                              LineDrawingAppStage expected,
                                              LineDrawingAppStage next,
                                              const char *fn_name) {
    if (!ctx) {
        return false;
    }
    if (ctx->stage != expected) {
        line_drawing_log_stage_error(fn_name, expected, ctx->stage);
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
    ctx->dispatch_summary.last_dispatch_exit_code = 1;
    ctx->wrapper_error = LINE_DRAWING_WRAP_OK;

    if (!line_drawing_app_transition_stage(ctx,
                                           LINE_DRAWING_APP_STAGE_INIT,
                                           LINE_DRAWING_APP_STAGE_BOOTSTRAPPED,
                                           "line_drawing_app_bootstrap_ctx")) {
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
                                           LINE_DRAWING_APP_STAGE_CONFIG_LOADED,
                                           "line_drawing_app_config_load_ctx")) {
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
                                           LINE_DRAWING_APP_STAGE_STATE_SEEDED,
                                           "line_drawing_app_state_seed_ctx")) {
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
                                           LINE_DRAWING_APP_STAGE_SUBSYSTEMS_READY,
                                           "line_drawing_app_subsystems_init_ctx")) {
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
                                           LINE_DRAWING_APP_STAGE_RUNTIME_STARTED,
                                           "line_drawing_runtime_start_ctx")) {
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
        line_drawing_log_wrapper_error(LINE_DRAWING_WRAP_DISPATCH_PREPARE_FAILED,
                                       "line_drawing_app_dispatch_prepare_ctx",
                                       ctx ? ctx->stage : LINE_DRAWING_APP_STAGE_INIT,
                                       "invalid wrapper context");
        return false;
    }
    if (ctx->stage != LINE_DRAWING_APP_STAGE_RUNTIME_STARTED) {
        line_drawing_log_stage_error("line_drawing_app_dispatch_prepare_ctx",
                                     LINE_DRAWING_APP_STAGE_RUNTIME_STARTED,
                                     ctx->stage);
        return false;
    }
    memset(request, 0, sizeof(*request));
    request->argc = ctx->launch_args.argc;
    request->argv = ctx->launch_args.argv;
    request->legacy_entry = ctx->legacy_entry;
    ctx->dispatch_summary.dispatch_count += 1u;
    ctx->ownership.dispatch_owned = true;
    return true;
}

static bool line_drawing_app_runtime_loop_adapter(const LineDrawingRuntimeLoopRequest *request,
                                                  LineDrawingRuntimeLoopOutcome *outcome) {
    LineDrawingDispatchOutcome dispatch_outcome = {0};
    if (!request || !outcome || !request->dispatch_request || !request->runtime_dispatch) {
        return false;
    }
    if (!request->runtime_dispatch(request->dispatch_request, &dispatch_outcome) ||
        !dispatch_outcome.dispatched) {
        return false;
    }
    outcome->dispatched = true;
    outcome->used_legacy_entry = dispatch_outcome.used_legacy_entry;
    outcome->exit_code = dispatch_outcome.exit_code;
    return true;
}

static bool line_drawing_app_dispatch_execute_ctx(
    LineDrawingAppContext *ctx,
    const LineDrawingDispatchRequest *request,
    LineDrawingDispatchOutcome *outcome) {
    LineDrawingRuntimeLoopRequest loop_request = {0};
    LineDrawingRuntimeLoopOutcome loop_outcome = {0};
    if (!ctx || !request || !outcome || !ctx->runtime_dispatch) {
        line_drawing_log_wrapper_error(LINE_DRAWING_WRAP_DISPATCH_EXECUTE_FAILED,
                                       "line_drawing_app_dispatch_execute_ctx",
                                       ctx ? ctx->stage : LINE_DRAWING_APP_STAGE_INIT,
                                       "invalid wrapper context");
        return false;
    }
    memset(outcome, 0, sizeof(*outcome));
    loop_request.dispatch_request = request;
    loop_request.runtime_dispatch = ctx->runtime_dispatch;
    if (!line_drawing_app_runtime_loop_adapter(&loop_request, &loop_outcome)) {
        return false;
    }
    outcome->dispatched = loop_outcome.dispatched;
    outcome->used_legacy_entry = loop_outcome.used_legacy_entry;
    outcome->exit_code = loop_outcome.exit_code;
    return true;
}

static int line_drawing_app_dispatch_finalize_ctx(LineDrawingAppContext *ctx,
                                                  const LineDrawingDispatchOutcome *outcome) {
    if (!ctx || !outcome) {
        line_drawing_log_wrapper_error(LINE_DRAWING_WRAP_DISPATCH_FINALIZE_FAILED,
                                       "line_drawing_app_dispatch_finalize_ctx",
                                       ctx ? ctx->stage : LINE_DRAWING_APP_STAGE_INIT,
                                       "invalid dispatch outcome");
        return LINE_DRAWING_WRAP_DISPATCH_FINALIZE_FAILED;
    }
    ctx->dispatch_summary.dispatch_succeeded = true;
    ctx->dispatch_summary.last_dispatch_exit_code = outcome->exit_code;
    ctx->exit_code = outcome->exit_code;
    ctx->wrapper_error = LINE_DRAWING_WRAP_OK;
    if (!line_drawing_app_transition_stage(ctx,
                                           LINE_DRAWING_APP_STAGE_RUNTIME_STARTED,
                                           LINE_DRAWING_APP_STAGE_LOOP_COMPLETED,
                                           "line_drawing_app_dispatch_finalize_ctx")) {
        ctx->wrapper_error = LINE_DRAWING_WRAP_DISPATCH_FINALIZE_FAILED;
        line_drawing_log_wrapper_error(LINE_DRAWING_WRAP_DISPATCH_FINALIZE_FAILED,
                                       "line_drawing_app_dispatch_finalize_ctx",
                                       ctx->stage,
                                       "failed to finalize loop stage transition");
        return LINE_DRAWING_WRAP_DISPATCH_FINALIZE_FAILED;
    }
    return ctx->exit_code;
}

static bool line_drawing_app_runtime_loop_handoff_ctx(const LineDrawingRunLoopHandoffRequest *request,
                                                      LineDrawingRunLoopHandoffOutcome *outcome) {
    LineDrawingDispatchOutcome dispatch_outcome = {0};
    if (!request || !outcome || !request->ctx || !request->dispatch_request.legacy_entry) {
        if (outcome) {
            memset(outcome, 0, sizeof(*outcome));
            outcome->wrapper_exit_code = LINE_DRAWING_WRAP_RUN_LOOP_HANDOFF_FAILED;
        }
        if (request && request->ctx) {
            request->ctx->wrapper_error = LINE_DRAWING_WRAP_RUN_LOOP_HANDOFF_FAILED;
            line_drawing_log_wrapper_error(LINE_DRAWING_WRAP_RUN_LOOP_HANDOFF_FAILED,
                                           "line_drawing_app_runtime_loop_handoff_ctx",
                                           request->ctx->stage,
                                           "invalid handoff request");
        }
        return false;
    }
    memset(outcome, 0, sizeof(*outcome));
    request->ctx->ownership.run_loop_handoff_owned = true;
    if (!line_drawing_app_dispatch_execute_ctx(request->ctx, &request->dispatch_request, &dispatch_outcome)) {
        request->ctx->dispatch_summary.dispatch_succeeded = false;
        request->ctx->dispatch_summary.last_dispatch_exit_code = LINE_DRAWING_WRAP_DISPATCH_EXECUTE_FAILED;
        request->ctx->wrapper_error = LINE_DRAWING_WRAP_DISPATCH_EXECUTE_FAILED;
        line_drawing_log_wrapper_error(LINE_DRAWING_WRAP_DISPATCH_EXECUTE_FAILED,
                                       "line_drawing_app_runtime_loop_handoff_ctx",
                                       request->ctx->stage,
                                       "dispatch execute failed");
        outcome->wrapper_exit_code = LINE_DRAWING_WRAP_DISPATCH_EXECUTE_FAILED;
        return false;
    }
    outcome->dispatched = dispatch_outcome.dispatched;
    outcome->used_legacy_entry = dispatch_outcome.used_legacy_entry;
    outcome->dispatch_exit_code = dispatch_outcome.exit_code;
    outcome->wrapper_exit_code = line_drawing_app_dispatch_finalize_ctx(request->ctx, &dispatch_outcome);
    if (outcome->wrapper_exit_code != dispatch_outcome.exit_code) {
        line_drawing_log_wrapper_error(LINE_DRAWING_WRAP_DISPATCH_FINALIZE_FAILED,
                                       "line_drawing_app_runtime_loop_handoff_ctx",
                                       request->ctx->stage,
                                       "dispatch finalize reported wrapper failure");
    }
    return true;
}

static int line_drawing_app_run_loop_ctx(LineDrawingAppContext *ctx) {
    LineDrawingDispatchRequest request = {0};
    LineDrawingRunLoopHandoffRequest handoff_request = {0};
    LineDrawingRunLoopHandoffOutcome handoff_outcome = {0};
    if (!line_drawing_app_dispatch_prepare_ctx(ctx, &request)) {
        if (ctx) {
            ctx->wrapper_error = LINE_DRAWING_WRAP_DISPATCH_PREPARE_FAILED;
        }
        return LINE_DRAWING_WRAP_DISPATCH_PREPARE_FAILED;
    }

    handoff_request.ctx = ctx;
    handoff_request.dispatch_request = request;
    if (!line_drawing_app_runtime_loop_handoff_ctx(&handoff_request, &handoff_outcome)) {
        return handoff_outcome.wrapper_exit_code;
    }
    return handoff_outcome.wrapper_exit_code;
}

static void line_drawing_app_release_ownership_ctx(LineDrawingAppContext *ctx) {
    if (!ctx) {
        return;
    }
    ctx->ownership.dispatch_owned = false;
    ctx->ownership.run_loop_handoff_owned = false;
    ctx->ownership.runtime_owned = false;
    ctx->ownership.subsystems_owned = false;
    ctx->ownership.state_seed_owned = false;
    ctx->ownership.config_owned = false;
    ctx->ownership.bootstrap_owned = false;
}

static void line_drawing_app_shutdown_ctx(LineDrawingAppContext *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->stage == LINE_DRAWING_APP_STAGE_SHUTDOWN_COMPLETED) {
        return;
    }
    line_drawing_app_release_ownership_ctx(ctx);
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
    int exit_code = LINE_DRAWING_WRAP_BOOTSTRAP_FAILED;

    g_line_drawing_app_ctx.launch_args.argc = argc;
    g_line_drawing_app_ctx.launch_args.argv = argv;
    line_drawing_app_set_legacy_entry(line_drawing_app_main_legacy);

    if (!line_drawing_app_bootstrap_ctx(&g_line_drawing_app_ctx)) {
        line_drawing_log_wrapper_error(LINE_DRAWING_WRAP_BOOTSTRAP_FAILED,
                                       "line_drawing_app_main",
                                       g_line_drawing_app_ctx.stage,
                                       "bootstrap failed");
        return exit_code;
    }
    if (!line_drawing_app_config_load_ctx(&g_line_drawing_app_ctx)) {
        exit_code = LINE_DRAWING_WRAP_CONFIG_LOAD_FAILED;
        line_drawing_log_wrapper_error(LINE_DRAWING_WRAP_CONFIG_LOAD_FAILED,
                                       "line_drawing_app_main",
                                       g_line_drawing_app_ctx.stage,
                                       "config load failed");
        goto shutdown;
    }
    if (!line_drawing_app_state_seed_ctx(&g_line_drawing_app_ctx)) {
        exit_code = LINE_DRAWING_WRAP_STATE_SEED_FAILED;
        line_drawing_log_wrapper_error(LINE_DRAWING_WRAP_STATE_SEED_FAILED,
                                       "line_drawing_app_main",
                                       g_line_drawing_app_ctx.stage,
                                       "state seed failed");
        goto shutdown;
    }
    if (!line_drawing_app_subsystems_init_ctx(&g_line_drawing_app_ctx)) {
        exit_code = LINE_DRAWING_WRAP_SUBSYSTEMS_INIT_FAILED;
        line_drawing_log_wrapper_error(LINE_DRAWING_WRAP_SUBSYSTEMS_INIT_FAILED,
                                       "line_drawing_app_main",
                                       g_line_drawing_app_ctx.stage,
                                       "subsystems init failed");
        goto shutdown;
    }
    if (!line_drawing_runtime_start_ctx(&g_line_drawing_app_ctx)) {
        exit_code = LINE_DRAWING_WRAP_RUNTIME_START_FAILED;
        line_drawing_log_wrapper_error(LINE_DRAWING_WRAP_RUNTIME_START_FAILED,
                                       "line_drawing_app_main",
                                       g_line_drawing_app_ctx.stage,
                                       "runtime start failed");
        goto shutdown;
    }
    exit_code = line_drawing_app_run_loop_ctx(&g_line_drawing_app_ctx);

shutdown:
    line_drawing_app_shutdown_ctx(&g_line_drawing_app_ctx);
    fprintf(stderr,
            "line_drawing: wrapper exit stage=%d exit_code=%d dispatch_count=%u dispatch_ok=%d wrapper_error=%d\n",
            (int)g_line_drawing_app_ctx.stage,
            exit_code,
            (unsigned)g_line_drawing_app_ctx.dispatch_summary.dispatch_count,
            g_line_drawing_app_ctx.dispatch_summary.dispatch_succeeded ? 1 : 0,
            g_line_drawing_app_ctx.wrapper_error);
    return exit_code;
}
