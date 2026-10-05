#pragma once

#include <stdbool.h>

typedef struct SDLAppLoopWaitPolicyInput {
    bool high_intensity_mode;
    bool interaction_active;
    bool background_busy;
    bool resize_pending;
} SDLAppLoopWaitPolicyInput;

int SDLAppLoop_ComputeWaitTimeoutMs(const SDLAppLoopWaitPolicyInput* input);

/* Dirty input is immediate; timed work and retries have explicit deadlines. */
int SDLAppLoop_DemandWaitMs(bool dirty, bool suspended, bool retry, int timed_delay_ms);
