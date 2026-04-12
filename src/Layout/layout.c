// src/Layout/layout.c
#include "layout.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

static const float kAnchorSnapRadius = 0.01f;  // very small world-space threshold

static ViewPlaneAxis Layout_GetActivePlaneAxis(void) {
    GlobalState* state = Global_Get();
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    return SpaceAdapter_ActivePlaneAxis(&viewCtx);
}

static ViewPlaneAxis Anchor_GetHandleAxis(const Anchor* anchor) {
    if (!anchor) return VIEW_PLANE_XY;
    switch (anchor->handleAxis) {
        case VIEW_PLANE_XY:
        case VIEW_PLANE_YZ:
        case VIEW_PLANE_XZ:
            return anchor->handleAxis;
        default:
            return VIEW_PLANE_XY;
    }
}

static bool Anchor_CanAcceptConnection(const Anchor* anchor) {
    if (!anchor) return false;
    if (anchor->type == ANCHOR_TYPE_CURVE) {
        return anchor->connectionCount < 2;
    }
    return true;
}

static void Anchor_ResetHandles(Anchor* anchor) {
    if (!anchor) return;
    anchor->handleInLength = 0.0f;
    anchor->handleInAngleDeg = 0.0f;
    anchor->handleOutLength = 0.0f;
    anchor->handleOutAngleDeg = 0.0f;
}

static void Anchor_ForceLinkedHandles(Anchor* anchor) {
    if (!anchor) return;
    float baseLen = anchor->handleInLength;
    float baseAngle = anchor->handleInAngleDeg;

    if (anchor->handleOutLength > baseLen) {
        baseLen = anchor->handleOutLength;
        baseAngle = Angle_NormalizeDeg(anchor->handleOutAngleDeg - 180.0f);
    }

    anchor->handleInLength = baseLen;
    anchor->handleInAngleDeg = baseAngle;
    anchor->handleOutLength = baseLen;
    anchor->handleOutAngleDeg = Angle_NormalizeDeg(baseAngle + 180.0f);
}

static void Anchor_EnsureHandleForWall(Layout* layout, int anchorIndex, int wallIndex) {
    if (!layout || wallIndex < 0 || (size_t)wallIndex >= layout->wallCount ||
        anchorIndex < 0 || (size_t)anchorIndex >= layout->anchorCount) return;
    Anchor* anchor = &layout->anchors[anchorIndex];
    if (!anchor || anchor->isDeleted || anchor->type != ANCHOR_TYPE_CURVE) return;

    Wall* wall = &layout->walls[wallIndex];
    int otherIdx = (wall->anchorA == anchorIndex) ? wall->anchorB : wall->anchorA;
    if (otherIdx < 0 || (size_t)otherIdx >= layout->anchorCount) return;
    Anchor* other = &layout->anchors[otherIdx];
    if (!other || other->isDeleted) return;

    Vec3 delta = Vec3_Sub(other->pos, anchor->pos);
    ViewPlaneAxis handleAxis = Anchor_GetHandleAxis(anchor);
    float planarDist = 0.0f;
    float handleAngle = 0.0f;
    Vec3_HandlePolarFromWorldDelta(delta, handleAxis, &planarDist, &handleAngle);
    float handleLen = fmaxf(planarDist * 0.3f, 0.05f);

    if (wall->anchorA == anchorIndex) {
        if (anchor->handleOutLength <= 0.0001f) {
            anchor->handleOutLength = handleLen;
            anchor->handleOutAngleDeg = handleAngle;
            if (anchor->handlesLinked) {
                Anchor_ForceLinkedHandles(anchor);
            }
        }
    } else {
        if (anchor->handleInLength <= 0.0001f) {
            anchor->handleInLength = handleLen;
            anchor->handleInAngleDeg = handleAngle;
            if (anchor->handlesLinked) {
                Anchor_ForceLinkedHandles(anchor);
            }
        }
    }
}

