#include "UI/ui_panel_internal.h"
#include "UI/ui_panel_file_layout.h"
#include "UI/ui_panel_file_summary.h"
#include "UI/ui_panel_scene_list.h"

#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Layout/layout_json.h"
#include "UI/info_overlay.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

enum {
    UI_LOAD_MENU_PADDING_PX = 8,
    UI_LOAD_MENU_HEADER_H = 28,
    UI_LOAD_MENU_FOOTER_H = 20,
    UI_LOAD_MENU_ROW_H = 24,
    UI_LOAD_MENU_SCROLLBAR_W = 10,
    UI_LOAD_MENU_GUTTER_PX = 8
};

static const char* k_scene_authoring_filename = "scene_authoring.json";
static const char* k_scene_runtime_filename = "scene_runtime.json";
static const char* k_runtime_file_browser_mode_path = "data/runtime/file_browser_mode.txt";
static const char* k_runtime_json_root_path = "data/runtime/file_browser_json_root.txt";
static const char* k_runtime_scene_root_path = "data/runtime/file_browser_scene_root.txt";
static const char* k_runtime_last_json_entry_path = "data/runtime/file_browser_last_json_entry.txt";
static const char* k_runtime_last_scene_entry_path = "data/runtime/file_browser_last_scene_entry.txt";

static bool UIPanel_PathIsDirectory(const char* path);

