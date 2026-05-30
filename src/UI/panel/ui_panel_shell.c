#include "UI/ui_panel_shell.h"
#include "Core/global_state.h"

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

const char* UIPanel_LeftTabLabel(UIPanelLeftTab tab) {
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    switch (tab) {
        case UI_PANEL_LEFT_TAB_SCENE: return object_mode ? "Object" : "Scene";
        case UI_PANEL_LEFT_TAB_FILE: return "File";
        case UI_PANEL_LEFT_TAB_COUNT:
        default: return object_mode ? "Object" : "Scene";
    }
}

const char* UIPanel_RightTabLabel(UIPanelRightTab tab) {
    const bool object_mode = Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    switch (tab) {
        case UI_PANEL_RIGHT_TAB_VIEW: return "View";
        case UI_PANEL_RIGHT_TAB_CREATE: return object_mode ? "Shape" : "Create";
        case UI_PANEL_RIGHT_TAB_OBJECT: return object_mode ? "Asset" : "Object";
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
    int tabW = 0;
    int gapTotal = 0;

    if (contentW < 0) contentW = 0;
    if (tabCount > 1) gapTotal = spacing * (tabCount - 1);
    if (tabCount > 0) {
        tabW = (contentW - gapTotal) / tabCount;
    }
    if (tabW < 24) tabW = 24;

    for (int i = 0; i < tabCount; ++i) {
        tabs[i].bounds = (SDL_Rect){
            contentX + i * (tabW + spacing),
            contentY,
            tabW,
            tabHeight
        };
        tabs[i].active = (i == activeIndex);
    }

    if (outBodyRect) {
        *outBodyRect = (SDL_Rect){
            contentX,
            contentY + tabHeight + spacing + 2,
            contentW,
            paneRect.h - ((contentY + tabHeight + spacing + 2) - paneRect.y) - padding
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
}

static bool UIPanel_PointInRect(int x, int y, SDL_Rect rect) {
    return x >= rect.x && x <= (rect.x + rect.w) &&
           y >= rect.y && y <= (rect.y + rect.h);
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
            UIPanel_SetActiveRightTab(ui, (UIPanelRightTab)i);
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
                return ui->activeRightTab == UI_PANEL_RIGHT_TAB_CREATE;
            case UI_PANEL_GROUP_RIGHT_PRISM:
            case UI_PANEL_GROUP_RIGHT_GIZMO:
            case UI_PANEL_GROUP_RIGHT_TRANSFORM:
            case UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS:
                return ui->activeRightTab == UI_PANEL_RIGHT_TAB_OBJECT;
            case UI_PANEL_GROUP_RIGHT_VIEW:
            case UI_PANEL_GROUP_RIGHT_MODES:
                return ui->activeRightTab == UI_PANEL_RIGHT_TAB_VIEW;
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
            return ui->activeRightTab == UI_PANEL_RIGHT_TAB_CREATE;

        case UI_PANEL_GROUP_RIGHT_PRISM:
        case UI_PANEL_GROUP_RIGHT_GIZMO:
        case UI_PANEL_GROUP_RIGHT_TRANSFORM:
        case UI_PANEL_GROUP_RIGHT_OBJECT_ACTIONS:
            return ui->activeRightTab == UI_PANEL_RIGHT_TAB_OBJECT;

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
