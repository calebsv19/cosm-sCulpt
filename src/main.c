// src/main.c
#include "line_drawing/line_drawing_app_main.h"
#include "Core/SDLApp/sdl_app_framework.h"
#include "Layout/Grid/grid.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/ui_panel.h"


#include "Input/input_handler.h"
#include "Input/input_routing_policy.h"
#include "Render/render_handler.h"
#include "Core/global_state.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>




#define DEFAULT_WINDOW_WIDTH  1280
#define DEFAULT_WINDOW_HEIGHT 720

typedef struct LineDrawingInputEventRaw {
    bool valid;
    SDL_Event event;
} LineDrawingInputEventRaw;

typedef struct LineDrawingInputEventNormalized {
    LineDrawingInputActionClass action_class;
    LineDrawingInputRoutePolicy route_policy;
    bool text_entry_capture;
    bool global_shortcuts_allowed;
    bool global_shortcut_blocked;
    bool request_quit;
    SDL_Event event;
} LineDrawingInputEventNormalized;

typedef struct LineDrawingInputRouteResult {
    bool consumed;
    bool requested_target_invalidation;
    bool requested_full_invalidation;
    uint32_t invalidation_reason_bits;
} LineDrawingInputRouteResult;

typedef struct LineDrawingInputDiagTotals {
    uint64_t raw_event_count;
    uint64_t normalized_count;
    uint64_t ignored_count;
    uint64_t immediate_count;
    uint64_t queued_count;
    uint64_t routed_global_count;
    uint64_t routed_focused_count;
    uint64_t blocked_global_shortcut_count;
    uint64_t target_invalidation_count;
    uint64_t full_invalidation_count;
} LineDrawingInputDiagTotals;

typedef struct LineDrawingRs1DiagTotals {
    uint64_t frame_count;
    uint64_t derive_ns_total;
    uint64_t submit_ns_total;
    uint64_t invalidation_reason_bits_consumed_total;
} LineDrawingRs1DiagTotals;

enum {
    LINE_DRAWING_INPUT_INVALIDATION_REASON_ACTION = 1u << 0,
    LINE_DRAWING_INPUT_INVALIDATION_REASON_EXIT = 1u << 1
};

static LineDrawingInputDiagTotals g_line_drawing_input_diag_totals = {0};
static LineDrawingUpdateFrame g_line_drawing_update_frame = {0};
static uint32_t g_line_drawing_pending_invalidation_bits = 0u;
static LineDrawingRs1DiagTotals g_line_drawing_rs1_diag_totals = {0};

static bool LineDrawingInputDiagEnabled(void) {
    const char* value = getenv("LINE_DRAWING_INPUT_DIAG");
    if (!value || !value[0]) return false;
    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "TRUE") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "on") == 0;
}

static bool LineDrawingRs1DiagEnabled(void) {
    const char* value = getenv("LINE_DRAWING_RS1_DIAG");
    if (!value || !value[0]) return false;
    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "TRUE") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "on") == 0;
}

static void LineDrawingInputIntake(const SDL_Event* event, LineDrawingInputEventRaw* out_raw) {
    if (!out_raw) return;
    memset(out_raw, 0, sizeof(*out_raw));
    if (!event) return;
    out_raw->valid = true;
    out_raw->event = *event;
}

static void LineDrawingInputNormalize(const LineDrawingInputEventRaw* raw,
                                      LineDrawingInputEventNormalized* out_normalized) {
    if (!out_normalized) return;
    memset(out_normalized, 0, sizeof(*out_normalized));
    if (!raw || !raw->valid) return;

    out_normalized->event = raw->event;
    {
        const bool text_entry_capture = UIPanel_IsCapturingKeyboard();
        const LineDrawingInputPolicyDecision policy =
            LineDrawingInputPolicyNormalize(&raw->event, text_entry_capture);
        out_normalized->action_class = policy.action_class;
        out_normalized->route_policy = policy.route_policy;
        out_normalized->text_entry_capture = policy.text_entry_capture;
        out_normalized->global_shortcuts_allowed = policy.global_shortcuts_allowed;
        out_normalized->global_shortcut_blocked = policy.global_shortcut_blocked;
        out_normalized->request_quit = policy.request_quit;
    }
}

static void LineDrawingInputRoute(AppContext* ctx,
                                  const LineDrawingInputEventNormalized* normalized,
                                  LineDrawingInputRouteResult* out_result) {
    if (!out_result) return;
    memset(out_result, 0, sizeof(*out_result));
    if (!ctx || !normalized) return;

    if (normalized->action_class == LINE_DRAWING_INPUT_ACTION_IGNORED) {
        return;
    }

    if (normalized->route_policy == LINE_DRAWING_INPUT_ROUTE_GLOBAL) {
        if (normalized->request_quit) {
            ctx->quit = true;
            out_result->consumed = true;
            out_result->requested_full_invalidation = true;
            out_result->invalidation_reason_bits |= LINE_DRAWING_INPUT_INVALIDATION_REASON_EXIT;
        }
        return;
    }

    if (normalized->route_policy == LINE_DRAWING_INPUT_ROUTE_FOCUSED ||
        normalized->route_policy == LINE_DRAWING_INPUT_ROUTE_FALLBACK) {
        SDL_Event mutable_event = normalized->event;
        Input_Handle(ctx, &mutable_event);
        out_result->consumed = true;
        out_result->requested_target_invalidation = true;
        out_result->invalidation_reason_bits |= LINE_DRAWING_INPUT_INVALIDATION_REASON_ACTION;
    }
}

