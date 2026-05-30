#include "Layout/asset/layout_object_asset_mesh_authoring.h"

#include "core_mesh_asset.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void LayoutObjectAsset_SetDiagnostics(char* diagnostics,
                                             size_t diagnostics_size,
                                             const char* message) {
    if (!diagnostics || diagnostics_size == 0u) {
        return;
    }
    if (!message) {
        diagnostics[0] = '\0';
        return;
    }
    snprintf(diagnostics, diagnostics_size, "%s", message);
}

static const char* LayoutObjectAsset_DefaultObjectType(Object3DKind kind) {
    switch (kind) {
        case OBJECT3D_KIND_PLANE:
            return "plane_primitive";
        case OBJECT3D_KIND_RECT_PRISM:
            return "rect_prism_primitive";
        default:
            return "unknown_primitive";
    }
}

static void LayoutObjectAsset_CoreVec3_FromVec3(Vec3 src, CoreObjectVec3* dst) {
    if (!dst) {
        return;
    }
    dst->x = (double)src.x;
    dst->y = (double)src.y;
    dst->z = (double)src.z;
}

static Vec3 LayoutObjectAsset_Vec3_FromCore(CoreObjectVec3 src) {
    Vec3 v = {0};
    v.x = (float)src.x;
    v.y = (float)src.y;
    v.z = (float)src.z;
    return v;
}

static void LayoutObjectAsset_CoreTransform_FromTransform(const Transform3D* src,
                                                          CoreObjectTransform* dst) {
    if (!src || !dst) {
        return;
    }
    LayoutObjectAsset_CoreVec3_FromVec3(src->position, &dst->position);
    LayoutObjectAsset_CoreVec3_FromVec3(src->rotationDeg, &dst->rotation_deg);
    LayoutObjectAsset_CoreVec3_FromVec3(src->scale, &dst->scale);
}

static Transform3D LayoutObjectAsset_Transform_FromCore(const CoreObjectTransform* src) {
    Transform3D t = Layout_Transform3D_Default();
    if (!src) {
        return t;
    }
    t.position = LayoutObjectAsset_Vec3_FromCore(src->position);
    t.rotationDeg = LayoutObjectAsset_Vec3_FromCore(src->rotation_deg);
    t.scale = LayoutObjectAsset_Vec3_FromCore(src->scale);
    return t;
}

static void LayoutObjectAsset_CoreFrame_FromPlaneFrame(const PlaneFrame3* src,
                                                       CoreMeshAssetFrame3* dst) {
    if (!src || !dst) {
        return;
    }
    LayoutObjectAsset_CoreVec3_FromVec3(src->origin, &dst->origin);
    LayoutObjectAsset_CoreVec3_FromVec3(src->axisU, &dst->axis_u);
    LayoutObjectAsset_CoreVec3_FromVec3(src->axisV, &dst->axis_v);
    LayoutObjectAsset_CoreVec3_FromVec3(src->normal, &dst->normal);
}

static PlaneFrame3 LayoutObjectAsset_PlaneFrame_FromCore(const CoreMeshAssetFrame3* src) {
    PlaneFrame3 frame;
    memset(&frame, 0, sizeof(frame));
    if (!src) {
        return frame;
    }
    frame.origin = LayoutObjectAsset_Vec3_FromCore(src->origin);
    frame.axisU = LayoutObjectAsset_Vec3_FromCore(src->axis_u);
    frame.axisV = LayoutObjectAsset_Vec3_FromCore(src->axis_v);
    frame.normal = LayoutObjectAsset_Vec3_FromCore(src->normal);
    return frame;
}

static Object3DKind LayoutObjectAsset_ObjectKindFromPrimitiveKind(
    CoreMeshAssetPrimitiveSeedKind kind) {
    switch (kind) {
        case CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE:
            return OBJECT3D_KIND_PLANE;
        case CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM:
            return OBJECT3D_KIND_RECT_PRISM;
        default:
            return OBJECT3D_KIND_UNKNOWN;
    }
}

static CoreMeshAssetPrimitiveSeedKind LayoutObjectAsset_PrimitiveKindFromObjectKind(
    Object3DKind kind) {
    switch (kind) {
        case OBJECT3D_KIND_PLANE:
            return CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE;
        case OBJECT3D_KIND_RECT_PRISM:
            return CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM;
        default:
            return CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_UNKNOWN;
    }
}

static void LayoutObjectAsset_BuildAssetIdFromPath(const char* path,
                                                   char* out_asset_id,
                                                   size_t out_asset_id_size) {
    const char* base = NULL;
    size_t len = 0u;
    if (!out_asset_id || out_asset_id_size == 0u) {
        return;
    }
    out_asset_id[0] = '\0';
    if (!path || !path[0]) {
        snprintf(out_asset_id, out_asset_id_size, "object_asset");
        return;
    }
    base = strrchr(path, '/');
    base = base ? (base + 1) : path;
    len = strlen(base);
    if (len > 5u && strcmp(base + len - 5u, ".json") == 0) {
        len -= 5u;
    }
    if (len == 0u) {
        snprintf(out_asset_id, out_asset_id_size, "object_asset");
        return;
    }
    if (len >= out_asset_id_size) {
        len = out_asset_id_size - 1u;
    }
    memcpy(out_asset_id, base, len);
    out_asset_id[len] = '\0';
}

