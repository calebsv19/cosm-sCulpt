// src/Layout/layout.c
#include "layout.h"
#include <stdlib.h>

void Layout_Init(Layout* layout, float gridSize) {
    layout->gridSize = gridSize;
    layout->walls = NULL;
    layout->wallCount = 0;
}

void Layout_AddWall(Layout* layout, Vec2 from, Vec2 to) {
    layout->walls = realloc(layout->walls, sizeof(Wall) * (layout->wallCount + 1));
    layout->walls[layout->wallCount++] = (Wall){from, to};
}

void Layout_Free(Layout* layout) {
    free(layout->walls);
    layout->walls = NULL;
    layout->wallCount = 0;
}

