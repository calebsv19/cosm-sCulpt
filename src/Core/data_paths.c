#include "Core/data_paths.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char* k_input_root_default = "config";
static const char* k_output_root_default = "export";
static const char* k_layout_root_default = "config";
static const char* k_runtime_input_root_path = "data/runtime/input_root.txt";
static const char* k_runtime_output_root_path = "data/runtime/output_root.txt";
static const char* k_runtime_layout_root_path = "data/runtime/layout_root.txt";

const char* LineDrawingDataPaths_DefaultInputRoot(void) {
    return k_input_root_default;
}

const char* LineDrawingDataPaths_DefaultOutputRoot(void) {
    return k_output_root_default;
}

const char* LineDrawingDataPaths_DefaultLayoutRoot(void) {
    return k_layout_root_default;
}

static bool ensure_runtime_dir(void) {
    if (mkdir("data", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    if (mkdir("data/runtime", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static void trim_ascii_whitespace(char* text) {
    size_t start = 0;
    size_t end = 0;
    size_t len = 0;
    if (!text) return;

    len = strlen(text);
    while (start < len && isspace((unsigned char)text[start])) {
        ++start;
    }
    end = len;
    while (end > start && isspace((unsigned char)text[end - 1])) {
        --end;
    }
    if (start > 0 || end < len) {
        const size_t out_len = end - start;
        memmove(text, text + start, out_len);
        text[out_len] = '\0';
    }
}

static void copy_non_empty_or_default(char* dst, size_t dst_size, const char* src, const char* fallback) {
    const char* chosen = fallback;
    if (src && src[0]) chosen = src;
    if (!dst || dst_size == 0) return;
    snprintf(dst, dst_size, "%s", chosen ? chosen : "");
}

static bool read_path_file(const char* file_path, char* out, size_t out_size) {
    FILE* fp = NULL;
    size_t count = 0;
    if (!file_path || !out || out_size == 0) return false;
    fp = fopen(file_path, "rb");
    if (!fp) return false;
    count = fread(out, 1, out_size - 1, fp);
    out[count] = '\0';
    fclose(fp);
    trim_ascii_whitespace(out);
    return out[0] != '\0';
}

static bool write_path_file(const char* file_path, const char* value) {
    FILE* fp = NULL;
    const size_t len = value ? strlen(value) : 0;
    if (!file_path || !value || value[0] == '\0') return false;
    fp = fopen(file_path, "wb");
    if (!fp) return false;
    if (fwrite(value, 1, len, fp) != len || fwrite("\n", 1, 1, fp) != 1) {
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;
}

void LineDrawingDataPaths_SetDefaults(LineDrawingDataPaths* paths) {
    if (!paths) return;
    copy_non_empty_or_default(paths->input_root, sizeof(paths->input_root), NULL, k_input_root_default);
    copy_non_empty_or_default(paths->output_root, sizeof(paths->output_root), NULL, k_output_root_default);
    copy_non_empty_or_default(paths->layout_root, sizeof(paths->layout_root), NULL, k_layout_root_default);
}

bool LineDrawingDataPaths_Load(LineDrawingDataPaths* paths) {
    char buffer[LINE_DRAWING_PATH_CAP];
    if (!paths) return false;

    LineDrawingDataPaths_SetDefaults(paths);

    if (read_path_file(k_runtime_input_root_path, buffer, sizeof(buffer))) {
        copy_non_empty_or_default(paths->input_root, sizeof(paths->input_root), buffer, k_input_root_default);
    }
    if (read_path_file(k_runtime_output_root_path, buffer, sizeof(buffer))) {
        copy_non_empty_or_default(paths->output_root, sizeof(paths->output_root), buffer, k_output_root_default);
    }
    if (read_path_file(k_runtime_layout_root_path, buffer, sizeof(buffer))) {
        copy_non_empty_or_default(paths->layout_root, sizeof(paths->layout_root), buffer, k_layout_root_default);
    }

    return true;
}

bool LineDrawingDataPaths_Save(const LineDrawingDataPaths* paths) {
    if (!paths) return false;
    if (!ensure_runtime_dir()) return false;
    if (!write_path_file(k_runtime_input_root_path, paths->input_root)) return false;
    if (!write_path_file(k_runtime_output_root_path, paths->output_root)) return false;
    if (!write_path_file(k_runtime_layout_root_path, paths->layout_root)) return false;
    return true;
}

bool LineDrawingDataPaths_BuildPath(char* out_path,
                                    size_t out_path_size,
                                    const char* root,
                                    const char* leaf_name) {
    const size_t root_len = root ? strlen(root) : 0;
    const size_t leaf_len = leaf_name ? strlen(leaf_name) : 0;
    const bool needs_slash = root_len > 0 && root[root_len - 1] != '/';
    if (!out_path || out_path_size == 0 || !root || !leaf_name || root_len == 0 || leaf_len == 0) {
        return false;
    }
    if (snprintf(out_path, out_path_size, "%s%s%s", root, needs_slash ? "/" : "", leaf_name) >=
        (int)out_path_size) {
        return false;
    }
    return true;
}
