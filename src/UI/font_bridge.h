#pragma once

#include "core_base.h"
#include "core_font.h"
#include "kit_render.h"

typedef enum LineDrawingFontSlot {
    LINE_DRAWING_FONT_SLOT_DEFAULT = 0,
    LINE_DRAWING_FONT_SLOT_MONO = 1,
    LINE_DRAWING_FONT_SLOT_SERIF = 2,
    LINE_DRAWING_FONT_SLOT_COUNT = 3
} LineDrawingFontSlot;

typedef struct LineDrawingResolvedFont {
    KitRenderResolvedTextRun text_run;
    char resolved_path[384];
    int used_shared_font;
} LineDrawingResolvedFont;

CoreResult line_drawing_font_bridge_resolve(LineDrawingFontSlot slot,
                                            int zoom_step,
                                            LineDrawingResolvedFont* out_resolved);
CoreFontPresetId line_drawing_font_bridge_current_preset_id(void);
const char* line_drawing_font_bridge_current_preset_name(void);
CoreResult line_drawing_font_bridge_set_preset_id(CoreFontPresetId preset_id);
CoreResult line_drawing_font_bridge_set_preset_name(const char* preset_name);
