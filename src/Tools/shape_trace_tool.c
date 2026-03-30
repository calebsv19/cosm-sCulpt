#include "ShapeLib/shape_core.h"
#include "ShapeLib/shape_json.h"
#include "core_trace.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SHAPE_TRACE_PROFILE_VERSION 1u

static void print_usage(const char* argv0) {
    fprintf(stderr,
            "usage: %s <shape_json> <out_trace_pack>\n",
            argv0 ? argv0 : "shape_trace_tool");
}

static float segment_length_approx(const ShapeSegment* seg) {
    if (!seg) return 0.0f;
    if (seg->type == SHAPE_SEGMENT_LINE) {
        const float dx = seg->p1.x - seg->p0.x;
        const float dy = seg->p1.y - seg->p0.y;
        return sqrtf(dx * dx + dy * dy);
    }

    {
        const float d0x = seg->c1.x - seg->p0.x;
        const float d0y = seg->c1.y - seg->p0.y;
        const float d1x = seg->c2.x - seg->c1.x;
        const float d1y = seg->c2.y - seg->c1.y;
        const float d2x = seg->p1.x - seg->c2.x;
        const float d2y = seg->p1.y - seg->c2.y;
        return sqrtf(d0x * d0x + d0y * d0y) +
               sqrtf(d1x * d1x + d1y * d1y) +
               sqrtf(d2x * d2x + d2y * d2y);
    }
}

int main(int argc, char** argv) {
    const char* in_path;
    const char* out_path;
    ShapeDocument doc;
    size_t shape_idx;
    size_t path_idx;
    size_t seg_idx;
    size_t total_segments = 0u;
    size_t total_paths = 0u;
    size_t total_shapes = 0u;
    size_t line_segments = 0u;
    size_t cubic_segments = 0u;
    CoreTraceConfig cfg;
    CoreTraceSession session;
    CoreResult r;

    if (argc != 3) {
        print_usage(argc > 0 ? argv[0] : "shape_trace_tool");
        return 1;
    }

    in_path = argv[1];
    out_path = argv[2];

    memset(&doc, 0, sizeof(doc));
    if (!ShapeDocument_LoadFromJsonFile(in_path, &doc)) {
        fprintf(stderr, "[shape_trace_tool] failed to load shape json: %s\n", in_path);
        return 1;
    }

    for (shape_idx = 0; shape_idx < doc.shapeCount; ++shape_idx) {
        const Shape* s = &doc.shapes[shape_idx];
        total_shapes += 1u;
        total_paths += s->pathCount;
        for (path_idx = 0; path_idx < s->pathCount; ++path_idx) {
            total_segments += s->paths[path_idx].segmentCount;
        }
    }

    cfg.sample_capacity = (total_segments * 4u) + 32u;
    cfg.marker_capacity = (total_shapes + total_paths + 32u);
    r = core_trace_session_init(&session, &cfg);
    if (r.code != CORE_OK) {
        fprintf(stderr, "[shape_trace_tool] trace init failed: %s\n", r.message);
        ShapeDocument_Free(&doc);
        return 1;
    }

    {
        char label[CORE_TRACE_MARKER_LABEL_MAX] = {0};
        snprintf(label, sizeof(label), "shape_trace_v%u", SHAPE_TRACE_PROFILE_VERSION);
        (void)core_trace_emit_marker(&session, "shape_marker", 0.0, label);
    }

    {
        double t = 1.0;
        size_t global_seg_index = 0u;

        for (shape_idx = 0; shape_idx < doc.shapeCount; ++shape_idx) {
            const Shape* s = &doc.shapes[shape_idx];
            char shape_label[CORE_TRACE_MARKER_LABEL_MAX] = {0};
            snprintf(shape_label, sizeof(shape_label), "shape_%zu", shape_idx);
            (void)core_trace_emit_marker(&session, "shape_marker", t, shape_label);

            for (path_idx = 0; path_idx < s->pathCount; ++path_idx) {
                const ShapePath* p = &s->paths[path_idx];
                char path_label[CORE_TRACE_MARKER_LABEL_MAX] = {0};
                snprintf(path_label, sizeof(path_label), "shape_%zu_path_%zu", shape_idx, path_idx);
                (void)core_trace_emit_marker(&session, "shape_marker", t, path_label);

                for (seg_idx = 0; seg_idx < p->segmentCount; ++seg_idx) {
                    const ShapeSegment* seg = &p->segments[seg_idx];
                    const float seg_type = (seg->type == SHAPE_SEGMENT_CUBIC_BEZIER) ? 1.0f : 0.0f;
                    const float seg_len = segment_length_approx(seg);

                    if (seg->type == SHAPE_SEGMENT_CUBIC_BEZIER) cubic_segments += 1u;
                    else line_segments += 1u;

                    (void)core_trace_emit_sample_f32(&session, "seg_type", t, seg_type);
                    (void)core_trace_emit_sample_f32(&session, "seg_len", t, seg_len);
                    (void)core_trace_emit_sample_f32(&session, "path_index", t, (float)path_idx);
                    (void)core_trace_emit_sample_f32(&session, "segment_index", t, (float)global_seg_index);

                    global_seg_index += 1u;
                    t += 1.0;
                }
            }
        }
    }

    r = core_trace_finalize(&session);
    if (r.code != CORE_OK) {
        fprintf(stderr, "[shape_trace_tool] trace finalize failed: %s\n", r.message);
        core_trace_session_reset(&session);
        ShapeDocument_Free(&doc);
        return 1;
    }

    r = core_trace_export_pack(&session, out_path);
    if (r.code != CORE_OK) {
        fprintf(stderr, "[shape_trace_tool] trace export failed: %s\n", r.message);
        core_trace_session_reset(&session);
        ShapeDocument_Free(&doc);
        return 1;
    }

    printf("[shape_trace_tool] wrote %s\n", out_path);
    printf("[shape_trace_tool] shapes=%zu paths=%zu segments=%zu\n", total_shapes, total_paths, total_segments);
    printf("[shape_trace_tool] segment_types line=%zu cubic=%zu\n", line_segments, cubic_segments);
    printf("[shape_trace_tool] lanes=seg_type,seg_len,path_index,segment_index markers=shape_marker\n");

    core_trace_session_reset(&session);
    ShapeDocument_Free(&doc);
    return 0;
}
