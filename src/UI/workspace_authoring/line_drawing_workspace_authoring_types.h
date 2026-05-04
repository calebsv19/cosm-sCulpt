#pragma once

#include "Core/line_drawing_pane_host.h"

typedef enum LineDrawingWorkspaceAuthoringOverlayMode {
    LINE_DRAWING_WORKSPACE_AUTHORING_OVERLAY_PANE = 0,
    LINE_DRAWING_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME = 1
} LineDrawingWorkspaceAuthoringOverlayMode;

typedef struct LineDrawingWorkspaceAuthoringHostState {
    int active;
    unsigned char overlay_mode;
    unsigned char key_c_down;
    unsigned char key_v_down;
    unsigned char entry_chord_armed_key;
    unsigned int enter_count;
    unsigned int cancel_count;
    unsigned int apply_count;
    unsigned int overlay_cycle_count;
    unsigned int consumed_event_count;
    unsigned int overlay_button_click_count;
    unsigned int font_theme_button_click_count;
    unsigned int last_event_consumed;
    unsigned int last_event_entered;
    unsigned int last_event_exited;
    char font_theme_status[128];
    int last_pointer_x;
    int last_pointer_y;
    unsigned int last_pointer_ready;
    unsigned int draft_baseline_valid;
    int baseline_font_zoom_step;
    char baseline_theme_preset[64];
    char baseline_font_preset[64];
    uint32_t baseline_node_count;
    uint32_t baseline_root_index;
    float baseline_top_height;
    float baseline_left_width;
    float baseline_right_width;
    CorePaneNode baseline_nodes[LINE_DRAWING_PANE_NODE_CAPACITY];
} LineDrawingWorkspaceAuthoringHostState;
