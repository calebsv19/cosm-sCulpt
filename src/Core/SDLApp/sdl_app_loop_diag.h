#pragma once

#include <stdint.h>

void SDLAppLoop_DiagTick(double frame_elapsed_sec,
                         uint32_t wait_blocked_ms,
                         uint32_t wait_call_count);
