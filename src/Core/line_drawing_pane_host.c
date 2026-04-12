#include "Core/line_drawing_pane_host.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    LINE_DRAWING_PANE_ID_TOP = 1101u,
    LINE_DRAWING_PANE_ID_LEFT = 1102u,
    LINE_DRAWING_PANE_ID_CENTER = 1103u,
    LINE_DRAWING_PANE_ID_RIGHT = 1104u
};

enum {
    LINE_DRAWING_MODULE_TYPE_TOP = 1201u,
    LINE_DRAWING_MODULE_TYPE_LEFT = 1202u,
    LINE_DRAWING_MODULE_TYPE_CENTER = 1203u,
    LINE_DRAWING_MODULE_TYPE_RIGHT = 1204u
};

static void LineDrawingPaneHost_SetError(LineDrawingPaneHost* host, const char* fmt, ...) {
    va_list args;
    if (!host || !fmt) return;
    host->last_error[0] = '\0';
    va_start(args, fmt);
    (void)vsnprintf(host->last_error, sizeof(host->last_error), fmt, args);
    va_end(args);
}

static uint32_t LineDrawingPaneHost_PaneIdForRole(LineDrawingPaneRole role) {
    switch (role) {
        case LINE_DRAWING_PANE_ROLE_TOP_BAR: return LINE_DRAWING_PANE_ID_TOP;
        case LINE_DRAWING_PANE_ROLE_LEFT_CONTROLS: return LINE_DRAWING_PANE_ID_LEFT;
        case LINE_DRAWING_PANE_ROLE_CENTER_CANVAS: return LINE_DRAWING_PANE_ID_CENTER;
        case LINE_DRAWING_PANE_ROLE_RIGHT_CONTROLS: return LINE_DRAWING_PANE_ID_RIGHT;
        default: return 0u;
    }
}

static void LineDrawingPaneHost_NoopRender(void* host_context, uint32_t pane_node_id, uint32_t instance_id) {
    (void)host_context;
    (void)pane_node_id;
    (void)instance_id;
}

static bool LineDrawingPaneHost_RegisterModuleDescriptors(LineDrawingPaneHost* host) {
    CorePaneModuleResult result = CORE_PANE_MODULE_ERR_INVALID_ARG;
    CorePaneModuleDescriptor descriptor;

    if (!host) return false;
    result = core_pane_module_registry_init(&host->module_registry,
                                            host->module_entries,
                                            LINE_DRAWING_PANE_MODULE_REGISTRY_CAPACITY);
    if (result != CORE_PANE_MODULE_OK) {
        LineDrawingPaneHost_SetError(host, "module registry init failed (%d)", (int)result);
        return false;
    }

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.module_type_id = LINE_DRAWING_MODULE_TYPE_TOP;
    descriptor.module_key = "top_bar";
    descriptor.display_name = "Top Bar";
    descriptor.version_major = 1u;
    descriptor.version_minor = 0u;
    descriptor.capabilities = CORE_PANE_MODULE_CAP_RENDER;
    descriptor.provider_kind = CORE_PANE_MODULE_PROVIDER_INTERNAL;
    descriptor.render = LineDrawingPaneHost_NoopRender;
    result = core_pane_module_register(&host->module_registry, &descriptor);
    if (result != CORE_PANE_MODULE_OK) {
        LineDrawingPaneHost_SetError(host, "register top module failed (%d)", (int)result);
        return false;
    }

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.module_type_id = LINE_DRAWING_MODULE_TYPE_LEFT;
    descriptor.module_key = "left_controls";
    descriptor.display_name = "Left Controls";
    descriptor.version_major = 1u;
    descriptor.version_minor = 0u;
    descriptor.capabilities = CORE_PANE_MODULE_CAP_RENDER;
    descriptor.provider_kind = CORE_PANE_MODULE_PROVIDER_INTERNAL;
    descriptor.render = LineDrawingPaneHost_NoopRender;
    result = core_pane_module_register(&host->module_registry, &descriptor);
    if (result != CORE_PANE_MODULE_OK) {
        LineDrawingPaneHost_SetError(host, "register left module failed (%d)", (int)result);
        return false;
    }

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.module_type_id = LINE_DRAWING_MODULE_TYPE_CENTER;
    descriptor.module_key = "center_canvas";
    descriptor.display_name = "Center Canvas";
    descriptor.version_major = 1u;
    descriptor.version_minor = 0u;
    descriptor.capabilities = CORE_PANE_MODULE_CAP_RENDER;
    descriptor.provider_kind = CORE_PANE_MODULE_PROVIDER_INTERNAL;
    descriptor.render = LineDrawingPaneHost_NoopRender;
    result = core_pane_module_register(&host->module_registry, &descriptor);
    if (result != CORE_PANE_MODULE_OK) {
        LineDrawingPaneHost_SetError(host, "register center module failed (%d)", (int)result);
        return false;
    }

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.module_type_id = LINE_DRAWING_MODULE_TYPE_RIGHT;
    descriptor.module_key = "right_controls";
    descriptor.display_name = "Right Controls";
    descriptor.version_major = 1u;
    descriptor.version_minor = 0u;
    descriptor.capabilities = CORE_PANE_MODULE_CAP_RENDER;
    descriptor.provider_kind = CORE_PANE_MODULE_PROVIDER_INTERNAL;
    descriptor.render = LineDrawingPaneHost_NoopRender;
    result = core_pane_module_register(&host->module_registry, &descriptor);
    if (result != CORE_PANE_MODULE_OK) {
        LineDrawingPaneHost_SetError(host, "register right module failed (%d)", (int)result);
        return false;
    }
    return true;
}

