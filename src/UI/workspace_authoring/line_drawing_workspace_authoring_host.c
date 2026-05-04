#include "UI/workspace_authoring/line_drawing_workspace_authoring_host.h"

#include <string.h>

#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "core_theme.h"
#include "kit_workspace_authoring.h"
#include "kit_workspace_authoring_ui.h"

static CoreResult line_drawing_authoring_invalid(const char* message) {
    CoreResult result = { CORE_ERR_INVALID_ARG, message };
    return result;
}

static uint32_t line_drawing_authoring_mod_bits(SDL_Keymod mods) {
    uint32_t bits = 0u;
    if ((mods & KMOD_SHIFT) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_SHIFT;
    if ((mods & KMOD_ALT) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_ALT;
    if ((mods & KMOD_CTRL) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_CTRL;
    if ((mods & KMOD_GUI) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_GUI;
    return bits;
}

static KitWorkspaceAuthoringKey line_drawing_authoring_key_from_sdl_keysym(
    const SDL_Keysym* keysym) {
    if (!keysym) return KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;

    switch (keysym->scancode) {
        case SDL_SCANCODE_C:
            return KIT_WORKSPACE_AUTHORING_KEY_C;
        case SDL_SCANCODE_V:
            return KIT_WORKSPACE_AUTHORING_KEY_V;
        default:
            break;
    }

    switch (keysym->sym) {
        case SDLK_TAB:
            return KIT_WORKSPACE_AUTHORING_KEY_TAB;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            return KIT_WORKSPACE_AUTHORING_KEY_ENTER;
        case SDLK_ESCAPE:
            return KIT_WORKSPACE_AUTHORING_KEY_ESCAPE;
        case SDLK_c:
            return KIT_WORKSPACE_AUTHORING_KEY_C;
        case SDLK_v:
            return KIT_WORKSPACE_AUTHORING_KEY_V;
        default:
            return KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    }
}

static void line_drawing_authoring_note_consumed(GlobalState* state) {
    if (!state) return;
    state->workspaceAuthoring.last_event_consumed = 1u;
    state->workspaceAuthoring.consumed_event_count += 1u;
}

static void line_drawing_authoring_capture_baseline(GlobalState* state) {
    LineDrawingWorkspaceAuthoringHostState* authoring = NULL;
    LineDrawingPaneHost* pane_host = NULL;
    const char* font_preset_name = NULL;
    if (!state) return;

    authoring = &state->workspaceAuthoring;
    pane_host = &state->paneHost;
    memset(authoring->font_theme_status, 0, sizeof(authoring->font_theme_status));
    authoring->baseline_font_zoom_step = FontManager_GetZoomStep();
    authoring->baseline_theme_preset[0] = '\0';
    authoring->baseline_font_preset[0] = '\0';
    (void)line_drawing3d_shared_theme_current_preset(authoring->baseline_theme_preset,
                                                     sizeof(authoring->baseline_theme_preset));
    font_preset_name = FontManager_GetSharedFontPresetName();
    if (font_preset_name && font_preset_name[0]) {
        SDL_strlcpy(authoring->baseline_font_preset,
                    font_preset_name,
                    sizeof(authoring->baseline_font_preset));
    }
    authoring->baseline_node_count = pane_host->node_count;
    authoring->baseline_root_index = pane_host->root_index;
    authoring->baseline_top_height = pane_host->target_top_height;
    authoring->baseline_left_width = pane_host->target_left_width;
    authoring->baseline_right_width = pane_host->target_right_width;
    memcpy(authoring->baseline_nodes, pane_host->nodes, sizeof(authoring->baseline_nodes));
    authoring->draft_baseline_valid = 1u;
}

static CoreResult line_drawing_authoring_restore_baseline(GlobalState* state) {
    LineDrawingWorkspaceAuthoringHostState* authoring = NULL;
    LineDrawingPaneHost* pane_host = NULL;
    if (!state) return line_drawing_authoring_invalid("null global state");

    authoring = &state->workspaceAuthoring;
    pane_host = &state->paneHost;
    if (!authoring->draft_baseline_valid) {
        return line_drawing_authoring_invalid("missing authoring baseline");
    }

    pane_host->node_count = authoring->baseline_node_count;
    pane_host->root_index = authoring->baseline_root_index;
    pane_host->target_top_height = authoring->baseline_top_height;
    pane_host->target_left_width = authoring->baseline_left_width;
    pane_host->target_right_width = authoring->baseline_right_width;
    memcpy(pane_host->nodes, authoring->baseline_nodes, sizeof(pane_host->nodes));
    (void)LineDrawingPaneHost_Rebuild(pane_host,
                                      (float)state->screenWidth,
                                      (float)state->screenHeight);
    (void)FontManager_SetZoomStep(authoring->baseline_font_zoom_step);
    if (authoring->baseline_theme_preset[0]) {
        (void)line_drawing3d_shared_theme_set_preset(authoring->baseline_theme_preset);
    }
    if (authoring->baseline_font_preset[0]) {
        (void)FontManager_SetSharedFontPresetName(authoring->baseline_font_preset);
    }
    return core_result_ok();
}

static void line_drawing_authoring_clear_baseline(GlobalState* state) {
    LineDrawingWorkspaceAuthoringHostState* authoring = NULL;
    if (!state) return;

    authoring = &state->workspaceAuthoring;
    authoring->draft_baseline_valid = 0u;
    authoring->baseline_node_count = 0u;
    authoring->baseline_root_index = 0u;
    authoring->baseline_top_height = 0.0f;
    authoring->baseline_left_width = 0.0f;
    authoring->baseline_right_width = 0.0f;
    authoring->baseline_font_zoom_step = 0;
    authoring->baseline_theme_preset[0] = '\0';
    authoring->baseline_font_preset[0] = '\0';
    memset(authoring->baseline_nodes, 0, sizeof(authoring->baseline_nodes));
}

static void line_drawing_authoring_note_font_theme_status(GlobalState* state,
                                                          const char* status) {
    if (!state) return;
    SDL_strlcpy(state->workspaceAuthoring.font_theme_status,
                status && status[0] ? status : "Action ready.",
                sizeof(state->workspaceAuthoring.font_theme_status));
}

static int line_drawing_authoring_apply_font_theme_button(
    GlobalState* state,
    KitWorkspaceAuthoringFontThemeButtonId button_id) {
    KitWorkspaceAuthoringFontThemeAction action;
    const char* status_text = NULL;
    const char* preset_name = NULL;

    if (!state ||
        !LineDrawingWorkspaceAuthoringHost_FontThemeOverlayActive(state) ||
        button_id == KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE ||
        !kit_workspace_authoring_ui_font_theme_button_enabled(button_id)) {
        return 0;
    }

    action = kit_workspace_authoring_ui_font_theme_action_for_button(button_id);
    switch (action.type) {
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_DEC:
            if (FontManager_AdjustZoomStep(-1)) {
                line_drawing_authoring_note_font_theme_status(state, "Text size decreased.");
                state->workspaceAuthoring.font_theme_button_click_count += 1u;
                return 1;
            }
            line_drawing_authoring_note_font_theme_status(state, "Text size is already at minimum.");
            return 1;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_INC:
            if (FontManager_AdjustZoomStep(1)) {
                line_drawing_authoring_note_font_theme_status(state, "Text size increased.");
                state->workspaceAuthoring.font_theme_button_click_count += 1u;
                return 1;
            }
            line_drawing_authoring_note_font_theme_status(state, "Text size is already at maximum.");
            return 1;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_RESET:
            if (FontManager_SetZoomStep(0)) {
                line_drawing_authoring_note_font_theme_status(state, "Text size reset.");
                state->workspaceAuthoring.font_theme_button_click_count += 1u;
                return 1;
            }
            line_drawing_authoring_note_font_theme_status(state, "Text size reset failed.");
            return 1;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_FONT_PRESET:
            if (FontManager_SetSharedFontPresetId(action.font_preset_id)) {
                preset_name = core_font_preset_name(action.font_preset_id);
                line_drawing_authoring_note_font_theme_status(
                    state,
                    preset_name && preset_name[0] ? preset_name : "Font preset changed.");
                state->workspaceAuthoring.font_theme_button_click_count += 1u;
                return 1;
            }
            line_drawing_authoring_note_font_theme_status(state, "Font preset change failed.");
            return 1;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_THEME_PRESET:
            preset_name = core_theme_preset_name(action.theme_preset_id);
            if (preset_name && line_drawing3d_shared_theme_set_preset(preset_name)) {
                line_drawing_authoring_note_font_theme_status(state, "Theme preset changed.");
                state->workspaceAuthoring.font_theme_button_click_count += 1u;
                return 1;
            }
            line_drawing_authoring_note_font_theme_status(state, "Theme preset change failed.");
            return 1;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_CUSTOM_THEME_STATUS:
            status_text = action.custom_status_text ? action.custom_status_text : "Custom theme action requested.";
            line_drawing_authoring_note_font_theme_status(state, status_text);
            state->workspaceAuthoring.font_theme_button_click_count += 1u;
            return 1;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_NONE:
        default:
            break;
    }
    return 0;
}

void LineDrawingWorkspaceAuthoringHost_Reset(LineDrawingWorkspaceAuthoringHostState* host) {
    if (!host) return;
    memset(host, 0, sizeof(*host));
    host->overlay_mode = LINE_DRAWING_WORKSPACE_AUTHORING_OVERLAY_PANE;
}

int LineDrawingWorkspaceAuthoringHost_Active(const GlobalState* state) {
    if (!state) return 0;
    return state->workspaceAuthoring.active ? 1 : 0;
}

int LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(const GlobalState* state) {
    if (!LineDrawingWorkspaceAuthoringHost_Active(state)) return 0;
    return state->workspaceAuthoring.overlay_mode ==
                   LINE_DRAWING_WORKSPACE_AUTHORING_OVERLAY_PANE
               ? 1
               : 0;
}

int LineDrawingWorkspaceAuthoringHost_FontThemeOverlayActive(const GlobalState* state) {
    if (!LineDrawingWorkspaceAuthoringHost_Active(state)) return 0;
    return state->workspaceAuthoring.overlay_mode ==
                   LINE_DRAWING_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME
               ? 1
               : 0;
}

CoreResult LineDrawingWorkspaceAuthoringHost_Enter(GlobalState* state) {
    if (!state) return line_drawing_authoring_invalid("null global state");

    if (!LineDrawingWorkspaceAuthoringHost_Active(state)) {
        line_drawing_authoring_capture_baseline(state);
        state->workspaceAuthoring.active = 1;
        state->workspaceAuthoring.overlay_mode = LINE_DRAWING_WORKSPACE_AUTHORING_OVERLAY_PANE;
        state->workspaceAuthoring.enter_count += 1u;
    }
    state->workspaceAuthoring.last_event_entered = 1u;
    return core_result_ok();
}

CoreResult LineDrawingWorkspaceAuthoringHost_Apply(GlobalState* state) {
    if (!state) return line_drawing_authoring_invalid("null global state");

    if (LineDrawingWorkspaceAuthoringHost_Active(state)) {
        state->workspaceAuthoring.active = 0;
        line_drawing_authoring_clear_baseline(state);
        state->workspaceAuthoring.apply_count += 1u;
    }
    state->workspaceAuthoring.key_c_down = 0u;
    state->workspaceAuthoring.key_v_down = 0u;
    state->workspaceAuthoring.entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    state->workspaceAuthoring.overlay_mode = LINE_DRAWING_WORKSPACE_AUTHORING_OVERLAY_PANE;
    state->workspaceAuthoring.last_event_exited = 1u;
    return core_result_ok();
}

CoreResult LineDrawingWorkspaceAuthoringHost_Cancel(GlobalState* state) {
    CoreResult restore_result = core_result_ok();
    if (!state) return line_drawing_authoring_invalid("null global state");

    if (LineDrawingWorkspaceAuthoringHost_Active(state)) {
        if (state->workspaceAuthoring.draft_baseline_valid) {
            restore_result = line_drawing_authoring_restore_baseline(state);
        }
        state->workspaceAuthoring.active = 0;
        line_drawing_authoring_clear_baseline(state);
        state->workspaceAuthoring.cancel_count += 1u;
    }
    state->workspaceAuthoring.key_c_down = 0u;
    state->workspaceAuthoring.key_v_down = 0u;
    state->workspaceAuthoring.entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    state->workspaceAuthoring.overlay_mode = LINE_DRAWING_WORKSPACE_AUTHORING_OVERLAY_PANE;
    state->workspaceAuthoring.last_event_exited = 1u;
    return restore_result;
}

CoreResult LineDrawingWorkspaceAuthoringHost_CycleOverlay(GlobalState* state) {
    if (!state) return line_drawing_authoring_invalid("null global state");
    if (!LineDrawingWorkspaceAuthoringHost_Active(state)) {
        return core_result_ok();
    }

    state->workspaceAuthoring.overlay_mode =
        (state->workspaceAuthoring.overlay_mode ==
         LINE_DRAWING_WORKSPACE_AUTHORING_OVERLAY_PANE)
            ? LINE_DRAWING_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME
            : LINE_DRAWING_WORKSPACE_AUTHORING_OVERLAY_PANE;
    state->workspaceAuthoring.overlay_cycle_count += 1u;
    return core_result_ok();
}

int LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(GlobalState* state, const SDL_Event* event) {
    KitWorkspaceAuthoringKey key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    uint32_t mod_bits = 0u;
    int authoring_alt_only = 0;
    int chord_pair_pressed = 0;
    const char* trigger = NULL;

    if (!state || !event) return 0;

    state->workspaceAuthoring.last_event_consumed = 0u;
    state->workspaceAuthoring.last_event_entered = 0u;
    state->workspaceAuthoring.last_event_exited = 0u;

    if (event->type == SDL_MOUSEMOTION) {
        if (LineDrawingWorkspaceAuthoringHost_Active(state)) {
            state->workspaceAuthoring.last_pointer_x = event->motion.x;
            state->workspaceAuthoring.last_pointer_y = event->motion.y;
            state->workspaceAuthoring.last_pointer_ready = 1u;
            line_drawing_authoring_note_consumed(state);
            return 1;
        }
        return 0;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (LineDrawingWorkspaceAuthoringHost_Active(state)) {
            KitWorkspaceAuthoringOverlayButton buttons[4];
            KitWorkspaceAuthoringOverlayButtonId hit;
            uint32_t count = 0u;
            state->workspaceAuthoring.last_pointer_x = event->button.x;
            state->workspaceAuthoring.last_pointer_y = event->button.y;
            state->workspaceAuthoring.last_pointer_ready = 1u;
            count = kit_workspace_authoring_ui_build_overlay_buttons(
                state->screenWidth,
                1,
                LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(state),
                buttons,
                4u);
            hit = kit_workspace_authoring_ui_overlay_hit_test(buttons,
                                                             count,
                                                             (float)event->button.x,
                                                             (float)event->button.y);
            if (hit == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE) {
                (void)LineDrawingWorkspaceAuthoringHost_CycleOverlay(state);
                state->workspaceAuthoring.overlay_button_click_count += 1u;
            } else if (hit == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_APPLY) {
                (void)LineDrawingWorkspaceAuthoringHost_Apply(state);
                state->workspaceAuthoring.overlay_button_click_count += 1u;
            } else if (hit == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL) {
                (void)LineDrawingWorkspaceAuthoringHost_Cancel(state);
                state->workspaceAuthoring.overlay_button_click_count += 1u;
            } else if (hit == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_ADD) {
                state->workspaceAuthoring.overlay_button_click_count += 1u;
            }
            if (hit == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE &&
                LineDrawingWorkspaceAuthoringHost_FontThemeOverlayActive(state)) {
                KitWorkspaceAuthoringFontThemeLayout layout = {0};
                KitWorkspaceAuthoringFontThemeButtonId font_hit =
                    KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE;
                if (kit_workspace_authoring_ui_font_theme_build_layout(NULL,
                                                                       state->screenWidth,
                                                                       state->screenHeight,
                                                                       &layout)) {
                    font_hit = kit_workspace_authoring_ui_font_theme_hit_button(
                        &layout,
                        (float)event->button.x,
                        (float)event->button.y);
                    (void)line_drawing_authoring_apply_font_theme_button(state, font_hit);
                }
            }
            line_drawing_authoring_note_consumed(state);
            return 1;
        }
        return 0;
    }

    if (event->type != SDL_KEYDOWN && event->type != SDL_KEYUP) {
        if (LineDrawingWorkspaceAuthoringHost_Active(state)) {
            line_drawing_authoring_note_consumed(state);
            return 1;
        }
        return 0;
    }

    key = line_drawing_authoring_key_from_sdl_keysym(&event->key.keysym);
    if (event->type == SDL_KEYUP) {
        if (key == KIT_WORKSPACE_AUTHORING_KEY_C) {
            state->workspaceAuthoring.key_c_down = 0u;
        } else if (key == KIT_WORKSPACE_AUTHORING_KEY_V) {
            state->workspaceAuthoring.key_v_down = 0u;
        }
        if (LineDrawingWorkspaceAuthoringHost_Active(state)) {
            line_drawing_authoring_note_consumed(state);
            return 1;
        }
        return 0;
    }

    if (event->key.repeat != 0) {
        return LineDrawingWorkspaceAuthoringHost_Active(state);
    }

    mod_bits = line_drawing_authoring_mod_bits((SDL_Keymod)event->key.keysym.mod);
    authoring_alt_only = ((mod_bits & KIT_WORKSPACE_AUTHORING_MOD_ALT) != 0u) &&
                         ((mod_bits & (KIT_WORKSPACE_AUTHORING_MOD_SHIFT |
                                       KIT_WORKSPACE_AUTHORING_MOD_CTRL |
                                       KIT_WORKSPACE_AUTHORING_MOD_GUI)) == 0u);

    if (authoring_alt_only) {
        if (key == KIT_WORKSPACE_AUTHORING_KEY_C) {
            state->workspaceAuthoring.key_c_down = 1u;
        } else if (key == KIT_WORKSPACE_AUTHORING_KEY_V) {
            state->workspaceAuthoring.key_v_down = 1u;
        }
    }

    chord_pair_pressed = kit_workspace_authoring_entry_chord_pressed(
        key,
        mod_bits,
        state->workspaceAuthoring.key_c_down ? 1 : 0,
        state->workspaceAuthoring.key_v_down ? 1 : 0);
    if (authoring_alt_only &&
        (key == KIT_WORKSPACE_AUTHORING_KEY_C || key == KIT_WORKSPACE_AUTHORING_KEY_V) &&
        state->workspaceAuthoring.entry_chord_armed_key !=
            KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN &&
        state->workspaceAuthoring.entry_chord_armed_key != (unsigned char)key) {
        chord_pair_pressed = 1;
    }

    if (chord_pair_pressed) {
        if (LineDrawingWorkspaceAuthoringHost_Active(state)) {
            (void)LineDrawingWorkspaceAuthoringHost_Cancel(state);
        } else {
            (void)LineDrawingWorkspaceAuthoringHost_Enter(state);
        }
        state->workspaceAuthoring.entry_chord_armed_key =
            KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
        line_drawing_authoring_note_consumed(state);
        return 1;
    }

    if (authoring_alt_only &&
        (key == KIT_WORKSPACE_AUTHORING_KEY_C || key == KIT_WORKSPACE_AUTHORING_KEY_V)) {
        state->workspaceAuthoring.entry_chord_armed_key = (unsigned char)key;
        line_drawing_authoring_note_consumed(state);
        return 1;
    }

    if (!LineDrawingWorkspaceAuthoringHost_Active(state)) {
        return 0;
    }

    if (key == KIT_WORKSPACE_AUTHORING_KEY_ESCAPE) {
        (void)LineDrawingWorkspaceAuthoringHost_Cancel(state);
        line_drawing_authoring_note_consumed(state);
        return 1;
    }
    if (key == KIT_WORKSPACE_AUTHORING_KEY_ENTER) {
        (void)LineDrawingWorkspaceAuthoringHost_Apply(state);
        line_drawing_authoring_note_consumed(state);
        return 1;
    }

    trigger = kit_workspace_authoring_trigger_from_key(key, mod_bits);
    if (trigger && strcmp(trigger, "tab") == 0) {
        (void)LineDrawingWorkspaceAuthoringHost_CycleOverlay(state);
        line_drawing_authoring_note_consumed(state);
        return 1;
    }
    if (trigger) {
        line_drawing_authoring_note_consumed(state);
        return 1;
    }

    line_drawing_authoring_note_consumed(state);
    return 1;
}
