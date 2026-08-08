#pragma once

#include "Core/SDLApp/sdl_app_framework.h"

#include <stdbool.h>

typedef void (*LineDrawingVulkanRolloutCallback)(AppContext* ctx);

bool LineDrawingVulkanRollout_Run(AppContext* ctx,
                                  LineDrawingVulkanRolloutCallback update,
                                  LineDrawingVulkanRolloutCallback render);