static bool UIPanel_FileBrowserEnsureRuntimeDir(void) {
    if (mkdir("data", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    if (mkdir("data/runtime", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static const char* UIPanel_FileBrowserModeToken(UILoadMenuMode mode) {
    switch (mode) {
        case UI_LOAD_MENU_MODE_JSON: return "json";
        case UI_LOAD_MENU_MODE_SCENE: return "scene";
        case UI_LOAD_MENU_MODE_NONE:
        default: return "none";
    }
}

static UILoadMenuMode UIPanel_FileBrowserModeFromToken(const char* token) {
    if (!token || !token[0]) return UI_LOAD_MENU_MODE_NONE;
    if (strcasecmp(token, "json") == 0) return UI_LOAD_MENU_MODE_JSON;
    if (strcasecmp(token, "scene") == 0) return UI_LOAD_MENU_MODE_SCENE;
    return UI_LOAD_MENU_MODE_NONE;
}

static bool UIPanel_SaveFileBrowserMode(const UIPanelState* ui) {
    FILE* file = NULL;
    const char* token = NULL;
    if (!ui) return false;
    if (!UIPanel_FileBrowserEnsureRuntimeDir()) return false;
    token = UIPanel_FileBrowserModeToken(ui->loadMenu.mode);
    file = fopen(k_runtime_file_browser_mode_path, "wb");
    if (!file) return false;
    if (fputs(token, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static const char* UIPanel_FileBrowserEntryStatePath(UILoadMenuMode mode) {
    switch (mode) {
        case UI_LOAD_MENU_MODE_JSON: return k_runtime_last_json_entry_path;
        case UI_LOAD_MENU_MODE_SCENE: return k_runtime_last_scene_entry_path;
        case UI_LOAD_MENU_MODE_NONE:
        default: return NULL;
    }
}

static const char* UIPanel_FileBrowserRootStatePath(UILoadMenuMode mode) {
    switch (mode) {
        case UI_LOAD_MENU_MODE_JSON: return k_runtime_json_root_path;
        case UI_LOAD_MENU_MODE_SCENE: return k_runtime_scene_root_path;
        case UI_LOAD_MENU_MODE_NONE:
        default: return NULL;
    }
}

static bool UIPanel_SaveRememberedEntryPath(UILoadMenuMode mode, const char* path) {
    FILE* file = NULL;
    const char* state_path = UIPanel_FileBrowserEntryStatePath(mode);
    if (!state_path || !path || !path[0]) return false;
    if (!UIPanel_FileBrowserEnsureRuntimeDir()) return false;
    file = fopen(state_path, "wb");
    if (!file) return false;
    if (fputs(path, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static bool UIPanel_LoadRememberedEntryPath(UILoadMenuMode mode, char* out_path, size_t out_path_size) {
    FILE* file = NULL;
    const char* state_path = UIPanel_FileBrowserEntryStatePath(mode);
    if (!state_path || !out_path || out_path_size == 0u) return false;
    out_path[0] = '\0';
    file = fopen(state_path, "rb");
    if (!file) return false;
    if (!fgets(out_path, (int)out_path_size, file)) {
        fclose(file);
        out_path[0] = '\0';
        return false;
    }
    fclose(file);
    out_path[strcspn(out_path, "\r\n")] = '\0';
    return out_path[0] != '\0';
}

static bool UIPanel_SaveBrowserRootPath(UILoadMenuMode mode, const char* path) {
    FILE* file = NULL;
    const char* state_path = UIPanel_FileBrowserRootStatePath(mode);
    if (!state_path || !path || !path[0]) return false;
    if (!UIPanel_FileBrowserEnsureRuntimeDir()) return false;
    file = fopen(state_path, "wb");
    if (!file) return false;
    if (fputs(path, file) == EOF || fputc('\n', file) == EOF) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static bool UIPanel_LoadBrowserRootPath(UILoadMenuMode mode, char* out_path, size_t out_path_size) {
    FILE* file = NULL;
    const char* state_path = UIPanel_FileBrowserRootStatePath(mode);
    if (!state_path || !out_path || out_path_size == 0u) return false;
    out_path[0] = '\0';
    file = fopen(state_path, "rb");
    if (!file) return false;
    if (!fgets(out_path, (int)out_path_size, file)) {
        fclose(file);
        out_path[0] = '\0';
        return false;
    }
    fclose(file);
    out_path[strcspn(out_path, "\r\n")] = '\0';
    return out_path[0] != '\0';
}

static bool UIPanel_ResolveBrowserRootForMode(UILoadMenuMode mode,
                                              char* out_path,
                                              size_t out_path_size) {
    const char* current_root = Global_GetInputRoot();
    if (!out_path || out_path_size == 0u) return false;
    out_path[0] = '\0';
    if (UIPanel_LoadBrowserRootPath(mode, out_path, out_path_size) &&
        UIPanel_PathIsDirectory(out_path)) {
        return true;
    }
    out_path[0] = '\0';
    if (current_root && current_root[0] &&
        snprintf(out_path, out_path_size, "%s", current_root) < (int)out_path_size) {
        return UIPanel_PathIsDirectory(out_path);
    }
    return false;
}

static bool UIPanel_ApplyBrowserRootForMode(UILoadMenuMode mode,
                                            const char* path,
                                            bool persist_global_root) {
    (void)persist_global_root;
    if (!path || !path[0] || !UIPanel_PathIsDirectory(path)) return false;
    return UIPanel_SaveBrowserRootPath(mode, path);
}

void UIPanel_RememberLoadedEntry(UILoadMenuMode mode, const char* path) {
    (void)UIPanel_SaveRememberedEntryPath(mode, path);
}

static void UIPanel_SetFileBrowserVisibleState(UIPanelState* ui, bool visible) {
    if (!ui) return;
    ui->loadMenu.visible = visible;
    ui->loadMenu.open = visible;
}

void UIPanel_LoadFileBrowserMode(UIPanelState* ui) {
    FILE* file = NULL;
    char line[32];
    if (!ui) return;
    file = fopen(k_runtime_file_browser_mode_path, "rb");
    if (!file) return;
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return;
    }
    fclose(file);
    line[strcspn(line, "\r\n")] = '\0';
    ui->loadMenu.mode = UIPanel_FileBrowserModeFromToken(line);
    UIPanel_SetFileBrowserVisibleState(ui, ui->loadMenu.mode != UI_LOAD_MENU_MODE_NONE);
}

static bool UIPanel_AddLoadEntry(UIPanelState* ui, const char* label, const char* full_path) {
    if (!ui || !label || !label[0] || !full_path || !full_path[0]) return false;
    if (ui->loadMenu.count >= MAX_CONFIG_FILES) return false;

    for (int i = 0; i < ui->loadMenu.count; ++i) {
        if (strcasecmp(ui->loadMenu.entryPaths[i], full_path) == 0) {
            return false;
        }
    }

    snprintf(ui->loadMenu.entries[ui->loadMenu.count],
             sizeof(ui->loadMenu.entries[0]),
             "%s",
             label);
    snprintf(ui->loadMenu.entryPaths[ui->loadMenu.count],
             sizeof(ui->loadMenu.entryPaths[0]),
             "%s",
             full_path);
    ui->loadMenu.count++;
    return true;
}

static bool UIPanel_PathIsDirectory(const char* path) {
    struct stat st = {0};
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static bool UIPanel_PathIsRegularFile(const char* path) {
    struct stat st = {0};
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static const char* UIPanel_PathBaseName(const char* path) {
    const char* slash = NULL;
    if (!path || !path[0]) return "";
    slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static bool UIPanel_ComposeSceneContractPaths(const char* scene_dir,
                                              char* out_authoring_path,
                                              size_t out_authoring_path_size,
                                              char* out_runtime_path,
                                              size_t out_runtime_path_size) {
    if (!scene_dir || !scene_dir[0]) return false;
    if (!out_authoring_path || out_authoring_path_size == 0u) return false;
    if (!out_runtime_path || out_runtime_path_size == 0u) return false;

    if (snprintf(out_authoring_path,
                 out_authoring_path_size,
                 "%s/%s",
                 scene_dir,
                 k_scene_authoring_filename) >= (int)out_authoring_path_size) {
        return false;
    }
    if (snprintf(out_runtime_path,
                 out_runtime_path_size,
                 "%s/%s",
                 scene_dir,
                 k_scene_runtime_filename) >= (int)out_runtime_path_size) {
        return false;
    }
    return true;
}

static bool UIPanel_DirectoryHasSceneContract(const char* scene_dir,
                                              char* out_authoring_path,
                                              size_t out_authoring_path_size) {
    char authoring_path[MAX_CONFIG_PATH];
    char runtime_path[MAX_CONFIG_PATH];
    if (!UIPanel_ComposeSceneContractPaths(scene_dir,
                                           authoring_path,
                                           sizeof(authoring_path),
                                           runtime_path,
                                           sizeof(runtime_path))) {
        return false;
    }
    if (!UIPanel_PathIsRegularFile(authoring_path) ||
        !UIPanel_PathIsRegularFile(runtime_path)) {
        return false;
    }
    if (out_authoring_path && out_authoring_path_size > 0u) {
        snprintf(out_authoring_path, out_authoring_path_size, "%s", authoring_path);
    }
    return true;
}

static void UIPanel_BuildSceneLabel(const char* group_name,
                                    const char* scene_dir,
                                    char* out_label,
                                    size_t out_label_size) {
    const char* scene_name = UIPanel_PathBaseName(scene_dir);
    if (!out_label || out_label_size == 0u) return;
    out_label[0] = '\0';

    if (group_name && group_name[0]) {
        snprintf(out_label, out_label_size, "%s/%s", group_name, scene_name);
        return;
    }
    snprintf(out_label, out_label_size, "%s", scene_name);
}

static void UIPanel_SwapLoadEntries(UIPanelState* ui, int a, int b) {
    char name_tmp[128];
    char path_tmp[MAX_CONFIG_PATH];
    if (!ui || a < 0 || b < 0 || a >= ui->loadMenu.count || b >= ui->loadMenu.count) return;
    snprintf(name_tmp, sizeof(name_tmp), "%s", ui->loadMenu.entries[a]);
    snprintf(path_tmp, sizeof(path_tmp), "%s", ui->loadMenu.entryPaths[a]);
    snprintf(ui->loadMenu.entries[a], sizeof(ui->loadMenu.entries[a]), "%s", ui->loadMenu.entries[b]);
    snprintf(ui->loadMenu.entryPaths[a], sizeof(ui->loadMenu.entryPaths[a]), "%s", ui->loadMenu.entryPaths[b]);
    snprintf(ui->loadMenu.entries[b], sizeof(ui->loadMenu.entries[b]), "%s", name_tmp);
    snprintf(ui->loadMenu.entryPaths[b], sizeof(ui->loadMenu.entryPaths[b]), "%s", path_tmp);
}

static float UIPanel_LoadMenuContentHeight(const UIPanelState* ui) {
    if (!ui) return 0.0f;
    return (float)ui->loadMenu.count * (float)UI_LOAD_MENU_ROW_H;
}

static SDL_Rect UIPanel_GetLoadMenuListClipRect(const UIPanelState* ui) {
    SDL_Rect rect = UIPanel_GetLoadMenuRect(ui);
    SDL_Rect clip = {
        rect.x + UI_LOAD_MENU_PADDING_PX,
        rect.y + UI_LOAD_MENU_HEADER_H,
        rect.w - (UI_LOAD_MENU_PADDING_PX * 2) - UI_LOAD_MENU_SCROLLBAR_W - UI_LOAD_MENU_GUTTER_PX,
        rect.h - UI_LOAD_MENU_HEADER_H - UI_LOAD_MENU_FOOTER_H
    };
    if (clip.w < 0) clip.w = 0;
    if (clip.h < 0) clip.h = 0;
    return clip;
}

static SDL_Rect UIPanel_GetLoadMenuScrollTrackRect(const UIPanelState* ui) {
    SDL_Rect rect = UIPanel_GetLoadMenuRect(ui);
    SDL_Rect clip = UIPanel_GetLoadMenuListClipRect(ui);
    SDL_Rect track = {
        rect.x + rect.w - UI_LOAD_MENU_PADDING_PX - UI_LOAD_MENU_SCROLLBAR_W,
        clip.y,
        UI_LOAD_MENU_SCROLLBAR_W,
        clip.h
    };
    if (track.h < 0) track.h = 0;
    return track;
}

static float UIPanel_LoadMenuMaxScrollOffset(const UIPanelState* ui) {
    SDL_Rect clip = UIPanel_GetLoadMenuListClipRect(ui);
    float max_offset = UIPanel_LoadMenuContentHeight(ui) - (float)clip.h;
    return max_offset > 0.0f ? max_offset : 0.0f;
}

static bool UIPanel_LoadMenuHasScrollableContent(const UIPanelState* ui) {
    return UIPanel_LoadMenuMaxScrollOffset(ui) > 0.5f;
}

static void UIPanel_LoadMenuClampScroll(UIPanelState* ui) {
    float max_offset = UIPanel_LoadMenuMaxScrollOffset(ui);
    if (!ui) return;
    if (ui->loadMenu.scrollOffsetPx < 0.0f) ui->loadMenu.scrollOffsetPx = 0.0f;
    if (ui->loadMenu.scrollOffsetPx > max_offset) ui->loadMenu.scrollOffsetPx = max_offset;
}

static void UIPanel_LoadMenuScrollIndexIntoView(UIPanelState* ui, int index) {
    SDL_Rect clip = {0, 0, 0, 0};
    float row_top = 0.0f;
    float row_bottom = 0.0f;
    if (!ui || index < 0 || index >= ui->loadMenu.count) return;
    clip = UIPanel_GetLoadMenuListClipRect(ui);
    if (clip.h <= 0) return;
    row_top = (float)index * (float)UI_LOAD_MENU_ROW_H;
    row_bottom = row_top + (float)UI_LOAD_MENU_ROW_H;
    if (row_top < ui->loadMenu.scrollOffsetPx) {
        ui->loadMenu.scrollOffsetPx = row_top;
    } else if (row_bottom > ui->loadMenu.scrollOffsetPx + (float)clip.h) {
        ui->loadMenu.scrollOffsetPx = row_bottom - (float)clip.h;
    }
    UIPanel_LoadMenuClampScroll(ui);
}

static SDL_Rect UIPanel_GetLoadMenuScrollThumbRect(const UIPanelState* ui) {
    SDL_Rect track = UIPanel_GetLoadMenuScrollTrackRect(ui);
    SDL_Rect thumb = track;
    float content_h = UIPanel_LoadMenuContentHeight(ui);
    float max_offset = UIPanel_LoadMenuMaxScrollOffset(ui);
    float ratio = 1.0f;
    float thumb_h = 0.0f;
    float travel = 0.0f;
    float offset_ratio = 0.0f;

    if (track.h <= 0 || content_h <= 0.0f) {
        thumb.h = 0;
        return thumb;
    }

    ratio = (float)track.h / content_h;
    if (ratio > 1.0f) ratio = 1.0f;
    thumb_h = ratio * (float)track.h;
    if (thumb_h < 12.0f) thumb_h = 12.0f;
    if (thumb_h > (float)track.h) thumb_h = (float)track.h;
    thumb.h = (int)lroundf(thumb_h);

    travel = (float)track.h - thumb_h;
    if (travel <= 0.0f || max_offset <= 0.0f) {
        return thumb;
    }

    offset_ratio = ui->loadMenu.scrollOffsetPx / max_offset;
    if (offset_ratio < 0.0f) offset_ratio = 0.0f;
    if (offset_ratio > 1.0f) offset_ratio = 1.0f;
    thumb.y = track.y + (int)lroundf(offset_ratio * travel);
    return thumb;
}

static int UIPanel_LoadMenuIndexAtPoint(const UIPanelState* ui, int mouseX, int mouseY) {
    SDL_Rect clip = UIPanel_GetLoadMenuListClipRect(ui);
    int content_y = 0;
    if (!ui) return -1;
    if (!SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &clip)) return -1;
    content_y = (mouseY - clip.y) + (int)floorf(ui->loadMenu.scrollOffsetPx);
    if (content_y < 0) return -1;
    {
        const int index = content_y / UI_LOAD_MENU_ROW_H;
        if (index < 0 || index >= ui->loadMenu.count) return -1;
        return index;
    }
}

static void UIPanel_ScanJsonDirectory(UIPanelState* ui, const char* root_dir) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;

    if (!ui || !root_dir || !root_dir[0]) return;
    dir = opendir(root_dir);
    if (!dir) return;

    while ((entry = readdir(dir)) != NULL && ui->loadMenu.count < MAX_CONFIG_FILES) {
        char full_path[MAX_CONFIG_PATH];
        const char* name = entry->d_name;
        size_t len = 0u;

        if (name[0] == '.') continue;
        len = strlen(name);
        if (len < 5u) continue;
        if (strcasecmp(name + len - 5u, ".json") != 0) continue;
        if (snprintf(full_path, sizeof(full_path), "%s/%s", root_dir, name) >= (int)sizeof(full_path)) {
            continue;
        }
        if (!UIPanel_PathIsRegularFile(full_path)) continue;
        (void)UIPanel_AddLoadEntry(ui, name, full_path);
    }

    closedir(dir);
}

static void UIPanel_TryAppendSceneDir(UIPanelState* ui, const char* scene_dir, const char* group_name) {
    char authoring_path[MAX_CONFIG_PATH];
    char label[128];
    if (!ui || !scene_dir || !scene_dir[0]) return;
    if (!UIPanel_DirectoryHasSceneContract(scene_dir, authoring_path, sizeof(authoring_path))) {
        return;
    }
    UIPanel_BuildSceneLabel(group_name, scene_dir, label, sizeof(label));
    (void)UIPanel_AddLoadEntry(ui, label, authoring_path);
}

static void UIPanel_ScanSceneRoot(UIPanelState* ui, const char* root_dir) {
    DIR* root = NULL;
    struct dirent* entry = NULL;
    if (!ui || !root_dir || !root_dir[0]) return;

    UIPanel_TryAppendSceneDir(ui, root_dir, NULL);

    root = opendir(root_dir);
    if (!root) return;

    while ((entry = readdir(root)) != NULL && ui->loadMenu.count < MAX_CONFIG_FILES) {
        char candidate_path[MAX_CONFIG_PATH];
        DIR* group_dir = NULL;
        struct dirent* grouped_entry = NULL;
        if (entry->d_name[0] == '.') continue;
        if (snprintf(candidate_path,
                     sizeof(candidate_path),
                     "%s/%s",
                     root_dir,
                     entry->d_name) >= (int)sizeof(candidate_path)) {
            continue;
        }
        if (!UIPanel_PathIsDirectory(candidate_path)) continue;
        if (UIPanel_DirectoryHasSceneContract(candidate_path, NULL, 0u)) {
            UIPanel_TryAppendSceneDir(ui, candidate_path, NULL);
            continue;
        }

        group_dir = opendir(candidate_path);
        if (!group_dir) continue;
        while ((grouped_entry = readdir(group_dir)) != NULL && ui->loadMenu.count < MAX_CONFIG_FILES) {
            char grouped_scene_path[MAX_CONFIG_PATH];
            if (grouped_entry->d_name[0] == '.') continue;
            if (snprintf(grouped_scene_path,
                         sizeof(grouped_scene_path),
                         "%s/%s",
                         candidate_path,
                         grouped_entry->d_name) >= (int)sizeof(grouped_scene_path)) {
                continue;
            }
            if (!UIPanel_PathIsDirectory(grouped_scene_path)) continue;
            UIPanel_TryAppendSceneDir(ui, grouped_scene_path, entry->d_name);
        }
        closedir(group_dir);
    }

    closedir(root);
}

static int UIPanel_FindActiveLoadMenuIndex(const UIPanelState* ui) {
    const char* active_path = NULL;
    if (!ui) return -1;
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        active_path = Global_GetCurrentConfigPath();
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        active_path = Global_GetCurrentSceneAuthoringPath();
    }
    if (!active_path || !active_path[0]) return -1;

    for (int i = 0; i < ui->loadMenu.count; ++i) {
        if (strcmp(ui->loadMenu.entryPaths[i], active_path) == 0) {
            return i;
        }
    }
    return -1;
}

static const char* UIPanel_GetActiveSessionPathForMode(const UIPanelState* ui) {
    if (!ui) return NULL;
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        return Global_GetCurrentConfigPath();
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        return Global_GetCurrentSceneAuthoringPath();
    }
    return NULL;
}

static int UIPanel_FindRememberedLoadMenuIndex(const UIPanelState* ui) {
    char remembered_path[MAX_CONFIG_PATH];
    if (!ui) return -1;
    if (!UIPanel_LoadRememberedEntryPath(ui->loadMenu.mode,
                                         remembered_path,
                                         sizeof(remembered_path))) {
        return -1;
    }
    for (int i = 0; i < ui->loadMenu.count; ++i) {
        if (strcmp(ui->loadMenu.entryPaths[i], remembered_path) == 0) {
            return i;
        }
    }
    return -1;
}

bool UIPanel_GetFileBrowserSelectionInfo(const UIPanelState* ui,
                                         UILoadMenuSelectionState* out_state,
                                         const char** out_path) {
    const char* active_path = NULL;
    char remembered_path[MAX_CONFIG_PATH];
    int remembered_index = -1;

    if (out_state) *out_state = UI_LOAD_MENU_SELECTION_NONE;
    if (out_path) *out_path = NULL;
    if (!ui || ui->loadMenu.mode == UI_LOAD_MENU_MODE_NONE) return false;

    active_path = UIPanel_GetActiveSessionPathForMode(ui);
    if (active_path && active_path[0] && ui->loadMenu.activeIndex >= 0 &&
        ui->loadMenu.activeIndex < ui->loadMenu.count &&
        strcmp(ui->loadMenu.entryPaths[ui->loadMenu.activeIndex], active_path) == 0) {
        if (out_state) *out_state = UI_LOAD_MENU_SELECTION_ACTIVE_SESSION;
        if (out_path) *out_path = ui->loadMenu.entryPaths[ui->loadMenu.activeIndex];
        return true;
    }

    remembered_index = UIPanel_FindRememberedLoadMenuIndex(ui);
    if (remembered_index >= 0 &&
        UIPanel_LoadRememberedEntryPath(ui->loadMenu.mode,
                                        remembered_path,
                                        sizeof(remembered_path))) {
        if (out_state) *out_state = UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY;
        if (out_path) *out_path = ui->loadMenu.entryPaths[remembered_index];
        return true;
    }

    return false;
}

static void UIPanel_LoadMenuClose(UIPanelState* ui) {
    if (!ui) return;
    UIPanel_SetFileBrowserVisibleState(ui, false);
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.scrollbarDragging = false;
    ui->loadMenu.mode = UI_LOAD_MENU_MODE_NONE;
    (void)UIPanel_SaveFileBrowserMode(ui);
}

static int UIPanel_CountEntriesForRoot(const char* root_path, UILoadMenuMode mode) {
    UIPanelState probe;
    if (!root_path || !root_path[0]) return 0;
    memset(&probe, 0, sizeof(probe));
    if (mode == UI_LOAD_MENU_MODE_JSON) {
        UIPanel_ScanJsonDirectory(&probe, root_path);
    } else if (mode == UI_LOAD_MENU_MODE_SCENE) {
        UIPanel_ScanSceneRoot(&probe, root_path);
    }
    return probe.loadMenu.count;
}

static bool UIPanel_ShowFileBrowserForCurrentRoot(UILoadMenuMode mode,
                                                  int anchor_button_id) {
    UIPanelState* ui = UIPanel_Get();
    char resolved_root[MAX_CONFIG_PATH];
    if (!ui) return false;

    ui->loadMenu.mode = mode;
    ui->loadMenu.anchorButtonId = anchor_button_id;
    if (UIPanel_ResolveBrowserRootForMode(mode, resolved_root, sizeof(resolved_root))) {
        snprintf(ui->loadMenu.rootPath, sizeof(ui->loadMenu.rootPath), "%s", resolved_root);
    } else if (Global_GetInputRoot() && Global_GetInputRoot()[0]) {
        snprintf(ui->loadMenu.rootPath, sizeof(ui->loadMenu.rootPath), "%s", Global_GetInputRoot());
        (void)UIPanel_SaveBrowserRootPath(mode, ui->loadMenu.rootPath);
    } else {
        ui->loadMenu.rootPath[0] = '\0';
    }
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_FILE;
    UIPanel_OnWindowResized(Global_GetScreenWidth(), Global_GetScreenHeight());
    UIPanel_RefreshConfigList();
    UIPanel_SetFileBrowserVisibleState(ui, true);
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.scrollOffsetPx = 0.0f;
    ui->loadMenu.scrollbarDragging = false;
    UIPanel_LoadMenuClampScroll(ui);
    UIPanel_CloseSaveDialog(ui);
    UIPanel_CloseRootDialog(ui);
    UIPanel_ClosePrismDimensionDialog(ui);
    UIPanel_CloseSceneBoundsDialog(ui);
    UIPanel_CloseConstructionPlaneDialog(ui);
    UIPanel_CloseObjectTransformDialog(ui);
    (void)UIPanel_SaveFileBrowserMode(ui);
    return true;
}

static bool UIPanel_RestorePersistedEntryForMode(UILoadMenuMode mode) {
    const char* path = NULL;
    if (mode == UI_LOAD_MENU_MODE_JSON) {
        path = Global_GetLastLayoutPath();
        if (!path || !path[0]) return false;
        return UIPanel_LoadLayoutFromPath(path);
    }
    if (mode == UI_LOAD_MENU_MODE_SCENE) {
        path = Global_GetLastSceneAuthoringPath();
        if (!path || !path[0]) return false;
        return UIPanel_LoadSceneFromPath(path);
    }
    return false;
}

bool UIPanel_LoadJsonFromFolderSelection(const char* selected_folder, bool persist_root) {
    if (!selected_folder || !selected_folder[0]) return false;
    if (UIPanel_CountEntriesForRoot(selected_folder, UI_LOAD_MENU_MODE_JSON) <= 0) {
        UIPanelState* ui = UIPanel_Get();
        if (ui) {
            UIPanel_SetFileBrowserVisibleState(ui, false);
        }
        return false;
    }
    if (!UIPanel_ApplyBrowserRootForMode(UI_LOAD_MENU_MODE_JSON, selected_folder, persist_root)) {
        return false;
    }
    if (!Global_SetInputRoot(selected_folder, persist_root)) {
        return false;
    }
    return UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_JSON,
                                                 UI_BTN_LOAD_JSON);
}

bool UIPanel_LoadSceneFromFolderSelection(const char* selected_folder, bool persist_root) {
    if (!selected_folder || !selected_folder[0]) return false;
    if (UIPanel_CountEntriesForRoot(selected_folder, UI_LOAD_MENU_MODE_SCENE) <= 0) {
        UIPanelState* ui = UIPanel_Get();
        if (ui) {
            UIPanel_SetFileBrowserVisibleState(ui, false);
        }
        return false;
    }
    if (!UIPanel_ApplyBrowserRootForMode(UI_LOAD_MENU_MODE_SCENE, selected_folder, persist_root)) {
        return false;
    }
    if (!Global_SetInputRoot(selected_folder, persist_root)) {
        return false;
    }
    return UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_SCENE,
                                                 UI_BTN_LOAD_SCENE);
}

void UIPanel_RefreshConfigList(void) {
    UIPanelState* ui = UIPanel_Get();
    const char* input_root = NULL;
    if (!ui) return;

    ui->loadMenu.count = 0;
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.activeIndex = -1;
    memset(ui->loadMenu.entries, 0, sizeof(ui->loadMenu.entries));
    memset(ui->loadMenu.entryPaths, 0, sizeof(ui->loadMenu.entryPaths));
    input_root = ui->loadMenu.rootPath[0] ? ui->loadMenu.rootPath : Global_GetInputRoot();
    if (!ui->loadMenu.rootPath[0] && input_root && input_root[0]) {
        snprintf(ui->loadMenu.rootPath, sizeof(ui->loadMenu.rootPath), "%s", input_root);
    }

    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        UIPanel_ScanJsonDirectory(ui, input_root);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        UIPanel_ScanSceneRoot(ui, input_root);
    }

    for (int i = 0; i < ui->loadMenu.count - 1; ++i) {
        for (int j = i + 1; j < ui->loadMenu.count; ++j) {
            if (strcasecmp(ui->loadMenu.entries[j], ui->loadMenu.entries[i]) < 0) {
                UIPanel_SwapLoadEntries(ui, i, j);
            }
        }
    }

    ui->loadMenu.activeIndex = UIPanel_FindActiveLoadMenuIndex(ui);
    if (ui->loadMenu.activeIndex < 0) {
        ui->loadMenu.activeIndex = UIPanel_FindRememberedLoadMenuIndex(ui);
    }
    if (ui->loadMenu.activeIndex >= 0) {
        UIPanel_LoadMenuScrollIndexIntoView(ui, ui->loadMenu.activeIndex);
    }
    if (!ui->loadMenu.visible && ui->loadMenu.mode != UI_LOAD_MENU_MODE_NONE) {
        UIPanel_SetFileBrowserVisibleState(ui, true);
    }
    UIPanel_LoadMenuClampScroll(ui);
}

void UIPanel_ToggleLoadMenu(void) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui) return;
    if (ui->loadMenu.visible) {
        UIPanel_LoadMenuClose(ui);
        return;
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_NONE) {
        return;
    }
    UIPanel_RefreshConfigList();
    if (ui->loadMenu.count <= 0) {
        UIPanel_SetFileBrowserVisibleState(ui, true);
        return;
    }
    UIPanel_SetFileBrowserVisibleState(ui, true);
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.scrollOffsetPx = 0.0f;
    UIPanel_LoadMenuClampScroll(ui);
    (void)UIPanel_SaveFileBrowserMode(ui);
}

bool UIPanel_IsLoadMenuOpen(void) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui) return false;
    return ui->activeLeftTab == UI_PANEL_LEFT_TAB_FILE &&
           ui->loadMenu.visible &&
           ui->loadMenu.mode != UI_LOAD_MENU_MODE_NONE;
}

void UIPanel_ActivateJsonBrowser(void) {
    (void)UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_JSON, UI_BTN_LOAD_JSON);
}

void UIPanel_ActivateSceneBrowser(void) {
    (void)UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_SCENE, UI_BTN_LOAD_SCENE);
}

bool UIPanel_RestorePersistedFileSession(void) {
    UIPanelState* ui = UIPanel_Get();
    bool restored = false;
    if (!ui || ui->loadMenu.mode == UI_LOAD_MENU_MODE_NONE) return false;

    restored = UIPanel_RestorePersistedEntryForMode(ui->loadMenu.mode);
    UIPanel_RefreshConfigList();
    UIPanel_SetFileBrowserVisibleState(ui, true);
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.activeIndex = UIPanel_FindActiveLoadMenuIndex(ui);
    if (ui->loadMenu.activeIndex < 0) {
        ui->loadMenu.activeIndex = UIPanel_FindRememberedLoadMenuIndex(ui);
    }
    ui->loadMenu.scrollOffsetPx = 0.0f;
    UIPanel_LoadMenuClampScroll(ui);
    return restored;
}

void Render_UIPanelFileBrowser(const UIPanelState* ui, SDL_Renderer* renderer) {
    LineDrawing3dThemePalette palette = {0};
    SDL_Color label_color = {200, 200, 210, 255};
    SDL_Color value_color = {230, 230, 235, 255};
    SDL_Color accent_color = {140, 170, 210, 255};
    SDL_Color panel_fill = {20, 20, 24, 170};
    SDL_Color panel_border = {90, 100, 115, 200};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    SDL_Rect rect = {0};
    SDL_Rect list_clip = {0};
    SDL_Rect track = {0};
    SDL_Rect thumb = {0};
    const char* title = NULL;
    const char* helper = NULL;
    const char* empty_label = NULL;
    SDL_Color active_fill = {58, 68, 84, 185};
    SDL_Color hover_fill = {42, 50, 62, 150};
    SDL_Color active_text_color = {240, 242, 248, 255};
    char status_line[96];
    int font_h = 14;
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);

    if (!ui || !renderer || !font) return;
    if (!UIPanel_IsLoadMenuOpen()) return;
    rect = UIPanel_GetLoadMenuRect(ui);
    if (rect.w <= 0 || rect.h <= 0) return;

    title = (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) ? "JSON Browser" :
            (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) ? "Scene Browser" :
            "Browser";
    helper = (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON)
                 ? "Single-click keeps the JSON browser open. Double-click Load JSON to pick its root. Session Paths edits only the live session root."
                 : "Single-click keeps the scene browser open. Double-click Load Scene to pick its root. Session Paths edits only the live session root.";
    empty_label = (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON)
                      ? "No JSON files found in the current input root."
                      : "No scene entries found in the current input root.";

    if (line_drawing3d_shared_theme_resolve_palette(&palette)) {
        label_color = palette.text_muted;
        value_color = palette.text_primary;
        accent_color = palette.button_border;
        panel_fill = palette.panel_fill;
        panel_fill.a = 165;
        panel_border = palette.panel_border;
        active_fill = UIPanelVisual_BlendColor(palette.panel_fill, palette.button_border, 102);
        active_fill.a = 188;
        hover_fill = UIPanelVisual_BlendColor(palette.panel_fill, palette.button_border, 48);
        hover_fill.a = 150;
        if ((((int)active_fill.r * 299) + ((int)active_fill.g * 587) + ((int)active_fill.b * 114)) / 1000 > 180) {
            active_fill.r = (Uint8)(((int)active_fill.r > 48) ? ((int)active_fill.r - 48) : 0);
            active_fill.g = (Uint8)(((int)active_fill.g > 48) ? ((int)active_fill.g - 48) : 0);
            active_fill.b = (Uint8)(((int)active_fill.b > 48) ? ((int)active_fill.b - 48) : 0);
            active_text_color = (SDL_Color){28, 32, 40, 255};
        } else {
            active_text_color = palette.text_primary;
        }
    }

    font_h = TTF_FontHeight(font);
    if (font_h < 12) font_h = 12;
    snprintf(status_line,
             sizeof(status_line),
             "%d %s",
             ui->loadMenu.count,
             (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON)
                 ? ((ui->loadMenu.count == 1) ? "JSON entry" : "JSON entries")
                 : ((ui->loadMenu.count == 1) ? "scene entry" : "scene entries"));
    list_clip = UIPanel_GetLoadMenuListClipRect(ui);
    track = UIPanel_GetLoadMenuScrollTrackRect(ui);
    thumb = UIPanel_GetLoadMenuScrollThumbRect(ui);

#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    UIPanelSummary_DrawCard(renderer,
                            rect,
                            panel_fill,
                            panel_border,
                            accent_color,
                            4);

    UIPanelSummary_DrawText(renderer, font, title, rect.x + metrics.pad_x, rect.y + metrics.pad_y + 1, label_color);
    UIPanelSummary_DrawTextClipped(renderer,
                                   font,
                                   status_line,
                                   rect.x + metrics.pad_x,
                                   rect.y + metrics.pad_y + font_h + 3,
                                   rect.w - (metrics.pad_x * 2),
                                   font_h + 4,
                                   value_color);
    UIPanelSummary_DrawDivider(renderer,
                               rect,
                               rect.y + UI_LOAD_MENU_HEADER_H - 2,
                               metrics.pad_x,
                               accent_color,
                               90);

    if (list_clip.w > 0 && list_clip.h > 0) {
        SDL_RenderSetClipRect(renderer, &list_clip);
    }
    if (ui->loadMenu.count <= 0) {
        UIPanelSummary_DrawTextClipped(renderer,
                                       font,
                                       empty_label,
                                       list_clip.x,
                                       list_clip.y + metrics.row_text_y,
                                       list_clip.w,
                                       font_h + 4,
                                       label_color);
    } else {
        const int first_index = (int)(ui->loadMenu.scrollOffsetPx / (float)UI_LOAD_MENU_ROW_H);
        const int offset_in_row = (int)ui->loadMenu.scrollOffsetPx % UI_LOAD_MENU_ROW_H;
        int y = list_clip.y - offset_in_row;
        for (int i = first_index; i < ui->loadMenu.count && y < list_clip.y + list_clip.h; ++i) {
            const bool hovered = (i == ui->loadMenu.hoverIndex);
            const bool active = (i == ui->loadMenu.activeIndex);
            SDL_Rect row_rect = {
                list_clip.x,
                y,
                list_clip.w,
                UI_LOAD_MENU_ROW_H - 2
            };
            {
                SDL_Color fill = active ? active_fill : hover_fill;
                SDL_Color border = active ? panel_border : UIPanelVisual_AdjustColor(panel_border, -10, -30);
                SDL_Color accent = active ? accent_color : UIPanelVisual_AdjustColor(accent_color, -8, 0);
                Uint8 inner_alpha = active ? 70 : 0;
                if (hovered || active) {
                    UIPanelVisual_DrawInteractiveRow(renderer,
                                                     row_rect,
                                                     fill,
                                                     border,
                                                     accent,
                                                     hovered,
                                                     active,
                                                     3,
                                                     inner_alpha);
                }
            }
            UIPanelSummary_DrawTextClipped(renderer,
                                           font,
                                           ui->loadMenu.entries[i],
                                           row_rect.x + metrics.pad_x,
                                           row_rect.y + metrics.row_text_y,
                                           row_rect.w - (metrics.pad_x * 2),
                                           font_h + 4,
                                           active ? active_text_color : value_color);
            y += UI_LOAD_MENU_ROW_H;
        }
    }
    SDL_RenderSetClipRect(renderer, NULL);

    if (UIPanel_LoadMenuHasScrollableContent(ui) &&
        track.w > 0 && track.h > 0 && thumb.h > 0) {
        SDL_Color thumb_fill = accent_color;
        SDL_Color thumb_border = panel_border;
        thumb_fill.a = 220;
        thumb_border.a = 210;
        UIPanelVisual_DrawScrollbar(renderer,
                                    track,
                                    thumb,
                                    UIPanelVisual_AdjustColor(panel_border, -12, -120),
                                    panel_border,
                                    thumb_fill,
                                    thumb_border,
                                    ui->loadMenu.scrollbarDragging);
    }

    UIPanelSummary_DrawTextClipped(renderer,
                                   font,
                                   helper,
                                   rect.x + metrics.pad_x,
                                   rect.y + rect.h - UI_LOAD_MENU_FOOTER_H + 4,
                                   rect.w - (metrics.pad_x * 2),
                                   font_h + 4,
                                   label_color);
}

