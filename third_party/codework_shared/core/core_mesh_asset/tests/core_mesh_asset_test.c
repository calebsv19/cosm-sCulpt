#include "core_mesh_asset.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CHECK(cond) do { if (!(cond)) { printf("fail:%d\n", __LINE__); return 1; } } while (0)

static int file_contains_all(const char *path, const char **needles, size_t needle_count) {
    FILE *fp;
    char buffer[8192];
    size_t nread;
    size_t i;
    fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    nread = fread(buffer, 1u, sizeof(buffer) - 1u, fp);
    fclose(fp);
    buffer[nread] = '\0';
    for (i = 0u; i < needle_count; ++i) {
        if (!strstr(buffer, needles[i])) {
            return 0;
        }
    }
    return 1;
}

static int test_parse_helpers(void) {
    CoreMeshAssetSchemaVariant variant = CORE_MESH_ASSET_SCHEMA_VARIANT_RUNTIME_V1;
    CoreMeshAssetType type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
    CoreMeshAssetPrimitiveSeedKind primitive_kind = CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM;
    CoreMeshAssetSourceMode mode = CORE_MESH_ASSET_SOURCE_MODE_REVOLVE;

    CHECK(core_mesh_asset_schema_variant_parse(NULL, &variant).code == CORE_ERR_INVALID_ARG);
    CHECK(variant == CORE_MESH_ASSET_SCHEMA_VARIANT_UNKNOWN);
    CHECK(core_mesh_asset_schema_variant_parse("mesh_asset_authoring_v1", &variant).code == CORE_OK);
    CHECK(variant == CORE_MESH_ASSET_SCHEMA_VARIANT_AUTHORING_V1);
    CHECK(strcmp(core_mesh_asset_schema_variant_name(CORE_MESH_ASSET_SCHEMA_VARIANT_RUNTIME_V1),
                 "mesh_asset_runtime_v1") == 0);

    CHECK(core_mesh_asset_type_parse(NULL, &type).code == CORE_ERR_INVALID_ARG);
    CHECK(type == CORE_MESH_ASSET_TYPE_UNKNOWN);
    CHECK(core_mesh_asset_type_parse("solid_mesh", &type).code == CORE_OK);
    CHECK(type == CORE_MESH_ASSET_TYPE_SOLID_MESH);

    CHECK(core_mesh_asset_primitive_seed_kind_parse(NULL, &primitive_kind).code ==
          CORE_ERR_INVALID_ARG);
    CHECK(primitive_kind == CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_UNKNOWN);
    CHECK(core_mesh_asset_primitive_seed_kind_parse("plane", &primitive_kind).code == CORE_OK);
    CHECK(primitive_kind == CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE);
    CHECK(strcmp(core_mesh_asset_primitive_seed_kind_name(
                     CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM),
                 "rect_prism") == 0);

    CHECK(core_mesh_asset_source_mode_parse(NULL, &mode).code == CORE_ERR_INVALID_ARG);
    CHECK(mode == CORE_MESH_ASSET_SOURCE_MODE_UNKNOWN);
    CHECK(core_mesh_asset_source_mode_parse("profile_extrusion", &mode).code == CORE_OK);
    CHECK(mode == CORE_MESH_ASSET_SOURCE_MODE_PROFILE_EXTRUSION);
    CHECK(strcmp(core_mesh_asset_source_mode_name(CORE_MESH_ASSET_SOURCE_MODE_REVOLVE),
                 "revolve") == 0);

    return 0;
}

static int test_authoring_contract_validation(void) {
    CoreMeshAssetAuthoringContract contract;
    core_mesh_asset_authoring_contract_init(&contract);

    CHECK(core_mesh_asset_authoring_contract_validate(NULL).code == CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_asset_authoring_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_asset_authoring_contract_set_asset_id(&contract, "asset_bookshelf_01").code == CORE_OK);
    CHECK(core_mesh_asset_authoring_contract_validate(&contract).code == CORE_OK);

    contract.source_mode = CORE_MESH_ASSET_SOURCE_MODE_UNKNOWN;
    CHECK(core_mesh_asset_authoring_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);
    contract.source_mode = CORE_MESH_ASSET_SOURCE_MODE_PROFILE_EXTRUSION;
    contract.world_scale = 0.0;
    CHECK(core_mesh_asset_authoring_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);
    contract.world_scale = 1.0;
    contract.pivot.axis_u.x = 0.0;
    CHECK(core_mesh_asset_authoring_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);
    contract.pivot.axis_u.x = 1.0;
    contract.pivot.normal.z = NAN;
    CHECK(core_mesh_asset_authoring_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);

    return 0;
}

