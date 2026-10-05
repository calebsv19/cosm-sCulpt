#include "UI/ui_panel_routes.h"
#include "UI/ui_panel_parts.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_measurement.h"
#include "Core/global_state.h"
#include "UI/font_manager.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/ui_panel_summary_surface.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

static bool UIPanel_IsValidLeftTab(UIPanelLeftTab tab) {
    return tab >= UI_PANEL_LEFT_TAB_SCENE && tab < UI_PANEL_LEFT_TAB_COUNT;
}

static bool UIPanel_IsValidRightTab(UIPanelRightTab tab) {
    return tab >= UI_PANEL_RIGHT_TAB_VIEW && tab < UI_PANEL_RIGHT_TAB_COUNT;
}

UIPanelLeftTab UIPanel_GetActiveLeftTab(const UIPanelState* ui) {
    if (!ui) return UI_PANEL_LEFT_TAB_SCENE;
    return ui->activeLeftTab;
}

UIPanelRightTab UIPanel_GetActiveRightTab(const UIPanelState* ui) {
    if (!ui) return UI_PANEL_RIGHT_TAB_VIEW;
    return ui->activeRightTab;
}

void UIPanel_SetActiveLeftTab(UIPanelState* ui, UIPanelLeftTab tab) {
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    if (!ui || !UIPanel_IsValidLeftTab(tab)) return;
    ui->activeLeftTab = tab;
    if (object_mode) {
        ui->objectActiveLeftTab = tab;
    } else {
        ui->sceneActiveLeftTab = tab;
    }
}

void UIPanel_SetActiveRightTab(UIPanelState* ui, UIPanelRightTab tab) {
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    if (!ui || !UIPanel_IsValidRightTab(tab)) return;
    if (ui->activeRightTab != tab) {
        UIPanel_MeasurementStopInput();
        UIPanel_PartsStopInput();
        UIPanel_RoutesStopInput();
        GlobalState* state = Global_Get();
        if (state && state->editor.viewportTool == VIEWPORT_TOOL_LINE) {
            state->editor.viewportTool = VIEWPORT_TOOL_SELECT;
            state->editor.mode = TOOL_IDLE;
        }
        if (state && tab != UI_PANEL_RIGHT_TAB_VIEW &&
            (state->editor.viewportTool == VIEWPORT_TOOL_ORBIT ||
             state->editor.viewportTool == VIEWPORT_TOOL_PAN))
            state->editor.viewportTool = VIEWPORT_TOOL_SELECT;
    }
    ui->measurement.active = tab == UI_PANEL_RIGHT_TAB_MEASURE;
    ui->contextMenuOpen = false;
    ui->activeRightTab = tab;
    if (object_mode) {
        ui->objectActiveRightTab = tab;
    } else {
        ui->sceneActiveRightTab = tab;
    }
}

void UIPanel_SyncWorkspaceTabState(UIPanelState* ui) {
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    if (!ui) return;
    ui->activeLeftTab = object_mode ? ui->objectActiveLeftTab : ui->sceneActiveLeftTab;
    ui->activeRightTab = object_mode ? ui->objectActiveRightTab : ui->sceneActiveRightTab;
    if (!UIPanel_IsValidLeftTab(ui->activeLeftTab)) {
        ui->activeLeftTab = UI_PANEL_LEFT_TAB_SCENE;
    }
    if (!UIPanel_IsValidRightTab(ui->activeRightTab)) {
        ui->activeRightTab = UI_PANEL_RIGHT_TAB_VIEW;
    }
}

void UIPanel_FocusObjectAuthoringTab(UIPanelState* ui) {
    if (!ui) return;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return;
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_CREATE);
}

void UIPanel_FocusObjectInspectorTab(UIPanelState* ui) {
    if (!ui) return;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return;
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_OBJECT);
}

void UIPanel_FocusObjectEditTab(UIPanelState* ui) {
    if (!ui) return;
    if (Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) return;
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_EDIT);
}

