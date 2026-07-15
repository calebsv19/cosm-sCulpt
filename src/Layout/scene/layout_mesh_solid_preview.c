#include "Layout/scene/layout_mesh_solid_preview.h"
#include "Layout/scene/layout_mesh_asset_path_resolver.h"

#include "Core/data_paths.h"
#include "Math/math_util.h"
#include "core_time.h"
#include "vk_renderer.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LD_MESH_SOLID_ASSET_CACHE_CAPACITY 8u
#define LD_MESH_SOLID_INTERACTIVE_TRIANGLES 8000u
#define LD_MESH_SOLID_SETTLED_TRIANGLES 18000u
#define LD_MESH_SOLID_MAX_CLUSTER_RESOLUTION 96
#define LD_MESH_SOLID_SETTLE_NS 150000000ull
#define LD_MESH_SOLID_INTERACTIVE_SCALE 0.60f
#define LD_MESH_SOLID_SETTLED_SCALE 0.75f

typedef struct {
    char runtimePath[512];
    off_t fileSize;
    time_t modifiedTime;
    bool populated;
    bool valid;
    LayoutMeshSolidPreviewLod interactive;
    LayoutMeshSolidPreviewLod settled;
} LayoutMeshSolidPreviewAssetCache;

typedef struct {
    LayoutMeshSolidPreviewAssetCache assets[LD_MESH_SOLID_ASSET_CACHE_CAPACITY];
    size_t nextAssetSlot;
    VkRenderer* renderer;
    VkRendererTexture texture;
    bool textureValid;
    uint8_t* rgba;
    float* depth;
    int rasterWidth;
    int rasterHeight;
    uint64_t surfaceSignature;
    uint64_t objectSignature;
    uint64_t appearanceSignature;
    CoreTimeNs qualityChangedAt;
    SpaceViewContext qualityViewContext;
    bool surfaceSignatureValid;
    bool objectSignatureValid;
    bool appearanceSignatureValid;
    bool qualityViewContextValid;
    bool renderedInteractive;
    bool pixelsValid;
    LayoutMeshSolidPreviewFrameStats lastStats;
} LayoutMeshSolidPreviewCache;

typedef struct {
    float x;
    float y;
    float depth;
} LayoutMeshSolidScreenVertex;

typedef struct {
    uint32_t keyA;
    uint32_t keyB;
    uint32_t keyC;
    uint32_t a;
    uint32_t b;
    uint32_t c;
} LayoutMeshSolidClusterTriangle;

static LayoutMeshSolidPreviewCache g_solidPreview;

// Clear one LOD descriptor after releasing its indexed buffers.
void Layout_MeshSolidPreviewFreeLod(LayoutMeshSolidPreviewLod* lod) {
    if (!lod) return;
    free(lod->vertices);
    free(lod->indices);
    memset(lod, 0, sizeof(*lod));
}

// Copy the authoritative runtime mesh when it already fits the requested budget.
static bool LayoutMeshSolid_CopyRuntimeMesh(const CoreMeshAssetRuntimeDocument* document,
                                            LayoutMeshSolidPreviewLod* outLod) {
    if (!document || !outLod || document->vertex_count == 0u || document->triangle_count == 0u) {
        return false;
    }
    if (document->vertex_count > UINT32_MAX ||
        document->triangle_count > SIZE_MAX / (3u * sizeof(uint32_t))) {
        return false;
    }

    outLod->vertices = (Vec3*)calloc(document->vertex_count, sizeof(Vec3));
    outLod->indices = (uint32_t*)calloc(document->triangle_count * 3u, sizeof(uint32_t));
    if (!outLod->vertices || !outLod->indices) {
        Layout_MeshSolidPreviewFreeLod(outLod);
        return false;
    }

    for (size_t i = 0u; i < document->vertex_count; ++i) {
        outLod->vertices[i] = (Vec3){
            (float)document->vertices[i].position.x,
            (float)document->vertices[i].position.y,
            (float)document->vertices[i].position.z
        };
    }
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle* triangle = &document->triangles[i];
        if (triangle->a > UINT32_MAX || triangle->b > UINT32_MAX || triangle->c > UINT32_MAX) {
            Layout_MeshSolidPreviewFreeLod(outLod);
            return false;
        }
        outLod->indices[i * 3u + 0u] = (uint32_t)triangle->a;
        outLod->indices[i * 3u + 1u] = (uint32_t)triangle->b;
        outLod->indices[i * 3u + 2u] = (uint32_t)triangle->c;
    }

    outLod->vertexCount = document->vertex_count;
    outLod->triangleCount = document->triangle_count;
    outLod->sourceVertexCount = document->vertex_count;
    outLod->sourceTriangleCount = document->triangle_count;
    outLod->clusterResolution = 0;
    return true;
}

// Quantize one coordinate into a bounded vertex-cluster grid cell.
static int LayoutMeshSolid_ClusterCoordinate(double value,
                                             double minimum,
                                             double maximum,
                                             int resolution) {
    const double extent = maximum - minimum;
    double normalized = 0.0;
    int coordinate = 0;
    if (resolution <= 1 || !isfinite(extent) || extent <= DBL_EPSILON) return 0;
    normalized = (value - minimum) / extent;
    if (normalized < 0.0) normalized = 0.0;
    if (normalized > 1.0) normalized = 1.0;
    coordinate = (int)floor(normalized * (double)resolution);
    if (coordinate >= resolution) coordinate = resolution - 1;
    if (coordinate < 0) coordinate = 0;
    return coordinate;
}

