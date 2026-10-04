#pragma once

#include <stddef.h>

typedef enum {
    LINE_DRAWING_FOLDER_PICKER_SELECTED = 0,
    LINE_DRAWING_FOLDER_PICKER_CANCELLED,
    LINE_DRAWING_FOLDER_PICKER_UNAVAILABLE,
    LINE_DRAWING_FOLDER_PICKER_FAILED
} LineDrawingFolderPickerResult;

/* Call on the UI thread. macOS uses an app-owned Cocoa modal panel; Linux uses
 * host picker tools without routing prompt or path text through a shell. */
LineDrawingFolderPickerResult LineDrawing_FolderPicker_Select(const char* prompt,
                                                              const char* initial_directory,
                                                              char* out_path,
                                                              size_t out_path_size);
