#pragma once
#include "Layout/layout.h"
#include "Core/SDLApp/sdl_app_framework.h"

typedef enum {
    HITBOX_NONE,
    HITBOX_WALL,
    HITBOX_POINT
} HitboxType;

typedef struct {
    HitboxType type;
    int index;      // Wall index (or point index if used)
    SDL_Rect bounds;
} Hitbox;

// Call once per frame after layout update
void HitboxSystem_Rebuild(const Layout* layout, float scale, float offsetX, float offsetY);

// Returns hitbox under screen-space mouse position
Hitbox HitboxSystem_GetHitAt(int mouseX, int mouseY);