static void LineDrawingPaneHost_SeedGraph(LineDrawingPaneHost* host) {
    if (!host) return;

    host->node_count = 7u;
    host->root_index = 0u;

    host->nodes[0] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 1u,
        .axis = CORE_PANE_AXIS_VERTICAL,
        .ratio_01 = 0.085f,
        .child_a = 1u,
        .child_b = 2u,
        .constraints = { 64.0f, 240.0f }
    };
    host->nodes[1] = (CorePaneNode){
        .type = CORE_PANE_NODE_LEAF,
        .id = LINE_DRAWING_PANE_ID_TOP
    };
    host->nodes[2] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 2u,
        .axis = CORE_PANE_AXIS_HORIZONTAL,
        .ratio_01 = 0.145f,
        .child_a = 3u,
        .child_b = 4u,
        .constraints = { 146.0f, 320.0f }
    };
    host->nodes[3] = (CorePaneNode){
        .type = CORE_PANE_NODE_LEAF,
        .id = LINE_DRAWING_PANE_ID_LEFT
    };
    host->nodes[4] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 3u,
        .axis = CORE_PANE_AXIS_HORIZONTAL,
        .ratio_01 = 0.825f,
        .child_a = 5u,
        .child_b = 6u,
        .constraints = { 320.0f, 146.0f }
    };
    host->nodes[5] = (CorePaneNode){
        .type = CORE_PANE_NODE_LEAF,
        .id = LINE_DRAWING_PANE_ID_CENTER
    };
    host->nodes[6] = (CorePaneNode){
        .type = CORE_PANE_NODE_LEAF,
        .id = LINE_DRAWING_PANE_ID_RIGHT
    };
}

