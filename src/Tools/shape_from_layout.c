#include "Tools/shape_from_layout.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    int*   indices;
    size_t count;
    size_t capacity;
    bool   closed;
} AnchorPath;

static ShapeVec2 ShapeVec2_FromLayoutVec(Vec3 v, ViewPlaneAxis axis) {
    Vec2 p = Vec3_ProjectToPlane(v, axis);
    ShapeVec2 r = { p.x, p.y };
    return r;
}

static void* sa_calloc(size_t n, size_t s) { return calloc(n, s); }
static void  sa_free(void* p) { free(p); }

static void AnchorPath_Free(AnchorPath* p) {
    if (!p) return;
    free(p->indices);
    p->indices = NULL;
    p->count = 0;
    p->capacity = 0;
    p->closed = false;
}

static bool AnchorPath_PushBack(AnchorPath* p, int idx) {
    if (!p) return false;
    if (p->count >= p->capacity) {
        size_t nc = p->capacity ? p->capacity * 2 : 8;
        int* tmp = (int*)realloc(p->indices, nc * sizeof(int));
        if (!tmp) return false;
        p->indices = tmp;
        p->capacity = nc;
    }
    p->indices[p->count++] = idx;
    return true;
}

static bool AnchorPath_PushFront(AnchorPath* p, int idx) {
    if (!p) return false;
    if (p->count >= p->capacity) {
        size_t nc = p->capacity ? p->capacity * 2 : 8;
        int* tmp = (int*)realloc(p->indices, nc * sizeof(int));
        if (!tmp) return false;
        p->indices = tmp;
        p->capacity = nc;
    }
    memmove(p->indices + 1, p->indices, p->count * sizeof(int));
    p->indices[0] = idx;
    p->count++;
    return true;
}

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

        *outWallIndex = wallIdx;
        *outOtherAnchor = other;
        return 1;
    }

    return 0;
}

