#pragma once

#include "UI/ui_panel.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct LineDrawingFileCatalogEntry {
    char label[128];
    char path[MAX_CONFIG_PATH];
} LineDrawingFileCatalogEntry;

const char* LineDrawingFileCatalog_SceneAuthoringFilename(void);
const char* LineDrawingFileCatalog_SceneRuntimeFilename(void);
bool LineDrawingFileCatalog_PathIsDirectory(const char* path);
bool LineDrawingFileCatalog_PathIsRegularFile(const char* path);
const char* LineDrawingFileCatalog_PathBasename(const char* path);
void LineDrawingFileCatalog_NormalizePath(char* path);
bool LineDrawingFileCatalog_ParentPath(const char* path,
                                       char* out_path,
                                       size_t out_path_size);
bool LineDrawingFileCatalog_JoinPath(const char* base,
                                     const char* leaf,
                                     char* out_path,
                                     size_t out_path_size);
bool LineDrawingFileCatalog_NameHasJsonSuffix(const char* name);
bool LineDrawingFileCatalog_DirectoryHasSceneContract(const char* scene_dir,
                                                      char* out_authoring_path,
                                                      size_t out_authoring_path_size);
void LineDrawingFileCatalog_BuildSceneLabel(const char* group_name,
                                            const char* scene_dir,
                                            char* out_label,
                                            size_t out_label_size);
int LineDrawingFileCatalog_ScanLayoutEntries(LineDrawingFileCatalogEntry* entries,
                                             int max_entries,
                                             const char* root_dir);
int LineDrawingFileCatalog_ScanSceneEntries(LineDrawingFileCatalogEntry* entries,
                                            int max_entries,
                                            const char* root_dir);
int LineDrawingFileCatalog_ScanRuntimeMeshEntries(LineDrawingFileCatalogEntry* entries,
                                                  int max_entries,
                                                  const char* root_dir);
int LineDrawingFileCatalog_ScanStlEntries(LineDrawingFileCatalogEntry* entries,
                                          int max_entries,
                                          const char* root_dir);
void LineDrawingFileCatalog_SortEntries(LineDrawingFileCatalogEntry* entries, int count);
