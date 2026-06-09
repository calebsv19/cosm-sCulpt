#include "test_framework.h"

#include "Menu/line_drawing_catalog_preview.h"
#include "Layout/layout_json.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool write_file(const char* path, const char* contents) {
    FILE* file = NULL;
    if (!path) return false;
    file = fopen(path, "wb");
    if (!file) return false;
    if (contents && contents[0]) {
        (void)fputs(contents, file);
    }
    fclose(file);
    return true;
}

static bool build_sample_layout(Layout* layout) {
    Transform3D transform = Layout_Transform3D_Default();
    Plane3 plane = Plane3_FromAxisZ(0.0f);
    uint32_t plane_id = 0u;
    uint32_t prism_id = 0u;
    Object3D* object = NULL;

    Layout_Init(layout, 1.0f);
    (void)Layout_AddAnchor3(layout, (Vec3){-2.0f, -1.5f, 0.0f});
    (void)Layout_AddAnchor3(layout, (Vec3){2.0f, -1.5f, 0.0f});
    Layout_AddWall3(layout, (Vec3){-2.0f, -1.5f, 0.0f}, (Vec3){2.0f, -1.5f, 0.0f});

    transform.position = (Vec3){0.0f, 0.0f, 0.0f};
    plane_id = Layout_ObjectStore_Create(&layout->objectStore,
                                         OBJECT3D_KIND_PLANE,
                                         &transform,
                                         "plane_primitive",
                                         CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                         CORE_OBJECT_PLANE_XY);
    object = Layout_ObjectStore_Find(&layout->objectStore, plane_id);
    TEST_ASSERT(object != NULL);
    object->plane.width = 4.0f;
    object->plane.height = 3.0f;
    object->plane.frame = PlaneFrame3_FromPlane(plane, transform.position);
    object->plane.frame.origin = transform.position;
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));

    transform.position = (Vec3){0.0f, 0.0f, 1.0f};
    prism_id = Layout_ObjectStore_Create(&layout->objectStore,
                                         OBJECT3D_KIND_RECT_PRISM,
                                         &transform,
                                         "rect_prism_primitive",
                                         CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                         CORE_OBJECT_PLANE_XY);
    object = Layout_ObjectStore_Find(&layout->objectStore, prism_id);
    TEST_ASSERT(object != NULL);
    object->rectPrism.width = 2.0f;
    object->rectPrism.height = 2.0f;
    object->rectPrism.depth = 2.0f;
    object->rectPrism.frame = PlaneFrame3_FromPlane(plane, transform.position);
    object->rectPrism.frame.origin = transform.position;
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));
    return true;
}

static bool test_catalog_preview_loads_layout_metadata_and_segments(void) {
    char temp_template[] = "/tmp/ld_catalog_preview_XXXXXX";
    char* root = NULL;
    char layout_path[PATH_MAX];
    Layout layout;
    LineDrawingCatalogPreviewData preview;

    root = mkdtemp(temp_template);
    TEST_ASSERT(root != NULL);
    snprintf(layout_path, sizeof(layout_path), "%s/sample.layout.json", root);

    TEST_ASSERT(build_sample_layout(&layout));
    TEST_ASSERT(Layout_SaveToFile(&layout, layout_path));
    Layout_Free(&layout);

    TEST_ASSERT(LineDrawingCatalogPreview_Load(&preview,
                                               LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT,
                                               layout_path));
    TEST_ASSERT(preview.loaded);
    TEST_ASSERT(!preview.load_failed);
    TEST_ASSERT(preview.has_preview);
    TEST_ASSERT(preview.object_count == 2);
    TEST_ASSERT(preview.plane_count == 1);
    TEST_ASSERT(preview.rect_prism_count == 1);
    TEST_ASSERT(preview.wall_count == 1);
    TEST_ASSERT(preview.segment_count > 0);
    TEST_ASSERT(preview.extent_x > 0.0f);
    TEST_ASSERT(preview.extent_y > 0.0f);
    TEST_ASSERT(preview.extent_z > 0.0f);
    return true;
}

static bool test_catalog_preview_loads_scene_authoring_snapshot(void) {
    char temp_template[] = "/tmp/ld_catalog_preview_scene_XXXXXX";
    char* root = NULL;
    char authoring_path[PATH_MAX];
    char* layout_snapshot = NULL;
    char* authoring_json = NULL;
    size_t authoring_size = 0u;
    Layout layout;
    LineDrawingCatalogPreviewData preview;

    root = mkdtemp(temp_template);
    TEST_ASSERT(root != NULL);
    snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", root);

    TEST_ASSERT(build_sample_layout(&layout));
    layout_snapshot = Layout_SaveToString(&layout);
    Layout_Free(&layout);
    TEST_ASSERT(layout_snapshot != NULL);

    authoring_size = strlen(layout_snapshot) + 128u;
    authoring_json = (char*)malloc(authoring_size);
    TEST_ASSERT(authoring_json != NULL);
    snprintf(authoring_json,
             authoring_size,
             "{\"schema_variant\":\"scene_authoring_v1\",\"extensions\":{\"line_drawing\":{\"layout_snapshot\":%s}}}",
             layout_snapshot);
    TEST_ASSERT(write_file(authoring_path, authoring_json));

    TEST_ASSERT(LineDrawingCatalogPreview_Load(&preview,
                                               LINE_DRAWING_CATALOG_PREVIEW_SOURCE_SCENE,
                                               authoring_path));
    TEST_ASSERT(preview.loaded);
    TEST_ASSERT(!preview.load_failed);
    TEST_ASSERT(preview.has_preview);
    TEST_ASSERT(preview.object_count == 2);
    TEST_ASSERT(preview.segment_count > 0);

    Layout_FreeString(layout_snapshot);
    free(authoring_json);
    return true;
}

bool catalog_preview_run_tests(void) {
    const TestCase cases[] = {
        {"LoadsLayoutMetadataAndSegments", test_catalog_preview_loads_layout_metadata_and_segments},
        {"LoadsSceneAuthoringSnapshot", test_catalog_preview_loads_scene_authoring_snapshot},
    };
    return run_test_cases("CatalogPreview", cases, sizeof(cases) / sizeof(cases[0]));
}
