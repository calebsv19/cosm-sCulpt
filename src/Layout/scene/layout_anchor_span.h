#pragma once
#include "Layout/layout_constraints.h"
/* Exact anchor-distance extrema over a hinge's entire declared interval. */
bool Layout_AnchorSpanRange(const Layout* layout,const LayoutConstraint* rule,Vec3 fixed,Vec3 moving,double* minimum,double* maximum);
