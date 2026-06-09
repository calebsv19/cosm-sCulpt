#include "Layout/layout.h"

#include "Core/global_state.h"
#include "Layout/scene/layout_mesh_preview_sidecar.h"
#include "core_io.h"

#include "cjson/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LD_MESH_INSTANCE_RUNTIME_PARSE_LIMIT_BYTES (8u * 1024u * 1024u)

static void MeshInstance_SetDiagnostics(char* diagnostics,
                                        size_t diagnostics_size,
                                        const char* message) {
    if (!diagnostics || diagnostics_size == 0u) return;
    snprintf(diagnostics, diagnostics_size, "%s", message ? message : "");
}

static bool MeshInstance_Vec3FromJson(const cJSON* node, Vec3* out) {
    const cJSON* x = NULL;
    const cJSON* y = NULL;
    const cJSON* z = NULL;
    if (!cJSON_IsObject(node) || !out) return false;
    x = cJSON_GetObjectItemCaseSensitive(node, "x");
    y = cJSON_GetObjectItemCaseSensitive(node, "y");
    z = cJSON_GetObjectItemCaseSensitive(node, "z");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(z)) return false;
    out->x = (float)x->valuedouble;
    out->y = (float)y->valuedouble;
    out->z = (float)z->valuedouble;
    return true;
}

static bool MeshInstance_ReadRuntimeSidecar(const char* path,
                                            MeshAssetInstance3D* out_instance,
                                            char* diagnostics,
                                            size_t diagnostics_size) {
    CoreBuffer buffer = {0};
    CoreResult read_result = {0};
    char* text = NULL;
    cJSON* root = NULL;
    bool ok = false;

    if (!path || !path[0] || !out_instance) {
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "missing runtime mesh path");
        return false;
    }
    if (Layout_MeshPreviewSidecarReadInstance(path,
                                              out_instance,
                                              diagnostics,
                                              diagnostics_size)) {
        return true;
    }
    {
        struct stat st;
        if (stat(path, &st) == 0 &&
            st.st_size > (off_t)LD_MESH_INSTANCE_RUNTIME_PARSE_LIMIT_BYTES) {
            MeshInstance_SetDiagnostics(diagnostics,
                                        diagnostics_size,
                                        "runtime mesh is too large to inspect without preview sidecar");
            return false;
        }
    }

    read_result = core_io_read_all(path, &buffer);
    if (read_result.code != CORE_OK || !buffer.data || buffer.size == 0u) {
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "failed to read runtime mesh sidecar");
        return false;
    }

    text = (char*)malloc(buffer.size + 1u);
    if (!text) {
        core_io_buffer_free(&buffer);
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "failed to allocate runtime mesh buffer");
        return false;
    }
    memcpy(text, buffer.data, buffer.size);
    text[buffer.size] = '\0';
    core_io_buffer_free(&buffer);

    root = cJSON_Parse(text);
    free(text);
    if (!cJSON_IsObject(root)) {
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "failed to parse runtime mesh sidecar");
        cJSON_Delete(root);
        return false;
    }

    const cJSON* schema_variant = cJSON_GetObjectItemCaseSensitive(root, "schema_variant");
    const cJSON* asset_id = cJSON_GetObjectItemCaseSensitive(root, "asset_id");
    const cJSON* source_asset_id = cJSON_GetObjectItemCaseSensitive(root, "source_asset_id");
    const cJSON* vertex_count = cJSON_GetObjectItemCaseSensitive(root, "vertex_count");
    const cJSON* triangle_count = cJSON_GetObjectItemCaseSensitive(root, "triangle_count");
    const cJSON* mesh = cJSON_GetObjectItemCaseSensitive(root, "mesh");
    const cJSON* bounds = cJSON_GetObjectItemCaseSensitive(root, "local_bounds");
    Vec3 min_bounds = {0};
    Vec3 max_bounds = {0};

    if (!cJSON_IsString(schema_variant) ||
        strcmp(schema_variant->valuestring, "mesh_asset_runtime_v1") != 0) {
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "sidecar is not mesh_asset_runtime_v1");
        goto cleanup;
    }
    if (!cJSON_IsString(asset_id) || !asset_id->valuestring || !asset_id->valuestring[0]) {
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "runtime mesh sidecar is missing asset_id");
        goto cleanup;
    }
    if ((!cJSON_IsNumber(vertex_count) || !cJSON_IsNumber(triangle_count)) &&
        cJSON_IsObject(mesh)) {
        vertex_count = cJSON_GetObjectItemCaseSensitive(mesh, "vertex_count");
        triangle_count = cJSON_GetObjectItemCaseSensitive(mesh, "triangle_count");
    }
    if (!cJSON_IsNumber(vertex_count) || vertex_count->valuedouble <= 0.0 ||
        !cJSON_IsNumber(triangle_count) || triangle_count->valuedouble <= 0.0) {
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "runtime mesh sidecar has invalid counts");
        goto cleanup;
    }
    if (!cJSON_IsObject(bounds) ||
        !MeshInstance_Vec3FromJson(cJSON_GetObjectItemCaseSensitive(bounds, "min"), &min_bounds) ||
        !MeshInstance_Vec3FromJson(cJSON_GetObjectItemCaseSensitive(bounds, "max"), &max_bounds) ||
        min_bounds.x > max_bounds.x ||
        min_bounds.y > max_bounds.y ||
        min_bounds.z > max_bounds.z) {
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "runtime mesh sidecar has invalid bounds");
        goto cleanup;
    }

    memset(out_instance, 0, sizeof(*out_instance));
    snprintf(out_instance->assetId, sizeof(out_instance->assetId), "%s", asset_id->valuestring);
    if (cJSON_IsString(source_asset_id) && source_asset_id->valuestring) {
        snprintf(out_instance->sourceAssetId,
                 sizeof(out_instance->sourceAssetId),
                 "%s",
                 source_asset_id->valuestring);
    }
    snprintf(out_instance->runtimePath, sizeof(out_instance->runtimePath), "%s", path);
    out_instance->localBoundsMin = min_bounds;
    out_instance->localBoundsMax = max_bounds;
    out_instance->vertexCount = (size_t)vertex_count->valuedouble;
    out_instance->triangleCount = (size_t)triangle_count->valuedouble;
    out_instance->lockToBounds = true;
    ok = true;
    MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, NULL);

