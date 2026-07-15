#include "Layout/scene/layout_mesh_asset_path_resolver.h"

#include "Core/global_state.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define LD_MESH_PATH_LOG_CAPACITY 8u

static char g_loggedRecoveryPaths[LD_MESH_PATH_LOG_CAPACITY][LINE_DRAWING_PATH_CAP];
static size_t g_nextLoggedRecoveryPath;

static void LayoutMeshPath_LogRecovery(const char* storedPath, const char* resolvedPath) {
    if (!storedPath || !resolvedPath) return;
    for (size_t i = 0u; i < LD_MESH_PATH_LOG_CAPACITY; ++i) {
        if (strcmp(g_loggedRecoveryPaths[i], storedPath) == 0) return;
    }
    SDL_Log("Recovered relocated mesh asset: %s -> %s", storedPath, resolvedPath);
    snprintf(g_loggedRecoveryPaths[g_nextLoggedRecoveryPath],
             sizeof(g_loggedRecoveryPaths[g_nextLoggedRecoveryPath]),
             "%s",
             storedPath);
    g_nextLoggedRecoveryPath = (g_nextLoggedRecoveryPath + 1u) % LD_MESH_PATH_LOG_CAPACITY;
}

static bool LayoutMeshPath_IsFile(const char* path) {
    struct stat info;
    return path && path[0] && stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

static const char* LayoutMeshPath_Basename(const char* path) {
    const char* slash = path ? strrchr(path, '/') : NULL;
    return slash ? slash + 1 : path;
}

static bool LayoutMeshPath_TryCandidate(const char* candidate,
                                        char* resolvedPath,
                                        size_t resolvedPathSize) {
    if (!LayoutMeshPath_IsFile(candidate) || !resolvedPath || resolvedPathSize == 0u) return false;
    snprintf(resolvedPath, resolvedPathSize, "%s", candidate);
    return true;
}

static bool LayoutMeshPath_TryRoot(const char* root,
                                   const char* basename,
                                   char* resolvedPath,
                                   size_t resolvedPathSize) {
    static const char* const suffixes[] = {"", "large_proofs", "curated", "assets/mesh_assets"};
    char candidate[LINE_DRAWING_PATH_CAP];
    if (!root || !root[0] || !basename || !basename[0]) return false;
    for (size_t i = 0u; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        if (suffixes[i][0]) {
            snprintf(candidate, sizeof(candidate), "%s/%s/%s", root, suffixes[i], basename);
        } else {
            snprintf(candidate, sizeof(candidate), "%s/%s", root, basename);
        }
        if (LayoutMeshPath_TryCandidate(candidate, resolvedPath, resolvedPathSize)) return true;
    }
    return false;
}

LayoutMeshPathResolution Layout_MeshAssetResolveRuntimePath(const char* storedPath,
                                                            char* resolvedPath,
                                                            size_t resolvedPathSize) {
    const char* desktopMarker = NULL;
    const char* basename = NULL;
    char candidate[LINE_DRAWING_PATH_CAP];
    if (resolvedPath && resolvedPathSize > 0u) resolvedPath[0] = '\0';
    if (!storedPath || !storedPath[0] || !resolvedPath || resolvedPathSize == 0u) {
        return LAYOUT_MESH_PATH_MISSING;
    }
    if (LayoutMeshPath_TryCandidate(storedPath, resolvedPath, resolvedPathSize)) {
        return LAYOUT_MESH_PATH_EXACT;
    }

    // Older scene files predate the Desktop/stls library root. Preserve those files and
    // resolve the one known relocation deterministically at read time.
    desktopMarker = strstr(storedPath, "/Desktop/");
    if (desktopMarker && strncmp(desktopMarker + 9, "stls/", 5) != 0) {
        const size_t prefixLength = (size_t)(desktopMarker - storedPath) + 9u;
        if (prefixLength + 5u + strlen(storedPath + prefixLength) + 1u <= sizeof(candidate)) {
            snprintf(candidate,
                     sizeof(candidate),
                     "%.*sstls/%s",
                     (int)prefixLength,
                     storedPath,
                     storedPath + prefixLength);
            if (LayoutMeshPath_TryCandidate(candidate, resolvedPath, resolvedPathSize)) {
                LayoutMeshPath_LogRecovery(storedPath, resolvedPath);
                return LAYOUT_MESH_PATH_RELOCATED;
            }
        }
    }

    basename = LayoutMeshPath_Basename(storedPath);
    if (LayoutMeshPath_TryRoot(Global_GetObjectAssetRoot(), basename, resolvedPath, resolvedPathSize) ||
        LayoutMeshPath_TryRoot(Global_GetInputRoot(), basename, resolvedPath, resolvedPathSize)) {
        LayoutMeshPath_LogRecovery(storedPath, resolvedPath);
        return LAYOUT_MESH_PATH_RELOCATED;
    }
    return LAYOUT_MESH_PATH_MISSING;
}