static void Anchor_SeedHandlesFromWalls(Layout* layout, int anchorIndex) {
    if (!layout || anchorIndex < 0 || (size_t)anchorIndex >= layout->anchorCount) return;
    Anchor* anchor = &layout->anchors[anchorIndex];
    Anchor_ResetHandles(anchor);
    if (!anchor || anchor->connectionCount == 0) return;

    ViewPlaneAxis handleAxis = Anchor_GetHandleAxis(anchor);
    for (int i = 0; i < anchor->connectionCount && i < 2; ++i) {
        int wallIdx = anchor->connectedWalls[i];
        if (wallIdx < 0 || wallIdx >= (int)layout->wallCount) continue;
        Wall* wall = &layout->walls[wallIdx];
        int otherIdx = (wall->anchorA == anchorIndex) ? wall->anchorB : wall->anchorA;
        if (otherIdx < 0 || otherIdx >= (int)layout->anchorCount) continue;
        Anchor* other = &layout->anchors[otherIdx];
        if (other->isDeleted) continue;

        Vec3 delta = Vec3_Sub(other->pos, anchor->pos);
        float planarDist = 0.0f;
        float handleAngle = 0.0f;
        Vec3_HandlePolarFromWorldDelta(delta, handleAxis, &planarDist, &handleAngle);
        float handleLen = fmaxf(planarDist * 0.3f, 0.05f);

        if (wall->anchorA == anchorIndex) {
            anchor->handleOutLength = handleLen;
            anchor->handleOutAngleDeg = handleAngle;
        } else {
            anchor->handleInLength = handleLen;
            anchor->handleInAngleDeg = handleAngle;
        }
    }

    if (anchor->connectionCount == 1) {
        if (anchor->handleInLength > 0.0f) {
            anchor->handleOutLength = anchor->handleInLength;
            anchor->handleOutAngleDeg = Angle_NormalizeDeg(anchor->handleInAngleDeg + 180.0f);
        } else {
            anchor->handleInLength = anchor->handleOutLength;
            anchor->handleInAngleDeg = Angle_NormalizeDeg(anchor->handleOutAngleDeg - 180.0f);
        }
    }
}

static int FindExistingAnchor(Layout* layout, Vec3 pos, float threshold) {
    for (size_t i = 0; i < layout->anchorCount; ++i) {
        Vec3 existing = layout->anchors[i].pos;
        float dx = existing.x - pos.x;
        float dy = existing.y - pos.y;
        float dz = existing.z - pos.z;
        if (dx * dx + dy * dy + dz * dz < threshold * threshold)
            return (int)i;
    }
    return -1;
}

//		HELPERS
// ======================================
// 	       ANCHOR LOGIC
int Layout_AddAnchor3(Layout* layout, Vec3 pos) {
    int existing = FindExistingAnchor(layout, pos, kAnchorSnapRadius);
    if (existing >= 0)
        return existing;

    layout->anchors = realloc(layout->anchors, sizeof(Anchor) * (layout->anchorCount + 1));
    Anchor newAnchor = {
        .pos = pos,
        .connectedWalls = NULL,
        .connectionCount = 0,
        .lockAngle = false,
        .angleDeg = 0.0f,
        .isDeleted = false,
        .isPersistent = false,
        .type = ANCHOR_TYPE_CORNER,
        .handlesLinked = true,
        .handleAxis = Layout_GetActivePlaneAxis(),
        .handleInLength = 0.0f,
        .handleInAngleDeg = 0.0f,
        .handleOutLength = 0.0f,
        .handleOutAngleDeg = 0.0f
    };
    layout->anchors[layout->anchorCount] = newAnchor;
    Global_FlagLayoutChanged();
    return (int)layout->anchorCount++;
}

int Layout_AddAnchor(Layout* layout, Vec2 pos) {
    return Layout_AddAnchor3(layout, Vec3_FromVec2(pos, 0.0f));
}

