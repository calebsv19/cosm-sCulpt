#pragma once
#include "Layout/layout_engineering.h"
typedef struct {const char* id;const LayoutEntityInfo* info;bool assembly;unsigned depth;} LDVisibilityRow;
void LDVisibilityTree_Reset(void);
size_t LDVisibilityTree_Build(const LayoutObjectStore* store,const char* search);
bool LDVisibilityTree_Row(size_t index,LDVisibilityRow* row);
void LDVisibilityTree_Expand(const char* id);
bool LDVisibilityTree_Expanded(const char* id);
int LDVisibilityTree_State(const LayoutObjectStore* store,const char* id);
bool LDVisibilityTree_Hide(LayoutObjectStore* store,const char* id,bool hidden);
bool LDVisibilityTree_Only(LayoutObjectStore* store,const char* id);

void LDVisibilityTree_Restore(LayoutObjectStore* store);
/* User collections and cross-cutting layers have separate inventories. */
size_t LDVisibilityTree_BuildScene(const LayoutObjectStore* store,const char* search);
size_t LDVisibilityTree_BuildLayers(const LayoutObjectStore* store,const char* search);
