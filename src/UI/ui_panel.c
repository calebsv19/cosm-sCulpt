#include "UI/ui_panel.h"
#include "UI/info_overlay.h"
#include "UI/font_manager.h"
#include "UI/shared_theme_font_adapter.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Layout/layout_json.h"
#include "Editor/editor.h"
#include "Tools/shape_from_layout.h"
#include "Tools/shape_export.h"
#include "ShapeLib/shape_json.h"
#include "Render/vulkan_adapter.h"
#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static UIPanelState g_uiPanel;  // Internal static state

static UIButton* UIPanel_FindButtonById(UIPanelState* ui, int id) {
    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == id) return &ui->buttons[i];
    }
    return NULL;
}

static void AddButton(UIPanelState* ui, const char* label, int x, int y, int w, int h, UIPanelSide side, int id) {
    if (ui->count >= MAX_UI_BUTTONS) return;

    UIButton* b = &ui->buttons[ui->count++];
    strncpy(b->label, label, sizeof(b->label) - 1);
    b->label[sizeof(b->label) - 1] = '\0';
    b->bounds = (SDL_Rect){ x, y, w, h };
    b->side = side;
    b->id = id;
    b->hovered = false;
    b->pressed = false;
}

static void SanitizeBuffer(char* buffer) {
    size_t len = strlen(buffer);
    while (len > 0 && isspace((unsigned char)buffer[len - 1])) {
        buffer[--len] = '\0';
    }
}

static void UIPanel_StopTextInputIfIdle(void) {
    if (!g_uiPanel.saveDialog.active &&
        !g_uiPanel.rootDialog.active &&
        SDL_IsTextInputActive()) {
        SDL_StopTextInput();
    }
}

static void UIPanel_CloseRootDialog(UIPanelState* ui) {
    if (!ui->rootDialog.active) return;
    ui->rootDialog.active = false;
    ui->rootDialog.target = UI_ROOT_TARGET_NONE;
    ui->rootDialog.buffer[0] = '\0';
    ui->rootDialog.length = 0;
    ui->rootDialog.cursor = 0;
    UIPanel_StopTextInputIfIdle();
}

static void UIPanel_CloseSaveDialog(UIPanelState* ui) {
    if (!ui->saveDialog.active) return;
    ui->saveDialog.active = false;
    ui->saveDialog.buffer[0] = '\0';
    ui->saveDialog.length = 0;
    ui->saveDialog.cursor = 0;
    UIPanel_StopTextInputIfIdle();
}

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
    UIPanelState* ui = &g_uiPanel;
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

