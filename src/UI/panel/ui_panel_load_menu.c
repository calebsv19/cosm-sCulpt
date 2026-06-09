#include "UI/ui_panel_internal.h"
#include "UI/ui_panel_file_layout.h"
#include "UI/panel/ui_panel_file_browser_internal.h"
#include "UI/ui_panel_file_summary.h"
#include "UI/ui_panel_object_workspace_summary.h"
#include "UI/ui_panel_scene_list.h"
#include "UI/ui_panel_shell.h"

#include "Core/line_drawing_file_catalog.h"
#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Layout/asset/layout_object_asset_mesh_authoring.h"
#include "Layout/layout_json.h"
#include "UI/info_overlay.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static const char* k_runtime_file_browser_mode_path = "data/runtime/file_browser_mode.txt";
static const char* k_runtime_json_root_path = "data/runtime/file_browser_json_root.txt";
static const char* k_runtime_scene_root_path = "data/runtime/file_browser_scene_root.txt";
static const char* k_runtime_object_root_path = "data/runtime/file_browser_object_root.txt";
static const char* k_runtime_mesh_root_path = "data/runtime/file_browser_mesh_root.txt";
static const char* k_runtime_stl_root_path = "data/runtime/file_browser_stl_root.txt";
static const char* k_runtime_last_json_entry_path = "data/runtime/file_browser_last_json_entry.txt";
static const char* k_runtime_last_scene_entry_path = "data/runtime/file_browser_last_scene_entry.txt";
static const char* k_runtime_last_object_entry_path = "data/runtime/file_browser_last_object_entry.txt";
static const char* k_runtime_last_mesh_entry_path = "data/runtime/file_browser_last_mesh_entry.txt";
static const char* k_runtime_last_stl_entry_path = "data/runtime/file_browser_last_stl_entry.txt";

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
        case UI_LOAD_MENU_MODE_OBJECT: return "object";
        case UI_LOAD_MENU_MODE_RUNTIME_MESH: return "runtime_mesh";
        case UI_LOAD_MENU_MODE_STL_IMPORT: return "stl_import";
        case UI_LOAD_MENU_MODE_NONE:
        default: return "none";
    }
}

static UILoadMenuMode UIPanel_FileBrowserModeFromToken(const char* token) {
    if (!token || !token[0]) return UI_LOAD_MENU_MODE_NONE;
    if (strcasecmp(token, "json") == 0) return UI_LOAD_MENU_MODE_JSON;
    if (strcasecmp(token, "scene") == 0) return UI_LOAD_MENU_MODE_SCENE;
    if (strcasecmp(token, "object") == 0) return UI_LOAD_MENU_MODE_OBJECT;
    if (strcasecmp(token, "runtime_mesh") == 0 || strcasecmp(token, "mesh") == 0) {
        return UI_LOAD_MENU_MODE_RUNTIME_MESH;
    }
    if (strcasecmp(token, "stl_import") == 0 || strcasecmp(token, "stl") == 0) {
        return UI_LOAD_MENU_MODE_STL_IMPORT;
    }
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
        case UI_LOAD_MENU_MODE_OBJECT: return k_runtime_last_object_entry_path;
        case UI_LOAD_MENU_MODE_RUNTIME_MESH: return k_runtime_last_mesh_entry_path;
        case UI_LOAD_MENU_MODE_STL_IMPORT: return k_runtime_last_stl_entry_path;
        case UI_LOAD_MENU_MODE_NONE:
        default: return NULL;
    }
}

