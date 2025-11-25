// src/Layout/layout.h
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "Math/math_util.h"

//        Anchor node in layout graph
// ======================================
typedef enum {
    ANCHOR_TYPE_CORNER = 0,
    ANCHOR_TYPE_CURVE  = 1
} AnchorType;

typedef struct {
    Vec2 pos;               // Position in world space
    int* connectedWalls;    // Dynamic array of wall indices
    int  connectionCount;

    bool lockAngle;         // Angle lock constraint enabled
    float angleDeg;         // Locked angle (if used)
    bool isDeleted;
    bool isPersistent;

    AnchorType type;        // Corner or smooth curve
    bool handlesLinked;     // When true, in/out handle angles mirror
    float handleInLength;   // Polar length for incoming handle
    float handleInAngleDeg; // Angle in degrees relative to +X
    float handleOutLength;  // Polar length for outgoing handle
    float handleOutAngleDeg;
} Anchor;


//        Wall edge between two anchors
// ======================================
typedef struct {
    int anchorA;            // Index into Layout.anchors
    int anchorB;
    bool lockLength;        // Optional constraint
    bool isDeleted;
} Wall;


//        Full layout graph
// ======================================
typedef struct {
    float gridSize;

    Anchor* anchors;
    size_t anchorCount;

    Wall* walls;
    size_t wallCount;
} Layout;


//        Public layout management
// ======================================
void Layout_Init(Layout* layout, float gridSize);
void Layout_Free(Layout* layout);
void Layout_CompactDeletedElements(Layout* layout);


//        Anchor management
// ======================================
int  Layout_AddAnchor(Layout* layout, Vec2 pos);
void Layout_RemoveAnchor(Layout* layout, int anchorIndex);
void Layout_MarkAnchorDeleted(Layout* layout, int anchorIndex);
bool Layout_SetAnchorType(Layout* layout, int anchorIndex, AnchorType type);
bool Layout_CanAnchorBecomeCurve(const Layout* layout, int anchorIndex);
bool Layout_SetHandlesLinked(Layout* layout, int anchorIndex, bool linked);


//        Wall management
// ======================================
void Layout_AddWall(Layout* layout, Vec2 from, Vec2 to);
void Layout_RemoveWall(Layout* layout, int wallIndex);
void Layout_MarkWallDeleted(Layout* layout, int wallIndex);