// Order clustered triangle keys while retaining the first source winding for drawing.
static int LayoutMeshSolid_CompareClusterTriangles(const void* lhs, const void* rhs) {
    const LayoutMeshSolidClusterTriangle* a = (const LayoutMeshSolidClusterTriangle*)lhs;
    const LayoutMeshSolidClusterTriangle* b = (const LayoutMeshSolidClusterTriangle*)rhs;
    if (a->keyA != b->keyA) return a->keyA < b->keyA ? -1 : 1;
    if (a->keyB != b->keyB) return a->keyB < b->keyB ? -1 : 1;
    if (a->keyC != b->keyC) return a->keyC < b->keyC ? -1 : 1;
    return 0;
}

// Build one complete clustered surface; all source triangles are remapped before degenerates are removed.
static bool LayoutMeshSolid_BuildClusteredAtResolution(
    const CoreMeshAssetRuntimeDocument* document,
    int resolution,
    LayoutMeshSolidPreviewLod* outLod) {
    size_t cellCount = 0u;
    int32_t* cellToCluster = NULL;
    uint32_t* vertexMap = NULL;
    Vec3* sums = NULL;
    uint32_t* counts = NULL;
    Vec3* vertices = NULL;
    uint32_t* indices = NULL;
    LayoutMeshSolidClusterTriangle* clusteredTriangles = NULL;
    size_t clusterCount = 0u;
    size_t triangleCount = 0u;
    const CoreMeshAssetBounds3 bounds = document->contract.local_bounds;

    if (!document || !outLod || resolution < 2 ||
        document->vertex_count == 0u || document->triangle_count == 0u ||
        document->vertex_count > UINT32_MAX) {
        return false;
    }
    if ((size_t)resolution > SIZE_MAX / (size_t)resolution) return false;
    cellCount = (size_t)resolution * (size_t)resolution;
    if (cellCount > SIZE_MAX / (size_t)resolution) return false;
    cellCount *= (size_t)resolution;

    cellToCluster = (int32_t*)malloc(cellCount * sizeof(int32_t));
    vertexMap = (uint32_t*)malloc(document->vertex_count * sizeof(uint32_t));
    sums = (Vec3*)calloc(document->vertex_count, sizeof(Vec3));
    counts = (uint32_t*)calloc(document->vertex_count, sizeof(uint32_t));
    vertices = (Vec3*)calloc(document->vertex_count, sizeof(Vec3));
    indices = (uint32_t*)malloc(document->triangle_count * 3u * sizeof(uint32_t));
    clusteredTriangles = (LayoutMeshSolidClusterTriangle*)malloc(
        document->triangle_count * sizeof(LayoutMeshSolidClusterTriangle));
    if (!cellToCluster || !vertexMap || !sums || !counts || !vertices || !indices ||
        !clusteredTriangles) goto fail;
    for (size_t i = 0u; i < cellCount; ++i) cellToCluster[i] = -1;

    for (size_t i = 0u; i < document->vertex_count; ++i) {
        const CoreObjectVec3 position = document->vertices[i].position;
        const int x = LayoutMeshSolid_ClusterCoordinate(position.x,
                                                        bounds.min.x,
                                                        bounds.max.x,
                                                        resolution);
        const int y = LayoutMeshSolid_ClusterCoordinate(position.y,
                                                        bounds.min.y,
                                                        bounds.max.y,
                                                        resolution);
        const int z = LayoutMeshSolid_ClusterCoordinate(position.z,
                                                        bounds.min.z,
                                                        bounds.max.z,
                                                        resolution);
        const size_t cell = (size_t)x +
                            ((size_t)y * (size_t)resolution) +
                            ((size_t)z * (size_t)resolution * (size_t)resolution);
        int32_t cluster = cellToCluster[cell];
        if (cluster < 0) {
            if (clusterCount >= UINT32_MAX) goto fail;
            cluster = (int32_t)clusterCount++;
            cellToCluster[cell] = cluster;
        }
        vertexMap[i] = (uint32_t)cluster;
        sums[cluster].x += (float)position.x;
        sums[cluster].y += (float)position.y;
        sums[cluster].z += (float)position.z;
        counts[cluster]++;
    }

    for (size_t i = 0u; i < clusterCount; ++i) {
        const float inverse = counts[i] > 0u ? 1.0f / (float)counts[i] : 1.0f;
        vertices[i] = Vec3_Scale(sums[i], inverse);
    }

    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle* source = &document->triangles[i];
        uint32_t a = 0u;
        uint32_t b = 0u;
        uint32_t c = 0u;
        if (source->a >= document->vertex_count ||
            source->b >= document->vertex_count ||
            source->c >= document->vertex_count) {
            continue;
        }
        a = vertexMap[source->a];
        b = vertexMap[source->b];
        c = vertexMap[source->c];
        if (a == b || b == c || c == a) continue;
        clusteredTriangles[triangleCount] = (LayoutMeshSolidClusterTriangle){
            .keyA = a,
            .keyB = b,
            .keyC = c,
            .a = a,
            .b = b,
            .c = c
        };
        if (clusteredTriangles[triangleCount].keyA > clusteredTriangles[triangleCount].keyB) {
            const uint32_t swap = clusteredTriangles[triangleCount].keyA;
            clusteredTriangles[triangleCount].keyA = clusteredTriangles[triangleCount].keyB;
            clusteredTriangles[triangleCount].keyB = swap;
        }
        if (clusteredTriangles[triangleCount].keyB > clusteredTriangles[triangleCount].keyC) {
            const uint32_t swap = clusteredTriangles[triangleCount].keyB;
            clusteredTriangles[triangleCount].keyB = clusteredTriangles[triangleCount].keyC;
            clusteredTriangles[triangleCount].keyC = swap;
        }
        if (clusteredTriangles[triangleCount].keyA > clusteredTriangles[triangleCount].keyB) {
            const uint32_t swap = clusteredTriangles[triangleCount].keyA;
            clusteredTriangles[triangleCount].keyA = clusteredTriangles[triangleCount].keyB;
            clusteredTriangles[triangleCount].keyB = swap;
        }
        triangleCount++;
    }
    if (clusterCount == 0u || triangleCount == 0u) goto fail;

    qsort(clusteredTriangles,
          triangleCount,
          sizeof(clusteredTriangles[0]),
          LayoutMeshSolid_CompareClusterTriangles);
    {
        size_t uniqueCount = 0u;
        for (size_t i = 0u; i < triangleCount; ++i) {
            const bool duplicate = i > 0u &&
                clusteredTriangles[i].keyA == clusteredTriangles[i - 1u].keyA &&
                clusteredTriangles[i].keyB == clusteredTriangles[i - 1u].keyB &&
                clusteredTriangles[i].keyC == clusteredTriangles[i - 1u].keyC;
            if (duplicate) continue;
            indices[uniqueCount * 3u + 0u] = clusteredTriangles[i].a;
            indices[uniqueCount * 3u + 1u] = clusteredTriangles[i].b;
            indices[uniqueCount * 3u + 2u] = clusteredTriangles[i].c;
            uniqueCount++;
        }
        triangleCount = uniqueCount;
    }

    outLod->vertices = vertices;
    outLod->indices = indices;
    outLod->vertexCount = clusterCount;
    outLod->triangleCount = triangleCount;
    outLod->sourceVertexCount = document->vertex_count;
    outLod->sourceTriangleCount = document->triangle_count;
    outLod->clusterResolution = resolution;
    free(cellToCluster);
    free(vertexMap);
    free(sums);
    free(counts);
    free(clusteredTriangles);
    return true;

