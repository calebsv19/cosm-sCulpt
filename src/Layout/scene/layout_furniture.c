#include "Layout/layout_furniture.h"
#include "Layout/layout_section.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool fail(char* message, size_t capacity, const char* text) {
    if (message && capacity) snprintf(message, capacity, "%s", text);
    return false;
}
static const Object3D* object(const LayoutObjectStore* store, const char* id) {
    for (size_t i = 0; i < store->count; ++i)
        if (!store->items[i].isDeleted && !strcmp(store->items[i].coreMeta.object_id, id))
            return &store->items[i];
    return NULL;
}
const LayoutFurnitureUnit* Layout_FindFurnitureUnit(const LayoutObjectStore* store, const char* id) {
    if (!store || !id || store->furniture_count > LAYOUT_MAX_FURNITURE_UNITS) return NULL;
    for (size_t i = 0; i < store->furniture_count; ++i)
        if (!strcmp(store->furniture[i].assembly_id, id)) return &store->furniture[i];
    return NULL;
}
const LayoutFurnitureUnit* Layout_FurnitureForObject(const LayoutObjectStore* store, uint32_t id) {
    const Object3D* o = store ? Layout_ObjectStore_FindConst(store, id) : NULL;
    return o ? Layout_FindFurnitureUnit(store, o->info.parent_id) : NULL;
}
static bool parameters(const LayoutFurnitureUnit* u) {
    if (!u || !u->assembly_id[0] || !memchr(u->assembly_id, 0, sizeof(u->assembly_id)) ||
        u->part_count < 5 || u->part_count > LAYOUT_MAX_FURNITURE_PARTS ||
        (u->back_sign != -1 && u->back_sign != 1)) return false;
    for (int k = 0; k < 3; ++k)
        if (!isfinite(u->center_m[k]) || fabs(u->center_m[k]) > 1000 ||
            !isfinite(u->size_m[k]) || u->size_m[k] <= 1e-6 || u->size_m[k] > 1000) return false;
    for (int k = 0; k < 4; ++k)
        if (!isfinite(u->thickness_m[k]) || u->thickness_m[k] <= 1e-6 ||
            u->thickness_m[k] >= fmin(u->size_m[0], fmin(u->size_m[1], u->size_m[2])) ||
            !isfinite(u->overhang_m[k]) || u->overhang_m[k] < 0 || u->overhang_m[k] > 1) return false;
    return isfinite(u->shelf_fraction) && u->shelf_fraction > 0 && u->shelf_fraction < 1;
}
/* Return the physical native prism for a named role, never a scaled cabinet mesh. */
bool Layout_FurniturePartGeometry(const Layout* layout, const LayoutFurnitureUnit* u,
                                  size_t index, Object3D* result) {
    if (!layout || !result || !parameters(u) || index >= u->part_count) return false;
    const LayoutAssembly* a = Layout_FindAssembly(&layout->objectStore, u->assembly_id);
    const LayoutFurniturePart* part = &u->parts[index];
    const Object3D* old = object(&layout->objectStore, part->entity_id);
    if (!a || !old || old->kind != OBJECT3D_KIND_RECT_PRISM || old->transform.scale.x != 1 ||
        old->transform.scale.y != 1 || old->transform.scale.z != 1 ||
        strcmp(old->info.parent_id, u->assembly_id)) return false;
    double d = u->size_m[0], l = u->size_m[1], h = u->size_m[2], t = u->thickness_m[0];
    double back = u->thickness_m[1], front = 0, pos[3] = {0}, span[3] = {0};
    for (size_t i = 0; i < u->part_count; ++i)
        if (u->parts[i].role == LAYOUT_FURNITURE_DOOR) front = u->thickness_m[2];
    double inner_d = d - back - front, inner_l = l - 2 * t, inner_h = h - 2 * t;
    if (inner_d <= 1e-6 || inner_l <= 1e-6 || inner_h <= 1e-6) return false;
    pos[0] = u->back_sign * (front - back) / 2;
    span[0] = inner_d; span[1] = inner_l; span[2] = t;
    switch (part->role) {
    case LAYOUT_FURNITURE_BACK:
        pos[0] = u->back_sign * (d - back) / 2;
        span[0] = back; span[1] = l; span[2] = h;
        break;
    case LAYOUT_FURNITURE_END_REAR:
    case LAYOUT_FURNITURE_END_FRONT:
        pos[1] = (part->role == LAYOUT_FURNITURE_END_FRONT ? 1 : -1) * (l - t) / 2;
        span[1] = t; span[2] = h;
        break;
    case LAYOUT_FURNITURE_BOTTOM:
    case LAYOUT_FURNITURE_TOP:
        pos[2] = (part->role == LAYOUT_FURNITURE_TOP ? 1 : -1) * (h - t) / 2;
        break;
    case LAYOUT_FURNITURE_DOOR:
        pos[0] = -u->back_sign * (d - front) / 2;
        span[0] = front; span[1] = l; span[2] = h;
        break;
    case LAYOUT_FURNITURE_SHELF:
        pos[2] = -h / 2 + t + u->shelf_fraction * inner_h;
        if (fabs(pos[2]) + t / 2 > h / 2 - t + 1e-9) return false;
        break;
    case LAYOUT_FURNITURE_WORKTOP:
        span[0] = d + u->overhang_m[0] + u->overhang_m[1];
        span[1] = l + u->overhang_m[2] + u->overhang_m[3];
        span[2] = u->thickness_m[3];
        pos[0] = u->back_sign * (u->overhang_m[0] - u->overhang_m[1]) / 2;
        pos[1] = (u->overhang_m[3] - u->overhang_m[2]) / 2;
        pos[2] = (h + span[2]) / 2;
        break;
    case LAYOUT_FURNITURE_CUSHION:
        if (!isfinite(part->span_m[2]) || part->span_m[2] <= 1e-6 || part->span_m[2] > 1) return false;
        span[0] = d; span[1] = l; span[2] = part->span_m[2];
        pos[0] = 0; pos[2] = (h + span[2]) / 2;
        break;
    case LAYOUT_FURNITURE_RUN_RAIL:
        for (int k = 0; k < 3; ++k) {
            if (!isfinite(part->offset_m[k]) || !isfinite(part->span_m[k])) return false;
            pos[k] = part->offset_m[k]; span[k] = part->span_m[k];
        }
        if (span[0] <= 1e-6 || span[2] <= 1e-6 || span[0] > 1 || span[2] > 1) return false;
        span[1] = l;
        break;
    case LAYOUT_FURNITURE_FIXTURE:
        for (int k = 0; k < 3; ++k) {
            if (!isfinite(part->offset_m[k]) || !isfinite(part->span_m[k]) ||
                part->span_m[k] <= 1e-6 || part->span_m[k] > 1000) return false;
            span[k] = part->span_m[k];
        }
        pos[0] = u->back_sign * (d / 2 - part->offset_m[0]);
        pos[1] = l / 2 - part->offset_m[1];
        pos[2] = h / 2 + part->offset_m[2];
        if (u->sink_opening) {
            const LayoutFurniturePart* first = NULL;
            for (size_t j=0;j<u->part_count;++j) if (u->parts[j].role==LAYOUT_FURNITURE_FIXTURE) { first=&u->parts[j]; break; }
            if (first==part && fabs(part->offset_m[2]-(u->thickness_m[3]-span[2]/2))>1e-9) return false;
        }
        /* Fixed-size fixtures fit the usable footprint; their top may cross a worktop.
         * A real worktop cutout is a separate feature, not inferred from this anchor. */
        if (u->back_sign * pos[0] + span[0] / 2 > d / 2 - back + 1e-6 ||
            -u->back_sign * pos[0] + span[0] / 2 > d / 2 - front + 1e-6 ||
            fabs(pos[1]) + span[1] / 2 > inner_l / 2 + 1e-6 ||
            pos[2] - span[2] / 2 < -h / 2 + t - 1e-6) return false;
        break;
    default: return false;
    }
    double scale = Layout_WorldScale(layout);
    Vec3 axes[] = {a->frame.axisU, a->frame.axisV, a->frame.normal}, center = a->frame.origin;
    for (int k = 0; k < 3; ++k)
        center = Vec3_Add(center, Vec3_Scale(axes[k], (float)((u->center_m[k] + pos[k]) / scale)));
    *result = *old;
    result->transform.position = center;
    result->rectPrism.frame = a->frame;
    result->rectPrism.frame.origin = center;
    result->rectPrism.width = (float)(span[0] / scale);
    result->rectPrism.height = (float)(span[1] / scale);
    result->rectPrism.depth = (float)(span[2] / scale);
    if (u->sink_opening && (part->role == LAYOUT_FURNITURE_TOP || part->role == LAYOUT_FURNITURE_WORKTOP || part->role == LAYOUT_FURNITURE_FIXTURE)) {
        const LayoutFurniturePart* sink = NULL;
        for (size_t j = 0; j < u->part_count; ++j)
            if (u->parts[j].role == LAYOUT_FURNITURE_FIXTURE) { sink = &u->parts[j]; break; }
        if (!sink) return false;
        /* Provisional 2 mm bowl wall and 2 mm installation clearance. */
        double sx = u->back_sign * (d / 2 - sink->offset_m[0]), sy = l / 2 - sink->offset_m[1];
        LayoutPanelOpening f = {.enabled=true};
        if (part->role == LAYOUT_FURNITURE_FIXTURE) {
            if (part != sink) return Layout_ObjectStore_ValidateObject(result);
            f.width = (float)((span[0] - .004) / scale);
            f.height = (float)((span[1] - .004) / scale); f.floor = (float)(.002 / scale);
        } else {
            f.u = (float)((sx - pos[0]) / scale); f.v = (float)((sy - pos[1]) / scale);
            f.width = (float)((sink->span_m[0] + .004) / scale);
            f.height = (float)((sink->span_m[1] + .004) / scale);
        }
        result->rectPrism.opening = f;
    }
    return Layout_ObjectStore_ValidateObject(result);
}
static bool same_geometry(const Object3D* a, const Object3D* b, double scale) {
    if (a->rectPrism.opening.enabled != b->rectPrism.opening.enabled) return false;
    if (a->rectPrism.opening.enabled) {
        const LayoutPanelOpening* x = &a->rectPrism.opening; const LayoutPanelOpening* y = &b->rectPrism.opening;
        const float av[] = {x->u,x->v,x->width,x->height,x->floor}, bv[] = {y->u,y->v,y->width,y->height,y->floor};
        for (int k = 0; k < 5; ++k) if (fabs((double)av[k]-bv[k])*scale > 2e-6) return false;
    }
    float pa[] = {a->transform.position.x, a->transform.position.y, a->transform.position.z};
    float pb[] = {b->transform.position.x, b->transform.position.y, b->transform.position.z};
    for (int k = 0; k < 3; ++k) if (fabs((double)pa[k] - pb[k]) * scale > 2e-6) return false;
    float da[] = {a->rectPrism.width, a->rectPrism.height, a->rectPrism.depth};
    float db[] = {b->rectPrism.width, b->rectPrism.height, b->rectPrism.depth};
    for (int k = 0; k < 3; ++k) if (fabs((double)da[k] - db[k]) * scale > 2e-6) return false;
    Vec3 aa[] = {a->rectPrism.frame.axisU, a->rectPrism.frame.axisV, a->rectPrism.frame.normal};
    Vec3 ab[] = {b->rectPrism.frame.axisU, b->rectPrism.frame.axisV, b->rectPrism.frame.normal};
    for (int k = 0; k < 3; ++k) if (Vec3_Length(Vec3_Sub(aa[k], ab[k])) > 1e-5f) return false;
    return Vec3_Length(Vec3_Sub(a->rectPrism.frame.origin, b->rectPrism.frame.origin)) * scale <= 2e-6;
}
bool Layout_ValidateFurniture(const Layout* layout, char* message, size_t capacity) {
    if (!layout || layout->objectStore.furniture_count > LAYOUT_MAX_FURNITURE_UNITS)
        return fail(message, capacity, "Furniture capacity exceeded.");
    const LayoutObjectStore* s = &layout->objectStore;
    for (size_t i = 0; i < s->furniture_count; ++i) {
        const LayoutFurnitureUnit* u = &s->furniture[i]; unsigned roles = 0;
        if (!parameters(u)) return fail(message, capacity, "Invalid furniture dimensions or thickness.");
        for (size_t j = 0; j < i; ++j)
            if (!strcmp(u->assembly_id, s->furniture[j].assembly_id))
                return fail(message, capacity, "Duplicate furniture unit.");
        for (size_t j = 0; j < u->part_count; ++j) {
            const LayoutFurniturePart* p = &u->parts[j]; Object3D expected;
            if (!p->entity_id[0] || !memchr(p->entity_id, 0, sizeof(p->entity_id)) ||
                p->role < LAYOUT_FURNITURE_BACK || p->role > LAYOUT_FURNITURE_CUSHION ||
                (p->role != LAYOUT_FURNITURE_FIXTURE && (roles & (1u << p->role))))
                return fail(message, capacity, "Invalid or duplicate furniture part role.");
            roles |= 1u << p->role;
            for (size_t k = 0; k <= i; ++k)
                for (size_t n = 0; n < (k == i ? j : s->furniture[k].part_count); ++n)
                    if (!strcmp(p->entity_id, s->furniture[k].parts[n].entity_id))
                        return fail(message, capacity, "A furniture part has multiple owners.");
            if (!Layout_FurniturePartGeometry(layout, u, j, &expected) ||
                !same_geometry(object(s, p->entity_id), &expected, Layout_WorldScale(layout)))
                return fail(message, capacity, "Managed part conflicts with its unit. Use Edit unit or Cancel the part edit.");
        }
        if ((roles & 31u) != 31u) return fail(message, capacity, "Furniture needs back, two ends, bottom and top.");
    }
    return Layout_ValidateFurnitureContacts(layout, !layout->geometryEditActive, message, capacity);
}
typedef struct { LayoutFurnitureUnit unit; int keep; } Edit;
static bool fits_bounds(const Layout* layout, const Object3D* o) {
    if (!layout->scene3d.bounds.enabled) return true;
    Vec3 corners[8];
    if (!Layout_Object3D_ComputeRectPrismCorners(o, corners)) return false;
    const SceneBounds3D* b = &layout->scene3d.bounds;
    for (int i = 0; i < 8; ++i)
        if (corners[i].x < b->min.x - 1e-6f || corners[i].x > b->max.x + 1e-6f ||
            corners[i].y < b->min.y - 1e-6f || corners[i].y > b->max.y + 1e-6f ||
            corners[i].z < b->min.z - 1e-6f || corners[i].z > b->max.z + 1e-6f) return false;
    return true;
}
static bool edit(Layout* layout, void* context) {
    Edit* e = context;
    LayoutFurnitureUnit* old = (LayoutFurnitureUnit*)Layout_FindFurnitureUnit(&layout->objectStore, e->unit.assembly_id);
    if (!old || e->keep < -1 || e->keep > 1 || e->unit.part_count != old->part_count ||
        e->unit.sink_opening != old->sink_opening || e->unit.back_sign != old->back_sign)
        return fail(layout->geometryMessage, sizeof(layout->geometryMessage), "Unit identity and part roles cannot change during resizing.");
    for (size_t j = 0; j < old->part_count; ++j) {
        LayoutFurniturePart a = old->parts[j], b = e->unit.parts[j];
        if (old->sink_opening && a.role == LAYOUT_FURNITURE_FIXTURE) {
            memset(a.offset_m, 0, sizeof(a.offset_m)); memset(b.offset_m, 0, sizeof(b.offset_m));
            memset(a.span_m, 0, sizeof(a.span_m)); memset(b.span_m, 0, sizeof(b.span_m));
        }
        if (memcmp(&a, &b, sizeof(a))) return false;
    }
    LayoutFurnitureUnit next = e->unit;
    memcpy(next.center_m, old->center_m, sizeof(next.center_m));
    next.center_m[0] += old->back_sign * (old->size_m[0] - next.size_m[0]) / 2;
    next.center_m[1] += e->keep * (old->size_m[1] - next.size_m[1]) / 2;
    next.center_m[2] += (next.size_m[2] - old->size_m[2]) / 2;
    if (next.sink_opening) for (size_t j=0;j<next.part_count;++j)
        if (next.parts[j].role == LAYOUT_FURNITURE_FIXTURE) {
            next.parts[j].offset_m[2] = next.thickness_m[3]-next.parts[j].span_m[2]/2;
            break;
        }
    if (!parameters(&next)) return fail(layout->geometryMessage, sizeof(layout->geometryMessage), "Enter positive dimensions and smaller panel thicknesses.");
    *old = next;
    return Layout_FurnitureRegenerateCandidate(layout, old) &&
        Layout_SolveFurnitureContacts(layout, old->assembly_id);
}
bool Layout_FurnitureRegenerateCandidate(Layout* layout, const LayoutFurnitureUnit* unit) {
    if (!layout || !layout->geometryEditActive || !unit) return false;
    for (size_t i = 0; i < unit->part_count; ++i) {
        Object3D expected;
        if (!Layout_FurniturePartGeometry(layout, unit, i, &expected))
            return fail(layout->geometryMessage, sizeof(layout->geometryMessage), "Unit is too small for its panels, shelf or fixed-size sink.");
        Object3D* live = Layout_ObjectStore_Find(&layout->objectStore, expected.objectId);
        if (same_geometry(live, &expected, Layout_WorldScale(layout))) continue;
        if (live->coreMeta.flags.locked)
            return fail(layout->geometryMessage, sizeof(layout->geometryMessage), "A unit member is locked; no parts changed.");
        if ((live->rectPrism.lockToBounds && !fits_bounds(layout, &expected)) ||
            live->rectPrism.lockToConstructionPlane)
            return fail(layout->geometryMessage, sizeof(layout->geometryMessage), "Unit member has a bounds or construction-plane lock.");
        *live = expected;
    }
    return true;
}
bool Layout_EditFurnitureUnit(Layout* layout, const LayoutFurnitureUnit* unit, int keep_length,
                              LayoutGeometryBeforePublish history, void* context) {
    if (!unit) return false;
    Edit e = {*unit, keep_length};
    return Layout_RunGeometryEdit(layout, 0, edit, &e, history, context);
}
