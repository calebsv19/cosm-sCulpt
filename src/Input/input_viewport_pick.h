#pragma once

#include "Input/input_mouse_internal.h"

#include <stddef.h>

typedef enum {
    VIEWPORT_PICK_REASON_NONE = 0,
    VIEWPORT_PICK_REASON_OUTSIDE_CENTER_PANE,
    VIEWPORT_PICK_REASON_RAW_HIT,
    VIEWPORT_PICK_REASON_EXACT_TOPOLOGY_EDGE,
    VIEWPORT_PICK_REASON_EXACT_TOPOLOGY_VERTEX,
    VIEWPORT_PICK_REASON_FILTERED_BY_OBJECT_EDIT_MODE,
    VIEWPORT_PICK_REASON_BODY_FALLBACK
} ViewportPickReason;

typedef struct {
    PointerPaneLane paneLane;
    Hitbox rawHit;
    Hitbox exactTopologyHit;
    Hitbox filteredHit;
    Hitbox bodyFallbackHit;
    Hitbox finalHit;
    ViewportPickReason reason;
    bool objectEditTopologyModeActive;
} ViewportPickResult;

ViewportPickResult ViewportPick_ResolveObjectWorkspaceHit(GlobalState* state,
                                                          int mouse_x,
                                                          int mouse_y,
                                                          bool require_center_pane);
const ViewportPickResult* ViewportPick_LastResult(int* out_mouse_x,
                                                  int* out_mouse_y);
const char* ViewportPick_HitboxTypeLabel(HitboxType type);
const char* ViewportPick_PaneLaneLabel(PointerPaneLane lane);
const char* ViewportPick_ReasonLabel(ViewportPickReason reason);
bool ViewportPick_FormatLastDebug(char* out,
                                  size_t out_size,
                                  const GlobalState* state);
