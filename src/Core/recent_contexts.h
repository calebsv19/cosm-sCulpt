#pragma once

#include "Core/data_paths.h"

#include <stdbool.h>

#define LINE_DRAWING_RECENT_CONTEXT_LIMIT 8

typedef struct LineDrawingRecentPathList {
    int count;
    char paths[LINE_DRAWING_RECENT_CONTEXT_LIMIT][LINE_DRAWING_PATH_CAP];
} LineDrawingRecentPathList;

typedef struct LineDrawingRecentContexts {
    LineDrawingRecentPathList layouts;
    LineDrawingRecentPathList scenes;
    LineDrawingRecentPathList object_assets;
    LineDrawingRecentPathList input_roots;
    LineDrawingRecentPathList output_roots;
} LineDrawingRecentContexts;

void LineDrawingRecentContexts_Init(LineDrawingRecentContexts* contexts);
bool LineDrawingRecentContexts_Load(LineDrawingRecentContexts* contexts);
bool LineDrawingRecentContexts_Save(const LineDrawingRecentContexts* contexts);
bool LineDrawingRecentContexts_TrackLayout(LineDrawingRecentContexts* contexts, const char* path);
bool LineDrawingRecentContexts_TrackScene(LineDrawingRecentContexts* contexts, const char* path);
bool LineDrawingRecentContexts_TrackObjectAsset(LineDrawingRecentContexts* contexts, const char* path);
bool LineDrawingRecentContexts_TrackInputRoot(LineDrawingRecentContexts* contexts, const char* path);
bool LineDrawingRecentContexts_TrackOutputRoot(LineDrawingRecentContexts* contexts, const char* path);