const char* UIPanel_LeftTabLabel(UIPanelLeftTab tab) {
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    switch (tab) {
        case UI_PANEL_LEFT_TAB_SCENE: return object_mode ? "Model" : "Scene";
        case UI_PANEL_LEFT_TAB_FILE: return object_mode ? "Assets" : "File";
        case UI_PANEL_LEFT_TAB_COUNT:
        default: return object_mode ? "Model" : "Scene";
    }
}

const char* UIPanel_RightTabLabel(UIPanelRightTab tab) {
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    switch (tab) {
        case UI_PANEL_RIGHT_TAB_VIEW: return "View";
        case UI_PANEL_RIGHT_TAB_CREATE: return object_mode ? "Tools" : "Create";
        case UI_PANEL_RIGHT_TAB_OBJECT: return object_mode ? "Properties" : "Object";
        case UI_PANEL_RIGHT_TAB_EDIT: return "Edit";
        case UI_PANEL_RIGHT_TAB_MEASURE: return "Measure";
        case UI_PANEL_RIGHT_TAB_PARTS: return "Parts";
        case UI_PANEL_RIGHT_TAB_ROUTES: return "Routes";
        case UI_PANEL_RIGHT_TAB_COUNT:
        default: return "View";
    }
}

static void UIPanel_RefreshTabLabels(UIPanelState* ui) {
    if (!ui) return;
    for (int i = 0; i < UI_PANEL_LEFT_TAB_COUNT; ++i) {
        snprintf(ui->leftTabs[i].label,
                 sizeof(ui->leftTabs[i].label),
                 "%s",
                 UIPanel_LeftTabLabel((UIPanelLeftTab)i));
        ui->leftTabs[i].active = ((UIPanelLeftTab)i == ui->activeLeftTab);
    }
    for (int i = 0; i < UI_PANEL_RIGHT_TAB_COUNT; ++i) {
        snprintf(ui->rightTabs[i].label,
                 sizeof(ui->rightTabs[i].label),
                 "%s",
                 UIPanel_RightTabLabel((UIPanelRightTab)i));
        ui->rightTabs[i].active = ((UIPanelRightTab)i == ui->activeRightTab);
    }
}

void UIPanel_InitShellState(UIPanelState* ui) {
    if (!ui) return;
    ui->contextMenuOpen = false;
    ui->sectionHelpOpen = false;
    ui->contextRect = (SDL_Rect){0};
    ui->sceneActiveLeftTab = UI_PANEL_LEFT_TAB_SCENE;
    ui->sceneActiveRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    ui->objectActiveLeftTab = UI_PANEL_LEFT_TAB_SCENE;
    ui->objectActiveRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_SCENE;
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    memset(ui->leftTabs, 0, sizeof(ui->leftTabs));
    memset(ui->rightTabs, 0, sizeof(ui->rightTabs));
    ui->leftPaneRect = (SDL_Rect){0, 0, 0, 0};
    ui->rightPaneRect = (SDL_Rect){0, 0, 0, 0};
    ui->leftBodyRect = (SDL_Rect){0, 0, 0, 0};
    ui->rightBodyRect = (SDL_Rect){0, 0, 0, 0};
    ui->scenePane.summaryRect = (SDL_Rect){0, 0, 0, 0};
    ui->scenePane.listRect = (SDL_Rect){0, 0, 0, 0};
    ui->scenePane.selectionRect = (SDL_Rect){0, 0, 0, 0};
    ui->scenePane.boundsRect = (SDL_Rect){0, 0, 0, 0};
    ui->objectWorkspacePane.summaryRect = (SDL_Rect){0, 0, 0, 0};
    ui->objectWorkspacePane.browserRect = (SDL_Rect){0, 0, 0, 0};
    ui->createPane.operationsRect = (SDL_Rect){0, 0, 0, 0};
    ui->editPane.summaryRect = (SDL_Rect){0, 0, 0, 0};
    ui->editPane.workspaceRect = (SDL_Rect){0, 0, 0, 0};
    ui->editPane.selectionModeRect = (SDL_Rect){0, 0, 0, 0};
    UIPanel_SyncWorkspaceTabState(ui);

    UIPanel_RefreshTabLabels(ui);
}

