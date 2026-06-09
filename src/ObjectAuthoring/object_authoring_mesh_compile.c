#include "ObjectAuthoring/object_authoring_mesh_compile.h"

#include "Layout/scene/layout_object_faces.h"
#include "ObjectAuthoring/object_authoring_eval.h"
#include "core_io.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../external/cjson/cJSON.h"

typedef struct {
    size_t vertex_count;
    size_t triangle_count;
    size_t surface_group_count;
} ObjectAuthoringMeshCounts;

static void ObjectAuthoringMesh_SetDiag(char* diagnostics,
                                        size_t diagnostics_size,
                                        const char* message) {
    if (!diagnostics || diagnostics_size == 0u) return;
    if (!message) {
        diagnostics[0] = '\0';
        return;
    }
    snprintf(diagnostics, diagnostics_size, "%s", message);
}

static char* ObjectAuthoringMesh_PrintJson(const cJSON* root) {
    size_t capacity = 65536u;
    const size_t max_capacity = 4u * 1024u * 1024u;
    if (!root) return NULL;
    while (capacity <= max_capacity) {
        char* text = (char*)malloc(capacity);
        if (!text) return NULL;
        if (cJSON_PrintPreallocated((cJSON*)root, text, (int)capacity, cJSON_True)) {
            return text;
        }
        free(text);
        capacity *= 2u;
    }
    return NULL;
}

static CoreObjectVec3 ObjectAuthoringMesh_CoreVec3(Vec3 value) {
    return (CoreObjectVec3){
        .x = (double)value.x,
        .y = (double)value.y,
        .z = (double)value.z
    };
}

static Object3D ObjectAuthoringMesh_ObjectFromBody(const ObjectAuthoringBody* body) {
    Object3D object;
    memset(&object, 0, sizeof(object));
    if (!body) return object;
    object.objectId = body->bodyId;
    object.kind = body->sourceKind;
    object.transform = body->transform;
    object.plane = body->plane;
    object.rectPrism = body->rectPrism;
    object.isDeleted = false;
    core_object_init(&object.coreMeta);
    snprintf(object.coreMeta.object_id,
             sizeof(object.coreMeta.object_id),
             "body_%u",
             body->bodyId);
    snprintf(object.coreMeta.object_type,
             sizeof(object.coreMeta.object_type),
             "%s",
             body->sourceKind == OBJECT3D_KIND_PLANE ? "plane_primitive"
                                                     : "rect_prism_primitive");
    object.coreMeta.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
    object.coreMeta.locked_plane = CORE_OBJECT_PLANE_XY;
    object.coreMeta.transform.position = ObjectAuthoringMesh_CoreVec3(body->transform.position);
    object.coreMeta.transform.rotation_deg = ObjectAuthoringMesh_CoreVec3(body->transform.rotationDeg);
    object.coreMeta.transform.scale = ObjectAuthoringMesh_CoreVec3(body->transform.scale);
    return object;
}

static bool ObjectAuthoringMesh_CountBody(const ObjectAuthoringBody* body,
                                          ObjectAuthoringMeshCounts* counts) {
    if (!body || !counts || body->bodyId == 0u) return false;
    switch (body->sourceKind) {
        case OBJECT3D_KIND_PLANE:
            counts->vertex_count += 4u;
            counts->triangle_count += 2u;
            counts->surface_group_count += 1u;
            return true;
        case OBJECT3D_KIND_RECT_PRISM:
            counts->vertex_count += 8u;
            counts->triangle_count += 12u;
            counts->surface_group_count += 6u;
            return true;
        case OBJECT3D_KIND_UNKNOWN:
        default:
            return false;
    }
}

static bool ObjectAuthoringMesh_UpdateBounds(CoreMeshAssetBounds3* bounds,
                                             CoreObjectVec3 position,
                                             bool* has_bounds) {
    if (!bounds || !has_bounds ||
        !isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z)) {
        return false;
    }
    if (!*has_bounds) {
        bounds->min = position;
        bounds->max = position;
        *has_bounds = true;
        return true;
    }
    bounds->min.x = fmin(bounds->min.x, position.x);
    bounds->min.y = fmin(bounds->min.y, position.y);
    bounds->min.z = fmin(bounds->min.z, position.z);
    bounds->max.x = fmax(bounds->max.x, position.x);
    bounds->max.y = fmax(bounds->max.y, position.y);
    bounds->max.z = fmax(bounds->max.z, position.z);
    return true;
}

