#pragma once

#include "Layout/layout.h"
#include "core_scene_compile.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Discover Sculpt-owned file-backed scene dependencies and render the shared
 * canonical manifest. The returned JSON uses core_alloc and must be released
 * with core_free by the caller.
 */
bool LineDrawingSceneExportDependencies_Collect(
    const Layout* layout,
    char** out_manifest_json,
    char out_digest_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE],
    size_t* out_dependency_count,
    char* diagnostics,
    size_t diagnostics_size);