static void UIPanel_UpdateSideTabs(UIPanelTabButton* tabs,
                                   int tabCount,
                                   int activeIndex,
                                   SDL_Rect paneRect,
                                   const UIPanelLayoutMetrics* metrics,
                                   SDL_Rect* outBodyRect) {
    const int padding = metrics ? metrics->pane_padding_px : 8;
    const int spacing = metrics ? metrics->button_spacing_px : 4;
    const int tabHeight = metrics ? metrics->tab_height_px : 24;
    int contentX = paneRect.x + padding;
    int contentY = paneRect.y + padding;
    int contentW = paneRect.w - (padding * 2);
    if (contentW < 0) contentW = 0;
    /* Hidden legacy identities are reached through the contextual tool selector. */
    int visible = tabCount;
    if (tabCount == UI_PANEL_RIGHT_TAB_COUNT) visible -= 2;
    int total = 0;
    TTF_Font* font = FontManager_GetUIPanelFont();
    for (int i = 0; i < tabCount; ++i) {
        if (tabCount == UI_PANEL_RIGHT_TAB_COUNT &&
            (i == UI_PANEL_RIGHT_TAB_EDIT || i == UI_PANEL_RIGHT_TAB_PARTS)) continue;
        int width = (int)strlen(tabs[i].label) * 8;
        if (font) (void)TTF_SizeUTF8(font, tabs[i].label, &width, NULL);
        total += width + 8;
    }
    int available = contentW - spacing * (visible - 1);
    int cursor = contentX;
    int tabY = contentY;
    for (int i = 0; i < tabCount; ++i) {
        tabs[i].bounds = (SDL_Rect){0};
        if (tabCount == UI_PANEL_RIGHT_TAB_COUNT &&
            (i == UI_PANEL_RIGHT_TAB_EDIT || i == UI_PANEL_RIGHT_TAB_PARTS)) continue;
        int width = (int)strlen(tabs[i].label) * 8;
        if (font) (void)TTF_SizeUTF8(font, tabs[i].label, &width, NULL);
        width += 8;
        if (total > available && total > 0) width = width * available / total;
        tabs[i].bounds = (SDL_Rect){cursor, tabY, width, tabHeight};
        tabs[i].active = i == activeIndex;
        cursor += width + spacing;
    }

    if (outBodyRect) {
        *outBodyRect = (SDL_Rect){
            contentX,
            tabY + tabHeight + spacing + 2,
            contentW,
            paneRect.h - ((tabY + tabHeight + spacing + 2) - paneRect.y) - padding
        };
        if (outBodyRect->w < 0) outBodyRect->w = 0;
        if (outBodyRect->h < 0) outBodyRect->h = 0;
    }
}

