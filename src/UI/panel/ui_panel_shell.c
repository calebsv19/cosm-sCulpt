#include "UI/ui_panel_shell.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

const char* UIPanel_LeftTabLabel(UIPanelLeftTab tab) {
    switch (tab) {
        case UI_PANEL_LEFT_TAB_SCENE: return "Scene";
        case UI_PANEL_LEFT_TAB_FILE: return "File";
        case UI_PANEL_LEFT_TAB_COUNT:
        default: return "Scene";
    }
}

const char* UIPanel_RightTabLabel(UIPanelRightTab tab) {
    switch (tab) {
        case UI_PANEL_RIGHT_TAB_VIEW: return "View";
        case UI_PANEL_RIGHT_TAB_CREATE: return "Create";
        case UI_PANEL_RIGHT_TAB_OBJECT: return "Object";
        case UI_PANEL_RIGHT_TAB_COUNT:
        default: return "View";
    }
}

void UIPanel_InitShellState(UIPanelState* ui) {
    if (!ui) return;
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_SCENE;
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    memset(ui->leftTabs, 0, sizeof(ui->leftTabs));
    memset(ui->rightTabs, 0, sizeof(ui->rightTabs));
    ui->leftPaneRect = (SDL_Rect){0, 0, 0, 0};
    ui->rightPaneRect = (SDL_Rect){0, 0, 0, 0};
    ui->leftBodyRect = (SDL_Rect){0, 0, 0, 0};
    ui->rightBodyRect = (SDL_Rect){0, 0, 0, 0};

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
    if (leftPaneRect) ui->leftPaneRect = *leftPaneRect;
    if (rightPaneRect) ui->rightPaneRect = *rightPaneRect;

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
            ui->activeLeftTab = (UIPanelLeftTab)i;
            return true;
        }
    }
    for (int i = 0; i < UI_PANEL_RIGHT_TAB_COUNT; ++i) {
        if (UIPanel_PointInRect(mouseX, mouseY, ui->rightTabs[i].bounds)) {
            ui->activeRightTab = (UIPanelRightTab)i;
            return true;
        }
    }
    return false;
}

bool UIPanel_ShouldShowGroup(const UIPanelState* ui, UIPanelGroup group) {
    if (!ui) return true;
    switch (group) {
        case UI_PANEL_GROUP_LEFT_SCENE_BOUNDS:
            return ui->activeLeftTab == UI_PANEL_LEFT_TAB_SCENE;
        case UI_PANEL_GROUP_LEFT_FILE_IO:
        case UI_PANEL_GROUP_LEFT_ROOT_PATHS:
            return ui->activeLeftTab == UI_PANEL_LEFT_TAB_FILE;

        case UI_PANEL_GROUP_RIGHT_VIEW:
        case UI_PANEL_GROUP_RIGHT_MODES:
            return ui->activeRightTab == UI_PANEL_RIGHT_TAB_VIEW;

        case UI_PANEL_GROUP_RIGHT_PRIMITIVES:
        case UI_PANEL_GROUP_RIGHT_CONSTRUCTION:
            return ui->activeRightTab == UI_PANEL_RIGHT_TAB_CREATE;

        case UI_PANEL_GROUP_RIGHT_PRISM:
        case UI_PANEL_GROUP_RIGHT_GIZMO:
        case UI_PANEL_GROUP_RIGHT_TRANSFORM:
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
