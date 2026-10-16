#include "Core/SDLApp/sdl_app_window.h"
#include "Core/global_state.h"
#include "Input/input_handler.h"
#include "UI/ui_panel.h"
#include "vk_renderer.h"
#include "Layout/layout_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void cancel_capture(AppContext* ctx) {
    Input_CancelWindowCapture();
    ctx->window_block_pointer = true;
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i)
        ctx->window_block_keys[i] |= ctx->window_held_keys[i];
    ctx->redraw_requested = true;
}

bool SDLAppWindow_Refresh(AppContext* ctx) {
    uint32_t changes = 0;
    bool initialized = ctx->window_state.initialized != 0;
    if (kit_ui_window_refresh_sdl(&ctx->window_state, ctx->window, &changes).code != CORE_OK)
        return false;
    if (initialized && changes) cancel_capture(ctx);
    if (changes & (KIT_UI_WINDOW_GEOMETRY | KIT_UI_WINDOW_MODE | KIT_UI_WINDOW_DISPLAY)) {
        if (ctx->window_state.logical_width > 0 && ctx->window_state.logical_height > 0)
            Global_SetWindowSize(ctx->window_state.logical_width, ctx->window_state.logical_height);
        /* The resource owner may already have recreated (e.g. explicit restart
           proof). Do not destroy an already matching native surface again. */
        VkExtent2D extent = ctx->renderer->context.swapchain.extent;
        if (extent.width != (uint32_t)ctx->window_state.drawable_width ||
            extent.height != (uint32_t)ctx->window_state.drawable_height)
            ctx->pending_swapchain_recreate = true;
        ctx->pending_swapchain_width = ctx->window_state.logical_width;
        ctx->pending_swapchain_height = ctx->window_state.logical_height;
    }
    bool focused = (ctx->window_state.flags & SDL_WINDOW_INPUT_FOCUS) != 0;
    if (!focused || !ctx->window_state.presentable) {
        ctx->window_resume_text |= SDL_IsTextInputActive() == SDL_TRUE;
        SDL_StopTextInput();
    } else if (ctx->window_resume_text) {
        SDL_StartTextInput();
        ctx->window_resume_text = false;
    }
    if (changes) ctx->redraw_requested = true;
    return true;
}

static Uint32 event_window(const SDL_Event* e) {
    switch (e->type) {
        case SDL_WINDOWEVENT: return e->window.windowID;
        case SDL_KEYDOWN: case SDL_KEYUP: return e->key.windowID;
        case SDL_TEXTINPUT: case SDL_TEXTEDITING: return e->text.windowID;
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: return e->button.windowID;
        case SDL_MOUSEMOTION: return e->motion.windowID;
        case SDL_MOUSEWHEEL: return e->wheel.windowID;
        case SDL_DROPFILE: case SDL_DROPTEXT: return e->drop.windowID;
        default: return 0;
    }
}

