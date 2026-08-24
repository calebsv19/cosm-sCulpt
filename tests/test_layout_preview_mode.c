#include "test_layout_internal.h"
#include "Layout/scene/layout_mesh_asset_path_resolver.h"

#include <sys/stat.h>
#include <unistd.h>

static bool test_preview_mode_defaults_to_wireframe(void) {
    ld_test_init_runtime();
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_WIREFRAME);
    TEST_ASSERT(strcmp(Global_GetPreviewModeLabel(Global_GetPreviewMode()), "Wireframe") == 0);
    TEST_ASSERT(strcmp(Global_GetPreviewModeExportValue(Global_GetPreviewMode()), "wireframe") == 0);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_preview_mode_cycles_wireframe_flat_material_bounds(void) {
    ld_test_init_runtime();
    TEST_ASSERT(Global_TogglePreviewMode());
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_FLAT);
    TEST_ASSERT(strcmp(Global_GetPreviewModeExportValue(Global_GetPreviewMode()), "flat") == 0);
    TEST_ASSERT(Global_TogglePreviewMode());
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_MATERIAL);
    TEST_ASSERT(strcmp(Global_GetPreviewModeExportValue(Global_GetPreviewMode()), "material") == 0);
    TEST_ASSERT(Global_TogglePreviewMode());
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_BOUNDS);
    TEST_ASSERT(strcmp(Global_GetPreviewModeLabel(Global_GetPreviewMode()), "Bounds") == 0);
    TEST_ASSERT(strcmp(Global_GetPreviewModeExportValue(Global_GetPreviewMode()), "bounds") == 0);
    TEST_ASSERT(Global_TogglePreviewMode());
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_WIREFRAME);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_preview_mode_rejects_invalid_values(void) {
    ld_test_init_runtime();
    TEST_ASSERT(!Global_SetPreviewMode((LineDrawingPreviewMode)99));
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_WIREFRAME);
    TEST_ASSERT(Global_SetPreviewMode(LINE_DRAWING_PREVIEW_MODE_FLAT));
    TEST_ASSERT(Global_GetPreviewMode() == LINE_DRAWING_PREVIEW_MODE_FLAT);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_mesh_path_resolver_recovers_legacy_desktop_library(void) {
    char root[256];
    char desktop[320];
    char stls[384];
    char library[448];
    char stored[512];
    char actual[512];
    char resolved[512];
    FILE* file = NULL;
    snprintf(root, sizeof(root), "/tmp/ld_mesh_path_%ld", (long)getpid());
    snprintf(desktop, sizeof(desktop), "%s/Desktop", root);
    snprintf(stls, sizeof(stls), "%s/stls", desktop);
    snprintf(library, sizeof(library), "%s/Legacy_Library", stls);
    snprintf(stored, sizeof(stored), "%s/Legacy_Library/skull.runtime.json", desktop);
    snprintf(actual, sizeof(actual), "%s/skull.runtime.json", library);
    TEST_ASSERT(mkdir(root, 0700) == 0);
    TEST_ASSERT(mkdir(desktop, 0700) == 0);
    TEST_ASSERT(mkdir(stls, 0700) == 0);
    TEST_ASSERT(mkdir(library, 0700) == 0);
    file = fopen(actual, "wb");
    TEST_ASSERT(file != NULL);
    TEST_ASSERT(fputs("{}", file) >= 0);
    fclose(file);

    ld_test_init_runtime();
    TEST_ASSERT(Layout_MeshAssetResolveRuntimePath(stored, resolved, sizeof(resolved)) ==
                LAYOUT_MESH_PATH_RELOCATED);
    TEST_ASSERT(strcmp(resolved, actual) == 0);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_mesh_path_resolver_prefers_scene_owned_attachment(void) {
    char root[256];
    char scene[320];
    char assets[384];
    char meshes[448];
    char authoring[512];
    char stored[512];
    char actual[512];
    char resolved[512];
    FILE* file = NULL;
    snprintf(root, sizeof(root), "/tmp/ld_mesh_scene_path_%ld", (long)getpid());
    snprintf(scene, sizeof(scene), "%s/scene_v2", root);
    snprintf(assets, sizeof(assets), "%s/assets", scene);
    snprintf(meshes, sizeof(meshes), "%s/mesh_assets", assets);
    snprintf(authoring, sizeof(authoring), "%s/scene_authoring.json", scene);
    snprintf(stored, sizeof(stored), "%s/missing/library/dragon.runtime.json", root);
    snprintf(actual, sizeof(actual), "%s/dragon.runtime.json", meshes);
    TEST_ASSERT(mkdir(root, 0700) == 0);
    TEST_ASSERT(mkdir(scene, 0700) == 0);
    TEST_ASSERT(mkdir(assets, 0700) == 0);
    TEST_ASSERT(mkdir(meshes, 0700) == 0);
    file = fopen(actual, "wb");
    TEST_ASSERT(file != NULL);
    TEST_ASSERT(fputs("{}", file) >= 0);
    fclose(file);
    TEST_ASSERT(Layout_MeshAssetResolveRuntimePathForScene(stored,
                                                           authoring,
                                                           resolved,
                                                           sizeof(resolved)) ==
                LAYOUT_MESH_PATH_RELOCATED);
    TEST_ASSERT(strcmp(resolved, actual) == 0);
    return true;
}

static bool test_mesh_reconciliation_refreshes_scene_owned_metadata(void) {
    char root[256];
    char scene[320];
    char assets[384];
    char meshes[448];
    char authoring[512];
    char stored[512];
    char runtime[512];
    char diagnostics[256];
    FILE* file = NULL;
    Layout layout;
    Transform3D transform;
    Object3D* object = NULL;
    uint32_t object_id = 0u;
    size_t resolved_count = 0u;
    size_t changed_count = 0u;
    size_t unresolved_count = 0u;
    const char* runtime_json =
        "{"
        "\"schema_variant\":\"mesh_asset_runtime_v1\","
        "\"asset_id\":\"dragon\","
        "\"source_asset_id\":\"stanford_dragon\","
        "\"vertex_count\":433400,"
        "\"triangle_count\":840682,"
        "\"local_bounds\":{"
            "\"min\":{\"x\":-1.0,\"y\":-2.0,\"z\":-3.0},"
            "\"max\":{\"x\":4.0,\"y\":5.0,\"z\":6.0}"
        "}"
        "}";

    snprintf(root, sizeof(root), "/tmp/ld_mesh_reconcile_%ld", (long)getpid());
    snprintf(scene, sizeof(scene), "%s/scene_v5", root);
    snprintf(assets, sizeof(assets), "%s/assets", scene);
    snprintf(meshes, sizeof(meshes), "%s/mesh_assets", assets);
    snprintf(authoring, sizeof(authoring), "%s/scene_authoring.json", scene);
    snprintf(stored, sizeof(stored), "%s/missing/library/dragon.runtime.json", root);
    snprintf(runtime, sizeof(runtime), "%s/dragon.runtime.json", meshes);
    TEST_ASSERT(mkdir(root, 0700) == 0);
    TEST_ASSERT(mkdir(scene, 0700) == 0);
    TEST_ASSERT(mkdir(assets, 0700) == 0);
    TEST_ASSERT(mkdir(meshes, 0700) == 0);
    file = fopen(runtime, "wb");
    TEST_ASSERT(file != NULL);
    TEST_ASSERT(fputs(runtime_json, file) >= 0);
    fclose(file);

    ld_test_init_runtime();
    Layout_Init(&layout, 1.0f);
    transform = Layout_Transform3D_Default();
    object_id = Layout_ObjectStore_Create(&layout.objectStore,
                                          OBJECT3D_KIND_MESH_ASSET_INSTANCE,
                                          &transform,
                                          "mesh_asset_instance",
                                          CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                          CORE_OBJECT_PLANE_XY);
    TEST_ASSERT(object_id != 0u);
    object = Layout_ObjectStore_Find(&layout.objectStore, object_id);
    TEST_ASSERT(object != NULL);
    snprintf(object->meshInstance.assetId, sizeof(object->meshInstance.assetId), "%s", "dragon");
    snprintf(object->meshInstance.runtimePath,
             sizeof(object->meshInstance.runtimePath),
             "%s",
             stored);
    object->meshInstance.vertexCount = 100207u;
    object->meshInstance.triangleCount = 202520u;
    object->meshInstance.localBoundsMin = (Vec3){-0.5f, -0.5f, -0.5f};
    object->meshInstance.localBoundsMax = (Vec3){0.5f, 0.5f, 0.5f};
    object->meshInstance.lockToBounds = true;

    TEST_ASSERT(Layout_ReconcileMeshAssetInstancesForScene(&layout,
                                                            authoring,
                                                            &resolved_count,
                                                            &changed_count,
                                                            &unresolved_count,
                                                            diagnostics,
                                                            sizeof(diagnostics)));
    TEST_ASSERT(resolved_count == 1u);
    TEST_ASSERT(changed_count == 1u);
    TEST_ASSERT(unresolved_count == 0u);
    TEST_ASSERT(strcmp(object->meshInstance.runtimePath, runtime) == 0);
    TEST_ASSERT(strcmp(object->meshInstance.sourceAssetId, "stanford_dragon") == 0);
    TEST_ASSERT(object->meshInstance.vertexCount == 433400u);
    TEST_ASSERT(object->meshInstance.triangleCount == 840682u);
    TEST_ASSERT(object->meshInstance.localBoundsMin.y == -2.0f);
    TEST_ASSERT(object->meshInstance.localBoundsMax.z == 6.0f);

    snprintf(object->meshInstance.assetId,
             sizeof(object->meshInstance.assetId),
             "%s",
             "different_dragon");
    snprintf(object->meshInstance.runtimePath,
             sizeof(object->meshInstance.runtimePath),
             "%s",
             stored);
    object->meshInstance.vertexCount = 7u;
    object->meshInstance.triangleCount = 3u;
    TEST_ASSERT(!Layout_ReconcileMeshAssetInstancesForScene(&layout,
                                                             authoring,
                                                             &resolved_count,
                                                             &changed_count,
                                                             &unresolved_count,
                                                             diagnostics,
                                                             sizeof(diagnostics)));
    TEST_ASSERT(resolved_count == 0u);
    TEST_ASSERT(changed_count == 0u);
    TEST_ASSERT(unresolved_count == 1u);
    TEST_ASSERT(strcmp(object->meshInstance.assetId, "different_dragon") == 0);
    TEST_ASSERT(strcmp(object->meshInstance.runtimePath, stored) == 0);
    TEST_ASSERT(object->meshInstance.vertexCount == 7u);
    TEST_ASSERT(object->meshInstance.triangleCount == 3u);

    Layout_Free(&layout);
    ld_test_shutdown_runtime();
    return true;
}

bool test_layout_preview_mode_run_tests(void) {
    const TestCase cases[] = {
        {"PreviewModeDefaultsToWireframe", test_preview_mode_defaults_to_wireframe},
        {"PreviewModeCyclesWireframeFlatMaterialBounds", test_preview_mode_cycles_wireframe_flat_material_bounds},
        {"PreviewModeRejectsInvalidValues", test_preview_mode_rejects_invalid_values},
        {"MeshPathResolverRecoversLegacyDesktopLibrary", test_mesh_path_resolver_recovers_legacy_desktop_library},
        {"MeshPathResolverPrefersSceneOwnedAttachment", test_mesh_path_resolver_prefers_scene_owned_attachment},
        {"MeshReconciliationRefreshesSceneOwnedMetadata",
         test_mesh_reconciliation_refreshes_scene_owned_metadata},
    };
    return run_test_cases("LayoutPreviewMode", cases, sizeof(cases) / sizeof(cases[0]));
}
