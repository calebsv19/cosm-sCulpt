#include "Tools/illustrative_van.h"
#include "Layout/layout_json.h"
#include "Layout/layout_motion.h"
#include "Layout/layout_relationships.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Object3D *object(Layout *l, const char *id) {
    for (size_t i = 0; i < l->objectStore.count; ++i)
        if (!strcmp(l->objectStore.items[i].coreMeta.object_id, id))
            return &l->objectStore.items[i];
    return NULL;
}
static void property(LayoutEntityInfo *info, const char *key, const char *value) {
    LayoutProperty *p = &info->properties[info->property_count++];
    p->kind = LAYOUT_PROPERTY_TEXT;
    snprintf(p->key, sizeof(p->key), "%s", key);
    snprintf(p->text, sizeof(p->text), "%s", value);
}
static bool assembly(Layout *l, const char *name, const char *parent, const char *subsystem, char id[64]) {
    LayoutAssembly a = {.frame = {.axisU = {1, 0, 0}, .axisV = {0, 1, 0}, .normal = {0, 0, 1}}};
    snprintf(a.info.label, 96, "%s", name);
    snprintf(a.info.entity_type, 64, "Assembly");
    snprintf(a.info.parent_id, 64, "%s", parent);
    property(&a.info, "subsystem", subsystem);
    property(&a.info, "dimensions_status", "illustrative");
    if (!Layout_EditAssembly(l, &a, NULL, NULL, NULL))
        return false;
    snprintf(id, 64, "%s", l->objectStore.assemblies[l->objectStore.assembly_count - 1].id);
    return true;
}
static bool box(Layout *l, const char *id, const char *name, const char *type, const char *parent,
                const char *subsystem, Vec3 center, Vec3 size, bool reference, bool door_frame) {
    RectPrismPrimitiveCreateParams p = {
        .width = size.x,
        .height = size.y,
        .depth = size.z,
        .useExplicitFrame = true,
        .explicitFrame = {.origin = center, .axisU = {1, 0, 0}, .axisV = {0, 1, 0}, .normal = {0, 0, 1}}};
    if (door_frame)
        p.explicitFrame =
            (PlaneFrame3){.origin = center, .axisU = {0, -1, 0}, .axisV = {0, 0, 1}, .normal = {-1, 0, 0}};
    uint32_t handle;
    if (!Layout_CreateRectPrismPrimitive(l, &p, &handle, NULL))
        return false;
    Object3D *o = Layout_ObjectStore_Find(&l->objectStore, handle);
    if (core_object_set_identity(&o->coreMeta, id, o->coreMeta.object_type).code != CORE_OK)
        return false;
    LayoutEntityInfo info = {.reference = reference};
    snprintf(info.label, 96, "%s", name);
    snprintf(info.entity_type, 64, "%s", type);
    snprintf(info.parent_id, 64, "%s", parent);
    property(&info, "subsystem", subsystem);
    property(&info, "dimensions_status", "illustrative");
    return Layout_SetEntityInfo(l, id, &info, NULL, NULL);
}
static bool link(Layout *l, LayoutRelationshipKind kind, const char *a, const char *b) {
    LayoutRelationship r = {.kind = kind};
    snprintf(r.source, 64, "%s", a);
    snprintf(r.target, 64, "%s", b);
    return Layout_EditRelationship(l, &r, NULL, NULL, NULL);
}
static bool volume(Layout *l, const char *id, const char *name, LayoutVolumeRole role, const char *owner,
                   Vec3 center, Vec3 size) {
    LayoutVolumeEdit v = {.role = role,
                          .center_meters = {center.x, center.y, center.z},
                          .size_meters = {size.x, size.y, size.z}};
    snprintf(v.name, 96, "%s", name);
    snprintf(v.owner, 64, "%s", owner);
    if (!Layout_EditVolume(l, &v, false, NULL, NULL))
        return false;
    Object3D *o = &l->objectStore.items[l->objectStore.count - 1];
    if (core_object_set_identity(&o->coreMeta, id, o->coreMeta.object_type).code != CORE_OK)
        return false;
    LayoutEntityInfo info = o->info;
    property(&info, "dimensions_status", "illustrative");
    return Layout_SetEntityInfo(l, id, &info, NULL, NULL);
}
static bool check(Layout *l, LayoutSpatialRuleKind kind, const char *a, const char *b, double gap) {
    LayoutSpatialRule r = {.kind = kind, .clearance_meters = gap};
    snprintf(r.source, 64, "%s", a);
    snprintf(r.target, 64, "%s", b);
    return Layout_EditSpatialRule(l, &r, NULL, NULL, NULL);
}
static bool build(Layout *l) {
    char root[64], oem[64], bed[64], water[64], door[64], power[64], furniture[64], exercise[64];
    Layout_Init(l, .1f);
    l->scene3d.bounds = (SceneBounds3D){
        .enabled = false, .clampOnEdit = false, .min = {-1.1f, -2, -.1f}, .max = {1.1f, 2, 2.05f}};
    if (!assembly(l, "Illustrative van - placeholder dimensions", "", "van", root) ||
        !assembly(l, "OEM reference", root, "oem", oem) || !assembly(l, "Lift bed", root, "bed", bed) ||
        !assembly(l, "Water cabinet", root, "water", water) ||
        !assembly(l, "Cabinet door", water, "water", door) ||
        !assembly(l, "Electrical bay", root, "electrical", power) ||
        !assembly(l, "Furniture", root, "interior", furniture) ||
        !assembly(l, "Obstruction exercises", root, "exercise", exercise))
        return false;
#define BOX(id, name, type, parent, sub, x, y, z, w, h, d, ref)                                              \
    if (!box(l, id, name, type, parent, sub, (Vec3){x, y, z}, (Vec3){w, h, d}, ref, false))                  \
    return false
    BOX("oem_floor", "Floor reference - 1850 x 3650 mm", "Panel", oem, "oem", 0, 0, -.02f, 1.85f, 3.65f, .04f,
        true);
    BOX("oem_driver_wall", "Driver wall reference", "Surface", oem, "oem", -.925f, 0, .95f, .02f, 3.65f,
        1.90f, true);
    BOX("oem_passenger_wall", "Passenger wall reference", "Surface", oem, "oem", .925f, 0, .95f, .02f, 3.65f,
        1.90f, true);
    BOX("oem_front_boundary", "Front boundary reference", "Surface", oem, "oem", 0, 1.825f, .95f, 1.85f, .02f,
        1.90f, true);
    BOX("oem_rear_boundary", "Rear opening reference", "Surface", oem, "oem", 0, -1.825f, .95f, 1.85f, .02f,
        1.90f, true);
    BOX("wheel_well_driver", "Driver wheel well reference", "PhysicalObject", oem, "oem", -.72f, -.85f, .18f,
        .4f, .85f, .36f, true);
    BOX("wheel_well_passenger", "Passenger wheel well reference", "PhysicalObject", oem, "oem", .72f, -.85f,
        .18f, .4f, .85f, .36f, true);
    for (int i = 0; i < 3; ++i) {
        char id[64], name[96];
        snprintf(id, 64, "roof_rib_%02d", i + 1);
        snprintf(name, 96, "Roof rib %d reference", i + 1);
        BOX(id, name, "StructuralMember", oem, "oem", 0, -1.2f + i * 1.2f, 1.88f, 1.85f, .04f, .04f, true);
    }
    BOX("bed_guide", "Bed lift reference", "MountPoint", oem, "oem", 0, -.9f, 0, .04f, .04f, .04f, true);
    BOX("bed_deck", "Bed deck - 1650 x 1750 mm", "Panel", bed, "bed", 0, -.9f, .90f, 1.65f, 1.75f, .06f,
        false);
    BOX("bed_mattress", "Mattress placeholder", "PhysicalObject", bed, "bed", 0, -.9f, .99f, 1.60f, 1.70f,
        .12f, false);
    BOX("bed_rail_driver", "Bed driver rail", "Rail", bed, "bed", -.80f, -.9f, .86f, .04f, 1.75f, .08f,
        false);
    BOX("bed_rail_passenger", "Bed passenger rail", "Rail", bed, "bed", .80f, -.9f, .86f, .04f, 1.75f, .08f,
        false);
    BOX("water_bottom", "Water cabinet bottom", "Panel", water, "water", -.62f, .75f, .025f, .55f, .95f,
        .018f, false);
    BOX("water_top", "Water cabinet top", "Panel", water, "water", -.62f, .75f, .90f, .55f, .95f, .018f,
        false);
    BOX("water_back", "Water cabinet back", "Panel", water, "water", -.895f, .75f, .45f, .018f, .95f, .9f,
        false);
    BOX("water_rear_side", "Water cabinet rear side", "Panel", water, "water", -.62f, .275f, .45f, .55f,
        .018f, .9f, false);
    BOX("water_front_side", "Water cabinet front side", "Panel", water, "water", -.62f, 1.225f, .45f, .55f,
        .018f, .9f, false);
    if (!box(l, "water_door", "Water cabinet door", "Panel", door, "water", (Vec3){-.33f, .75f, .45f},
             (Vec3){.75f, .65f, .025f}, false, true) ||
        !box(l, "door_mount", "Door hinge reference", "MountPoint", oem, "oem", (Vec3){-.33f, 1.125f, .45f},
             (Vec3){.025f, .025f, .025f}, true, true))
        return false;
    BOX("door_handle", "Door handle placeholder", "PhysicalObject", door, "water", -.29f, .43f, .50f, .035f,
        .06f, .03f, false);
    BOX("water_tank", "Water tank placeholder", "Tank", water, "water", -.62f, .70f, .30f, .30f, .50f, .50f,
        false);
    BOX("water_pump", "Water pump placeholder", "Actuator", water, "water", -.62f, 1.10f, .15f, .12f, .12f,
        .16f, false);
    BOX("water_controller", "Water controller placeholder", "Controller", water, "water", -.50f, .90f, .72f,
        .08f, .03f, .06f, false);
    BOX("battery", "Battery placeholder", "Battery", power, "electrical", .68f, -.20f, .25f, .35f, .35f, .50f,
        false);
    BOX("fuse_box", "Fuse box placeholder", "Fuse", power, "electrical", -.65f, 1.52f, 1.10f, .25f, .15f,
        .35f, false);
    BOX("can_hub", "CAN hub placeholder", "Controller", power, "electrical", -.65f, 1.52f, .76f, .10f, .08f,
        .06f, false);
    BOX("passenger_storage", "Passenger storage placeholder", "PhysicalObject", furniture, "interior", .78f,
        .95f, .45f, .25f, .9f, .9f, false);
    BOX("bracket_fixed", "Mount bracket fixed arm", "StructuralMember", power, "electrical", -.62f, 1.42f,
        .70f, .14f, .025f, .025f, false);
    if (!box(l, "bracket_mated", "Mount bracket 90-degree arm", "StructuralMember", power, "electrical",
             (Vec3){-.55f, 1.49f, .70f}, (Vec3){.14f, .025f, .025f}, false, false))
        return false;
    if (!Layout_RotateObject3D(l, object(l, "bracket_mated")->objectId, (Vec3){0, 0, 1}, 90, NULL, NULL))
        return false;
    BOX("bed_obstruction", "TEST cabinet - blocks bed lift", "PhysicalObject", exercise, "exercise", -.62f,
        -.95f, 1.35f, .5f, .8f, .45f, false);
    BOX("door_obstruction", "TEST post - blocks door swing", "PhysicalObject", exercise, "exercise", .15f,
        .72f, .45f, .12f, .15f, .65f, false);
#undef BOX
    LayoutEntityInfo info = object(l, "water_controller")->info;
    property(&info, "network", "can0");
    property(&info, "node_role", "water_monitor");
    property(&info, "power_domain", "placeholder_24V_to_5V");
    if (!Layout_SetEntityInfo(l, "water_controller", &info, NULL, NULL) ||
        !link(l, LAYOUT_RELATIONSHIP_SUPPORTED_BY, "bed_deck", "bed_rail_driver") ||
        !link(l, LAYOUT_RELATIONSHIP_SUPPORTED_BY, "bed_deck", "bed_rail_passenger") ||
        !link(l, LAYOUT_RELATIONSHIP_ATTACHED_TO, "water_bottom", "oem_floor") ||
        !link(l, LAYOUT_RELATIONSHIP_ATTACHED_TO, "water_pump", "water_bottom") ||
        !link(l, LAYOUT_RELATIONSHIP_CONTAINED_BY, "water_controller", water) ||
        !link(l, LAYOUT_RELATIONSHIP_CONTAINED_BY, "water_tank", water) ||
        !link(l, LAYOUT_RELATIONSHIP_CONTAINED_BY, "battery", power))
        return false;
    LayoutConstraint travel = {.axis = {0, 0, 1}};
    snprintf(travel.id, 64, "bed_lift");
    snprintf(travel.a.entity_id, 64, "bed_guide");
    snprintf(travel.b.entity_id, 64, "bed_deck");
    snprintf(travel.motion_assembly, 64, "%s", bed);
    LayoutConstraint hinge = {.axis = {0, 0, 1}};
    snprintf(hinge.id, 64, "water_door_hinge");
    snprintf(hinge.a.entity_id, 64, "door_mount");
    hinge.a.kind = LAYOUT_REFERENCE_AXIS_U;
    snprintf(hinge.b.entity_id, 64, "water_door");
    hinge.b.kind = LAYOUT_REFERENCE_AXIS_U;
    hinge.b.local_offset_meters[0] = -.375;
    snprintf(hinge.motion_assembly, 64, "%s", door);
    if (!Layout_InitLinearTravel(l, &travel, .90, 1.60) ||
        !Layout_ConstraintEdit(l, &travel, NULL, NULL, NULL) || !Layout_InitAngularTravel(l, &hinge, 0, 90) ||
        !Layout_ConstraintEdit(l, &hinge, NULL, NULL, NULL))
        return false;
    LayoutConstraint height = {.kind = LAYOUT_CONSTRAINT_DISTANCE, .axis = {0, 0, 1}, .target = .57};
    snprintf(height.id, 64, "controller_height");
    snprintf(height.a.entity_id, 64, "water_pump");
    snprintf(height.b.entity_id, 64, "water_controller");
    LayoutConstraint angle = {.kind = LAYOUT_CONSTRAINT_PLANAR_MATE, .axis = {0, 0, 1}, .target = 90};
    snprintf(angle.id, 64, "bracket_right_angle");
    snprintf(angle.a.entity_id, 64, "bracket_fixed");
    angle.a.kind = LAYOUT_REFERENCE_AXIS_U;
    angle.a.local_offset_meters[0] = .07;
    snprintf(angle.b.entity_id, 64, "bracket_mated");
    angle.b.kind = LAYOUT_REFERENCE_AXIS_U;
    angle.b.local_offset_meters[0] = -.07;
    if (!Layout_ConstraintEdit(l, &height, NULL, NULL, NULL) ||
        !Layout_ConstraintEdit(l, &angle, NULL, NULL, NULL))
        return false;
    if (!volume(l, "walkway", "Walkway - illustrative reserved space", LAYOUT_VOLUME_KEEPOUT, "",
                (Vec3){.35f, .80f, .85f}, (Vec3){.60f, 1.30f, 1.70f}) ||
        !volume(l, "fuse_service", "Fuse box - front service access", LAYOUT_VOLUME_SERVICE, "fuse_box",
                (Vec3){-.27f, 1.52f, 1.10f}, (Vec3){.45f, .4f, .55f}) ||
        !Layout_GenerateMotionEnvelope(l, "bed_lift", 33, NULL, NULL) ||
        !Layout_GenerateMotionEnvelope(l, "water_door_hinge", 33, NULL, NULL))
        return false;
    const LayoutMotionEnvelope *b = Layout_FindRuleEnvelope(&l->objectStore, "bed_lift");
    const LayoutMotionEnvelope *d = Layout_FindRuleEnvelope(&l->objectStore, "water_door_hinge");
    if (!check(l, LAYOUT_SPATIAL_NO_INTERSECTION,
               Layout_ObjectStore_FindConst(&l->objectStore, b->object_id)->coreMeta.object_id,
               "bed_obstruction", 0) ||
        !check(l, LAYOUT_SPATIAL_NO_INTERSECTION,
               Layout_ObjectStore_FindConst(&l->objectStore, d->object_id)->coreMeta.object_id,
               "door_obstruction", 0) ||
        !check(l, LAYOUT_SPATIAL_CLEARANCE, "water_controller", "water_pump", .10))
        return false;
    return Layout_ValidateEngineering(l, NULL, 0) && Layout_ValidateConstraints(l, NULL, 0) &&
           Layout_ValidateMotionEnvelopes(l, NULL, 0);
}

