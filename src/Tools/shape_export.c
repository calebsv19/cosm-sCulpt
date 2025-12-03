#include "Tools/shape_export.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(_WIN32)
#include <direct.h>
#endif

static const char* kShapeExportDir = "export";

const char* ShapeExport_GetExportDir(void) {
    return kShapeExportDir;
}

static bool EnsureDirectoryExists(const char* path) {
    if (!path || !path[0]) return false;

    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

#if defined(_WIN32)
    if (_mkdir(path) == 0) {
        return true;
    }
#else
    if (mkdir(path, 0755) == 0) {
        return true;
    }
#endif

    if (errno == EEXIST) {
        return true;
    }
    return false;
}

static const char* ExtractFileName(const char* path) {
    if (!path) return NULL;
    const char* slash = strrchr(path, '/');
    const char* backslash = strrchr(path, '\\');
    const char* start = path;
    if (slash && backslash) {
        start = (slash > backslash) ? slash + 1 : backslash + 1;
    } else if (slash) {
        start = slash + 1;
    } else if (backslash) {
        start = backslash + 1;
    }
    return (*start) ? start : NULL;
}

bool ShapeExport_BuildPath(const char* requestedName,
                           char* outPath,
                           size_t outSize) {
    if (!requestedName || !outPath || outSize == 0) {
        return false;
    }

    const char* name = ExtractFileName(requestedName);
    if (!name || !*name) {
        name = requestedName;
    }
    if (!name || !*name) {
        return false;
    }

    if (!EnsureDirectoryExists(ShapeExport_GetExportDir())) {
        return false;
    }

    int written = snprintf(outPath, outSize, "%s/%s",
                           ShapeExport_GetExportDir(), name);
    if (written <= 0 || (size_t)written >= outSize) {
        return false;
    }
    return true;
}
