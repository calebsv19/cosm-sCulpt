#include "UI/ui_panel_internal.h"

#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Layout/layout_json.h"
#include "UI/info_overlay.h"
#include "UI/font_manager.h"

#include <dirent.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

enum {
    UI_LOAD_MENU_PADDING_PX = 8,
    UI_LOAD_MENU_HEADER_H = 32,
    UI_LOAD_MENU_FOOTER_H = 24,
    UI_LOAD_MENU_ROW_H = 26,
    UI_LOAD_MENU_SCROLLBAR_W = 10,
    UI_LOAD_MENU_GUTTER_PX = 8,
    UI_LOAD_MENU_SECTION_GAP_PX = 8,
    UI_LOAD_MENU_MAX_H = 360,
    UI_LOAD_MENU_MIN_W = 320,
    UI_LOAD_MENU_DEFAULT_W = 360
};

static const char* k_scene_authoring_filename = "scene_authoring.json";
static const char* k_scene_runtime_filename = "scene_runtime.json";

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

static bool UIPanel_FindButtonRect(const UIPanelState* ui, int button_id, SDL_Rect* out_rect) {
    if (!ui || !out_rect) return false;
    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == button_id) {
            *out_rect = ui->buttons[i].bounds;
            return true;
        }
    }
    *out_rect = (SDL_Rect){0, 0, 0, 0};
    return false;
}

static SDL_Rect UIPanel_CoreRectToSDLRect(CorePaneRect rect) {
    SDL_Rect out = {0, 0, 0, 0};
    int x0 = (int)floorf(rect.x);
    int y0 = (int)floorf(rect.y);
    int x1 = (int)ceilf(rect.x + rect.width);
    int y1 = (int)ceilf(rect.y + rect.height);
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    out.x = x0;
    out.y = y0;
    out.w = x1 - x0;
    out.h = y1 - y0;
    return out;
}

static bool UIPanel_QueryLeftPaneRect(SDL_Rect* out_rect) {
    const LineDrawingPaneHost* pane_host = NULL;
    CorePaneRect pane_rect = {0};
    if (!out_rect) return false;
    *out_rect = (SDL_Rect){0, 0, 0, 0};

    pane_host = Global_GetPaneHostConst();
    if (!pane_host || !pane_host->initialized) return false;
    if (!LineDrawingPaneHost_GetRectForRole(pane_host,
                                            LINE_DRAWING_PANE_ROLE_LEFT_CONTROLS,
                                            &pane_rect)) {
        return false;
    }

    *out_rect = UIPanel_CoreRectToSDLRect(pane_rect);
    return out_rect->w > 0 && out_rect->h > 0;
}

static int UIPanel_LoadMenuFontHeightPx(TTF_Font* font) {
    int height = 14;
    if (font && TTF_FontHeight(font) > 0) {
        height = TTF_FontHeight(font);
    }
    return height;
}

static int UIPanel_LoadMenuLineGapPx(TTF_Font* font) {
    int gap = UIPanel_LoadMenuFontHeightPx(font) / 3;
    if (gap < 4) gap = 4;
    return gap;
}

static bool UIPanel_GetLeftButtonLaneBounds(const UIPanelState* ui, SDL_Rect* out_rect) {
    int min_x = INT_MAX;
    int min_y = INT_MAX;
    int max_x = 0;
    int max_y = 0;
    if (!ui || !out_rect) return false;
    *out_rect = (SDL_Rect){0, 0, 0, 0};

    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->side != UI_PANEL_LEFT) continue;
        if (btn->bounds.x < min_x) min_x = btn->bounds.x;
        if (btn->bounds.y < min_y) min_y = btn->bounds.y;
        if (btn->bounds.x + btn->bounds.w > max_x) max_x = btn->bounds.x + btn->bounds.w;
        if (btn->bounds.y + btn->bounds.h > max_y) max_y = btn->bounds.y + btn->bounds.h;
    }

    if (min_x == INT_MAX || min_y == INT_MAX || max_x <= min_x || max_y <= min_y) return false;
    *out_rect = (SDL_Rect){ min_x, min_y, max_x - min_x, max_y - min_y };
    return true;
}