fail:
    free(cellToCluster);
    free(vertexMap);
    free(sums);
    free(counts);
    free(vertices);
    free(indices);
    free(clusteredTriangles);
    return false;
}

// Select the most detailed clustered surface that stays within the triangle budget.
bool Layout_MeshSolidPreviewBuildLod(const CoreMeshAssetRuntimeDocument* document,
                                     size_t targetTriangles,
                                     LayoutMeshSolidPreviewLod* outLod) {
    int low = 2;
    int high = LD_MESH_SOLID_MAX_CLUSTER_RESOLUTION;
    LayoutMeshSolidPreviewLod best = {0};
    LayoutMeshSolidPreviewLod smallest = {0};
    if (!document || !outLod || targetTriangles == 0u) return false;
    memset(outLod, 0, sizeof(*outLod));
    if (core_mesh_asset_runtime_document_validate(document).code != CORE_OK) return false;
    if (document->triangle_count <= targetTriangles) {
        return LayoutMeshSolid_CopyRuntimeMesh(document, outLod);
    }

    while (low <= high) {
        const int resolution = low + ((high - low) / 2);
        LayoutMeshSolidPreviewLod candidate = {0};
        if (!LayoutMeshSolid_BuildClusteredAtResolution(document, resolution, &candidate)) {
            Layout_MeshSolidPreviewFreeLod(&best);
            Layout_MeshSolidPreviewFreeLod(&smallest);
            return false;
        }
        if (candidate.triangleCount <= targetTriangles) {
            if (candidate.triangleCount > best.triangleCount) {
                Layout_MeshSolidPreviewFreeLod(&best);
                best = candidate;
                memset(&candidate, 0, sizeof(candidate));
            }
            low = resolution + 1;
        } else {
            if (smallest.triangleCount == 0u || candidate.triangleCount < smallest.triangleCount) {
                Layout_MeshSolidPreviewFreeLod(&smallest);
                smallest = candidate;
                memset(&candidate, 0, sizeof(candidate));
            }
            high = resolution - 1;
        }
        Layout_MeshSolidPreviewFreeLod(&candidate);
    }

    if (best.triangleCount > 0u) {
        Layout_MeshSolidPreviewFreeLod(&smallest);
        *outLod = best;
        return true;
    }
    if (smallest.triangleCount > 0u) {
        *outLod = smallest;
        return true;
    }
    return false;
}

