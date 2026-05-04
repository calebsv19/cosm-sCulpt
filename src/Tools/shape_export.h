#pragma once

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHAPE_EXPORT_PATH_MAX 512

// Returns the directory where all shape exports are stored.
const char* ShapeExport_GetExportDir(void);

// Build an export path inside the provided root using only the basename of
// the requested file. The resulting path is written to outPath.
bool ShapeExport_BuildPathInRoot(const char* root,
                                 const char* requestedName,
                                 char* outPath,
                                 size_t outSize);

// Build an export path inside the shared export directory using only the
// basename of the requested file. The resulting path is written to outPath.
bool ShapeExport_BuildPath(const char* requestedName,
                           char* outPath,
                           size_t outSize);

#ifdef __cplusplus
}
#endif
