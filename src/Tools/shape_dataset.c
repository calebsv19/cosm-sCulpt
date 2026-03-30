#include "Tools/shape_dataset.h"

#include "core_base.h"

#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define LINE_DRAWING_DATASET_PROFILE "line_drawing_shape_diag_v1"
#define LINE_DRAWING_DATASET_FAMILY "line_drawing_shape_diag"
#define LINE_DRAWING_DATASET_VARIANT "3d"
#define LINE_DRAWING_DATASET_SCHEMA_VERSION 1

typedef struct LineDrawingAnchorRowV1 {
    float x;
    float y;
    float z;
    float handle_in_length;
    float handle_in_angle_deg;
    float handle_out_length;
    float handle_out_angle_deg;
    uint32_t persistent;
    uint32_t anchor_type;
    uint32_t handle_axis;
} LineDrawingAnchorRowV1;

typedef struct LineDrawingWallRowV1 {
    uint32_t a;
    uint32_t b;
    uint32_t lock_length;
} LineDrawingWallRowV1;

typedef struct LineDrawingAnchorPackBaseV1 {
    float x;
    float y;
    float handle_in_length;
    float handle_in_angle_deg;
    float handle_out_length;
    float handle_out_angle_deg;
    uint32_t persistent;
    uint32_t anchor_type;
} LineDrawingAnchorPackBaseV1;

typedef struct LineDrawingAnchorPackExt3DV1 {
    float z;
    uint32_t handle_axis;
} LineDrawingAnchorPackExt3DV1;

typedef struct LayoutCompactRows {
    LineDrawingAnchorRowV1* anchors;
    LineDrawingWallRowV1* walls;
    int* old_to_compact;
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
} LayoutCompactRows;

static const char* axis_to_string(ViewPlaneAxis axis) {
    switch (axis) {
        case VIEW_PLANE_YZ: return "yz";
        case VIEW_PLANE_XZ: return "xz";
        case VIEW_PLANE_XY:
        default:
            return "xy";
    }
}

static void summary_zero(LineDrawingDatasetSummary* summary) {
    if (!summary) return;
    memset(summary, 0, sizeof(*summary));
}

static void compact_rows_free(LayoutCompactRows* rows) {
    if (!rows) return;
    core_free(rows->anchors);
    core_free(rows->walls);
    core_free(rows->old_to_compact);
    memset(rows, 0, sizeof(*rows));
}

