#pragma once
#include "Layout/layout_engineering.h"
/* Shared presentation classification for tree controls and saved presets. */
int Layout_GuideCategory(const LayoutObjectStore* store,const Object3D* object);
const char* Layout_GuideLabel(int category);
bool Layout_IsGuideView(const LayoutSavedView* view);
bool Layout_GuideViewMatches(const LayoutObjectStore* store,const Object3D* object,const LayoutSavedView* view);
bool Layout_GuideHiddenByPreset(const LayoutObjectStore* store,const Object3D* object);
void Layout_ShowGuidePresets(LayoutObjectStore* store);
