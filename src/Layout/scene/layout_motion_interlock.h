#pragma once
#include "Layout/layout.h"
bool Layout_MotionInterlocksValidate(const Layout* layout, char* message, size_t size);
bool Layout_MotionInterlocksAllow(const Layout* layout, const char* id, double position, char* message, size_t size);