UIPanelState* UIPanel_Get(void) {
    return &g_uiPanel;
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

static void UIPanel_BeginRootDialog(UIRootDialogTarget target) {
    UIPanelState* ui = &g_uiPanel;
    const char* current = UIPanel_GetRootValue(target);
    ui->rootDialog.active = true;
    ui->rootDialog.target = target;
    ui->rootDialog.buffer[0] = '\0';
    ui->rootDialog.length = 0;
    ui->rootDialog.cursor = 0;
    if (current && current[0]) {
        snprintf(ui->rootDialog.buffer, sizeof(ui->rootDialog.buffer), "%s", current);
        ui->rootDialog.length = strlen(ui->rootDialog.buffer);
        ui->rootDialog.cursor = ui->rootDialog.length;
    }
    ui->loadMenu.open = false;
    UIPanel_CloseSaveDialog(ui);
    if (!SDL_IsTextInputActive()) SDL_StartTextInput();
}

static bool UIPanel_ApplyRootDialog(UIPanelState* ui) {
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

static bool UIPanel_PerformSave(UIPanelState* ui) {
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

void UIPanel_ExportShape(void) {
    GlobalState* state = Global_Get();
    if (!state) return;

    const char* requested = Global_GetCurrentConfigPath();
    if (!requested || !*requested) {
        requested = "layout_export.json";
    }

    char exportPath[SHAPE_EXPORT_PATH_MAX];
    if (!ShapeExport_BuildPath(requested, exportPath, sizeof(exportPath))) {
        SDL_Log("[UI] Export failed: unable to prepare export path for '%s'", requested);
        return;
    }

    Layout_CompactDeletedElements(&state->layout);

    ShapeDocument doc;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    ViewPlaneAxis exportAxis = SpaceAdapter_ActivePlaneAxis(&viewCtx);
    if (!ShapeDocument_FromLayoutProjected(requested, &state->layout, exportAxis, &doc)) {
        SDL_Log("[UI] Export failed: could not build shape data.");
        return;
    }

    if (ShapeDocument_SaveToJsonFile(&doc, exportPath)) {
        SDL_Log("[UI] Exported Shape JSON to %s", exportPath);
    } else {
        SDL_Log("[UI] Export failed: could not write %s", exportPath);
    }

    ShapeDocument_Free(&doc);
}

void UIPanel_Init(int screenW, int screenH) {
    (void)screenH;
    g_uiPanel.count = 0;
    g_uiPanel.saveDialog.active = false;
    g_uiPanel.saveDialog.buffer[0] = '\0';
    g_uiPanel.saveDialog.length = 0;
    g_uiPanel.saveDialog.cursor = 0;
    g_uiPanel.loadMenu.open = false;
    g_uiPanel.loadMenu.count = 0;
    g_uiPanel.loadMenu.hoverIndex = -1;
    g_uiPanel.rootDialog.active = false;
    g_uiPanel.rootDialog.target = UI_ROOT_TARGET_NONE;
    g_uiPanel.rootDialog.buffer[0] = '\0';
    g_uiPanel.rootDialog.length = 0;
    g_uiPanel.rootDialog.cursor = 0;

    int padding = 10;
    int btnW = 140;
    int btnH = 28;
    int spacing = 6;

    int topOffset = INFO_OVERLAY_HEIGHT + padding;

    int xL = padding;
    int yL = topOffset;
    AddButton(&g_uiPanel, "Save JSON", xL, yL, btnW, btnH, UI_PANEL_LEFT, UI_BTN_SAVE_JSON);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Load JSON", xL, yL, btnW, btnH, UI_PANEL_LEFT, UI_BTN_LOAD_JSON);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Export Shape", xL, yL, btnW, btnH, UI_PANEL_LEFT, UI_BTN_EXPORT_SHAPE);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Input Edit", xL, yL, btnW, btnH, UI_PANEL_LEFT, UI_BTN_INPUT_ROOT_EDIT);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Input Folder", xL, yL, btnW, btnH, UI_PANEL_LEFT, UI_BTN_INPUT_ROOT_FOLDER);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Output Edit", xL, yL, btnW, btnH, UI_PANEL_LEFT, UI_BTN_OUTPUT_ROOT_EDIT);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Output Folder", xL, yL, btnW, btnH, UI_PANEL_LEFT, UI_BTN_OUTPUT_ROOT_FOLDER);

    int xR = screenW - btnW - padding;
    int yR = topOffset;
    AddButton(&g_uiPanel, "Reset Origin (O)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, UI_BTN_RESET_ORIGIN);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Zoom In (+)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, UI_BTN_ZOOM_IN);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Zoom Out (-)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, UI_BTN_ZOOM_OUT);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Toggle Delete (D)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, UI_BTN_TOGGLE_DELETE);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Pin Anchor (P)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, UI_BTN_PIN_ANCHOR);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Link Handles (L)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, UI_BTN_LINK_HANDLES);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Mode: 3D (M)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, UI_BTN_TOGGLE_SPACE_MODE);

    UIPanel_RefreshConfigList();
}

const UIButton* UIPanel_GetButtons(UIPanelState* ui, int* outCount) {
    if (outCount) *outCount = ui->count;
    return ui->buttons;
}

void UIPanel_BeginSaveDialog(void) {
    UIPanelState* ui = &g_uiPanel;
    ui->saveDialog.active = true;
    UIPanel_PopulateDefaultFilename(ui);
    ui->loadMenu.open = false;
    UIPanel_CloseRootDialog(ui);
    if (!SDL_IsTextInputActive()) SDL_StartTextInput();
}

bool UIPanel_IsSaveDialogActive(void) {
    return g_uiPanel.saveDialog.active;
}

bool UIPanel_IsRootDialogActive(void) {
    return g_uiPanel.rootDialog.active;
}

bool UIPanel_IsCapturingKeyboard(void) {
    return UIPanel_IsSaveDialogActive() || UIPanel_IsRootDialogActive();
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
        SDL_Log("[UI] Input root folder dialog canceled/unavailable.");
        return false;
    }
    if (!Global_SetInputRoot(path, true)) {
        SDL_Log("[UI] Failed to set input root: %s", path);
        return false;
    }
    UIPanel_RefreshConfigList();
    SDL_Log("[UI] Input root updated from folder dialog: %s", path);
    return true;
}

