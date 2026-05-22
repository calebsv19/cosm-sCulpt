#pragma once

#include "Core/recent_contexts.h"
#include "UI/ui_panel.h"

#include <stdbool.h>

typedef enum LineDrawingRecentMenuEntryKind {
    LINE_DRAWING_RECENT_MENU_ENTRY_LAYOUT = 0,
    LINE_DRAWING_RECENT_MENU_ENTRY_SCENE,
    LINE_DRAWING_RECENT_MENU_ENTRY_INPUT_ROOT,
    LINE_DRAWING_RECENT_MENU_ENTRY_OUTPUT_ROOT
} LineDrawingRecentMenuEntryKind;

typedef struct LineDrawingRecentMenuEntry {
    LineDrawingRecentMenuEntryKind kind;
    bool current;
    char label[128];
    char description[160];
    char path[MAX_CONFIG_PATH];
} LineDrawingRecentMenuEntry;

typedef struct LineDrawingRecentMenuList {
    int entry_count;
    int layout_count;
    int scene_count;
    int input_root_count;
    int output_root_count;
    LineDrawingRecentMenuEntry entries[MAX_CONFIG_FILES];
} LineDrawingRecentMenuList;

void LineDrawingRecentMenuList_Init(LineDrawingRecentMenuList* list);
void LineDrawingRecentMenuList_Refresh(LineDrawingRecentMenuList* list,
                                       const LineDrawingRecentContexts* contexts,
                                       const char* current_layout_path,
                                       const char* current_scene_path,
                                       const char* current_input_root,
                                       const char* current_output_root);
const LineDrawingRecentMenuEntry* LineDrawingRecentMenuList_GetEntry(
    const LineDrawingRecentMenuList* list,
    int index);

