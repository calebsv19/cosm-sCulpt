#pragma once

#include "Core/data_paths.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum LineDrawingStartupRootKind {
    LINE_DRAWING_STARTUP_ROOT_INPUT = 0,
    LINE_DRAWING_STARTUP_ROOT_OUTPUT = 1,
    LINE_DRAWING_STARTUP_ROOT_LAYOUT = 2,
    LINE_DRAWING_STARTUP_ROOT_OBJECT_ASSET = 3
} LineDrawingStartupRootKind;

typedef enum LineDrawingStartupRootFallbackReason {
    LINE_DRAWING_STARTUP_ROOT_FALLBACK_UNCHANGED = 0,
    LINE_DRAWING_STARTUP_ROOT_FALLBACK_UNSET = 1,
    LINE_DRAWING_STARTUP_ROOT_FALLBACK_MISSING = 2
} LineDrawingStartupRootFallbackReason;

typedef struct LineDrawingStartupRootFallbackEntry {
    LineDrawingStartupRootKind kind;
    LineDrawingStartupRootFallbackReason reason;
    bool changed;
    char prior[LINE_DRAWING_PATH_CAP];
    char fallback[LINE_DRAWING_PATH_CAP];
} LineDrawingStartupRootFallbackEntry;

#define LINE_DRAWING_STARTUP_ROOT_FALLBACK_CAP 4u

typedef struct LineDrawingStartupRootFallbackReport {
    LineDrawingStartupRootFallbackEntry entries[LINE_DRAWING_STARTUP_ROOT_FALLBACK_CAP];
    size_t count;
    bool changed;
} LineDrawingStartupRootFallbackReport;

const char* LineDrawingStartupRootKind_Label(LineDrawingStartupRootKind kind);

bool LineDrawingStartupConfig_ApplyRootFallbacks(
    LineDrawingDataPaths* paths,
    LineDrawingStartupRootFallbackReport* out_report);