void ObjectAuthoringRuntimeMesh_Init(ObjectAuthoringRuntimeMesh* mesh) {
    if (!mesh) return;
    memset(mesh, 0, sizeof(*mesh));
    core_mesh_asset_runtime_contract_init(&mesh->contract);
}

void ObjectAuthoringRuntimeMesh_Free(ObjectAuthoringRuntimeMesh* mesh) {
    if (!mesh) return;
    free(mesh->vertices);
    free(mesh->triangles);
    free(mesh->surfaceGroups);
    ObjectAuthoringRuntimeMesh_Init(mesh);
}

static bool ObjectAuthoringRuntimeMesh_SetVertexCount(ObjectAuthoringRuntimeMesh* mesh,
                                                      size_t vertex_count) {
    if (!mesh || vertex_count == 0u) return false;
    mesh->vertices = (ObjectAuthoringRuntimeMeshVertex*)calloc(
        vertex_count,
        sizeof(*mesh->vertices));
    if (!mesh->vertices) return false;
    mesh->vertexCount = vertex_count;
    mesh->contract.vertex_count = vertex_count;
    return true;
}

static bool ObjectAuthoringRuntimeMesh_SetTriangleCount(ObjectAuthoringRuntimeMesh* mesh,
                                                        size_t triangle_count) {
    if (!mesh || triangle_count == 0u) return false;
    mesh->triangles = (ObjectAuthoringRuntimeMeshTriangle*)calloc(
        triangle_count,
        sizeof(*mesh->triangles));
    if (!mesh->triangles) return false;
    mesh->triangleCount = triangle_count;
    mesh->contract.triangle_count = triangle_count;
    return true;
}

static bool ObjectAuthoringRuntimeMesh_SetSurfaceGroupCount(ObjectAuthoringRuntimeMesh* mesh,
                                                            size_t surface_group_count) {
    if (!mesh || surface_group_count == 0u) return false;
    mesh->surfaceGroups = (ObjectAuthoringRuntimeMeshSurfaceGroup*)calloc(
        surface_group_count,
        sizeof(*mesh->surfaceGroups));
    if (!mesh->surfaceGroups) return false;
    mesh->surfaceGroupCount = surface_group_count;
    return true;
}

static bool ObjectAuthoringRuntimeMesh_SurfaceGroupExists(
    const ObjectAuthoringRuntimeMesh* mesh,
    const char* group_id) {
    if (!mesh || !group_id || group_id[0] == '\0') return false;
    for (size_t i = 0u; i < mesh->surfaceGroupCount; ++i) {
        if (strcmp(mesh->surfaceGroups[i].groupId, group_id) == 0) {
            return true;
        }
    }
    return false;
}

static bool ObjectAuthoringRuntimeMesh_BoundsContain(CoreMeshAssetBounds3 bounds,
                                                     CoreObjectVec3 point) {
    const double eps = 1e-7;
    return point.x >= bounds.min.x - eps && point.x <= bounds.max.x + eps &&
           point.y >= bounds.min.y - eps && point.y <= bounds.max.y + eps &&
           point.z >= bounds.min.z - eps && point.z <= bounds.max.z + eps;
}

static double ObjectAuthoringRuntimeMesh_TriangleAreaSq(
    const ObjectAuthoringRuntimeMesh* mesh,
    const ObjectAuthoringRuntimeMeshTriangle* triangle) {
    CoreObjectVec3 a = mesh->vertices[triangle->a].position;
    CoreObjectVec3 b = mesh->vertices[triangle->b].position;
    CoreObjectVec3 c = mesh->vertices[triangle->c].position;
    double ux = b.x - a.x;
    double uy = b.y - a.y;
    double uz = b.z - a.z;
    double vx = c.x - a.x;
    double vy = c.y - a.y;
    double vz = c.z - a.z;
    double nx = uy * vz - uz * vy;
    double ny = uz * vx - ux * vz;
    double nz = ux * vy - uy * vx;
    return nx * nx + ny * ny + nz * nz;
}

