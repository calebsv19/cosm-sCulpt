#include "UI/panel/ui_panel_file_browser_internal.h"

#include "Core/global_state.h"

#include <string.h>
#include <stdio.h>

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
        case UI_LOAD_MENU_MODE_OBJECT: return "Asset";
        case UI_LOAD_MENU_MODE_NONE:
        default: return "Browser";
    }
}

int UIPanel_FindRememberedLoadMenuIndex(const UIPanelState* ui) {
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