static const char* UIPanel_FileBrowserRootStatePath(UILoadMenuMode mode) {
    switch (mode) {
        case UI_LOAD_MENU_MODE_JSON: return k_runtime_json_root_path;
        case UI_LOAD_MENU_MODE_SCENE: return k_runtime_scene_root_path;
        case UI_LOAD_MENU_MODE_OBJECT: return k_runtime_object_root_path;
        case UI_LOAD_MENU_MODE_RUNTIME_MESH: return k_runtime_mesh_root_path;
        case UI_LOAD_MENU_MODE_STL_IMPORT: return k_runtime_stl_root_path;
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

bool UIPanel_LoadRememberedEntryPath(UILoadMenuMode mode, char* out_path, size_t out_path_size) {
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
    const char* current_root = (mode == UI_LOAD_MENU_MODE_OBJECT ||
                                mode == UI_LOAD_MENU_MODE_RUNTIME_MESH ||
                                mode == UI_LOAD_MENU_MODE_STL_IMPORT)
                                   ? Global_GetObjectAssetRoot()
                                   : Global_GetInputRoot();
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

static void UIPanel_ScanObjectAssetRoot(UIPanelState* ui, const char* root_dir) {
    LineDrawingFileCatalogEntry entries[MAX_CONFIG_FILES];
    int count = 0;
    if (!ui || !root_dir || !root_dir[0]) return;
    memset(entries, 0, sizeof(entries));
    count = LineDrawingFileCatalog_ScanLayoutEntries(entries, MAX_CONFIG_FILES, root_dir);
    UIPanel_AppendCatalogEntries(ui, entries, count);
}

static void UIPanel_ScanRuntimeMeshRoot(UIPanelState* ui, const char* root_dir) {
    LineDrawingFileCatalogEntry entries[MAX_CONFIG_FILES];
    int count = 0;
    if (!ui || !root_dir || !root_dir[0]) return;
    memset(entries, 0, sizeof(entries));
    count = LineDrawingFileCatalog_ScanRuntimeMeshEntries(entries, MAX_CONFIG_FILES, root_dir);
    UIPanel_AppendCatalogEntries(ui, entries, count);
}

static void UIPanel_ScanStlRoot(UIPanelState* ui, const char* root_dir) {
    LineDrawingFileCatalogEntry entries[MAX_CONFIG_FILES];
    int count = 0;
    if (!ui || !root_dir || !root_dir[0]) return;
    memset(entries, 0, sizeof(entries));
    count = LineDrawingFileCatalog_ScanStlEntries(entries, MAX_CONFIG_FILES, root_dir);
    UIPanel_AppendCatalogEntries(ui, entries, count);
}

static int UIPanel_FindActiveLoadMenuIndex(const UIPanelState* ui) {
    const char* active_path = NULL;
    if (!ui) return -1;
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        active_path = Global_GetCurrentConfigPath();
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        active_path = Global_GetCurrentSceneAuthoringPath();
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) {
        active_path = Global_GetCurrentObjectAssetPath();
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        GlobalState* state = Global_Get();
        active_path = state ? state->lastObjectRuntimeMeshPath : NULL;
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        active_path = NULL;
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

const char* UIPanel_GetActiveSessionPathForMode(const UIPanelState* ui) {
    if (!ui) return NULL;
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        return Global_GetCurrentConfigPath();
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        return Global_GetCurrentSceneAuthoringPath();
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) {
        return Global_GetCurrentObjectAssetPath();
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        GlobalState* state = Global_Get();
        return state ? state->lastObjectRuntimeMeshPath : NULL;
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        return NULL;
    }
    return NULL;
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
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) {
        return (recents->object_assets.count > 0 && recents->object_assets.paths[0][0] != '\0')
                   ? recents->object_assets.paths[0]
                   : NULL;
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        GlobalState* state = Global_Get();
        return state && state->lastObjectRuntimeMeshPath[0]
                   ? state->lastObjectRuntimeMeshPath
                   : NULL;
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        char remembered_path[MAX_CONFIG_PATH];
        if (UIPanel_LoadRememberedEntryPath(UI_LOAD_MENU_MODE_STL_IMPORT,
                                            remembered_path,
                                            sizeof(remembered_path))) {
            return NULL;
        }
        return NULL;
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
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) {
        levels = 1;
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        levels = 1;
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        levels = 1;
    } else {
        return false;
    }

    return UIPanel_CopyParentDirectoryPath(active_path, levels, out_path, out_path_size);
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
    } else if (mode == UI_LOAD_MENU_MODE_OBJECT) {
        UIPanel_ScanObjectAssetRoot(&probe, root_path);
    } else if (mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        UIPanel_ScanRuntimeMeshRoot(&probe, root_path);
    } else if (mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        UIPanel_ScanStlRoot(&probe, root_path);
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
    } else if ((mode == UI_LOAD_MENU_MODE_OBJECT ||
                mode == UI_LOAD_MENU_MODE_RUNTIME_MESH ||
                mode == UI_LOAD_MENU_MODE_STL_IMPORT) &&
               Global_GetObjectAssetRoot() &&
               Global_GetObjectAssetRoot()[0]) {
        snprintf(ui->loadMenu.rootPath,
                 sizeof(ui->loadMenu.rootPath),
                 "%s",
                 Global_GetObjectAssetRoot());
        (void)UIPanel_SaveBrowserRootPath(mode, ui->loadMenu.rootPath);
    } else if (Global_GetInputRoot() && Global_GetInputRoot()[0]) {
        snprintf(ui->loadMenu.rootPath, sizeof(ui->loadMenu.rootPath), "%s", Global_GetInputRoot());
        (void)UIPanel_SaveBrowserRootPath(mode, ui->loadMenu.rootPath);
    } else {
        ui->loadMenu.rootPath[0] = '\0';
    }
    UIPanel_SetActiveLeftTab(ui, UI_PANEL_LEFT_TAB_FILE);
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
    if (mode == UI_LOAD_MENU_MODE_OBJECT) {
        path = Global_GetLastObjectAssetPath();
        if (!path || !path[0]) return false;
        return UIPanel_LoadObjectAssetFromPath(path);
    }
    if (mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        GlobalState* state = Global_Get();
        path = state ? state->lastObjectRuntimeMeshPath : NULL;
        if (!path || !path[0]) return false;
        return UIPanel_PlaceRuntimeMeshAsSceneInstance(path);
    }
    if (mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        char remembered_path[MAX_CONFIG_PATH];
        if (!UIPanel_LoadRememberedEntryPath(mode, remembered_path, sizeof(remembered_path))) {
            return false;
        }
        return UIPanel_ImportStlAndPlaceFromPath(remembered_path);
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

bool UIPanel_LoadObjectAssetFromFolderSelection(const char* selected_folder, bool persist_root) {
    if (!selected_folder || !selected_folder[0]) return false;
    if (UIPanel_CountEntriesForRoot(selected_folder, UI_LOAD_MENU_MODE_OBJECT) <= 0) {
        UIPanelState* ui = UIPanel_Get();
        if (ui) {
            UIPanel_SetFileBrowserVisibleState(ui, false);
        }
        return false;
    }
    if (!UIPanel_ApplyBrowserRootForMode(UI_LOAD_MENU_MODE_OBJECT, selected_folder, persist_root)) {
        return false;
    }
    if (!Global_SetObjectAssetRoot(selected_folder, persist_root)) {
        return false;
    }
    return UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_OBJECT,
                                                 UI_BTN_LOAD_JSON);
}

bool UIPanel_LoadStlFromFolderSelection(const char* selected_folder, bool persist_root) {
    if (!selected_folder || !selected_folder[0]) return false;
    if (UIPanel_CountEntriesForRoot(selected_folder, UI_LOAD_MENU_MODE_STL_IMPORT) <= 0) {
        UIPanelState* ui = UIPanel_Get();
        if (ui) {
            UIPanel_SetFileBrowserVisibleState(ui, false);
        }
        return false;
    }
    if (!UIPanel_ApplyBrowserRootForMode(UI_LOAD_MENU_MODE_STL_IMPORT,
                                         selected_folder,
                                         persist_root)) {
        return false;
    }
    if (!Global_SetObjectAssetRoot(selected_folder, persist_root)) {
        return false;
    }
    return UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_STL_IMPORT,
                                                 UI_BTN_LOAD_STL);
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
    input_root = ui->loadMenu.rootPath[0]
                     ? ui->loadMenu.rootPath
                     : ((ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT ||
                         ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH ||
                         ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT)
                            ? Global_GetObjectAssetRoot()
                            : Global_GetInputRoot());
    if (!ui->loadMenu.rootPath[0] && input_root && input_root[0]) {
        snprintf(ui->loadMenu.rootPath, sizeof(ui->loadMenu.rootPath), "%s", input_root);
    }

    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        UIPanel_ScanJsonDirectory(ui, input_root);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        UIPanel_ScanSceneRoot(ui, input_root);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) {
        UIPanel_ScanObjectAssetRoot(ui, input_root);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        UIPanel_ScanRuntimeMeshRoot(ui, input_root);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        UIPanel_ScanStlRoot(ui, input_root);
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

void UIPanel_ActivateObjectAssetBrowser(void) {
    (void)UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_OBJECT, UI_BTN_LOAD_JSON);
}

void UIPanel_ActivateRuntimeMeshBrowser(void) {
    (void)UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_RUNTIME_MESH,
                                                UI_BTN_LOAD_MESH_ASSET);
}

