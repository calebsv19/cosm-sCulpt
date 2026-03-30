#pragma once

#include "Layout/layout.h"
#include "core_data.h"
#include "core_pack.h"

#include <stdint.h>

typedef struct LineDrawingDatasetSummary {
    uint32_t active_anchor_count;
    uint32_t active_wall_count;
    uint32_t curved_anchor_count;
    uint32_t persistent_anchor_count;
    float min_x;
    float min_y;
    float min_z;
    float max_x;
    float max_y;
    float max_z;
} LineDrawingDatasetSummary;

CoreResult LineDrawingDataset_BuildFromLayout(const Layout* layout,
                                              ViewPlaneAxis axis,
                                              CoreDataset* out_dataset,
                                              LineDrawingDatasetSummary* out_summary);

CoreResult LineDrawingDataset_ExportPackV1(const Layout* layout,
                                           ViewPlaneAxis axis,
                                           const char* pack_path,
                                           LineDrawingDatasetSummary* out_summary);
