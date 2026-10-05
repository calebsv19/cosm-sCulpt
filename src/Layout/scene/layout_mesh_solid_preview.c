#include "Layout/layout_engineering.h"
#include "Layout/layout_section.h"
#include "Core/global_state.h"
#include "Layout/scene/layout_mesh_solid_preview.h"
#include "Layout/scene/layout_mesh_asset_path_resolver.h"

#include "Core/data_paths.h"
#include "Math/math_util.h"
#include "core_time.h"
#include "kit_viewport3d.h"
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
    int32_t* owner;
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
    SDL_Rect clip;
    float rasterScale;
} LayoutMeshSolidPreviewCache;

typedef struct {
    float x;
    float y;
    float depth;
} LayoutMeshSolidScreenVertex;

static LayoutMeshSolidPreviewCache g_solidPreview;

// Keep the established app-local entry points as thin shared-core adapters.
void Layout_MeshSolidPreviewFreeLod(LayoutMeshSolidPreviewLod* lod) {
    core_mesh_preview_lod_mesh_free(lod);
}

bool Layout_MeshSolidPreviewBuildLod(const CoreMeshAssetRuntimeDocument* document,
                                     size_t targetTriangles,
                                     LayoutMeshSolidPreviewLod* outLod) {
    return core_mesh_preview_build_lod_mesh(document, targetTriangles, outLod).code == CORE_OK;
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
    hash = LayoutMeshSolid_HashBytes(hash, &layout->metersPerWorldUnit, sizeof(layout->metersPerWorldUnit));
    hash = LayoutMeshSolid_HashBytes(hash,
                                    &layout->objectStore.count,
                                    sizeof(layout->objectStore.count));
    for (size_t i = 0u; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        char resolvedPath[LINE_DRAWING_PATH_CAP];
        if (!Layout_ObjectShown(&layout->objectStore,object) || object->info.volume_role != LAYOUT_VOLUME_NONE) continue;
        hash = LayoutMeshSolid_HashBytes(hash, &object->kind, sizeof(object->kind));
        hash = LayoutMeshSolid_HashBytes(hash, &object->plane, sizeof(object->plane));
        hash = LayoutMeshSolid_HashBytes(hash, &object->rectPrism, sizeof(object->rectPrism));
        hash = LayoutMeshSolid_HashBytes(hash, &object->info, sizeof(object->info));
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
    hash = LayoutMeshSolid_HashBytes(hash, &clip, sizeof(clip));
    const GlobalState* state = Global_Get();
    if (state) {
        hash = LayoutMeshSolid_HashBytes(hash, &state->sectionView, sizeof(state->sectionView));
        hash = LayoutMeshSolid_HashBytes(hash, &state->workspaceMode, sizeof(state->workspaceMode));
    }
    return hash;
}

// Build a separate surface-style signature. Appearance changes reraster at the
// current quality tier instead of pretending the camera or mesh moved.
static uint64_t LayoutMeshSolid_AppearanceSignature(LayoutMeshSolidPreviewStyle style) {
    uint64_t hash = 1469598103934665603ull;
    hash = LayoutMeshSolid_HashBytes(hash, &style, sizeof(style));
    const GlobalState* state = Global_Get();
    if (state) {
        hash = LayoutMeshSolid_HashBytes(hash, &state->editor.selectedObject3DId, sizeof(state->editor.selectedObject3DId));
        hash = LayoutMeshSolid_HashBytes(hash, &state->editor.hoveredObject3DId, sizeof(state->editor.hoveredObject3DId));
    }
    return hash;
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

// Native primitives and runtime meshes share one color/depth/owner pass.
static void LayoutMeshSolid_RasterizeTriangle(Vec3 worldA, Vec3 worldB, Vec3 worldC,
    const SpaceViewContext* viewContext, const Grid* grid, SDL_Rect clip, float rasterScale,
    int width, int height, int32_t ownerId, SDL_Color color, bool cap,
    uint8_t* rgba, float* depth, int32_t* owner, LayoutMeshSolidPreviewFrameStats* stats) {
    const Vec3 lightDirection = Vec3_Normalize((Vec3){0.38f, -0.42f, -0.82f});
    LayoutMeshSolidScreenVertex a={0}, b={0}, c={0};
    Vec3 normal={0};
    float area=0, light=0, baseR=color.r, baseG=color.g, baseB=color.b;
    uint8_t alpha=255;
    int minX=0,maxX=0,minY=0,maxY=0;
    stats->submittedTriangles++;
        a = LayoutMeshSolid_ProjectVertex(worldA, viewContext, grid, clip, rasterScale);
        b = LayoutMeshSolid_ProjectVertex(worldB, viewContext, grid, clip, rasterScale);
        c = LayoutMeshSolid_ProjectVertex(worldC, viewContext, grid, clip, rasterScale);
        area = LayoutMeshSolid_Edge(a.x, a.y, b.x, b.y, c.x, c.y);
        if (!isfinite(area) || fabsf(area) <= 1e-5f) return;

        minX = (int)floorf(fminf(a.x, fminf(b.x, c.x)));
        maxX = (int)ceilf(fmaxf(a.x, fmaxf(b.x, c.x)));
        minY = (int)floorf(fminf(a.y, fminf(b.y, c.y)));
        maxY = (int)ceilf(fmaxf(a.y, fmaxf(b.y, c.y)));
        if (maxX < 0 || maxY < 0 || minX >= width || minY >= height) return;
        if (minX < 0) minX = 0;
        if (minY < 0) minY = 0;
        if (maxX >= width) maxX = width - 1;
        if (maxY >= height) maxY = height - 1;

        normal = Vec3_Normalize(Vec3_Cross(Vec3_Sub(worldB, worldA), Vec3_Sub(worldC, worldA)));
        light = cap ? 1.0f : 0.48f + (0.52f * fabsf(Vec3_Dot(normal, lightDirection)));
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
                owner[pixel] = ownerId;
                rgba[pixel * 4u + 0u] = LayoutMeshSolid_Channel(baseR * light);
                rgba[pixel * 4u + 1u] = LayoutMeshSolid_Channel(baseG * light);
                rgba[pixel * 4u + 2u] = LayoutMeshSolid_Channel(baseB * light);
                rgba[pixel * 4u + 3u] = alpha;
            }
        }
    stats->rasterizedTriangles++;
}
static SDL_Color LayoutMeshSolid_Color(const Object3D* object, bool material) {
    if (!material) return (SDL_Color){164,185,202,255};
    for (size_t i=0;i<object->info.property_count;++i) {
        const LayoutProperty* p=&object->info.properties[i];
        if (strcmp(p->key,"material") || p->kind!=LAYOUT_PROPERTY_TEXT) continue;
        if (strstr(p->text,"plywood") || strstr(p->text,"wood")) return (SDL_Color){207,173,119,255};
        if (strstr(p->text,"steel") || strstr(p->text,"aluminum")) return (SDL_Color){174,185,192,255};
        if (strstr(p->text,"foam")) return (SDL_Color){164,189,150,255};
    }
    return (SDL_Color){148,168,183,255}; /* Unknown material stays neutral. */
}
bool Layout_RasterNativeSurfaces(const Layout* layout, const LayoutSectionView* section,
    const SpaceViewContext* view, const Grid* grid, SDL_Rect clip, float raster_scale,
    int width, int height, bool material, uint8_t* rgba, float* depth, int32_t* owner,
    LayoutMeshSolidPreviewFrameStats* stats) {
    if(!layout || !view || !grid || !rgba || !depth || !owner || !stats ||
        width<=0 || height<=0 || !isfinite(raster_scale) || raster_scale<=0) return false;
    for(size_t i=0;i<layout->objectStore.count;++i) {
        const Object3D* object=&layout->objectStore.items[i];
        if(!Layout_ObjectShown(&layout->objectStore,object) || object->info.volume_role!=LAYOUT_VOLUME_NONE ||
            object->kind==OBJECT3D_KIND_MESH_ASSET_INSTANCE)continue;
        LayoutSurfaceTriangle triangles[LAYOUT_SURFACE_MAX_TRIANGLES];
        size_t n=Layout_BuildNativeSurface(object,section,Layout_WorldScale(layout),triangles);
        SDL_Color color=LayoutMeshSolid_Color(object,material);
        for(size_t t=0;t<n;++t) LayoutMeshSolid_RasterizeTriangle(triangles[t].a,triangles[t].b,triangles[t].c,
            view,grid,clip,raster_scale,width,height,(int32_t)i,color,triangles[t].cap,rgba,depth,owner,stats);
        if(n)stats->meshCount++;
    }
    return true;
}
static void LayoutMeshSolid_RasterizeObject(const Object3D* object,
    const LayoutMeshSolidPreviewLod* lod, const SpaceViewContext* viewContext,
    const Grid* grid, SDL_Rect clip, float rasterScale, int width, int height,
    int32_t ownerId, bool materialMode, uint8_t* rgba, float* depth, int32_t* owner,
    LayoutMeshSolidPreviewFrameStats* stats) {
    if (!object || !lod) return;
    const GlobalState* state=Global_Get();
    const LayoutSectionView* section=state ? &state->sectionView : NULL;
    if (section && section->mode==LAYOUT_SECTION_EXACT) return; /* Native solids only; no mesh caps claimed. */
    SDL_Color color=LayoutMeshSolid_Color(object,materialMode);
    for(size_t i=0;i<lod->triangle_count;++i) {
        Vec3 p[3]; bool valid=true;
        for(int j=0;j<3;++j) {
            uint32_t index=lod->indices[i*3u+(size_t)j];
            if(index>=lod->vertex_count) { valid=false; break; }
            p[j]=Layout_Transform3D_ApplyLocalPoint(object->transform,
                (Vec3){(float)lod->vertices[index].x,(float)lod->vertices[index].y,(float)lod->vertices[index].z});
        }
        if(!valid) continue;
        Vec3 clipped[12]; size_t n=3; const Vec3* points=p;
        if(section && section->mode==LAYOUT_SECTION_CUTAWAY) {
            n=Layout_ClipSectionPolygon(p,3,section->axis,
                (float)(section->position_meters/Layout_WorldScale(&state->layout)),section->flipped,clipped);
            points=clipped;
        }
        for(size_t j=1;j+1<n;++j) LayoutMeshSolid_RasterizeTriangle(points[0],points[j],points[j+1],
            viewContext,grid,clip,rasterScale,width,height,ownerId,color,false,rgba,depth,owner,stats);
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
    KitViewport3dOutlinePalette palette = kit_viewport3d_outline_palette_default();
    KitViewport3dOutlineParams params;
    size_t count = 0u;
    const KitViewport3dColor color = {outlineR, outlineG, outlineB, outlineA};
    for (size_t i = 0u; i < KIT_VIEWPORT3D_OBJECT_ACCENT_CAP; ++i) {
        palette.object_accents[i] = color;
    }
    palette.selected = color;
    palette.hover = color;
    params = (KitViewport3dOutlineParams){
        .rgba = rgba,
        .depth = depth,
        .owner = NULL,
        .width = width,
        .height = height,
        .depth_format = KIT_VIEWPORT3D_DEPTH_F32,
        .relative_depth_threshold = 0.18,
        .selected_owner = -1,
        .hover_owner = -1,
        .outline_only = outlineOnly,
        .palette = &palette
    };
    if (!kit_viewport3d_apply_outline(&params, &count)) return 0u;
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
        int32_t* owner = (int32_t*)malloc(pixels * sizeof(*owner));
        if (!rgba || !depth || !owner) {
            free(rgba);
            free(depth);
            free(owner);
            return false;
        }
        free(g_solidPreview.rgba);
        free(g_solidPreview.depth);
        free(g_solidPreview.owner);
        g_solidPreview.rgba = rgba;
        g_solidPreview.depth = depth;
        g_solidPreview.owner = owner;
        g_solidPreview.rasterWidth = width;
        g_solidPreview.rasterHeight = height;
    }
    memset(g_solidPreview.rgba, 0, pixels * 4u);
    for (size_t i = 0u; i < pixels; ++i) {
        g_solidPreview.depth[i] = INFINITY;
        g_solidPreview.owner[i] = -1;
    }
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
        /* Thin panel sections need full screen resolution; mesh preview LOD stays unchanged. */
        if (Global_Get() && Global_Get()->sectionView.mode != LAYOUT_SECTION_OFF) rasterScale = 1.0f;
        rasterWidth = (int)ceilf((float)clip.w * rasterScale);
        rasterHeight = (int)ceilf((float)clip.h * rasterScale);
        if (!LayoutMeshSolid_PrepareBuffers(rasterWidth, rasterHeight)) return false;
        stats.interactiveQuality = interactive;

        if(!outlineOnly && Global_GetWorkspaceMode()==LINE_DRAWING_WORKSPACE_MODE_SCENE) {
            const GlobalState* state=Global_Get();
            (void)Layout_RasterNativeSurfaces(layout,state?&state->sectionView:NULL,viewContext,grid,
                clip,rasterScale,rasterWidth,rasterHeight,materialMode,g_solidPreview.rgba,
                g_solidPreview.depth,g_solidPreview.owner,&stats);
        }
        for (size_t i = 0u; i < layout->objectStore.count; ++i) {
            const Object3D* object = &layout->objectStore.items[i];
            LayoutMeshSolidPreviewAssetCache* asset = NULL;
            const LayoutMeshSolidPreviewLod* lod = NULL;
            if (!Layout_ObjectShown(&layout->objectStore,object) || object->kind != OBJECT3D_KIND_MESH_ASSET_INSTANCE ||
                !object->meshInstance.runtimePath[0] || object->info.volume_role != LAYOUT_VOLUME_NONE) {
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
                                            (int32_t)i,
                                            materialMode,
                                            g_solidPreview.rgba,
                                            g_solidPreview.depth,
                                            g_solidPreview.owner,
                                            &stats);
            stats.meshCount++;
        }

        for (size_t pixel = 0u; pixel < (size_t)rasterWidth * (size_t)rasterHeight; ++pixel) {
            if (g_solidPreview.rgba[pixel * 4u + 3u] != 0u) stats.coveredPixels++;
        }
        {
            int selected=-1,hovered=-1;
            const GlobalState* state=Global_Get();
            if(state)for(size_t i=0;i<layout->objectStore.count;++i) {
                if(layout->objectStore.items[i].objectId==state->editor.selectedObject3DId)selected=(int)i;
                if(layout->objectStore.items[i].objectId==state->editor.hoveredObject3DId)hovered=(int)i;
            }
            const KitViewport3dOutlineParams outlineParams = {
                .rgba = g_solidPreview.rgba,
                .depth = g_solidPreview.depth,
                .owner = g_solidPreview.owner,
                .width = rasterWidth,
                .height = rasterHeight,
                .depth_format = KIT_VIEWPORT3D_DEPTH_F32,
                .relative_depth_threshold = 0.18,
                .selected_owner = selected,
                .hover_owner = hovered,
                .outline_only = outlineOnly,
                .palette = NULL
            };
            (void)kit_viewport3d_apply_outline(&outlineParams,
                                               &stats.silhouettePixels);
        }
        if (!LayoutMeshSolid_UpdateTexture(vk, rasterWidth, rasterHeight)) return false;
        g_solidPreview.renderer = vk;
        g_solidPreview.lastStats = stats;
        g_solidPreview.renderedInteractive = interactive;
        g_solidPreview.pixelsValid = true;
        g_solidPreview.clip=clip;
        g_solidPreview.rasterScale=rasterScale;
    }

    if (!g_solidPreview.textureValid || g_solidPreview.lastStats.meshCount == 0u) return false;
    vk_renderer_draw_texture(vk, &g_solidPreview.texture, NULL, &clip);
    if (outStats) *outStats = g_solidPreview.lastStats;
    return true;
}

