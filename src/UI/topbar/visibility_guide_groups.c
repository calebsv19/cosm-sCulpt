#include "UI/topbar/visibility_guide_groups.h"
#include "Layout/layout_guide_groups.h"
int LDVisibilityGuide_Category(const LayoutObjectStore* store,const Object3D* object) {
    return Layout_GuideCategory(store,object);
}
const char* LDVisibilityGuide_Label(int category) { return Layout_GuideLabel(category); }
