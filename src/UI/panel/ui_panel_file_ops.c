#include "UI/ui_panel_internal.h"

#include "Core/global_state.h"
#include "Layout/layout_json.h"
#include "Editor/editor.h"
#include "Tools/scene_import.h"
#include "Tools/scene_export.h"
#include "Tools/shape_export.h"

#include <SDL2/SDL.h>
#include <ctype.h>
#include <stdbool.h>
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

static void EscapeAppleScriptString(const char* input, char* output, size_t output_size) {
    size_t out_index = 0;
    if (!output || output_size == 0) return;
    output[0] = '\0';
    if (!input) return;

    for (size_t i = 0; input[i] != '\0' && out_index + 1 < output_size; ++i) {
        const char c = input[i];
        if ((c == '\\' || c == '"') && out_index + 2 < output_size) {
            output[out_index++] = '\\';
        }
        if (out_index + 1 >= output_size) break;
        output[out_index++] = c;
    }
    output[out_index] = '\0';
}

static void UIPanel_GetDefaultLoadDirectory(char* out_dir, size_t out_dir_size) {
    const char* current_path = Global_GetCurrentConfigPath();
    const char* input_root = Global_GetInputRoot();
    if (!out_dir || out_dir_size == 0) return;

    out_dir[0] = '\0';
    if (current_path && current_path[0] != '\0') {
        const char* last_slash = strrchr(current_path, '/');
        if (last_slash && last_slash != current_path) {
            const size_t len = (size_t)(last_slash - current_path);
            const size_t copy_len = len < out_dir_size - 1 ? len : out_dir_size - 1;
            memcpy(out_dir, current_path, copy_len);
            out_dir[copy_len] = '\0';
            return;
        }
    }

    if (input_root && input_root[0] != '\0') {
        snprintf(out_dir, out_dir_size, "%s", input_root);
        return;
    }

    snprintf(out_dir, out_dir_size, "%s", k_legacy_layout_root);
}