bool ObjectAuthoringRuntimeMesh_Validate(const ObjectAuthoringRuntimeMesh* mesh,
                                         char* diagnostics,
                                         size_t diagnostics_size) {
    CoreResult result;
    ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "");
    if (!mesh) {
        ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "runtime mesh is null");
        return false;
    }
    result = core_mesh_asset_runtime_contract_validate(&mesh->contract);
    if (result.code != CORE_OK) {
        ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, result.message);
        return false;
    }
    if (mesh->vertexCount == 0u || !mesh->vertices ||
        mesh->triangleCount == 0u || !mesh->triangles ||
        mesh->surfaceGroupCount == 0u || !mesh->surfaceGroups) {
        ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "runtime mesh arrays are missing");
        return false;
    }
    if (mesh->contract.vertex_count != mesh->vertexCount ||
        mesh->contract.triangle_count != mesh->triangleCount) {
        ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "runtime mesh counts are inconsistent");
        return false;
    }
    for (size_t i = 0u; i < mesh->vertexCount; ++i) {
        CoreObjectVec3 p = mesh->vertices[i].position;
        if (!isfinite(p.x) || !isfinite(p.y) || !isfinite(p.z) ||
            !ObjectAuthoringRuntimeMesh_BoundsContain(mesh->contract.local_bounds, p)) {
            ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "runtime vertex is invalid");
            return false;
        }
    }
    for (size_t i = 0u; i < mesh->surfaceGroupCount; ++i) {
        const ObjectAuthoringRuntimeMeshSurfaceGroup* group = &mesh->surfaceGroups[i];
        if (group->groupId[0] == '\0' || group->triangleCount == 0u ||
            group->triangleStart > mesh->triangleCount ||
            group->triangleCount > mesh->triangleCount - group->triangleStart) {
            ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "runtime surface group is invalid");
            return false;
        }
    }
    for (size_t i = 0u; i < mesh->triangleCount; ++i) {
        const ObjectAuthoringRuntimeMeshTriangle* triangle = &mesh->triangles[i];
        if (triangle->a >= mesh->vertexCount || triangle->b >= mesh->vertexCount ||
            triangle->c >= mesh->vertexCount || triangle->a == triangle->b ||
            triangle->a == triangle->c || triangle->b == triangle->c ||
            ObjectAuthoringRuntimeMesh_TriangleAreaSq(mesh, triangle) <= 1e-18 ||
            !ObjectAuthoringRuntimeMesh_SurfaceGroupExists(mesh, triangle->surfaceGroupId)) {
            ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "runtime triangle is invalid");
            return false;
        }
    }
    return true;
}

static cJSON* ObjectAuthoringRuntimeMesh_Vec3Json(CoreObjectVec3 value) {
    cJSON* node = cJSON_CreateObject();
    if (!node) return NULL;
    cJSON_AddNumberToObject(node, "x", value.x);
    cJSON_AddNumberToObject(node, "y", value.y);
    cJSON_AddNumberToObject(node, "z", value.z);
    return node;
}

