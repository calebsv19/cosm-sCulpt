#pragma once

#include <stdbool.h>

#include "UI/ui_panel.h"

bool UIPanel_HandleSceneListClick(int mouseX, int mouseY);
bool UIPanel_HandleSceneListWheel(int mouseX, int mouseY, float wheel_delta);
void UIPanel_HandleSceneListMouseUp(void);
void UIPanel_HandleSceneListMouseMotion(int mouseX, int mouseY);
void UIPanel_SceneListClearSelection(void);
bool UIPanel_SceneListDeleteSelectedObject(void);
void Render_UIPanelSceneList(const UIPanelState* ui, SDL_Renderer* renderer);
