#include "UI/platform/line_drawing_folder_picker.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__) && !defined(LINE_DRAWING_FOLDER_PICKER_FORCE_LINUX)
#define LINE_DRAWING_FOLDER_PICKER_MACOS 1
#else
#define LINE_DRAWING_FOLDER_PICKER_MACOS 0
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if !LINE_DRAWING_FOLDER_PICKER_MACOS
#include "UI/platform/line_drawing_picker_process.h"
#endif

#if LINE_DRAWING_FOLDER_PICKER_MACOS
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdint.h>

/* Keep the modal event loop in the app process. A child script leaves SDL's
 * Cocoa host unable to answer accessibility or normal window events. */
static id picker_object(id receiver, const char* selector) {
    return ((id (*)(id, SEL))objc_msgSend)(receiver, sel_registerName(selector));
}

static void picker_set_object(id receiver, const char* selector, id value) {
    ((void (*)(id, SEL, id))objc_msgSend)(receiver, sel_registerName(selector), value);
}

static void picker_set_bool(id receiver, const char* selector, BOOL value) {
    ((void (*)(id, SEL, BOOL))objc_msgSend)(receiver, sel_registerName(selector), value);
}

static id picker_string(const char* text) {
    return ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), text);
}

static LineDrawingFolderPickerResult select_macos_folder(const char* prompt,
                                                         const char* initial_directory,
                                                         char* out_path,
                                                         size_t out_path_size) {
    id pool = picker_object(picker_object((id)objc_getClass("NSAutoreleasePool"), "alloc"), "init");
    id panel = picker_object((id)objc_getClass("NSOpenPanel"), "openPanel");
    LineDrawingFolderPickerResult result = LINE_DRAWING_FOLDER_PICKER_FAILED;
    if (panel) {
        picker_set_object(panel, "setTitle:", picker_string(prompt));
        picker_set_bool(panel, "setCanChooseDirectories:", YES);
        picker_set_bool(panel, "setCanChooseFiles:", NO);
        picker_set_bool(panel, "setAllowsMultipleSelection:", NO);
        if (initial_directory && initial_directory[0]) {
            id url = ((id (*)(id, SEL, id))objc_msgSend)(
                (id)objc_getClass("NSURL"), sel_registerName("fileURLWithPath:"), picker_string(initial_directory));
            picker_set_object(panel, "setDirectoryURL:", url);
        }
        intptr_t response = ((intptr_t (*)(id, SEL))objc_msgSend)(panel, sel_registerName("runModal"));
        result = LINE_DRAWING_FOLDER_PICKER_CANCELLED;
        if (response == 1) {
            id url = picker_object(panel, "URL");
            const char* path = ((const char* (*)(id, SEL))objc_msgSend)(url, sel_registerName("fileSystemRepresentation"));
            result = LINE_DRAWING_FOLDER_PICKER_FAILED;
            if (path && path[0] && strlen(path) < out_path_size) {
                memcpy(out_path, path, strlen(path) + 1u);
                result = LINE_DRAWING_FOLDER_PICKER_SELECTED;
            }
        }
    }
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("drain"));
    return result;
}
#else
static LineDrawingFolderPickerResult select_linux_folder(const char* prompt,
                                                         const char* initial_directory,
                                                         char* out_path,
                                                         size_t out_path_size) {
    const char* zenity_argv[8] = {"zenity", "--file-selection", "--directory", "--title", prompt, NULL, NULL, NULL};
    const char* kdialog_argv[7] = {"kdialog", "--getexistingdirectory", NULL, "--title", prompt, NULL, NULL};
    LineDrawingFolderPickerResult result;

    if (initial_directory && initial_directory[0]) {
        zenity_argv[5] = "--filename";
        zenity_argv[6] = initial_directory;
        kdialog_argv[2] = initial_directory;
    } else {
        kdialog_argv[2] = ".";
    }
    result = LineDrawing_PickerProcess_Run(zenity_argv, out_path, out_path_size);
    if (result != LINE_DRAWING_FOLDER_PICKER_UNAVAILABLE) return result;
    return LineDrawing_PickerProcess_Run(kdialog_argv, out_path, out_path_size);
}
#endif

LineDrawingFolderPickerResult LineDrawing_FolderPicker_Select(const char* prompt,
                                                              const char* initial_directory,
                                                              char* out_path,
                                                              size_t out_path_size) {
    if (!prompt || !prompt[0] || !out_path || out_path_size < 2u) {
        return LINE_DRAWING_FOLDER_PICKER_FAILED;
    }
    out_path[0] = '\0';
#if LINE_DRAWING_FOLDER_PICKER_MACOS
    return select_macos_folder(prompt, initial_directory, out_path, out_path_size);
#elif defined(__linux__) || defined(LINE_DRAWING_FOLDER_PICKER_FORCE_LINUX)
    return select_linux_folder(prompt, initial_directory, out_path, out_path_size);
#else
    (void)initial_directory;
    return LINE_DRAWING_FOLDER_PICKER_UNAVAILABLE;
#endif
}