static void LineDrawingInputInvalidate(const LineDrawingInputEventNormalized* normalized,
                                       LineDrawingInputRouteResult* result) {
    if (!normalized || !result) return;
    if (normalized->action_class == LINE_DRAWING_INPUT_ACTION_IGNORED) {
        return;
    }
    if (!result->requested_target_invalidation && !result->requested_full_invalidation) {
        result->requested_target_invalidation = true;
        result->invalidation_reason_bits |= LINE_DRAWING_INPUT_INVALIDATION_REASON_ACTION;
    }
}

static void LineDrawingInputDiagAccumulate(const LineDrawingInputEventRaw* raw,
                                           const LineDrawingInputEventNormalized* normalized,
                                           const LineDrawingInputRouteResult* route) {
    if (!raw || !normalized || !route) return;
    if (raw->valid) g_line_drawing_input_diag_totals.raw_event_count += 1u;
    if (normalized->action_class != LINE_DRAWING_INPUT_ACTION_IGNORED) {
        g_line_drawing_input_diag_totals.normalized_count += 1u;
    }
    if (normalized->action_class == LINE_DRAWING_INPUT_ACTION_IGNORED) {
        g_line_drawing_input_diag_totals.ignored_count += 1u;
    } else if (normalized->action_class == LINE_DRAWING_INPUT_ACTION_IMMEDIATE) {
        g_line_drawing_input_diag_totals.immediate_count += 1u;
    } else if (normalized->action_class == LINE_DRAWING_INPUT_ACTION_QUEUED) {
        g_line_drawing_input_diag_totals.queued_count += 1u;
    }
    if (normalized->route_policy == LINE_DRAWING_INPUT_ROUTE_GLOBAL && route->consumed) {
        g_line_drawing_input_diag_totals.routed_global_count += 1u;
    }
    if ((normalized->route_policy == LINE_DRAWING_INPUT_ROUTE_FOCUSED ||
         normalized->route_policy == LINE_DRAWING_INPUT_ROUTE_FALLBACK) && route->consumed) {
        g_line_drawing_input_diag_totals.routed_focused_count += 1u;
    }
    if (normalized->global_shortcut_blocked) {
        g_line_drawing_input_diag_totals.blocked_global_shortcut_count += 1u;
    }
    if (route->requested_target_invalidation) {
        g_line_drawing_input_diag_totals.target_invalidation_count += 1u;
    }
    if (route->requested_full_invalidation) {
        g_line_drawing_input_diag_totals.full_invalidation_count += 1u;
    }
}

static void LineDrawingInputDiagMaybeEmit(const LineDrawingInputEventNormalized* normalized,
                                          const LineDrawingInputRouteResult* route) {
    if (!normalized || !route) return;
    if (!LineDrawingInputDiagEnabled()) return;
    if (!(normalized->event.type == SDL_QUIT ||
          normalized->event.type == SDL_KEYDOWN ||
          normalized->event.type == SDL_MOUSEBUTTONDOWN ||
          normalized->event.type == SDL_MOUSEWHEEL)) {
        return;
    }
    printf("[ir1] line_drawing_input action=%d route=%d consumed=%d capture=%d gallow=%d gblock=%d "
           "invalidate(target=%d full=%d reasons=0x%x) totals(raw=%llu norm=%llu ig=%llu "
           "imm=%llu q=%llu rg=%llu rf=%llu gb=%llu it=%llu if=%llu)\n",
           (int)normalized->action_class,
           (int)normalized->route_policy,
           route->consumed ? 1 : 0,
           normalized->text_entry_capture ? 1 : 0,
           normalized->global_shortcuts_allowed ? 1 : 0,
           normalized->global_shortcut_blocked ? 1 : 0,
           route->requested_target_invalidation ? 1 : 0,
           route->requested_full_invalidation ? 1 : 0,
           (unsigned int)route->invalidation_reason_bits,
           (unsigned long long)g_line_drawing_input_diag_totals.raw_event_count,
           (unsigned long long)g_line_drawing_input_diag_totals.normalized_count,
           (unsigned long long)g_line_drawing_input_diag_totals.ignored_count,
           (unsigned long long)g_line_drawing_input_diag_totals.immediate_count,
           (unsigned long long)g_line_drawing_input_diag_totals.queued_count,
           (unsigned long long)g_line_drawing_input_diag_totals.routed_global_count,
           (unsigned long long)g_line_drawing_input_diag_totals.routed_focused_count,
           (unsigned long long)g_line_drawing_input_diag_totals.blocked_global_shortcut_count,
           (unsigned long long)g_line_drawing_input_diag_totals.target_invalidation_count,
           (unsigned long long)g_line_drawing_input_diag_totals.full_invalidation_count);
}

