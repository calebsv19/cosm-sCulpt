#pragma once
#include "Layout/layout_constraints.h"
bool Layout_FoldingElbow(Vec3 a,Vec3 b,Vec3 normal,double first_m,double second_m,int branch,Vec3* elbow);
bool Layout_FoldingBracesValidate(const Layout* layout,bool check_pose);
bool Layout_FoldingBraceApply(Layout* candidate,const LayoutConstraint* rule);
bool Layout_FoldingBraceRead(const cJSON* json,LayoutFoldingBrace* brace);
bool Layout_FoldingBraceWrite(const LayoutFoldingBrace* brace,cJSON* rule_json);

/* A conceptual manual interlock, not force/contact or hardware qualification. */
bool Layout_FoldingLockValid(const Layout* layout,const LayoutConstraint* rule);
bool Layout_FoldingBraceSetLock(Layout* layout,const char* rule_id,bool engaged,LayoutGeometryBeforePublish hook,void* context);
