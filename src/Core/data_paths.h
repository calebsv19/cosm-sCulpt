#pragma once

#include <stdbool.h>
#include <stddef.h>

#define LINE_DRAWING_PATH_CAP 256

typedef struct LineDrawingDataPaths {
    char input_root[LINE_DRAWING_PATH_CAP];
    char output_root[LINE_DRAWING_PATH_CAP];
    char layout_root[LINE_DRAWING_PATH_CAP];
} LineDrawingDataPaths;

void LineDrawingDataPaths_SetDefaults(LineDrawingDataPaths* paths);
bool LineDrawingDataPaths_Load(LineDrawingDataPaths* paths);
bool LineDrawingDataPaths_Save(const LineDrawingDataPaths* paths);
const char* LineDrawingDataPaths_DefaultInputRoot(void);
const char* LineDrawingDataPaths_DefaultOutputRoot(void);
const char* LineDrawingDataPaths_DefaultLayoutRoot(void);
bool LineDrawingDataPaths_BuildPath(char* out_path,
                                    size_t out_path_size,
                                    const char* root,
                                    const char* leaf_name);
