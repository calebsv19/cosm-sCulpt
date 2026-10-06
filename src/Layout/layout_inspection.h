#pragma once
#include "Core/space_mode_adapter.h"
#include "Layout/layout_engineering.h"

/* Optional inspection_facing text property: inherit (absence), solid, or
 * +/-X/Y/Z in the declaring entity's rigid local frame. No geometry changes. */
typedef enum {
    LAYOUT_INSPECTION_INHERIT,
    LAYOUT_INSPECTION_SOLID,
    LAYOUT_INSPECTION_POS_X,
    LAYOUT_INSPECTION_NEG_X,
    LAYOUT_INSPECTION_POS_Y,
    LAYOUT_INSPECTION_NEG_Y,
    LAYOUT_INSPECTION_POS_Z,
    LAYOUT_INSPECTION_NEG_Z,
    LAYOUT_INSPECTION_INVALID
} LayoutInspectionFacing;
LayoutInspectionFacing Layout_InspectionFacing(const LayoutEntityInfo *info);
const char *Layout_InspectionFacingName(LayoutInspectionFacing facing);
bool Layout_SetInspectionFacing(Layout *layout, const char *id, LayoutInspectionFacing facing,
                                LayoutGeometryBeforePublish history, void *context);
/* Inherited from nearest declaring ancestor; solid explicitly stops inheritance.
 * Classification uses viewing heading, so a whole unit stays coherent. Tangent
 * views and unassigned objects remain solid. Applies only to inspection views. */
bool Layout_InspectionOutlined(const LayoutObjectStore *store, const Object3D *object,
                               const SpaceViewContext *view);
