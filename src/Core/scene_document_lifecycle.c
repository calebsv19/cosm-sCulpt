#include "Core/scene_document_lifecycle.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_diagnostics(char* out, size_t out_size, const char* message) {
    if (!out || out_size == 0u) return;
    snprintf(out, out_size, "%s", message ? message : "unknown error");
}

LineDrawingSceneDocumentSourceKind LineDrawingSceneDocument_Classify(
    const char* scene_authoring_path,
    const char* layout_path,
    const char* object_asset_path) {
    if (object_asset_path && object_asset_path[0]) return LINE_DRAWING_SCENE_DOCUMENT_OBJECT_ASSET;
    if (scene_authoring_path && scene_authoring_path[0]) return LINE_DRAWING_SCENE_DOCUMENT_CANONICAL_SCENE;
    if (layout_path && layout_path[0]) return LINE_DRAWING_SCENE_DOCUMENT_LAYOUT;
    return LINE_DRAWING_SCENE_DOCUMENT_UNTITLED;
}

const char* LineDrawingSceneDocument_SourceKindLabel(LineDrawingSceneDocumentSourceKind kind) {
    switch (kind) {
        case LINE_DRAWING_SCENE_DOCUMENT_LAYOUT: return "Layout";
        case LINE_DRAWING_SCENE_DOCUMENT_CANONICAL_SCENE: return "Scene Authoring";
        case LINE_DRAWING_SCENE_DOCUMENT_OBJECT_ASSET: return "Object Asset";
        case LINE_DRAWING_SCENE_DOCUMENT_UNTITLED:
        default: return "Untitled";
    }
}

bool LineDrawingSceneDocument_BuildRuntimePath(
    const char* scene_authoring_path,
    char* out_runtime_path,
    size_t out_runtime_path_size) {
    const char* slash = NULL;
    size_t directory_length = 0u;
    if (!scene_authoring_path || !scene_authoring_path[0] || !out_runtime_path ||
        out_runtime_path_size == 0u) {
        return false;
    }
    slash = strrchr(scene_authoring_path, '/');
    if (!slash || strcmp(slash + 1, "scene_authoring.json") != 0) return false;
    directory_length = (size_t)(slash - scene_authoring_path);
    return snprintf(out_runtime_path,
                    out_runtime_path_size,
                    "%.*s/scene_runtime.json",
                    (int)directory_length,
                    scene_authoring_path) < (int)out_runtime_path_size;
}

static int compare_file_modification_time(const struct stat* lhs, const struct stat* rhs) {
#if defined(__APPLE__)
    if (lhs->st_mtimespec.tv_sec != rhs->st_mtimespec.tv_sec) {
        return lhs->st_mtimespec.tv_sec < rhs->st_mtimespec.tv_sec ? -1 : 1;
    }
    if (lhs->st_mtimespec.tv_nsec != rhs->st_mtimespec.tv_nsec) {
        return lhs->st_mtimespec.tv_nsec < rhs->st_mtimespec.tv_nsec ? -1 : 1;
    }
#else
    if (lhs->st_mtim.tv_sec != rhs->st_mtim.tv_sec) {
        return lhs->st_mtim.tv_sec < rhs->st_mtim.tv_sec ? -1 : 1;
    }
    if (lhs->st_mtim.tv_nsec != rhs->st_mtim.tv_nsec) {
        return lhs->st_mtim.tv_nsec < rhs->st_mtim.tv_nsec ? -1 : 1;
    }
#endif
    return 0;
}

LineDrawingSceneRuntimeStatus LineDrawingSceneDocument_RuntimeStatus(
    const char* scene_authoring_path) {
    char runtime_path[1024];
    struct stat authoring_stat;
    struct stat runtime_stat;
    if (!scene_authoring_path || !scene_authoring_path[0]) {
        return LINE_DRAWING_SCENE_RUNTIME_NOT_APPLICABLE;
    }
    if (!LineDrawingSceneDocument_BuildRuntimePath(
            scene_authoring_path, runtime_path, sizeof(runtime_path))) {
        return LINE_DRAWING_SCENE_RUNTIME_UNKNOWN;
    }
    if (stat(scene_authoring_path, &authoring_stat) != 0 || !S_ISREG(authoring_stat.st_mode)) {
        return LINE_DRAWING_SCENE_RUNTIME_UNKNOWN;
    }
    if (stat(runtime_path, &runtime_stat) != 0) {
        return errno == ENOENT ? LINE_DRAWING_SCENE_RUNTIME_MISSING
                              : LINE_DRAWING_SCENE_RUNTIME_UNKNOWN;
    }
    if (!S_ISREG(runtime_stat.st_mode)) return LINE_DRAWING_SCENE_RUNTIME_UNKNOWN;
    return compare_file_modification_time(&runtime_stat, &authoring_stat) >= 0
               ? LINE_DRAWING_SCENE_RUNTIME_CURRENT
               : LINE_DRAWING_SCENE_RUNTIME_STALE;
}

