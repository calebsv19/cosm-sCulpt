#include "Layout/layout.h"
#include "Layout/layout_json.h"
#include "Tools/shape_dataset.h"

#include <stdio.h>
#include <string.h>

static void print_usage(const char* argv0) {
    fprintf(stderr,
            "usage: %s <layout_json> <out_pack> [--axis xy|yz|xz]\n",
            argv0 ? argv0 : "shape_pack_tool");
}

static int parse_axis(const char* value, ViewPlaneAxis* out_axis) {
    if (!value || !out_axis) return 0;
    if (strcmp(value, "xy") == 0) {
        *out_axis = VIEW_PLANE_XY;
        return 1;
    }
    if (strcmp(value, "yz") == 0) {
        *out_axis = VIEW_PLANE_YZ;
        return 1;
    }
    if (strcmp(value, "xz") == 0) {
        *out_axis = VIEW_PLANE_XZ;
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    const char* layout_path;
    const char* pack_path;
    ViewPlaneAxis axis = VIEW_PLANE_XY;
    Layout layout;
    CoreDataset dataset;
    LineDrawingDatasetSummary summary;
    CoreResult result;

    if (argc < 3) {
        print_usage(argc > 0 ? argv[0] : "shape_pack_tool");
        return 1;
    }

    layout_path = argv[1];
    pack_path = argv[2];

    for (int i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--axis") == 0) {
            if (i + 1 >= argc || !parse_axis(argv[i + 1], &axis)) {
                fprintf(stderr, "[shape_pack_tool] invalid axis value\n");
                print_usage(argv[0]);
                return 1;
            }
            ++i;
            continue;
        }

        fprintf(stderr, "[shape_pack_tool] unknown argument: %s\n", argv[i]);
        print_usage(argv[0]);
        return 1;
    }

    Layout_Init(&layout, 1.0f);
    if (!Layout_LoadFromFile(&layout, layout_path)) {
        fprintf(stderr, "[shape_pack_tool] failed to load layout: %s\n", layout_path);
        Layout_Free(&layout);
        return 1;
    }

    result = LineDrawingDataset_BuildFromLayout(&layout, axis, &dataset, &summary);
    if (result.code != CORE_OK) {
        fprintf(stderr, "[shape_pack_tool] dataset build failed: %s\n", result.message);
        Layout_Free(&layout);
        return 1;
    }

    core_dataset_free(&dataset);

    result = LineDrawingDataset_ExportPackV1(&layout, axis, pack_path, &summary);
    if (result.code != CORE_OK) {
        fprintf(stderr, "[shape_pack_tool] pack export failed: %s\n", result.message);
        Layout_Free(&layout);
        return 1;
    }

    printf("[shape_pack_tool] wrote %s\n", pack_path);
    printf("[shape_pack_tool] anchors=%u walls=%u curves=%u persistent=%u\n",
           summary.active_anchor_count,
           summary.active_wall_count,
           summary.curved_anchor_count,
           summary.persistent_anchor_count);
    printf("[shape_pack_tool] bounds min(%.3f, %.3f, %.3f) max(%.3f, %.3f, %.3f)\n",
           summary.min_x,
           summary.min_y,
           summary.min_z,
           summary.max_x,
           summary.max_y,
           summary.max_z);

    Layout_Free(&layout);
    return 0;
}
