#pragma once

#include "ObjectAuthoring/object_authoring_document.h"

#include <stdbool.h>
#include <stddef.h>

bool ObjectAuthoringDocument_SaveExtensionToFile(const ObjectAuthoringDocument* doc,
                                                 const char* path,
                                                 char* diagnostics,
                                                 size_t diagnostics_size);
bool ObjectAuthoringDocument_LoadExtensionFromFile(ObjectAuthoringDocument* doc,
                                                   const char* path,
                                                   bool* out_found,
                                                   char* diagnostics,
                                                   size_t diagnostics_size);
