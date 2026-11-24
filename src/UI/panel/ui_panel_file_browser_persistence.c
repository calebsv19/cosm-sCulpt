#include "UI/panel/ui_panel_file_browser_internal.h"
#include "UI/ui_panel_internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

typedef struct BrowserStateFiles {
    UILoadMenuMode mode;
    const char* token;
    const char* root_path;
    const char* entry_path;
} BrowserStateFiles;

static const char k_mode_path[] = "data/runtime/file_browser_mode.txt";
static const BrowserStateFiles k_browser_files[] = {
    {UI_LOAD_MENU_MODE_JSON, "json",
     "data/runtime/file_browser_json_root.txt",
     "data/runtime/file_browser_last_json_entry.txt"},
    {UI_LOAD_MENU_MODE_SCENE, "scene",
     "data/runtime/file_browser_scene_root.txt",
     "data/runtime/file_browser_last_scene_entry.txt"},
    {UI_LOAD_MENU_MODE_OBJECT, "object",
     "data/runtime/file_browser_object_root.txt",
     "data/runtime/file_browser_last_object_entry.txt"},
    {UI_LOAD_MENU_MODE_RUNTIME_MESH, "runtime_mesh",
     "data/runtime/file_browser_mesh_root.txt",
     "data/runtime/file_browser_last_mesh_entry.txt"},
    {UI_LOAD_MENU_MODE_STL_IMPORT, "stl_import",
     "data/runtime/file_browser_stl_root.txt",
     "data/runtime/file_browser_last_stl_entry.txt"},
};

static const BrowserStateFiles* browser_files_for_mode(UILoadMenuMode mode) {
    for (size_t i = 0; i < sizeof(k_browser_files) / sizeof(k_browser_files[0]); ++i) {
        if (k_browser_files[i].mode == mode) return &k_browser_files[i];
    }
    return NULL;
}

static UILoadMenuMode browser_mode_from_token(const char* token) {
    for (size_t i = 0; i < sizeof(k_browser_files) / sizeof(k_browser_files[0]); ++i) {
        if (strcasecmp(token, k_browser_files[i].token) == 0) {
            return k_browser_files[i].mode;
        }
    }
    if (strcasecmp(token, "mesh") == 0) return UI_LOAD_MENU_MODE_RUNTIME_MESH;
    if (strcasecmp(token, "stl") == 0) return UI_LOAD_MENU_MODE_STL_IMPORT;
    return UI_LOAD_MENU_MODE_NONE;
}

static bool ensure_runtime_dir(void) {
    if (mkdir("data", 0755) != 0 && errno != EEXIST) return false;
    if (mkdir("data/runtime", 0755) != 0 && errno != EEXIST) return false;
    return true;
}

static bool write_state_line(const char* state_path, const char* value) {
    if (!value || !value[0] || !ensure_runtime_dir()) return false;
    FILE* file = fopen(state_path, "wb");
    if (!file) return false;
    bool written = fputs(value, file) != EOF && fputc('\n', file) != EOF;
    fclose(file);
    return written;
}

/* A blank line is a mode token, while an empty or missing file leaves the mode alone. */
static bool read_state_line(const char* state_path, char* out, size_t out_size) {
    if (!out || out_size == 0u) return false;
    out[0] = '\0';
    FILE* file = fopen(state_path, "rb");
    if (!file) return false;
    bool read = fgets(out, (int)out_size, file) != NULL;
    fclose(file);
    if (!read) {
        out[0] = '\0';
        return false;
    }
    out[strcspn(out, "\r\n")] = '\0';
    return true;
}

bool UIPanel_ReadFileBrowserMode(UILoadMenuMode* out_mode) {
    char line[32];
    if (!out_mode || !read_state_line(k_mode_path, line, sizeof(line))) return false;
    *out_mode = browser_mode_from_token(line);
    return true;
}

bool UIPanel_SaveFileBrowserMode(UILoadMenuMode mode) {
    const BrowserStateFiles* files = browser_files_for_mode(mode);
    return write_state_line(k_mode_path, files ? files->token : "none");
}

bool UIPanel_LoadBrowserRootPath(UILoadMenuMode mode, char* out_path, size_t out_path_size) {
    const BrowserStateFiles* files = browser_files_for_mode(mode);
    return files && read_state_line(files->root_path, out_path, out_path_size) && out_path[0];
}

bool UIPanel_SaveBrowserRootPath(UILoadMenuMode mode, const char* path) {
    const BrowserStateFiles* files = browser_files_for_mode(mode);
    return files && write_state_line(files->root_path, path);
}

bool UIPanel_LoadRememberedEntryPath(UILoadMenuMode mode, char* out_path, size_t out_path_size) {
    const BrowserStateFiles* files = browser_files_for_mode(mode);
    return files && read_state_line(files->entry_path, out_path, out_path_size) && out_path[0];
}

void UIPanel_RememberLoadedEntry(UILoadMenuMode mode, const char* path) {
    const BrowserStateFiles* files = browser_files_for_mode(mode);
    if (files) (void)write_state_line(files->entry_path, path);
}

bool UIPanel_ClearRememberedEntryPath(UILoadMenuMode mode) {
    const BrowserStateFiles* files = browser_files_for_mode(mode);
    if (!files) return false;
    if (remove(files->entry_path) == 0) return true;
    return errno == ENOENT;
}