static bool BuildAnchorPaths(const Layout* layout,
                             AnchorPath** outPaths,
                             size_t* outPathCount) {
    if (!layout || !outPaths || !outPathCount) return false;

    size_t wallCount = layout->wallCount;
    bool* visited = (bool*)sa_calloc(wallCount, sizeof(bool));
    if (!visited && wallCount > 0) return false;

    AnchorPath* paths = NULL;
    size_t pathCount = 0;
    size_t pathCap = 0;

    for (size_t wi = 0; wi < wallCount; ++wi) {
        if (visited[wi]) continue;
        Wall* w = &layout->walls[wi];
        if (!w || w->isDeleted) continue;

        if (pathCount >= pathCap) {
            size_t nc = pathCap ? pathCap * 2 : 4;
            AnchorPath* tmp = (AnchorPath*)realloc(paths, nc * sizeof(AnchorPath));
            if (!tmp) { free(visited); return false; }
            paths = tmp;
            pathCap = nc;
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

        int frontPrev = a, front = b;
        while (1) {
            int nextWall = -1, nextAnchor = -1;
            if (!FindNextWallFromAnchor(layout, front, frontPrev, visited, &nextWall, &nextAnchor)) {
                break;
            }
            visited[nextWall] = true;
            if (nextAnchor == path->indices[0]) {
                path->closed = true;
                break;
            }
            if (!AnchorPath_PushBack(path, nextAnchor)) {
                break;
            }
            frontPrev = front;
            front = nextAnchor;
        }

        int backPrev = b, back = a;
        while (!path->closed) {
            int nextWall = -1, nextAnchor = -1;
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
            back = nextAnchor;
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
        toAnchor   < 0 || (size_t)toAnchor   >= layout->anchorCount) {
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

static ShapeSegment MakeSegmentFromAnchors(const Layout* layout,
                                           int anchorIndexA,
                                           int anchorIndexB,
                                           ViewPlaneAxis axis) {
    ShapeSegment seg;
    memset(&seg, 0, sizeof(seg));
    if (!layout ||
        anchorIndexA < 0 || (size_t)anchorIndexA >= layout->anchorCount ||
        anchorIndexB < 0 || (size_t)anchorIndexB >= layout->anchorCount) {
        return seg;
    }

    const Anchor* a = &layout->anchors[anchorIndexA];
    const Anchor* b = &layout->anchors[anchorIndexB];

    seg.p0 = ShapeVec2_FromLayoutVec(a->pos, axis);
    seg.p1 = ShapeVec2_FromLayoutVec(b->pos, axis);

    bool forward = true;
    DetermineWallDirection(layout, anchorIndexA, anchorIndexB, &forward);

    bool useAOut = forward;
    bool useBIn  = forward;

    bool aHasHandle = false;
    bool bHasHandle = false;
    ShapeVec2 c1 = seg.p0;
    ShapeVec2 c2 = seg.p1;

    if (a && a->type == ANCHOR_TYPE_CURVE) {
        float len = useAOut ? a->handleOutLength : a->handleInLength;
        float ang = useAOut ? a->handleOutAngleDeg : a->handleInAngleDeg;
        if (len > 0.0f) {
            ViewPlaneAxis handleAxis = a->handleAxis;
            Vec3 hWorld = Vec3_HandleWorldPosition(a->pos, handleAxis, len, ang);
            Vec2 hProj = Vec3_ProjectToPlane(hWorld, axis);
            c1.x = hProj.x;
            c1.y = hProj.y;
            aHasHandle = true;
        }
    }

    if (b && b->type == ANCHOR_TYPE_CURVE) {
        float len = useBIn ? b->handleInLength : b->handleOutLength;
        float ang = useBIn ? b->handleInAngleDeg : b->handleOutAngleDeg;
        if (len > 0.0f) {
            ViewPlaneAxis handleAxis = b->handleAxis;
            Vec3 hWorld = Vec3_HandleWorldPosition(b->pos, handleAxis, len, ang);
            Vec2 hProj = Vec3_ProjectToPlane(hWorld, axis);
            c2.x = hProj.x;
            c2.y = hProj.y;
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

bool ShapeDocument_FromLayoutProjected(const char* name,
                                       const Layout* layout,
                                       ViewPlaneAxis axis,
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
        for (size_t i = 0; i < pathCount; ++i) AnchorPath_Free(&paths[i]);
        free(paths);
        return false;
    }

    Shape* shape = &shapes[0];
    shape->name = name;
    shape->pathCount = pathCount;
    shape->paths = (ShapePath*)sa_calloc(pathCount, sizeof(ShapePath));
    if (!shape->paths && pathCount > 0) {
        sa_free(shapes);
        for (size_t i = 0; i < pathCount; ++i) AnchorPath_Free(&paths[i]);
        free(paths);
        return false;
    }

    for (size_t pi = 0; pi < pathCount; ++pi) {
        AnchorPath* ap = &paths[pi];
        ShapePath* sp  = &shape->paths[pi];

        if (ap->count < 2) {
            AnchorPath_Free(ap);
            continue;
        }

        size_t segCount = ap->closed ? ap->count : (ap->count - 1);
        sp->closed = ap->closed;
        sp->segmentCount = segCount;
        sp->segments = (ShapeSegment*)sa_calloc(segCount, sizeof(ShapeSegment));
        if (!sp->segments && segCount > 0) {
            for (size_t pj = 0; pj <= pi; ++pj) {
                ShapePath* s2 = &shape->paths[pj];
                sa_free(s2->segments);
                s2->segments = NULL;
                s2->segmentCount = 0;
            }
            sa_free(shape->paths);
            sa_free(shapes);
            for (size_t i = 0; i < pathCount; ++i) AnchorPath_Free(&paths[i]);
            free(paths);
            outDoc->shapeCount = 0;
            outDoc->shapes = NULL;
            return false;
        }

        for (size_t si = 0; si < segCount; ++si) {
            int idxA = ap->indices[si];
            int idxB = (si == segCount - 1 && ap->closed)
                     ? ap->indices[0]
                     : ap->indices[si + 1];

            if (idxA < 0 || (size_t)idxA >= layout->anchorCount ||
                idxB < 0 || (size_t)idxB >= layout->anchorCount) {
                continue;
            }
            if (layout->anchors[idxA].isDeleted ||
                layout->anchors[idxB].isDeleted) {
                continue;
            }

            sp->segments[si] = MakeSegmentFromAnchors(layout, idxA, idxB, axis);
        }

        AnchorPath_Free(ap);
    }

    free(paths);
    outDoc->shapeCount = 1;
    outDoc->shapes = shapes;
    return true;
}

bool ShapeDocument_FromLayout(const char* name,
                              const Layout* layout,
                              ShapeDocument* outDoc) {
    return ShapeDocument_FromLayoutProjected(name, layout, VIEW_PLANE_XY, outDoc);
}