SDL_Rect UIPanel_GetLoadMenuRect(const UIPanelState* ui) {
    SDL_Rect rect = {0, 0, 0, 0};
    if (ui && UIPanel_GetFilePaneRects(ui, NULL, NULL, NULL, &rect)) {
        return rect;
    }
    return rect;
}

SDL_Rect UIPanel_GetLoadMenuPaneClipRect(const UIPanelState* ui) {
    return UIPanel_GetLoadMenuRect(ui);
}

bool UIPanel_LoadLayoutFromPath(const char* path) {
    GlobalState* state = Global_Get();
    if (!state || !path || path[0] == '\0') return false;

    Editor_ClearHistory(&state->editor);

    if (Layout_LoadFromFile(&state->layout, path)) {
        SDL_Log("[UI] Loaded layout %s", path);
        Global_OnLayoutLoaded(path);
        UIPanel_RememberLoadedEntry(UI_LOAD_MENU_MODE_JSON, path);
        UIPanel_RefreshConfigList();
        state->editor.selectedAnchorIndex = -1;
        state->editor.selectedWallIndex = -1;
        state->editor.selectedObject3DId = 0u;
        state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        state->editor.hoveredAnchorIndex = -1;
        state->editor.hoveredWallIndex = -1;
        state->editor.hoveredObject3DId = 0u;
        state->editor.hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        state->editor.hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        state->editor.hoveredHandleAnchor = -1;
        state->editor.hoveredHandleComponent = -1;
        state->editor.hoveredGizmoAxis = -1;
        state->editor.hoveredObject3DGizmoAxis = -1;
        state->editor.activeObject3DGizmoAxis = -1;
        Editor_HistoryCapture(&state->editor, &state->layout);
        return true;
    }

    SDL_Log("[UI] Failed to load layout %s", path);
    return false;
}

