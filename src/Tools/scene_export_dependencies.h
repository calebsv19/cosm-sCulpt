#pragma once

#include "Layout/layout.h"
#include "core_io.h"
#include "core_scene_compile.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct LineDrawingSceneExportDependencies {
    char* manifest_json;
    char digest_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    CoreSceneCompileDependency* entries;
    CoreBuffer* payload_buffers;
    char (*content_digests)[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    size_t count;
} LineDrawingSceneExportDependencies;

/* Discover and retain Sculpt-owned dependency payloads through publication. */
bool LineDrawingSceneExportDependencies_Collect(
    const Layout* layout,
    const char* scene_authoring_path,
    LineDrawingSceneExportDependencies* out_dependencies,
    char* diagnostics,
    size_t diagnostics_size);

/* Release every manifest and retained payload allocation owned by a collection. */
void LineDrawingSceneExportDependencies_Destroy(
    LineDrawingSceneExportDependencies* dependencies);

/* Bind generated mesh references to the same contained payloads published by
 * core_scene_compile. The consumer does not resolve identities from the manifest. */
bool LineDrawingSceneExportDependencies_BindPaths(char **authoring_json,
    const LineDrawingSceneExportDependencies *dependencies);
