#include "UI/ui_panel_internal.h"

#include "Core/global_state.h"
#include "Layout/layout_json.h"
#include "Editor/editor.h"
#include "Tools/scene_export.h"
#include "Tools/shape_export.h"

#include <SDL2/SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static const char* k_legacy_layout_root = "config";

static void SanitizeBuffer(char* buffer) {
    size_t len = strlen(buffer);
    while (len > 0 && isspace((unsigned char)buffer[len - 1])) {
        buffer[--len] = '\0';
    }
}

static void UIPanel_PopulateDefaultFilename(UIPanelState* ui) {
    const char* path = Global_GetCurrentConfigPath();
    ui->saveDialog.buffer[0] = '\0';
    ui->saveDialog.length = 0;
    ui->saveDialog.cursor = 0;

    if (!path || !*path) return;

    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;

    size_t len = strlen(base);
    if (len >= 5 && strcasecmp(base + len - 5, ".json") == 0) {
        len -= 5;
    }
    if (len >= sizeof(ui->saveDialog.buffer)) len = sizeof(ui->saveDialog.buffer) - 1;

    memcpy(ui->saveDialog.buffer, base, len);
    ui->saveDialog.buffer[len] = '\0';
    ui->saveDialog.length = len;
    ui->saveDialog.cursor = len;
}

static const char* UIPanel_GetRootValue(UIRootDialogTarget target) {
    switch (target) {
        case UI_ROOT_TARGET_INPUT:
            return Global_GetInputRoot();
        case UI_ROOT_TARGET_OUTPUT:
            return Global_GetOutputRoot();
        default:
            return NULL;
    }
}

static bool UIPanel_SetRootValue(UIRootDialogTarget target, const char* value) {
    switch (target) {
        case UI_ROOT_TARGET_INPUT:
            return Global_SetInputRoot(value, true);
        case UI_ROOT_TARGET_OUTPUT:
            return Global_SetOutputRoot(value, true);
        default:
            return false;
    }
}

static bool UIPanel_SelectFolderWithPrompt(const char* prompt, char* out_path, size_t out_path_size) {
#if defined(__APPLE__)
    FILE* pipe = NULL;
    char command[512];
    if (!prompt || !out_path || out_path_size == 0) return false;
    out_path[0] = '\0';
    snprintf(command,
             sizeof(command),
             "/usr/bin/osascript -e 'POSIX path of (choose folder with prompt \"%s\")'",
             prompt);
    pipe = popen(command, "r");
    if (!pipe) return false;
    if (!fgets(out_path, (int)out_path_size, pipe)) {
        (void)pclose(pipe);
        out_path[0] = '\0';
        return false;
    }
    (void)pclose(pipe);
    SanitizeBuffer(out_path);
    return out_path[0] != '\0';
#else
    (void)prompt;
    (void)out_path;
    (void)out_path_size;
    return false;
#endif
}

void UIPanel_BeginRootDialog(UIRootDialogTarget target) {
    UIPanelState* ui = UIPanel_Get();
    const char* current = UIPanel_GetRootValue(target);
    ui->rootDialog.active = true;
    ui->rootDialog.target = target;
    if (current && *current) {
        snprintf(ui->rootDialog.buffer, sizeof(ui->rootDialog.buffer), "%s", current);
    } else {
        ui->rootDialog.buffer[0] = '\0';
    }
    SanitizeBuffer(ui->rootDialog.buffer);
    ui->rootDialog.length = strlen(ui->rootDialog.buffer);
    ui->rootDialog.cursor = ui->rootDialog.length;

    ui->loadMenu.open = false;
    UIPanel_CloseSaveDialog(ui);
    UIPanel_ClosePrismDimensionDialog(ui);
    UIPanel_CloseSceneBoundsDialog(ui);
    UIPanel_CloseConstructionPlaneDialog(ui);
    UIPanel_CloseObjectTransformDialog(ui);
    if (!SDL_IsTextInputActive()) SDL_StartTextInput();
}

bool UIPanel_ApplyRootDialog(UIPanelState* ui) {
    if (!ui || !ui->rootDialog.active) return false;
    SanitizeBuffer(ui->rootDialog.buffer);
    ui->rootDialog.length = strlen(ui->rootDialog.buffer);
    if (ui->rootDialog.length == 0) {
        SDL_Log("[UI] Root update aborted: path is empty.");
        return false;
    }
    if (!UIPanel_SetRootValue(ui->rootDialog.target, ui->rootDialog.buffer)) {
        SDL_Log("[UI] Root update failed for path '%s'", ui->rootDialog.buffer);
        return false;
    }
    if (ui->rootDialog.target == UI_ROOT_TARGET_INPUT) {
        UIPanel_RefreshConfigList();
    }
    SDL_Log("[UI] Root updated: %s", ui->rootDialog.buffer);
    UIPanel_CloseRootDialog(ui);
    return true;
}

