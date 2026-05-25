// src/Layout/layout_origin.c
#include "layout_origin.h"
#include "Core/global_state.h"

void Layout_ShiftOriginToAnchor(Layout* layout,
                                Grid* grid,
                                int anchorIndex,
                                float viewportCenterX,
                                float viewportCenterY) {
    if (anchorIndex < 0 || anchorIndex >= (int)layout->anchorCount) return;
    Anchor* target = &layout->anchors[anchorIndex];
    Vec3 originShift = target->pos;

    // Shift all anchors
    for (size_t i = 0; i < layout->anchorCount; ++i) {
        layout->anchors[i].pos.x -= originShift.x;
        layout->anchors[i].pos.y -= originShift.y;
        layout->anchors[i].pos.z -= originShift.z;
    }

    // Center origin visually on screen
    float pixelsPerUnit = grid->scale * grid->gridSize;
    grid->offsetX = -viewportCenterX / pixelsPerUnit;
    grid->offsetY = -viewportCenterY / pixelsPerUnit;

    Global_FlagLayoutChanged();
    Global_FlagGridChanged();
}
