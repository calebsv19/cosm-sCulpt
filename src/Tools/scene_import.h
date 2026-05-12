#pragma once

#include "Layout/layout.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool LineDrawingSceneImport_LoadLayoutFromAuthoringFile(Layout* layout,
                                                        const char* authoring_path,
                                                        char* diagnostics,
                                                        size_t diagnostics_size);

#ifdef __cplusplus
}
#endif