static bool UIPanel_LoadEntryByIndex(int index) {
    UIPanelState* ui = UIPanel_Get();
    bool loaded = false;
    if (!ui || index < 0 || index >= ui->loadMenu.count) return false;
    if (ui->loadMenu.rootPath[0] != '\0') {
        (void)Global_SetInputRoot(ui->loadMenu.rootPath, true);
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        loaded = UIPanel_LoadLayoutFromPath(ui->loadMenu.entryPaths[index]);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        loaded = UIPanel_LoadSceneFromPath(ui->loadMenu.entryPaths[index]);
    }
    return loaded;
}

bool UIPanel_HandleLoadMenuWheel(int mouseX, int mouseY, float wheel_delta) {
    UIPanelState* ui = UIPanel_Get();
    SDL_Rect rect = {0, 0, 0, 0};
    if (!ui || !UIPanel_IsLoadMenuOpen()) return false;
    rect = UIPanel_GetLoadMenuRect(ui);
    if (!SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &rect)) return false;
    if (!UIPanel_LoadMenuHasScrollableContent(ui)) return true;
    ui->loadMenu.scrollOffsetPx -= wheel_delta * ((float)UI_LOAD_MENU_ROW_H * 3.0f);
    UIPanel_LoadMenuClampScroll(ui);
    return true;
}

