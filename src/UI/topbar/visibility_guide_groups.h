#pragma once
#include "Layout/layout_engineering.h"
/* Metadata-based functional categories; zero is a physical/non-guide object. */
int LDVisibilityGuide_Category(const LayoutObjectStore* store, const Object3D* object);
const char* LDVisibilityGuide_Label(int category);