void Layout_RemoveAnchor(Layout* layout, int anchorIndex) {
    if (anchorIndex < 0 || (size_t)anchorIndex >= layout->anchorCount)
        return;

    Anchor* target = &layout->anchors[anchorIndex];

    // Copy wall indices to avoid mutation while iterating
    int wallCount = target->connectionCount;
    int* connected = malloc(sizeof(int) * wallCount);
    memcpy(connected, target->connectedWalls, sizeof(int) * wallCount);

    for (int i = 0; i < wallCount; ++i) {
        int wallIdx = connected[i];
        if (wallIdx < 0 || wallIdx >= (int)layout->wallCount) continue;

        Wall* w = &layout->walls[wallIdx];
        int other = (w->anchorA == anchorIndex) ? w->anchorB : w->anchorA;

        // Check if the other anchor is safe to preserve
        bool safeToDelete = true;
        if (other >= 0 && other < (int)layout->anchorCount) {
            Anchor* otherAnchor = &layout->anchors[other];
            if (!otherAnchor->isDeleted && otherAnchor->connectionCount > 1) {
                safeToDelete = false;
            }
        }

        if (safeToDelete) {
            Layout_MarkWallDeleted(layout, wallIdx);
        }
    }

    target->isPersistent = false;
    Layout_MarkAnchorDeleted(layout, anchorIndex);

    // Optional: auto-prune orphan anchors
    if (Global_Get()->editor.deleteMode == DELETE_MODE_AUTO_PRUNE) {
        for (int i = 0; i < wallCount; ++i) {
	        int wallIdx = connected[i];
	        if (wallIdx < 0 || wallIdx >= (int)layout->wallCount) continue;
	
	        Wall* w = &layout->walls[wallIdx];
	        int other = (w->anchorA == anchorIndex) ? w->anchorB : w->anchorA;
	
	        if (other >= 0 && other < (int)layout->anchorCount) {
	            Anchor* otherAnchor = &layout->anchors[other];
	            if (!otherAnchor->isDeleted && otherAnchor->connectionCount == 0) {
	                Layout_MarkAnchorDeleted(layout, other);
                    }
		}
        }
    }
    free(connected);
}


void Layout_MarkAnchorDeleted(Layout* layout, int anchorIndex) {
    if (anchorIndex < 0 || (size_t)anchorIndex >= layout->anchorCount) return;
    Anchor* a = &layout->anchors[anchorIndex];
    a->isDeleted = true;

    // Optionally: mark all connected walls for deletion too
    for (int i = 0; i < a->connectionCount; ++i) {
        int wallIdx = a->connectedWalls[i];
        Layout_MarkWallDeleted(layout, wallIdx);
    }

    free(a->connectedWalls);
    a->connectedWalls = NULL;
    a->connectionCount = 0;
    Global_FlagLayoutChanged();
}

bool Layout_CanAnchorBecomeCurve(const Layout* layout, int anchorIndex) {
    if (!layout || anchorIndex < 0 || (size_t)anchorIndex >= layout->anchorCount) return false;
    const Anchor* anchor = &layout->anchors[anchorIndex];
    if (anchor->isDeleted) return false;
    return anchor->connectionCount == 2;
}

bool Layout_SetAnchorType(Layout* layout, int anchorIndex, AnchorType type) {
    if (!layout || anchorIndex < 0 || (size_t)anchorIndex >= layout->anchorCount) return false;
    Anchor* anchor = &layout->anchors[anchorIndex];
    if (anchor->isDeleted) return false;
    if (anchor->type == type) return true;

    switch (type) {
        case ANCHOR_TYPE_CORNER:
            anchor->type = ANCHOR_TYPE_CORNER;
            anchor->handlesLinked = true;
            Anchor_ResetHandles(anchor);
            Global_FlagLayoutChanged();
            return true;
        case ANCHOR_TYPE_CURVE:
            if (!Layout_CanAnchorBecomeCurve(layout, anchorIndex)) {
                fprintf(stderr, "[Layout] Anchor %d needs exactly 2 connections to become curved\n", anchorIndex);
                return false;
            }
            anchor->type = ANCHOR_TYPE_CURVE;
            anchor->handlesLinked = true;
            anchor->handleAxis = Layout_GetActivePlaneAxis();
            Anchor_SeedHandlesFromWalls(layout, anchorIndex);
            Anchor_ForceLinkedHandles(anchor);
            Global_FlagLayoutChanged();
            return true;
        default:
            break;
    }
    return false;
}

