#pragma once
#include "UI/platform/line_drawing_folder_picker.h"
LineDrawingFolderPickerResult LineDrawing_PickerProcess_Run(const char* const argv[],
                                                           char* path, size_t capacity);
