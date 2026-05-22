#pragma once

#include "Layout/layout.h"
#include "UI/ui_panel.h"

#include <stdbool.h>

typedef enum LineDrawingCatalogPreviewSourceKind {
    LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT = 0,
    LINE_DRAWING_CATALOG_PREVIEW_SOURCE_SCENE
} LineDrawingCatalogPreviewSourceKind;

typedef struct LineDrawingCatalogPreviewSegment {
    float x0;
    float y0;
    float x1;
    float y1;
} LineDrawingCatalogPreviewSegment;

enum {
    LINE_DRAWING_CATALOG_PREVIEW_MAX_SEGMENTS = 256,
    LINE_DRAWING_CATALOG_PREVIEW_CACHE_CAPACITY = 64
};

typedef struct LineDrawingCatalogPreviewData {
    bool loaded;
    bool load_failed;
    bool has_preview;
    int object_count;
    int plane_count;
    int rect_prism_count;
    int anchor_count;
    int wall_count;
    float extent_x;
    float extent_y;
    float extent_z;
    int segment_count;
    char diagnostics[128];
    LineDrawingCatalogPreviewSegment segments[LINE_DRAWING_CATALOG_PREVIEW_MAX_SEGMENTS];
} LineDrawingCatalogPreviewData;

typedef struct LineDrawingCatalogPreviewCacheEntry {
    bool occupied;
    LineDrawingCatalogPreviewSourceKind kind;
    char path[MAX_CONFIG_PATH];
    LineDrawingCatalogPreviewData data;
} LineDrawingCatalogPreviewCacheEntry;

typedef struct LineDrawingCatalogPreviewCache {
    int next_replace_index;
    LineDrawingCatalogPreviewCacheEntry entries[LINE_DRAWING_CATALOG_PREVIEW_CACHE_CAPACITY];
} LineDrawingCatalogPreviewCache;

void LineDrawingCatalogPreviewCache_Init(LineDrawingCatalogPreviewCache* cache);
const LineDrawingCatalogPreviewData* LineDrawingCatalogPreviewCache_Get(
    LineDrawingCatalogPreviewCache* cache,
    LineDrawingCatalogPreviewSourceKind kind,
    const char* path);
bool LineDrawingCatalogPreview_Load(LineDrawingCatalogPreviewData* out_preview,
                                    LineDrawingCatalogPreviewSourceKind kind,
                                    const char* path);
