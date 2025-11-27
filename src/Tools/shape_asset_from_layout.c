// Tools/shape_asset_from_layout.c
//
// Bridge from Layout (anchors + walls) to a simple ShapeDocument
// representation that other tools/sims can consume.

#include "Tools/shape_asset.h"
#include "Layout/layout.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

// --------- small helpers ---------

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void* sa_calloc(size_t count, size_t size) {
    void* p = calloc(count, size);
    return p;
}

static void sa_free(void* p) {
    free(p);
}

// --------- ShapeDocument lifetime ---------

void ShapeDocument_Free(ShapeDocument* doc) {
    if (!doc) return;

    if (doc->shapes) {
        for (size_t si = 0; si < doc->shapeCount; ++si) {
            Shape* shape = &doc->shapes[si];
            if (!shape->paths) continue;
            for (size_t pi = 0; pi < shape->pathCount; ++pi) {
                ShapePath* path = &shape->paths[pi];
                sa_free(path->segments);
                path->segments = NULL;
                path->segmentCount = 0;
            }
            sa_free(shape->paths);
            shape->paths = NULL;
            shape->pathCount = 0;
        }
        sa_free(doc->shapes);
    }

    doc->shapes = NULL;
    doc->shapeCount = 0;
}

// --------- internal path extraction types ---------

typedef struct {
    int*   indices;  // anchor indices
    size_t count;
    size_t capacity;
    bool   closed;
} AnchorPath;

static bool AnchorPath_PushBack(AnchorPath* path, int anchorIndex) {
    if (!path) return false;
    if (path->count >= path->capacity) {
        size_t newCap = path->capacity ? path->capacity * 2 : 8;
        int* tmp = (int*)realloc(path->indices, newCap * sizeof(int));
        if (!tmp) return false;
        path->indices = tmp;
        path->capacity = newCap;
    }
    path->indices[path->count++] = anchorIndex;
    return true;
}

static bool AnchorPath_PushFront(AnchorPath* path, int anchorIndex) {
    if (!path) return false;
    if (path->count >= path->capacity) {
        size_t newCap = path->capacity ? path->capacity * 2 : 8;
        int* tmp = (int*)realloc(path->indices, newCap * sizeof(int));
        if (!tmp) return false;
        path->indices = tmp;
        path->capacity = newCap;
    }
    // shift right by one
    memmove(path->indices + 1, path->indices, path->count * sizeof(int));
    path->indices[0] = anchorIndex;
    path->count++;
    return true;
}

static void AnchorPath_Free(AnchorPath* path) {
    if (!path) return;
    free(path->indices);
    path->indices = NULL;
    path->count = 0;
    path->capacity = 0;
    path->closed = false;
}

// Find a single unvisited wall connected to `anchorIndex` that does not
// lead back to `prevAnchorIndex`. Returns 1 if found, 0 otherwise.
static int FindNextWallFromAnchor(const Layout* layout,
                                  int anchorIndex,
                                  int prevAnchorIndex,
                                  const bool* visitedWall,
                                  int* outWallIndex,
                                  int* outOtherAnchor) {
    if (!layout || !visitedWall || !outWallIndex || !outOtherAnchor) return 0;
    if (anchorIndex < 0 || (size_t)anchorIndex >= layout->anchorCount) return 0;	

    const Anchor* anchor = &layout->anchors[anchorIndex];
    if (!anchor || anchor->isDeleted) return 0;

    for (int i = 0; i < anchor->connectionCount; ++i) {
        int wallIdx = anchor->connectedWalls[i];
        if (wallIdx < 0 || (size_t)wallIdx >= layout->wallCount) continue;

        const Wall* w = &layout->walls[wallIdx];
        if (!w || w->isDeleted) continue;
        if (visitedWall[wallIdx]) continue;

        int other = (w->anchorA == anchorIndex) ? w->anchorB : w->anchorA;
        if (other == prevAnchorIndex) continue;
        if (other < 0 || (size_t)other >= layout->anchorCount) continue;

        const Anchor* otherAnchor = &layout->anchors[other];
        if (!otherAnchor || otherAnchor->isDeleted) continue;

        *outWallIndex  = wallIdx;
        *outOtherAnchor = other;
        return 1;
    }

    return 0;
}

