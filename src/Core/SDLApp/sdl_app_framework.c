#include "sdl_app_framework.h"
#include "Core/global_state.h"
#include "Render/vulkan_adapter.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <stdio.h>

static bool app_try_recreate_swapchain(AppContext* ctx, int width, int height) {
    if (!ctx || !ctx->renderer || !ctx->window) {
        return false;
    }
    return VulkanAdapter_RecreateSwapchain(ctx, width, height);
}

bool App_Init(AppContext* ctx, const char* title, int width, int height, bool vsync) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
                         SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_VULKAN;
    ctx->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
					width, height, windowFlags);
    if (!ctx->window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    (void)vsync;
    if (!VulkanAdapter_Init(ctx, ctx->window)) {
        SDL_DestroyWindow(ctx->window);
        SDL_Quit();
        return false;
    }

    ctx->deltaTime = 0.0f;
    ctx->quit = false;
    ctx->userData = NULL;
    ctx->pending_swapchain_recreate = false;
    ctx->pending_swapchain_width = 0;
    ctx->pending_swapchain_height = 0;
    ctx->renderMode = RENDER_ALWAYS;
    ctx->renderThreshold = 0.033f;     // 30 FPS by default
    ctx->timeSinceLastRender = 0.0f;

    return true;
}

void App_Run(AppContext* ctx, AppCallbacks* callbacks) {
    Uint64 lastTime = SDL_GetPerformanceCounter();
    SDL_Event event;

    while (!ctx->quit) {
        // Input Handling
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                ctx->quit = true;
            }

            if (event.type == SDL_WINDOWEVENT &&
                (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                 event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
                int newW = event.window.data1;
                int newH = event.window.data2;
                Global_SetWindowSize(newW, newH);
                ctx->pending_swapchain_recreate = true;
                ctx->pending_swapchain_width = newW;
                ctx->pending_swapchain_height = newH;
            }
    
            if (callbacks && callbacks->handleInput) {
                callbacks->handleInput(ctx, &event);
            }
	}

        // Delta Time Calculation
        Uint64 now = SDL_GetPerformanceCounter();
	ctx->deltaTime = (float)(now - lastTime) / SDL_GetPerformanceFrequency();
	lastTime = now;
	
	ctx->timeSinceLastRender += ctx->deltaTime;
	
	// Update logic
	if (callbacks && callbacks->handleUpdate) {
	    callbacks->handleUpdate(ctx);
	}
	
        if (ctx->pending_swapchain_recreate) {
            if (app_try_recreate_swapchain(ctx,
                                           ctx->pending_swapchain_width,
                                           ctx->pending_swapchain_height)) {
                ctx->pending_swapchain_recreate = false;
            }
        }

	// Render logic
	bool shouldRender = false;
	
	if (ctx->renderMode == RENDER_ALWAYS) {
	    shouldRender = true;
	} else if (ctx->renderMode == RENDER_THROTTLED) {
	    if (ctx->timeSinceLastRender >= ctx->renderThreshold) {
	        shouldRender = true;
	    }
	}
	
	if (shouldRender && callbacks && callbacks->handleRender) {
	    App_RenderOnce(ctx, callbacks->handleRender);
	    ctx->timeSinceLastRender = 0.0f;
	}
        // Optional: You could insert a manual delay or frame cap here if needed.
    }
}

void App_Shutdown(AppContext* ctx) {
    if (ctx->renderer) {
        VulkanAdapter_Shutdown(ctx);
    }
    if (ctx->window) {
        SDL_DestroyWindow(ctx->window);
        ctx->window = NULL;
    }
    SDL_Quit();
}


void App_SetRenderMode(AppContext* ctx, RenderMode mode, float threshold) {
    ctx->renderMode = mode;
    ctx->renderThreshold = threshold;
}

bool App_RenderOnce(AppContext* ctx, void (*handleRender)(AppContext* ctx)) {
    static int logged_end_failure = 0;
    static int logged_extent_mismatch = 0;
    static int logged_pending_swapchain = 0;
    static int logged_minimized = 0;
    static int logged_drawable_zero = 0;
    static int logged_no_draw = 0;
    static int logged_draw_calls = 0;

    if (!ctx || !ctx->renderer || !ctx->window) {
        return false;
    }
    if (ctx->pending_swapchain_recreate) {
        if (!logged_pending_swapchain) {
            fprintf(stderr, "[LineDrawing] Waiting on swapchain recreate.\n");
            logged_pending_swapchain = 1;
        }
        return false;
    }
    logged_pending_swapchain = 0;
    Uint32 window_flags = SDL_GetWindowFlags(ctx->window);
    if (window_flags & SDL_WINDOW_MINIMIZED) {
        if (!logged_minimized) {
            fprintf(stderr, "[LineDrawing] Window minimized; skipping render.\n");
            logged_minimized = 1;
        }
        return false;
    }
    logged_minimized = 0;

    int winW = 0;
    int winH = 0;
    SDL_GetWindowSize(ctx->window, &winW, &winH);
    if (winW <= 0 || winH <= 0) {
        return false;
    }

    int drawableW = 0;
    int drawableH = 0;
    SDL_Vulkan_GetDrawableSize(ctx->window, &drawableW, &drawableH);
    if (drawableW <= 0 || drawableH <= 0) {
        if (!logged_drawable_zero) {
            fprintf(stderr, "[LineDrawing] Drawable size is zero; skipping render.\n");
            logged_drawable_zero = 1;
        }
        return false;
    }
    logged_drawable_zero = 0;
    Global_SetWindowSize(winW, winH);
    vk_renderer_set_logical_size(ctx->renderer, (float)winW, (float)winH);

    VkExtent2D swapExtent = ctx->renderer->context.swapchain.extent;
    if (swapExtent.width != (uint32_t)drawableW ||
        swapExtent.height != (uint32_t)drawableH) {
        if (!logged_extent_mismatch) {
            fprintf(stderr,
                    "[LineDrawing] Drawable size (%dx%d) differs from swapchain extent (%ux%u); "
                    "recreating swapchain.\n",
                    drawableW, drawableH, swapExtent.width, swapExtent.height);
            logged_extent_mismatch = 1;
        }
        app_try_recreate_swapchain(ctx, winW, winH);
        return false;
    }
    logged_extent_mismatch = 0;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (!VulkanAdapter_BeginFrame(ctx, &cmd)) {
        logged_end_failure = 0;
        return false;
    }

    if (handleRender) {
        handleRender(ctx);
    }

    if (ctx->renderer) {
        uint32_t draw_calls = ctx->renderer->draw_state.draw_call_count;
        if (!logged_draw_calls) {
            fprintf(stderr, "[LineDrawing] Vulkan draw calls this frame: %u\n", draw_calls);
            logged_draw_calls = 1;
        }
        if (draw_calls == 0) {
            if (!logged_no_draw) {
                fprintf(stderr, "[LineDrawing] No Vulkan draw calls recorded this frame.\n");
                logged_no_draw = 1;
            }
        } else if (logged_no_draw) {
            fprintf(stderr, "[LineDrawing] Vulkan draw calls resumed (%u).\n", draw_calls);
            logged_no_draw = 0;
        }
    }

    if (!VulkanAdapter_EndFrame(ctx, cmd)) {
        if (!logged_end_failure) {
            fprintf(stderr, "[LineDrawing] vk_renderer_end_frame failed.\n");
            logged_end_failure = 1;
        }
        return false;
    }
    logged_end_failure = 0;
    return true;
}
