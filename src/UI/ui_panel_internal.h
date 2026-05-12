#pragma once

#include "UI/ui_panel.h"
#include "Core/global_state.h"

const char* UIPanel_ViewPlaneAxisLabel(ViewPlaneAxis axis);
const char* UIPanel_ViewPlaneCoordinateLabel(ViewPlaneAxis axis);
ViewPlane UIPanel_CurrentConstructionViewPlane(const GlobalState* state);
const char* UIPanel_PrismDimensionTargetLabel(UIPrismDimensionDialogTarget target);
const char* UIPanel_SceneBoundsTargetLabel(UISceneBoundsDialogTarget target);
const char* UIPanel_ObjectTransformTargetLabel(UIObjectTransformDialogTarget target);
SDL_Rect UIPanel_GetLoadMenuRect(const UIPanelState* ui);
SDL_Rect UIPanel_GetLoadMenuPaneClipRect(const UIPanelState* ui);

void UIPanel_CloseRootDialog(UIPanelState* ui);
void UIPanel_CloseSaveDialog(UIPanelState* ui);
void UIPanel_ClosePrismDimensionDialog(UIPanelState* ui);
void UIPanel_CloseSceneBoundsDialog(UIPanelState* ui);
void UIPanel_CloseConstructionPlaneDialog(UIPanelState* ui);
void UIPanel_CloseObjectTransformDialog(UIPanelState* ui);

bool UIPanel_BeginPrismDimensionDialog(UIPrismDimensionDialogTarget target);
bool UIPanel_BeginSceneBoundsDialog(UISceneBoundsDialogTarget target);
bool UIPanel_BeginConstructionPlaneDialog(UIConstructionPlaneDialogTarget target);
bool UIPanel_BeginObjectTransformDialog(UIObjectTransformDialogTarget target);

bool UIPanel_ApplyPrismDimensionDialog(UIPanelState* ui);
bool UIPanel_ApplySceneBoundsDialog(UIPanelState* ui);
bool UIPanel_ApplyConstructionPlaneDialog(UIPanelState* ui);
bool UIPanel_ApplyObjectTransformDialog(UIPanelState* ui);

void UIPanel_BeginRootDialog(UIRootDialogTarget target);
bool UIPanel_ApplyRootDialog(UIPanelState* ui);
bool UIPanel_PerformSave(UIPanelState* ui);
bool UIPanel_LoadLayoutFromPath(const char* path);
bool UIPanel_LoadSceneFromPath(const char* path);
bool UIPanel_LoadJsonFromFolderSelection(const char* selected_folder, bool persist_root);
bool UIPanel_LoadSceneFromFolderSelection(const char* selected_folder, bool persist_root);
