// src/main.c
#include "line_drawing/line_drawing_app_main.h"
#include "Core/SDLApp/sdl_app_framework.h"
#include "Menu/line_drawing_host_menu.h"
#include "Layout/Grid/grid.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/ui_panel.h"
#include "UI/workspace_authoring/line_drawing_workspace_authoring_host.h"


#include "Input/input_handler.h"
#include "Input/input_routing_policy.h"
#include "Render/render_handler.h"
#include "Core/global_state.h"
#include "core_mesh_asset.h"
#include <math.h>
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
static LineDrawingHostMenuState g_line_drawing_host_menu = {0};

typedef enum LineDrawingHostMode {
    LINE_DRAWING_HOST_MODE_MENU = 0,
    LINE_DRAWING_HOST_MODE_EDITOR
} LineDrawingHostMode;

static LineDrawingHostMode g_line_drawing_host_mode = LINE_DRAWING_HOST_MODE_MENU;

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

    if (!normalized->text_entry_capture &&
        LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(Global_Get(), &normalized->event)) {
        out_result->consumed = true;
        out_result->requested_target_invalidation = true;
        out_result->invalidation_reason_bits |= LINE_DRAWING_INPUT_INVALIDATION_REASON_ACTION;
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

static void LineDrawingHostEnterMenu(void) {
    GlobalState* state = Global_Get();
    if (state && LineDrawingWorkspaceAuthoringHost_Active(state)) {
        (void)LineDrawingWorkspaceAuthoringHost_Cancel(state);
    }
    UIPanel_ResetTransientUiState();
    g_line_drawing_host_mode = LINE_DRAWING_HOST_MODE_MENU;
    g_line_drawing_pending_invalidation_bits |= LINE_DRAWING_INPUT_INVALIDATION_REASON_ACTION;
}

static void LineDrawingHostEnterEditor(void) {
    UIPanel_ResetTransientUiState();
    g_line_drawing_host_mode = LINE_DRAWING_HOST_MODE_EDITOR;
    g_line_drawing_pending_invalidation_bits |= LINE_DRAWING_INPUT_INVALIDATION_REASON_ACTION;
}

static bool LineDrawingHostMenuShortcutRequested(const SDL_Event* event) {
    const SDL_Keymod mods = SDL_GetModState();
    if (!event || event->type != SDL_KEYDOWN) return false;
    if (event->key.repeat != 0) return false;
    if (event->key.keysym.sym != SDLK_m) return false;
    return ((mods & KMOD_CTRL) != 0) || ((mods & KMOD_GUI) != 0);
}

static bool LineDrawingHostReturnToMenuRequested(const SDL_Event* event) {
    if (!event || event->type != SDL_KEYDOWN) return false;
    if (event->key.repeat != 0) return false;
    if (event->key.keysym.sym == SDLK_ESCAPE) return true;
    return LineDrawingHostMenuShortcutRequested(event);
}

static void handleInput(AppContext *ctx, SDL_Event* event) {
    if (g_line_drawing_host_mode == LINE_DRAWING_HOST_MODE_MENU) {
        LineDrawingHostMenuCommand command = {0};
        if (LineDrawingHostMenu_HandleEvent(&g_line_drawing_host_menu, ctx, event, &command)) {
            if (command.type == LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR) {
                LineDrawingHostEnterEditor();
            } else if (command.type == LINE_DRAWING_HOST_MENU_COMMAND_QUIT) {
                ctx->quit = true;
            }
        }
        return;
    }

    if (!UIPanel_IsCapturingKeyboard() &&
        !LineDrawingWorkspaceAuthoringHost_Active(Global_Get()) &&
        LineDrawingHostReturnToMenuRequested(event)) {
        LineDrawingHostEnterMenu();
        return;
    }

    LineDrawingRunInputRoutingFrame(ctx, event);
}


static void handleUpdate(AppContext *ctx) {
    if (g_line_drawing_host_mode == LINE_DRAWING_HOST_MODE_MENU) {
        (void)ctx;
        memset(&g_line_drawing_update_frame, 0, sizeof(g_line_drawing_update_frame));
        return;
    }

    Global_TickSystems(ctx);
    g_line_drawing_update_frame.state = Global_Get();
    g_line_drawing_update_frame.state_ready = (g_line_drawing_update_frame.state != NULL);
    g_line_drawing_update_frame.hitboxes_rebuilt = g_line_drawing_update_frame.state_ready;
}


static void handleRender(AppContext *ctx) {
    if (g_line_drawing_host_mode == LINE_DRAWING_HOST_MODE_MENU) {
        LineDrawingHostMenu_Render(&g_line_drawing_host_menu, ctx);
        g_line_drawing_pending_invalidation_bits = 0u;
        return;
    }

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

static void LineDrawingRuntimeShutdown(AppContext* app) {
    if (LineDrawingWorkspaceAuthoringHost_Active(Global_Get())) {
        (void)LineDrawingWorkspaceAuthoringHost_Cancel(Global_Get());
    }
    line_drawing3d_shared_theme_save_persisted();
    (void)FontManager_SavePersistedPrefs();
    FontManager_Quit();
    Global_Shutdown();
    App_Shutdown(app);
}

static bool LineDrawingVisualArtifactModeIsEditor(const char* mode) {
    return mode && strcmp(mode, "editor") == 0;
}

// Stage one normalized runtime mesh for deterministic Solid/Material artifact captures.
static bool LineDrawingVisualArtifactStageMesh(const char* runtimePath) {
    CoreMeshAssetRuntimeDocument document;
    GlobalState* state = Global_Get();
    Transform3D transform = Layout_Transform3D_Default();
    Object3D* object = NULL;
    uint32_t objectId = 0u;
    double spanX = 0.0;
    double spanY = 0.0;
    double spanZ = 0.0;
    double maxSpan = 0.0;
    double scale = 1.0;
    CoreObjectVec3 center = {0};
    if (!state || !runtimePath || !runtimePath[0]) return false;
    core_mesh_asset_runtime_document_init(&document);
    if (core_mesh_asset_runtime_document_load_file(runtimePath, &document).code != CORE_OK) {
        core_mesh_asset_runtime_document_free(&document);
        return false;
    }

    spanX = document.contract.local_bounds.max.x - document.contract.local_bounds.min.x;
    spanY = document.contract.local_bounds.max.y - document.contract.local_bounds.min.y;
    spanZ = document.contract.local_bounds.max.z - document.contract.local_bounds.min.z;
    maxSpan = fmax(spanX, fmax(spanY, spanZ));
    if (!isfinite(maxSpan) || maxSpan <= 0.0) {
        core_mesh_asset_runtime_document_free(&document);
        return false;
    }
    scale = 10.0 / maxSpan;
    center = (CoreObjectVec3){
        0.5 * (document.contract.local_bounds.min.x + document.contract.local_bounds.max.x),
        0.5 * (document.contract.local_bounds.min.y + document.contract.local_bounds.max.y),
        0.5 * (document.contract.local_bounds.min.z + document.contract.local_bounds.max.z)
    };
    transform.scale = (Vec3){(float)scale, (float)scale, (float)scale};
    transform.position = (Vec3){
        (float)(-center.x * scale),
        (float)(-center.y * scale),
        (float)(-center.z * scale)
    };
    objectId = Layout_ObjectStore_Create(&state->layout.objectStore,
                                         OBJECT3D_KIND_MESH_ASSET_INSTANCE,
                                         &transform,
                                         "visual_artifact_mesh",
                                         CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                         CORE_OBJECT_PLANE_XY);
    object = Layout_ObjectStore_Find(&state->layout.objectStore, objectId);
    if (!object) {
        core_mesh_asset_runtime_document_free(&document);
        return false;
    }
    snprintf(object->meshInstance.assetId,
             sizeof(object->meshInstance.assetId),
             "%s",
             document.contract.asset_id);
    snprintf(object->meshInstance.sourceAssetId,
             sizeof(object->meshInstance.sourceAssetId),
             "%s",
             document.contract.source_asset_id);
    snprintf(object->meshInstance.runtimePath,
             sizeof(object->meshInstance.runtimePath),
             "%s",
             runtimePath);
    object->meshInstance.localBoundsMin = (Vec3){
        (float)document.contract.local_bounds.min.x,
        (float)document.contract.local_bounds.min.y,
        (float)document.contract.local_bounds.min.z
    };
    object->meshInstance.localBoundsMax = (Vec3){
        (float)document.contract.local_bounds.max.x,
        (float)document.contract.local_bounds.max.y,
        (float)document.contract.local_bounds.max.z
    };
    object->meshInstance.vertexCount = document.vertex_count;
    object->meshInstance.triangleCount = document.triangle_count;
    object->meshInstance.lockToBounds = false;
    state->layout.scene3d.bounds.enabled = false;
    state->spaceMode = SPACE_MODE_3D;
    state->previewMode = LINE_DRAWING_PREVIEW_MODE_FLAT;
    {
        const char* previewMode = getenv("LINE_DRAWING_VISUAL_PREVIEW_MODE");
        if (previewMode && strcmp(previewMode, "bounds") == 0) {
            state->previewMode = LINE_DRAWING_PREVIEW_MODE_BOUNDS;
        } else if (previewMode && strcmp(previewMode, "wire") == 0) {
            state->previewMode = LINE_DRAWING_PREVIEW_MODE_WIREFRAME;
        } else if (previewMode && strcmp(previewMode, "material") == 0) {
            state->previewMode = LINE_DRAWING_PREVIEW_MODE_MATERIAL;
        }
    }
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 12.0f;
    state->freeViewCamera.target = (Vec3){0.0f, 0.0f, 0.0f};
    state->editor.selectedObject3DId = 0u;
    state->editor.hoveredObject3DId = 0u;
    core_mesh_asset_runtime_document_free(&document);
    return true;
}

static int LineDrawingRunVisualArtifactProof(AppContext* app,
                                             const char* artifact_path,
                                             const char* proof_mode) {
    const char* visualMeshPath = getenv("LINE_DRAWING_VISUAL_MESH_RUNTIME");
    VkResult capture_request = VK_ERROR_INITIALIZATION_FAILED;
    bool rendered = false;
    uint32_t draw_calls = 0u;

    if (!app || !app->renderer || !artifact_path || !artifact_path[0]) {
        fprintf(stderr, "line_drawing: visual-artifact missing renderer or output path\n");
        return 1;
    }

    if (LineDrawingVisualArtifactModeIsEditor(proof_mode)) {
        LineDrawingHostEnterEditor();
        if (visualMeshPath && visualMeshPath[0] &&
            !LineDrawingVisualArtifactStageMesh(visualMeshPath)) {
            fprintf(stderr,
                    "line_drawing: visual-artifact failed to stage mesh path=%s\n",
                    visualMeshPath);
            return 1;
        }
        handleUpdate(app);
    }

    if (visualMeshPath && visualMeshPath[0]) {
        if (!App_RenderOnce(app, handleRender)) {
            fprintf(stderr, "line_drawing: visual-artifact mesh warmup render failed\n");
            return 1;
        }
        SDL_Delay(180u);
    }

    capture_request = vk_renderer_request_capture(app->renderer, artifact_path);
    if (capture_request != VK_SUCCESS) {
        fprintf(stderr,
                "line_drawing: visual-artifact capture request failed code=%d path=%s\n",
                (int)capture_request,
                artifact_path);
        return 1;
    }

    rendered = App_RenderOnce(app, handleRender);
    draw_calls = app->renderer ? app->renderer->draw_state.draw_call_count : 0u;
    if (!rendered) {
        fprintf(stderr, "line_drawing: visual-artifact render failed path=%s\n", artifact_path);
        return 1;
    }
    if (draw_calls == 0u) {
        fprintf(stderr, "line_drawing: visual-artifact produced zero draw calls path=%s\n",
                artifact_path);
        return 1;
    }

    return 0;
}

int line_drawing_app_main_legacy(int argc, char **argv) {
    (void)argc;
    (void)argv;
    const char* visual_artifact_path = getenv("LINE_DRAWING_VISUAL_ARTIFACT");
    const char* visual_artifact_mode = getenv("LINE_DRAWING_VISUAL_ARTIFACT_MODE");
    AppContext app;
    if (!App_Init(&app, "LineDrawing", DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, true))
        return 1;

    line_drawing3d_shared_theme_load_persisted();
    if (!FontManager_Init()) return 1;
    (void)FontManager_LoadPersistedPrefs();
    if (!FontManager_LoadFonts()) return 1;

    // Initialize global program state (grid, layout, editor, etc.)
    Global_Init(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    (void)UIPanel_RestorePersistedFileSession();
    LineDrawingHostMenu_Init(&g_line_drawing_host_menu);
    LineDrawingHostEnterMenu();

    AppCallbacks cbs = {
        .handleInput  = handleInput,
        .handleUpdate = handleUpdate,
        .handleRender = handleRender
    };
    App_SetRenderMode(&app, RENDER_THROTTLED, 1.0f / 60.0f);
    if (visual_artifact_path && visual_artifact_path[0]) {
        const int proof_result =
            LineDrawingRunVisualArtifactProof(&app, visual_artifact_path, visual_artifact_mode);
        LineDrawingRuntimeShutdown(&app);
        return proof_result;
    }

    App_Run(&app, &cbs);

    LineDrawingRuntimeShutdown(&app);
    return 0;
}

int main(int argc, char **argv) {
    return line_drawing_app_main(argc, argv);
}
