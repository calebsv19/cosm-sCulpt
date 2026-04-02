#pragma once

#include "Layout/layout.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool LineDrawingCanonicalScene_ExportLayoutToFile(const Layout* layout,
                                                  const char* sceneId,
                                                  const char* outputPath);

char* LineDrawingCanonicalScene_ExportLayoutToString(const Layout* layout,
                                                     const char* sceneId);

#ifdef __cplusplus
}
#endif

