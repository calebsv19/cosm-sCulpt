#include "UI/ui_panel.h"
#include "UI/info_overlay.h"
#include <string.h>



static UIPanelState g_uiPanel;  // Internal static state

UIPanelState* UIPanel_Get(void) {
    return &g_uiPanel;
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

void UIPanel_Init(int screenW, int screenH) {
    (void)screenH;
    g_uiPanel.count = 0;

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
}

const UIButton* UIPanel_GetButtons(UIPanelState* ui, int* outCount) {
    if (outCount) *outCount = ui->count;
    return ui->buttons;
}