// Hash raw state bytes into the frame cache signature.
static uint64_t LayoutMeshSolid_HashBytes(uint64_t hash, const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    for (size_t i = 0u; i < size; ++i) {
        hash ^= (uint64_t)bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

// Build a deterministic signature for authoritative object geometry. Selection
// and hover are viewport overlays and intentionally stay out of this cache key.
static uint64_t LayoutMeshSolid_ObjectSignature(const Layout* layout) {
    uint64_t hash = 1469598103934665603ull;
    hash = LayoutMeshSolid_HashBytes(hash,
                                    &layout->objectStore.count,
                                    sizeof(layout->objectStore.count));
    for (size_t i = 0u; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        char resolvedPath[LINE_DRAWING_PATH_CAP];
        if (object->isDeleted || object->kind != OBJECT3D_KIND_MESH_ASSET_INSTANCE) continue;
        hash = LayoutMeshSolid_HashBytes(hash, &object->objectId, sizeof(object->objectId));
        hash = LayoutMeshSolid_HashBytes(hash, &object->transform, sizeof(object->transform));
        hash = LayoutMeshSolid_HashBytes(hash,
                                        object->meshInstance.runtimePath,
                                        strlen(object->meshInstance.runtimePath));
        if (Layout_MeshAssetResolveRuntimePath(object->meshInstance.runtimePath,
                                               resolvedPath,
                                               sizeof(resolvedPath)) != LAYOUT_MESH_PATH_MISSING) {
            hash = LayoutMeshSolid_HashBytes(hash, resolvedPath, strlen(resolvedPath));
        }
    }
    return hash;
}

// Build the complete screen-projection signature. A changed projection must be
// rerasterized, but it does not necessarily require a reduced mesh LOD.
static uint64_t LayoutMeshSolid_SurfaceSignature(uint64_t objectSignature,
                                                 const SpaceViewContext* viewContext,
                                                 const Grid* grid,
                                                 SDL_Rect clip) {
    uint64_t hash = 1469598103934665603ull;
    hash = LayoutMeshSolid_HashBytes(hash, &objectSignature, sizeof(objectSignature));
    hash = LayoutMeshSolid_HashBytes(hash, viewContext, sizeof(*viewContext));
    hash = LayoutMeshSolid_HashBytes(hash, grid, sizeof(*grid));
    return LayoutMeshSolid_HashBytes(hash, &clip, sizeof(clip));
}

// Build a separate surface-style signature. Appearance changes reraster at the
// current quality tier instead of pretending the camera or mesh moved.
static uint64_t LayoutMeshSolid_AppearanceSignature(LayoutMeshSolidPreviewStyle style) {
    uint64_t hash = 1469598103934665603ull;
    return LayoutMeshSolid_HashBytes(hash, &style, sizeof(style));
}

// Release both quality levels for one cached runtime asset.
static void LayoutMeshSolid_ClearAsset(LayoutMeshSolidPreviewAssetCache* asset) {
    if (!asset) return;
    Layout_MeshSolidPreviewFreeLod(&asset->interactive);
    Layout_MeshSolidPreviewFreeLod(&asset->settled);
    memset(asset, 0, sizeof(*asset));
}

// Load or retrieve the two coherent LODs for one runtime mesh path.
static LayoutMeshSolidPreviewAssetCache* LayoutMeshSolid_AssetForPath(const char* runtimePath) {
    struct stat info;
    LayoutMeshSolidPreviewAssetCache* slot = NULL;
    CoreMeshAssetRuntimeDocument document;
    if (!runtimePath || !runtimePath[0] || stat(runtimePath, &info) != 0) return NULL;

    for (size_t i = 0u; i < LD_MESH_SOLID_ASSET_CACHE_CAPACITY; ++i) {
        LayoutMeshSolidPreviewAssetCache* candidate = &g_solidPreview.assets[i];
        if (!candidate->populated || strcmp(candidate->runtimePath, runtimePath) != 0) continue;
        if (candidate->fileSize == info.st_size && candidate->modifiedTime == info.st_mtime) {
            return candidate->valid ? candidate : NULL;
        }
        slot = candidate;
        break;
    }
    if (!slot) {
        for (size_t i = 0u; i < LD_MESH_SOLID_ASSET_CACHE_CAPACITY; ++i) {
            if (!g_solidPreview.assets[i].populated) {
                slot = &g_solidPreview.assets[i];
                break;
            }
        }
    }
    if (!slot) {
        slot = &g_solidPreview.assets[g_solidPreview.nextAssetSlot];
        g_solidPreview.nextAssetSlot =
            (g_solidPreview.nextAssetSlot + 1u) % LD_MESH_SOLID_ASSET_CACHE_CAPACITY;
    }

    LayoutMeshSolid_ClearAsset(slot);
    slot->populated = true;
    slot->fileSize = info.st_size;
    slot->modifiedTime = info.st_mtime;
    snprintf(slot->runtimePath, sizeof(slot->runtimePath), "%s", runtimePath);
    core_mesh_asset_runtime_document_init(&document);
    if (core_mesh_asset_runtime_document_load_file(runtimePath, &document).code != CORE_OK) {
        core_mesh_asset_runtime_document_free(&document);
        return NULL;
    }
    slot->valid = Layout_MeshSolidPreviewBuildLod(&document,
                                                  LD_MESH_SOLID_INTERACTIVE_TRIANGLES,
                                                  &slot->interactive) &&
                  Layout_MeshSolidPreviewBuildLod(&document,
                                                  LD_MESH_SOLID_SETTLED_TRIANGLES,
                                                  &slot->settled);
    core_mesh_asset_runtime_document_free(&document);
    if (!slot->valid) {
        Layout_MeshSolidPreviewFreeLod(&slot->interactive);
        Layout_MeshSolidPreviewFreeLod(&slot->settled);
        return NULL;
    }
    return slot;
}

// Resolve view depth in the same orthographic basis used by the viewport projection.
static float LayoutMeshSolid_ViewDepth(Vec3 point, const SpaceViewContext* viewContext) {
    if (SpaceAdapter_IsFreeViewEnabled(viewContext)) {
        return Vec3_Dot(Vec3_Sub(point, viewContext->camera.target),
                        FreeView_Forward(&viewContext->camera));
    }
    switch (viewContext->plane.axis) {
        case VIEW_PLANE_YZ: return point.x;
        case VIEW_PLANE_XZ: return point.y;
        case VIEW_PLANE_XY:
        default: return point.z;
    }
}

// Convert one transformed world point into the current reduced-resolution raster target.
static LayoutMeshSolidScreenVertex LayoutMeshSolid_ProjectVertex(
    Vec3 world,
    const SpaceViewContext* viewContext,
    const Grid* grid,
    SDL_Rect clip,
    float rasterScale) {
    const Vec2 screen = WorldToScreen(SpaceAdapter_ProjectToView(world, viewContext), grid);
    return (LayoutMeshSolidScreenVertex){
        (screen.x - (float)clip.x) * rasterScale,
        (screen.y - (float)clip.y) * rasterScale,
        LayoutMeshSolid_ViewDepth(world, viewContext)
    };
}

// Evaluate a signed edge function for barycentric triangle coverage.
static float LayoutMeshSolid_Edge(float ax, float ay, float bx, float by, float px, float py) {
    return ((px - ax) * (by - ay)) - ((py - ay) * (bx - ax));
}

// Convert one lit floating channel into a byte.
static uint8_t LayoutMeshSolid_Channel(float value) {
    if (value < 0.0f) value = 0.0f;
    if (value > 255.0f) value = 255.0f;
    return (uint8_t)lroundf(value);
}

// Rasterize one transformed indexed LOD with a CPU depth test and flat face lighting.
static void LayoutMeshSolid_RasterizeObject(const Object3D* object,
                                            const LayoutMeshSolidPreviewLod* lod,
                                            const SpaceViewContext* viewContext,
                                            const Grid* grid,
                                            SDL_Rect clip,
                                            float rasterScale,
                                            int width,
                                            int height,
                                            bool materialMode,
                                            uint8_t* rgba,
                                            float* depth,
                                            LayoutMeshSolidPreviewFrameStats* stats) {
    const Vec3 lightDirection = Vec3_Normalize((Vec3){0.38f, -0.42f, -0.82f});
    if (!object || !lod || !viewContext || !grid || !rgba || !depth || !stats) return;

    for (size_t triangleIndex = 0u; triangleIndex < lod->triangleCount; ++triangleIndex) {
        const uint32_t ia = lod->indices[triangleIndex * 3u + 0u];
        const uint32_t ib = lod->indices[triangleIndex * 3u + 1u];
        const uint32_t ic = lod->indices[triangleIndex * 3u + 2u];
        Vec3 worldA = {0};
        Vec3 worldB = {0};
        Vec3 worldC = {0};
        LayoutMeshSolidScreenVertex a = {0};
        LayoutMeshSolidScreenVertex b = {0};
        LayoutMeshSolidScreenVertex c = {0};
        Vec3 normal = {0};
        float area = 0.0f;
        float light = 0.0f;
        float baseR = materialMode ? 177.0f : 104.0f;
        float baseG = materialMode ? 128.0f : 166.0f;
        float baseB = materialMode ? 210.0f : 218.0f;
        uint8_t alpha = 255u;
        int minX = 0;
        int maxX = 0;
        int minY = 0;
        int maxY = 0;

        stats->submittedTriangles++;
        if (ia >= lod->vertexCount || ib >= lod->vertexCount || ic >= lod->vertexCount) continue;
        worldA = Layout_Transform3D_ApplyLocalPoint(object->transform, lod->vertices[ia]);
        worldB = Layout_Transform3D_ApplyLocalPoint(object->transform, lod->vertices[ib]);
        worldC = Layout_Transform3D_ApplyLocalPoint(object->transform, lod->vertices[ic]);
        a = LayoutMeshSolid_ProjectVertex(worldA, viewContext, grid, clip, rasterScale);
        b = LayoutMeshSolid_ProjectVertex(worldB, viewContext, grid, clip, rasterScale);
        c = LayoutMeshSolid_ProjectVertex(worldC, viewContext, grid, clip, rasterScale);
        area = LayoutMeshSolid_Edge(a.x, a.y, b.x, b.y, c.x, c.y);
        if (!isfinite(area) || fabsf(area) <= 1e-5f) continue;

        minX = (int)floorf(fminf(a.x, fminf(b.x, c.x)));
        maxX = (int)ceilf(fmaxf(a.x, fmaxf(b.x, c.x)));
        minY = (int)floorf(fminf(a.y, fminf(b.y, c.y)));
        maxY = (int)ceilf(fmaxf(a.y, fmaxf(b.y, c.y)));
        if (maxX < 0 || maxY < 0 || minX >= width || minY >= height) continue;
        if (minX < 0) minX = 0;
        if (minY < 0) minY = 0;
        if (maxX >= width) maxX = width - 1;
        if (maxY >= height) maxY = height - 1;

        normal = Vec3_Normalize(Vec3_Cross(Vec3_Sub(worldB, worldA), Vec3_Sub(worldC, worldA)));
        light = 0.36f + (0.64f * fabsf(Vec3_Dot(normal, lightDirection)));
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const float px = (float)x + 0.5f;
                const float py = (float)y + 0.5f;
                const float w0 = LayoutMeshSolid_Edge(b.x, b.y, c.x, c.y, px, py) / area;
                const float w1 = LayoutMeshSolid_Edge(c.x, c.y, a.x, a.y, px, py) / area;
                const float w2 = 1.0f - w0 - w1;
                const size_t pixel = (size_t)y * (size_t)width + (size_t)x;
                float pixelDepth = 0.0f;
                if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) continue;
                pixelDepth = (w0 * a.depth) + (w1 * b.depth) + (w2 * c.depth);
                if (!isfinite(pixelDepth) || pixelDepth >= depth[pixel]) continue;
                depth[pixel] = pixelDepth;
                rgba[pixel * 4u + 0u] = LayoutMeshSolid_Channel(baseR * light);
                rgba[pixel * 4u + 1u] = LayoutMeshSolid_Channel(baseG * light);
                rgba[pixel * 4u + 2u] = LayoutMeshSolid_Channel(baseB * light);
                rgba[pixel * 4u + 3u] = alpha;
            }
        }
        stats->rasterizedTriangles++;
    }
}

