#include "test_framework.h"

#include "Layout/layout.h"
#include "Math/math_util.h"
#include "Tools/shape_dataset.h"

#include <string.h>

static bool test_dataset_build_from_layout_basic(void) {
    Layout layout;
    CoreDataset dataset;
    LineDrawingDatasetSummary summary;

    Layout_Init(&layout, 1.0f);

    int a = Layout_AddAnchor3(&layout, (Vec3){0.0f, 0.0f, 0.0f});
    int b = Layout_AddAnchor3(&layout, (Vec3){2.0f, 1.0f, 3.0f});
    TEST_ASSERT(a >= 0);
    TEST_ASSERT(b >= 0);

    layout.anchors[a].isPersistent = true;
    layout.anchors[b].type = ANCHOR_TYPE_CURVE;

    Layout_AddWall3(&layout, layout.anchors[a].pos, layout.anchors[b].pos);

    CoreResult r = LineDrawingDataset_BuildFromLayout(&layout, VIEW_PLANE_XY, &dataset, &summary);
    TEST_ASSERT(r.code == CORE_OK);

    TEST_ASSERT(summary.active_anchor_count == 2u);
    TEST_ASSERT(summary.active_wall_count == 1u);
    TEST_ASSERT(summary.curved_anchor_count == 1u);
    TEST_ASSERT(summary.persistent_anchor_count == 1u);

    const CoreMetadataItem* profile = core_dataset_find_metadata(&dataset, "profile");
    TEST_ASSERT(profile != NULL);
    TEST_ASSERT(profile->type == CORE_META_STRING);
    TEST_ASSERT(strcmp(profile->as.string_value, "line_drawing_shape_diag_v1") == 0);

    const CoreMetadataItem* family = core_dataset_find_metadata(&dataset, "schema_family");
    TEST_ASSERT(family != NULL);
    TEST_ASSERT(family->type == CORE_META_STRING);
    TEST_ASSERT(strcmp(family->as.string_value, "line_drawing_shape_diag") == 0);

    const CoreMetadataItem* variant = core_dataset_find_metadata(&dataset, "schema_variant");
    TEST_ASSERT(variant != NULL);
    TEST_ASSERT(variant->type == CORE_META_STRING);
    TEST_ASSERT(strcmp(variant->as.string_value, "3d") == 0);

    const CoreDataItem* anchors = core_dataset_find(&dataset, "anchors_v1");
    TEST_ASSERT(anchors != NULL);
    TEST_ASSERT(anchors->kind == CORE_DATA_TABLE_TYPED);
    TEST_ASSERT(anchors->as.table_typed.row_count == 2u);
    TEST_ASSERT(anchors->as.table_typed.column_count == 8u);

    const CoreDataItem* anchors_ext = core_dataset_find(&dataset, "anchors_3d_ext_v1");
    TEST_ASSERT(anchors_ext != NULL);
    TEST_ASSERT(anchors_ext->kind == CORE_DATA_TABLE_TYPED);
    TEST_ASSERT(anchors_ext->as.table_typed.row_count == 2u);
    TEST_ASSERT(anchors_ext->as.table_typed.column_count == 3u);

    const CoreDataItem* walls = core_dataset_find(&dataset, "walls_v1");
    TEST_ASSERT(walls != NULL);
    TEST_ASSERT(walls->kind == CORE_DATA_TABLE_TYPED);
    TEST_ASSERT(walls->as.table_typed.row_count == 1u);

    core_dataset_free(&dataset);
    Layout_Free(&layout);
    return true;
}

bool shape_dataset_run_tests(void) {
    const TestCase cases[] = {
        { "dataset_build_from_layout_basic", test_dataset_build_from_layout_basic },
    };
    return run_test_cases("ShapeDataset", cases, sizeof(cases) / sizeof(cases[0]));
}