static int test_authoring_document_validation(void) {
    CoreMeshAssetAuthoringDocument document;
    CoreMeshAssetPrimitiveSeed *plane = NULL;
    CoreMeshAssetPrimitiveSeed *rect_prism = NULL;

    core_mesh_asset_authoring_document_init(&document);
    CHECK(core_mesh_asset_authoring_document_validate(NULL).code == CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_asset_authoring_contract_set_asset_id(&document.contract, "asset_demo").code ==
          CORE_OK);
    document.contract.source_mode = CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED;
    CHECK(core_mesh_asset_authoring_document_validate(&document).code == CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_asset_authoring_document_set_primitive_seed_count(&document, 2u).code ==
          CORE_OK);

    plane = &document.primitive_seeds[0];
    rect_prism = &document.primitive_seeds[1];

    CHECK(core_object_set_identity(&plane->object, "primitive_1", "plane_primitive").code ==
          CORE_OK);
    CHECK(core_mesh_asset_authoring_contract_validate(&document.contract).code == CORE_OK);
    CHECK(core_mesh_asset_authoring_contract_validate(&document.contract).code == CORE_OK);
    plane->kind = CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE;
    strcpy(plane->primitive_id, "primitive_1");
    plane->object.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    plane->object.locked_plane = CORE_OBJECT_PLANE_XY;
    plane->object.transform.scale.x = 1.0;
    plane->object.transform.scale.y = 1.0;
    plane->object.transform.scale.z = 1.0;
    plane->plane.width = 2.0;
    plane->plane.height = 1.5;
    plane->plane.frame.origin.x = 0.0;
    plane->plane.frame.origin.y = 0.0;
    plane->plane.frame.origin.z = 0.0;
    plane->plane.frame.axis_u.x = 1.0;
    plane->plane.frame.axis_v.y = 1.0;
    plane->plane.frame.normal.z = 1.0;
    plane->object.transform.position = plane->plane.frame.origin;
    plane->object.flags.visible = true;
    plane->object.flags.selectable = true;

    CHECK(core_object_set_identity(&rect_prism->object,
                                   "primitive_2",
                                   "rect_prism_primitive").code == CORE_OK);
    rect_prism->kind = CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM;
    strcpy(rect_prism->primitive_id, "primitive_2");
    rect_prism->object.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
    rect_prism->object.locked_plane = CORE_OBJECT_PLANE_XY;
    rect_prism->object.transform.scale.x = 1.0;
    rect_prism->object.transform.scale.y = 1.0;
    rect_prism->object.transform.scale.z = 1.0;
    rect_prism->rect_prism.width = 1.0;
    rect_prism->rect_prism.height = 0.5;
    rect_prism->rect_prism.depth = 0.75;
    rect_prism->rect_prism.frame.origin.x = 0.5;
    rect_prism->rect_prism.frame.origin.y = 0.0;
    rect_prism->rect_prism.frame.origin.z = 0.25;
    rect_prism->rect_prism.frame.axis_u.x = 1.0;
    rect_prism->rect_prism.frame.axis_v.y = 1.0;
    rect_prism->rect_prism.frame.normal.z = 1.0;
    rect_prism->object.transform.position = rect_prism->rect_prism.frame.origin;
    rect_prism->object.flags.visible = true;
    rect_prism->object.flags.selectable = true;

    CHECK(core_mesh_asset_authoring_document_validate(&document).code == CORE_OK);
    rect_prism->rect_prism.depth = -1.0;
    CHECK(core_mesh_asset_authoring_document_validate(&document).code == CORE_ERR_INVALID_ARG);
    core_mesh_asset_authoring_document_free(&document);
    return 0;
}

