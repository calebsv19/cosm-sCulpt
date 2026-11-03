#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/font_bridge.h"
#include "core_io.h"
#include "core_theme.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char* style_path = "data/runtime/ui_style.txt";

/* Bound legacy input and reject embedded NUL bytes rather than accepting a prefix. */
static bool read_text(const char* path, char* text, size_t capacity) {
    CoreBuffer data = {0};
    if (core_io_read_all(path, &data).code != CORE_OK) return false;
    bool ok = data.size > 0 && data.size < capacity && !memchr(data.data, 0, data.size);
    if (ok) {
        memcpy(text, data.data, data.size);
        text[data.size] = 0;
    }
    core_io_buffer_free(&data);
    return ok;
}

/* Parse the whole bounded integer without overflow or trailing data. */
static bool parse_zoom(const char* text, int* zoom) {
    char* end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    while (end && isspace((unsigned char)*end)) ++end;
    if (errno || end == text || !end || *end || value < -2 || value > 4) return false;
    *zoom = (int)value;
    return true;
}

/* Load one authoritative atomic style record, or read the pre-existing legacy files. */
bool FontManager_LoadPersistedPrefs(void) {
    char text[320], version[32], preset[64], zoom_text[32], theme[64];
    int zoom = FontManager_GetZoomStep(), used = 0;
    CoreFontPreset font;
    CoreThemePresetId theme_id;
    if (core_io_path_exists(style_path)) {
        if (!read_text(style_path, text, sizeof(text)) ||
            sscanf(text, "%31s %63s %31s %63s %n", version, preset, zoom_text, theme, &used) != 4 ||
            !used || text[used] || strcmp(version, "sculpt-ui-style-v1") ||
            !parse_zoom(zoom_text, &zoom) ||
            core_font_get_preset_by_name(preset, &font).code != CORE_OK ||
            core_theme_preset_id_from_name(theme, &theme_id).code != CORE_OK) return false;
        if (!FontManager_ApplyStyle(font.id, zoom)) return false;
        return line_drawing3d_shared_theme_set_preset(theme);
    }
    bool loaded = false;
    if (read_text("data/runtime/font_preset.txt", text, sizeof(text)) &&
        sscanf(text, "%63s", preset) == 1 &&
        core_font_get_preset_by_name(preset, &font).code == CORE_OK) {
        loaded = line_drawing_font_bridge_set_preset_id(font.id).code == CORE_OK;
    }
    if (read_text("data/runtime/font_zoom_step.txt", text, sizeof(text)) && parse_zoom(text, &zoom)) {
        loaded = FontManager_ApplyStyle(FontManager_GetSharedFontPresetId(), zoom) || loaded;
    }
    return loaded;
}

/* Font, zoom and theme become accepted together through the existing atomic I/O owner. */
bool FontManager_SavePersistedPrefs(void) {
    char theme[64], payload[256];
    if (!line_drawing3d_shared_theme_current_preset(theme, sizeof(theme))) return false;
    int size = snprintf(payload, sizeof(payload), "sculpt-ui-style-v1\n%s\n%d\n%s\n",
                        FontManager_GetSharedFontPresetName(), FontManager_GetZoomStep(), theme);
    if (size < 1 || (size_t)size >= sizeof(payload)) return false;
    if ((mkdir("data", 0755) != 0 && errno != EEXIST) ||
        (mkdir("data/runtime", 0755) != 0 && errno != EEXIST)) return false;
    return core_io_write_all_atomic(style_path, payload, (size_t)size).code == CORE_OK;
}
