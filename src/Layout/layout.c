// src/Layout/layout.c
#include "layout.h"
#include "Core/global_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>



static int FindExistingAnchor(Layout* layout, Vec2 pos, float threshold) {
    for (size_t i = 0; i < layout->anchorCount; ++i) {
        Vec2 existing = layout->anchors[i].pos;
        float dx = existing.x - pos.x;
        float dy = existing.y - pos.y;
        if (dx * dx + dy * dy < threshold * threshold)
            return (int)i;
    }
    return -1;
}

//		HELPERS
// ======================================
// 	       ANCHOR LOGIC
int Layout_AddAnchor(Layout* layout, Vec2 pos) {
    float snapRadius = 0.01f;  // very small world-space threshold

    int existing = FindExistingAnchor(layout, pos, snapRadius);
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
        .isPersistent = false
    };
    layout->anchors[layout->anchorCount] = newAnchor;
    return (int)layout->anchorCount++;
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

    free(connected);
    
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
}


//             ANCHOR LOGIC
// ======================================
//             WALL LOGIC

void Layout_AddWall(Layout* layout, Vec2 from, Vec2 to) {
    int indexA = Layout_AddAnchor(layout, from);
    int indexB = Layout_AddAnchor(layout, to);

    layout->walls = realloc(layout->walls, sizeof(Wall) * (layout->wallCount + 1));
    Wall newWall = {
        .anchorA = indexA,
        .anchorB = indexB,
        .lockLength = false
    };
    int wallIndex = (int)layout->wallCount;
    layout->walls[layout->wallCount++] = newWall;

    // Update anchor connections
    Anchor* a = &layout->anchors[indexA];
    Anchor* b = &layout->anchors[indexB];

    a->connectedWalls = realloc(a->connectedWalls, sizeof(int) * (a->connectionCount + 1));
    a->connectedWalls[a->connectionCount++] = wallIndex;

    b->connectedWalls = realloc(b->connectedWalls, sizeof(int) * (b->connectionCount + 1));
    b->connectedWalls[b->connectionCount++] = wallIndex;
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
}


//		WALL LOGIC
// ======================================
// 	       Layout State

void Layout_Init(Layout* layout, float gridSize) {
    layout->gridSize = gridSize;

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
    layout->anchors = NULL;
    layout->walls = NULL;
    layout->wallCount = 0;
    layout->anchorCount = 0;
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