void UIPanel_ActivateStlImportBrowser(void) {
    (void)UIPanel_ShowFileBrowserForCurrentRoot(UI_LOAD_MENU_MODE_STL_IMPORT,
                                                UI_BTN_LOAD_STL);
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

bool UIPanel_LoadObjectAssetFromPath(const char* path) {
    GlobalState* state = Global_Get();
    ObjectAuthoringDocument loaded_authoring;
    bool has_authoring = false;
    char diagnostics[256];
    if (!state || !path || path[0] == '\0') return false;

    Editor_ClearHistory(&state->editor);
    ObjectAuthoringDocument_Init(&loaded_authoring);

    if (LayoutObjectAssetMeshAuthoring_LoadWithAuthoring(&state->layout,
                                                         &loaded_authoring,
                                                         &has_authoring,
                                                         path,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        SDL_Log("[UI] Loaded object asset %s", path);
        if (has_authoring) {
            ObjectAuthoringSession_Clear(&state->objectAuthoring);
            state->objectAuthoring.attached = true;
            state->objectAuthoring.sourceSceneObjectId = 0u;
            (void)ObjectAuthoringDocument_Copy(&state->objectAuthoring.document,
                                               &loaded_authoring);
        } else {
            (void)ObjectAuthoringSession_ResetFromLayout(&state->objectAuthoring,
                                                         &state->layout,
                                                         0u);
        }
        Global_OnObjectAssetLoaded(path);
        UIPanel_RememberLoadedEntry(UI_LOAD_MENU_MODE_OBJECT, path);
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
        ObjectAuthoringDocument_Free(&loaded_authoring);
        return true;
    }

    ObjectAuthoringDocument_Free(&loaded_authoring);
    SDL_Log("[UI] Failed to load object asset %s%s%s%s",
            path,
            diagnostics[0] ? " (" : "",
            diagnostics[0] ? diagnostics : "",
            diagnostics[0] ? ")" : "");
    return false;
}

static bool UIPanel_LoadEntryByIndex(int index) {
    UIPanelState* ui = UIPanel_Get();
    bool loaded = false;
    if (!ui || index < 0 || index >= ui->loadMenu.count) return false;
    if (ui->loadMenu.rootPath[0] != '\0' &&
        (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT ||
         ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH ||
         ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT)) {
        (void)Global_SetObjectAssetRoot(ui->loadMenu.rootPath, true);
    } else if (ui->loadMenu.rootPath[0] != '\0') {
        (void)Global_SetInputRoot(ui->loadMenu.rootPath, true);
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        loaded = UIPanel_LoadLayoutFromPath(ui->loadMenu.entryPaths[index]);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        loaded = UIPanel_LoadSceneFromPath(ui->loadMenu.entryPaths[index]);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT) {
        loaded = UIPanel_LoadObjectAssetFromPath(ui->loadMenu.entryPaths[index]);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH) {
        loaded = UIPanel_PlaceRuntimeMeshAsSceneInstance(ui->loadMenu.entryPaths[index]);
        if (loaded) {
            UIPanel_RememberLoadedEntry(UI_LOAD_MENU_MODE_RUNTIME_MESH,
                                        ui->loadMenu.entryPaths[index]);
        }
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) {
        loaded = UIPanel_ImportStlAndPlaceFromPath(ui->loadMenu.entryPaths[index]);
        if (loaded) {
            UIPanel_RememberLoadedEntry(UI_LOAD_MENU_MODE_STL_IMPORT,
                                        ui->loadMenu.entryPaths[index]);
        }
    }
    return loaded;
}

bool UIPanel_HandleLoadMenuWheel(int mouseX, int mouseY, float wheel_delta) {
    UIPanelState* ui = UIPanel_Get();
    SDL_Rect rect = {0, 0, 0, 0};
    const float row_step = 24.0f;
    if (!ui || !UIPanel_IsLoadMenuOpen()) return false;
    rect = UIPanel_GetLoadMenuRect(ui);
    if (!SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &rect)) return false;
    if (!UIPanel_LoadMenuHasScrollableContent(ui)) return true;
    ui->loadMenu.scrollOffsetPx -= wheel_delta * (row_step * 3.0f);
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

    {
        SDL_Rect set_dir_button = UIPanel_GetLoadMenuSetDirectoryButtonRect(ui);
        if (set_dir_button.w > 0 &&
            SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &set_dir_button)) {
            return UIPanel_OpenDirectoryDialogForActiveBrowser();
        }
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
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    const bool has_face_target = state &&
        state->editor.selectedObjectAssetBodyId != 0u &&
        state->editor.selectedObjectAssetFace != OBJECT3D_FACE_NONE;
    const bool sketch_active = state &&
        (state->editor.objectFaceSketchToolArmed ||
         state->editor.objectFaceSketchDragging ||
         state->editor.objectFaceSketchHasRectangle);
    const bool use_face_sketch_controls = object_mode && (has_face_target || sketch_active);
    if (ui) {
        for (int i = 0; i < ui->count; ++i) {
            UIButton* btn = &ui->buttons[i];
            const SDL_Rect r = btn->bounds;
            btn->hovered = mouseX >= r.x && mouseX <= r.x + r.w &&
                           mouseY >= r.y && mouseY <= r.y + r.h;
            if (btn->hovered && state && state->spaceMode == SPACE_MODE_3D) {
                if (btn->id == UI_BTN_CREATE_PLANE && !use_face_sketch_controls) {
                    preview = PRIMITIVE_PLACEMENT_PREVIEW_PLANE;
                } else if (btn->id == UI_BTN_CREATE_RECT_PRISM && !use_face_sketch_controls) {
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
    UIPanel_ObjectWorkspaceHandleModelTreeMouseMotion(mouseX, mouseY);

    if (!ui || !UIPanel_IsLoadMenuOpen()) {
        if (ui) ui->loadMenu.hoverIndex = -1;
        return;
    }

    ui->loadMenu.hoverIndex = UIPanel_LoadMenuIndexAtPoint(ui, mouseX, mouseY);
}
