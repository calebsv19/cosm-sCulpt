#include "Layout/layout_inspection.h"
#include <stdio.h>
#include <string.h>

static const char *names[] = {"inherit", "solid", "+X", "-X", "+Y", "-Y", "+Z", "-Z"};
const char *Layout_InspectionFacingName(LayoutInspectionFacing facing) {
    return facing >= LAYOUT_INSPECTION_INHERIT && facing < LAYOUT_INSPECTION_INVALID ? names[facing]
                                                                                     : "invalid";
}
LayoutInspectionFacing Layout_InspectionFacing(const LayoutEntityInfo *info) {
    if (!info)
        return LAYOUT_INSPECTION_INHERIT;
    if (info->property_count > LAYOUT_MAX_PROPERTIES)
        return LAYOUT_INSPECTION_INVALID;
    for (size_t i = 0; i < info->property_count && i < LAYOUT_MAX_PROPERTIES; ++i) {
        const LayoutProperty *p = &info->properties[i];
        if (!memchr(p->key, 0, sizeof(p->key)))
            return LAYOUT_INSPECTION_INVALID;
        if (strcmp(p->key, "inspection_facing"))
            continue;
        if (!memchr(p->text, 0, sizeof(p->text)))
            return LAYOUT_INSPECTION_INVALID;
        if (p->kind == LAYOUT_PROPERTY_TEXT)
            for (int j = 0; j < LAYOUT_INSPECTION_INVALID; ++j)
                if (!strcmp(p->text, names[j]))
                    return (LayoutInspectionFacing)j;
        return LAYOUT_INSPECTION_INVALID;
    }
    return LAYOUT_INSPECTION_INHERIT;
}
bool Layout_SetInspectionFacing(Layout *layout, const char *id, LayoutInspectionFacing facing,
                                LayoutGeometryBeforePublish history, void *context) {
    const LayoutEntityInfo *old = layout ? Layout_EntityInfo(&layout->objectStore, id) : NULL;
    if (!old || facing < LAYOUT_INSPECTION_INHERIT || facing >= LAYOUT_INSPECTION_INVALID)
        return false;
    if (Layout_InspectionFacing(old) == facing)
        return true;
    LayoutEntityInfo next = *old;
    size_t i = 0;
    while (i < next.property_count && strcmp(next.properties[i].key, "inspection_facing"))
        ++i;
    if (facing == LAYOUT_INSPECTION_INHERIT) {
        if (i < next.property_count) {
            memmove(&next.properties[i], &next.properties[i + 1],
                    (next.property_count - i - 1) * sizeof(LayoutProperty));
            memset(&next.properties[--next.property_count], 0, sizeof(LayoutProperty));
        }
    } else {
        if (i == LAYOUT_MAX_PROPERTIES) {
            snprintf(layout->geometryMessage, sizeof(layout->geometryMessage),
                     "No free property slot. Set facing on the parent assembly instead.");
            return false;
        }
        if (i == next.property_count)
            ++next.property_count;
        next.properties[i] = (LayoutProperty){.kind = LAYOUT_PROPERTY_TEXT};
        snprintf(next.properties[i].key, sizeof(next.properties[i].key), "inspection_facing");
        snprintf(next.properties[i].text, sizeof(next.properties[i].text), "%s", names[facing]);
    }
    return Layout_SetEntityInfo(layout, id, &next, history, context);
}
static PlaneFrame3 object_frame(const Object3D *object) {
    if (object->kind != OBJECT3D_KIND_MESH_ASSET_INSTANCE)
        return object->kind == OBJECT3D_KIND_PLANE ? object->plane.frame : object->rectPrism.frame;
    Transform3D t = object->transform;
    t.position = (Vec3){0};
    t.scale = (Vec3){1, 1, 1};
    return (PlaneFrame3){.axisU = Layout_Transform3D_ApplyLocalPoint(t, (Vec3){1, 0, 0}),
                         .axisV = Layout_Transform3D_ApplyLocalPoint(t, (Vec3){0, 1, 0}),
                         .normal = Layout_Transform3D_ApplyLocalPoint(t, (Vec3){0, 0, 1})};
}
bool Layout_InspectionOutlined(const LayoutObjectStore *store, const Object3D *object,
                               const SpaceViewContext *view) {
    if (!store || !object || !view || !view->inspection ||
        Layout_EntityIsSpatialGuide(&object->info))
        return false;
    const LayoutEntityInfo *info = &object->info;
    PlaneFrame3 frame = object_frame(object);
    LayoutInspectionFacing facing = Layout_InspectionFacing(info);
    for (int depth = 0; facing == LAYOUT_INSPECTION_INHERIT && depth < LAYOUT_MAX_ASSEMBLIES;
         ++depth) {
        const LayoutAssembly *parent = Layout_FindAssembly(store, info->parent_id);
        if (!parent)
            break;
        info = &parent->info;
        frame = parent->frame;
        facing = Layout_InspectionFacing(info);
    }
    if (facing <= LAYOUT_INSPECTION_SOLID || facing >= LAYOUT_INSPECTION_INVALID)
        return false;
    int axis = (facing - LAYOUT_INSPECTION_POS_X) / 2;
    Vec3 inward = axis == 0 ? frame.axisU : axis == 1 ? frame.axisV : frame.normal;
    if ((facing - LAYOUT_INSPECTION_POS_X) % 2)
        inward = Vec3_Scale(inward, -1);
    Vec3 forward = view->perspective.enabled              ? view->perspective.forward
                   : SpaceAdapter_IsFreeViewEnabled(view) ? FreeView_Forward(&view->camera)
                   : view->plane.axis == VIEW_PLANE_YZ    ? (Vec3){1, 0, 0}
                   : view->plane.axis == VIEW_PLANE_XZ    ? (Vec3){0, 1, 0}
                                                          : (Vec3){0, 0, 1};
    /* Keep units solid through 15 degrees beyond tangent (210-degree span). */
    const float outlineThreshold = 0.2588190451f; /* sin(15 degrees). */
    return Vec3_Dot(inward, forward) > outlineThreshold;
}