bool Layout_SetHandlesLinked(Layout* layout, int anchorIndex, bool linked) {
    if (!layout || anchorIndex < 0 || (size_t)anchorIndex >= layout->anchorCount) return false;
    Anchor* anchor = &layout->anchors[anchorIndex];
    if (anchor->isDeleted || anchor->type != ANCHOR_TYPE_CURVE) return false;

    anchor->handlesLinked = linked;
    if (linked) {
        Anchor_ForceLinkedHandles(anchor);
    }

    Global_FlagLayoutChanged();
    return true;
}


//             ANCHOR LOGIC
// ======================================
//             WALL LOGIC

void Layout_AddWall3(Layout* layout, Vec3 from, Vec3 to) {
    int existingA = FindExistingAnchor(layout, from, kAnchorSnapRadius);
    int existingB = FindExistingAnchor(layout, to,   kAnchorSnapRadius);

    if (existingA >= 0 && !Anchor_CanAcceptConnection(&layout->anchors[existingA])) {
        fprintf(stderr, "[Layout] Anchor %d is a curve anchor and cannot exceed 2 connections\n",
                existingA);
        return;
    }
    if (existingB >= 0 && !Anchor_CanAcceptConnection(&layout->anchors[existingB])) {
        fprintf(stderr, "[Layout] Anchor %d is a curve anchor and cannot exceed 2 connections\n",
                existingB);
        return;
    }

    int indexA = Layout_AddAnchor3(layout, from);
    int indexB = Layout_AddAnchor3(layout, to);

    Anchor* anchorA = &layout->anchors[indexA];
    Anchor* anchorB = &layout->anchors[indexB];
    if (!Anchor_CanAcceptConnection(anchorA) || !Anchor_CanAcceptConnection(anchorB)) {
        fprintf(stderr, "[Layout] Refused to connect anchors %d and %d: curve anchors are limited to 2 walls\n",
                indexA, indexB);
        return;
    }

    layout->walls = realloc(layout->walls, sizeof(Wall) * (layout->wallCount + 1));
    Wall newWall = {
        .anchorA = indexA,
        .anchorB = indexB,
        .lockLength = false
    };
    int wallIndex = (int)layout->wallCount;
    layout->walls[layout->wallCount++] = newWall;

    // Update anchor connections
    Anchor* a = anchorA;
    Anchor* b = anchorB;

    a->connectedWalls = realloc(a->connectedWalls, sizeof(int) * (a->connectionCount + 1));
    a->connectedWalls[a->connectionCount++] = wallIndex;

    b->connectedWalls = realloc(b->connectedWalls, sizeof(int) * (b->connectionCount + 1));
    b->connectedWalls[b->connectionCount++] = wallIndex;
    Global_FlagLayoutChanged();

    Anchor_EnsureHandleForWall(layout, indexA, wallIndex);
    Anchor_EnsureHandleForWall(layout, indexB, wallIndex);
}

void Layout_AddWall(Layout* layout, Vec2 from, Vec2 to) {
    Layout_AddWall3(layout, Vec3_FromVec2(from, 0.0f), Vec3_FromVec2(to, 0.0f));
}


static void RemoveWallFromAnchor(Anchor* anchor, int wallIndex) {
    if (!anchor || anchor->isDeleted) return;

    for (int i = 0; i < anchor->connectionCount; ++i) {
        if (anchor->connectedWalls[i] == wallIndex) {
            memmove(&anchor->connectedWalls[i],
                    &anchor->connectedWalls[i + 1],
                    sizeof(int) * (anchor->connectionCount - i - 1));
            anchor->connectionCount--;
            break;
        }
    }
}

