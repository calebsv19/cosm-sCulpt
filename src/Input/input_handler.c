// src/Input/input_handler.c
#include "input_handler.h"
#include "input_mouse.h"
#include "input_keyboard.h"   // ← NEW
#include "Core/global_state.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_measurement.h"
#include <SDL2/SDL.h>

//        Public input handler
// ======================================
void Input_Handle(AppContext *ctx, SDL_Event* event) {
    if (UIPanel_Get()->measurement.active) {
        if (event->type == SDL_KEYDOWN) (void)UIPanel_MeasurementKey(event->key.keysym.sym);
        else if (event->type == SDL_TEXTINPUT) (void)UIPanel_MeasurementText(event->text.text);
        else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
            (void)UIPanel_MeasurementClick(event->button.x, event->button.y);
        if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP || event->type == SDL_TEXTINPUT ||
            event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP ||
            event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEWHEEL) return;
    }
    if (event->type == SDL_TEXTINPUT) {
        if (UIPanel_HandleTextInput(event->text.text)) {
            return;
        }
    }

    if (event->type == SDL_MOUSEMOTION) {
        UIPanel_HandleMouseMotion(event->motion.x, event->motion.y);
    }

    Global_RebuildHitboxesIfDirty();

    Input_MouseHandle(ctx, event);

    Input_KeyboardHandle(ctx, event);
}
