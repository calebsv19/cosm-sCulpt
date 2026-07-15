#include "test_framework.h"

#include "Layout/scene/layout_mesh_solid_preview.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Build a regular triangulated sheet large enough to exercise clustered LOD generation.
static bool ld_test_build_grid_mesh(CoreMeshAssetRuntimeDocument* document, size_t side) {
    const size_t vertexCount = side * side;
    const size_t triangleCount = (side - 1u) * (side - 1u) * 2u;
    if (!document || side < 3u) return false;
    core_mesh_asset_runtime_document_init(document);
    core_mesh_asset_runtime_contract_init(&document->contract);
    if (core_mesh_asset_runtime_contract_set_asset_id(&document->contract,
                                                       "solid_preview_grid").code != CORE_OK ||
        core_mesh_asset_runtime_contract_set_source_asset_id(&document->contract,
                                                              "solid_preview_grid_source").code != CORE_OK ||
        core_mesh_asset_runtime_document_set_vertex_count(document, vertexCount).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_triangle_count(document, triangleCount).code != CORE_OK ||
        core_mesh_asset_runtime_document_set_surface_group_count(document, 1u).code != CORE_OK) {
        core_mesh_asset_runtime_document_free(document);
        return false;
    }
    document->contract.asset_type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
    document->contract.local_bounds.min = (CoreObjectVec3){0.0, 0.0, 0.0};
    document->contract.local_bounds.max =
        (CoreObjectVec3){(double)(side - 1u), (double)(side - 1u), 0.0};
    document->contract.topology_closed_volume = false;
    document->contract.topology_manifold_expected = false;
    snprintf(document->surface_groups[0].group_id,
             sizeof(document->surface_groups[0].group_id),
             "surface_default");
    document->surface_groups[0].triangle_start = 0u;
    document->surface_groups[0].triangle_count = triangleCount;

    for (size_t y = 0u; y < side; ++y) {
        for (size_t x = 0u; x < side; ++x) {
            const size_t vertex = y * side + x;
            document->vertices[vertex].position =
                (CoreObjectVec3){(double)x, (double)y, 0.0};
        }
    }
    {
        size_t triangle = 0u;
        for (size_t y = 0u; y + 1u < side; ++y) {
            for (size_t x = 0u; x + 1u < side; ++x) {
                const size_t a = y * side + x;
                const size_t b = a + 1u;
                const size_t c = a + side;
                const size_t d = c + 1u;
                document->triangles[triangle++] =
                    (CoreMeshAssetRuntimeTriangle){.a = a, .b = b, .c = d};
                snprintf(document->triangles[triangle - 1u].surface_group_id,
                         sizeof(document->triangles[triangle - 1u].surface_group_id),
                         "surface_default");
                document->triangles[triangle++] =
                    (CoreMeshAssetRuntimeTriangle){.a = a, .b = d, .c = c};
                snprintf(document->triangles[triangle - 1u].surface_group_id,
                         sizeof(document->triangles[triangle - 1u].surface_group_id),
                         "surface_default");
            }
        }
    }
    return core_mesh_asset_runtime_document_validate(document).code == CORE_OK;
}

static bool test_mesh_solid_lod_preserves_coherent_indexed_surface(void) {
    CoreMeshAssetRuntimeDocument document;
    LayoutMeshSolidPreviewLod lod = {0};
    TEST_ASSERT(ld_test_build_grid_mesh(&document, 33u));
    TEST_ASSERT(document.triangle_count == 2048u);
    TEST_ASSERT(Layout_MeshSolidPreviewBuildLod(&document, 180u, &lod));
    TEST_ASSERT(lod.triangle_count > 0u);
    TEST_ASSERT(lod.triangle_count <= 180u);
    TEST_ASSERT(lod.triangle_count < lod.source_triangle_count);
    TEST_ASSERT(lod.vertex_count > 3u);
    TEST_ASSERT(lod.cluster_resolution >= 2);
    for (size_t i = 0u; i < lod.triangle_count * 3u; ++i) {
        TEST_ASSERT(lod.indices[i] < lod.vertex_count);
    }
    for (size_t i = 0u; i < lod.triangle_count; ++i) {
        const uint32_t a = lod.indices[i * 3u + 0u];
        const uint32_t b = lod.indices[i * 3u + 1u];
        const uint32_t c = lod.indices[i * 3u + 2u];
        TEST_ASSERT(a != b && b != c && c != a);
    }
    Layout_MeshSolidPreviewFreeLod(&lod);
    core_mesh_asset_runtime_document_free(&document);
    return true;
}