static CoreResult compact_rows_build(const Layout* layout, LayoutCompactRows* out_rows) {
    if (!layout || !out_rows) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    memset(out_rows, 0, sizeof(*out_rows));
    out_rows->min_x = FLT_MAX;
    out_rows->min_y = FLT_MAX;
    out_rows->min_z = FLT_MAX;
    out_rows->max_x = -FLT_MAX;
    out_rows->max_y = -FLT_MAX;
    out_rows->max_z = -FLT_MAX;

    out_rows->old_to_compact = (int*)core_alloc(sizeof(int) * layout->anchorCount);
    if (!out_rows->old_to_compact && layout->anchorCount > 0) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    for (size_t i = 0; i < layout->anchorCount; ++i) {
        out_rows->old_to_compact[i] = -1;
    }

    for (size_t i = 0; i < layout->anchorCount; ++i) {
        const Anchor* a = &layout->anchors[i];
        if (a->isDeleted) continue;
        out_rows->old_to_compact[i] = (int)out_rows->active_anchor_count;
        out_rows->active_anchor_count++;
    }

    out_rows->anchors = (LineDrawingAnchorRowV1*)core_calloc(out_rows->active_anchor_count,
                                                              sizeof(LineDrawingAnchorRowV1));
    if (!out_rows->anchors && out_rows->active_anchor_count > 0) {
        compact_rows_free(out_rows);
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    for (size_t old_idx = 0; old_idx < layout->anchorCount; ++old_idx) {
        int compact_idx = out_rows->old_to_compact[old_idx];
        if (compact_idx < 0) continue;

        const Anchor* a = &layout->anchors[old_idx];
        LineDrawingAnchorRowV1* row = &out_rows->anchors[compact_idx];

        row->x = a->pos.x;
        row->y = a->pos.y;
        row->z = a->pos.z;
        row->handle_in_length = a->handleInLength;
        row->handle_in_angle_deg = a->handleInAngleDeg;
        row->handle_out_length = a->handleOutLength;
        row->handle_out_angle_deg = a->handleOutAngleDeg;
        row->persistent = a->isPersistent ? 1u : 0u;
        row->anchor_type = (uint32_t)a->type;
        row->handle_axis = (uint32_t)a->handleAxis;

        if (a->type == ANCHOR_TYPE_CURVE) out_rows->curved_anchor_count++;
        if (a->isPersistent) out_rows->persistent_anchor_count++;

        if (a->pos.x < out_rows->min_x) out_rows->min_x = a->pos.x;
        if (a->pos.y < out_rows->min_y) out_rows->min_y = a->pos.y;
        if (a->pos.z < out_rows->min_z) out_rows->min_z = a->pos.z;
        if (a->pos.x > out_rows->max_x) out_rows->max_x = a->pos.x;
        if (a->pos.y > out_rows->max_y) out_rows->max_y = a->pos.y;
        if (a->pos.z > out_rows->max_z) out_rows->max_z = a->pos.z;
    }

    for (size_t i = 0; i < layout->wallCount; ++i) {
        const Wall* w = &layout->walls[i];
        if (w->isDeleted) continue;
        if (w->anchorA < 0 || w->anchorB < 0) continue;
        if ((size_t)w->anchorA >= layout->anchorCount || (size_t)w->anchorB >= layout->anchorCount) continue;
        if (out_rows->old_to_compact[w->anchorA] < 0 || out_rows->old_to_compact[w->anchorB] < 0) continue;
        out_rows->active_wall_count++;
    }

    out_rows->walls = (LineDrawingWallRowV1*)core_calloc(out_rows->active_wall_count,
                                                          sizeof(LineDrawingWallRowV1));
    if (!out_rows->walls && out_rows->active_wall_count > 0) {
        compact_rows_free(out_rows);
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    uint32_t wall_idx = 0;
    for (size_t i = 0; i < layout->wallCount; ++i) {
        const Wall* w = &layout->walls[i];
        if (w->isDeleted) continue;
        if (w->anchorA < 0 || w->anchorB < 0) continue;
        if ((size_t)w->anchorA >= layout->anchorCount || (size_t)w->anchorB >= layout->anchorCount) continue;
        if (out_rows->old_to_compact[w->anchorA] < 0 || out_rows->old_to_compact[w->anchorB] < 0) continue;

        LineDrawingWallRowV1* row = &out_rows->walls[wall_idx++];
        row->a = (uint32_t)out_rows->old_to_compact[w->anchorA];
        row->b = (uint32_t)out_rows->old_to_compact[w->anchorB];
        row->lock_length = w->lockLength ? 1u : 0u;
    }

    if (out_rows->active_anchor_count == 0) {
        out_rows->min_x = out_rows->min_y = out_rows->min_z = 0.0f;
        out_rows->max_x = out_rows->max_y = out_rows->max_z = 0.0f;
    }

    return core_result_ok();
}

static void fill_summary_from_rows(const LayoutCompactRows* rows,
                                   LineDrawingDatasetSummary* out_summary) {
    if (!out_summary || !rows) return;

    out_summary->active_anchor_count = rows->active_anchor_count;
    out_summary->active_wall_count = rows->active_wall_count;
    out_summary->curved_anchor_count = rows->curved_anchor_count;
    out_summary->persistent_anchor_count = rows->persistent_anchor_count;
    out_summary->min_x = rows->min_x;
    out_summary->min_y = rows->min_y;
    out_summary->min_z = rows->min_z;
    out_summary->max_x = rows->max_x;
    out_summary->max_y = rows->max_y;
    out_summary->max_z = rows->max_z;
}

CoreResult LineDrawingDataset_BuildFromLayout(const Layout* layout,
                                              ViewPlaneAxis axis,
                                              CoreDataset* out_dataset,
                                              LineDrawingDatasetSummary* out_summary) {
    LayoutCompactRows rows;
    CoreResult r;

    if (!layout || !out_dataset) {
        CoreResult invalid = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return invalid;
    }

    core_dataset_init(out_dataset);
    summary_zero(out_summary);

    r = compact_rows_build(layout, &rows);
    if (r.code != CORE_OK) return r;

    r = core_dataset_add_metadata_string(out_dataset, "profile", LINE_DRAWING_DATASET_PROFILE);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(out_dataset, "schema_family", LINE_DRAWING_DATASET_FAMILY);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(out_dataset, "schema_variant", LINE_DRAWING_DATASET_VARIANT);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(out_dataset, "schema_version", LINE_DRAWING_DATASET_SCHEMA_VERSION);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(out_dataset, "projection_axis", axis_to_string(axis));
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(out_dataset, "active_anchor_count", (int64_t)rows.active_anchor_count);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(out_dataset, "active_wall_count", (int64_t)rows.active_wall_count);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(out_dataset, "curved_anchor_count", (int64_t)rows.curved_anchor_count);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(out_dataset, "persistent_anchor_count", (int64_t)rows.persistent_anchor_count);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_f64(out_dataset, "bounds_min_x", rows.min_x);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_f64(out_dataset, "bounds_min_y", rows.min_y);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_f64(out_dataset, "bounds_min_z", rows.min_z);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_f64(out_dataset, "bounds_max_x", rows.max_x);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_f64(out_dataset, "bounds_max_y", rows.max_y);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_f64(out_dataset, "bounds_max_z", rows.max_z);
    if (r.code != CORE_OK) goto fail;

    {
        const char* anchor_cols[] = {
            "x", "y",
            "persistent", "anchor_type",
            "handle_in_length", "handle_in_angle_deg",
            "handle_out_length", "handle_out_angle_deg"
        };
        const CoreTableColumnType anchor_types[] = {
            CORE_TABLE_COL_F32, CORE_TABLE_COL_F32,
            CORE_TABLE_COL_U32, CORE_TABLE_COL_U32,
            CORE_TABLE_COL_F32, CORE_TABLE_COL_F32,
            CORE_TABLE_COL_F32, CORE_TABLE_COL_F32
        };

        const void* anchor_data[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

        float* xs = NULL;
        float* ys = NULL;
        uint32_t* persistent = NULL;
        uint32_t* anchor_type = NULL;
        float* handle_in_len = NULL;
        float* handle_in_ang = NULL;
        float* handle_out_len = NULL;
        float* handle_out_ang = NULL;

        uint32_t n = rows.active_anchor_count;
        if (n > 0) {
            xs = (float*)core_alloc(sizeof(float) * n);
            ys = (float*)core_alloc(sizeof(float) * n);
            persistent = (uint32_t*)core_alloc(sizeof(uint32_t) * n);
            anchor_type = (uint32_t*)core_alloc(sizeof(uint32_t) * n);
            handle_in_len = (float*)core_alloc(sizeof(float) * n);
            handle_in_ang = (float*)core_alloc(sizeof(float) * n);
            handle_out_len = (float*)core_alloc(sizeof(float) * n);
            handle_out_ang = (float*)core_alloc(sizeof(float) * n);

            if (!xs || !ys || !persistent || !anchor_type ||
                !handle_in_len || !handle_in_ang || !handle_out_len || !handle_out_ang) {
                core_free(xs);
                core_free(ys);
                core_free(persistent);
                core_free(anchor_type);
                core_free(handle_in_len);
                core_free(handle_in_ang);
                core_free(handle_out_len);
                core_free(handle_out_ang);
                r.code = CORE_ERR_OUT_OF_MEMORY;
                r.message = "out of memory";
                goto fail;
            }
        }

        for (uint32_t i = 0; i < n; ++i) {
            const LineDrawingAnchorRowV1* row = &rows.anchors[i];
            xs[i] = row->x;
            ys[i] = row->y;
            persistent[i] = row->persistent;
            anchor_type[i] = row->anchor_type;
            handle_in_len[i] = row->handle_in_length;
            handle_in_ang[i] = row->handle_in_angle_deg;
            handle_out_len[i] = row->handle_out_length;
            handle_out_ang[i] = row->handle_out_angle_deg;
        }

        anchor_data[0] = xs;
        anchor_data[1] = ys;
        anchor_data[2] = persistent;
        anchor_data[3] = anchor_type;
        anchor_data[4] = handle_in_len;
        anchor_data[5] = handle_in_ang;
        anchor_data[6] = handle_out_len;
        anchor_data[7] = handle_out_ang;

        r = core_dataset_add_table_typed(out_dataset,
                                         "anchors_v1",
                                         anchor_cols,
                                         anchor_types,
                                         (uint32_t)(sizeof(anchor_cols) / sizeof(anchor_cols[0])),
                                         n,
                                         anchor_data);

        core_free(xs);
        core_free(ys);
        core_free(persistent);
        core_free(anchor_type);
        core_free(handle_in_len);
        core_free(handle_in_ang);
        core_free(handle_out_len);
        core_free(handle_out_ang);

        if (r.code != CORE_OK) goto fail;
    }

    {
        const char* ext_cols[] = { "anchor_index", "z", "handle_axis" };
        const CoreTableColumnType ext_types[] = {
            CORE_TABLE_COL_U32,
            CORE_TABLE_COL_F32,
            CORE_TABLE_COL_U32
        };
        const void* ext_data[] = { NULL, NULL, NULL };

        uint32_t n = rows.active_anchor_count;
        uint32_t* anchor_index = NULL;
        float* zs = NULL;
        uint32_t* handle_axis_col = NULL;

        if (n > 0) {
            anchor_index = (uint32_t*)core_alloc(sizeof(uint32_t) * n);
            zs = (float*)core_alloc(sizeof(float) * n);
            handle_axis_col = (uint32_t*)core_alloc(sizeof(uint32_t) * n);
            if (!anchor_index || !zs || !handle_axis_col) {
                core_free(anchor_index);
                core_free(zs);
                core_free(handle_axis_col);
                r.code = CORE_ERR_OUT_OF_MEMORY;
                r.message = "out of memory";
                goto fail;
            }
        }

        for (uint32_t i = 0; i < n; ++i) {
            const LineDrawingAnchorRowV1* row = &rows.anchors[i];
            anchor_index[i] = i;
            zs[i] = row->z;
            handle_axis_col[i] = row->handle_axis;
        }

        ext_data[0] = anchor_index;
        ext_data[1] = zs;
        ext_data[2] = handle_axis_col;

        r = core_dataset_add_table_typed(out_dataset,
                                         "anchors_3d_ext_v1",
                                         ext_cols,
                                         ext_types,
                                         (uint32_t)(sizeof(ext_cols) / sizeof(ext_cols[0])),
                                         n,
                                         ext_data);

        core_free(anchor_index);
        core_free(zs);
        core_free(handle_axis_col);

        if (r.code != CORE_OK) goto fail;
    }

    {
        const char* wall_cols[] = { "a", "b", "lock_length" };
        const CoreTableColumnType wall_types[] = {
            CORE_TABLE_COL_U32,
            CORE_TABLE_COL_U32,
            CORE_TABLE_COL_U32
        };
        const void* wall_data[] = { NULL, NULL, NULL };

        uint32_t n = rows.active_wall_count;
        uint32_t* a_col = NULL;
        uint32_t* b_col = NULL;
        uint32_t* lock_col = NULL;

        if (n > 0) {
            a_col = (uint32_t*)core_alloc(sizeof(uint32_t) * n);
            b_col = (uint32_t*)core_alloc(sizeof(uint32_t) * n);
            lock_col = (uint32_t*)core_alloc(sizeof(uint32_t) * n);
            if (!a_col || !b_col || !lock_col) {
                core_free(a_col);
                core_free(b_col);
                core_free(lock_col);
                r.code = CORE_ERR_OUT_OF_MEMORY;
                r.message = "out of memory";
                goto fail;
            }
        }

        for (uint32_t i = 0; i < n; ++i) {
            const LineDrawingWallRowV1* row = &rows.walls[i];
            a_col[i] = row->a;
            b_col[i] = row->b;
            lock_col[i] = row->lock_length;
        }

        wall_data[0] = a_col;
        wall_data[1] = b_col;
        wall_data[2] = lock_col;

        r = core_dataset_add_table_typed(out_dataset,
                                         "walls_v1",
                                         wall_cols,
                                         wall_types,
                                         (uint32_t)(sizeof(wall_cols) / sizeof(wall_cols[0])),
                                         n,
                                         wall_data);

        core_free(a_col);
        core_free(b_col);
        core_free(lock_col);

        if (r.code != CORE_OK) goto fail;
    }

    fill_summary_from_rows(&rows, out_summary);
    compact_rows_free(&rows);
    return core_result_ok();

fail:
    core_dataset_free(out_dataset);
    summary_zero(out_summary);
    compact_rows_free(&rows);
    return r;
}

CoreResult LineDrawingDataset_ExportPackV1(const Layout* layout,
                                           ViewPlaneAxis axis,
                                           const char* pack_path,
                                           LineDrawingDatasetSummary* out_summary) {
    LayoutCompactRows rows;
    CorePackWriter writer;
    CoreResult r;
    char meta_json[512];

    if (!layout || !pack_path) {
        CoreResult invalid = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return invalid;
    }

    summary_zero(out_summary);
    r = compact_rows_build(layout, &rows);
    if (r.code != CORE_OK) return r;

    r = core_pack_writer_open(pack_path, &writer);
    if (r.code != CORE_OK) {
        compact_rows_free(&rows);
        return r;
    }

    {
        uint32_t header[5];
        header[0] = LINE_DRAWING_DATASET_SCHEMA_VERSION;
        header[1] = rows.active_anchor_count;
        header[2] = rows.active_wall_count;
        header[3] = rows.curved_anchor_count;
        header[4] = rows.persistent_anchor_count;
        r = core_pack_writer_add_chunk(&writer, "LDHD", header, sizeof(header));
        if (r.code != CORE_OK) goto fail_close;
    }

    {
        int meta_len = snprintf(meta_json, sizeof(meta_json),
                                "{\"profile\":\"%s\",\"schema_family\":\"%s\",\"schema_variant\":\"%s\","
                                "\"schema_version\":%d,"
                                "\"projection_axis\":\"%s\","
                                "\"active_anchor_count\":%u,\"active_wall_count\":%u,"
                                "\"curved_anchor_count\":%u,\"persistent_anchor_count\":%u,"
                                "\"bounds\":{\"min\":[%.6f,%.6f,%.6f],\"max\":[%.6f,%.6f,%.6f]}}",
                                LINE_DRAWING_DATASET_PROFILE,
                                LINE_DRAWING_DATASET_FAMILY,
                                LINE_DRAWING_DATASET_VARIANT,
                                LINE_DRAWING_DATASET_SCHEMA_VERSION,
                                axis_to_string(axis),
                                rows.active_anchor_count,
                                rows.active_wall_count,
                                rows.curved_anchor_count,
                                rows.persistent_anchor_count,
                                rows.min_x, rows.min_y, rows.min_z,
                                rows.max_x, rows.max_y, rows.max_z);
        if (meta_len < 0 || (size_t)meta_len >= sizeof(meta_json)) {
            r.code = CORE_ERR_FORMAT;
            r.message = "metadata JSON overflow";
            goto fail_close;
        }

        r = core_pack_writer_add_chunk(&writer, "LDMJ", meta_json, (uint64_t)meta_len + 1u);
        if (r.code != CORE_OK) goto fail_close;
    }

    {
        uint32_t n = rows.active_anchor_count;
        LineDrawingAnchorPackBaseV1* base_rows = NULL;
        if (n > 0) {
            base_rows = (LineDrawingAnchorPackBaseV1*)core_alloc(sizeof(LineDrawingAnchorPackBaseV1) * n);
            if (!base_rows) {
                r.code = CORE_ERR_OUT_OF_MEMORY;
                r.message = "out of memory";
                goto fail_close;
            }
        }

        for (uint32_t i = 0; i < n; ++i) {
            const LineDrawingAnchorRowV1* src = &rows.anchors[i];
            LineDrawingAnchorPackBaseV1* dst = &base_rows[i];
            dst->x = src->x;
            dst->y = src->y;
            dst->handle_in_length = src->handle_in_length;
            dst->handle_in_angle_deg = src->handle_in_angle_deg;
            dst->handle_out_length = src->handle_out_length;
            dst->handle_out_angle_deg = src->handle_out_angle_deg;
            dst->persistent = src->persistent;
            dst->anchor_type = src->anchor_type;
        }

        r = core_pack_writer_add_chunk(&writer,
                                       "LDAN",
                                       base_rows,
                                       (uint64_t)(n * sizeof(LineDrawingAnchorPackBaseV1)));
        core_free(base_rows);
        if (r.code != CORE_OK) goto fail_close;
    }

    r = core_pack_writer_add_chunk(&writer,
                                   "LDWL",
                                   rows.walls,
                                   (uint64_t)(rows.active_wall_count * sizeof(LineDrawingWallRowV1)));
    if (r.code != CORE_OK) goto fail_close;

    {
        uint32_t n = rows.active_anchor_count;
        LineDrawingAnchorPackExt3DV1* ext = NULL;
        if (n > 0) {
            ext = (LineDrawingAnchorPackExt3DV1*)core_alloc(sizeof(LineDrawingAnchorPackExt3DV1) * n);
            if (!ext) {
                r.code = CORE_ERR_OUT_OF_MEMORY;
                r.message = "out of memory";
                goto fail_close;
            }
        }

        for (uint32_t i = 0; i < n; ++i) {
            const LineDrawingAnchorRowV1* src = &rows.anchors[i];
            ext[i].z = src->z;
            ext[i].handle_axis = src->handle_axis;
        }

        r = core_pack_writer_add_chunk(&writer,
                                       "LDA3",
                                       ext,
                                       (uint64_t)(n * sizeof(LineDrawingAnchorPackExt3DV1)));
        core_free(ext);
        if (r.code != CORE_OK) goto fail_close;
    }

    r = core_pack_writer_close(&writer);
    if (r.code != CORE_OK) {
        compact_rows_free(&rows);
        return r;
    }

    fill_summary_from_rows(&rows, out_summary);
    compact_rows_free(&rows);
    return core_result_ok();

fail_close:
    (void)core_pack_writer_close(&writer);
    compact_rows_free(&rows);
    return r;
}