// Mark screen-space coverage boundaries and meaningful depth discontinuities as silhouette pixels.
static size_t LayoutMeshSolid_ApplyOutline(uint8_t* rgba,
                                           const float* depth,
                                           int width,
                                           int height,
                                           uint8_t outlineR,
                                           uint8_t outlineG,
                                           uint8_t outlineB,
                                           uint8_t outlineA,
                                           bool outlineOnly) {
    uint8_t* boundary = NULL;
    size_t count = 0u;
    if (!rgba || !depth || width <= 2 || height <= 2) return 0u;
    boundary = (uint8_t*)calloc((size_t)width * (size_t)height, sizeof(uint8_t));
    if (!boundary) return 0u;

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            const size_t pixel = (size_t)y * (size_t)width + (size_t)x;
            static const int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            if (rgba[pixel * 4u + 3u] == 0u) continue;
            for (size_t n = 0u; n < 4u; ++n) {
                const size_t neighbor = (size_t)(y + offsets[n][1]) * (size_t)width +
                                        (size_t)(x + offsets[n][0]);
                const bool empty = rgba[neighbor * 4u + 3u] == 0u;
                const float threshold = 0.18f * (1.0f + fabsf(depth[pixel]));
                const bool depthEdge = !empty &&
                    isfinite(depth[neighbor]) &&
                    fabsf(depth[pixel] - depth[neighbor]) > threshold;
                if (empty || depthEdge) {
                    boundary[pixel] = 1u;
                    break;
                }
            }
        }
    }

    for (size_t pixel = 0u; pixel < (size_t)width * (size_t)height; ++pixel) {
        if (boundary[pixel]) {
            rgba[pixel * 4u + 0u] = outlineR;
            rgba[pixel * 4u + 1u] = outlineG;
            rgba[pixel * 4u + 2u] = outlineB;
            rgba[pixel * 4u + 3u] = outlineA;
            count++;
        } else if (outlineOnly) {
            rgba[pixel * 4u + 0u] = 0u;
            rgba[pixel * 4u + 1u] = 0u;
            rgba[pixel * 4u + 2u] = 0u;
            rgba[pixel * 4u + 3u] = 0u;
        }
    }
    free(boundary);
    return count;
}