static float LineDrawingPaneHost_Clamp(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

bool LineDrawingPaneHost_Rebuild(LineDrawingPaneHost* host, float width, float height) {
    CorePaneRect bounds;
    CorePaneValidationReport report;
    uint32_t leaf_ids[LINE_DRAWING_PANE_LEAF_CAPACITY];
    CorePaneModuleResult module_result = CORE_PANE_MODULE_ERR_INVALID_ARG;
    uint32_t i = 0u;

    if (!host) return false;
    if (width < 64.0f || height < 64.0f) {
        LineDrawingPaneHost_SetError(host, "invalid bounds %.2fx%.2f", width, height);
        return false;
    }

    /* Keep pane sizing near fixed authoring chrome targets while preserving min constraints. */
    {
        const float fallback_side = 184.0f;
        const float min_center = 320.0f;
        float target_top = (host->target_top_height > 0.0f) ? host->target_top_height : 64.0f;
        float target_left = (host->target_left_width > 0.0f) ? host->target_left_width : fallback_side;
        float target_right = (host->target_right_width > 0.0f) ? host->target_right_width : fallback_side;
        float min_top = 48.0f;
        float min_left = 96.0f;
        float min_right = 96.0f;
        float ratio = 0.0f;
        float remaining = 0.0f;
        float target_center = 0.0f;
        float max_top = 0.0f;
        float max_left = 0.0f;
        float max_right = 0.0f;

        if (host->target_top_height > 0.0f) {
            min_top = fmaxf(48.0f, host->target_top_height - 10.0f);
        }
        if (host->target_left_width > 0.0f) {
            min_left = fmaxf(92.0f, host->target_left_width - 14.0f);
        }
        if (host->target_right_width > 0.0f) {
            min_right = fmaxf(92.0f, host->target_right_width - 14.0f);
        }

        host->nodes[0].constraints = (CorePaneConstraints){ min_top, 220.0f };
        host->nodes[2].constraints = (CorePaneConstraints){ min_left, min_center + min_right };
        host->nodes[4].constraints = (CorePaneConstraints){ min_center, min_right };

        max_top = fmaxf(min_top, height - 220.0f);
        target_top = LineDrawingPaneHost_Clamp(target_top, min_top, max_top);
        ratio = target_top / height;
        if (ratio < 0.05f) ratio = 0.05f;
        if (ratio > 0.18f) ratio = 0.18f;
        host->nodes[0].ratio_01 = ratio;

        max_left = fmaxf(min_left, width - (min_center + min_right));
        target_left = LineDrawingPaneHost_Clamp(target_left, min_left, max_left);
        ratio = target_left / width;
        if (ratio < 0.10f) ratio = 0.10f;
        if (ratio > 0.28f) ratio = 0.28f;
        host->nodes[2].ratio_01 = ratio;

        remaining = width - target_left;
        if (remaining < (min_center + min_right)) remaining = min_center + min_right;
        max_right = fmaxf(min_right, remaining - min_center);
        target_right = LineDrawingPaneHost_Clamp(target_right, min_right, max_right);
        target_center = remaining - target_right;
        if (target_center < min_center) target_center = min_center;
        ratio = target_center / remaining;
        if (ratio < 0.50f) ratio = 0.50f;
        if (ratio > 0.90f) ratio = 0.90f;
        host->nodes[4].ratio_01 = ratio;
    }

    bounds = (CorePaneRect){ 0.0f, 0.0f, width, height };
    memset(&report, 0, sizeof(report));
    if (!core_pane_validate_graph(host->nodes,
                                  host->node_count,
                                  host->root_index,
                                  bounds,
                                  &report)) {
        LineDrawingPaneHost_SetError(host,
                                     "pane graph invalid code=%s node=%u rel=%u",
                                     core_pane_validation_code_string(report.code),
                                     report.node_index,
                                     report.related_index);
        return false;
    }
    if (!core_pane_solve(host->nodes,
                         host->node_count,
                         host->root_index,
                         bounds,
                         host->leaves,
                         LINE_DRAWING_PANE_LEAF_CAPACITY,
                         &host->leaf_count)) {
        LineDrawingPaneHost_SetError(host, "core_pane_solve failed");
        return false;
    }

    for (i = 0u; i < host->leaf_count; ++i) {
        leaf_ids[i] = host->leaves[i].id;
    }
    module_result = core_pane_module_validate_bindings(&host->module_registry,
                                                       host->module_bindings,
                                                       host->module_binding_count,
                                                       leaf_ids,
                                                       host->leaf_count);
    if (module_result != CORE_PANE_MODULE_OK) {
        LineDrawingPaneHost_SetError(host, "pane module bindings invalid (%d)", (int)module_result);
        return false;
    }

    host->bounds_width = width;
    host->bounds_height = height;
    core_layout_acknowledge_rebuild(&host->layout_state);
    host->last_error[0] = '\0';
    return true;
}

void LineDrawingPaneHost_SetChromeTargets(LineDrawingPaneHost* host,
                                          float top_height,
                                          float left_width,
                                          float right_width) {
    if (!host) return;
    host->target_top_height = top_height;
    host->target_left_width = left_width;
    host->target_right_width = right_width;
}

bool LineDrawingPaneHost_Init(LineDrawingPaneHost* host, float width, float height) {
    if (!host) return false;
    memset(host, 0, sizeof(*host));
    core_layout_state_init(&host->layout_state);
    LineDrawingPaneHost_SeedGraph(host);

    if (!LineDrawingPaneHost_RegisterModuleDescriptors(host)) {
        return false;
    }

    host->module_binding_count = 4u;
    host->module_bindings[0] = (CorePaneModuleBinding){
        .instance_id = 1u,
        .pane_node_id = LINE_DRAWING_PANE_ID_TOP,
        .module_type_id = LINE_DRAWING_MODULE_TYPE_TOP,
        .config_variant = 0u,
        .runtime_flags = 0u
    };
    host->module_bindings[1] = (CorePaneModuleBinding){
        .instance_id = 2u,
        .pane_node_id = LINE_DRAWING_PANE_ID_LEFT,
        .module_type_id = LINE_DRAWING_MODULE_TYPE_LEFT,
        .config_variant = 0u,
        .runtime_flags = 0u
    };
    host->module_bindings[2] = (CorePaneModuleBinding){
        .instance_id = 3u,
        .pane_node_id = LINE_DRAWING_PANE_ID_CENTER,
        .module_type_id = LINE_DRAWING_MODULE_TYPE_CENTER,
        .config_variant = 0u,
        .runtime_flags = 0u
    };
    host->module_bindings[3] = (CorePaneModuleBinding){
        .instance_id = 4u,
        .pane_node_id = LINE_DRAWING_PANE_ID_RIGHT,
        .module_type_id = LINE_DRAWING_MODULE_TYPE_RIGHT,
        .config_variant = 0u,
        .runtime_flags = 0u
    };

    if (!LineDrawingPaneHost_Rebuild(host, width, height)) {
        return false;
    }

    host->initialized = true;
    return true;
}

bool LineDrawingPaneHost_GetRectForRole(const LineDrawingPaneHost* host,
                                        LineDrawingPaneRole role,
                                        CorePaneRect* out_rect) {
    uint32_t pane_id = 0u;
    uint32_t i = 0u;
    if (!host || !out_rect || !host->initialized) return false;

    pane_id = LineDrawingPaneHost_PaneIdForRole(role);
    if (pane_id == 0u) return false;
    for (i = 0u; i < host->leaf_count; ++i) {
        if (host->leaves[i].id == pane_id) {
            *out_rect = host->leaves[i].rect;
            return true;
        }
    }
    return false;
}

const char* LineDrawingPaneHost_LastError(const LineDrawingPaneHost* host) {
    if (!host || !host->last_error[0]) return "none";
    return host->last_error;
}