static int test_runtime_contract_validation(void) {
    CoreMeshAssetRuntimeContract contract;
    core_mesh_asset_runtime_contract_init(&contract);

    CHECK(core_mesh_asset_runtime_contract_validate(NULL).code == CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_asset_runtime_contract_set_asset_id(&contract, "asset_bookshelf_01").code == CORE_OK);
    CHECK(core_mesh_asset_runtime_contract_set_source_asset_id(&contract, "asset_bookshelf_01").code == CORE_OK);
    contract.vertex_count = 48u;
    contract.triangle_count = 24u;
    contract.local_bounds.min.x = -1.0;
    contract.local_bounds.min.y = -0.5;
    contract.local_bounds.min.z = 0.0;
    contract.local_bounds.max.x = 1.0;
    contract.local_bounds.max.y = 0.5;
    contract.local_bounds.max.z = 2.0;
    CHECK(core_mesh_asset_runtime_contract_validate(&contract).code == CORE_OK);

    contract.triangle_count = 0u;
    CHECK(core_mesh_asset_runtime_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);
    contract.triangle_count = 24u;
    contract.local_bounds.min.z = 3.0;
    CHECK(core_mesh_asset_runtime_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);

    return 0;
}

static int test_fixture_tokens(void) {
    static const char *authoring_needles[] = {
        "\"schema_variant\"",
        "\"mesh_asset_authoring_v1\"",
        "\"source_mode\"",
        "\"primitive_seed\"",
        "\"primitive_seeds\""
    };
    static const char *runtime_needles[] = {
        "\"schema_variant\"",
        "\"mesh_asset_runtime_v1\"",
        "\"source_asset_id\"",
        "\"triangle_count\""
    };

    CHECK(file_contains_all("tests/fixtures/mesh_asset_authoring_v1_sample.json",
                            authoring_needles,
                            sizeof(authoring_needles) / sizeof(authoring_needles[0])));
    CHECK(file_contains_all("tests/fixtures/mesh_asset_runtime_v1_sample.json",
                            runtime_needles,
                            sizeof(runtime_needles) / sizeof(runtime_needles[0])));
    return 0;
}

static int test_authoring_document_file_roundtrip(void) {
    CoreMeshAssetAuthoringDocument document;
    CoreMeshAssetAuthoringDocument reloaded;
    const char *path = "/private/tmp/core_mesh_asset_authoring_roundtrip.json";
    CoreResult load_result;

    core_mesh_asset_authoring_document_init(&document);
    core_mesh_asset_authoring_document_init(&reloaded);

    load_result = core_mesh_asset_authoring_document_load_file(
        "tests/fixtures/mesh_asset_authoring_v1_sample.json",
        &document);
    if (load_result.code != CORE_OK) {
        printf("roundtrip-load-fail:%d:%s\n",
               load_result.code,
               load_result.message ? load_result.message : "null");
        return 1;
    }
    CHECK(document.contract.source_mode == CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED);
    CHECK(document.primitive_seed_count == 2u);
    CHECK(document.primitive_seeds[0].kind == CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE);
    CHECK(document.primitive_seeds[1].kind == CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM);
    CHECK(core_mesh_asset_authoring_document_save_file(&document, path).code == CORE_OK);
    CHECK(core_mesh_asset_authoring_document_load_file(path, &reloaded).code == CORE_OK);
    CHECK(strcmp(reloaded.contract.asset_id, "asset_bookshelf_01") == 0);
    CHECK(reloaded.primitive_seed_count == 2u);
    CHECK(strcmp(reloaded.primitive_seeds[1].object.object_type, "rect_prism_primitive") == 0);

    core_mesh_asset_authoring_document_free(&document);
    core_mesh_asset_authoring_document_free(&reloaded);
    remove(path);
    return 0;
}

int main(void) {
    if (test_parse_helpers() != 0) return 1;
    if (test_authoring_contract_validation() != 0) return 1;
    if (test_authoring_document_validation() != 0) return 1;
    if (test_runtime_contract_validation() != 0) return 1;
    if (test_fixture_tokens() != 0) return 1;
    if (test_authoring_document_file_roundtrip() != 0) return 1;
    return 0;
}