bool UIPanel_OpenOutputRootFolderDialog(void) {
    char path[256];
    if (!UIPanel_SelectFolderWithPrompt("Choose sCulpt Output Root", path, sizeof(path))) {
        SDL_Log("[UI] Output root folder dialog canceled/unavailable.");
        return false;
    }
    if (!Global_SetOutputRoot(path, true)) {
        SDL_Log("[UI] Failed to set output root: %s", path);
        return false;
    }
    SDL_Log("[UI] Output root updated from folder dialog: %s", path);
    return true;
}

bool UIPanel_HandleTextInput(const char* text) {
    UIPanelState* ui = &g_uiPanel;
    if (!text) return false;

    if (ui->saveDialog.active) {
        for (const char* p = text; *p; ++p) {
            unsigned char c = (unsigned char)*p;
            if (!(isalnum(c) || c == '_' || c == '-' || c == ' ')) continue;
            if (ui->saveDialog.length + 1 >= sizeof(ui->saveDialog.buffer)) break;
            memmove(&ui->saveDialog.buffer[ui->saveDialog.cursor + 1],
                    &ui->saveDialog.buffer[ui->saveDialog.cursor],
                    ui->saveDialog.length - ui->saveDialog.cursor + 1);
            ui->saveDialog.buffer[ui->saveDialog.cursor] = (char)c;
            ui->saveDialog.length++;
            ui->saveDialog.cursor++;
        }
        ui->saveDialog.buffer[ui->saveDialog.length] = '\0';
        return true;
    }

    if (ui->rootDialog.active) {
        for (const char* p = text; *p; ++p) {
            unsigned char c = (unsigned char)*p;
            if (c < 32 || c == 127) continue;
            if (ui->rootDialog.length + 1 >= sizeof(ui->rootDialog.buffer)) break;
            memmove(&ui->rootDialog.buffer[ui->rootDialog.cursor + 1],
                    &ui->rootDialog.buffer[ui->rootDialog.cursor],
                    ui->rootDialog.length - ui->rootDialog.cursor + 1);
            ui->rootDialog.buffer[ui->rootDialog.cursor] = (char)c;
            ui->rootDialog.length++;
            ui->rootDialog.cursor++;
        }
        ui->rootDialog.buffer[ui->rootDialog.length] = '\0';
        return true;
    }

    return false;
}

bool UIPanel_HandleKeyEvent(const SDL_Event* event) {
    UIPanelState* ui = &g_uiPanel;
    if (event->type != SDL_KEYDOWN) return false;

    SDL_Keycode key = event->key.keysym.sym;
    if (ui->saveDialog.active) {
        if (key == SDLK_RETURN) {
            return UIPanel_PerformSave(ui);
        }
        if (key == SDLK_ESCAPE) {
            UIPanel_CloseSaveDialog(ui);
            return true;
        }
        switch (key) {
            case SDLK_BACKSPACE:
                if (ui->saveDialog.cursor > 0 && ui->saveDialog.length > 0) {
                    memmove(&ui->saveDialog.buffer[ui->saveDialog.cursor - 1],
                            &ui->saveDialog.buffer[ui->saveDialog.cursor],
                            ui->saveDialog.length - ui->saveDialog.cursor + 1);
                    ui->saveDialog.cursor--;
                    ui->saveDialog.length--;
                }
                return true;
            case SDLK_DELETE:
                if (ui->saveDialog.cursor < ui->saveDialog.length) {
                    memmove(&ui->saveDialog.buffer[ui->saveDialog.cursor],
                            &ui->saveDialog.buffer[ui->saveDialog.cursor + 1],
                            ui->saveDialog.length - ui->saveDialog.cursor);
                    ui->saveDialog.length--;
                }
                return true;
            case SDLK_LEFT:
                if (ui->saveDialog.cursor > 0) ui->saveDialog.cursor--;
                return true;
            case SDLK_RIGHT:
                if (ui->saveDialog.cursor < ui->saveDialog.length) ui->saveDialog.cursor++;
                return true;
            case SDLK_HOME:
                ui->saveDialog.cursor = 0;
                return true;
            case SDLK_END:
                ui->saveDialog.cursor = ui->saveDialog.length;
                return true;
            default:
                break;
        }
        return false;
    }

    if (ui->rootDialog.active) {
        if (key == SDLK_RETURN) {
            return UIPanel_ApplyRootDialog(ui);
        }
        if (key == SDLK_ESCAPE) {
            UIPanel_CloseRootDialog(ui);
            return true;
        }
        switch (key) {
            case SDLK_BACKSPACE:
                if (ui->rootDialog.cursor > 0 && ui->rootDialog.length > 0) {
                    memmove(&ui->rootDialog.buffer[ui->rootDialog.cursor - 1],
                            &ui->rootDialog.buffer[ui->rootDialog.cursor],
                            ui->rootDialog.length - ui->rootDialog.cursor + 1);
                    ui->rootDialog.cursor--;
                    ui->rootDialog.length--;
                }
                return true;
            case SDLK_DELETE:
                if (ui->rootDialog.cursor < ui->rootDialog.length) {
                    memmove(&ui->rootDialog.buffer[ui->rootDialog.cursor],
                            &ui->rootDialog.buffer[ui->rootDialog.cursor + 1],
                            ui->rootDialog.length - ui->rootDialog.cursor);
                    ui->rootDialog.length--;
                }
                return true;
            case SDLK_LEFT:
                if (ui->rootDialog.cursor > 0) ui->rootDialog.cursor--;
                return true;
            case SDLK_RIGHT:
                if (ui->rootDialog.cursor < ui->rootDialog.length) ui->rootDialog.cursor++;
                return true;
            case SDLK_HOME:
                ui->rootDialog.cursor = 0;
                return true;
            case SDLK_END:
                ui->rootDialog.cursor = ui->rootDialog.length;
                return true;
            default:
                break;
        }
        return false;
    }

    return false;
}