bool LineDrawingVanExample_Write(const char *path) {
    Layout l = {0}, loaded = {0};
    char *json = NULL;
    bool ok = false;
    FILE *file = NULL;
    if (!path || !build(&l))
        goto done;
    json = Layout_SaveToString(&l);
    Layout_Init(&loaded, .1f);
    if (!json || !Layout_LoadFromString(&loaded, json))
        goto done;
    cJSON *report = Layout_SpatialReportJson(&loaded);
    cJSON *ranges = cJSON_CreateArray();
    if (!report || !ranges) {
        cJSON_Delete(report);
        cJSON_Delete(ranges);
        goto done;
    }
    const char *rules[] = {"bed_lift", "water_door_hinge"};
    const char *targets[] = {"bed_obstruction", "door_obstruction"};
    for (int i = 0; i < 2; ++i) {
        LayoutMotionRangeResult result;
        if (!Layout_CheckMotionRange(&loaded, Layout_FindRuleEnvelope(&loaded.objectStore, rules[i]),
                                     targets[i], 0, false, 257, &result) ||
            result.status != LAYOUT_MOTION_RANGE_FAILURE) {
            cJSON_Delete(report);
            cJSON_Delete(ranges);
            goto done;
        }
        cJSON *item = cJSON_CreateObject();
        cJSON_AddItemToArray(ranges, item);
        cJSON_AddStringToObject(item, "movement", rules[i]);
        cJSON_AddStringToObject(item, "obstacle", targets[i]);
        cJSON_AddStringToObject(item, "status", "contact_at_tested_pose");
        cJSON_AddNumberToObject(item, "position", result.position);
        cJSON_AddNumberToObject(item, "testedPoses", result.tested_poses);
        cJSON_AddStringToObject(item, "member", result.member_id);
    }
    cJSON_AddItemToObject(report, "exampleRangeChecks", ranges);
    /* Solve the teaching exercise on the reloaded candidate only. The saved
     * starting document intentionally retains both obstructions. */
    if (!Layout_SetObject3DPosition(&loaded, object(&loaded, "bed_obstruction")->objectId,
                                    (Vec3){-.62f, .43f, 1.35f}, NULL) ||
        !Layout_SetObject3DPosition(&loaded, object(&loaded, "door_obstruction")->objectId,
                                    (Vec3){.80f, 1.60f, .45f}, NULL)) {
        cJSON_Delete(report);
        goto done;
    }
    cJSON *resolved = cJSON_CreateArray();
    for (int i = 0; i < 2; ++i) {
        LayoutMotionRangeResult result;
        if (!Layout_CheckMotionRange(&loaded, Layout_FindRuleEnvelope(&loaded.objectStore, rules[i]),
                                     targets[i], 0, false, 257, &result) ||
            result.status != LAYOUT_MOTION_RANGE_CLEAR) {
            cJSON_Delete(resolved);
            cJSON_Delete(report);
            goto done;
        }
        cJSON *item = cJSON_CreateObject();
        cJSON_AddItemToArray(resolved, item);
        cJSON_AddStringToObject(item, "movement", rules[i]);
        cJSON_AddStringToObject(item, "status", "separated_range");
        cJSON_AddNumberToObject(item, "testedPoses", result.tested_poses);
    }
    cJSON *resolved_report = Layout_SpatialReportJson(&loaded);
    if (!resolved_report) {
        cJSON_Delete(resolved);
        cJSON_Delete(report);
        goto done;
    }
    cJSON_AddItemToObject(report, "resolvedExerciseChecks", resolved);
    cJSON_AddItemToObject(report, "resolvedSpatialReport", resolved_report);
    cJSON_AddStringToObject(report, "dimensionsStatus", "illustrative_not_measured");
    char *audit = cJSON_Print(report);
    cJSON_Delete(report);
    if (!audit)
        goto done;
    /* Exclusive creation protects both the tracked starting example and user edits. */
    file = fopen(path, "wx");
    if (file) {
        ok = fwrite(json, 1, strlen(json), file) == strlen(json);
        if (fclose(file) != 0)
            ok = false;
        file = NULL;
    }
    if (ok)
        puts(audit);
    free(audit);
done:
    if (file)
        fclose(file);
    Layout_FreeString(json);
    Layout_Free(&loaded);
    Layout_Free(&l);
    if (!ok)
        fprintf(stderr, "Van example refused: invalid fixture or destination already exists: %s\n",
                path ? path : "");
    return ok;
}