static bool UIPanel_GetLeftRootSummaryRect(const UIPanelState* ui, SDL_Rect* out_rect) {
    SDL_Rect lane = {0, 0, 0, 0};
    TTF_Font* font = FontManager_Get(FONT_DEFAULT);
    int font_h = 14;
    int line_gap = 4;
    int panel_pad = 8;
    int panel_h = 0;
    if (!out_rect) return false;
    *out_rect = (SDL_Rect){0, 0, 0, 0};
    if (!UIPanel_GetLeftButtonLaneBounds(ui, &lane)) return false;

    font_h = UIPanel_LoadMenuFontHeightPx(font);
    line_gap = UIPanel_LoadMenuLineGapPx(font);
    panel_pad = font_h / 2;
    if (panel_pad < 8) panel_pad = 8;
    panel_h = (panel_pad * 2) + (font_h * 4) + (line_gap * 3);

    *out_rect = (SDL_Rect){
        lane.x,
        lane.y + lane.h + UI_LOAD_MENU_SECTION_GAP_PX,
        lane.w,
        panel_h
    };
    return true;
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

static void UIPanel_LoadMenuClose(UIPanelState* ui) {
    if (!ui) return;
    ui->loadMenu.open = false;
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.scrollbarDragging = false;
}

static bool UIPanel_OpenLoadMenuForRoot(UILoadMenuMode mode,
                                        int anchor_button_id,
                                        const char* selected_folder,
                                        bool persist_root) {
    UIPanelState* ui = UIPanel_Get();
    char previous_root[MAX_CONFIG_PATH];
    const char* current_root = Global_GetInputRoot();
    UILoadMenuMode previous_mode = UI_LOAD_MENU_MODE_NONE;
    int previous_anchor = UI_BTN_LOAD_JSON;

    if (!ui || !selected_folder || !selected_folder[0]) return false;

    previous_root[0] = '\0';
    if (current_root && current_root[0]) {
        snprintf(previous_root, sizeof(previous_root), "%s", current_root);
    }
    previous_mode = ui->loadMenu.mode;
    previous_anchor = ui->loadMenu.anchorButtonId;

    if (!Global_SetInputRoot(selected_folder, persist_root)) {
        return false;
    }

    ui->loadMenu.mode = mode;
    ui->loadMenu.anchorButtonId = anchor_button_id;
    snprintf(ui->loadMenu.rootPath, sizeof(ui->loadMenu.rootPath), "%s", selected_folder);
    UIPanel_RefreshConfigList();
    if (ui->loadMenu.count <= 0) {
        if (previous_root[0]) {
            (void)Global_SetInputRoot(previous_root, persist_root);
        }
        ui->loadMenu.mode = previous_mode;
        ui->loadMenu.anchorButtonId = previous_anchor;
        if (previous_root[0]) {
            snprintf(ui->loadMenu.rootPath, sizeof(ui->loadMenu.rootPath), "%s", previous_root);
        } else {
            ui->loadMenu.rootPath[0] = '\0';
        }
        UIPanel_RefreshConfigList();
        UIPanel_LoadMenuClose(ui);
        return false;
    }

    ui->loadMenu.open = true;
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
    return true;
}

bool UIPanel_LoadJsonFromFolderSelection(const char* selected_folder, bool persist_root) {
    return UIPanel_OpenLoadMenuForRoot(UI_LOAD_MENU_MODE_JSON,
                                       UI_BTN_LOAD_JSON,
                                       selected_folder,
                                       persist_root);
}

bool UIPanel_LoadSceneFromFolderSelection(const char* selected_folder, bool persist_root) {
    return UIPanel_OpenLoadMenuForRoot(UI_LOAD_MENU_MODE_SCENE,
                                       UI_BTN_LOAD_SCENE,
                                       selected_folder,
                                       persist_root);
}

void UIPanel_RefreshConfigList(void) {
    UIPanelState* ui = UIPanel_Get();
    const char* input_root = Global_GetInputRoot();
    if (!ui) return;

    ui->loadMenu.count = 0;
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.activeIndex = -1;
    memset(ui->loadMenu.entries, 0, sizeof(ui->loadMenu.entries));
    memset(ui->loadMenu.entryPaths, 0, sizeof(ui->loadMenu.entryPaths));
    if (ui->loadMenu.rootPath[0] == '\0' && input_root && input_root[0]) {
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
    ui->loadMenu.scrollOffsetPx = 0.0f;
    UIPanel_LoadMenuClampScroll(ui);
}

void UIPanel_ToggleLoadMenu(void) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui) return;
    if (ui->loadMenu.open) {
        UIPanel_LoadMenuClose(ui);
        return;
    }
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_NONE) {
        return;
    }
    UIPanel_RefreshConfigList();
    if (ui->loadMenu.count <= 0) {
        ui->loadMenu.open = false;
        return;
    }
    ui->loadMenu.open = true;
    ui->loadMenu.hoverIndex = -1;
    ui->loadMenu.scrollOffsetPx = 0.0f;
    UIPanel_LoadMenuClampScroll(ui);
}

bool UIPanel_IsLoadMenuOpen(void) {
    return UIPanel_Get()->loadMenu.open;
}

