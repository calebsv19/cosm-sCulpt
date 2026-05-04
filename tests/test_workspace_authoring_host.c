#include "test_framework.h"

#include "UI/workspace_authoring/line_drawing_workspace_authoring_host.h"
#include "kit_workspace_authoring_ui.h"

#include <string.h>

static SDL_Event authoring_key_event(Uint32 type,
                                     SDL_Scancode scancode,
                                     SDL_Keycode key,
                                     SDL_Keymod mod) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.key.type = type;
    event.key.keysym.scancode = scancode;
    event.key.keysym.sym = key;
    event.key.keysym.mod = mod;
    return event;
}

static bool authoring_seed_state(GlobalState* state) {
    memset(state, 0, sizeof(*state));
    state->screenWidth = 1280;
    state->screenHeight = 720;
    TEST_ASSERT(LineDrawingPaneHost_Init(&state->paneHost, 1280.0f, 720.0f));
    LineDrawingWorkspaceAuthoringHost_Reset(&state->workspaceAuthoring);
    return true;
}

static bool test_authoring_entry_chord_and_cancel(void) {
    GlobalState state;
    SDL_Event plain_c;
    SDL_Event alt_c;
    SDL_Event alt_v;
    SDL_Event tab;
    SDL_Event escape;
    TEST_ASSERT(authoring_seed_state(&state));

    plain_c = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_NONE);
    TEST_ASSERT(!LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &plain_c));
    TEST_ASSERT(!LineDrawingWorkspaceAuthoringHost_Active(&state));

    alt_c = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_ALT);
    alt_v = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_v, KMOD_ALT);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &alt_c));
    TEST_ASSERT(!LineDrawingWorkspaceAuthoringHost_Active(&state));
    TEST_ASSERT(state.workspaceAuthoring.entry_chord_armed_key != 0u);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &alt_v));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_Active(&state));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(&state));
    TEST_ASSERT(state.workspaceAuthoring.enter_count == 1u);
    TEST_ASSERT(state.workspaceAuthoring.draft_baseline_valid == 1u);

    tab = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_TAB, SDLK_TAB, KMOD_NONE);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &tab));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_FontThemeOverlayActive(&state));
    TEST_ASSERT(state.workspaceAuthoring.overlay_cycle_count == 1u);

    escape = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, KMOD_NONE);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &escape));
    TEST_ASSERT(!LineDrawingWorkspaceAuthoringHost_Active(&state));
    TEST_ASSERT(state.workspaceAuthoring.cancel_count == 1u);
    TEST_ASSERT(state.workspaceAuthoring.draft_baseline_valid == 0u);
    return true;
}

static bool test_authoring_sequential_physical_chord_and_apply(void) {
    GlobalState state;
    SDL_Event alt_c_down;
    SDL_Event alt_c_up;
    SDL_Event alt_v_down;
    SDL_Event enter;
    TEST_ASSERT(authoring_seed_state(&state));

    alt_c_down = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_UNKNOWN, KMOD_ALT);
    alt_c_up = authoring_key_event(SDL_KEYUP, SDL_SCANCODE_C, SDLK_UNKNOWN, KMOD_ALT);
    alt_v_down = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_UNKNOWN, KMOD_ALT);
    enter = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_RETURN, SDLK_RETURN, KMOD_NONE);

    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &alt_c_down));
    TEST_ASSERT(!LineDrawingWorkspaceAuthoringHost_Active(&state));
    TEST_ASSERT(!LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &alt_c_up));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &alt_v_down));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_Active(&state));
    TEST_ASSERT(state.workspaceAuthoring.enter_count == 1u);

    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &enter));
    TEST_ASSERT(!LineDrawingWorkspaceAuthoringHost_Active(&state));
    TEST_ASSERT(state.workspaceAuthoring.apply_count == 1u);
    TEST_ASSERT(state.workspaceAuthoring.draft_baseline_valid == 0u);
    return true;
}

static bool test_authoring_captures_runtime_events_while_active(void) {
    GlobalState state;
    SDL_Event alt_c;
    SDL_Event alt_v;
    SDL_Event mouse_down;
    TEST_ASSERT(authoring_seed_state(&state));

    alt_c = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_ALT);
    alt_v = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_v, KMOD_ALT);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &alt_c));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &alt_v));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_Active(&state));

    memset(&mouse_down, 0, sizeof(mouse_down));
    mouse_down.type = SDL_MOUSEBUTTONDOWN;
    mouse_down.button.type = SDL_MOUSEBUTTONDOWN;
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &mouse_down));
    TEST_ASSERT(state.workspaceAuthoring.consumed_event_count >= 3u);
    return true;
}