bool ObjectAuthoringRuntimeMesh_SaveFile(const ObjectAuthoringRuntimeMesh* mesh,
                                         const char* path,
                                         char* diagnostics,
                                         size_t diagnostics_size) {
    cJSON* root = NULL;
    cJSON* bounds = NULL;
    cJSON* mesh_node = NULL;
    cJSON* vertices = NULL;
    cJSON* triangles = NULL;
    cJSON* groups = NULL;
    char* text = NULL;
    CoreResult result;

    if (!ObjectAuthoringRuntimeMesh_Validate(mesh, diagnostics, diagnostics_size) ||
        !path || path[0] == '\0') {
        if (!path || path[0] == '\0') {
            ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "invalid runtime mesh path");
        }
        return false;
    }

    root = cJSON_CreateObject();
    if (!root) goto oom;
    cJSON_AddStringToObject(root, "schema_family", "codework_geometry");
    cJSON_AddStringToObject(root, "schema_variant", "mesh_asset_runtime_v1");
    cJSON_AddNumberToObject(root, "schema_version", CORE_MESH_ASSET_SCHEMA_VERSION_1);
    cJSON_AddStringToObject(root, "asset_id", mesh->contract.asset_id);
    cJSON_AddStringToObject(root, "source_asset_id", mesh->contract.source_asset_id);
    cJSON_AddStringToObject(root, "asset_type", core_mesh_asset_type_name(mesh->contract.asset_type));
    bounds = cJSON_CreateObject();
    if (!bounds) goto oom;
    cJSON_AddItemToObject(bounds, "min", ObjectAuthoringRuntimeMesh_Vec3Json(mesh->contract.local_bounds.min));
    cJSON_AddItemToObject(bounds, "max", ObjectAuthoringRuntimeMesh_Vec3Json(mesh->contract.local_bounds.max));
    cJSON_AddItemToObject(root, "local_bounds", bounds);

    mesh_node = cJSON_CreateObject();
    vertices = cJSON_CreateArray();
    triangles = cJSON_CreateArray();
    if (!mesh_node || !vertices || !triangles) goto oom;
    cJSON_AddNumberToObject(mesh_node, "vertex_count", (double)mesh->vertexCount);
    cJSON_AddNumberToObject(mesh_node, "triangle_count", (double)mesh->triangleCount);
    for (size_t i = 0u; i < mesh->vertexCount; ++i) {
        cJSON_AddItemToArray(vertices,
                             ObjectAuthoringRuntimeMesh_Vec3Json(mesh->vertices[i].position));
    }
    for (size_t i = 0u; i < mesh->triangleCount; ++i) {
        const ObjectAuthoringRuntimeMeshTriangle* triangle = &mesh->triangles[i];
        cJSON* triangle_node = cJSON_CreateObject();
        if (!triangle_node) goto oom;
        cJSON_AddNumberToObject(triangle_node, "a", (double)triangle->a);
        cJSON_AddNumberToObject(triangle_node, "b", (double)triangle->b);
        cJSON_AddNumberToObject(triangle_node, "c", (double)triangle->c);
        cJSON_AddStringToObject(triangle_node, "surface_group_id", triangle->surfaceGroupId);
        cJSON_AddItemToArray(triangles, triangle_node);
    }
    cJSON_AddItemToObject(mesh_node, "vertices", vertices);
    cJSON_AddItemToObject(mesh_node, "triangles", triangles);
    cJSON_AddItemToObject(root, "mesh", mesh_node);

    groups = cJSON_CreateArray();
    if (!groups) goto oom;
    for (size_t i = 0u; i < mesh->surfaceGroupCount; ++i) {
        const ObjectAuthoringRuntimeMeshSurfaceGroup* group = &mesh->surfaceGroups[i];
        cJSON* group_node = cJSON_CreateObject();
        cJSON* span_node = cJSON_CreateObject();
        if (!group_node || !span_node) goto oom;
        cJSON_AddStringToObject(group_node, "group_id", group->groupId);
        cJSON_AddNumberToObject(span_node, "start", (double)group->triangleStart);
        cJSON_AddNumberToObject(span_node, "count", (double)group->triangleCount);
        cJSON_AddItemToObject(group_node, "triangle_span", span_node);
        cJSON_AddItemToArray(groups, group_node);
    }
    cJSON_AddItemToObject(root, "surface_groups", groups);

    text = ObjectAuthoringMesh_PrintJson(root);
    if (!text) goto oom;
    result = core_io_write_all(path, text, strlen(text));
    free(text);
    cJSON_Delete(root);
    if (result.code != CORE_OK) {
        ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, result.message);
        return false;
    }
    return true;

oom:
    cJSON_Delete(root);
    ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "out of memory writing runtime mesh");
    return false;
}

static bool ObjectAuthoringMesh_WriteVertex(ObjectAuthoringRuntimeMesh* runtime,
                                            size_t index,
                                            Vec3 position,
                                            CoreMeshAssetBounds3* bounds,
                                            bool* has_bounds) {
    CoreObjectVec3 core_position;
    if (!runtime || index >= runtime->vertexCount) return false;
    core_position = ObjectAuthoringMesh_CoreVec3(position);
    if (!ObjectAuthoringMesh_UpdateBounds(bounds, core_position, has_bounds)) return false;
    runtime->vertices[index].position = core_position;
    return true;
}