size_t Layout_MeshSolidPreviewApplySilhouette(uint8_t* rgba,
                                              const float* depth,
                                              int width,
                                              int height,
                                              uint8_t outlineR,
                                              uint8_t outlineG,
                                              uint8_t outlineB,
                                              uint8_t outlineA) {
    return LayoutMeshSolid_ApplyOutline(rgba,
                                       depth,
                                       width,
                                       height,
                                       outlineR,
                                       outlineG,
                                       outlineB,
                                       outlineA,
                                       false);
}

size_t Layout_MeshSolidPreviewApplyOutlineOnly(uint8_t* rgba,
                                              const float* depth,
                                              int width,
                                              int height,
                                              uint8_t outlineR,
                                              uint8_t outlineG,
                                              uint8_t outlineB,
                                              uint8_t outlineA) {
    return LayoutMeshSolid_ApplyOutline(rgba,
                                       depth,
                                       width,
                                       height,
                                       outlineR,
                                       outlineG,
                                       outlineB,
                                       outlineA,
                                       true);
}

// Resize and clear the shared CPU color/depth buffers for one viewport pass.
static bool LayoutMeshSolid_PrepareBuffers(int width, int height) {
    const size_t pixels = (size_t)width * (size_t)height;
    if (width <= 0 || height <= 0 || pixels > SIZE_MAX / 4u) return false;
    if (g_solidPreview.rasterWidth != width || g_solidPreview.rasterHeight != height) {
        uint8_t* rgba = (uint8_t*)malloc(pixels * 4u);
        float* depth = (float*)malloc(pixels * sizeof(float));
        if (!rgba || !depth) {
            free(rgba);
            free(depth);
            return false;
        }
        free(g_solidPreview.rgba);
        free(g_solidPreview.depth);
        g_solidPreview.rgba = rgba;
        g_solidPreview.depth = depth;
        g_solidPreview.rasterWidth = width;
        g_solidPreview.rasterHeight = height;
    }
    memset(g_solidPreview.rgba, 0, pixels * 4u);
    for (size_t i = 0u; i < pixels; ++i) g_solidPreview.depth[i] = INFINITY;
    return true;
}

