#include "UI/shared_special.h"
#include "UI/shared_controls.h"
#include "UI/shared_text.h"
#include "UI/panel/ui_panel_collection_inspection.h"
#include "UI/ui_panel_section.h"
#include "UI/ui_panel_camera.h"
#include "UI/ui_panel_furniture.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_routes.h"
#include "UI/ui_panel_parts.h"
// src/Input/input_handler.c
#include "input_handler.h"
#include "UI/topbar/line_drawing_editor_topbar.h"
#include "UI/topbar/line_drawing_view_controls.h"
#include "input_mouse.h"
#include "Input/input_viewport_navigation.h"
#include "input_keyboard.h"   // ← NEW
#include "Core/global_state.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_measurement.h"
#include <SDL2/SDL.h>

//        Public input handler
// ======================================
void Input_Handle(AppContext *ctx, SDL_Event* event) {
    // Escape cancels the scope that owns the first press. A repeat must not
    // cancel a newly exposed tool or quit after a dialog closes.
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE && event->key.repeat)
        return;
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
        InputViewportNavigation_ResetGesture();
    if (UIPanel_SharedTextEvent(event)) return;
    if (LDInline_Event(event)) return;
    LineDrawingEditorTopbar_UpdateControls();
    if (LDControls_EditorEvent(event)) return;
    if (LDViewControls_Event(event)) return;
    if (UIPanel_ContextEvent(event)) return;
    if (UIPanel_CameraEvent(event)) return;
    if (CameraView_HandleEvent(Global_Get(),event)) return;
    if (UIPanel_SectionEvent(event)) return;
    if (UIPanel_FurnitureEvent(event)) return;
    if ((event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) &&
        InputViewportNavigation_HandleMouseButton(&event->button)) return;
    if (event->type == SDL_MOUSEMOTION &&
        InputViewportNavigation_HandleMouseMotion(&event->motion)) return;
    if (UIPanel_RoutesEvent(event)) return;
    if (UIPanel_PartsEvent(event)) return;
    UIPanelState* panel=UIPanel_Get();
    if(UIPanel_CollectionInspectionInputEvent(event))return;
    if(UIPanel_TravelEvent(event))return;
    if (panel->measurement.active && panel->activeRightTab==UI_PANEL_RIGHT_TAB_MEASURE) {
        if (event->type==SDL_KEYDOWN && (panel->measurement.placing || panel->measurement.picking)) {
            if (panel->measurement.placing && (event->key.keysym.sym==SDLK_RETURN || event->key.keysym.sym==SDLK_KP_ENTER)) {
                if (panel->measurement.placing>=7) UIPanel_MeasurementApplyButton(panel->measurement.placing);
                else UIPanel_MeasurementStopInput();
            } else if (panel->measurement.placing && (event->key.keysym.mod & (KMOD_CTRL|KMOD_GUI)) && event->key.keysym.sym==SDLK_a)
                panel->measurement.replace_text=true;
            else if (panel->measurement.placing || event->key.keysym.sym==SDLK_ESCAPE)
                (void)UIPanel_MeasurementKey(event->key.keysym.sym);
            return;
        }
        if (event->type==SDL_TEXTINPUT && panel->measurement.placing) {
            (void)UIPanel_MeasurementText(event->text.text); return;
        }
        if (event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT &&
            UIPanel_MeasurementClick(event->button.x,event->button.y)) return;
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