static void MaybePruneOrphanAnchors(Layout* layout, int a, int b) {
    if (Global_Get()->editor.deleteMode != DELETE_MODE_AUTO_PRUNE) return;

    if (a >= 0 && a < (int)layout->anchorCount) {
        Anchor* anchorA = &layout->anchors[a];
        if (!anchorA->isDeleted && anchorA->connectionCount == 0) {
            Layout_MarkAnchorDeleted(layout, a);
        }
    }

    if (b >= 0 && b < (int)layout->anchorCount) {
        Anchor* anchorB = &layout->anchors[b];
        if (!anchorB->isDeleted && anchorB->connectionCount == 0) {
            Layout_MarkAnchorDeleted(layout, b);
        }
    }
}


void Layout_RemoveWall(Layout* layout, int wallIndex) {
    if (wallIndex < 0 || (size_t)wallIndex >= layout->wallCount)
        return;

    Wall* w = &layout->walls[wallIndex];
    if (w->isDeleted) return;

    int a = w->anchorA;
    int b = w->anchorB;

    if (a >= 0 && a < (int)layout->anchorCount)
        RemoveWallFromAnchor(&layout->anchors[a], wallIndex);
    if (b >= 0 && b < (int)layout->anchorCount)
        RemoveWallFromAnchor(&layout->anchors[b], wallIndex);

    Layout_MarkWallDeleted(layout, wallIndex);

    // Only in AUTO_PRUNE mode, remove orphan anchors
    if (Global_Get()->editor.deleteMode == DELETE_MODE_AUTO_PRUNE) {
        MaybePruneOrphanAnchors(layout, a, b);
    } else {
        // In SAFE mode: preserve anchors that would otherwise be pruned
        if (a >= 0 && a < (int)layout->anchorCount) {
            Anchor* anchorA = &layout->anchors[a];
            if (!anchorA->isDeleted && anchorA->connectionCount == 0) {
                anchorA->isPersistent = true;
            }
        }
        if (b >= 0 && b < (int)layout->anchorCount) {
            Anchor* anchorB = &layout->anchors[b];
            if (!anchorB->isDeleted && anchorB->connectionCount == 0) {
                anchorB->isPersistent = true;
            }
        }
    }
}



void Layout_MarkWallDeleted(Layout* layout, int wallIndex) {
    if (wallIndex < 0 || (size_t)wallIndex >= layout->wallCount) return;
    layout->walls[wallIndex].isDeleted = true;

    Wall* w = &layout->walls[wallIndex];

    // Remove from anchor connection lists
    if (w->anchorA >= 0 && w->anchorA < (int)layout->anchorCount) {
        RemoveWallFromAnchor(&layout->anchors[w->anchorA], wallIndex);
    }
    if (w->anchorB >= 0 && w->anchorB < (int)layout->anchorCount) {
        RemoveWallFromAnchor(&layout->anchors[w->anchorB], wallIndex);
    }
    Global_FlagLayoutChanged();
}


//		WALL LOGIC
// ======================================
// 	       Layout State

void Layout_Init(Layout* layout, float gridSize) {
    layout->gridSize = gridSize;
    Layout_Scene3DSettings_SetDefaults(&layout->scene3d);
    Layout_ObjectStore_Init(&layout->objectStore);

    layout->walls = NULL;
    layout->wallCount = 0;

    layout->anchors = NULL;
    layout->anchorCount = 0;
}


void Layout_Free(Layout* layout) {
    for (size_t i = 0; i < layout->anchorCount; ++i) {
        free(layout->anchors[i].connectedWalls);
    }
    free(layout->anchors);
    free(layout->walls);
    Layout_ObjectStore_Free(&layout->objectStore);
    layout->anchors = NULL;
    layout->walls = NULL;
    layout->wallCount = 0;
    layout->anchorCount = 0;
}