static void LineDrawingRunInputRoutingFrame(AppContext* ctx, const SDL_Event* event) {
    LineDrawingInputEventRaw raw;
    LineDrawingInputEventNormalized normalized;
    LineDrawingInputRouteResult route;

    LineDrawingInputIntake(event, &raw);
    LineDrawingInputNormalize(&raw, &normalized);
    LineDrawingInputRoute(ctx, &normalized, &route);
    LineDrawingInputInvalidate(&normalized, &route);
    LineDrawingInputDiagAccumulate(&raw, &normalized, &route);
    LineDrawingInputDiagMaybeEmit(&normalized, &route);
    g_line_drawing_pending_invalidation_bits |= route.invalidation_reason_bits;
}


static void handleInput(AppContext *ctx, SDL_Event* event) {
    LineDrawingRunInputRoutingFrame(ctx, event);
}


static void handleUpdate(AppContext *ctx) {
    Global_TickSystems(ctx);
    g_line_drawing_update_frame.state = Global_Get();
    g_line_drawing_update_frame.state_ready = (g_line_drawing_update_frame.state != NULL);
    g_line_drawing_update_frame.hitboxes_rebuilt = g_line_drawing_update_frame.state_ready;
}


static void handleRender(AppContext *ctx) {
    LineDrawingRenderDeriveFrame derive_frame = {0};
    uint32_t frame_invalidation_bits = g_line_drawing_pending_invalidation_bits;
    uint64_t derive_begin = SDL_GetPerformanceCounter();
    uint64_t derive_end = 0;
    uint64_t submit_end = 0;
    uint64_t perf_freq = SDL_GetPerformanceFrequency();
    uint64_t derive_ns = 0;
    uint64_t submit_ns = 0;

    Render_DeriveFrame(&g_line_drawing_update_frame, ctx, &derive_frame);
    derive_end = SDL_GetPerformanceCounter();
    Render_SubmitFrame(ctx, &derive_frame);
    submit_end = SDL_GetPerformanceCounter();

    if (perf_freq > 0) {
        derive_ns = (uint64_t)((derive_end - derive_begin) * 1000000000ull / perf_freq);
        submit_ns = (uint64_t)((submit_end - derive_end) * 1000000000ull / perf_freq);
    }

    g_line_drawing_rs1_diag_totals.frame_count += 1u;
    g_line_drawing_rs1_diag_totals.derive_ns_total += derive_ns;
    g_line_drawing_rs1_diag_totals.submit_ns_total += submit_ns;
    g_line_drawing_rs1_diag_totals.invalidation_reason_bits_consumed_total +=
        (uint64_t)frame_invalidation_bits;

    if (LineDrawingRs1DiagEnabled()) {
        printf("[rs1] line_drawing frame=%llu derive_ns=%llu submit_ns=%llu invalidation_bits=0x%x "
               "totals(frames=%llu derive_ns=%llu submit_ns=%llu invalid_bits_sum=%llu)\n",
               (unsigned long long)g_line_drawing_rs1_diag_totals.frame_count,
               (unsigned long long)derive_ns,
               (unsigned long long)submit_ns,
               (unsigned int)frame_invalidation_bits,
               (unsigned long long)g_line_drawing_rs1_diag_totals.frame_count,
               (unsigned long long)g_line_drawing_rs1_diag_totals.derive_ns_total,
               (unsigned long long)g_line_drawing_rs1_diag_totals.submit_ns_total,
               (unsigned long long)g_line_drawing_rs1_diag_totals.invalidation_reason_bits_consumed_total);
    }
    g_line_drawing_pending_invalidation_bits = 0u;
}

int line_drawing_app_main_legacy(int argc, char **argv) {
    (void)argc;
    (void)argv;
    AppContext app;
    if (!App_Init(&app, "LineDrawing", DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, true))
        return 1;

    line_drawing3d_shared_theme_load_persisted();
    if (!FontManager_Init()) return 1;
    if (!FontManager_LoadFonts()) return 1;

    // Initialize global program state (grid, layout, editor, etc.)
    Global_Init(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);

    AppCallbacks cbs = {
        .handleInput  = handleInput,
        .handleUpdate = handleUpdate,
        .handleRender = handleRender
    };
    App_SetRenderMode(&app, RENDER_THROTTLED, 1.0f / 60.0f);
    App_Run(&app, &cbs);

    line_drawing3d_shared_theme_save_persisted();
    FontManager_Quit();
    Global_Shutdown();  // free layout memory, etc.
    App_Shutdown(&app);
    return 0;
}

int main(int argc, char **argv) {
    return line_drawing_app_main(argc, argv);
}