bool UIPanel_HandleLoadMenuClick(int mouseX, int mouseY) {
    UIPanelState* ui = UIPanel_Get();
    SDL_Rect rect = {0, 0, 0, 0};
    SDL_Rect track = {0, 0, 0, 0};
    SDL_Rect thumb = {0, 0, 0, 0};
    int index = -1;
    if (!ui || !UIPanel_IsLoadMenuOpen()) return false;

    rect = UIPanel_GetLoadMenuRect(ui);
    if (!SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &rect)) {
        return false;
    }

    if (ui->loadMenu.count <= 0) {
        return true;
    }

    if (UIPanel_LoadMenuHasScrollableContent(ui)) {
        track = UIPanel_GetLoadMenuScrollTrackRect(ui);
        thumb = UIPanel_GetLoadMenuScrollThumbRect(ui);
        if (SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &track)) {
            float max_offset = UIPanel_LoadMenuMaxScrollOffset(ui);
            float ratio = 0.0f;
            if (!SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &thumb)) {
                int usable_h = track.h - thumb.h;
                int rel_y = mouseY - track.y - (thumb.h / 2);
                if (usable_h > 0) {
                    ratio = (float)rel_y / (float)usable_h;
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;
                    ui->loadMenu.scrollOffsetPx = ratio * max_offset;
                    UIPanel_LoadMenuClampScroll(ui);
                }
            }
            return true;
        }
    }

    index = UIPanel_LoadMenuIndexAtPoint(ui, mouseX, mouseY);
    if (index >= 0) {
        if (UIPanel_LoadEntryByIndex(index)) {
            ui->loadMenu.activeIndex = UIPanel_FindActiveLoadMenuIndex(ui);
        }
        return true;
    }

    return true;
}