// Upload or update the cached reduced-resolution texture after rasterization.
static bool LayoutMeshSolid_UpdateTexture(VkRenderer* renderer, int width, int height) {
    VkResult result = VK_SUCCESS;
    if (!renderer || !g_solidPreview.rgba) return false;
    if (g_solidPreview.textureValid &&
        (g_solidPreview.texture.width != (uint32_t)width ||
         g_solidPreview.texture.height != (uint32_t)height)) {
        vk_renderer_wait_idle(renderer);
        vk_renderer_texture_destroy(renderer, &g_solidPreview.texture);
        memset(&g_solidPreview.texture, 0, sizeof(g_solidPreview.texture));
        g_solidPreview.textureValid = false;
    }
    if (!g_solidPreview.textureValid) {
        result = vk_renderer_texture_create_from_rgba(renderer,
                                                      g_solidPreview.rgba,
                                                      (uint32_t)width,
                                                      (uint32_t)height,
                                                      VK_FILTER_LINEAR,
                                                      &g_solidPreview.texture);
        g_solidPreview.textureValid = result == VK_SUCCESS;
        return g_solidPreview.textureValid;
    }
    result = vk_renderer_texture_update_rgba_subrect(renderer,
                                                     &g_solidPreview.texture,
                                                     g_solidPreview.rgba,
                                                     (size_t)width * 4u,
                                                     0u,
                                                     0u,
                                                     (uint32_t)width,
                                                     (uint32_t)height);
    return result == VK_SUCCESS;
}

// Keep navigation responsive, then promote to the settled LOD after a short stable interval.
bool Layout_MeshSolidPreviewUsesInteractiveQuality(uint64_t nowNs,
                                                   uint64_t signatureChangedAtNs) {
    return nowNs != 0u &&
           core_time_diff_ns(nowNs, signatureChangedAtNs) < LD_MESH_SOLID_SETTLE_NS;
}

// Keep appearance and editor-overlay invalidation independent from LOD quality.
bool Layout_MeshSolidPreviewInvalidationResetsQuality(
    LayoutMeshSolidPreviewInvalidation invalidation) {
    return (invalidation & LAYOUT_MESH_SOLID_INVALIDATION_GEOMETRY) != 0;
}

bool Layout_MeshSolidPreviewViewChangeResetsQuality(
    const SpaceViewContext* previous,
    const SpaceViewContext* current) {
    if (!previous || !current) return true;
    if (previous->camera.enabled != current->camera.enabled) return true;
    if (current->camera.enabled) {
        return previous->camera.yawDeg != current->camera.yawDeg ||
               previous->camera.pitchDeg != current->camera.pitchDeg;
    }
    return previous->plane.axis != current->plane.axis;
}

