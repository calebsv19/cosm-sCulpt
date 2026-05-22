#pragma once

#include "Menu/line_drawing_catalog_preview.h"
#include "UI/ui_panel.h"

#include <stdbool.h>

typedef enum LineDrawingRootBrowserEntryKind {
    LINE_DRAWING_ROOT_BROWSER_ENTRY_NEARBY_INPUT_ROOT = 0
} LineDrawingRootBrowserEntryKind;

typedef struct LineDrawingRootBrowserEntry {
    LineDrawingRootBrowserEntryKind kind;
    bool enabled;
    int score;
    LineDrawingCatalogPreviewSourceKind preview_kind;
    char label[128];
    char description[160];
    char path[MAX_CONFIG_PATH];
    char preview_path[MAX_CONFIG_PATH];
} LineDrawingRootBrowserEntry;

typedef struct LineDrawingRootBrowser {
    char current_path[MAX_CONFIG_PATH];
    int entry_count;
    int nearby_count;
    LineDrawingRootBrowserEntry entries[MAX_CONFIG_FILES];
} LineDrawingRootBrowser;

void LineDrawingRootBrowser_Init(LineDrawingRootBrowser* browser);
void LineDrawingRootBrowser_Refresh(LineDrawingRootBrowser* browser,
                                    const char* browse_root,
                                    const char* input_root,
                                    const char* output_root);
const LineDrawingRootBrowserEntry* LineDrawingRootBrowser_GetEntry(
    const LineDrawingRootBrowser* browser,
    int index);
