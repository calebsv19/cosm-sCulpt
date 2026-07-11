#pragma once

#include "UI/ui_panel.h"

bool UIPanel_SceneAuthoringInspectorHasSelection(void);
int UIPanel_SceneAuthoringInspectorReservedHeight(const UIPanelState* ui);
int UIPanel_SceneAuthoringInspectorDetailsHeight(const UIPanelState* ui);
void Render_UIPanelSceneAuthoringInspector(const UIPanelState* ui, SDL_Renderer* renderer);

bool UIPanel_ToggleSceneAuthoringEditMode(void);
bool UIPanel_ToggleSelectedSceneAuthoringLightEnabled(void);
bool UIPanel_CycleSelectedSceneAuthoringLightKind(void);
bool UIPanel_CycleSelectedSceneAuthoringLightPath(void);
bool UIPanel_CycleSelectedSceneAuthoringCameraPathKind(void);
bool UIPanel_CycleSelectedSceneAuthoringMaterialColor(void);