static void authoring_button_point(const GlobalState* state,
                                   KitWorkspaceAuthoringOverlayButtonId button_id,
                                   int* out_x,
                                   int* out_y) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    uint32_t count = kit_workspace_authoring_ui_build_overlay_buttons(
        state->screenWidth,
        LineDrawingWorkspaceAuthoringHost_Active(state),
        LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(state),
        buttons,
        4u);
    uint32_t i;
    for (i = 0u; i < count; ++i) {
        if (buttons[i].id == button_id) {
            *out_x = (int)(buttons[i].rect.x + buttons[i].rect.width * 0.5f);
            *out_y = (int)(buttons[i].rect.y + buttons[i].rect.height * 0.5f);
            return;
        }
    }
    *out_x = 0;
    *out_y = 0;
}

static SDL_Event authoring_mouse_down(int x, int y) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x;
    event.button.y = y;
    return event;
}

static bool authoring_font_theme_button_point(int width,
                                              int height,
                                              KitWorkspaceAuthoringFontThemeButtonId button_id,
                                              int* out_x,
                                              int* out_y) {
    KitWorkspaceAuthoringFontThemeLayout layout;
    memset(&layout, 0, sizeof(layout));
    TEST_ASSERT(kit_workspace_authoring_ui_font_theme_build_layout(NULL, width, height, &layout));
    switch (button_id) {
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_CUSTOM_THEME_CREATE_STUB:
            *out_x = (int)(layout.custom_theme_buttons[0].x +
                           layout.custom_theme_buttons[0].width * 0.5f);
            *out_y = (int)(layout.custom_theme_buttons[0].y +
                           layout.custom_theme_buttons[0].height * 0.5f);
            return true;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC:
            *out_x = (int)(layout.text_size_inc_button.x +
                           layout.text_size_inc_button.width * 0.5f);
            *out_y = (int)(layout.text_size_inc_button.y +
                           layout.text_size_inc_button.height * 0.5f);
            return true;
        default:
            break;
    }
    *out_x = 0;
    *out_y = 0;
    return false;
}

static bool test_authoring_overlay_buttons_control_state(void) {
    GlobalState state;
    SDL_Event alt_c;
    SDL_Event alt_v;
    SDL_Event click;
    int x = 0;
    int y = 0;
    TEST_ASSERT(authoring_seed_state(&state));

    alt_c = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_ALT);
    alt_v = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_v, KMOD_ALT);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &alt_c));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &alt_v));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_PaneOverlayActive(&state));

    authoring_button_point(&state, KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE, &x, &y);
    TEST_ASSERT(x > 0 && y > 0);
    click = authoring_mouse_down(x, y);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &click));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_FontThemeOverlayActive(&state));
    TEST_ASSERT(state.workspaceAuthoring.overlay_button_click_count == 1u);

    authoring_button_point(&state, KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL, &x, &y);
    TEST_ASSERT(x > 0 && y > 0);
    click = authoring_mouse_down(x, y);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &click));
    TEST_ASSERT(!LineDrawingWorkspaceAuthoringHost_Active(&state));
    TEST_ASSERT(state.workspaceAuthoring.cancel_count == 1u);
    return true;
}

static bool test_authoring_font_theme_overlay_hit_uses_shared_layout(void) {
    GlobalState state;
    SDL_Event alt_c;
    SDL_Event alt_v;
    SDL_Event tab;
    SDL_Event click;
    int x = 0;
    int y = 0;
    TEST_ASSERT(authoring_seed_state(&state));

    alt_c = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_ALT);
    alt_v = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_v, KMOD_ALT);
    tab = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_TAB, SDLK_TAB, KMOD_NONE);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &alt_c));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &alt_v));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &tab));
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_FontThemeOverlayActive(&state));

    TEST_ASSERT(authoring_font_theme_button_point(
        state.screenWidth,
        state.screenHeight,
        KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_CUSTOM_THEME_CREATE_STUB,
        &x,
        &y));
    TEST_ASSERT(x > 0 && y > 0);
    click = authoring_mouse_down(x, y);
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_HandleSdlEvent(&state, &click));
    TEST_ASSERT(state.workspaceAuthoring.font_theme_button_click_count == 1u);
    TEST_ASSERT(state.workspaceAuthoring.font_theme_status[0] != '\0');
    TEST_ASSERT(LineDrawingWorkspaceAuthoringHost_FontThemeOverlayActive(&state));
    return true;
}

bool workspace_authoring_host_run_tests(void) {
    const TestCase cases[] = {
        {"entry chord and cancel", test_authoring_entry_chord_and_cancel},
        {"sequential physical chord and apply", test_authoring_sequential_physical_chord_and_apply},
        {"runtime events captured while active", test_authoring_captures_runtime_events_while_active},
        {"overlay buttons control state", test_authoring_overlay_buttons_control_state},
        {"font theme overlay hit uses shared layout", test_authoring_font_theme_overlay_hit_uses_shared_layout},
    };
    return run_test_cases("WorkspaceAuthoringHost", cases, sizeof(cases) / sizeof(cases[0]));
}
