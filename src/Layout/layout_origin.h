// src/Layout/layout_origin.h
#pragma once

#include "Layout/layout.h"
#include "Layout/Grid/grid.h"

void Layout_ShiftOriginToAnchor(Layout* layout,
                                Grid* grid,
                                int anchorIndex,
                                float viewportCenterX,
                                float viewportCenterY);
