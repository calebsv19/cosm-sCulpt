#include "UI/shared_theme_font_adapter.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static int fail(const char* msg) {
    fprintf(stderr, "shared_theme_font_adapter_test: %s\n", msg);
    return 1;
}

int main(void) {
    LineDrawing3dThemePalette palette = {0};
    char path[256] = {0};
    int point_size = 0;
    size_t i = 0;
    const char* theme_presets[] = {
        "studio_blue",
        "harbor_blue",
        "midnight_contrast",
        "soft_light",
        "standard_grey",
        "greyscale"
    };

    unsetenv("LINE_DRAWING3D_USE_SHARED_THEME_FONT");
    unsetenv("LINE_DRAWING3D_USE_SHARED_THEME");
    unsetenv("LINE_DRAWING3D_USE_SHARED_FONT");
    unsetenv("LINE_DRAWING3D_THEME_PRESET");
    unsetenv("LINE_DRAWING3D_FONT_PRESET");

    if (!line_drawing3d_shared_theme_resolve_palette(&palette)) {
        return fail("theme should resolve by default");
    }
    if (!line_drawing3d_shared_font_resolve_ui_regular(path, sizeof(path), &point_size)) {
        return fail("font should resolve by default");
    }

    setenv("LINE_DRAWING3D_USE_SHARED_THEME_FONT", "1", 1);
    setenv("LINE_DRAWING3D_THEME_PRESET", "standard_grey", 1);
    if (!line_drawing3d_shared_theme_resolve_palette(&palette)) {
        return fail("theme should resolve when shared toggle is enabled");
    }

    setenv("LINE_DRAWING3D_USE_SHARED_FONT", "0", 1);
    if (line_drawing3d_shared_font_resolve_ui_regular(path, sizeof(path), &point_size)) {
        return fail("font should be disabled via per-domain toggle");
    }

    setenv("LINE_DRAWING3D_USE_SHARED_FONT", "1", 1);
    setenv("LINE_DRAWING3D_FONT_PRESET", "studio_blue", 1);
    if (!line_drawing3d_shared_font_resolve_ui_regular(path, sizeof(path), &point_size)) {
        return fail("font should resolve when enabled");
    }
    if (path[0] == '\0' || point_size <= 0) {
        return fail("font resolution returned invalid path or point size");
    }

    for (i = 0; i < sizeof(theme_presets) / sizeof(theme_presets[0]); ++i) {
        setenv("LINE_DRAWING3D_THEME_PRESET", theme_presets[i], 1);
        if (!line_drawing3d_shared_theme_resolve_palette(&palette)) {
            return fail("theme preset matrix should resolve");
        }
    }

    puts("shared_theme_font_adapter_test: success");
    return 0;
}
