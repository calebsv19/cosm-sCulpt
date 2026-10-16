#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
/* Header and popup share these measured rectangles with input and native proofs. */
enum { LD_VIEW_CAMERA=1, LD_VIEW_VISIBILITY, LD_VIEW_MORE, LD_VIEW_APPEARANCE,
 LD_VIEW_EXPLORE=10, LD_VIEW_SAVED, LD_VIEW_EDITOR, LD_VIEW_PLANE,
 LD_VIEW_ORIENTATION, LD_VIEW_SPACE, LD_VIEW_BOUNDS, LD_VIEW_GIZMO,
 LD_VIEW_WIRE, LD_VIEW_SOLID, LD_VIEW_MATERIAL, LD_VIEW_EDGES,
 LD_VIEW_ONLY=30, LD_VIEW_RESTORE, LD_VIEW_SEARCH, LD_VIEW_COVER_RESET,
 LD_VIEW_ROW_EXPAND=300, LD_VIEW_ROW=100, LD_VIEW_ROW_ONLY=200 };
void LDViewControls_SetHeader(SDL_Rect camera, SDL_Rect visibility, SDL_Rect more, SDL_Rect appearance);
bool LDViewControls_Rect(int action, SDL_Rect* out);
bool LDViewControls_Click(int x,int y);
bool LDViewControls_Event(const SDL_Event* event);
void LDViewControls_RenderHeader(SDL_Renderer* renderer);
void LDViewControls_RenderPopup(SDL_Renderer* renderer);
void LDViewControls_Close(void);
size_t LDViewControls_MatchingCount(void);

int LDViewControls_ActivePopup(void);
