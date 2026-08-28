#pragma once

#include "UI/ui_panel.h"

// Return the active right-tab scroll offset.
float UIPanel_RightScrollOffset(const UIPanelState* ui);

// Store content height and clamp the active tab offset after reflow.
void UIPanel_RightScrollSetContentHeight(UIPanelState* ui, float content_height_px);

// Consume wheel input only while the pointer is inside a scrollable right body.
bool UIPanel_RightScrollHandleWheel(int mouse_x, int mouse_y, float wheel_delta);

// Handle scrollbar press, track jump, drag, and release.
bool UIPanel_RightScrollHandleClick(int mouse_x, int mouse_y);
void UIPanel_RightScrollHandleMouseMotion(int mouse_x, int mouse_y);
void UIPanel_RightScrollHandleMouseUp(void);

// Draw the active right-pane scrollbar when content exceeds the viewport.
void UIPanel_RightScrollRender(const UIPanelState* ui, SDL_Renderer* renderer);