// Render the mesh scene only when its state or adaptive quality level changes, then reuse its texture.
bool Layout_RenderMeshSolidPreview(SDL_Renderer* renderer,
                                   const Layout* layout,
                                   const SpaceViewContext* viewContext,
                                   const Grid* grid,
                                   int screenWidth,
                                   int screenHeight,
                                   LayoutMeshSolidPreviewStyle style,
                                   LayoutMeshSolidPreviewFrameStats* outStats) {
    SDL_Rect clip = {0, 0, screenWidth, screenHeight};
    const CoreTimeNs now = core_time_now_ns();
    uint64_t surfaceSignature = 0u;
    uint64_t objectSignature = 0u;
    uint64_t appearanceSignature = 0u;
    bool surfaceChanged = false;
    bool objectChanged = false;
    bool viewQualityChanged = false;
    bool appearanceChanged = false;
    LayoutMeshSolidPreviewInvalidation invalidation =
        LAYOUT_MESH_SOLID_INVALIDATION_NONE;
    bool interactive = false;
    bool needsRaster = false;
    float rasterScale = LD_MESH_SOLID_SETTLED_SCALE;
    int rasterWidth = 0;
    int rasterHeight = 0;
    VkRenderer* vk = (VkRenderer*)renderer;
    const bool materialMode = style == LAYOUT_MESH_SOLID_STYLE_MATERIAL;
    const bool outlineOnly = style == LAYOUT_MESH_SOLID_STYLE_WIRE_OUTLINE;

    if (outStats) memset(outStats, 0, sizeof(*outStats));
    if (!renderer || !layout || !viewContext || !grid || screenWidth <= 0 || screenHeight <= 0) {
        return false;
    }
    if (SDL_RenderIsClipEnabled(renderer)) SDL_RenderGetClipRect(renderer, &clip);
    if (clip.w <= 0 || clip.h <= 0) return false;

    objectSignature = LayoutMeshSolid_ObjectSignature(layout);
    surfaceSignature = LayoutMeshSolid_SurfaceSignature(objectSignature,
                                                        viewContext,
                                                        grid,
                                                        clip);
    appearanceSignature = LayoutMeshSolid_AppearanceSignature(style);
    surfaceChanged = !g_solidPreview.surfaceSignatureValid ||
                     surfaceSignature != g_solidPreview.surfaceSignature;
    objectChanged = !g_solidPreview.objectSignatureValid ||
                    objectSignature != g_solidPreview.objectSignature;
    viewQualityChanged = !g_solidPreview.qualityViewContextValid ||
                         Layout_MeshSolidPreviewViewChangeResetsQuality(
                             &g_solidPreview.qualityViewContext,
                             viewContext);
    appearanceChanged = !g_solidPreview.appearanceSignatureValid ||
                        appearanceSignature != g_solidPreview.appearanceSignature;
    if (surfaceChanged) invalidation |= LAYOUT_MESH_SOLID_INVALIDATION_PROJECTION;
    if (objectChanged || viewQualityChanged) {
        invalidation |= LAYOUT_MESH_SOLID_INVALIDATION_GEOMETRY;
    }
    if (appearanceChanged) invalidation |= LAYOUT_MESH_SOLID_INVALIDATION_APPEARANCE;
    g_solidPreview.surfaceSignature = surfaceSignature;
    g_solidPreview.surfaceSignatureValid = true;
    g_solidPreview.objectSignature = objectSignature;
    g_solidPreview.objectSignatureValid = true;
    g_solidPreview.qualityViewContext = *viewContext;
    g_solidPreview.qualityViewContextValid = true;
    if (Layout_MeshSolidPreviewInvalidationResetsQuality(invalidation)) {
        g_solidPreview.qualityChangedAt = now;
    }
    if (appearanceChanged) {
        g_solidPreview.appearanceSignature = appearanceSignature;
        g_solidPreview.appearanceSignatureValid = true;
    }
    interactive = Layout_MeshSolidPreviewUsesInteractiveQuality(
        now,
        g_solidPreview.qualityChangedAt);
    needsRaster = surfaceChanged || appearanceChanged || !g_solidPreview.pixelsValid ||
                  interactive != g_solidPreview.renderedInteractive;

    if (needsRaster) {
        LayoutMeshSolidPreviewFrameStats stats = {0};
        rasterScale = interactive ? LD_MESH_SOLID_INTERACTIVE_SCALE
                                  : LD_MESH_SOLID_SETTLED_SCALE;
        rasterWidth = (int)ceilf((float)clip.w * rasterScale);
        rasterHeight = (int)ceilf((float)clip.h * rasterScale);
        if (!LayoutMeshSolid_PrepareBuffers(rasterWidth, rasterHeight)) return false;
        stats.interactiveQuality = interactive;

        for (size_t i = 0u; i < layout->objectStore.count; ++i) {
            const Object3D* object = &layout->objectStore.items[i];
            LayoutMeshSolidPreviewAssetCache* asset = NULL;
            const LayoutMeshSolidPreviewLod* lod = NULL;
            if (object->isDeleted || object->kind != OBJECT3D_KIND_MESH_ASSET_INSTANCE ||
                !object->meshInstance.runtimePath[0]) {
                continue;
            }
            char resolvedPath[LINE_DRAWING_PATH_CAP];
            if (Layout_MeshAssetResolveRuntimePath(object->meshInstance.runtimePath,
                                                   resolvedPath,
                                                   sizeof(resolvedPath)) == LAYOUT_MESH_PATH_MISSING) {
                continue;
            }
            asset = LayoutMeshSolid_AssetForPath(resolvedPath);
            if (!asset) continue;
            lod = interactive ? &asset->interactive : &asset->settled;
            LayoutMeshSolid_RasterizeObject(object,
                                            lod,
                                            viewContext,
                                            grid,
                                            clip,
                                            rasterScale,
                                            rasterWidth,
                                            rasterHeight,
                                            materialMode,
                                            g_solidPreview.rgba,
                                            g_solidPreview.depth,
                                            &stats);
            stats.meshCount++;
        }

        for (size_t pixel = 0u; pixel < (size_t)rasterWidth * (size_t)rasterHeight; ++pixel) {
            if (g_solidPreview.rgba[pixel * 4u + 3u] != 0u) stats.coveredPixels++;
        }
        if (outlineOnly) {
            stats.silhouettePixels = Layout_MeshSolidPreviewApplyOutlineOnly(
                g_solidPreview.rgba,
                g_solidPreview.depth,
                rasterWidth,
                rasterHeight,
                118u,
                190u,
                240u,
                248u);
        } else {
            stats.silhouettePixels = Layout_MeshSolidPreviewApplySilhouette(
                g_solidPreview.rgba,
                g_solidPreview.depth,
                rasterWidth,
                rasterHeight,
                materialMode ? 92u : 56u,
                materialMode ? 58u : 92u,
                materialMode ? 118u : 122u,
                248u);
        }
        if (!LayoutMeshSolid_UpdateTexture(vk, rasterWidth, rasterHeight)) return false;
        g_solidPreview.renderer = vk;
        g_solidPreview.lastStats = stats;
        g_solidPreview.renderedInteractive = interactive;
        g_solidPreview.pixelsValid = true;
    }

    if (!g_solidPreview.textureValid || g_solidPreview.lastStats.meshCount == 0u) return false;
    vk_renderer_draw_texture(vk, &g_solidPreview.texture, NULL, &clip);
    if (outStats) *outStats = g_solidPreview.lastStats;
    return true;
}

// Tear down every cache owned by the app-local solid preview renderer.
void Layout_MeshSolidPreviewShutdown(SDL_Renderer* renderer) {
    VkRenderer* vk = (VkRenderer*)renderer;
    for (size_t i = 0u; i < LD_MESH_SOLID_ASSET_CACHE_CAPACITY; ++i) {
        LayoutMeshSolid_ClearAsset(&g_solidPreview.assets[i]);
    }
    if (g_solidPreview.textureValid && vk) {
        vk_renderer_wait_idle(vk);
        vk_renderer_texture_destroy(vk, &g_solidPreview.texture);
    }
    free(g_solidPreview.rgba);
    free(g_solidPreview.depth);
    memset(&g_solidPreview, 0, sizeof(g_solidPreview));
}
