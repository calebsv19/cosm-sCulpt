#pragma once

#include "Core/line_drawing_file_catalog.h"
#include "UI/ui_panel.h"

#include <stdbool.h>

typedef LineDrawingFileCatalogEntry LineDrawingSceneCatalogEntry;

typedef struct LineDrawingSceneCatalog {
    char input_root[MAX_CONFIG_PATH];
    int layout_count;
    int scene_count;
    int active_layout_index;
    int active_scene_index;
    LineDrawingSceneCatalogEntry layouts[MAX_CONFIG_FILES];
    LineDrawingSceneCatalogEntry scenes[MAX_CONFIG_FILES];
} LineDrawingSceneCatalog;

void LineDrawingSceneCatalog_Init(LineDrawingSceneCatalog* catalog);
void LineDrawingSceneCatalog_Refresh(LineDrawingSceneCatalog* catalog,
                                     const char* input_root,
                                     const char* current_layout_path,
                                     const char* current_scene_path);
bool LineDrawingSceneCatalog_EntryMatchesQuery(const LineDrawingSceneCatalogEntry* entry,
                                               const char* query);
const LineDrawingSceneCatalogEntry* LineDrawingSceneCatalog_GetLayout(
    const LineDrawingSceneCatalog* catalog,
    int index);
const LineDrawingSceneCatalogEntry* LineDrawingSceneCatalog_GetScene(
    const LineDrawingSceneCatalog* catalog,
    int index);
