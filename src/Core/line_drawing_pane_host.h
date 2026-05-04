#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core_layout.h"
#include "core_pane.h"
#include "core_pane_module.h"
#include "kit_pane.h"

typedef enum LineDrawingPaneRole {
    LINE_DRAWING_PANE_ROLE_TOP_BAR = 0,
    LINE_DRAWING_PANE_ROLE_LEFT_CONTROLS = 1,
    LINE_DRAWING_PANE_ROLE_CENTER_CANVAS = 2,
    LINE_DRAWING_PANE_ROLE_RIGHT_CONTROLS = 3
} LineDrawingPaneRole;

enum {
    LINE_DRAWING_PANE_NODE_CAPACITY = 12u,
    LINE_DRAWING_PANE_LEAF_CAPACITY = 8u,
    LINE_DRAWING_PANE_MODULE_REGISTRY_CAPACITY = 8u,
    LINE_DRAWING_PANE_MODULE_BINDING_CAPACITY = 8u
};

typedef struct LineDrawingPaneHost {
    CoreLayoutState layout_state;
    CorePaneNode nodes[LINE_DRAWING_PANE_NODE_CAPACITY];
    uint32_t node_count;
    uint32_t root_index;
    CorePaneLeafRect leaves[LINE_DRAWING_PANE_LEAF_CAPACITY];
    uint32_t leaf_count;
    CorePaneModuleDescriptor module_entries[LINE_DRAWING_PANE_MODULE_REGISTRY_CAPACITY];
    CorePaneModuleRegistry module_registry;
    CorePaneModuleBinding module_bindings[LINE_DRAWING_PANE_MODULE_BINDING_CAPACITY];
    uint32_t module_binding_count;
    float bounds_width;
    float bounds_height;
    float target_top_height;
    float target_left_width;
    float target_right_width;
    KitPaneSplitterInteraction splitter_interaction;
    bool initialized;
    char last_error[160];
} LineDrawingPaneHost;

bool LineDrawingPaneHost_Init(LineDrawingPaneHost* host, float width, float height);
bool LineDrawingPaneHost_Rebuild(LineDrawingPaneHost* host, float width, float height);
void LineDrawingPaneHost_SetChromeTargets(LineDrawingPaneHost* host,
                                          float top_height,
                                          float left_width,
                                          float right_width);
void LineDrawingPaneHost_UpdatePointer(LineDrawingPaneHost* host, float pointer_x, float pointer_y);
bool LineDrawingPaneHost_BeginSplitterDrag(LineDrawingPaneHost* host, float pointer_x, float pointer_y);
bool LineDrawingPaneHost_UpdateSplitterDrag(LineDrawingPaneHost* host, float pointer_x, float pointer_y);
void LineDrawingPaneHost_EndSplitterDrag(LineDrawingPaneHost* host);
bool LineDrawingPaneHost_IsSplitterDragActive(const LineDrawingPaneHost* host);
bool LineDrawingPaneHost_GetVisibleSplitter(const LineDrawingPaneHost* host,
                                            CorePaneRect* out_rect,
                                            bool* out_hovered,
                                            bool* out_active);
bool LineDrawingPaneHost_GetRectForRole(const LineDrawingPaneHost* host,
                                        LineDrawingPaneRole role,
                                        CorePaneRect* out_rect);
const char* LineDrawingPaneHost_LastError(const LineDrawingPaneHost* host);