cleanup:
    cJSON_Delete(root);
    return ok;
}

static bool MeshInstance_PayloadEquals(const MeshAssetInstance3D* a,
                                       const MeshAssetInstance3D* b) {
    if (!a || !b) return false;
    return strcmp(a->assetId, b->assetId) == 0 &&
           strcmp(a->sourceAssetId, b->sourceAssetId) == 0 &&
           strcmp(a->runtimePath, b->runtimePath) == 0 &&
           a->localBoundsMin.x == b->localBoundsMin.x &&
           a->localBoundsMin.y == b->localBoundsMin.y &&
           a->localBoundsMin.z == b->localBoundsMin.z &&
           a->localBoundsMax.x == b->localBoundsMax.x &&
           a->localBoundsMax.y == b->localBoundsMax.y &&
           a->localBoundsMax.z == b->localBoundsMax.z &&
           a->vertexCount == b->vertexCount &&
           a->triangleCount == b->triangleCount &&
           a->lockToBounds == b->lockToBounds;
}

bool Layout_CreateMeshAssetInstanceFromRuntimeAsset(Layout* layout,
                                                    const char* runtimeAssetPath,
                                                    const Transform3D* transform,
                                                    uint32_t* outObjectId,
                                                    char* diagnostics,
                                                    size_t diagnostics_size) {
    MeshAssetInstance3D mesh = {0};
    Transform3D resolved_transform = Layout_Transform3D_Default();

    if (outObjectId) *outObjectId = 0u;
    MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, NULL);
    if (!layout || !runtimeAssetPath || !runtimeAssetPath[0]) {
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "missing layout or runtime mesh path");
        return false;
    }
    if (!MeshInstance_ReadRuntimeSidecar(runtimeAssetPath,
                                         &mesh,
                                         diagnostics,
                                         diagnostics_size)) {
        return false;
    }
    if (transform) resolved_transform = *transform;
    if (!Layout_SceneBounds3D_ClampPoint(&layout->scene3d.bounds,
                                         &resolved_transform.position,
                                         NULL)) {
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "mesh instance placement is outside invalid bounds");
        return false;
    }

    const uint32_t object_id = Layout_ObjectStore_Create(&layout->objectStore,
                                                        OBJECT3D_KIND_MESH_ASSET_INSTANCE,
                                                        &resolved_transform,
                                                        "mesh_asset_instance",
                                                        CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                                        CORE_OBJECT_PLANE_XY);
    if (object_id == 0u) {
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "failed to create mesh asset instance");
        return false;
    }

    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, object_id);
    if (!object) return false;
    object->meshInstance = mesh;

    if (!Layout_ObjectStore_ValidateObject(object)) {
        (void)Layout_ObjectStore_Delete(&layout->objectStore, object_id);
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "mesh asset instance failed validation");
        return false;
    }

    if (outObjectId) *outObjectId = object_id;
    MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, NULL);
    return true;
}