static bool test_mesh_solid_lod_keeps_small_mesh_exact(void) {
    CoreMeshAssetRuntimeDocument document;
    LayoutMeshSolidPreviewLod lod = {0};
    TEST_ASSERT(ld_test_build_grid_mesh(&document, 4u));
    TEST_ASSERT(Layout_MeshSolidPreviewBuildLod(&document, 100u, &lod));
    TEST_ASSERT(lod.cluster_resolution == 0);
    TEST_ASSERT(lod.vertex_count == document.vertex_count);
    TEST_ASSERT(lod.triangle_count == document.triangle_count);
    Layout_MeshSolidPreviewFreeLod(&lod);
    core_mesh_asset_runtime_document_free(&document);
    return true;
}

static bool test_mesh_solid_silhouette_marks_boundary_not_interior(void) {
    enum { width = 7, height = 7 };
    uint8_t rgba[width * height * 4];
    float depth[width * height];
    memset(rgba, 0, sizeof(rgba));
    for (size_t i = 0u; i < width * height; ++i) depth[i] = INFINITY;
    for (int y = 2; y <= 4; ++y) {
        for (int x = 2; x <= 4; ++x) {
            const size_t pixel = (size_t)y * width + (size_t)x;
            rgba[pixel * 4u + 0u] = 120u;
            rgba[pixel * 4u + 1u] = 160u;
            rgba[pixel * 4u + 2u] = 200u;
            rgba[pixel * 4u + 3u] = 220u;
            depth[pixel] = 1.0f;
        }
    }
    TEST_ASSERT(Layout_MeshSolidPreviewApplySilhouette(
                    rgba, depth, width, height, 10u, 20u, 30u, 255u) == 8u);
    TEST_ASSERT(rgba[((size_t)3 * width + 3u) * 4u + 0u] == 120u);
    TEST_ASSERT(rgba[((size_t)2 * width + 2u) * 4u + 0u] == 10u);
    TEST_ASSERT(rgba[((size_t)2 * width + 2u) * 4u + 3u] == 255u);
    return true;
}

static bool test_mesh_solid_outline_only_clears_filled_interior(void) {
    enum { WIDTH = 7, HEIGHT = 7 };
    uint8_t rgba[WIDTH * HEIGHT * 4] = {0};
    float depth[WIDTH * HEIGHT];
    for (size_t i = 0u; i < WIDTH * HEIGHT; ++i) depth[i] = INFINITY;
    for (int y = 2; y <= 4; ++y) {
        for (int x = 2; x <= 4; ++x) {
            const size_t pixel = (size_t)y * WIDTH + (size_t)x;
            rgba[pixel * 4u + 3u] = 255u;
            depth[pixel] = 1.0f;
        }
    }

    TEST_ASSERT(Layout_MeshSolidPreviewApplyOutlineOnly(
        rgba, depth, WIDTH, HEIGHT, 10u, 20u, 30u, 240u) == 8u);
    TEST_ASSERT(rgba[((size_t)3 * WIDTH + 3u) * 4u + 3u] == 0u);
    TEST_ASSERT(rgba[((size_t)2 * WIDTH + 3u) * 4u + 3u] == 240u);
    TEST_ASSERT(rgba[((size_t)2 * WIDTH + 3u) * 4u + 0u] == 10u);
    return true;
}

static bool test_mesh_solid_adaptive_quality_settles_after_stable_interval(void) {
    const uint64_t changedAt = 1000000000ull;
    TEST_ASSERT(Layout_MeshSolidPreviewUsesInteractiveQuality(changedAt, changedAt));
    TEST_ASSERT(Layout_MeshSolidPreviewUsesInteractiveQuality(changedAt + 149999999ull,
                                                              changedAt));
    TEST_ASSERT(!Layout_MeshSolidPreviewUsesInteractiveQuality(changedAt + 150000000ull,
                                                               changedAt));
    TEST_ASSERT(!Layout_MeshSolidPreviewUsesInteractiveQuality(0u, changedAt));
    return true;
}