SDL_Rect UIPanel_GetLoadMenuRect(const UIPanelState* ui) {
    SDL_Rect button_rect = {12, InfoOverlay_HeightPx() + 44, 168, 26};
    SDL_Rect pane_rect = {0, 0, 0, 0};
    SDL_Rect root_summary_rect = {0, 0, 0, 0};
    UIPanelLayoutMetrics metrics = {0};
    int x = 12;
    int y = InfoOverlay_HeightPx() + 44;
    int w = UI_LOAD_MENU_DEFAULT_W;
    int h = UI_LOAD_MENU_HEADER_H + UI_LOAD_MENU_FOOTER_H + UI_LOAD_MENU_ROW_H;
    int screen_h = Global_GetScreenHeight();
    int available_h = 0;
    int max_list_h = 0;
    int pane_inset = 10;
    float content_h = 0.0f;

    UIPanel_GetLayoutMetrics(&metrics);
    pane_inset = metrics.pane_padding_px;
    if (pane_inset < 6) pane_inset = 6;

    if (ui && UIPanel_QueryLeftPaneRect(&pane_rect)) {
        x = pane_rect.x + pane_inset;
        y = pane_rect.y + pane_inset;
        w = pane_rect.w - (pane_inset * 2);
        available_h = pane_rect.h - (pane_inset * 2);

        if (UIPanel_GetLeftRootSummaryRect(ui, &root_summary_rect)) {
            y = root_summary_rect.y + root_summary_rect.h + UI_LOAD_MENU_SECTION_GAP_PX;
            available_h = (pane_rect.y + pane_rect.h - pane_inset) - y;
        }

        if (w < 24) w = 24;
        if (available_h < UI_LOAD_MENU_HEADER_H + UI_LOAD_MENU_FOOTER_H + UI_LOAD_MENU_ROW_H) {
            available_h = UI_LOAD_MENU_HEADER_H + UI_LOAD_MENU_FOOTER_H + UI_LOAD_MENU_ROW_H;
        }
    } else if (ui && UIPanel_FindButtonRect(ui, ui->loadMenu.anchorButtonId, &button_rect)) {
        x = button_rect.x;
        y = button_rect.y + button_rect.h + 4;
        if (button_rect.w > w) w = button_rect.w;
        if (w < UI_LOAD_MENU_MIN_W) w = UI_LOAD_MENU_MIN_W;
        available_h = screen_h - y - 16;
    }

    if (available_h <= 0) {
        available_h = screen_h - y - 16;
        if (available_h < UI_LOAD_MENU_HEADER_H + UI_LOAD_MENU_FOOTER_H + UI_LOAD_MENU_ROW_H) {
            available_h = UI_LOAD_MENU_HEADER_H + UI_LOAD_MENU_FOOTER_H + UI_LOAD_MENU_ROW_H;
        }
    }

    content_h = UIPanel_LoadMenuContentHeight(ui);
    max_list_h = (int)lroundf(content_h);
    if (max_list_h < UI_LOAD_MENU_ROW_H) max_list_h = UI_LOAD_MENU_ROW_H;
    if (max_list_h > UI_LOAD_MENU_MAX_H) max_list_h = UI_LOAD_MENU_MAX_H;
    h = UI_LOAD_MENU_HEADER_H + UI_LOAD_MENU_FOOTER_H + max_list_h;
    if (h > available_h) h = available_h;

    return (SDL_Rect){ x, y, w, h };
}

SDL_Rect UIPanel_GetLoadMenuPaneClipRect(const UIPanelState* ui) {
    SDL_Rect pane_rect = {0, 0, 0, 0};
    SDL_Rect menu_rect = UIPanel_GetLoadMenuRect(ui);
    (void)ui;
    if (UIPanel_QueryLeftPaneRect(&pane_rect)) {
        return pane_rect;
    }
    return menu_rect;
}

bool UIPanel_LoadLayoutFromPath(const char* path) {
    GlobalState* state = Global_Get();
    if (!state || !path || path[0] == '\0') return false;

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
        return true;
    }

    SDL_Log("[UI] Failed to load layout %s", path);
    return false;
}

static bool UIPanel_LoadEntryByIndex(int index) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui || index < 0 || index >= ui->loadMenu.count) return false;
    if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON) {
        return UIPanel_LoadLayoutFromPath(ui->loadMenu.entryPaths[index]);
    } else if (ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE) {
        return UIPanel_LoadSceneFromPath(ui->loadMenu.entryPaths[index]);
    }
    return false;
}

bool UIPanel_HandleLoadMenuWheel(int mouseX, int mouseY, float wheel_delta) {
    UIPanelState* ui = UIPanel_Get();
    SDL_Rect rect = {0, 0, 0, 0};
    if (!ui || !ui->loadMenu.open) return false;
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
    if (!ui || !ui->loadMenu.open) return false;

    rect = UIPanel_GetLoadMenuRect(ui);
    if (!SDL_PointInRect(&(SDL_Point){ mouseX, mouseY }, &rect)) {
        UIPanel_LoadMenuClose(ui);
        return false;
    }

    if (ui->loadMenu.count <= 0) {
        UIPanel_LoadMenuClose(ui);
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

    if (!ui || !ui->loadMenu.open) {
        if (ui) ui->loadMenu.hoverIndex = -1;
        return;
    }

    ui->loadMenu.hoverIndex = UIPanel_LoadMenuIndexAtPoint(ui, mouseX, mouseY);
}
