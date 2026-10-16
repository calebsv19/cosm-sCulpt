#pragma once
#include "Layout/layout_engineering.h"
#define LAYOUT_MAX_COVERING_COLLECTIONS 256
typedef struct LayoutCoveringCollection {char id[64],parent[64];LayoutEntityInfo info;} LayoutCoveringCollection;
bool Layout_CoveringInfoContains(const LayoutEntityInfo* info,const char* collection);
bool Layout_CoveringInfoValid(const LayoutEntityInfo* info);
bool Layout_CoveringCollectionValid(const LayoutObjectStore* store,const char* id);
bool Layout_CoveringCollectionContains(const LayoutObjectStore* store,const char* collection,const char* entity);
size_t Layout_CoveringCollections(const LayoutObjectStore* store,LayoutCoveringCollection* collections,size_t capacity);
bool Layout_CoveringSetHidden(LayoutObjectStore* store,const char* id,bool hidden);
bool Layout_CoveringHiddenByPreset(const LayoutObjectStore* store,const char* entity);
void Layout_CoveringReset(LayoutObjectStore* store);
