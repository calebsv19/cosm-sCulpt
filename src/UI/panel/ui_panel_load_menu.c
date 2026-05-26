#include "UI/ui_panel_internal.h"
#include "UI/ui_panel_file_layout.h"
#include "UI/ui_panel_file_summary.h"
#include "UI/ui_panel_scene_list.h"

#include "Core/line_drawing_file_catalog.h"
#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Layout/layout_json.h"
#include "UI/info_overlay.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

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

static const char* k_runtime_file_browser_mode_path = "data/runtime/file_browser_mode.txt";
static const char* k_runtime_json_root_path = "data/runtime/file_browser_json_root.txt";
static const char* k_runtime_scene_root_path = "data/runtime/file_browser_scene_root.txt";
static const char* k_runtime_last_json_entry_path = "data/runtime/file_browser_last_json_entry.txt";
static const char* k_runtime_last_scene_entry_path = "data/runtime/file_browser_last_scene_entry.txt";

static bool UIPanel_PathIsDirectory(const char* path);
static int UIPanel_FindLoadMenuIndexForPath(const UIPanelState* ui, const char* path);

static bool UIPanel_PathIsRegularFile(const char* path) {
    struct stat st = {0};
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static bool UIPanel_CopyParentDirectoryPath(const char* path,
                                            int levels,
                                            char* out_path,
                                            size_t out_path_size) {
    size_t length = 0u;
    if (!path || !path[0] || !out_path || out_path_size == 0u || levels <= 0) return false;
    length = strlen(path);
    if (length >= out_path_size) length = out_path_size - 1u;
    memcpy(out_path, path, length);
    out_path[length] = '\0';

    for (int i = 0; i < levels; ++i) {
        char* slash = strrchr(out_path, '/');
        if (!slash || slash == out_path) return false;
        *slash = '\0';
    }

    return out_path[0] != '\0' && UIPanel_PathIsDirectory(out_path);
}

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

static bool UIPanel_ClearRememberedEntryPath(UILoadMenuMode mode) {
    const char* state_path = UIPanel_FileBrowserEntryStatePath(mode);
    if (!state_path) return false;
    if (remove(state_path) == 0) return true;
    return errno == ENOENT;
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

static void UIPanel_AppendCatalogEntries(UIPanelState* ui,
                                         const LineDrawingFileCatalogEntry* entries,
                                         int count) {
    int i = 0;
    if (!ui || !entries || count <= 0) return;
    for (i = 0; i < count && ui->loadMenu.count < MAX_CONFIG_FILES; ++i) {
        (void)UIPanel_AddLoadEntry(ui, entries[i].label, entries[i].path);
    }
}

static void UIPanel_ScanJsonDirectory(UIPanelState* ui, const char* root_dir) {
    LineDrawingFileCatalogEntry entries[MAX_CONFIG_FILES];
    int count = 0;
    if (!ui || !root_dir || !root_dir[0]) return;
    memset(entries, 0, sizeof(entries));
    count = LineDrawingFileCatalog_ScanLayoutEntries(entries, MAX_CONFIG_FILES, root_dir);
    UIPanel_AppendCatalogEntries(ui, entries, count);
}

static void UIPanel_ScanSceneRoot(UIPanelState* ui, const char* root_dir) {
    LineDrawingFileCatalogEntry entries[MAX_CONFIG_FILES];
    int count = 0;
    if (!ui || !root_dir || !root_dir[0]) return;
    memset(entries, 0, sizeof(entries));
    count = LineDrawingFileCatalog_ScanSceneEntries(entries, MAX_CONFIG_FILES, root_dir);
    UIPanel_AppendCatalogEntries(ui, entries, count);
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
    return UIPanel_FindLoadMenuIndexForPath(ui, active_path);
}

static int UIPanel_FindLoadMenuIndexForPath(const UIPanelState* ui, const char* path) {
    if (!ui || !path || !path[0]) return -1;
    for (int i = 0; i < ui->loadMenu.count; ++i) {
        if (strcmp(ui->loadMenu.entryPaths[i], path) == 0) {
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

static const char* UIPanel_LoadMenuBaseName(const char* path) {
    const char* base = NULL;
    if (!path || !path[0]) return "(unset)";
    base = strrchr(path, '/');
    return base ? (base + 1) : path;
}

static const char* UIPanel_GetModeBrowseName(UILoadMenuMode mode) {
    switch (mode) {
        case UI_LOAD_MENU_MODE_JSON: return "JSON";
        case UI_LOAD_MENU_MODE_SCENE: return "Scene";
        case UI_LOAD_MENU_MODE_NONE:
        default: return "Browser";
    }
}

static const char* UIPanel_GetPreferredSessionPathForMode(const UIPanelState* ui) {
    const LineDrawingRecentContexts* recents = NULL;
    const char* active_path = NULL;
    if (!ui) return NULL;

    active_path = UIPanel_GetActiveSessionPathForMode(ui);
    if (UIPanel_PathIsRegularFile(active_path)) {
        return active_path;
    }

    recents = Global_GetRecentContexts();
    if (!recents) return NULL;
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        return (recents->layouts.count > 0 && recents->layouts.paths[0][0] != '\0')
                   ? recents->layouts.paths[0]
                   : NULL;
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        return (recents->scenes.count > 0 && recents->scenes.paths[0][0] != '\0')
                   ? recents->scenes.paths[0]
                   : NULL;
    }
    return NULL;
}

static bool UIPanel_DeriveBrowserRootFromActiveSession(const UIPanelState* ui,
                                                       char* out_path,
                                                       size_t out_path_size) {
    const char* active_path = NULL;
    int levels = 0;
    if (!ui || !out_path || out_path_size == 0u) return false;
    active_path = UIPanel_GetPreferredSessionPathForMode(ui);
    if (!active_path || !active_path[0]) return false;

    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        levels = 1;
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        levels = 2;
    } else {
        return false;
    }

    return UIPanel_CopyParentDirectoryPath(active_path, levels, out_path, out_path_size);
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

bool UIPanel_GetFileBrowserRowSelectionState(const UIPanelState* ui,
                                             int index,
                                             UILoadMenuSelectionState* out_state) {
    const char* active_path = NULL;
    char remembered_path[MAX_CONFIG_PATH];

    if (out_state) *out_state = UI_LOAD_MENU_SELECTION_NONE;
    if (!ui || index < 0 || index >= ui->loadMenu.count) return false;

    active_path = UIPanel_GetActiveSessionPathForMode(ui);
    if (active_path && active_path[0] &&
        strcmp(ui->loadMenu.entryPaths[index], active_path) == 0) {
        if (out_state) *out_state = UI_LOAD_MENU_SELECTION_ACTIVE_SESSION;
        return true;
    }

    if (UIPanel_LoadRememberedEntryPath(ui->loadMenu.mode,
                                        remembered_path,
                                        sizeof(remembered_path)) &&
        strcmp(ui->loadMenu.entryPaths[index], remembered_path) == 0) {
        if (out_state) *out_state = UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY;
        return true;
    }

    return false;
}

bool UIPanel_GetFileBrowserSelectionInfo(const UIPanelState* ui,
                                         UILoadMenuSelectionState* out_state,
                                         const char** out_path) {
    char remembered_path[MAX_CONFIG_PATH];
    int remembered_index = -1;

    if (out_state) *out_state = UI_LOAD_MENU_SELECTION_NONE;
    if (out_path) *out_path = NULL;
    if (!ui || ui->loadMenu.mode == UI_LOAD_MENU_MODE_NONE) return false;

    if (ui->loadMenu.activeIndex >= 0 &&
        UIPanel_GetFileBrowserRowSelectionState(ui, ui->loadMenu.activeIndex, out_state)) {
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

bool UIPanel_GetFileBrowserStatusText(const UIPanelState* ui,
                                      char* out_text,
                                      size_t out_text_size) {
    UILoadMenuSelectionState selection_state = UI_LOAD_MENU_SELECTION_NONE;
    const char* selection_path = NULL;
    const char* mode_name = NULL;

    if (!out_text || out_text_size == 0u) return false;
    out_text[0] = '\0';
    if (!ui) return false;

    mode_name = UIPanel_GetModeBrowseName(ui->loadMenu.mode);
    if (UIPanel_GetFileBrowserSelectionInfo(ui, &selection_state, &selection_path) &&
        selection_path && selection_path[0]) {
        return snprintf(out_text,
                        out_text_size,
                        "Browser  %s row %s",
                        selection_state == UI_LOAD_MENU_SELECTION_ACTIVE_SESSION
                            ? "Active"
                            : "Remembered",
                        UIPanel_LoadMenuBaseName(selection_path)) < (int)out_text_size;
    }

    if (ui->loadMenu.count > 0) {
        return snprintf(out_text,
                        out_text_size,
                        "Browser  %s mode has entries but no active row",
                        mode_name) < (int)out_text_size;
    }

    return snprintf(out_text,
                    out_text_size,
                    "Browser  %s mode has no entries",
                    mode_name) < (int)out_text_size;
}

bool UIPanel_GetFileBrowserActionHintText(const UIPanelState* ui,
                                          char* out_text,
                                          size_t out_text_size) {
    UILoadMenuSelectionState selection_state = UI_LOAD_MENU_SELECTION_NONE;
    const char* selection_path = NULL;
    const char* mode_name = NULL;

    if (!out_text || out_text_size == 0u) return false;
    out_text[0] = '\0';
    if (!ui) return false;

    mode_name = UIPanel_GetModeBrowseName(ui->loadMenu.mode);
    if (UIPanel_GetFileBrowserSelectionInfo(ui, &selection_state, &selection_path)) {
        if (selection_state == UI_LOAD_MENU_SELECTION_ACTIVE_SESSION) {
            return snprintf(out_text,
                            out_text_size,
                            "Actions  Use Session re-centers the live row if the browse root drifts. Clear Last is only for remembered fallback rows.") < (int)out_text_size;
        }
        if (selection_state == UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY) {
            return snprintf(out_text,
                            out_text_size,
                            "Actions  Use Session restores the live session row. Clear Last removes this remembered fallback row.") < (int)out_text_size;
        }
    }

    if (ui->loadMenu.count > 0) {
        return snprintf(out_text,
                        out_text_size,
                        "Actions  Use Session targets the live session row. Clear Last removes any stale remembered fallback for %s mode.",
                        mode_name) < (int)out_text_size;
    }

    return snprintf(out_text,
                    out_text_size,
                    "Actions  Double-click Load %s to pick the %s mode root.",
                    mode_name,
                    mode_name) < (int)out_text_size;
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

bool UIPanel_FocusFileBrowserOnActiveSession(void) {
    UIPanelState* ui = UIPanel_Get();
    const char* preferred_path = NULL;
    char root_path[MAX_CONFIG_PATH];
    if (!ui || ui->loadMenu.mode == UI_LOAD_MENU_MODE_NONE) return false;
    preferred_path = UIPanel_GetPreferredSessionPathForMode(ui);
    if (!preferred_path || !preferred_path[0]) return false;
    if (!UIPanel_DeriveBrowserRootFromActiveSession(ui, root_path, sizeof(root_path))) {
        return false;
    }

    if (!UIPanel_ApplyBrowserRootForMode(ui->loadMenu.mode, root_path, false)) {
        return false;
    }
    snprintf(ui->loadMenu.rootPath, sizeof(ui->loadMenu.rootPath), "%s", root_path);
    UIPanel_SetFileBrowserVisibleState(ui, true);
    UIPanel_RefreshConfigList();
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.activeIndex = UIPanel_FindActiveLoadMenuIndex(ui);
    if (ui->loadMenu.activeIndex < 0) {
        ui->loadMenu.activeIndex = UIPanel_FindLoadMenuIndexForPath(ui, preferred_path);
    }
    if (ui->loadMenu.activeIndex < 0) return false;
    UIPanel_LoadMenuScrollIndexIntoView(ui, ui->loadMenu.activeIndex);
    (void)UIPanel_SaveFileBrowserMode(ui);
    return true;
}

bool UIPanel_ClearRememberedFileBrowserEntry(void) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui || ui->loadMenu.mode == UI_LOAD_MENU_MODE_NONE) return false;
    if (!UIPanel_ClearRememberedEntryPath(ui->loadMenu.mode)) return false;
    UIPanel_RefreshConfigList();
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.activeIndex = UIPanel_FindActiveLoadMenuIndex(ui);
    if (ui->loadMenu.activeIndex >= 0) {
        UIPanel_LoadMenuScrollIndexIntoView(ui, ui->loadMenu.activeIndex);
    }
    return true;
}

void Render_UIPanelFileBrowser(const UIPanelState* ui, SDL_Renderer* renderer) {
    UIPanelVisualPalette palette = {0};
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
    const char* empty_label = NULL;
    SDL_Color active_fill = {58, 68, 84, 185};
    SDL_Color hover_fill = {42, 50, 62, 150};
    SDL_Color active_text_color = {240, 242, 248, 255};
    SDL_Color remembered_fill = {92, 82, 42, 176};
    SDL_Color remembered_border = {186, 156, 84, 210};
    SDL_Color remembered_accent = {222, 188, 96, 255};
    SDL_Color chip_fill = {36, 40, 48, 220};
    SDL_Color chip_border = {140, 170, 210, 235};
    char status_line[96];
    char helper_line[320];
    int font_h = 14;
    int chip_font_h = 0;
    UIPanelVisualMetrics metrics = UIPanelVisual_MakeMetrics(font);

    if (!ui || !renderer || !font) return;
    if (!UIPanel_IsLoadMenuOpen()) return;
    rect = UIPanel_GetLoadMenuRect(ui);
    if (rect.w <= 0 || rect.h <= 0) return;

    title = (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) ? "JSON Browser" :
            (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) ? "Scene Browser" :
            "Browser";
    if (!UIPanel_GetFileBrowserActionHintText(ui, helper_line, sizeof(helper_line))) {
        snprintf(helper_line,
                 sizeof(helper_line),
                 "Actions  Use Session targets the live row. Clear Last removes remembered fallback rows.");
    }
    empty_label = (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON)
                      ? "No JSON files found in the current input root."
                      : "No scene entries found in the current input root.";

    if (UIPanelVisual_ResolvePalette(&palette)) {
        label_color = palette.text_muted;
        value_color = palette.text_primary;
        accent_color = palette.accent;
        panel_fill = palette.workspace_fill;
        panel_fill.a = palette.workspace_fill.a;
        panel_border = palette.pane_border;
        active_fill = UIPanelVisual_BlendColor(palette.workspace_fill, palette.button_border, 102);
        active_fill.a = 188;
        hover_fill = UIPanelVisual_BlendColor(palette.workspace_fill, palette.button_border, 48);
        hover_fill.a = 150;
        remembered_fill = UIPanelVisual_BlendColor(palette.workspace_fill, palette.button_border, 128);
        remembered_fill.a = 176;
        remembered_border = UIPanelVisual_BlendColor(palette.button_border,
                                                     (SDL_Color){255, 210, 120, 255},
                                                     110);
        remembered_border.a = 210;
        remembered_accent = UIPanelVisual_BlendColor(palette.button_border,
                                                     (SDL_Color){255, 214, 132, 255},
                                                     136);
        remembered_accent.a = 255;
        chip_fill = UIPanelVisual_AdjustColor(palette.workspace_fill, 8, 8);
        chip_fill.a = 220;
        chip_border = palette.button_border;
        chip_border.a = 235;
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
    chip_font_h = font_h;
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
            UILoadMenuSelectionState row_state = UI_LOAD_MENU_SELECTION_NONE;
            SDL_Rect row_rect = {
                list_clip.x,
                y,
                list_clip.w,
                UI_LOAD_MENU_ROW_H - 2
            };
            SDL_Rect chip_rect = {0, 0, 0, 0};
            const char* chip_label = NULL;
            int text_max_width = row_rect.w - (metrics.pad_x * 2);
            (void)UIPanel_GetFileBrowserRowSelectionState(ui, i, &row_state);
            if (row_state == UI_LOAD_MENU_SELECTION_ACTIVE_SESSION) {
                chip_label = "LIVE";
            } else if (row_state == UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY) {
                chip_label = "LAST";
            }
            if (chip_label) {
                chip_rect.w = 40;
                chip_rect.h = metrics.chip_h - 2;
                chip_rect.x = row_rect.x + row_rect.w - metrics.pad_x - chip_rect.w;
                chip_rect.y = row_rect.y + ((row_rect.h - chip_rect.h) / 2);
                text_max_width = chip_rect.x - row_rect.x - (metrics.pad_x * 2) - 6;
                if (text_max_width < 24) text_max_width = 24;
            }
            {
                SDL_Color fill = active ? active_fill : hover_fill;
                SDL_Color border = active ? panel_border : UIPanelVisual_AdjustColor(panel_border, -10, -30);
                SDL_Color accent = active ? accent_color : UIPanelVisual_AdjustColor(accent_color, -8, 0);
                Uint8 inner_alpha = active ? 70 : 0;
                if (row_state == UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY) {
                    fill = remembered_fill;
                    border = remembered_border;
                    accent = remembered_accent;
                    inner_alpha = active ? 92 : 0;
                }
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
                                           text_max_width,
                                           font_h + 4,
                                           active ? active_text_color : value_color);
            if (chip_label && chip_rect.w > 0 && chip_rect.h > 0) {
                SDL_Color local_chip_border = (row_state == UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY)
                                                  ? remembered_border
                                                  : chip_border;
                UIPanelVisual_DrawLabelChip(renderer,
                                            chip_rect,
                                            chip_fill,
                                            local_chip_border,
                                            chip_fill.a,
                                            local_chip_border.a);
                UIPanelSummary_DrawTextClipped(renderer,
                                               font,
                                               chip_label,
                                               chip_rect.x + 6,
                                               chip_rect.y + ((chip_rect.h - chip_font_h) / 2),
                                               chip_rect.w - 12,
                                               chip_font_h + 2,
                                               row_state == UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY
                                                   ? remembered_accent
                                                   : accent_color);
            }
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
                                   helper_line,
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
