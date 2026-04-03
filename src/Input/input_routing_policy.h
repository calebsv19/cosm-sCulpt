#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum LineDrawingInputActionClass {
    LINE_DRAWING_INPUT_ACTION_IGNORED = 0,
    LINE_DRAWING_INPUT_ACTION_IMMEDIATE,
    LINE_DRAWING_INPUT_ACTION_QUEUED
} LineDrawingInputActionClass;

typedef enum LineDrawingInputRoutePolicy {
    LINE_DRAWING_INPUT_ROUTE_NONE = 0,
    LINE_DRAWING_INPUT_ROUTE_GLOBAL,
    LINE_DRAWING_INPUT_ROUTE_FOCUSED,
    LINE_DRAWING_INPUT_ROUTE_FALLBACK
} LineDrawingInputRoutePolicy;

typedef struct LineDrawingInputPolicyDecision {
    LineDrawingInputActionClass action_class;
    LineDrawingInputRoutePolicy route_policy;
    bool text_entry_capture;
    bool global_shortcuts_allowed;
    bool global_shortcut_blocked;
    bool request_quit;
} LineDrawingInputPolicyDecision;

LineDrawingInputPolicyDecision LineDrawingInputPolicyNormalize(const SDL_Event* event,
                                                               bool text_entry_capture);