static void LayoutObjectAsset_ComputePivot(const Layout* layout, CoreMeshAssetFrame3* out_pivot) {
    bool has_bounds = false;
    Vec3 min_point = {0};
    Vec3 max_point = {0};
    if (!out_pivot) {
        return;
    }
    memset(out_pivot, 0, sizeof(*out_pivot));
    out_pivot->axis_u.x = 1.0;
    out_pivot->axis_v.y = 1.0;
    out_pivot->normal.z = 1.0;
    if (!layout) {
        return;
    }
    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        Vec3 object_min = {0};
        Vec3 object_max = {0};
        const Object3D* object = &layout->objectStore.items[i];
        if (object->isDeleted || !Layout_Object3D_ComputeWorldAABB(object, &object_min, &object_max)) {
            continue;
        }
        if (!has_bounds) {
            min_point = object_min;
            max_point = object_max;
            has_bounds = true;
        } else {
            min_point.x = fminf(min_point.x, object_min.x);
            min_point.y = fminf(min_point.y, object_min.y);
            min_point.z = fminf(min_point.z, object_min.z);
            max_point.x = fmaxf(max_point.x, object_max.x);
            max_point.y = fmaxf(max_point.y, object_max.y);
            max_point.z = fmaxf(max_point.z, object_max.z);
        }
    }
    if (has_bounds) {
        Vec3 center = {
            (min_point.x + max_point.x) * 0.5f,
            (min_point.y + max_point.y) * 0.5f,
            (min_point.z + max_point.z) * 0.5f
        };
        LayoutObjectAsset_CoreVec3_FromVec3(center, &out_pivot->origin);
    }
}

bool LayoutObjectAssetMeshAuthoring_Save(const Layout* layout,
                                         const char* path,
                                         char* diagnostics,
                                         size_t diagnostics_size) {
    CoreMeshAssetAuthoringDocument document;
    CoreResult r;
    size_t live_index = 0u;
    char asset_id[64];

    LayoutObjectAsset_SetDiagnostics(diagnostics, diagnostics_size, "");
    if (!layout || !path || path[0] == '\0') {
        LayoutObjectAsset_SetDiagnostics(diagnostics, diagnostics_size, "invalid save arguments");
        return false;
    }

    core_mesh_asset_authoring_document_init(&document);
    LayoutObjectAsset_BuildAssetIdFromPath(path, asset_id, sizeof(asset_id));
    r = core_mesh_asset_authoring_contract_set_asset_id(&document.contract, asset_id);
    if (r.code != CORE_OK) {
        LayoutObjectAsset_SetDiagnostics(diagnostics, diagnostics_size, r.message);
        return false;
    }
    document.contract.unit_kind = CORE_UNIT_METER;
    document.contract.world_scale = 1.0;
    document.contract.asset_type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
    document.contract.source_mode = CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED;
    LayoutObjectAsset_ComputePivot(layout, &document.contract.pivot);
    r = core_mesh_asset_authoring_document_set_primitive_seed_count(
        &document,
        Layout_ObjectStore_LiveCount(&layout->objectStore));
    if (r.code != CORE_OK) {
        LayoutObjectAsset_SetDiagnostics(diagnostics, diagnostics_size, r.message);
        core_mesh_asset_authoring_document_free(&document);
        return false;
    }

    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        CoreMeshAssetPrimitiveSeed* seed = NULL;
        const char* object_type = NULL;
        if (object->isDeleted) {
            continue;
        }
        if (!Layout_ObjectStore_ValidateObject(object)) {
            LayoutObjectAsset_SetDiagnostics(diagnostics,
                                             diagnostics_size,
                                             "layout contains an invalid authored object");
            core_mesh_asset_authoring_document_free(&document);
            return false;
        }
        seed = &document.primitive_seeds[live_index++];
        snprintf(seed->primitive_id, sizeof(seed->primitive_id), "primitive_%u", object->objectId);
        seed->kind = LayoutObjectAsset_PrimitiveKindFromObjectKind(object->kind);
        seed->object = object->coreMeta;
        if (seed->object.object_id[0] == '\0') {
            snprintf(seed->object.object_id, sizeof(seed->object.object_id), "%s", seed->primitive_id);
        }
        object_type = seed->object.object_type[0] ? seed->object.object_type
                                                   : LayoutObjectAsset_DefaultObjectType(object->kind);
        if (seed->object.object_type[0] == '\0') {
            snprintf(seed->object.object_type,
                     sizeof(seed->object.object_type),
                     "%s",
                     object_type);
        }
        LayoutObjectAsset_CoreTransform_FromTransform(&object->transform, &seed->object.transform);
        if (object->kind == OBJECT3D_KIND_PLANE) {
            seed->plane.width = (double)object->plane.width;
            seed->plane.height = (double)object->plane.height;
            seed->plane.lock_to_construction_plane = object->plane.lockToConstructionPlane;
            seed->plane.lock_to_bounds = object->plane.lockToBounds;
            LayoutObjectAsset_CoreFrame_FromPlaneFrame(&object->plane.frame, &seed->plane.frame);
        } else if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
            seed->rect_prism.width = (double)object->rectPrism.width;
            seed->rect_prism.height = (double)object->rectPrism.height;
            seed->rect_prism.depth = (double)object->rectPrism.depth;
            seed->rect_prism.lock_to_construction_plane =
                object->rectPrism.lockToConstructionPlane;
            seed->rect_prism.lock_to_bounds = object->rectPrism.lockToBounds;
            LayoutObjectAsset_CoreFrame_FromPlaneFrame(&object->rectPrism.frame,
                                                       &seed->rect_prism.frame);
        } else {
            LayoutObjectAsset_SetDiagnostics(diagnostics,
                                             diagnostics_size,
                                             "unsupported object kind in object asset save");
            core_mesh_asset_authoring_document_free(&document);
            return false;
        }
    }

    r = core_mesh_asset_authoring_document_save_file(&document, path);
    if (r.code != CORE_OK) {
        LayoutObjectAsset_SetDiagnostics(diagnostics, diagnostics_size, r.message);
        core_mesh_asset_authoring_document_free(&document);
        return false;
    }
    core_mesh_asset_authoring_document_free(&document);
    return true;
}