// Tear down every cache owned by the app-local solid preview renderer.
/* The rendered owner buffer is authoritative for solid body picking. Empty or
 * nonselectable foreground pixels must not fall through to hidden object origins. */
bool Layout_SolidPreviewPick(const Layout* layout, const SpaceViewContext* view, const Grid* grid,
    int x, int y, uint32_t* object_id) {
    if (!object_id) return false;
    *object_id=0;
    if (!g_solidPreview.pixelsValid || !layout || !view || !grid) return false;
    uint64_t signature=LayoutMeshSolid_SurfaceSignature(LayoutMeshSolid_ObjectSignature(layout),view,grid,g_solidPreview.clip);
    if (signature!=g_solidPreview.surfaceSignature) return false;
    int px=(int)floorf((x-g_solidPreview.clip.x)*g_solidPreview.rasterScale);
    int py=(int)floorf((y-g_solidPreview.clip.y)*g_solidPreview.rasterScale);
    if(px<0 || py<0 || px>=g_solidPreview.rasterWidth || py>=g_solidPreview.rasterHeight) return true;
    int32_t index=g_solidPreview.owner[(size_t)py*(size_t)g_solidPreview.rasterWidth+(size_t)px];
    if(index>=0 && (size_t)index<layout->objectStore.count) {
        const Object3D* object=&layout->objectStore.items[index];
        if(Layout_ObjectShown(&layout->objectStore,object) && object->coreMeta.flags.selectable) *object_id=object->objectId;
    }
    return true;
}

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
    free(g_solidPreview.owner);
    memset(&g_solidPreview, 0, sizeof(g_solidPreview));
}
