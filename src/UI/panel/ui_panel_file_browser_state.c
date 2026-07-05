#include "UI/panel/ui_panel_file_browser_internal.h"

#include "Core/global_state.h"

#include <string.h>

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

    mode_name = UIPanel_FileStatusBrowseModeName(ui->loadMenu.mode);
    if (UIPanel_GetFileBrowserSelectionInfo(ui, &selection_state, &selection_path) &&
        selection_path && selection_path[0]) {
        return UIPanel_FileStatusWriteMessage(
            out_text,
            out_text_size,
            "Browser",
            "%s row %s",
            selection_state == UI_LOAD_MENU_SELECTION_ACTIVE_SESSION ? "Active" : "Remembered",
            UIPanel_FileStatusDisplayBaseName(selection_path));
    }

    if (ui->loadMenu.count > 0) {
        return UIPanel_FileStatusWriteMessage(out_text,
                                              out_text_size,
                                              "Browser",
                                              "%s mode has entries but no active row",
                                              mode_name);
    }

    return UIPanel_FileStatusWriteMessage(out_text,
                                          out_text_size,
                                          "Browser",
                                          "%s mode has no entries",
                                          mode_name);
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

    mode_name = UIPanel_FileStatusBrowseModeName(ui->loadMenu.mode);
    if (ui->loadMenu.loadProgressState == UI_LOAD_PROGRESS_LOADING &&
        ui->loadMenu.loadProgressMode == ui->loadMenu.mode) {
        return UIPanel_FileStatusWriteMessage(
            out_text,
            out_text_size,
            "Actions",
            "%s is loading. Wait for the expanded row to finish before starting another load.",
            ui->loadMenu.loadProgressLabel[0] ? ui->loadMenu.loadProgressLabel : mode_name);
    }
    if ((ui->loadMenu.loadProgressState == UI_LOAD_PROGRESS_COMPLETE ||
         ui->loadMenu.loadProgressState == UI_LOAD_PROGRESS_FAILED) &&
        ui->loadMenu.loadProgressMode == ui->loadMenu.mode &&
        ui->loadMenu.loadProgressDetail[0]) {
        return UIPanel_FileStatusWriteMessage(out_text,
                                              out_text_size,
                                              "Actions",
                                              "%s",
                                              ui->loadMenu.loadProgressDetail);
    }

    const char* runtime_mesh_status = Global_GetObjectRuntimeMeshStatus();
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT &&
        runtime_mesh_status &&
        strncmp(runtime_mesh_status,
                "STL import failed:",
                strlen("STL import failed:")) == 0) {
        return UIPanel_FileStatusWriteMessage(out_text,
                                              out_text_size,
                                              "Actions",
                                              "%s",
                                              runtime_mesh_status);
    }

    if (UIPanel_GetFileBrowserSelectionInfo(ui, &selection_state, &selection_path)) {
        if (selection_state == UI_LOAD_MENU_SELECTION_ACTIVE_SESSION) {
            return UIPanel_FileStatusWriteMessage(out_text,
                                                  out_text_size,
                                                  "Actions",
                                                  "%s",
                                                  "Use Session re-centers the live row if the browse root drifts. Clear Last is only for remembered fallback rows.");
        }
        if (selection_state == UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY) {
            return UIPanel_FileStatusWriteMessage(out_text,
                                                  out_text_size,
                                                  "Actions",
                                                  "%s",
                                                  "Use Session restores the live session row. Clear Last removes this remembered fallback row.");
        }
    }

    if (ui->loadMenu.count > 0) {
        return UIPanel_FileStatusWriteMessage(
            out_text,
            out_text_size,
            "Actions",
            "Use Session targets the live session row. Clear Last removes any stale remembered fallback for %s mode.",
            mode_name);
    }

    return UIPanel_FileStatusWriteMessage(out_text,
                                          out_text_size,
                                          "Actions",
                                          "Use Set Directory in the browser header to pick the %s root.",
                                          mode_name);
}