void UIPanel_UpdateTabLayout(UIPanelState* ui,
                             const SDL_Rect* leftPaneRect,
                             const SDL_Rect* rightPaneRect,
                             const UIPanelLayoutMetrics* metrics) {
    if (!ui) return;
    UIPanel_SyncWorkspaceTabState(ui);
    if (leftPaneRect) ui->leftPaneRect = *leftPaneRect;
    if (rightPaneRect) ui->rightPaneRect = *rightPaneRect;
    UIPanel_RefreshTabLabels(ui);

    UIPanel_UpdateSideTabs(ui->leftTabs,
                           UI_PANEL_LEFT_TAB_COUNT,
                           (int)ui->activeLeftTab,
                           ui->leftPaneRect,
                           metrics,
                           &ui->leftBodyRect);
    UIPanel_UpdateSideTabs(ui->rightTabs,
                           UI_PANEL_RIGHT_TAB_COUNT,
                           (int)ui->activeRightTab,
                           ui->rightPaneRect,
                           metrics,
                           &ui->rightBodyRect);
    ui->contextRect = (SDL_Rect){0};
    if ((Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_SCENE ||
         ui->activeRightTab == UI_PANEL_RIGHT_TAB_OBJECT || ui->activeRightTab == UI_PANEL_RIGHT_TAB_EDIT) &&
        (ui->activeRightTab == UI_PANEL_RIGHT_TAB_VIEW || ui->activeRightTab == UI_PANEL_RIGHT_TAB_OBJECT ||
         ui->activeRightTab == UI_PANEL_RIGHT_TAB_PARTS || ui->activeRightTab == UI_PANEL_RIGHT_TAB_EDIT)) {
        int h = metrics ? metrics->button_height_px : 24;
        ui->contextRect = (SDL_Rect){ui->rightBodyRect.x, ui->rightBodyRect.y, ui->rightBodyRect.w, h};
        ui->rightBodyRect.y += h + 4;
        ui->rightBodyRect.h -= h + 4;
        if (ui->rightBodyRect.h < 0) ui->rightBodyRect.h = 0;
    }
    if (ui->activeRightTab == UI_PANEL_RIGHT_TAB_PARTS) {
        int primary = ui->parts.mode == 2 ? UI_PANEL_RIGHT_TAB_VIEW : UI_PANEL_RIGHT_TAB_OBJECT;
        ui->rightTabs[primary].active = true;
    } else if (ui->activeRightTab == UI_PANEL_RIGHT_TAB_EDIT) ui->rightTabs[UI_PANEL_RIGHT_TAB_OBJECT].active = true;
}

