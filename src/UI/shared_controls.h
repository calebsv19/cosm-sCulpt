#pragma once
#include <SDL2/SDL.h>
#include "kit_ui_surface.h"

/* Host-owned snapshots: keys name actions; drawing and action dispatch stay local. */
void LDControls_Add(KitUiSurface* surface, unsigned domain, uint64_t value,
                    SDL_Rect bounds, const SDL_Rect* clip, bool enabled);
bool LDControls_Route(KitUiSurface* surface, const SDL_Event* event, SDL_Point* activation);
void LDControls_Draw(SDL_Renderer* renderer, const KitUiSurface* surface);
void LDControls_Cancel(KitUiSurface* surface);
void LDControls_CancelAll(void);
KitUiSurface* LDControls_Menu(void);
bool LDControls_EditorEvent(const SDL_Event* event);
void LDControls_EditorDraw(SDL_Renderer* renderer);
void LineDrawingEditorTopbar_Collect(KitUiSurface* surface);
void UIPanel_CollectShellControls(KitUiSurface* surface);