static void ObjectAuthoringMesh_SurfaceGroupId(ObjectAuthoringBodyId body_id,
                                               Object3DFaceKind face,
                                               char* out_id,
                                               size_t out_id_size) {
    const ObjectAuthoringFaceId face_id = ObjectAuthoringFaceId_FromPrimitive(body_id, face);
    if (!out_id || out_id_size == 0u) return;
    snprintf(out_id, out_id_size, "face_%u", face_id);
}

static bool ObjectAuthoringMesh_WriteSurfaceGroup(ObjectAuthoringRuntimeMesh* runtime,
                                                  size_t group_index,
                                                  ObjectAuthoringBodyId body_id,
                                                  Object3DFaceKind face,
                                                  size_t triangle_start,
                                                  size_t triangle_count,
                                                  char* out_group_id,
                                                  size_t out_group_id_size) {
    ObjectAuthoringRuntimeMeshSurfaceGroup* group = NULL;
    char group_id[64];
    if (!runtime || group_index >= runtime->surfaceGroupCount ||
        triangle_count == 0u || !out_group_id || out_group_id_size == 0u) {
        return false;
    }
    ObjectAuthoringMesh_SurfaceGroupId(body_id, face, group_id, sizeof(group_id));
    group = &runtime->surfaceGroups[group_index];
    snprintf(group->groupId, sizeof(group->groupId), "%s", group_id);
    group->triangleStart = triangle_start;
    group->triangleCount = triangle_count;
    snprintf(out_group_id, out_group_id_size, "%s", group_id);
    return true;
}

static bool ObjectAuthoringMesh_WriteTriangle(ObjectAuthoringRuntimeMesh* runtime,
                                              size_t triangle_index,
                                              size_t a,
                                              size_t b,
                                              size_t c,
                                              const char* group_id) {
    ObjectAuthoringRuntimeMeshTriangle* triangle = NULL;
    if (!runtime || triangle_index >= runtime->triangleCount ||
        !group_id || group_id[0] == '\0') {
        return false;
    }
    triangle = &runtime->triangles[triangle_index];
    triangle->a = a;
    triangle->b = b;
    triangle->c = c;
    snprintf(triangle->surfaceGroupId, sizeof(triangle->surfaceGroupId), "%s", group_id);
    return true;
}

static bool ObjectAuthoringMesh_WriteQuad(ObjectAuthoringRuntimeMesh* runtime,
                                          size_t group_index,
                                          size_t* triangle_index,
                                          ObjectAuthoringBodyId body_id,
                                          Object3DFaceKind face,
                                          size_t a,
                                          size_t b,
                                          size_t c,
                                          size_t d) {
    char group_id[64];
    if (!runtime || !triangle_index) return false;
    if (!ObjectAuthoringMesh_WriteSurfaceGroup(runtime,
                                               group_index,
                                               body_id,
                                               face,
                                               *triangle_index,
                                               2u,
                                               group_id,
                                               sizeof(group_id))) {
        return false;
    }
    if (!ObjectAuthoringMesh_WriteTriangle(runtime, *triangle_index, a, b, c, group_id)) {
        return false;
    }
    *triangle_index += 1u;
    if (!ObjectAuthoringMesh_WriteTriangle(runtime, *triangle_index, a, c, d, group_id)) {
        return false;
    }
    *triangle_index += 1u;
    return true;
}

static bool ObjectAuthoringMesh_WritePlaneBody(ObjectAuthoringRuntimeMesh* runtime,
                                               const ObjectAuthoringBody* body,
                                               size_t* vertex_index,
                                               size_t* triangle_index,
                                               size_t* group_index,
                                               CoreMeshAssetBounds3* bounds,
                                               bool* has_bounds) {
    Object3D object = ObjectAuthoringMesh_ObjectFromBody(body);
    Vec3 corners[4];
    size_t base = 0u;
    if (!runtime || !body || !vertex_index || !triangle_index || !group_index) return false;
    if (!Layout_Object3D_ComputePlaneCorners(&object, corners)) return false;
    base = *vertex_index;
    for (size_t i = 0u; i < 4u; ++i) {
        if (!ObjectAuthoringMesh_WriteVertex(runtime, base + i, corners[i], bounds, has_bounds)) {
            return false;
        }
    }
    if (!ObjectAuthoringMesh_WriteQuad(runtime,
                                       *group_index,
                                       triangle_index,
                                       body->bodyId,
                                       OBJECT3D_FACE_PLANE_SURFACE,
                                       base + 0u,
                                       base + 1u,
                                       base + 2u,
                                       base + 3u)) {
        return false;
    }
    *vertex_index += 4u;
    *group_index += 1u;
    return true;
}

