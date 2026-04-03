#include "Input/input_routing_policy.h"

static LineDrawingInputPolicyDecision LineDrawingInputPolicyIgnored(void) {
    LineDrawingInputPolicyDecision decision;
    decision.action_class = LINE_DRAWING_INPUT_ACTION_IGNORED;
    decision.route_policy = LINE_DRAWING_INPUT_ROUTE_NONE;
    decision.text_entry_capture = false;
    decision.global_shortcuts_allowed = false;
    decision.global_shortcut_blocked = false;
    decision.request_quit = false;
    return decision;
}

LineDrawingInputPolicyDecision LineDrawingInputPolicyNormalize(const SDL_Event* event,
                                                               bool text_entry_capture) {
    LineDrawingInputPolicyDecision decision = LineDrawingInputPolicyIgnored();
    if (!event) {
        return decision;
    }

    if (event->type == SDL_QUIT ||
        (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE)) {
        decision.action_class = LINE_DRAWING_INPUT_ACTION_IMMEDIATE;
        decision.route_policy = LINE_DRAWING_INPUT_ROUTE_GLOBAL;
        decision.global_shortcuts_allowed = true;
        decision.request_quit = true;
        return decision;
    }

    if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) {
        decision.action_class = LINE_DRAWING_INPUT_ACTION_IMMEDIATE;
        decision.text_entry_capture = text_entry_capture;
        if (text_entry_capture) {
            decision.route_policy = LINE_DRAWING_INPUT_ROUTE_FOCUSED;
            decision.global_shortcuts_allowed = false;
            decision.global_shortcut_blocked = true;
            return decision;
        }

        decision.global_shortcuts_allowed = true;
        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
            decision.route_policy = LINE_DRAWING_INPUT_ROUTE_GLOBAL;
            decision.request_quit = true;
            return decision;
        }

        decision.route_policy = LINE_DRAWING_INPUT_ROUTE_FOCUSED;
        return decision;
    }

    if (event->type == SDL_TEXTINPUT ||
        event->type == SDL_MOUSEBUTTONDOWN ||
        event->type == SDL_MOUSEBUTTONUP ||
        event->type == SDL_MOUSEMOTION ||
        event->type == SDL_MOUSEWHEEL) {
        decision.action_class = LINE_DRAWING_INPUT_ACTION_IMMEDIATE;
        decision.route_policy = LINE_DRAWING_INPUT_ROUTE_FOCUSED;
        return decision;
    }

    return decision;
}