const char* LineDrawingSceneDocument_RuntimeStatusLabel(LineDrawingSceneRuntimeStatus status) {
    switch (status) {
        case LINE_DRAWING_SCENE_RUNTIME_MISSING: return "missing (export required)";
        case LINE_DRAWING_SCENE_RUNTIME_CURRENT: return "current";
        case LINE_DRAWING_SCENE_RUNTIME_STALE: return "stale (export required)";
        case LINE_DRAWING_SCENE_RUNTIME_UNKNOWN: return "unknown";
        case LINE_DRAWING_SCENE_RUNTIME_NOT_APPLICABLE:
        default: return "not built";
    }
}

static bool requested_name_is_safe(const char* name) {
    if (!name || !name[0] || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return false;
    for (const char* p = name; *p; ++p) {
        if (*p == '/' || *p == '\\' || (unsigned char)*p < 32u) return false;
    }
    return true;
}

bool LineDrawingSceneDocument_BuildSiblingSaveAsPaths(
    const char* source_authoring_path,
    const char* requested_name,
    char* out_scene_dir,
    size_t out_scene_dir_size,
    char* out_authoring_path,
    size_t out_authoring_path_size,
    char* diagnostics,
    size_t diagnostics_size) {
    const char* source_file = NULL;
    const char* source_dir_end = NULL;
    const char* parent_end = NULL;
    size_t parent_len = 0u;
    size_t name_len = 0u;
    char name[128];

    if (diagnostics && diagnostics_size) diagnostics[0] = '\0';
    if (!source_authoring_path || !out_scene_dir || !out_authoring_path ||
        out_scene_dir_size == 0u || out_authoring_path_size == 0u) {
        write_diagnostics(diagnostics, diagnostics_size, "invalid Save As arguments");
        return false;
    }
    source_file = strrchr(source_authoring_path, '/');
    if (!source_file || source_file == source_authoring_path ||
        strcmp(source_file + 1, "scene_authoring.json") != 0) {
        write_diagnostics(diagnostics, diagnostics_size, "source is not a canonical scene bundle");
        return false;
    }
    source_dir_end = source_file;
    parent_end = source_dir_end;
    while (parent_end > source_authoring_path && parent_end[-1] != '/') --parent_end;
    if (parent_end <= source_authoring_path) {
        write_diagnostics(diagnostics, diagnostics_size, "scene bundle has no sibling parent");
        return false;
    }
    parent_len = (size_t)(parent_end - source_authoring_path - 1);
    name_len = requested_name ? strlen(requested_name) : 0u;
    if (name_len > 5u && strcasecmp(requested_name + name_len - 5u, ".json") == 0) name_len -= 5u;
    if (name_len >= sizeof(name)) name_len = sizeof(name) - 1u;
    if (name_len) memcpy(name, requested_name, name_len);
    name[name_len] = '\0';
    if (!requested_name_is_safe(name) || strcmp(name, "scene_authoring") == 0) {
        write_diagnostics(diagnostics, diagnostics_size, "scene name must be a safe sibling folder name");
        return false;
    }
    if (snprintf(out_scene_dir, out_scene_dir_size, "%.*s/%s", (int)parent_len,
                 source_authoring_path, name) >= (int)out_scene_dir_size ||
        snprintf(out_authoring_path, out_authoring_path_size, "%s/scene_authoring.json",
                 out_scene_dir) >= (int)out_authoring_path_size) {
        write_diagnostics(diagnostics, diagnostics_size, "Save As path is too long");
        return false;
    }
    return true;
}

static bool copy_regular_file(const char* source, const char* destination) {
    FILE* in = fopen(source, "rb");
    FILE* out = NULL;
    char buffer[16384];
    size_t count = 0u;
    bool ok = false;
    if (!in) return false;
    out = fopen(destination, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    ok = true;
    while ((count = fread(buffer, 1u, sizeof(buffer), in)) > 0u) {
        if (fwrite(buffer, 1u, count, out) != count) {
            ok = false;
            break;
        }
    }
    if (ferror(in)) ok = false;
    if (fclose(out) != 0) ok = false;
    fclose(in);
    if (!ok) (void)unlink(destination);
    return ok;
}

/* Save As carries editable attachment roots only. Compiled outputs, receipts,
 * dependency payloads, caches, renders, and project scaffolds are intentionally
 * regenerated for the new authoring identity. */
static bool top_level_entry_is_authoring_owned(const char* name, const struct stat* st) {
    if (!name || !st) return false;
    if (S_ISDIR(st->st_mode)) {
        return strcmp(name, "assets") == 0 || strcmp(name, "attachments") == 0;
    }
    return false;
}

static bool copy_directory(const char* source, const char* destination, unsigned depth) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;
    if (depth > 24u || mkdir(destination, 0755) != 0) return false;
    dir = opendir(source);
    if (!dir) return false;
    while ((entry = readdir(dir)) != NULL) {
        char source_path[1024];
        char destination_path[1024];
        struct stat st;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (snprintf(source_path, sizeof(source_path), "%s/%s", source, entry->d_name) >= (int)sizeof(source_path) ||
            snprintf(destination_path, sizeof(destination_path), "%s/%s", destination, entry->d_name) >= (int)sizeof(destination_path) ||
            lstat(source_path, &st) != 0) {
            closedir(dir);
            return false;
        }
        if (depth == 0u && !top_level_entry_is_authoring_owned(entry->d_name, &st)) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            if (!copy_directory(source_path, destination_path, depth + 1u)) {
                closedir(dir);
                return false;
            }
        } else if (S_ISREG(st.st_mode)) {
            if (!copy_regular_file(source_path, destination_path)) {
                closedir(dir);
                return false;
            }
        } else {
            closedir(dir);
            return false;
        }
    }
    return closedir(dir) == 0;
}