void UIPanel_ToggleLoadMenu(void) {
    UIPanelState* ui = &g_uiPanel;
    ui->loadMenu.open = !ui->loadMenu.open;
    if (ui->loadMenu.open) {
        UIPanel_RefreshConfigList();
    }
    ui->loadMenu.hoverIndex = -1;
    UIPanel_CloseSaveDialog(ui);
    UIPanel_CloseRootDialog(ui);
}

bool UIPanel_IsLoadMenuOpen(void) {
    return g_uiPanel.loadMenu.open;
}

static SDL_Rect UIPanel_GetLoadMenuRect(const UIPanelState* ui) {
    const UIButton* loadBtn = UIPanel_FindButtonById((UIPanelState*)ui, UI_BTN_LOAD_JSON);
    SDL_Rect rect = {0, 0, 0, 0};
    if (!loadBtn) return rect;

    rect.x = loadBtn->bounds.x + loadBtn->bounds.w + 10;
    rect.y = loadBtn->bounds.y;
    rect.w = 200;
    int itemHeight = 24;
    int count = ui->loadMenu.count > 0 ? ui->loadMenu.count : 1;
    rect.h = count * itemHeight + 12;
    return rect;
}

static void UIPanel_LoadConfigByIndex(int index) {
    UIPanelState* ui = &g_uiPanel;
    if (index < 0 || index >= ui->loadMenu.count) return;

    char path[256];
    if (ui->loadMenu.entryPaths[index][0]) {
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
        state->editor.hoveredAnchorIndex = -1;
        state->editor.hoveredWallIndex = -1;
        state->editor.hoveredHandleAnchor = -1;
        state->editor.hoveredHandleComponent = -1;
        state->editor.hoveredGizmoAxis = -1;
        Editor_HistoryCapture(&state->editor, &state->layout);
    } else {
        SDL_Log("[UI] Failed to load layout %s", path);
    }
}

bool UIPanel_HandleLoadMenuClick(int mouseX, int mouseY) {
    UIPanelState* ui = &g_uiPanel;
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
    UIPanelState* ui = &g_uiPanel;
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

static void DrawText(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color) {
    if (!text || !*text) return;
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;

#if USE_VULKAN
    VulkanAdapter_DrawText(renderer, font, text, x, y, color);
#else
    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }

    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
#endif
}

