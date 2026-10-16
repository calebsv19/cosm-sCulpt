#pragma once
#include "Layout/layout_collection_inspection.h"
/* Current-pose reference only. No inferred cut sizes, stock, joints or tolerances. */
cJSON* Layout_ConstructionReference(const Layout* layout,const char* collection);
/* Fresh directory only; complete marker is written last. Failure removes only files
 * created by this call, never existing user output. Snapshot includes unsaved edits. */
bool Layout_ExportConstructionReference(const Layout* layout,const char* collection,
    const char* source_label,const char* new_directory,char* error,size_t capacity);
bool Layout_ReferenceSheetsWrite(const cJSON* specification,const char* directory);