static void remove_directory_recursive(const char* path, unsigned depth) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;
    if (!path || !path[0] || depth > 24u) return;
    dir = opendir(path);
    if (!dir) return;
    while ((entry = readdir(dir)) != NULL) {
        char child[1024];
        struct stat st;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >= (int)sizeof(child) ||
            lstat(child, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) remove_directory_recursive(child, depth + 1u);
        else (void)unlink(child);
    }
    closedir(dir);
    (void)rmdir(path);
}

bool LineDrawingSceneDocument_CloneBundleForSaveAs(
    const char* source_authoring_path,
    const char* destination_scene_dir,
    char* diagnostics,
    size_t diagnostics_size) {
    char source_scene_dir[1024];
    const char* slash = NULL;
    struct stat st;
    if (diagnostics && diagnostics_size) diagnostics[0] = '\0';
    if (!source_authoring_path || !destination_scene_dir ||
        !(slash = strrchr(source_authoring_path, '/'))) {
        write_diagnostics(diagnostics, diagnostics_size, "invalid scene bundle paths");
        return false;
    }
    errno = 0;
    if (stat(destination_scene_dir, &st) == 0) {
        write_diagnostics(diagnostics, diagnostics_size, "destination scene bundle already exists");
        return false;
    }
    if (errno != ENOENT || (size_t)(slash - source_authoring_path) >= sizeof(source_scene_dir)) {
        write_diagnostics(diagnostics, diagnostics_size, "cannot inspect destination scene bundle");
        return false;
    }
    memcpy(source_scene_dir, source_authoring_path, (size_t)(slash - source_authoring_path));
    source_scene_dir[slash - source_authoring_path] = '\0';
    if (!copy_directory(source_scene_dir, destination_scene_dir, 0u)) {
        remove_directory_recursive(destination_scene_dir, 0u);
        write_diagnostics(diagnostics, diagnostics_size, "could not clone scene bundle attachments");
        return false;
    }
    return true;
}

void LineDrawingSceneDocument_RemoveClonedBundle(const char* scene_dir) {
    remove_directory_recursive(scene_dir, 0u);
}
