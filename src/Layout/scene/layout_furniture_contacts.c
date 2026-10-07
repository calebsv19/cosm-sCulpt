#include "Layout/layout_furniture.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static bool fail(char* text, size_t n, const char* id, const char* reason) {
    if (text && n) snprintf(text, n, "Connection %s: %s", id ? id : "", reason);
    return false;
}
static bool bounded(const char* text, size_t n) { return text[0] && memchr(text, 0, n); }
static bool face(const Layout* l, const char* id, int end, double point[3], Vec3* normal) {
    const LayoutFurnitureUnit* u = Layout_FindFurnitureUnit(&l->objectStore, id);
    const LayoutAssembly* a = Layout_FindAssembly(&l->objectStore, id);
    if (!u || !a || (end != -1 && end != 1)) return false;
    const Vec3 axes[] = {a->frame.axisU, a->frame.axisV, a->frame.normal};
    double origin[] = {a->frame.origin.x, a->frame.origin.y, a->frame.origin.z};
    double local[] = {u->center_m[0], u->center_m[1] + end * u->size_m[1] / 2, u->center_m[2]};
    for (int k = 0; k < 3; ++k) point[k] = origin[k] * Layout_WorldScale(l);
    for (int j = 0; j < 3; ++j) {
        point[0] += axes[j].x * local[j]; point[1] += axes[j].y * local[j]; point[2] += axes[j].z * local[j];
    }
    *normal = Vec3_Scale(a->frame.axisV, (float)end);
    return true;
}
static bool error(const Layout* l, const LayoutFurnitureContact* c, double* delta, Vec3* normal) {
    double a[3], b[3]; Vec3 n;
    if (!face(l, c->driver, c->driver_end, a, normal) || !face(l, c->follower, c->follower_end, b, &n) ||
        Vec3_Length(Vec3_Add(*normal, n)) > 1e-5f) return false;
    *delta = c->gap_m - ((b[0] - a[0]) * normal->x + (b[1] - a[1]) * normal->y + (b[2] - a[2]) * normal->z);
    return isfinite(*delta);
}
/* Each unit has at most one enabled driver. Bounded topological traversal detects cycles. */
static bool order(const Layout* l, size_t indices[LAYOUT_MAX_FURNITURE_CONTACTS], char* message, size_t n) {
    const LayoutObjectStore* s = &l->objectStore; bool done[LAYOUT_MAX_FURNITURE_CONTACTS] = {0};
    for (size_t at = 0; at < s->furniture_contact_count; ++at) {
        bool found = false;
        for (size_t i = 0; i < s->furniture_contact_count && !found; ++i) {
            if (done[i]) continue;
            const LayoutFurnitureContact* c = &s->furniture_contacts[i]; bool ready = true;
            if (c->enabled) for (size_t j = 0; j < s->furniture_contact_count; ++j)
                if (!done[j] && s->furniture_contacts[j].enabled && !strcmp(c->driver, s->furniture_contacts[j].follower)) ready = false;
            if (ready) { indices[at] = i; done[i] = true; found = true; }
        }
        if (!found) return fail(message, n, NULL, "driving links form a cycle.");
    }
    return true;
}
bool Layout_ValidateFurnitureContacts(const Layout* l, bool satisfied, char* message, size_t n) {
    if (!l || l->objectStore.furniture_contact_count > LAYOUT_MAX_FURNITURE_CONTACTS)
        return fail(message, n, NULL, "capacity exceeded.");
    const LayoutObjectStore* s = &l->objectStore;
    for (size_t i = 0; i < s->furniture_contact_count; ++i) {
        const LayoutFurnitureContact* c = &s->furniture_contacts[i];
        if (!bounded(c->id, sizeof(c->id)) || !bounded(c->driver, sizeof(c->driver)) ||
            !bounded(c->follower, sizeof(c->follower)) || (c->driver_end != -1 && c->driver_end != 1) ||
            (c->follower_end != -1 && c->follower_end != 1) || c->behavior < LAYOUT_FURNITURE_FOLLOW_MOVE ||
            c->behavior > LAYOUT_FURNITURE_FOLLOW_FIT_RUN || !isfinite(c->gap_m) || c->gap_m < 0 || c->gap_m > 1)
            return fail(message, n, NULL, "invalid endpoints, behavior or gap (0–1 m).");
        if (!Layout_FindFurnitureUnit(s, c->driver) || !Layout_FindFurnitureUnit(s, c->follower) ||
            !strcmp(c->driver, c->follower) || Layout_IsDescendant(s, c->driver, c->follower) ||
            Layout_IsDescendant(s, c->follower, c->driver))
            return fail(message, n, c->id, "choose distinct managed units outside each other's hierarchy.");
        for (size_t j = 0; j < i; ++j) {
            const LayoutFurnitureContact* p = &s->furniture_contacts[j];
            if (!strcmp(c->id, p->id)) return fail(message, n, c->id, "duplicate ID.");
            if (c->enabled && p->enabled && !strcmp(c->follower, p->follower))
                return fail(message, n, c->id, "follower already has an enabled driver.");
        }
        if (c->enabled) {
            double delta; Vec3 normal;
            if (!error(l, c, &delta, &normal)) return fail(message, n, c->id, "run-end planes must be parallel and face one another.");
            if (satisfied && fabs(delta) > 2e-6) return fail(message, n, c->id, "saved faces disagree with the declared gap.");
        }
    }
    size_t indices[LAYOUT_MAX_FURNITURE_CONTACTS];
    return order(l, indices, message, n);
}
bool Layout_SolveFurnitureContacts(Layout* l, const char* protected_unit) {
    if (!l || !l->geometryEditActive || !Layout_ValidateFurnitureContacts(l, false, l->geometryMessage, sizeof(l->geometryMessage))) return false;
    size_t indices[LAYOUT_MAX_FURNITURE_CONTACTS];
    if (!order(l, indices, l->geometryMessage, sizeof(l->geometryMessage))) return false;
    for (size_t at = 0; at < l->objectStore.furniture_contact_count; ++at) {
        const LayoutFurnitureContact* c = &l->objectStore.furniture_contacts[indices[at]];
        if (!c->enabled) continue;
        double delta; Vec3 normal;
        if (!error(l, c, &delta, &normal)) return false;
        if (fabs(delta) <= 2e-6) continue;
        if (protected_unit && !strcmp(protected_unit, c->follower))
            return fail(l->geometryMessage, sizeof(l->geometryMessage), c->id, "this end is driven. Edit the driver or change the retained end.");
        bool ok;
        if (c->behavior == LAYOUT_FURNITURE_FOLLOW_MOVE) {
            double translation[] = {normal.x * delta, normal.y * delta, normal.z * delta};
            ok = Layout_TranslateAssemblyCandidate(l, c->follower, translation);
        } else {
            LayoutFurnitureUnit* u = (LayoutFurnitureUnit*)Layout_FindFurnitureUnit(&l->objectStore, c->follower);
            const LayoutAssembly* a = Layout_FindAssembly(&l->objectStore, c->follower);
            double local = delta * Vec3_Dot(normal, a->frame.axisV);
            u->size_m[1] += c->follower_end * local; u->center_m[1] += local / 2;
            ok = Layout_FurnitureRegenerateCandidate(l, u);
        }
        if (!ok) {
            char reason[192]; snprintf(reason, sizeof(reason), "%.190s", l->geometryMessage);
            return fail(l->geometryMessage, sizeof(l->geometryMessage), c->id, reason[0] ? reason : "follower cannot fit.");
        }
    }
    return Layout_ValidateFurnitureContacts(l, true, l->geometryMessage, sizeof(l->geometryMessage));
}
typedef struct { const LayoutFurnitureContact* value; const char* remove; } Edit;
static bool edit(Layout* l, void* context) {
    Edit* e = context; LayoutObjectStore* s = &l->objectStore;
    const char* id = e->remove ? e->remove : e->value->id;
    size_t i = 0; while (i < s->furniture_contact_count && strcmp(s->furniture_contacts[i].id, id)) ++i;
    if (e->remove) {
        if (i == s->furniture_contact_count) return false;
        memmove(&s->furniture_contacts[i], &s->furniture_contacts[i + 1], (s->furniture_contact_count - i - 1) * sizeof(s->furniture_contacts[0]));
        memset(&s->furniture_contacts[--s->furniture_contact_count], 0, sizeof(s->furniture_contacts[0]));
    } else {
        LayoutFurnitureContact c = *e->value;
        if (!c.id[0]) {
            if (s->furniture_contact_count == LAYOUT_MAX_FURNITURE_CONTACTS || s->next_furniture_contact_id == UINT32_MAX) return false;
            snprintf(c.id, sizeof(c.id), "furniture_fit_%u", s->next_furniture_contact_id++);
            for (size_t j = 0; j < s->furniture_contact_count; ++j) if (!strcmp(c.id, s->furniture_contacts[j].id)) return false;
            i = s->furniture_contact_count++;
        } else if (i == s->furniture_contact_count) return false;
        s->furniture_contacts[i] = c;
    }
    return true;
}
bool Layout_EditFurnitureContact(Layout* l, const LayoutFurnitureContact* c, const char* remove,
                                  LayoutGeometryBeforePublish history, void* context) {
    if ((!c && !remove) || (c && (!memchr(c->id, 0, sizeof(c->id)) || !memchr(c->driver, 0, sizeof(c->driver)) || !memchr(c->follower, 0, sizeof(c->follower))))) return false;
    Edit e = {c, remove};
    return Layout_RunGeometryEdit(l, 0, edit, &e, history, context);
}
