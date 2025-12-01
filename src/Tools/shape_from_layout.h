#pragma once

#include <stdbool.h>
#include "Layout/layout.h"
#include "ShapeLib/shape_core.h"

bool ShapeDocument_FromLayout(const char* name,
                              const Layout* layout,
                              ShapeDocument* outDoc);
