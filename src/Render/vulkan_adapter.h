#pragma once

#include "Core/SDLApp/sdl_app_framework.h"
#include "vk_renderer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool VulkanAdapter_Init(AppContext* ctx, SDL_Window* window);
void VulkanAdapter_Shutdown(AppContext* ctx);
bool VulkanAdapter_RecreateSwapchain(AppContext* ctx, int width, int height);
bool VulkanAdapter_BeginFrame(AppContext* ctx, VkCommandBuffer* out_cmd);
bool VulkanAdapter_EndFrame(AppContext* ctx, VkCommandBuffer cmd);

void VulkanAdapter_Clear(SDL_Renderer* renderer, int width, int height, SDL_Color color);
void VulkanAdapter_DrawText(SDL_Renderer* renderer, TTF_Font* font,
                            const char* text, int x, int y, SDL_Color color);

#ifdef __cplusplus
}
#endif
