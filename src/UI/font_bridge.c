#include "UI/font_bridge.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "core_font.h"
#include "core_io.h"

typedef struct LineDrawingFontBridgeSlotSpec {
    CoreFontRoleId role_id;
    CoreFontTextSizeTier text_tier;
    int min_point_size;
    const char* const* legacy_paths;
    size_t legacy_path_count;
} LineDrawingFontBridgeSlotSpec;

static const char* const k_default_legacy_paths[] = {
    "include/fonts/Lato/Lato-Regular.ttf",
    "third_party/codework_shared/assets/fonts/Lato-Regular.ttf"
};

static const char* const k_mono_legacy_paths[] = {
    "include/fonts/Lato/Lato-Regular.ttf",
    "third_party/codework_shared/assets/fonts/Lato-Regular.ttf"
};

static const char* const k_serif_legacy_paths[] = {
    "include/fonts/Lato/Lato-Regular.ttf",
    "third_party/codework_shared/assets/fonts/Lato-Regular.ttf"
};

static int g_font_runtime_initialized = 0;
static CoreFontPresetId g_font_runtime_preset = CORE_FONT_PRESET_IDE;

static CoreResult line_drawing_font_bridge_invalid(const char* message) {
    CoreResult r = {CORE_ERR_INVALID_ARG, message};
    return r;
}

static int line_drawing_font_bridge_parse_bool_env(const char* value, int* out_value) {
    char lowered[16];
    size_t i = 0;

    if (!value || !value[0] || !out_value) {
        return 0;
    }

    for (; value[i] && i < sizeof(lowered) - 1; ++i) {
        lowered[i] = (char)tolower((unsigned char)value[i]);
    }
    lowered[i] = '\0';

    if (strcmp(lowered, "1") == 0 || strcmp(lowered, "true") == 0 || strcmp(lowered, "yes") == 0 ||
        strcmp(lowered, "on") == 0) {
        *out_value = 1;
        return 1;
    }
    if (strcmp(lowered, "0") == 0 || strcmp(lowered, "false") == 0 || strcmp(lowered, "no") == 0 ||
        strcmp(lowered, "off") == 0) {
        *out_value = 0;
        return 1;
    }
    return 0;
}

static int line_drawing_font_bridge_shared_enabled(void) {
    int enabled = 1;

    if (line_drawing_font_bridge_parse_bool_env(getenv("LINE_DRAWING3D_USE_SHARED_FONT"), &enabled)) {
        return enabled;
    }
    if (line_drawing_font_bridge_parse_bool_env(getenv("LINE_DRAWING3D_USE_SHARED_THEME_FONT"), &enabled)) {
        return enabled;
    }
    return 1;
}

static void line_drawing_font_bridge_runtime_init_if_needed(void) {
    const char* preset_name = getenv("LINE_DRAWING3D_FONT_PRESET");
    CoreFontPreset preset;

    if (g_font_runtime_initialized) {
        return;
    }
    if (preset_name && preset_name[0] &&
        core_font_get_preset_by_name(preset_name, &preset).code == CORE_OK) {
        g_font_runtime_preset = preset.id;
    } else {
        g_font_runtime_preset = CORE_FONT_PRESET_IDE;
    }
    g_font_runtime_initialized = 1;
}

CoreFontPresetId line_drawing_font_bridge_current_preset_id(void) {
    line_drawing_font_bridge_runtime_init_if_needed();
    return g_font_runtime_preset;
}

const char* line_drawing_font_bridge_current_preset_name(void) {
    line_drawing_font_bridge_runtime_init_if_needed();
    return core_font_preset_name(g_font_runtime_preset);
}

CoreResult line_drawing_font_bridge_set_preset_id(CoreFontPresetId preset_id) {
    CoreFontPreset preset;
    CoreResult result = core_font_get_preset(preset_id, &preset);
    if (result.code != CORE_OK) {
        return result;
    }
    g_font_runtime_preset = preset_id;
    g_font_runtime_initialized = 1;
    return core_result_ok();
}

CoreResult line_drawing_font_bridge_set_preset_name(const char* preset_name) {
    CoreFontPreset preset;
    if (!preset_name || !preset_name[0]) {
        return line_drawing_font_bridge_invalid("missing font preset name");
    }
    if (core_font_get_preset_by_name(preset_name, &preset).code != CORE_OK) {
        return line_drawing_font_bridge_invalid("unknown font preset name");
    }
    return line_drawing_font_bridge_set_preset_id(preset.id);
}

static CoreResult line_drawing_font_bridge_context_init(KitRenderContext* ctx, int zoom_step) {
    CoreResult result;

    if (!ctx) {
        return line_drawing_font_bridge_invalid("null context");
    }

    result = kit_render_context_init(ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_GREYSCALE,
                                     line_drawing_font_bridge_current_preset_id());
    if (result.code != CORE_OK) {
        return result;
    }

    result = kit_render_set_text_zoom_step(ctx, zoom_step);
    if (result.code != CORE_OK) {
        kit_render_context_shutdown(ctx);
        return result;
    }

    return core_result_ok();
}

