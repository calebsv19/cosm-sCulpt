// src/Input/input_mouse.c
#include "input_mouse.h"
#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Layout/grid.h"
#include "Math/math_util.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

static bool dragging = false;
static int lastMx = 0, lastMy = 0;

void Input_MouseHandle(AppContext *ctx, SDL_Event* event) {
    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;
    EditorState* editor = &state->editor;

    float scale = grid->scale;
    float gridSize = grid->gridSize;

    switch (event->type) {
        case SDL_MOUSEWHEEL: {
            int w, h;
            SDL_GetRendererOutputSize(ctx->renderer, &w, &h);
            float factor = (event->wheel.y > 0) ? 1.1f : 0.9f;
            Grid_zoom(grid, factor, w / 2.0f, h / 2.0f);
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
            if (event->button.button == SDL_BUTTON_LEFT) {
                dragging = true;
                lastMx = event->button.x;
                lastMy = event->button.y;
            } else if (event->button.button == SDL_BUTTON_RIGHT) {
 		printf("Clicked button\n");
                Vec2 world = {
                    .x = event->button.x / (gridSize * scale) + grid->offsetX,
                    .y = event->button.y / (gridSize * scale) + grid->offsetY
                };
                world = Vec2_Snap(world, gridSize);
                Editor_ClickAt(editor, world);
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (event->button.button == SDL_BUTTON_LEFT) {
                dragging = false;
            }
            // No action needed on RIGHT mouse up anymore
            break;

        case SDL_MOUSEMOTION:
            if (dragging) {
                int dx = event->motion.x - lastMx;
                int dy = event->motion.y - lastMy;
                Grid_pan(grid, -dx, -dy);
                lastMx = event->motion.x;
                lastMy = event->motion.y;
            }
            break;
    }
}