// Build a list of AnchorPath from the Layout's walls.
// Each path is a chain of anchors (possibly closed if it loops).
static bool BuildAnchorPaths(const Layout* layout,
                             AnchorPath** outPaths,
                             size_t* outPathCount) {
    if (!layout || !outPaths || !outPathCount) return false;

    size_t wallCount = layout->wallCount;
    bool* visited = (bool*)sa_calloc(wallCount, sizeof(bool));
    if (!visited && wallCount > 0) return false;

    AnchorPath* paths = NULL;
    size_t pathCount = 0;
    size_t pathCap   = 0;

    for (size_t wi = 0; wi < wallCount; ++wi) {
        if (visited[wi]) continue;
        Wall* w = &layout->walls[wi];
        if (!w || w->isDeleted) continue;

        // allocate new path
        if (pathCount >= pathCap) {
            size_t newCap = pathCap ? pathCap * 2 : 4;
            AnchorPath* tmp = (AnchorPath*)realloc(paths, newCap * sizeof(AnchorPath));
            if (!tmp) {
                free(visited);
                return false;
            }
            paths = tmp;
            pathCap = newCap;
        }
        AnchorPath* path = &paths[pathCount];
        memset(path, 0, sizeof(*path));

        int a = w->anchorA;
        int b = w->anchorB;

        if (!AnchorPath_PushBack(path, a) ||
            !AnchorPath_PushBack(path, b)) {
            AnchorPath_Free(path);
            free(visited);
            free(paths);
            return false;
        }
        visited[wi] = true;

        // grow forward from b
        int frontPrev = a;
        int front     = b;
        while (1) {
            int nextWall = -1;
            int nextAnchor = -1;
            if (!FindNextWallFromAnchor(layout, front, frontPrev, visited, &nextWall, &nextAnchor)) {
                break;
            }
            visited[nextWall] = true;

            if (nextAnchor == path->indices[0]) {
                // closed loop
                path->closed = true;
                break;
            }

            if (!AnchorPath_PushBack(path, nextAnchor)) {
                // fail hard and clean up later
                break;
            }

            frontPrev = front;
            front     = nextAnchor;
        }

        // grow backward from a
        int backPrev = b;
        int back     = a;
        while (!path->closed) {
            int nextWall = -1;
            int nextAnchor = -1;
            if (!FindNextWallFromAnchor(layout, back, backPrev, visited, &nextWall, &nextAnchor)) {
                break;
            }
            visited[nextWall] = true;

            if (nextAnchor == path->indices[path->count - 1]) {
                path->closed = true;
                break;
            }

            if (!AnchorPath_PushFront(path, nextAnchor)) {
                break;
            }

            backPrev = back;
            back     = nextAnchor;
        }

        ++pathCount;
    }

    free(visited);

    *outPaths = paths;
    *outPathCount = pathCount;
    return true;
}

static bool DetermineWallDirection(const Layout* layout,
                                   int fromAnchor,
                                   int toAnchor,
                                   bool* outForward) {
    if (!layout ||
        fromAnchor < 0 || (size_t)fromAnchor >= layout->anchorCount ||
        toAnchor < 0   || (size_t)toAnchor   >= layout->anchorCount) {
        return false;
    }

    const Anchor* anchor = &layout->anchors[fromAnchor];
    if (!anchor || !anchor->connectedWalls) return false;

    for (int i = 0; i < anchor->connectionCount; ++i) {
        int wallIdx = anchor->connectedWalls[i];
        if (wallIdx < 0 || (size_t)wallIdx >= layout->wallCount) continue;

        const Wall* wall = &layout->walls[wallIdx];
        if (wall->isDeleted) continue;

        int a = wall->anchorA;
        int b = wall->anchorB;
        if ((a == fromAnchor && b == toAnchor) ||
            (b == fromAnchor && a == toAnchor)) {
            if (outForward) {
                *outForward = (a == fromAnchor && b == toAnchor);
            }
            return true;
        }
    }

    return false;
}