bool LayoutObjectAssetMeshAuthoring_Load(Layout* layout,
                                         const char* path,
                                         char* diagnostics,
                                         size_t diagnostics_size) {
    CoreMeshAssetAuthoringDocument document;
    CoreResult r;
    Layout loaded_layout;
    float grid_size = 1.0f;

    LayoutObjectAsset_SetDiagnostics(diagnostics, diagnostics_size, "");
    if (!layout || !path || path[0] == '\0') {
        LayoutObjectAsset_SetDiagnostics(diagnostics, diagnostics_size, "invalid load arguments");
        return false;
    }

    core_mesh_asset_authoring_document_init(&document);
    r = core_mesh_asset_authoring_document_load_file(path, &document);
    if (r.code != CORE_OK) {
        LayoutObjectAsset_SetDiagnostics(diagnostics, diagnostics_size, r.message);
        return false;
    }

    grid_size = layout->gridSize;
    Layout_Init(&loaded_layout, grid_size);
    for (size_t i = 0; i < document.primitive_seed_count; ++i) {
        const CoreMeshAssetPrimitiveSeed* seed = &document.primitive_seeds[i];
        Transform3D transform = LayoutObjectAsset_Transform_FromCore(&seed->object.transform);
        const Object3DKind kind = LayoutObjectAsset_ObjectKindFromPrimitiveKind(seed->kind);
        const uint32_t object_id = Layout_ObjectStore_Create(&loaded_layout.objectStore,
                                                             kind,
                                                             &transform,
                                                             seed->object.object_type,
                                                             seed->object.dimensional_mode,
                                                             seed->object.locked_plane);
        Object3D* object = Layout_ObjectStore_Find(&loaded_layout.objectStore, object_id);
        if (kind == OBJECT3D_KIND_UNKNOWN || object_id == 0u || !object) {
            LayoutObjectAsset_SetDiagnostics(diagnostics,
                                             diagnostics_size,
                                             "failed to rebuild object asset primitive");
            Layout_Free(&loaded_layout);
            core_mesh_asset_authoring_document_free(&document);
            return false;
        }
        object->coreMeta = seed->object;
        object->transform = transform;
        if (kind == OBJECT3D_KIND_PLANE) {
            object->plane.width = (float)seed->plane.width;
            object->plane.height = (float)seed->plane.height;
            object->plane.frame = LayoutObjectAsset_PlaneFrame_FromCore(&seed->plane.frame);
            object->plane.lockToConstructionPlane = seed->plane.lock_to_construction_plane;
            object->plane.lockToBounds = seed->plane.lock_to_bounds;
        } else {
            object->rectPrism.width = (float)seed->rect_prism.width;
            object->rectPrism.height = (float)seed->rect_prism.height;
            object->rectPrism.depth = (float)seed->rect_prism.depth;
            object->rectPrism.frame = LayoutObjectAsset_PlaneFrame_FromCore(&seed->rect_prism.frame);
            object->rectPrism.lockToConstructionPlane =
                seed->rect_prism.lock_to_construction_plane;
            object->rectPrism.lockToBounds = seed->rect_prism.lock_to_bounds;
        }
        if (!Layout_ObjectStore_ValidateObject(object)) {
            LayoutObjectAsset_SetDiagnostics(diagnostics,
                                             diagnostics_size,
                                             "object asset document produced an invalid object");
            Layout_Free(&loaded_layout);
            core_mesh_asset_authoring_document_free(&document);
            return false;
        }
    }

    Layout_Free(layout);
    *layout = loaded_layout;
    core_mesh_asset_authoring_document_free(&document);
    return true;
}