static bool ObjectAuthoringMesh_WriteRectPrismBody(ObjectAuthoringRuntimeMesh* runtime,
                                                   const ObjectAuthoringBody* body,
                                                   size_t* vertex_index,
                                                   size_t* triangle_index,
                                                   size_t* group_index,
                                                   CoreMeshAssetBounds3* bounds,
                                                   bool* has_bounds) {
    static const struct {
        Object3DFaceKind face;
        size_t corners[4];
    } kFaces[6] = {
        { OBJECT3D_FACE_RECT_PRISM_NEG_N, { 0u, 1u, 2u, 3u } },
        { OBJECT3D_FACE_RECT_PRISM_POS_N, { 4u, 5u, 6u, 7u } },
        { OBJECT3D_FACE_RECT_PRISM_NEG_V, { 0u, 1u, 5u, 4u } },
        { OBJECT3D_FACE_RECT_PRISM_POS_V, { 3u, 2u, 6u, 7u } },
        { OBJECT3D_FACE_RECT_PRISM_NEG_U, { 0u, 4u, 7u, 3u } },
        { OBJECT3D_FACE_RECT_PRISM_POS_U, { 1u, 2u, 6u, 5u } }
    };
    Object3D object = ObjectAuthoringMesh_ObjectFromBody(body);
    Vec3 corners[8];
    size_t base = 0u;
    if (!runtime || !body || !vertex_index || !triangle_index || !group_index) return false;
    if (!Layout_Object3D_ComputeRectPrismCorners(&object, corners)) return false;
    base = *vertex_index;
    for (size_t i = 0u; i < 8u; ++i) {
        if (!ObjectAuthoringMesh_WriteVertex(runtime, base + i, corners[i], bounds, has_bounds)) {
            return false;
        }
    }
    for (size_t i = 0u; i < 6u; ++i) {
        if (!ObjectAuthoringMesh_WriteQuad(runtime,
                                           *group_index,
                                           triangle_index,
                                           body->bodyId,
                                           kFaces[i].face,
                                           base + kFaces[i].corners[0],
                                           base + kFaces[i].corners[1],
                                           base + kFaces[i].corners[2],
                                           base + kFaces[i].corners[3])) {
            return false;
        }
        *group_index += 1u;
    }
    *vertex_index += 8u;
    return true;
}

static bool ObjectAuthoringMesh_WriteBody(ObjectAuthoringRuntimeMesh* runtime,
                                          const ObjectAuthoringBody* body,
                                          size_t* vertex_index,
                                          size_t* triangle_index,
                                          size_t* group_index,
                                          CoreMeshAssetBounds3* bounds,
                                          bool* has_bounds) {
    if (!body) return false;
    switch (body->sourceKind) {
        case OBJECT3D_KIND_PLANE:
            return ObjectAuthoringMesh_WritePlaneBody(runtime,
                                                      body,
                                                      vertex_index,
                                                      triangle_index,
                                                      group_index,
                                                      bounds,
                                                      has_bounds);
        case OBJECT3D_KIND_RECT_PRISM:
            return ObjectAuthoringMesh_WriteRectPrismBody(runtime,
                                                          body,
                                                          vertex_index,
                                                          triangle_index,
                                                          group_index,
                                                          bounds,
                                                          has_bounds);
        case OBJECT3D_KIND_UNKNOWN:
        default:
            return false;
    }
}

