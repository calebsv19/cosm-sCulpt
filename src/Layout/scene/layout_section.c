#include "Layout/layout_section.h"
#include "Layout/layout_engineering.h"
#include "Math/math_util.h"
#include <math.h>
#include <float.h>
#include <string.h>

static float coord(Vec3 p, int axis) { return axis == 0 ? p.x : axis == 1 ? p.y : p.z; }
static float signed_distance(Vec3 p, int axis, float position, bool flipped) {
    float d = coord(p, axis) - position;
    return flipped ? -d : d;
}
size_t Layout_ClipSectionPolygon(const Vec3* input, size_t count, int axis, float position, bool flipped,
                                 Vec3 output[12]) {
    if (!input || !output || count < 3 || count > 8 || axis < 0 || axis > 2)
        return 0;
    size_t n = 0;
    for (size_t i = 0; i < count; ++i) {
        Vec3 a = input[i], b = input[(i + 1) % count];
        float da = signed_distance(a, axis, position, flipped);
        float db = signed_distance(b, axis, position, flipped);
        if (da <= 0)
            output[n++] = a;
        if ((da < 0 && db > 0) || (da > 0 && db < 0))
            output[n++] = Vec3_Add(a, Vec3_Scale(Vec3_Sub(b, a), da / (da - db)));
    }
    return n;
}
static void unique_point(Vec3* points, size_t* count, Vec3 p) {
    for (size_t i = 0; i < *count; ++i)
        if (Vec3_Length(Vec3_Sub(points[i], p)) < 1e-6f)
            return;
    if (*count < 8)
        points[(*count)++] = p;
}
static size_t section_polygon(const Vec3 corners[8], int axis, float position, Vec3 points[8]) {
    static const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                     {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    size_t n = 0;
    for (int e = 0; e < 12; ++e) {
        Vec3 a = corners[edges[e][0]], b = corners[edges[e][1]];
        float da = coord(a, axis) - position, db = coord(b, axis) - position;
        if (fabsf(da) <= 1e-7f)
            unique_point(points, &n, a);
        if (fabsf(db) <= 1e-7f)
            unique_point(points, &n, b);
        if ((da < 0 && db > 0) || (da > 0 && db < 0))
            unique_point(points, &n, Vec3_Add(a, Vec3_Scale(Vec3_Sub(b, a), da / (da - db))));
    }
    if (n < 3)
        return 0;
    Vec3 center = {0};
    for (size_t i = 0; i < n; ++i)
        center = Vec3_Add(center, Vec3_Scale(points[i], 1.0f / (float)n));
    int u = (axis + 1) % 3, v = (axis + 2) % 3;
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j) {
            Vec3 a = Vec3_Sub(points[i], center), b = Vec3_Sub(points[j], center);
            if (atan2f(coord(a, v), coord(a, u)) > atan2f(coord(b, v), coord(b, u))) {
                Vec3 tmp = points[i];
                points[i] = points[j];
                points[j] = tmp;
            }
        }
    return n;
}
static void triangulate(const Vec3* p, size_t n, bool cap, bool reverse, LayoutSurfaceTriangle* out,
                        size_t* count) {
    for (size_t i = 1; i + 1 < n && *count < LAYOUT_SURFACE_MAX_TRIANGLES; ++i)
        out[(*count)++] = (LayoutSurfaceTriangle){p[0], p[reverse ? i + 1 : i], p[reverse ? i : i + 1], cap};
}
bool Layout_PanelOpeningValid(const RectPrismPrimitive3D* p) {
    const LayoutPanelOpening* f = &p->opening;
    if (!f->enabled) return true;
    return isfinite(f->u) && isfinite(f->v) && isfinite(f->width) && isfinite(f->height) &&
        isfinite(f->floor) && f->width > 1e-6f && f->height > 1e-6f && f->floor >= 0 &&
        f->floor < p->depth - 1e-6f && fabsf(f->u) + f->width / 2 < p->width / 2 - 1e-6f &&
        fabsf(f->v) + f->height / 2 < p->height / 2 - 1e-6f;
}
static Object3D cell(const Object3D* o, float x0, float x1, float y0, float y1, float z0, float z1) {
    Object3D p = *o;
    p.rectPrism.opening = (LayoutPanelOpening){0};
    p.rectPrism.width = x1-x0; p.rectPrism.height = y1-y0; p.rectPrism.depth = z1-z0;
    Vec3 delta = Vec3_Add(Vec3_Scale(o->rectPrism.frame.axisU, (x0+x1)/2),
                          Vec3_Scale(o->rectPrism.frame.axisV, (y0+y1)/2));
    delta = Vec3_Add(delta, Vec3_Scale(o->rectPrism.frame.normal, (z0+z1)/2));
    p.transform.position = Vec3_Add(o->transform.position, delta);
    p.rectPrism.frame.origin = p.transform.position;
    return p;
}
size_t Layout_NativeSolidCells(const Object3D* o, Object3D cells[5]) {
    if (!o || !cells || !Layout_ObjectStore_ValidateObject(o)) return 0;
    if (o->kind != OBJECT3D_KIND_RECT_PRISM || !o->rectPrism.opening.enabled) { cells[0] = *o; return 1; }
    const RectPrismPrimitive3D* p = &o->rectPrism; const LayoutPanelOpening* f = &p->opening;
    float x = p->width/2, y = p->height/2, z = p->depth/2;
    float a = f->u-f->width/2, b = f->u+f->width/2, c = f->v-f->height/2, d = f->v+f->height/2;
    cells[0] = cell(o,-x,a,-y,y,-z,z); cells[1] = cell(o,b,x,-y,y,-z,z);
    cells[2] = cell(o,a,b,-y,c,-z,z); cells[3] = cell(o,a,b,d,y,-z,z);
    if (f->floor > 0) { cells[4] = cell(o,a,b,c,d,-z,-z+f->floor); return 5; }
    return 4;
}
void Layout_PanelOpeningCorners(const Object3D* o, Vec3 corners[8]) {
    const LayoutPanelOpening* f = &o->rectPrism.opening;
    Object3D hole = cell(o,f->u-f->width/2,f->u+f->width/2,
        f->v-f->height/2,f->v+f->height/2,-o->rectPrism.depth/2+f->floor,o->rectPrism.depth/2);
    (void)Layout_Object3D_ComputeRectPrismCorners(&hole,corners);
}
size_t Layout_BuildNativeSurface(const Object3D* object, const LayoutSectionView* section,
                                 double meters_per_world,
                                 LayoutSurfaceTriangle out[LAYOUT_SURFACE_MAX_TRIANGLES]) {
    static const int faces[6][4] = {{3, 2, 1, 0}, {4, 5, 6, 7}, {0, 1, 5, 4},
                                    {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5}};
    if (!object || !out || !Layout_ObjectStore_ValidateObject(object) || !isfinite(meters_per_world) ||
        meters_per_world <= 0)
        return 0;
    LayoutSectionView off = {0};
    if (!section)
        section = &off;
    if (section->mode < LAYOUT_SECTION_OFF || section->mode > LAYOUT_SECTION_CUTAWAY)
        return 0;
    if (section->axis < 0 || section->axis > 2 || !isfinite(section->position_meters))
        return 0;
    float position = (float)(section->position_meters / meters_per_world);
    Vec3 corners[8];
    size_t count = 0;
    if (object->kind == OBJECT3D_KIND_PLANE) {
        if (section->mode == LAYOUT_SECTION_EXACT || !Layout_Object3D_ComputePlaneCorners(object, corners))
            return 0;
        Vec3 clipped[12];
        size_t n = 4;
        if (section->mode == LAYOUT_SECTION_CUTAWAY) {
            n = Layout_ClipSectionPolygon(corners, 4, section->axis, position, section->flipped, clipped);
            triangulate(clipped, n, false, false, out, &count);
        } else
            triangulate(corners, n, false, false, out, &count);
        return count;
    }
    if (object->kind != OBJECT3D_KIND_RECT_PRISM || !Layout_Object3D_ComputeRectPrismCorners(object, corners))
        return 0;
    if (object->rectPrism.opening.enabled) {
        Object3D cells[5]; size_t total = Layout_NativeSolidCells(object,cells);
        for (size_t i = 0; i < total; ++i) {
            (void)Layout_Object3D_ComputeRectPrismCorners(&cells[i],corners);
            if (section->mode != LAYOUT_SECTION_EXACT) for (int f = 0; f < 6; ++f) {
                if ((i < 2 && f == (i == 0 ? 5 : 4)) || (i >= 2 && i < 4 && f >= 4)) continue;
                if (i == 4 && f >= 2) continue;
                if ((i == 2 && f == 3) || (i == 3 && f == 2)) continue;
                Vec3 quad[4], clipped[12];
                for (int j = 0; j < 4; ++j) quad[j] = corners[faces[f][j]];
                size_t n = 4;
                if (section->mode == LAYOUT_SECTION_CUTAWAY) {
                    n = Layout_ClipSectionPolygon(quad,4,section->axis,position,section->flipped,clipped);
                    triangulate(clipped,n,false,false,out,&count);
                } else triangulate(quad,n,false,false,out,&count);
            }
            if (section->mode != LAYOUT_SECTION_OFF) {
                Vec3 cap[8]; size_t n = section_polygon(corners,section->axis,position,cap);
                triangulate(cap,n,true,section->flipped,out,&count);
            }
        }
        /* The four exposed cavity walls. Retained floor closes a sink bowl. */
        Vec3 inner[8]; Layout_PanelOpeningCorners(object,inner);
        if (section->mode != LAYOUT_SECTION_EXACT) for (int f = 2; f < 6; ++f) {
            Vec3 quad[4], clipped[12];
            for (int j = 0; j < 4; ++j) quad[j] = inner[faces[f][3-j]];
            size_t n = 4;
            if (section->mode == LAYOUT_SECTION_CUTAWAY) {
                n = Layout_ClipSectionPolygon(quad,4,section->axis,position,section->flipped,clipped);
                triangulate(clipped,n,false,false,out,&count);
            } else triangulate(quad,n,false,false,out,&count);
        }
        return count;
    }
    Vec3 cap[8];
    size_t cap_count =
        section->mode == LAYOUT_SECTION_OFF ? 0 : section_polygon(corners, section->axis, position, cap);
    if (section->mode != LAYOUT_SECTION_EXACT)
        for (int f = 0; f < 6; ++f) {
            Vec3 quad[4], clipped[12];
            for (int i = 0; i < 4; ++i)
                quad[i] = corners[faces[f][i]];
            if (section->mode == LAYOUT_SECTION_CUTAWAY) {
                size_t n =
                    Layout_ClipSectionPolygon(quad, 4, section->axis, position, section->flipped, clipped);
                triangulate(clipped, n, false, false, out, &count);
            } else
                triangulate(quad, 4, false, false, out, &count);
        }
    triangulate(cap, cap_count, true, section->flipped, out, &count);
    return count;
}
bool Layout_SectionRange(const Layout* layout, int axis, double* minimum, double* maximum) {
    if (!layout || !minimum || !maximum || axis < 0 || axis > 2)
        return false;
    double lo = INFINITY, hi = -INFINITY, scale = Layout_WorldScale(layout);
    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* o = &layout->objectStore.items[i];
        Vec3 p[8];
        size_t n = 0;
        if (!Layout_ObjectShown(&layout->objectStore, o) || Layout_EntityIsSpatialGuide(&o->info))
            continue;
        if (o->kind == OBJECT3D_KIND_RECT_PRISM && Layout_Object3D_ComputeRectPrismCorners(o, p))
            n = 8;
        else if (o->kind == OBJECT3D_KIND_PLANE && Layout_Object3D_ComputePlaneCorners(o, p))
            n = 4;
        else if (o->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE &&
                 Layout_Object3D_ComputeMeshInstanceCorners(o, p))
            n = 8;
        for (size_t j = 0; j < n; ++j) {
            double v = coord(p[j], axis) * scale;
            lo = fmin(lo, v);
            hi = fmax(hi, v);
        }
    }
    if (!isfinite(lo) || !isfinite(hi) || hi - lo < 1e-9)
        return false;
    *minimum = lo;
    *maximum = hi;
    return true;
}
int Layout_PanelThicknessAxis(const Object3D* object) {
    if (!object || object->kind != OBJECT3D_KIND_RECT_PRISM || strcmp(object->info.entity_type, "Panel"))
        return -1;
    /* The current panel contract identifies thickness by the smallest local extent. */
    float d[3] = {object->rectPrism.width, object->rectPrism.height, object->rectPrism.depth};
    int axis = 0;
    for (int i = 1; i < 3; ++i)
        if (d[i] < d[axis])
            axis = i;
    return axis;
}
bool Layout_SetPanelThickness(Layout* layout, uint32_t object_id, double meters, int keep_face,
                              LayoutGeometryBeforePublish history, void* context) {
    const Object3D* o = layout ? Layout_ObjectStore_FindConst(&layout->objectStore, object_id) : NULL;
    int axis = Layout_PanelThicknessAxis(o);
    if (axis < 0 || keep_face < -1 || keep_face > 1 || !isfinite(meters) || meters <= 0 ||
        o->transform.scale.x != 1 || o->transform.scale.y != 1 || o->transform.scale.z != 1)
        return false;
    Object3D next = *o;
    float* dims[3] = {&next.rectPrism.width, &next.rectPrism.height, &next.rectPrism.depth};
    double value = meters / Layout_WorldScale(layout);
    /* Preserve the identified thickness direction; forbid a thickness that overtakes either panel span. */
    if (value > FLT_MAX || value < 1e-6 || value >= *dims[(axis + 1) % 3] || value >= *dims[(axis + 2) % 3])
        return false;
    if ((float)value == *dims[axis])
        return true;
    Vec3 basis = axis == 0   ? next.rectPrism.frame.axisU
                 : axis == 1 ? next.rectPrism.frame.axisV
                             : next.rectPrism.frame.normal;
    float shift = (float)keep_face * (*dims[axis] - (float)value) * .5f;
    Vec3 delta = Vec3_Scale(Vec3_Normalize(basis), shift);
    next.transform.position = Vec3_Add(next.transform.position, delta);
    next.rectPrism.frame.origin = Vec3_Add(next.rectPrism.frame.origin, delta);
    *dims[axis] = (float)value;
    return Layout_ReplaceGeometryObject(layout, &next, history, context);
}

