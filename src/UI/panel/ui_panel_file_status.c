#include "UI/panel/ui_panel_file_browser_internal.h"

#include "Core/line_drawing_file_catalog.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const char* UIPanel_FileStatusDisplayBaseName(const char* path) {
    const char* base = NULL;
    if (!path || !path[0]) return "(unset)";
    base = LineDrawingFileCatalog_PathBasename(path);
    return base && base[0] ? base : "(unset)";
}
const char* UIPanel_FileStatusBrowseModeName(UILoadMenuMode mode) {
    switch (mode) {
        case UI_LOAD_MENU_MODE_JSON: return "JSON";
        case UI_LOAD_MENU_MODE_SCENE: return "Scene";
        case UI_LOAD_MENU_MODE_OBJECT: return "Asset";
        case UI_LOAD_MENU_MODE_RUNTIME_MESH: return "Runtime Mesh";
        case UI_LOAD_MENU_MODE_STL_IMPORT: return "STL";
        case UI_LOAD_MENU_MODE_NONE:
        default: return "Browser";
    }
}

const char* UIPanel_FileStatusSummaryModeName(UILoadMenuMode mode) {
    switch (mode) {
        case UI_LOAD_MENU_MODE_JSON: return "JSON";
        case UI_LOAD_MENU_MODE_SCENE: return "Scene";
        case UI_LOAD_MENU_MODE_OBJECT: return "Asset";
        case UI_LOAD_MENU_MODE_RUNTIME_MESH: return "Mesh";
        case UI_LOAD_MENU_MODE_STL_IMPORT: return "STL";
        case UI_LOAD_MENU_MODE_NONE:
        default: return "Idle";
    }
}

bool UIPanel_FileStatusWriteMessage(char* out_text,
                                    size_t out_text_size,
                                    const char* prefix,
                                    const char* format,
                                    ...) {
    int prefix_len = 0;
    int detail_len = 0;
    va_list args;

    if (!out_text || out_text_size == 0u || !prefix || !prefix[0] || !format) {
        return false;
    }
    out_text[0] = '\0';

    prefix_len = snprintf(out_text, out_text_size, "%s  ", prefix);
    if (prefix_len < 0 || prefix_len >= (int)out_text_size) {
        return false;
    }

    va_start(args, format);
    detail_len = vsnprintf(out_text + prefix_len,
                           out_text_size - (size_t)prefix_len,
                           format,
                           args);
    va_end(args);

    return detail_len >= 0 &&
           prefix_len + detail_len < (int)out_text_size;
}
