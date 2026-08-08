#include "Render/vulkan_rollout.h"

#include "Core/global_state.h"
#include "Render/vulkan_adapter.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if LINE_DRAWING_VK_RUNTIME_AVAILABLE
#include "vk_runtime.h"

static const char* rollout_capture_path(const char* variable, const char* fallback) {
    const char* value = getenv(variable);
    return value && value[0] ? value : fallback;
}

static double rollout_minimum_scale(void) {
    const char* value = getenv("LINE_DRAWING_VULKAN_ROLLOUT_MIN_SCALE");
    char* end = NULL;
    double parsed = value && value[0] ? strtod(value, &end) : 1.0;
    if (!isfinite(parsed) || parsed < 1.0 || parsed > 4.0 || !end || *end != '\0') {
        return 1.0;
    }
    return parsed;
}

static bool rollout_extent(AppContext* ctx,
                           VkExtent2D* out_extent,
                           double* out_scale) {
    int logical_width = 0;
    int logical_height = 0;
    int drawable_width = 0;
    int drawable_height = 0;
    double scale_x = 0.0;
    double scale_y = 0.0;
    if (!ctx || !ctx->window || !ctx->renderer || !out_extent || !out_scale) return false;
    SDL_GetWindowSize(ctx->window, &logical_width, &logical_height);
    SDL_Vulkan_GetDrawableSize(ctx->window, &drawable_width, &drawable_height);
    if (logical_width <= 0 || logical_height <= 0 || drawable_width <= 0 || drawable_height <= 0) {
        return false;
    }
    scale_x = (double)drawable_width / (double)logical_width;
    scale_y = (double)drawable_height / (double)logical_height;
    if (!isfinite(scale_x) || !isfinite(scale_y) || fabs(scale_x - scale_y) > 0.01 ||
        scale_x < rollout_minimum_scale()) {
        return false;
    }
    *out_extent = ctx->renderer->context.swapchain.extent;
    *out_scale = scale_x;
    return out_extent->width == (uint32_t)drawable_width &&
           out_extent->height == (uint32_t)drawable_height;
}

static bool rollout_verify_runtime(AppContext* ctx, const char* stage) {
    VkRendererDevice* device = NULL;
    const VkRuntimeCapabilityReport* report = NULL;
    const char* version = vk_runtime_version_string();
    if (!ctx || !ctx->renderer || !ctx->renderer->context.device) return false;
    device = ctx->renderer->context.device;
    report = vk_runtime_get_capability_report(&device->runtime);
    if (!version || !version[0] || !report || report->status != VK_RUNTIME_STATUS_OK ||
        report->device_count == 0u || report->selected_device_index >= report->device_count ||
        !report->validation_requested || !report->validation_available ||
        !report->validation_enabled || report->validation_load_failed ||
        report->validation_warning_count != 0u || report->validation_error_count != 0u ||
        device->instance != device->runtime.instance ||
        device->device != device->runtime.device ||
        device->graphics_queue != device->runtime.graphics_queue ||
        device->present_queue != device->runtime.present_queue) {
        fprintf(stderr, "LINE_DRAWING_VULKAN_RUNTIME stage=%s status=fail\n", stage);
        return false;
    }
    printf("LINE_DRAWING_VULKAN_RUNTIME schema=1 stage=%s runtime=%s device=%s "
           "validation_requested=1 validation_enabled=1 warnings=%u errors=%u\n",
           stage,
           version,
           report->devices[report->selected_device_index].device_name,
           (unsigned int)report->validation_warning_count,
           (unsigned int)report->validation_error_count);
    return true;
}

static bool rollout_render(AppContext* ctx,
                           LineDrawingVulkanRolloutCallback update,
                           LineDrawingVulkanRolloutCallback render,
                           const char* capture) {
    bool rendered = false;
    if (update) update(ctx);
    if (capture && vk_renderer_request_capture(ctx->renderer, capture) != VK_SUCCESS) return false;
    rendered = App_RenderOnce(ctx, render);
    if (rendered) {
        /* The exact 1.3.1 compatibility surface exposes conservative idle
           synchronization for deterministic lifecycle/readback proofs. */
        vk_renderer_wait_idle(ctx->renderer);
    }
    return rendered;
}

bool LineDrawingVulkanRollout_Run(AppContext* ctx,
                                  LineDrawingVulkanRolloutCallback update,
                                  LineDrawingVulkanRolloutCallback render) {
    const char* initial_capture = rollout_capture_path(
        "LINE_DRAWING_VULKAN_ROLLOUT_INITIAL_CAPTURE", "line-drawing-initial.bmp");
    const char* resized_capture = rollout_capture_path(
        "LINE_DRAWING_VULKAN_ROLLOUT_RESIZED_CAPTURE", "line-drawing-resized.bmp");
    VkExtent2D initial_extent = {0};
    VkExtent2D resized_extent = {0};
    double initial_scale = 0.0;
    double resized_scale = 0.0;

    if (!ctx || !rollout_verify_runtime(ctx, "startup") ||
        !rollout_render(ctx, update, render, NULL) ||
        !rollout_extent(ctx, &initial_extent, &initial_scale) ||
        !rollout_render(ctx, update, render, initial_capture)) {
        return false;
    }

    SDL_SetWindowSize(ctx->window, 1440, 900);
    SDL_Delay(100u);
    SDL_PumpEvents();
    Global_SetWindowSize(1440, 900);
    if (!VulkanAdapter_RecreateSwapchain(ctx, 1440, 900) ||
        !rollout_render(ctx, update, render, resized_capture) ||
        !rollout_extent(ctx, &resized_extent, &resized_scale) ||
        (initial_extent.width == resized_extent.width &&
         initial_extent.height == resized_extent.height) ||
        !rollout_verify_runtime(ctx, "resized")) {
        return false;
    }

    VulkanAdapter_Shutdown(ctx);
    if (!VulkanAdapter_Init(ctx, ctx->window) ||
        !rollout_render(ctx, update, render, NULL) ||
        !rollout_verify_runtime(ctx, "restart")) {
        return false;
    }

    printf("LINE_DRAWING_VULKAN_ROLLOUT schema=1 status=pass "
           "initial=%ux%u resized=%ux%u initial_scale=%.3f resized_scale=%.3f "
           "initial_capture=%s resized_capture=%s\n",
           initial_extent.width,
           initial_extent.height,
           resized_extent.width,
           resized_extent.height,
           initial_scale,
           resized_scale,
           initial_capture,
           resized_capture);
    return true;
}

#else

bool LineDrawingVulkanRollout_Run(AppContext* ctx,
                                  LineDrawingVulkanRolloutCallback update,
                                  LineDrawingVulkanRolloutCallback render) {
    (void)ctx;
    (void)update;
    (void)render;
    fprintf(stderr, "LINE_DRAWING_VULKAN_ROLLOUT status=fail reason=vk_runtime_unavailable\n");
    return false;
}

#endif
