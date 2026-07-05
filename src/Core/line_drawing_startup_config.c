#include "Core/line_drawing_startup_config.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

typedef struct LineDrawingStartupRootField {
    LineDrawingStartupRootKind kind;
    char* value;
    size_t value_size;
    const char* fallback;
} LineDrawingStartupRootField;

static bool line_drawing_startup_root_dir_exists(const char* path) {
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

const char* LineDrawingStartupRootKind_Label(LineDrawingStartupRootKind kind) {
    switch (kind) {
        case LINE_DRAWING_STARTUP_ROOT_INPUT:
            return "input root";
        case LINE_DRAWING_STARTUP_ROOT_OUTPUT:
            return "output root";
        case LINE_DRAWING_STARTUP_ROOT_LAYOUT:
            return "layout root";
        case LINE_DRAWING_STARTUP_ROOT_OBJECT_ASSET:
            return "object asset root";
        default:
            return "unknown root";
    }
}

static void line_drawing_startup_record_entry(
    LineDrawingStartupRootFallbackReport* report,
    const LineDrawingStartupRootField* field,
    LineDrawingStartupRootFallbackReason reason,
    bool changed) {
    LineDrawingStartupRootFallbackEntry* entry = NULL;
    if (!report || !field ||
        report->count >= LINE_DRAWING_STARTUP_ROOT_FALLBACK_CAP) {
        return;
    }
    entry = &report->entries[report->count++];
    entry->kind = field->kind;
    entry->reason = reason;
    entry->changed = changed;
    snprintf(entry->prior, sizeof(entry->prior), "%s", field->value ? field->value : "");
    snprintf(entry->fallback, sizeof(entry->fallback), "%s", field->fallback ? field->fallback : "");
    report->changed = report->changed || changed;
}

static void line_drawing_startup_apply_field(
    LineDrawingStartupRootFallbackReport* report,
    const LineDrawingStartupRootField* field) {
    LineDrawingStartupRootFallbackReason reason = LINE_DRAWING_STARTUP_ROOT_FALLBACK_UNCHANGED;
    bool changed = false;

    if (!field || !field->value || field->value_size == 0u || !field->fallback) {
        return;
    }

    if (field->value[0] == '\0') {
        reason = LINE_DRAWING_STARTUP_ROOT_FALLBACK_UNSET;
        changed = true;
    } else if (!line_drawing_startup_root_dir_exists(field->value)) {
        reason = LINE_DRAWING_STARTUP_ROOT_FALLBACK_MISSING;
        changed = true;
    }

    line_drawing_startup_record_entry(report, field, reason, changed);
    if (changed) {
        snprintf(field->value, field->value_size, "%s", field->fallback);
    }
}

bool LineDrawingStartupConfig_ApplyRootFallbacks(
    LineDrawingDataPaths* paths,
    LineDrawingStartupRootFallbackReport* out_report) {
    LineDrawingStartupRootFallbackReport local_report = {0};
    LineDrawingStartupRootField fields[LINE_DRAWING_STARTUP_ROOT_FALLBACK_CAP];

    if (!paths) return false;

    fields[0] = (LineDrawingStartupRootField){
        LINE_DRAWING_STARTUP_ROOT_INPUT,
        paths->input_root,
        sizeof(paths->input_root),
        LineDrawingDataPaths_DefaultInputRoot()
    };
    fields[1] = (LineDrawingStartupRootField){
        LINE_DRAWING_STARTUP_ROOT_OUTPUT,
        paths->output_root,
        sizeof(paths->output_root),
        LineDrawingDataPaths_DefaultOutputRoot()
    };
    fields[2] = (LineDrawingStartupRootField){
        LINE_DRAWING_STARTUP_ROOT_LAYOUT,
        paths->layout_root,
        sizeof(paths->layout_root),
        LineDrawingDataPaths_DefaultLayoutRoot()
    };
    fields[3] = (LineDrawingStartupRootField){
        LINE_DRAWING_STARTUP_ROOT_OBJECT_ASSET,
        paths->object_asset_root,
        sizeof(paths->object_asset_root),
        LineDrawingDataPaths_DefaultObjectAssetRoot()
    };

    for (size_t i = 0u; i < LINE_DRAWING_STARTUP_ROOT_FALLBACK_CAP; ++i) {
        line_drawing_startup_apply_field(&local_report, &fields[i]);
    }

    if (out_report) {
        *out_report = local_report;
    }
    return true;
}