Vec3 Layout_ComputeCentroid(const Layout* layout, bool* outHasAnchors) {
    if (!layout || layout->anchorCount == 0) {
        if (outHasAnchors) *outHasAnchors = false;
        return (Vec3){ 0.0f, 0.0f, 0.0f };
    }

    Vec3 sum = { 0.0f, 0.0f, 0.0f };
    size_t count = 0;
    for (size_t i = 0; i < layout->anchorCount; ++i) {
        const Anchor* a = &layout->anchors[i];
        if (a->isDeleted) continue;
        sum = Vec3_Add(sum, a->pos);
        ++count;
    }

    if (count == 0) {
        if (outHasAnchors) *outHasAnchors = false;
        return (Vec3){ 0.0f, 0.0f, 0.0f };
    }

    if (outHasAnchors) *outHasAnchors = true;
    return Vec3_Scale(sum, 1.0f / (float)count);
}


void Layout_CompactDeletedElements(Layout* layout) {
    // === WALL COMPACTION ===
    int* wallRemap = malloc(sizeof(int) * layout->wallCount);
    size_t newWallCount = 0;

    for (size_t i = 0; i < layout->wallCount; ++i) {
        wallRemap[i] = -1;
        if (!layout->walls[i].isDeleted) {
            if (i != newWallCount)
                layout->walls[newWallCount] = layout->walls[i];
            wallRemap[i] = (int)newWallCount;
            newWallCount++;
        }
    }
    layout->wallCount = newWallCount;
    layout->walls = realloc(layout->walls, sizeof(Wall) * layout->wallCount);

    // === ANCHOR COMPACTION ===
    int* anchorRemap = malloc(sizeof(int) * layout->anchorCount);
    size_t newAnchorCount = 0;

    for (size_t i = 0; i < layout->anchorCount; ++i) {
        anchorRemap[i] = -1;
        if (!layout->anchors[i].isDeleted) {
            if (i != newAnchorCount)
                layout->anchors[newAnchorCount] = layout->anchors[i];
            anchorRemap[i] = (int)newAnchorCount;
            newAnchorCount++;
        }
    }
    layout->anchorCount = newAnchorCount;
    layout->anchors = realloc(layout->anchors, sizeof(Anchor) * layout->anchorCount);

    // === FIX WALL ANCHOR INDICES ===
    for (size_t i = 0; i < layout->wallCount; ++i) {
        Wall* w = &layout->walls[i];
        w->anchorA = anchorRemap[w->anchorA];
        w->anchorB = anchorRemap[w->anchorB];
        w->isDeleted = false; // Reset
    }

    // === FIX ANCHOR CONNECTED WALL LISTS ===
    for (size_t i = 0; i < layout->anchorCount; ++i) {
        Anchor* a = &layout->anchors[i];
        int* newList = malloc(sizeof(int) * a->connectionCount);
        int newCount = 0;

        for (int j = 0; j < a->connectionCount; ++j) {
            int oldWall = a->connectedWalls[j];
            int remapped = wallRemap[oldWall];
            if (remapped >= 0) {
                newList[newCount++] = remapped;
            }
        }

        free(a->connectedWalls);
        a->connectedWalls = newList;
        a->connectionCount = newCount;
        a->isDeleted = false; // Reset
    }


    // === FINAL PASS: Prune anchors that now have zero wall connections ===
    GlobalState* state = Global_Get();
    bool shouldPrune = (state->editor.deleteMode == DELETE_MODE_AUTO_PRUNE);

    if (shouldPrune) {
        for (size_t i = 0; i < layout->anchorCount; ++i) {
            Anchor* a = &layout->anchors[i];
            if (!a->isDeleted && a->connectionCount == 0 && !a->isPersistent) {
                a->isDeleted = true;
            }
        }
    }


    free(wallRemap);
    free(anchorRemap);
}