bool SDLAppWindow_Filter(AppContext* ctx, SDL_Event* e) {
    Uint32 id = event_window(e);
    if (id && id != SDL_GetWindowID(ctx->window)) {
        if (e->type == SDL_DROPFILE || e->type == SDL_DROPTEXT) SDL_free(e->drop.file);
        return false;
    }
    if (kit_ui_window_event_invalidates_sdl(e)) cancel_capture(ctx);
    if (!SDLAppWindow_Refresh(ctx)) { ctx->quit = true; return false; }
    bool key = e->type == SDL_KEYDOWN || e->type == SDL_KEYUP;
    int scan = key ? (int)e->key.keysym.scancode : 0;
    if (key && scan > 0 && scan < SDL_NUM_SCANCODES) {
        ctx->window_held_keys[scan] = e->type == SDL_KEYDOWN;
        if (ctx->window_block_keys[scan]) {
            if (e->type == SDL_KEYUP) ctx->window_block_keys[scan] = false;
            return false;
        }
    }
    bool input = key || e->type == SDL_MOUSEMOTION || e->type == SDL_MOUSEWHEEL ||
        e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP ||
        e->type == SDL_TEXTINPUT || e->type == SDL_TEXTEDITING ||
        e->type == SDL_DROPFILE || e->type == SDL_DROPTEXT;
    if (input && (!ctx->window_state.presentable || !(ctx->window_state.flags & SDL_WINDOW_INPUT_FOCUS))) {
        if (e->type == SDL_DROPFILE || e->type == SDL_DROPTEXT) SDL_free(e->drop.file);
        return false;
    }
    if (!UIPanel_IsCapturingKeyboard()) {
        int fullscreen = kit_ui_window_fullscreen_key_sdl(ctx->window, e);
        if (fullscreen) {
            if (fullscreen < 0) ctx->quit = true;
            cancel_capture(ctx);
            (void)SDLAppWindow_Refresh(ctx);
            return false;
        }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN) ctx->window_block_pointer = false;
    if (ctx->window_block_pointer && (e->type == SDL_MOUSEBUTTONUP ||
        (e->type == SDL_MOUSEMOTION && e->motion.state))) return false;
    int* x = NULL; int* y = NULL;
    if (e->type == SDL_MOUSEMOTION) { x = &e->motion.x; y = &e->motion.y; }
    if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
        x = &e->button.x; y = &e->button.y;
    }
    /* UI/canvas coordinates stay logical. Retina scaling belongs to Vulkan. */
    if (x && !kit_ui_window_map_point_sdl(&ctx->window_state,
        ctx->window_state.logical_width, ctx->window_state.logical_height, *x, *y, x, y))
        return false;
    return true;
}

static int capture(void* user, const char* path) {
    AppContext* ctx = user;
    return vk_renderer_request_capture(ctx->renderer, path) == VK_SUCCESS;
}

typedef struct {
    char* document;
    size_t undo, redo;
    bool dirty;
} WindowCheckpoint;

void SDLAppWindow_Shutdown(AppContext* ctx) {
    WindowCheckpoint* checkpoint = ctx->window_checkpoint;
    if (checkpoint) { Layout_FreeString(checkpoint->document); free(checkpoint); }
    ctx->window_checkpoint = NULL;
}

int SDLAppWindow_Probe(AppContext* ctx) {
    if (!ctx->window_probe.directory) return 0;
    GlobalState* state = Global_Get();
    if (!ctx->window_checkpoint && ctx->presented_frames) {
        WindowCheckpoint* checkpoint = calloc(1, sizeof(*checkpoint));
        if (!checkpoint) return ctx->window_probe.status = -1;
        checkpoint->document = Layout_SaveToString(&state->layout);
        checkpoint->undo = Editor_UndoCount(&state->editor);
        checkpoint->redo = Editor_RedoCount(&state->editor);
        checkpoint->dirty = state->layoutDirtySinceSave;
        ctx->window_checkpoint = checkpoint;
        if (!checkpoint->document) return ctx->window_probe.status = -1;
    }
    int status = kit_ui_window_probe_tick_sdl(&ctx->window_probe, ctx->window,
        ctx->presented_frames, capture, ctx);
    if (status > 0) {
        WindowCheckpoint* checkpoint = ctx->window_checkpoint;
        char* document = Layout_SaveToString(&state->layout);
        bool same = checkpoint && document && !strcmp(document, checkpoint->document) &&
            checkpoint->undo == Editor_UndoCount(&state->editor) &&
            checkpoint->redo == Editor_RedoCount(&state->editor) &&
            checkpoint->dirty == state->layoutDirtySinceSave;
        Layout_FreeString(document);
        fprintf(stdout, "WINDOW_DOCUMENT status=%s history=preserved saved_state=preserved\n", same ? "pass" : "fail");
        if (!same) return ctx->window_probe.status = -1;
    }
    return status;
}
