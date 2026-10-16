#pragma once
#include "Layout/layout_constraints.h"
bool Layout_TelescopingStaysValidate(const Layout* layout,bool check_pose);
bool Layout_TelescopingStayApply(Layout* candidate,const LayoutConstraint* rule);
bool Layout_TelescopingStayRead(const cJSON* json,LayoutTelescopingStay* stay);
bool Layout_TelescopingStayWrite(const LayoutTelescopingStay* stay,cJSON* rule_json);
bool Layout_TelescopingStayAnchors(const Layout* layout,const LayoutConstraint* rule,Vec3* fixed,Vec3* moving);