static cJSON* vector_json(Vec3 p) {
    cJSON* j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j,"x",p.x); cJSON_AddNumberToObject(j,"y",p.y); cJSON_AddNumberToObject(j,"z",p.z);
    return j;
}
cJSON* Layout_PanelRuntimeMesh(const Object3D* o) {
    if (!o || o->kind != OBJECT3D_KIND_RECT_PRISM || !o->rectPrism.opening.enabled) return NULL;
    LayoutSurfaceTriangle surface[LAYOUT_SURFACE_MAX_TRIANGLES];
    size_t n = Layout_BuildNativeSurface(o,NULL,1,surface);
    if (!n) return NULL;
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root,"schema_family","codework_geometry");
    cJSON_AddStringToObject(root,"schema_variant","mesh_asset_runtime_v1");
    cJSON_AddNumberToObject(root,"schema_version",1);
    cJSON_AddStringToObject(root,"asset_id",o->coreMeta.object_id);
    cJSON_AddStringToObject(root,"source_asset_id",o->coreMeta.object_id);
    cJSON_AddStringToObject(root,"asset_type","solid_mesh");
    Vec3 lo = {FLT_MAX,FLT_MAX,FLT_MAX}, hi = {-FLT_MAX,-FLT_MAX,-FLT_MAX};
    cJSON* mesh = cJSON_AddObjectToObject(root,"mesh");
    cJSON* vertices = cJSON_AddArrayToObject(mesh,"vertices"), *triangles = cJSON_AddArrayToObject(mesh,"triangles");
    /* Keep the translation in the scene object. Evaluated vertices include its rigid frame. */
    for (size_t i = 0; i < n; ++i) {
        Vec3 p[] = {surface[i].a,surface[i].b,surface[i].c};
        for (int k = 0; k < 3; ++k) {
            p[k] = Vec3_Sub(p[k],o->transform.position);
            lo = (Vec3){fminf(lo.x,p[k].x),fminf(lo.y,p[k].y),fminf(lo.z,p[k].z)};
            hi = (Vec3){fmaxf(hi.x,p[k].x),fmaxf(hi.y,p[k].y),fmaxf(hi.z,p[k].z)};
            cJSON_AddItemToArray(vertices,vector_json(p[k]));
        }
        cJSON* t = cJSON_CreateObject(); cJSON_AddItemToArray(triangles,t);
        cJSON_AddNumberToObject(t,"a",i*3); cJSON_AddNumberToObject(t,"b",i*3+1); cJSON_AddNumberToObject(t,"c",i*3+2);
        cJSON_AddStringToObject(t,"surface_group_id","panel");
    }
    cJSON_AddNumberToObject(mesh,"vertex_count",n*3); cJSON_AddNumberToObject(mesh,"triangle_count",n);
    cJSON* bounds = cJSON_AddObjectToObject(root,"local_bounds");
    cJSON_AddItemToObject(bounds,"min",vector_json(lo)); cJSON_AddItemToObject(bounds,"max",vector_json(hi));
    cJSON* groups = cJSON_AddArrayToObject(root,"surface_groups"), *g = cJSON_CreateObject(); cJSON_AddItemToArray(groups,g);
    cJSON_AddStringToObject(g,"group_id","panel"); cJSON* span = cJSON_AddObjectToObject(g,"triangle_span");
    cJSON_AddNumberToObject(span,"start",0); cJSON_AddNumberToObject(span,"count",n);
    return root;
}