// Create ShapeSegment from two anchors and their handle data.
static ShapeSegment MakeSegmentFromAnchors(const Layout* layout,
                                           int anchorIndexA,
                                           int anchorIndexB) {
    ShapeSegment seg;
    memset(&seg, 0, sizeof(seg));
    if (!layout ||
        anchorIndexA < 0 || (size_t)anchorIndexA >= layout->anchorCount ||
        anchorIndexB < 0 || (size_t)anchorIndexB >= layout->anchorCount) {
        return seg;
    }

    const Anchor* a = &layout->anchors[anchorIndexA];
    const Anchor* b = &layout->anchors[anchorIndexB];
    seg.p0 = a->pos;
    seg.p1 = b->pos;

    bool forward = true;
    DetermineWallDirection(layout, anchorIndexA, anchorIndexB, &forward);

    bool useAOut = forward;
    bool useBIn = forward;

    bool aHasHandle = false;
    bool bHasHandle = false;
    Vec2 c1 = seg.p0;
    Vec2 c2 = seg.p1;

    if (a && a->type == ANCHOR_TYPE_CURVE) {
        float len = useAOut ? a->handleOutLength : a->handleInLength;
        float ang = useAOut ? a->handleOutAngleDeg : a->handleInAngleDeg;
        if (len > 0.0f) {
            Vec2 offset = Vec2_FromPolar(len, ang);
            c1.x += offset.x;
            c1.y += offset.y;
            aHasHandle = true;
        }
    }

    if (b && b->type == ANCHOR_TYPE_CURVE) {
        float len = useBIn ? b->handleInLength : b->handleOutLength;
        float ang = useBIn ? b->handleInAngleDeg : b->handleOutAngleDeg;
        if (len > 0.0f) {
            Vec2 offset = Vec2_FromPolar(len, ang);
            c2.x += offset.x;
            c2.y += offset.y;
            bHasHandle = true;
        }
    }

    if (aHasHandle || bHasHandle) {
        seg.type = SHAPE_SEGMENT_CUBIC_BEZIER;
        seg.c1 = c1;
        seg.c2 = c2;
    } else {
        seg.type = SHAPE_SEGMENT_LINE;
    }

    return seg;
}

// Convert AnchorPaths into a ShapeDocument with a single Shape.
bool ShapeDocument_FromLayout(const char* name,
                              const Layout* layout,
                              ShapeDocument* outDoc) {
    if (!layout || !outDoc) return false;

    memset(outDoc, 0, sizeof(*outDoc));

    AnchorPath* paths = NULL;
    size_t pathCount = 0;
    if (!BuildAnchorPaths(layout, &paths, &pathCount)) {
        return false;
    }

    Shape* shapes = (Shape*)sa_calloc(1, sizeof(Shape));
    if (!shapes) {
        for (size_t i = 0; i < pathCount; ++i) {
            AnchorPath_Free(&paths[i]);
        }
        free(paths);
        return false;
    }

    Shape* shape = &shapes[0];
    shape->name = name;

    shape->pathCount = pathCount;
    shape->paths = (ShapePath*)sa_calloc(pathCount, sizeof(ShapePath));
    if (!shape->paths && pathCount > 0) {
        sa_free(shapes);
        for (size_t i = 0; i < pathCount; ++i) {
            AnchorPath_Free(&paths[i]);
        }
        free(paths);
        return false;
    }

    for (size_t pi = 0; pi < pathCount; ++pi) {
        AnchorPath* ap = &paths[pi];
        ShapePath* sp = &shape->paths[pi];

        if (ap->count < 2) {
            // degenerate path
            AnchorPath_Free(ap);
            continue;
        }

        size_t segmentCount = ap->closed ? ap->count : (ap->count - 1);
        sp->closed = ap->closed;
        sp->segmentCount = segmentCount;
        sp->segments = (ShapeSegment*)sa_calloc(segmentCount, sizeof(ShapeSegment));
        if (!sp->segments && segmentCount > 0) {
            // cleanup everything on failure
            for (size_t pj = 0; pj <= pi; ++pj) {
                ShapePath* s2 = &shape->paths[pj];
                sa_free(s2->segments);
                s2->segments = NULL;
                s2->segmentCount = 0;
            }
            sa_free(shape->paths);
            sa_free(shapes);
            for (size_t i = 0; i < pathCount; ++i) {
                AnchorPath_Free(&paths[i]);
            }
            free(paths);
            outDoc->shapeCount = 0;
            outDoc->shapes = NULL;
            return false;
        }

        // fill segments
        for (size_t si = 0; si < segmentCount; ++si) {
            int idxA = ap->indices[si];
            int idxB;
            if (si == segmentCount - 1 && ap->closed) {
                idxB = ap->indices[0];
            } else {
                idxB = ap->indices[si + 1];
            }

            if (idxA < 0 || (size_t)idxA >= layout->anchorCount ||
                idxB < 0 || (size_t)idxB >= layout->anchorCount) {
                continue;
            }

            if (layout->anchors[idxA].isDeleted || layout->anchors[idxB].isDeleted) {
                continue;
            }

            sp->segments[si] = MakeSegmentFromAnchors(layout, idxA, idxB);
        }

        AnchorPath_Free(ap);
    }

    free(paths);

    outDoc->shapeCount = 1;
    outDoc->shapes = shapes;
    return true;
}