bool UIPanel_PerformSave(UIPanelState* ui) {
    SanitizeBuffer(ui->saveDialog.buffer);
    ui->saveDialog.length = strlen(ui->saveDialog.buffer);
    if (ui->saveDialog.cursor > ui->saveDialog.length) ui->saveDialog.cursor = ui->saveDialog.length;

    if (ui->saveDialog.length == 0) {
        SDL_Log("[UI] Save aborted: filename is empty.");
        return false;
    }

    char filename[160];
    strncpy(filename, ui->saveDialog.buffer, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';

    size_t len = strlen(filename);
    if (len < 5 || strcasecmp(filename + len - 5, ".json") != 0) {
        if (len + 5 < sizeof(filename)) {
            strcat(filename, ".json");
        } else {
            SDL_Log("[UI] Save aborted: filename too long.");
            return false;
        }
    }

    char path[256];
    char fallback_path[256];
    const char* input_root = Global_GetInputRoot();
    bool saved = false;

    GlobalState* state = Global_Get();
    Layout_CompactDeletedElements(&state->layout);

    if (LineDrawingDataPaths_BuildPath(path, sizeof(path), input_root ? input_root : k_legacy_layout_root, filename)) {
        if (Layout_SaveToFile(&state->layout, path)) {
            saved = true;
        } else {
            SDL_Log("[UI] Primary save failed at %s; trying legacy fallback.", path);
        }
    }

    if (!saved) {
        if (!LineDrawingDataPaths_BuildPath(fallback_path, sizeof(fallback_path), k_legacy_layout_root, filename)) {
            SDL_Log("[UI] Save failed: invalid legacy fallback path.");
            return false;
        }
        if (!Layout_SaveToFile(&state->layout, fallback_path)) {
            SDL_Log("[UI] Failed to save layout to %s", fallback_path);
            return false;
        }
        snprintf(path, sizeof(path), "%s", fallback_path);
    }

    SDL_Log("[UI] Layout saved to %s", path);
    Global_OnLayoutSaved(path);
    Editor_ClearHistory(&state->editor);
    Editor_HistoryCapture(&state->editor, &state->layout);
    UIPanel_RefreshConfigList();
    ui->saveDialog.cursor = ui->saveDialog.length;
    UIPanel_CloseSaveDialog(ui);
    return true;
}

void UIPanel_BeginSaveDialog(void) {
    UIPanelState* ui = UIPanel_Get();
    ui->saveDialog.active = true;
    UIPanel_PopulateDefaultFilename(ui);
    ui->loadMenu.open = false;
    UIPanel_CloseRootDialog(ui);
    UIPanel_ClosePrismDimensionDialog(ui);
    UIPanel_CloseSceneBoundsDialog(ui);
    UIPanel_CloseConstructionPlaneDialog(ui);
    UIPanel_CloseObjectTransformDialog(ui);
    if (!SDL_IsTextInputActive()) SDL_StartTextInput();
}

void UIPanel_ExportScene(void) {
    GlobalState* state = Global_Get();
    LineDrawingSceneExportPaths export_paths;
    char diagnostics[256];
    const char* output_root = NULL;
    if (!state) return;

    output_root = Global_GetOutputRoot();
    Layout_CompactDeletedElements(&state->layout);

    if (!LineDrawingSceneExport_ExportLayoutToOutputRoot(&state->layout,
                                                         Global_GetCurrentConfigPath(),
                                                         output_root,
                                                         &export_paths,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        SDL_Log("[UI] Scene export failed: %s", diagnostics[0] ? diagnostics : "unknown error");
        return;
    }

    SDL_Log("[UI] Exported scene directory to %s", export_paths.scene_dir);
    SDL_Log("[UI] Exported authoring scene to %s", export_paths.authoring_path);
    SDL_Log("[UI] Exported runtime scene to %s", export_paths.runtime_path);
}

void UIPanel_BeginInputRootDialog(void) {
    UIPanel_BeginRootDialog(UI_ROOT_TARGET_INPUT);
}

void UIPanel_BeginOutputRootDialog(void) {
    UIPanel_BeginRootDialog(UI_ROOT_TARGET_OUTPUT);
}

bool UIPanel_OpenInputRootFolderDialog(void) {
    char path[256];
    if (!UIPanel_SelectFolderWithPrompt("Choose sCulpt Input Root", path, sizeof(path))) {
        SDL_Log("[UI] Input root selection canceled.");
        return false;
    }
    if (!Global_SetInputRoot(path, true)) {
        SDL_Log("[UI] Failed to set input root to %s", path);
        return false;
    }
    UIPanel_RefreshConfigList();
    SDL_Log("[UI] Input root updated: %s", path);
    return true;
}

bool UIPanel_OpenOutputRootFolderDialog(void) {
    char path[256];
    if (!UIPanel_SelectFolderWithPrompt("Choose sCulpt Output Root", path, sizeof(path))) {
        SDL_Log("[UI] Output root selection canceled.");
        return false;
    }
    if (!Global_SetOutputRoot(path, true)) {
        SDL_Log("[UI] Failed to set output root to %s", path);
        return false;
    }
    SDL_Log("[UI] Output root updated: %s", path);
    return true;
}