static void DrawTextClipped(SDL_Renderer* renderer,
                            const char* text,
                            int x,
                            int y,
                            int max_width,
                            SDL_Color color) {
    TTF_Font* font = NULL;
    static const char* k_ellipsis = "...";
    char clipped[512];
    size_t len = 0;
    int text_w = 0;
    int ellipsis_w = 0;

    if (!text || !text[0] || max_width <= 0) return;
    font = FontManager_Get(FONT_DEFAULT);
    if (!font) return;

    if (TTF_SizeUTF8(font, text, &text_w, NULL) == 0 && text_w <= max_width) {
        DrawText(renderer, text, x, y, color);
        return;
    }

    if (TTF_SizeUTF8(font, k_ellipsis, &ellipsis_w, NULL) != 0 || ellipsis_w >= max_width) {
        return;
    }

    len = strlen(text);
    while (len > 0) {
        --len;
        if (len + strlen(k_ellipsis) + 1 >= sizeof(clipped)) continue;
        memcpy(clipped, text, len);
        clipped[len] = '\0';
        strcat(clipped, k_ellipsis);
        if (TTF_SizeUTF8(font, clipped, &text_w, NULL) == 0 && text_w <= max_width) {
            DrawText(renderer, clipped, x, y, color);
            return;
        }
    }
}

static void RenderSaveDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    if (!ui->saveDialog.active) return;
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    int width = Global_GetScreenWidth();
    int height = Global_GetScreenHeight();

    SDL_Rect backdrop = { 0, 0, width, height };
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.modal_scrim.r, palette.modal_scrim.g,
                               palette.modal_scrim.b, palette.modal_scrim.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
    }
    SDL_RenderFillRect(renderer, &backdrop);

    SDL_Rect panel = {
        width / 2 - 220,
        INFO_OVERLAY_HEIGHT + 20,
        440,
        130
    };

    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 35, 40, 48, 240);
    }
    SDL_RenderFillRect(renderer, &panel);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 255);
    }
    SDL_RenderDrawRect(renderer, &panel);

    int textX = panel.x + 16;
    int textY = panel.y + 16;
    char line[256];

    snprintf(line, sizeof(line), "Save layout as (*.json):");
    DrawText(renderer, line, textX, textY,
             has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});

    SDL_Rect inputRect = {
        panel.x + 14,
        panel.y + 48,
        panel.w - 28,
        32
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.background_fill.r, palette.background_fill.g,
                               palette.background_fill.b, palette.background_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    }
    SDL_RenderFillRect(renderer, &inputRect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &inputRect);

    char buffer[200];
    snprintf(buffer, sizeof(buffer), "%s", ui->saveDialog.buffer);
    DrawTextClipped(renderer,
                    buffer,
                    inputRect.x + 8,
                    inputRect.y + 6,
                    inputRect.w - 16,
                    has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        char caretBuf[128];
        size_t len = ui->saveDialog.cursor;
        if (len > sizeof(caretBuf) - 1) len = sizeof(caretBuf) - 1;
        memcpy(caretBuf, ui->saveDialog.buffer, len);
        caretBuf[len] = '\0';
        int caretOffset = 0;
        TTF_SizeUTF8(font, caretBuf, &caretOffset, NULL);
        int caretX = inputRect.x + 8 + caretOffset;
        if (caretX > inputRect.x + inputRect.w - 8) caretX = inputRect.x + inputRect.w - 8;
        int caretTop = inputRect.y + 4;
        int caretBottom = inputRect.y + inputRect.h - 4;
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.text_primary.r, palette.text_primary.g,
                                   palette.text_primary.b, 220);
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
        }
        SDL_RenderDrawLine(renderer, caretX, caretTop, caretX, caretBottom);
    }

    snprintf(line, sizeof(line), "Press Enter to confirm, Esc to cancel.");
    DrawText(renderer, line, textX, panel.y + panel.h - 36,
             has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});
}