bool ObjectAuthoring_CompileRuntimeMesh(const ObjectAuthoringDocument* source,
                                        const char* runtime_asset_id,
                                        const char* source_asset_id,
                                        ObjectAuthoringRuntimeMesh* out_runtime,
                                        char* diagnostics,
                                        size_t diagnostics_size) {
    ObjectAuthoringDocument evaluated;
    ObjectAuthoringEvalDiagnostics eval_diagnostics;
    ObjectAuthoringMeshCounts counts = {0};
    ObjectAuthoringRuntimeMesh runtime;
    CoreResult result;
    CoreMeshAssetBounds3 bounds = {0};
    bool has_bounds = false;
    size_t vertex_index = 0u;
    size_t triangle_index = 0u;
    size_t group_index = 0u;

    ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "");
    if (!source || !runtime_asset_id || runtime_asset_id[0] == '\0' ||
        !source_asset_id || source_asset_id[0] == '\0' || !out_runtime) {
        ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "invalid runtime compile arguments");
        return false;
    }

    ObjectAuthoringDocument_Init(&evaluated);
    if (!ObjectAuthoring_EvaluateDocument(source, &evaluated, &eval_diagnostics)) {
        ObjectAuthoringMesh_SetDiag(diagnostics,
                                    diagnostics_size,
                                    eval_diagnostics.message[0]
                                        ? eval_diagnostics.message
                                        : "failed to evaluate object authoring document");
        ObjectAuthoringDocument_Free(&evaluated);
        return false;
    }
    if (evaluated.bodyCount == 0u) {
        ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "evaluated document has no bodies");
        ObjectAuthoringDocument_Free(&evaluated);
        return false;
    }
    for (size_t i = 0u; i < evaluated.bodyCount; ++i) {
        if (!ObjectAuthoringMesh_CountBody(&evaluated.bodies[i], &counts)) {
            ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "unsupported body in runtime compile");
            ObjectAuthoringDocument_Free(&evaluated);
            return false;
        }
    }

    ObjectAuthoringRuntimeMesh_Init(&runtime);
    result = core_mesh_asset_runtime_contract_set_asset_id(&runtime.contract, runtime_asset_id);
    if (result.code == CORE_OK) {
        result = core_mesh_asset_runtime_contract_set_source_asset_id(&runtime.contract,
                                                                      source_asset_id);
    }
    if (result.code == CORE_OK) {
        result = ObjectAuthoringRuntimeMesh_SetVertexCount(&runtime, counts.vertex_count)
                     ? core_result_ok()
                     : (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    if (result.code == CORE_OK) {
        result = ObjectAuthoringRuntimeMesh_SetTriangleCount(&runtime, counts.triangle_count)
                     ? core_result_ok()
                     : (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    if (result.code == CORE_OK) {
        result = ObjectAuthoringRuntimeMesh_SetSurfaceGroupCount(&runtime,
                                                                 counts.surface_group_count)
                     ? core_result_ok()
                     : (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    if (result.code != CORE_OK) {
        ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, result.message);
        ObjectAuthoringRuntimeMesh_Free(&runtime);
        ObjectAuthoringDocument_Free(&evaluated);
        return false;
    }

    runtime.contract.asset_type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
    runtime.contract.topology_manifold_expected = true;
    runtime.contract.topology_closed_volume = true;
    for (size_t i = 0u; i < evaluated.bodyCount; ++i) {
        const ObjectAuthoringBody* body = &evaluated.bodies[i];
        if (body->sourceKind == OBJECT3D_KIND_PLANE) {
            runtime.contract.topology_closed_volume = false;
        }
        if (!ObjectAuthoringMesh_WriteBody(&runtime,
                                           body,
                                           &vertex_index,
                                           &triangle_index,
                                           &group_index,
                                           &bounds,
                                           &has_bounds)) {
            ObjectAuthoringMesh_SetDiag(diagnostics, diagnostics_size, "failed to emit runtime mesh body");
            ObjectAuthoringRuntimeMesh_Free(&runtime);
            ObjectAuthoringDocument_Free(&evaluated);
            return false;
        }
    }
    runtime.contract.local_bounds = bounds;

    if (!ObjectAuthoringRuntimeMesh_Validate(&runtime, diagnostics, diagnostics_size)) {
        ObjectAuthoringRuntimeMesh_Free(&runtime);
        ObjectAuthoringDocument_Free(&evaluated);
        return false;
    }

    ObjectAuthoringRuntimeMesh_Free(out_runtime);
    *out_runtime = runtime;
    ObjectAuthoringDocument_Free(&evaluated);
    return true;
}
