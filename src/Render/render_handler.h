// src/Render/render_handler.h
#pragma once
#include "Core/SDLApp/sdl_app_framework.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include <stdint.h>

typedef struct LineDrawingUpdateFrame {
    GlobalState* state;
    bool state_ready;
    bool hitboxes_rebuilt;
} LineDrawingUpdateFrame;

typedef struct LineDrawingRenderDeriveFrame {
    bool valid;
    GlobalState* state;
    SpaceViewContext view_ctx;
    Grid* grid;
    Layout* layout;
    EditorState* editor;
    int screen_width;
    int screen_height;
    SDL_Color background;
    uint32_t vk_draw_calls_before;
    bool free_view_enabled;
} LineDrawingRenderDeriveFrame;

void Render_Frame(AppContext* ctx);
void Render_DeriveFrame(const LineDrawingUpdateFrame* update_frame,
                        AppContext* ctx,
                        LineDrawingRenderDeriveFrame* out_derive_frame);
void Render_SubmitFrame(AppContext* ctx,
                        const LineDrawingRenderDeriveFrame* derive_frame);