static void RenderRootDialog(SDL_Renderer* renderer, const UIPanelState* ui) {
    const char* title = NULL;
    const char* description = "Enter to apply, Esc to cancel.";
    int width = 0;
    int height = 0;
    int textX = 0;
    int textY = 0;
    SDL_Rect backdrop = {0, 0, 0, 0};
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect inputRect = {0, 0, 0, 0};
    TTF_Font* font = NULL;
    char caretBuf[256];
    int caretOffset = 0;
    int caretX = 0;
    int caretTop = 0;
    int caretBottom = 0;
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    if (!ui->rootDialog.active) return;

    switch (ui->rootDialog.target) {
        case UI_ROOT_TARGET_INPUT:
            title = "Set Input Root";
            break;
        case UI_ROOT_TARGET_OUTPUT:
            title = "Set Output Root";
            break;
        default:
            title = "Set Root";
            break;
    }

    width = Global_GetScreenWidth();
    height = Global_GetScreenHeight();
    backdrop = (SDL_Rect){0, 0, width, height};
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.modal_scrim.r, palette.modal_scrim.g,
                               palette.modal_scrim.b, palette.modal_scrim.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    }
    SDL_RenderFillRect(renderer, &backdrop);

    panel = (SDL_Rect){
        width / 2 - 300,
        INFO_OVERLAY_HEIGHT + 20,
        600,
        154
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 35, 40, 48, 240);
    }
    SDL_RenderFillRect(renderer, &panel);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 255);
    }
    SDL_RenderDrawRect(renderer, &panel);

    textX = panel.x + 16;
    textY = panel.y + 14;
    DrawText(renderer,
             title,
             textX,
             textY,
             has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
    DrawText(renderer,
             description,
             textX,
             textY + 22,
             has_shared_palette ? palette.text_muted : (SDL_Color){180, 180, 190, 255});

    inputRect = (SDL_Rect){
        panel.x + 14,
        panel.y + 58,
        panel.w - 28,
        34
    };
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.background_fill.r, palette.background_fill.g,
                               palette.background_fill.b, palette.background_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    }
    SDL_RenderFillRect(renderer, &inputRect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    }
    SDL_RenderDrawRect(renderer, &inputRect);
    DrawTextClipped(renderer,
                    ui->rootDialog.buffer,
                    inputRect.x + 8,
                    inputRect.y + 8,
                    inputRect.w - 16,
                    has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255});

    font = FontManager_Get(FONT_DEFAULT);
    if (font) {
        size_t len = ui->rootDialog.cursor;
        if (len >= sizeof(caretBuf)) len = sizeof(caretBuf) - 1;
        memcpy(caretBuf, ui->rootDialog.buffer, len);
        caretBuf[len] = '\0';
        if (TTF_SizeUTF8(font, caretBuf, &caretOffset, NULL) == 0) {
            caretX = inputRect.x + 8 + caretOffset;
            if (caretX > inputRect.x + inputRect.w - 8) caretX = inputRect.x + inputRect.w - 8;
            caretTop = inputRect.y + 4;
            caretBottom = inputRect.y + inputRect.h - 4;
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.text_primary.r, palette.text_primary.g,
                                       palette.text_primary.b, 220);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            }
            SDL_RenderDrawLine(renderer, caretX, caretTop, caretX, caretBottom);
        }
    }
}

static void RenderLoadMenu(SDL_Renderer* renderer, const UIPanelState* ui) {
    if (!ui->loadMenu.open) return;
    LineDrawing3dThemePalette palette = {0};
    const bool has_shared_palette = line_drawing3d_shared_theme_resolve_palette(&palette);

    SDL_Rect rect = UIPanel_GetLoadMenuRect(ui);
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 28, 32, 40, 240);
    }
    SDL_RenderFillRect(renderer, &rect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 255);
    }
    SDL_RenderDrawRect(renderer, &rect);

    int itemHeight = 24;
    int y = rect.y + 6;

    if (ui->loadMenu.count == 0) {
        DrawText(renderer, "(No layouts found)", rect.x + 10, y,
                 has_shared_palette ? palette.text_muted : (SDL_Color){190, 190, 195, 255});
        return;
    }

    for (int i = 0; i < ui->loadMenu.count; ++i) {
        if (i == ui->loadMenu.hoverIndex) {
            SDL_Rect highlight = { rect.x + 2, y - 2, rect.w - 4, itemHeight };
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.menu_highlight.r, palette.menu_highlight.g,
                                       palette.menu_highlight.b, palette.menu_highlight.a);
            } else {
                SDL_SetRenderDrawColor(renderer, 60, 90, 140, 180);
            }
            SDL_RenderFillRect(renderer, &highlight);
        }
        DrawTextClipped(renderer,
                        ui->loadMenu.entries[i],
                        rect.x + 10,
                        y + 2,
                        rect.w - 20,
                        has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
        y += itemHeight;
    }
}

void UIPanel_RenderOverlays(SDL_Renderer* renderer) {
    const UIPanelState* ui = &g_uiPanel;
    RenderLoadMenu(renderer, ui);
    RenderSaveDialog(renderer, ui);
    RenderRootDialog(renderer, ui);
}
