#include "UI/ui_panel_internal.h"

#include "Core/global_state.h"
#include "UI/info_overlay.h"
#include "Layout/layout_json.h"
#include "Editor/editor.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static const char* k_legacy_layout_root = "config";

static bool UIPanel_IsLegacyLayoutRoot(const char* root) {
    if (!root || !root[0]) return true;
    if (strcmp(root, k_legacy_layout_root) == 0) return true;
    if (strcmp(root, "./config") == 0) return true;
    return false;
}

static bool UIPanel_AddConfigEntry(UIPanelState* ui, const char* name, const char* full_path) {
    if (!ui || !name || !full_path || ui->loadMenu.count >= MAX_CONFIG_FILES) return false;

    for (int i = 0; i < ui->loadMenu.count; ++i) {
        if (strcasecmp(ui->loadMenu.entries[i], name) == 0) {
            return false;
        }
    }

    snprintf(ui->loadMenu.entries[ui->loadMenu.count], sizeof(ui->loadMenu.entries[0]), "%s", name);
    snprintf(ui->loadMenu.entryPaths[ui->loadMenu.count], sizeof(ui->loadMenu.entryPaths[0]), "%s", full_path);
    ui->loadMenu.count++;
    return true;
}

static void UIPanel_ScanConfigDirectory(UIPanelState* ui, const char* root_dir) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;
    char full_path[MAX_CONFIG_PATH];

    if (!ui || !root_dir || !root_dir[0]) return;

    dir = opendir(root_dir);
    if (!dir) return;

    while ((entry = readdir(dir)) != NULL && ui->loadMenu.count < MAX_CONFIG_FILES) {
        const char* name = NULL;
        size_t len = 0;
        if (entry->d_name[0] == '.') continue;
        name = entry->d_name;
        len = strlen(name);
        if (len < 5) continue;
        if (strcasecmp(name + len - 5, ".json") != 0) continue;
        if (!LineDrawingDataPaths_BuildPath(full_path, sizeof(full_path), root_dir, name)) continue;
        (void)UIPanel_AddConfigEntry(ui, name, full_path);
    }

    closedir(dir);
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

void UIPanel_RefreshConfigList(void) {
    UIPanelState* ui = UIPanel_Get();
    const char* input_root = Global_GetInputRoot();
    ui->loadMenu.count = 0;
    ui->loadMenu.hoverIndex = -1;
    memset(ui->loadMenu.entries, 0, sizeof(ui->loadMenu.entries));
    memset(ui->loadMenu.entryPaths, 0, sizeof(ui->loadMenu.entryPaths));

    UIPanel_ScanConfigDirectory(ui, input_root ? input_root : k_legacy_layout_root);
    if (!UIPanel_IsLegacyLayoutRoot(input_root)) {
        UIPanel_ScanConfigDirectory(ui, k_legacy_layout_root);
    }

    for (int i = 0; i < ui->loadMenu.count - 1; ++i) {
        for (int j = i + 1; j < ui->loadMenu.count; ++j) {
            if (strcasecmp(ui->loadMenu.entries[j], ui->loadMenu.entries[i]) < 0) {
                UIPanel_SwapLoadEntries(ui, i, j);
            }
        }
    }
}

void UIPanel_ToggleLoadMenu(void) {
    UIPanelState* ui = UIPanel_Get();
    ui->loadMenu.open = !ui->loadMenu.open;
    if (ui->loadMenu.open) {
        UIPanel_RefreshConfigList();
        ui->loadMenu.hoverIndex = -1;
    }

    if (!ui->loadMenu.open) {
        UIPanel_CloseSaveDialog(ui);
        UIPanel_CloseRootDialog(ui);
        UIPanel_ClosePrismDimensionDialog(ui);
        UIPanel_CloseSceneBoundsDialog(ui);
        UIPanel_CloseConstructionPlaneDialog(ui);
        UIPanel_CloseObjectTransformDialog(ui);
    }
}

bool UIPanel_IsLoadMenuOpen(void) {
    return UIPanel_Get()->loadMenu.open;
}

SDL_Rect UIPanel_GetLoadMenuRect(const UIPanelState* ui) {
    int x = 12;
    int y = InfoOverlay_HeightPx() + 44;
    int w = 260;
    int count = ui ? ui->loadMenu.count : 0;
    int h = 0;

    h = (count > 0 ? count : 1) * 24 + 12;
    if (h > 300) h = 300;

    return (SDL_Rect){ x, y, w, h };
}

static void UIPanel_LoadConfigByIndex(int index) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui || index < 0 || index >= ui->loadMenu.count) return;

    char path[MAX_CONFIG_PATH] = {0};
    if (ui->loadMenu.entryPaths[index][0] != '\0') {
        snprintf(path, sizeof(path), "%s", ui->loadMenu.entryPaths[index]);
    } else if (!LineDrawingDataPaths_BuildPath(path,
                                               sizeof(path),
                                               k_legacy_layout_root,
                                               ui->loadMenu.entries[index])) {
        SDL_Log("[UI] Invalid layout path for entry '%s'", ui->loadMenu.entries[index]);
        return;
    }

    GlobalState* state = Global_Get();
    Editor_ClearHistory(&state->editor);

    if (Layout_LoadFromFile(&state->layout, path)) {
        SDL_Log("[UI] Loaded layout %s", path);
        Global_OnLayoutLoaded(path);
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
    } else {
        SDL_Log("[UI] Failed to load layout %s", path);
    }
}

bool UIPanel_HandleLoadMenuClick(int mouseX, int mouseY) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui->loadMenu.open) return false;

    SDL_Rect rect = UIPanel_GetLoadMenuRect(ui);
    if (SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &rect)) {
        if (ui->loadMenu.count == 0) {
            ui->loadMenu.open = false;
            return true;
        }
        int itemHeight = 24;
        int index = (mouseY - rect.y - 6) / itemHeight;
        if (index >= 0 && index < ui->loadMenu.count) {
            UIPanel_LoadConfigByIndex(index);
        }
        ui->loadMenu.open = false;
        return true;
    }

    ui->loadMenu.open = false;
    return false;
}

void UIPanel_HandleMouseMotion(int mouseX, int mouseY) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui->loadMenu.open) {
        ui->loadMenu.hoverIndex = -1;
        return;
    }

    SDL_Rect rect = UIPanel_GetLoadMenuRect(ui);
    if (!SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &rect)) {
        ui->loadMenu.hoverIndex = -1;
        return;
    }

    int itemHeight = 24;
    int index = (mouseY - rect.y - 6) / itemHeight;
    if (index >= 0 && index < ui->loadMenu.count) {
        ui->loadMenu.hoverIndex = index;
    } else {
        ui->loadMenu.hoverIndex = -1;
    }
}
