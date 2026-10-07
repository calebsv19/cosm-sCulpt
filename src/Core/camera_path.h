#pragma once
#include "Core/camera_view.h"
#include <stdint.h>
const LineDrawingScenePath *CameraPath_Selected(const GlobalState *state);
size_t CameraPath_SelectedAnchor(const GlobalState *state);
bool CameraPath_SelectPoint(GlobalState *state,size_t anchor);
bool CameraPath_ViewPoint(GlobalState *state,size_t anchor);
bool CameraPath_Apply(GlobalState *state);
bool CameraPath_Add(GlobalState *state,bool new_path);
bool CameraPath_Move(GlobalState *state,int direction);
bool CameraPath_Remove(GlobalState *state);
bool CameraPath_SetSeconds(GlobalState *state,float seconds);
bool CameraPath_Scrub(GlobalState *state,float seconds);
bool CameraPath_TogglePlay(GlobalState *state);
void CameraPath_NavigationChanged(GlobalState *state);
void CameraPath_Stop(GlobalState *state);
void CameraPath_Reconcile(GlobalState *state);
bool CameraPath_Tick(GlobalState *state,float seconds);