void UIPanel_HandleMouseMotion(int mouseX, int mouseY) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    PrimitivePlacementPreviewKind preview = PRIMITIVE_PLACEMENT_PREVIEW_NONE;
    if (ui) {
        for (int i = 0; i < ui->count; ++i) {
            UIButton* btn = &ui->buttons[i];
            const SDL_Rect r = btn->bounds;
            btn->hovered = mouseX >= r.x && mouseX <= r.x + r.w &&
                           mouseY >= r.y && mouseY <= r.y + r.h;
            if (btn->hovered && state && state->spaceMode == SPACE_MODE_3D) {
                if (btn->id == UI_BTN_CREATE_PLANE) {
                    preview = PRIMITIVE_PLACEMENT_PREVIEW_PLANE;
                } else if (btn->id == UI_BTN_CREATE_RECT_PRISM) {
                    preview = PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM;
                }
            }
        }
    }
    if (state && state->editor.primitivePlacementPreview != preview) {
        state->editor.primitivePlacementPreview = preview;
        Global_FlagGridChanged();
    }

    UIPanel_HandleSceneListMouseMotion(mouseX, mouseY);

    if (!ui || !UIPanel_IsLoadMenuOpen()) {
        if (ui) ui->loadMenu.hoverIndex = -1;
        return;
    }

    ui->loadMenu.hoverIndex = UIPanel_LoadMenuIndexAtPoint(ui, mouseX, mouseY);
}