static const LineDrawingFontBridgeSlotSpec* line_drawing_font_bridge_slot_spec(LineDrawingFontSlot slot) {
    static const LineDrawingFontBridgeSlotSpec specs[] = {
        {
            CORE_FONT_ROLE_UI_REGULAR,
            CORE_FONT_TEXT_SIZE_BASIC,
            8,
            k_default_legacy_paths,
            sizeof(k_default_legacy_paths) / sizeof(k_default_legacy_paths[0])
        },
        {
            CORE_FONT_ROLE_UI_MONO,
            CORE_FONT_TEXT_SIZE_BASIC,
            8,
            k_mono_legacy_paths,
            sizeof(k_mono_legacy_paths) / sizeof(k_mono_legacy_paths[0])
        },
        {
            CORE_FONT_ROLE_UI_REGULAR,
            CORE_FONT_TEXT_SIZE_BASIC,
            8,
            k_serif_legacy_paths,
            sizeof(k_serif_legacy_paths) / sizeof(k_serif_legacy_paths[0])
        }
    };

    if ((int)slot < 0 || (size_t)slot >= (sizeof(specs) / sizeof(specs[0]))) {
        return NULL;
    }
    return &specs[(size_t)slot];
}

static void line_drawing_font_bridge_copy_text(char* dst, size_t dst_cap, const char* src) {
    if (!dst || dst_cap == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_cap - 1);
    dst[dst_cap - 1] = '\0';
}

static int line_drawing_font_bridge_try_copy_existing(char* dst, size_t dst_cap, const char* candidate) {
    if (!candidate || !candidate[0]) {
        return 0;
    }
    if (!core_io_path_exists(candidate)) {
        return 0;
    }
    line_drawing_font_bridge_copy_text(dst, dst_cap, candidate);
    return 1;
}

static int line_drawing_font_bridge_copy_resolved_path(const char* candidate,
                                                       char* dst,
                                                       size_t dst_cap) {
    char adjusted[384];

    if (line_drawing_font_bridge_try_copy_existing(dst, dst_cap, candidate)) {
        return 1;
    }

    if (candidate && strncmp(candidate, "shared/", 7) == 0) {
        snprintf(adjusted, sizeof(adjusted), "third_party/codework_shared/%s", candidate + 7);
        if (line_drawing_font_bridge_try_copy_existing(dst, dst_cap, adjusted)) {
            return 1;
        }
    }

    return 0;
}

CoreResult line_drawing_font_bridge_resolve(LineDrawingFontSlot slot,
                                            int zoom_step,
                                            LineDrawingResolvedFont* out_resolved) {
    const LineDrawingFontBridgeSlotSpec* spec = line_drawing_font_bridge_slot_spec(slot);
    KitRenderContext render_ctx;
    KitRenderResolvedTextRun text_run;
    CoreResult result;
    size_t i = 0;

    if (!spec || !out_resolved) {
        return line_drawing_font_bridge_invalid("invalid font bridge resolve request");
    }

    memset(out_resolved, 0, sizeof(*out_resolved));

    result = line_drawing_font_bridge_context_init(&render_ctx, zoom_step);
    if (result.code != CORE_OK) {
        return result;
    }

    result = kit_render_resolve_text_run(&render_ctx,
                                         spec->role_id,
                                         spec->text_tier,
                                         1.0f,
                                         &text_run);
    kit_render_context_shutdown(&render_ctx);
    if (result.code != CORE_OK) {
        return result;
    }

    if (text_run.logical_point_size < spec->min_point_size) {
        text_run.logical_point_size = spec->min_point_size;
    }
    text_run.raster_point_size = text_run.logical_point_size;
    text_run.raster_scale = 1.0f;
    text_run.upload_filter = KIT_RENDER_TEXT_UPLOAD_FILTER_LINEAR;

    if (line_drawing_font_bridge_shared_enabled()) {
        if (line_drawing_font_bridge_copy_resolved_path(text_run.role_spec.primary_path,
                                                        out_resolved->resolved_path,
                                                        sizeof(out_resolved->resolved_path)) ||
            line_drawing_font_bridge_copy_resolved_path(text_run.role_spec.fallback_path,
                                                        out_resolved->resolved_path,
                                                        sizeof(out_resolved->resolved_path))) {
            out_resolved->used_shared_font = 1;
        }
    }

    if (!out_resolved->resolved_path[0]) {
        for (i = 0; i < spec->legacy_path_count; ++i) {
            if (line_drawing_font_bridge_copy_resolved_path(spec->legacy_paths[i],
                                                            out_resolved->resolved_path,
                                                            sizeof(out_resolved->resolved_path))) {
                out_resolved->used_shared_font = 0;
                break;
            }
        }
    }

    out_resolved->text_run = text_run;
    if (!out_resolved->resolved_path[0]) {
        return line_drawing_font_bridge_invalid("failed to resolve bridge font path");
    }
    return core_result_ok();
}
