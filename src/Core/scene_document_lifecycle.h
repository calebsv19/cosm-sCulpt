#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum LineDrawingSceneDocumentSourceKind {
    LINE_DRAWING_SCENE_DOCUMENT_UNTITLED = 0,
    LINE_DRAWING_SCENE_DOCUMENT_LAYOUT,
    LINE_DRAWING_SCENE_DOCUMENT_CANONICAL_SCENE,
    LINE_DRAWING_SCENE_DOCUMENT_OBJECT_ASSET
} LineDrawingSceneDocumentSourceKind;

typedef enum LineDrawingSceneRuntimeStatus {
    LINE_DRAWING_SCENE_RUNTIME_NOT_APPLICABLE = 0,
    LINE_DRAWING_SCENE_RUNTIME_MISSING,
    LINE_DRAWING_SCENE_RUNTIME_CURRENT,
    LINE_DRAWING_SCENE_RUNTIME_STALE,
    LINE_DRAWING_SCENE_RUNTIME_UNKNOWN
} LineDrawingSceneRuntimeStatus;

LineDrawingSceneDocumentSourceKind LineDrawingSceneDocument_Classify(
    const char* scene_authoring_path,
    const char* layout_path,
    const char* object_asset_path);

const char* LineDrawingSceneDocument_SourceKindLabel(LineDrawingSceneDocumentSourceKind kind);

bool LineDrawingSceneDocument_BuildRuntimePath(
    const char* scene_authoring_path,
    char* out_runtime_path,
    size_t out_runtime_path_size);

LineDrawingSceneRuntimeStatus LineDrawingSceneDocument_RuntimeStatus(
    const char* scene_authoring_path);

const char* LineDrawingSceneDocument_RuntimeStatusLabel(LineDrawingSceneRuntimeStatus status);

bool LineDrawingSceneDocument_BuildSiblingSaveAsPaths(
    const char* source_authoring_path,
    const char* requested_name,
    char* out_scene_dir,
    size_t out_scene_dir_size,
    char* out_authoring_path,
    size_t out_authoring_path_size,
    char* diagnostics,
    size_t diagnostics_size);

/* Clones only authoring-owned assets/ and attachments/. Runtime, dependency,
 * receipt, cache, render, project, and package metadata are regenerated. */
bool LineDrawingSceneDocument_CloneBundleForSaveAs(
    const char* source_authoring_path,
    const char* destination_scene_dir,
    char* diagnostics,
    size_t diagnostics_size);

/* Removes only a fresh destination created by CloneBundleForSaveAs. */
void LineDrawingSceneDocument_RemoveClonedBundle(const char* scene_dir);
