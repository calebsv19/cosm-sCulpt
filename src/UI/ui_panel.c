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

static void UIPanel_CloseSaveDialog(UIPanelState* ui) {
    if (!ui->saveDialog.active) return;
    ui->saveDialog.active = false;
    ui->saveDialog.buffer[0] = '\0';
    ui->saveDialog.length = 0;
    ui->saveDialog.cursor = 0;
    if (SDL_IsTextInputActive()) SDL_StopTextInput();
}

void UIPanel_RefreshConfigList(void) {
    UIPanelState* ui = &g_uiPanel;
    ui->loadMenu.count = 0;
    ui->loadMenu.hoverIndex = -1;

    DIR* dir = opendir("config");
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && ui->loadMenu.count < MAX_CONFIG_FILES) {
        if (entry->d_name[0] == '.') continue;
        const char* name = entry->d_name;
        size_t len = strlen(name);
        if (len < 5) continue;
        if (strcasecmp(name + len - 5, ".json") != 0) continue;

        strncpy(ui->loadMenu.entries[ui->loadMenu.count], name, sizeof(ui->loadMenu.entries[0]) - 1);
        ui->loadMenu.entries[ui->loadMenu.count][sizeof(ui->loadMenu.entries[0]) - 1] = '\0';
        ui->loadMenu.count++;
    }
    closedir(dir);

    for (int i = 0; i < ui->loadMenu.count - 1; ++i) {
        for (int j = i + 1; j < ui->loadMenu.count; ++j) {
            if (strcasecmp(ui->loadMenu.entries[j], ui->loadMenu.entries[i]) < 0) {
                char tmp[128];
                snprintf(tmp, sizeof(tmp), "%s", ui->loadMenu.entries[i]);
                snprintf(ui->loadMenu.entries[i], sizeof(ui->loadMenu.entries[i]), "%s", ui->loadMenu.entries[j]);
                snprintf(ui->loadMenu.entries[j], sizeof(ui->loadMenu.entries[j]), "%s", tmp);
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
    snprintf(path, sizeof(path), "config/%s", filename);

    GlobalState* state = Global_Get();
    Layout_CompactDeletedElements(&state->layout);
    if (!Layout_SaveToFile(&state->layout, path)) {
        SDL_Log("[UI] Failed to save layout to %s", path);
        return false;
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

    int padding = 10;
    int btnW = 140;
    int btnH = 28;
    int spacing = 6;

    int topOffset = INFO_OVERLAY_HEIGHT + padding;

    int xL = padding;
    int yL = topOffset;
    AddButton(&g_uiPanel, "Save JSON", xL, yL, btnW, btnH, UI_PANEL_LEFT, 0);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Load JSON", xL, yL, btnW, btnH, UI_PANEL_LEFT, 1);
    yL += btnH + spacing;
    AddButton(&g_uiPanel, "Export Shape", xL, yL, btnW, btnH, UI_PANEL_LEFT, 2);

    int xR = screenW - btnW - padding;
    int yR = topOffset;
    AddButton(&g_uiPanel, "Reset Origin (O)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, 10);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Zoom In (+)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, 11);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Zoom Out (-)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, 12);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Toggle Delete (D)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, 13);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Pin Anchor (P)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, 14);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Link Handles (L)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, 15);
    yR += btnH + spacing;
    AddButton(&g_uiPanel, "Mode: 3D (M)", xR, yR, btnW, btnH, UI_PANEL_RIGHT, 16);

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
    if (!SDL_IsTextInputActive()) SDL_StartTextInput();
}

bool UIPanel_IsSaveDialogActive(void) {
    return g_uiPanel.saveDialog.active;
}

bool UIPanel_IsCapturingKeyboard(void) {
    return UIPanel_IsSaveDialogActive();
}

bool UIPanel_HandleTextInput(const char* text) {
    UIPanelState* ui = &g_uiPanel;
    if (!ui->saveDialog.active || !text) return false;

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

bool UIPanel_HandleKeyEvent(const SDL_Event* event) {
    UIPanelState* ui = &g_uiPanel;
    if (!ui->saveDialog.active) return false;
    if (event->type != SDL_KEYDOWN) return false;

    SDL_Keycode key = event->key.keysym.sym;
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

void UIPanel_ToggleLoadMenu(void) {
    UIPanelState* ui = &g_uiPanel;
    ui->loadMenu.open = !ui->loadMenu.open;
    if (ui->loadMenu.open) {
        UIPanel_RefreshConfigList();
    }
    ui->loadMenu.hoverIndex = -1;
    UIPanel_CloseSaveDialog(ui);
}

bool UIPanel_IsLoadMenuOpen(void) {
    return g_uiPanel.loadMenu.open;
}

static SDL_Rect UIPanel_GetLoadMenuRect(const UIPanelState* ui) {
    const UIButton* loadBtn = UIPanel_FindButtonById((UIPanelState*)ui, 1);
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
    snprintf(path, sizeof(path), "config/%s", ui->loadMenu.entries[index]);

    GlobalState* state = Global_Get();
    Editor_ClearHistory(&state->editor);

    if (Layout_LoadFromFile(&state->layout, path)) {
        SDL_Log("[UI] Loaded layout %s", path);
        Global_OnLayoutLoaded(path);
        state->editor.selectedAnchorIndex = -1;
        state->editor.selectedWallIndex = -1;
        state->editor.hoveredAnchorIndex = -1;
        state->editor.hoveredWallIndex = -1;
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
    DrawText(renderer, buffer, inputRect.x + 8, inputRect.y + 6,
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
        DrawText(renderer, ui->loadMenu.entries[i], rect.x + 10, y + 2,
                 has_shared_palette ? palette.text_primary : (SDL_Color){230, 230, 235, 255});
        y += itemHeight;
    }
}

void UIPanel_RenderOverlays(SDL_Renderer* renderer) {
    const UIPanelState* ui = &g_uiPanel;
    RenderLoadMenu(renderer, ui);
    RenderSaveDialog(renderer, ui);
}