static bool UIPanel_PointInRect(int x, int y, SDL_Rect rect) {
    return rect.w > 0 && rect.h > 0 && x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

bool UIPanel_HandleTabClick(UIPanelState* ui, int mouseX, int mouseY) {
    if (!ui) return false;

    for (int i = 0; i < UI_PANEL_LEFT_TAB_COUNT; ++i) {
        if (UIPanel_PointInRect(mouseX, mouseY, ui->leftTabs[i].bounds)) {
            UIPanel_SetActiveLeftTab(ui, (UIPanelLeftTab)i);
            return true;
        }
    }
    for (int i = 0; i < UI_PANEL_RIGHT_TAB_COUNT; ++i) {
        if (UIPanel_PointInRect(mouseX, mouseY, ui->rightTabs[i].bounds)) {
            if (i==(int)ui->activeRightTab) return true;
            if (i==UI_PANEL_RIGHT_TAB_MEASURE && !ui->measurement.refs[0].entity_id[0]) UIPanel_BeginMeasurement();
            else {
                UIPanel_SetActiveRightTab(ui, (UIPanelRightTab)i);
                if (i==UI_PANEL_RIGHT_TAB_MEASURE) (void)UIPanel_BeginSelectedMotion();
            }
            return true;
        }
    }
    return false;
}

bool UIPanel_ShouldShowGroup(const UIPanelState* ui, UIPanelGroup group) {
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    if (!ui) return true;
    if (object_mode) {
        switch (group) {
            case UI_PANEL_GROUP_LEFT_FILE_IO:
            case UI_PANEL_GROUP_LEFT_ROOT_PATHS:
                return ui->activeLeftTab == UI_PANEL_LEFT_TAB_FILE;
            case UI_PANEL_GROUP_RIGHT_PRIMITIVES:
            case UI_PANEL_GROUP_RIGHT_OPERATIONS:
            case UI_PANEL_GROUP_RIGHT_CONSTRUCTION:
            case UI_PANEL_GROUP_RIGHT_CREATE_CATEGORIES:
                return ui->activeRightTab == UI_PANEL_RIGHT_TAB_CREATE;
            case UI_PANEL_GROUP_RIGHT_PRISM:
            case UI_PANEL_GROUP_RIGHT_GIZMO:
            case UI_PANEL_GROUP_RIGHT_TRANSFORM:
            case UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS:
                return ui->activeRightTab == UI_PANEL_RIGHT_TAB_OBJECT;
            case UI_PANEL_GROUP_RIGHT_VIEW:
            case UI_PANEL_GROUP_RIGHT_MODES:
                return ui->activeRightTab == UI_PANEL_RIGHT_TAB_VIEW;
            case UI_PANEL_GROUP_RIGHT_EDIT_SELECT:
                return ui->activeRightTab == UI_PANEL_RIGHT_TAB_EDIT;
            case UI_PANEL_GROUP_NONE:
            default:
                return false;
        }
    }
    switch (group) {
        case UI_PANEL_GROUP_LEFT_SCENE_SELECTION:
        case UI_PANEL_GROUP_LEFT_SCENE_BOUNDS:
            return ui->activeLeftTab == UI_PANEL_LEFT_TAB_SCENE;
        case UI_PANEL_GROUP_LEFT_FILE_IO:
        case UI_PANEL_GROUP_LEFT_ROOT_PATHS:
            return ui->activeLeftTab == UI_PANEL_LEFT_TAB_FILE;

        case UI_PANEL_GROUP_RIGHT_VIEW:
        case UI_PANEL_GROUP_RIGHT_MODES:
            return ui->activeRightTab == UI_PANEL_RIGHT_TAB_VIEW;

        case UI_PANEL_GROUP_RIGHT_PRIMITIVES:
        case UI_PANEL_GROUP_RIGHT_OPERATIONS:
        case UI_PANEL_GROUP_RIGHT_CONSTRUCTION:
        case UI_PANEL_GROUP_RIGHT_CREATE_CATEGORIES:
            return ui->activeRightTab == UI_PANEL_RIGHT_TAB_CREATE;

        case UI_PANEL_GROUP_RIGHT_PRISM:
        case UI_PANEL_GROUP_RIGHT_GIZMO:
        case UI_PANEL_GROUP_RIGHT_TRANSFORM:
        case UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS:
            return ui->activeRightTab == UI_PANEL_RIGHT_TAB_OBJECT;

        case UI_PANEL_GROUP_RIGHT_EDIT_SELECT:
            return false;

        case UI_PANEL_GROUP_NONE:
        default:
            return false;
    }
}

bool UIPanel_ShouldRenderRootSummary(const UIPanelState* ui) {
    return ui && ui->activeLeftTab == UI_PANEL_LEFT_TAB_FILE;
}

bool UIPanel_ShouldRenderObjectSummary(const UIPanelState* ui) {
    return ui && ui->activeRightTab == UI_PANEL_RIGHT_TAB_OBJECT;
}


static bool context_is_view(const UIPanelState* ui) {
    return ui->activeRightTab == UI_PANEL_RIGHT_TAB_VIEW ||
           (ui->activeRightTab == UI_PANEL_RIGHT_TAB_PARTS && ui->parts.mode == 2);
}
static const char* context_label(const UIPanelState* ui) {
    if (ui->activeRightTab == UI_PANEL_RIGHT_TAB_VIEW) return "Camera / section";
    if (ui->activeRightTab == UI_PANEL_RIGHT_TAB_EDIT) return "Edit tools";
    if (ui->activeRightTab == UI_PANEL_RIGHT_TAB_PARTS)
        return (const char*[]){"Details", "Assemblies", "Visibility", "Connections", "Volumes", "Checks"}
            [ui->parts.mode >= 0 && ui->parts.mode < 6 ? ui->parts.mode : 0];
    return "Geometry";
}
static int context_count(const UIPanelState* ui) {
    return Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT ? 2 : context_is_view(ui) ? 2 : 6;
}

void UIPanel_RenderContext(SDL_Renderer* renderer, const UIPanelState* ui) {
    if (!renderer || !ui || ui->contextRect.w <= 0) return;
    UIPanelVisualPalette p = {0};
    (void)UIPanelVisual_ResolvePalette(&p);
    TTF_Font* font = FontManager_GetUIPanelFont();
    int count = ui->contextMenuOpen ? context_count(ui) : 0;
    for (int i = -1; i < count; ++i) {
        SDL_Rect r = ui->contextRect;
        r.y += (i + 1) * (r.h + 2);
        const char* label = i < 0 ? context_label(ui) : Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT
            ? (const char*[]){"Geometry", "Edit tools"}[i] : context_is_view(ui)
            ? (const char*[]){"Camera / section", "Visibility / saved views"}[i]
            : (const char*[]){"Geometry", "Details / tags", "Assemblies", "Connections", "Volumes", "Checks", "Edit tools"}[i];
        UIPanelVisual_DrawFrame(renderer, r, p.button_fill, p.button_border, 0);
        if (font) UIPanelSummary_DrawTextClipped(renderer, font, label, r.x + 5, r.y + 2, r.w - 24, r.h - 3, p.text_primary);
        if (i < 0 && font) UIPanelSummary_DrawTextClipped(renderer, font, "v", r.x + r.w - 15, r.y + 2, 12, r.h - 3, p.text_muted);
    }
}
bool UIPanel_ContextEvent(const SDL_Event* event) {
    UIPanelState* ui = UIPanel_Get();
    if (!event || !ui) return false;
    if (UIPanel_IsSaveDialogActive() || UIPanel_IsRootDialogActive() || UIPanel_IsPrismDimensionDialogActive() ||
        UIPanel_IsSceneBoundsDialogActive() || UIPanel_IsConstructionPlaneDialogActive() ||
        UIPanel_IsObjectTransformDialogActive() || UIPanel_IsScenePropertyDialogActive()) {
        ui->contextMenuOpen = false;
        return false;
    }
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) ui->contextMenuOpen = false;
    if (ui->contextMenuOpen && event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
        ui->contextMenuOpen = false;
        return true;
    }
    if (event->type != SDL_MOUSEBUTTONDOWN || event->button.button != SDL_BUTTON_LEFT)
        return ui->contextMenuOpen && (event->type == SDL_KEYDOWN || event->type == SDL_TEXTINPUT ||
            event->type == SDL_MOUSEWHEEL || event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEBUTTONUP);
    int x = event->button.x, y = event->button.y;
    if (UIPanel_PointInRect(x, y, ui->contextRect)) {
        ui->contextMenuOpen = !ui->contextMenuOpen;
        return true;
    }
    if (!ui->contextMenuOpen) return false;
    bool view = context_is_view(ui);
    int count = context_count(ui);
    ui->contextMenuOpen = false;
    for (int i = 0; i < count; ++i) {
        SDL_Rect r = ui->contextRect;
        r.y += (i + 1) * (r.h + 2);
        if (!UIPanel_PointInRect(x, y, r)) continue;
        if (i == 0) UIPanel_SetActiveRightTab(ui, view ? UI_PANEL_RIGHT_TAB_VIEW : UI_PANEL_RIGHT_TAB_OBJECT);
        else if (Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT && i == 1) UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_EDIT);
        else {
            UIPanel_PartsEnterMode(view ? 2 : (const int[]){0, 0, 1, 3, 4, 5}[i]);
        }
        ui->rightScroll[ui->activeRightTab].scrollOffsetPx = 0;
        Global_FlagHitboxesDirty();
        UIPanel_OnWindowResized(Global_Get()->screenWidth, Global_Get()->screenHeight);
        return true;
    }
    return true; /* Dismissal consumes the click instead of editing an obscured object. */
}