bool Layout_RefreshMeshAssetInstanceFromRuntimeAsset(Layout* layout,
                                                     uint32_t objectId,
                                                     bool* outChanged,
                                                     char* diagnostics,
                                                     size_t diagnostics_size) {
    Object3D* object = NULL;
    MeshAssetInstance3D refreshed = {0};
    MeshAssetInstance3D previous = {0};
    const char* runtime_path = NULL;

    if (outChanged) *outChanged = false;
    MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, NULL);
    if (!layout || objectId == 0u) {
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "missing layout or mesh instance object id");
        return false;
    }

    object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    if (!object || object->kind != OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "object is not a mesh asset instance");
        return false;
    }

    runtime_path = object->meshInstance.runtimePath;
    if (!MeshInstance_ReadRuntimeSidecar(runtime_path,
                                         &refreshed,
                                         diagnostics,
                                         diagnostics_size)) {
        return false;
    }

    refreshed.lockToBounds = object->meshInstance.lockToBounds;
    if (MeshInstance_PayloadEquals(&object->meshInstance, &refreshed)) {
        return true;
    }

    previous = object->meshInstance;
    object->meshInstance = refreshed;
    if (!Layout_ObjectStore_ValidateObject(object)) {
        object->meshInstance = previous;
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "refreshed mesh asset instance failed validation");
        return false;
    }

    if (outChanged) *outChanged = true;
    Global_FlagLayoutChanged();
    MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, NULL);
    return true;
}

bool Layout_RefreshMeshAssetInstancesFromRuntimeAsset(Layout* layout,
                                                      const char* runtimeAssetPath,
                                                      size_t* outRefreshedCount,
                                                      size_t* outChangedCount,
                                                      char* diagnostics,
                                                      size_t diagnostics_size) {
    MeshAssetInstance3D refreshed = {0};
    size_t refreshed_count = 0u;
    size_t changed_count = 0u;
    bool any_invalid = false;

    if (outRefreshedCount) *outRefreshedCount = 0u;
    if (outChangedCount) *outChangedCount = 0u;
    MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, NULL);
    if (!layout || !runtimeAssetPath || !runtimeAssetPath[0]) {
        MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, "missing layout or runtime mesh path");
        return false;
    }
    if (!MeshInstance_ReadRuntimeSidecar(runtimeAssetPath,
                                         &refreshed,
                                         diagnostics,
                                         diagnostics_size)) {
        return false;
    }

    for (size_t i = 0u; i < layout->objectStore.count; ++i) {
        Object3D* object = &layout->objectStore.items[i];
        MeshAssetInstance3D updated = refreshed;
        MeshAssetInstance3D previous = {0};
        if (object->isDeleted ||
            object->kind != OBJECT3D_KIND_MESH_ASSET_INSTANCE ||
            strcmp(object->meshInstance.runtimePath, runtimeAssetPath) != 0) {
            continue;
        }

        ++refreshed_count;
        updated.lockToBounds = object->meshInstance.lockToBounds;
        if (MeshInstance_PayloadEquals(&object->meshInstance, &updated)) {
            continue;
        }

        previous = object->meshInstance;
        object->meshInstance = updated;
        if (!Layout_ObjectStore_ValidateObject(object)) {
            object->meshInstance = previous;
            any_invalid = true;
            continue;
        }
        ++changed_count;
    }

    if (outRefreshedCount) *outRefreshedCount = refreshed_count;
    if (outChangedCount) *outChangedCount = changed_count;
    if (changed_count > 0u) {
        Global_FlagLayoutChanged();
    }
    if (any_invalid) {
        MeshInstance_SetDiagnostics(diagnostics,
                                    diagnostics_size,
                                    "one or more mesh asset instances failed validation");
        return false;
    }
    MeshInstance_SetDiagnostics(diagnostics, diagnostics_size, NULL);
    return true;
}