static void UIPanel_GetDefaultSceneSelectionDirectory(char* out_dir, size_t out_dir_size) {
    const char* current_scene_path = Global_GetCurrentSceneAuthoringPath();
    const char* input_root = Global_GetInputRoot();
    if (!out_dir || out_dir_size == 0) return;
    out_dir[0] = '\0';

    if (current_scene_path && current_scene_path[0] != '\0') {
        const char* last_slash = strrchr(current_scene_path, '/');
        if (last_slash && last_slash != current_scene_path) {
            const size_t len = (size_t)(last_slash - current_scene_path);
            const size_t copy_len = len < out_dir_size - 1 ? len : out_dir_size - 1;
            memcpy(out_dir, current_scene_path, copy_len);
            out_dir[copy_len] = '\0';
            return;
        }
    }

    if (input_root && input_root[0] != '\0') {
        snprintf(out_dir, out_dir_size, "%s", input_root);
        return;
    }

    snprintf(out_dir, out_dir_size, "%s", k_legacy_layout_root);
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

static bool UIPanel_DeriveLayoutHintFromScenePath(const char* scene_path,
                                                  char* out_path,
                                                  size_t out_path_size) {
    const char* last_slash = NULL;
    const char* dir_name = NULL;
    size_t dir_len = 0u;
    size_t dir_name_len = 0u;
    if (!scene_path || !scene_path[0] || !out_path || out_path_size == 0u) return false;

    last_slash = strrchr(scene_path, '/');
    if (!last_slash || last_slash == scene_path) return false;
    dir_len = (size_t)(last_slash - scene_path);

    dir_name = last_slash;
    while (dir_name > scene_path && dir_name[-1] != '/') {
        --dir_name;
    }
    dir_name_len = (size_t)(last_slash - dir_name);
    if (dir_name_len == 0u) return false;

    if (snprintf(out_path,
                 out_path_size,
                 "%.*s/%.*s.json",
                 (int)dir_len,
                 scene_path,
                 (int)dir_name_len,
                 dir_name) >= (int)out_path_size) {
        return false;
    }
    return true;
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

static bool UIPanel_SelectFolderWithPromptAndDefault(const char* prompt,
                                                     const char* default_dir,
                                                     char* out_path,
                                                     size_t out_path_size) {
#if defined(__APPLE__)
    FILE* pipe = NULL;
    char command[1024];
    char escaped_prompt[256];
    char escaped_dir[LINE_DRAWING_PATH_CAP * 2];

    if (!prompt || !default_dir || !default_dir[0] || !out_path || out_path_size == 0) return false;

    out_path[0] = '\0';
    EscapeAppleScriptString(prompt, escaped_prompt, sizeof(escaped_prompt));
    EscapeAppleScriptString(default_dir, escaped_dir, sizeof(escaped_dir));
    snprintf(command,
             sizeof(command),
             "/usr/bin/osascript "
             "-e 'set defaultDir to POSIX file \"%s\"' "
             "-e 'POSIX path of (choose folder with prompt \"%s\" default location defaultDir)'",
             escaped_dir,
             escaped_prompt);
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
    (void)default_dir;
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

bool UIPanel_LoadSceneFromPath(const char* path) {
    GlobalState* state = Global_Get();
    char diagnostics[256];
    char layout_hint[LINE_DRAWING_PATH_CAP];
    if (!state || !path || path[0] == '\0') return false;

    Editor_ClearHistory(&state->editor);

    if (!LineDrawingSceneImport_LoadLayoutFromAuthoringFile(&state->layout,
                                                            path,
                                                            diagnostics,
                                                            sizeof(diagnostics))) {
        SDL_Log("[UI] Failed to import scene %s (%s)", path, diagnostics[0] ? diagnostics : "unknown");
        return false;
    }

    layout_hint[0] = '\0';
    if (!UIPanel_DeriveLayoutHintFromScenePath(path, layout_hint, sizeof(layout_hint))) {
        snprintf(layout_hint, sizeof(layout_hint), "%s", path);
    }

    SDL_Log("[UI] Imported scene %s", path);
    Global_OnSceneLoaded(path, layout_hint);
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

bool UIPanel_OpenJsonFolderDialog(void) {
    char selected_folder[LINE_DRAWING_PATH_CAP];
    char default_dir[LINE_DRAWING_PATH_CAP];

    UIPanel_GetDefaultLoadDirectory(default_dir, sizeof(default_dir));
    if (!UIPanel_SelectFolderWithPromptAndDefault("Choose sCulpt JSON Root",
                                                  default_dir,
                                                  selected_folder,
                                                  sizeof(selected_folder))) {
        SDL_Log("[UI] JSON root selection canceled.");
        return false;
    }

    if (!UIPanel_LoadJsonFromFolderSelection(selected_folder, true)) {
        SDL_Log("[UI] JSON root selection rejected: %s", selected_folder);
        return false;
    }

    return true;
}

bool UIPanel_OpenSceneFolderDialog(void) {
    char selected_folder[LINE_DRAWING_PATH_CAP];
    char default_dir[LINE_DRAWING_PATH_CAP];

    UIPanel_GetDefaultSceneSelectionDirectory(default_dir, sizeof(default_dir));
    if (!UIPanel_SelectFolderWithPromptAndDefault("Choose sCulpt Scene Folder or Scene Root",
                                                  default_dir,
                                                  selected_folder,
                                                  sizeof(selected_folder))) {
        SDL_Log("[UI] Scene folder selection canceled.");
        return false;
    }

    if (!UIPanel_LoadSceneFromFolderSelection(selected_folder, true)) {
        SDL_Log("[UI] Scene folder selection rejected: %s", selected_folder);
        return false;
    }

    return true;
}

void UIPanel_ExportScene(void) {
    GlobalState* state = Global_Get();
    LineDrawingSceneExportPaths export_paths;
    char diagnostics[256];
    const char* output_root = NULL;
    const char* authoring_path = NULL;
    if (!state) return;

    output_root = Global_GetOutputRoot();
    authoring_path = Global_GetCurrentSceneAuthoringPath();
    Layout_CompactDeletedElements(&state->layout);

    if ((authoring_path && authoring_path[0] != '\0')
            ? !LineDrawingSceneExport_ExportLayoutToAuthoringPath(&state->layout,
                                                                  authoring_path,
                                                                  &export_paths,
                                                                  diagnostics,
                                                                  sizeof(diagnostics))
            : !LineDrawingSceneExport_ExportLayoutToOutputRoot(&state->layout,
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
