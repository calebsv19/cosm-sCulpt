#pragma once

#include <stdbool.h>

typedef struct SDLAppLoopWaitPolicyInput {
    bool high_intensity_mode;
    bool interaction_active;
    bool background_busy;
    bool resize_pending;
} SDLAppLoopWaitPolicyInput;

int SDLAppLoop_ComputeWaitTimeoutMs(const SDLAppLoopWaitPolicyInput* input);
