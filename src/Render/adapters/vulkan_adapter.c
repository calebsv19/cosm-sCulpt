#include "Render/vulkan_adapter.h"
#include "UI/text_draw.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

bool VulkanAdapter_Init(AppContext* ctx, SDL_Window* window) {
    if (!ctx || !window) {
        return false;
    }

    VkRendererConfig cfg;
    vk_renderer_config_set_defaults(&cfg);
    cfg.enable_validation = SDL_FALSE;
    cfg.clear_color[0] = 20.0f / 255.0f;
    cfg.clear_color[1] = 20.0f / 255.0f;
    cfg.clear_color[2] = 23.0f / 255.0f;
    cfg.clear_color[3] = 1.0f;

    VkResult init = vk_renderer_init(&ctx->renderer_storage, window, &cfg);
    if (init != VK_SUCCESS) {
        SDL_Log("Failed to initialize Vulkan renderer (code %d)", init);
        return false;
    }

    ctx->renderer = &ctx->renderer_storage;
    return true;
}

void VulkanAdapter_Shutdown(AppContext* ctx) {
    if (!ctx || !ctx->renderer) {
        return;
    }
    line_drawing_text_reset_renderer((SDL_Renderer*)ctx->renderer);
    vk_renderer_shutdown(ctx->renderer);
    ctx->renderer = NULL;
}

bool VulkanAdapter_RecreateSwapchain(AppContext* ctx, int width, int height) {
    if (!ctx || !ctx->renderer || !ctx->window) {
        return false;
    }
    if (width <= 0 || height <= 0) {
        return false;
    }
    VkResult result = vk_renderer_recreate_swapchain(ctx->renderer, ctx->window);
    if (result != VK_SUCCESS) {
        return false;
    }
    vk_renderer_set_logical_size(ctx->renderer, (float)width, (float)height);
    return true;
}

bool VulkanAdapter_BeginFrame(AppContext* ctx, VkCommandBuffer* out_cmd) {
    static int logged_begin_out_of_date = 0;
    static int logged_begin_failure = 0;

    if (!ctx || !ctx->renderer || !out_cmd) {
        return false;
    }

    VkFramebuffer fb = VK_NULL_HANDLE;
    VkExtent2D extent = {0};
    VkResult frame = vk_renderer_begin_frame(ctx->renderer, out_cmd, &fb, &extent);
    if (frame == VK_ERROR_OUT_OF_DATE_KHR || frame == VK_SUBOPTIMAL_KHR) {
        if (!logged_begin_out_of_date) {
            fprintf(stderr,
                    "[LineDrawing] vk_renderer_begin_frame reported %s; recreating swapchain.\n",
                    (frame == VK_SUBOPTIMAL_KHR) ? "SUBOPTIMAL" : "OUT_OF_DATE");
            logged_begin_out_of_date = 1;
        }
        int winW = 0;
        int winH = 0;
        SDL_GetWindowSize(ctx->window, &winW, &winH);
        VulkanAdapter_RecreateSwapchain(ctx, winW, winH);
        return false;
    }
    logged_begin_out_of_date = 0;
    if (frame != VK_SUCCESS) {
        if (!logged_begin_failure) {
            fprintf(stderr, "[LineDrawing] vk_renderer_begin_frame failed: %d\n", frame);
            logged_begin_failure = 1;
        }
        return false;
    }
    logged_begin_failure = 0;
    return true;
}

bool VulkanAdapter_EndFrame(AppContext* ctx, VkCommandBuffer cmd) {
    static int logged_end_failure = 0;

    if (!ctx || !ctx->renderer) {
        return false;
    }

    VkResult end = vk_renderer_end_frame(ctx->renderer, cmd);
    if (end == VK_ERROR_OUT_OF_DATE_KHR || end == VK_SUBOPTIMAL_KHR) {
        if (!logged_end_failure) {
            fprintf(stderr,
                    "[LineDrawing] vk_renderer_end_frame reported %s; recreating swapchain.\n",
                    (end == VK_SUBOPTIMAL_KHR) ? "SUBOPTIMAL" : "OUT_OF_DATE");
            logged_end_failure = 1;
        }
        int winW = 0;
        int winH = 0;
        SDL_GetWindowSize(ctx->window, &winW, &winH);
        VulkanAdapter_RecreateSwapchain(ctx, winW, winH);
        return false;
    }
    if (end != VK_SUCCESS) {
        if (!logged_end_failure) {
            fprintf(stderr, "[LineDrawing] vk_renderer_end_frame failed: %d\n", end);
            logged_end_failure = 1;
        }
        return false;
    }
    logged_end_failure = 0;
    return true;
}

void VulkanAdapter_Clear(SDL_Renderer* renderer, int width, int height, SDL_Color color) {
    if (!renderer || width <= 0 || height <= 0) {
        return;
    }
    SDL_Rect rect = {0, 0, width, height};
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

void VulkanAdapter_DrawText(SDL_Renderer* renderer, TTF_Font* font,
                            const char* text, int x, int y, SDL_Color color) {
    if (!renderer || !font || !text || !text[0]) {
        return;
    }
    (void)line_drawing_text_draw_utf8_at(renderer, font, text, x, y, color);
}