static bool test_mesh_solid_quality_only_resets_for_geometry_changes(void) {
    TEST_ASSERT(!Layout_MeshSolidPreviewInvalidationResetsQuality(
        LAYOUT_MESH_SOLID_INVALIDATION_NONE));
    TEST_ASSERT(!Layout_MeshSolidPreviewInvalidationResetsQuality(
        LAYOUT_MESH_SOLID_INVALIDATION_APPEARANCE));
    TEST_ASSERT(!Layout_MeshSolidPreviewInvalidationResetsQuality(
        LAYOUT_MESH_SOLID_INVALIDATION_OVERLAY));
    TEST_ASSERT(!Layout_MeshSolidPreviewInvalidationResetsQuality(
        LAYOUT_MESH_SOLID_INVALIDATION_PROJECTION));
    TEST_ASSERT(!Layout_MeshSolidPreviewInvalidationResetsQuality(
        LAYOUT_MESH_SOLID_INVALIDATION_APPEARANCE |
        LAYOUT_MESH_SOLID_INVALIDATION_OVERLAY));
    TEST_ASSERT(Layout_MeshSolidPreviewInvalidationResetsQuality(
        LAYOUT_MESH_SOLID_INVALIDATION_GEOMETRY));
    TEST_ASSERT(Layout_MeshSolidPreviewInvalidationResetsQuality(
        LAYOUT_MESH_SOLID_INVALIDATION_GEOMETRY |
        LAYOUT_MESH_SOLID_INVALIDATION_APPEARANCE |
        LAYOUT_MESH_SOLID_INVALIDATION_PROJECTION |
        LAYOUT_MESH_SOLID_INVALIDATION_OVERLAY));
    return true;
}

static bool test_mesh_solid_view_quality_ignores_pan_and_plane_offset(void) {
    SpaceViewContext previous = {
        .plane = { .axis = VIEW_PLANE_XY, .offset = 0.0f },
        .camera = {
            .enabled = true,
            .yawDeg = 25.0f,
            .pitchDeg = -15.0f,
            .target = {1.0f, 2.0f, 3.0f},
        },
    };
    SpaceViewContext current = previous;

    current.camera.target = (Vec3){9.0f, -4.0f, 2.0f};
    current.plane.offset = 12.0f;
    TEST_ASSERT(!Layout_MeshSolidPreviewViewChangeResetsQuality(&previous, &current));

    current = previous;
    current.camera.yawDeg += 1.0f;
    TEST_ASSERT(Layout_MeshSolidPreviewViewChangeResetsQuality(&previous, &current));
    current = previous;
    current.camera.pitchDeg -= 1.0f;
    TEST_ASSERT(Layout_MeshSolidPreviewViewChangeResetsQuality(&previous, &current));
    current = previous;
    current.camera.enabled = false;
    TEST_ASSERT(Layout_MeshSolidPreviewViewChangeResetsQuality(&previous, &current));

    previous.camera.enabled = false;
    current = previous;
    current.plane.offset = -5.0f;
    TEST_ASSERT(!Layout_MeshSolidPreviewViewChangeResetsQuality(&previous, &current));
    current.plane.axis = VIEW_PLANE_YZ;
    TEST_ASSERT(Layout_MeshSolidPreviewViewChangeResetsQuality(&previous, &current));
    return true;
}

bool test_layout_mesh_solid_preview_run_tests(void) {
    const TestCase cases[] = {
        {"MeshSolidLodPreservesCoherentIndexedSurface",
         test_mesh_solid_lod_preserves_coherent_indexed_surface},
        {"MeshSolidLodKeepsSmallMeshExact", test_mesh_solid_lod_keeps_small_mesh_exact},
        {"MeshSolidSilhouetteMarksBoundaryNotInterior",
         test_mesh_solid_silhouette_marks_boundary_not_interior},
        {"MeshSolidOutlineOnlyClearsFilledInterior",
         test_mesh_solid_outline_only_clears_filled_interior},
        {"MeshSolidAdaptiveQualitySettlesAfterStableInterval",
         test_mesh_solid_adaptive_quality_settles_after_stable_interval},
        {"MeshSolidQualityOnlyResetsForGeometryChanges",
         test_mesh_solid_quality_only_resets_for_geometry_changes},
        {"MeshSolidViewQualityIgnoresPanAndPlaneOffset",
         test_mesh_solid_view_quality_ignores_pan_and_plane_offset},
    };
    return run_test_cases("LayoutMeshSolidPreview", cases, sizeof(cases) / sizeof(cases[0]));
}
