// src/Layout/layout_origin.c
#include "layout_origin.h"
#include "Core/global_state.h"

void Layout_ShiftOriginToAnchor(Layout* layout, Grid* grid, int anchorIndex, int screenW, int screenH) {
    if (anchorIndex < 0 || anchorIndex >= (int)layout->anchorCount) return;
    Anchor* target = &layout->anchors[anchorIndex];
    Vec2 originShift = target->pos;

    // Shift all anchors
    for (size_t i = 0; i < layout->anchorCount; ++i) {
        layout->anchors[i].pos.x -= originShift.x;
        layout->anchors[i].pos.y -= originShift.y;
    }

    // Center origin visually on screen
    float pixelsPerUnit = grid->scale * grid->gridSize;
    grid->offsetX = -((float)screenW / 2.0f) / pixelsPerUnit;
    grid->offsetY = -((float)screenH / 2.0f) / pixelsPerUnit;

    Global_FlagLayoutChanged();
    Global_FlagGridChanged();
}
