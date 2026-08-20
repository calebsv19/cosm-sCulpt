#include "test_layout_internal.h"

static cJSON* ld_test_find_path_by_id(cJSON* paths, const char* path_id) {
    int count = 0;
    int i = 0;
    if (!cJSON_IsArray(paths) || !path_id) return NULL;
    count = cJSON_GetArraySize(paths);
    for (i = 0; i < count; ++i) {
        cJSON* path = cJSON_GetArrayItem(paths, i);
        cJSON* id = cJSON_GetObjectItem(path, "path_id");
        if (cJSON_IsString(id) && id->valuestring && strcmp(id->valuestring, path_id) == 0) {
            return path;
        }
    }
    return NULL;
}

static cJSON* ld_test_find_camera_by_id(cJSON* cameras, const char* camera_id) {
    int count = 0;
    int i = 0;
    if (!cJSON_IsArray(cameras) || !camera_id) return NULL;
    count = cJSON_GetArraySize(cameras);
    for (i = 0; i < count; ++i) {
        cJSON* camera = cJSON_GetArrayItem(cameras, i);
        cJSON* id = cJSON_GetObjectItem(camera, "camera_id");
        if (cJSON_IsString(id) && id->valuestring && strcmp(id->valuestring, camera_id) == 0) {
            return camera;
        }
    }
    return NULL;
}

static cJSON* ld_test_find_light_by_id(cJSON* lights, const char* light_id) {
    int count = 0;
    int i = 0;
    if (!cJSON_IsArray(lights) || !light_id) return NULL;
    count = cJSON_GetArraySize(lights);
    for (i = 0; i < count; ++i) {
        cJSON* light = cJSON_GetArrayItem(lights, i);
        cJSON* id = cJSON_GetObjectItem(light, "light_id");
        if (cJSON_IsString(id) && id->valuestring && strcmp(id->valuestring, light_id) == 0) {
            return light;
        }
    }
    return NULL;
}

static cJSON* ld_test_find_material_binding(cJSON* bindings,
                                            const char* target_kind,
                                            const char* object_id,
                                            const char* material_id,
                                            const char* target_key,
                                            const char* target_value) {
    int count = 0;
    int i = 0;
    if (!cJSON_IsArray(bindings) || !target_kind || !object_id || !material_id) return NULL;
    count = cJSON_GetArraySize(bindings);
    for (i = 0; i < count; ++i) {
        cJSON* binding = cJSON_GetArrayItem(bindings, i);
        cJSON* kind = cJSON_GetObjectItem(binding, "target_kind");
        cJSON* object = cJSON_GetObjectItem(binding, "object_id");
        cJSON* material = cJSON_GetObjectItem(binding, "material_id");
        cJSON* target = target_key ? cJSON_GetObjectItem(binding, target_key) : NULL;
        if (!cJSON_IsString(kind) || !cJSON_IsString(object) || !cJSON_IsString(material)) {
            continue;
        }
        if (strcmp(kind->valuestring, target_kind) != 0 ||
            strcmp(object->valuestring, object_id) != 0 ||
            strcmp(material->valuestring, material_id) != 0) {
            continue;
        }
        if (target_key &&
            (!cJSON_IsString(target) || strcmp(target->valuestring, target_value) != 0)) {
            continue;
        }
        return binding;
    }
    return NULL;
}

static bool test_shape_export_projection_axis_mapping(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Layout_AddWall3(layout, (Vec3){ 2.0f, 1.0f, 3.0f }, (Vec3){ 2.0f, 4.0f, 5.0f });

    ShapeDocument doc = {0};
    TEST_ASSERT(ShapeDocument_FromLayoutProjected("plane_yz", layout, VIEW_PLANE_YZ, &doc));
    TEST_ASSERT(doc.shapeCount == 1);
    TEST_ASSERT(doc.shapes[0].pathCount == 1);
    TEST_ASSERT(doc.shapes[0].paths[0].segmentCount == 1);

    const ShapeSegment* seg = &doc.shapes[0].paths[0].segments[0];
    TEST_ASSERT(ld_test_nearly_equal(seg->p0.x, 1.0f));
    TEST_ASSERT(ld_test_nearly_equal(seg->p0.y, 3.0f));
    TEST_ASSERT(ld_test_nearly_equal(seg->p1.x, 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(seg->p1.y, 5.0f));

    ShapeDocument_Free(&doc);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_space_mode_toggle_contract_2d_resets_plane_and_free_view(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();

    state->activePlane.axis = VIEW_PLANE_YZ;
    state->activePlane.offset = 9.0f;
    state->freeViewCamera.enabled = true;

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_2D, false));
    TEST_ASSERT(state->spaceMode == SPACE_MODE_2D);
    TEST_ASSERT(state->activePlane.axis == VIEW_PLANE_XY);
    TEST_ASSERT(ld_test_nearly_equal(state->activePlane.offset, 0.0f));
    TEST_ASSERT(!state->freeViewCamera.enabled);

    TEST_ASSERT(Global_ToggleSpaceMode(false));
    TEST_ASSERT(state->spaceMode == SPACE_MODE_3D);
    TEST_ASSERT(state->activePlane.axis == VIEW_PLANE_XY);
    TEST_ASSERT(ld_test_nearly_equal(state->activePlane.offset, 0.0f));
    TEST_ASSERT(!state->freeViewCamera.enabled);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_2d_payload(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Layout_AddWall(layout, (Vec2){ 0.0f, 0.0f }, (Vec2){ 2.0f, 1.0f });

    char* scene_json = LineDrawingCanonicalScene_ExportLayoutToString(layout, "scene_test_2d");
    TEST_ASSERT(scene_json != NULL);

    cJSON* root = cJSON_Parse(scene_json);
    TEST_ASSERT(root != NULL);
    free(scene_json);

    {
        cJSON* schema_family = cJSON_GetObjectItem(root, "schema_family");
        cJSON* schema_variant = cJSON_GetObjectItem(root, "schema_variant");
        cJSON* scene_id = cJSON_GetObjectItem(root, "scene_id");
        cJSON* space_mode_intent = cJSON_GetObjectItem(root, "space_mode_intent");
        cJSON* space_mode_default = cJSON_GetObjectItem(root, "space_mode_default");
        cJSON* conversion_policy = cJSON_GetObjectItem(root, "conversion_policy");
        cJSON* unit_system = cJSON_GetObjectItem(root, "unit_system");
        TEST_ASSERT(cJSON_IsString(schema_family));
        TEST_ASSERT(cJSON_IsString(schema_variant));
        TEST_ASSERT(cJSON_IsString(scene_id));
        TEST_ASSERT(cJSON_IsString(space_mode_intent));
        TEST_ASSERT(cJSON_IsString(space_mode_default));
        TEST_ASSERT(cJSON_IsString(conversion_policy));
        TEST_ASSERT(cJSON_IsString(unit_system));
        TEST_ASSERT(strcmp(schema_family->valuestring, "codework_scene") == 0);
        TEST_ASSERT(strcmp(schema_variant->valuestring, "scene_authoring_v1") == 0);
        TEST_ASSERT(strcmp(scene_id->valuestring, "scene_test_2d") == 0);
        TEST_ASSERT(strcmp(space_mode_intent->valuestring, "2d") == 0);
        TEST_ASSERT(strcmp(space_mode_default->valuestring, "2d") == 0);
        TEST_ASSERT(strcmp(conversion_policy->valuestring, "explicit_only") == 0);
        TEST_ASSERT(strcmp(unit_system->valuestring, "meters") == 0);
    }

    cJSON* objects = cJSON_GetObjectItem(root, "objects");
    cJSON* hierarchy = cJSON_GetObjectItem(root, "hierarchy");
    cJSON* materials = cJSON_GetObjectItem(root, "materials");
    cJSON* lights = cJSON_GetObjectItem(root, "lights");
    cJSON* cameras = cJSON_GetObjectItem(root, "cameras");
    TEST_ASSERT(cJSON_IsArray(objects));
    TEST_ASSERT(cJSON_IsArray(hierarchy));
    TEST_ASSERT(cJSON_IsArray(materials));
    TEST_ASSERT(cJSON_IsArray(lights));
    TEST_ASSERT(cJSON_IsArray(cameras));
    TEST_ASSERT(cJSON_GetArraySize(objects) == 3);

    cJSON* obj = ld_test_find_object_by_id(objects, "obj_line_drawing_layout");
    TEST_ASSERT(cJSON_IsObject(obj));
    {
        cJSON* object_mode_intent = cJSON_GetObjectItem(obj, "space_mode_intent");
        cJSON* object_id = cJSON_GetObjectItem(obj, "object_id");
        cJSON* dimensional_mode = cJSON_GetObjectItem(obj, "dimensional_mode");
        cJSON* locked_plane = cJSON_GetObjectItem(obj, "locked_plane");
        cJSON* projection_policy = cJSON_GetObjectItem(obj, "projection_policy");
        cJSON* extrusion_policy = cJSON_GetObjectItem(obj, "extrusion_policy");
        cJSON* material_ref = cJSON_GetObjectItem(obj, "material_ref");
        TEST_ASSERT(cJSON_IsString(object_mode_intent));
        TEST_ASSERT(cJSON_IsString(object_id));
        TEST_ASSERT(cJSON_IsString(dimensional_mode));
        TEST_ASSERT(cJSON_IsString(locked_plane));
        TEST_ASSERT(cJSON_IsString(projection_policy));
        TEST_ASSERT(cJSON_IsString(extrusion_policy));
        TEST_ASSERT(cJSON_IsObject(material_ref));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(material_ref, "id")->valuestring, "mat_default") == 0);
        TEST_ASSERT(strcmp(object_mode_intent->valuestring, "2d") == 0);
        TEST_ASSERT(strcmp(object_id->valuestring, "obj_line_drawing_layout") == 0);
        TEST_ASSERT(strcmp(dimensional_mode->valuestring, "plane_locked") == 0);
        TEST_ASSERT(strcmp(locked_plane->valuestring, "xy") == 0);
        TEST_ASSERT(strcmp(projection_policy->valuestring, "plane_xy_lock") == 0);
        TEST_ASSERT(strcmp(extrusion_policy->valuestring, "none") == 0);
    }

    cJSON* transform = cJSON_GetObjectItem(obj, "transform");
    cJSON* position = cJSON_GetObjectItem(transform, "position");
    TEST_ASSERT(ld_test_nearly_equal((float)cJSON_GetObjectItem(position, "z")->valuedouble, 0.0f));

    {
        cJSON* hierarchy_item = cJSON_GetArrayItem(hierarchy, 0);
        TEST_ASSERT(cJSON_IsObject(hierarchy_item));
        TEST_ASSERT(cJSON_GetArraySize(hierarchy) == 2);
        TEST_ASSERT(cJSON_GetArraySize(materials) == 1);
        TEST_ASSERT(cJSON_GetArraySize(lights) == 1);
        TEST_ASSERT(cJSON_GetArraySize(cameras) == 1);
    }

    TEST_ASSERT(ld_test_find_object_by_id(objects, "obj_line_drawing_anchor_set") != NULL);
    TEST_ASSERT(ld_test_find_object_by_id(objects, "obj_line_drawing_wall_set") != NULL);

    cJSON_Delete(root);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_3d_payload(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Layout_AddWall3(layout, (Vec3){ 0.0f, 0.0f, 0.0f }, (Vec3){ 0.0f, 1.0f, 5.0f });

    char* scene_json = LineDrawingCanonicalScene_ExportLayoutToString(layout, "scene_test_3d");
    TEST_ASSERT(scene_json != NULL);

    cJSON* root = cJSON_Parse(scene_json);
    TEST_ASSERT(root != NULL);
    free(scene_json);

    {
        cJSON* space_mode_intent = cJSON_GetObjectItem(root, "space_mode_intent");
        cJSON* space_mode_default = cJSON_GetObjectItem(root, "space_mode_default");
        cJSON* conversion_policy = cJSON_GetObjectItem(root, "conversion_policy");
        TEST_ASSERT(cJSON_IsString(space_mode_intent));
        TEST_ASSERT(cJSON_IsString(space_mode_default));
        TEST_ASSERT(cJSON_IsString(conversion_policy));
        TEST_ASSERT(strcmp(space_mode_intent->valuestring, "3d") == 0);
        TEST_ASSERT(strcmp(space_mode_default->valuestring, "3d") == 0);
        TEST_ASSERT(strcmp(conversion_policy->valuestring, "explicit_only") == 0);
    }

    cJSON* objects = cJSON_GetObjectItem(root, "objects");
    TEST_ASSERT(cJSON_IsArray(objects));
    TEST_ASSERT(cJSON_GetArraySize(objects) == 3);

    cJSON* obj = ld_test_find_object_by_id(objects, "obj_line_drawing_layout");
    TEST_ASSERT(cJSON_IsObject(obj));
    {
        cJSON* object_mode_intent = cJSON_GetObjectItem(obj, "space_mode_intent");
        cJSON* dimensional_mode = cJSON_GetObjectItem(obj, "dimensional_mode");
        cJSON* projection_policy = cJSON_GetObjectItem(obj, "projection_policy");
        cJSON* extrusion_policy = cJSON_GetObjectItem(obj, "extrusion_policy");
        TEST_ASSERT(cJSON_IsString(object_mode_intent));
        TEST_ASSERT(cJSON_IsString(dimensional_mode));
        TEST_ASSERT(cJSON_IsString(projection_policy));
        TEST_ASSERT(cJSON_IsString(extrusion_policy));
        TEST_ASSERT(strcmp(object_mode_intent->valuestring, "3d") == 0);
        TEST_ASSERT(strcmp(dimensional_mode->valuestring, "full_3d") == 0);
        TEST_ASSERT(strcmp(projection_policy->valuestring, "none") == 0);
        TEST_ASSERT(strcmp(extrusion_policy->valuestring, "none") == 0);
    }
    TEST_ASSERT(cJSON_GetObjectItem(obj, "locked_plane") == NULL);

    cJSON* transform = cJSON_GetObjectItem(obj, "transform");
    cJSON* position = cJSON_GetObjectItem(transform, "position");
    TEST_ASSERT(fabs(cJSON_GetObjectItem(position, "z")->valuedouble) > 0.0001);

    cJSON_Delete(root);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_preserves_existing_extensions(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    const char* path = "/tmp/line_drawing_scene_export_preserve.json";

    const char* seed_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_seed\","
        "\"space_mode_default\":\"2d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[{"
            "\"object_id\":\"obj_line_drawing_layout\","
            "\"object_type\":\"curve_path\","
            "\"dimensional_mode\":\"plane_locked\","
            "\"transform\":{\"position\":{\"x\":0,\"y\":0,\"z\":0},\"rotation\":{\"x\":0,\"y\":0,\"z\":0},\"scale\":{\"x\":1,\"y\":1,\"z\":1}},"
            "\"geometry_ref\":{\"kind\":\"shape_asset\",\"id\":\"shape_line_drawing_layout\"},"
            "\"tags\":[\"authoring\"],"
            "\"flags\":{\"visible\":true,\"locked\":false,\"selectable\":true},"
            "\"extensions\":{\"ray_tracing\":{\"material\":\"glass\"}}"
        "}],"
        "\"constraints\":[],"
        "\"extensions\":{\"physics_sim\":{\"gravity\":9.8},\"custom_ns\":{\"keep\":true}}"
        "}";

    {
        FILE* f = fopen(path, "wb");
        TEST_ASSERT(f != NULL);
        TEST_ASSERT(fwrite(seed_json, 1u, strlen(seed_json), f) == strlen(seed_json));
        TEST_ASSERT(fclose(f) == 0);
    }

    Layout_AddWall(layout, (Vec2){ 0.0f, 0.0f }, (Vec2){ 1.0f, 0.0f });
    TEST_ASSERT(LineDrawingCanonicalScene_ExportLayoutToFile(layout, "scene_preserve", path));

    {
        FILE* f = fopen(path, "rb");
        long len = 0;
        char* buf = NULL;
        cJSON* root = NULL;
        cJSON* exts = NULL;
        cJSON* objects = NULL;
        cJSON* obj = NULL;
        cJSON* obj_ext = NULL;

        TEST_ASSERT(f != NULL);
        TEST_ASSERT(fseek(f, 0, SEEK_END) == 0);
        len = ftell(f);
        TEST_ASSERT(len > 0);
        TEST_ASSERT(fseek(f, 0, SEEK_SET) == 0);
        buf = (char*)malloc((size_t)len + 1u);
        TEST_ASSERT(buf != NULL);
        TEST_ASSERT(fread(buf, 1u, (size_t)len, f) == (size_t)len);
        buf[len] = '\0';
        TEST_ASSERT(fclose(f) == 0);

        root = cJSON_Parse(buf);
        free(buf);
        TEST_ASSERT(root != NULL);

        exts = cJSON_GetObjectItem(root, "extensions");
        TEST_ASSERT(cJSON_IsObject(exts));
        TEST_ASSERT(cJSON_GetObjectItem(exts, "physics_sim") != NULL);
        TEST_ASSERT(cJSON_GetObjectItem(exts, "custom_ns") != NULL);
        TEST_ASSERT(cJSON_GetObjectItem(exts, "line_drawing") != NULL);

        objects = cJSON_GetObjectItem(root, "objects");
        TEST_ASSERT(cJSON_IsArray(objects));
        TEST_ASSERT(cJSON_GetArraySize(objects) == 3);
        obj = ld_test_find_object_by_id(objects, "obj_line_drawing_layout");
        TEST_ASSERT(cJSON_IsObject(obj));
        obj_ext = cJSON_GetObjectItem(obj, "extensions");
        TEST_ASSERT(cJSON_IsObject(obj_ext));
        TEST_ASSERT(cJSON_GetObjectItem(obj_ext, "ray_tracing") != NULL);
        TEST_ASSERT(cJSON_GetObjectItem(obj_ext, "line_drawing") != NULL);

        cJSON_Delete(root);
    }

    remove(path);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_preserves_camera_path_authoring_metadata(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    const char* path = "/tmp/line_drawing_scene_export_camera_path_authoring.json";
    LineDrawingSceneAuthoringOptions options = {
        .camera_id = "cam_review_path",
        .camera_type = "perspective",
    };

    const char* seed_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_camera_path_seed\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
            "\"ray_tracing\":{"
                "\"authoring\":{"
                    "\"camera_path\":{"
                        "\"camera_id\":\"cam_review_path\","
                        "\"points\":["
                            "{\"x\":-2.0,\"y\":-3.0,\"z\":2.5},"
                            "{\"x\":1.5,\"y\":-2.0,\"z\":2.1}"
                        "]"
                    "},"
                    "\"camera_path_depth\":{"
                        "\"camera_id\":\"cam_review_path\","
                        "\"samples\":["
                            "{\"frame\":0,\"lookPitch\":-0.2},"
                            "{\"frame\":12,\"lookPitch\":-0.1}"
                        "]"
                    "}"
                "}"
            "}"
        "}"
        "}";

    TEST_ASSERT(ld_test_write_text_file_basic(path, seed_json));
    Layout_AddWall3(layout, (Vec3){ -1.0f, -1.0f, 0.0f }, (Vec3){ 1.0f, 1.0f, 1.0f });
    TEST_ASSERT(LineDrawingCanonicalScene_ExportLayoutToFileWithOptions(
        layout, "scene_camera_path_round_trip", path, &options));

    {
        char* scene_json = ld_test_read_text_file(path);
        cJSON* root = NULL;
        cJSON* cameras = NULL;
        cJSON* camera = NULL;
        cJSON* extensions = NULL;
        cJSON* ray_tracing = NULL;
        cJSON* authoring = NULL;
        cJSON* camera_path = NULL;
        cJSON* camera_path_depth = NULL;
        cJSON* points = NULL;
        cJSON* samples = NULL;

        TEST_ASSERT(scene_json != NULL);
        root = cJSON_Parse(scene_json);
        free(scene_json);
        TEST_ASSERT(root != NULL);

        cameras = cJSON_GetObjectItem(root, "cameras");
        TEST_ASSERT(cJSON_IsArray(cameras));
        TEST_ASSERT(cJSON_GetArraySize(cameras) == 1);
        camera = cJSON_GetArrayItem(cameras, 0);
        TEST_ASSERT(cJSON_IsObject(camera));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera, "camera_id")->valuestring,
                           "cam_review_path") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera, "camera_type")->valuestring,
                           "perspective") == 0);
        TEST_ASSERT(cJSON_GetObjectItem(camera, "camera_path") == NULL);
        TEST_ASSERT(cJSON_GetObjectItem(camera, "camera_path_depth") == NULL);

        extensions = cJSON_GetObjectItem(root, "extensions");
        ray_tracing = cJSON_IsObject(extensions)
            ? cJSON_GetObjectItem(extensions, "ray_tracing")
            : NULL;
        authoring = cJSON_IsObject(ray_tracing)
            ? cJSON_GetObjectItem(ray_tracing, "authoring")
            : NULL;
        camera_path = cJSON_IsObject(authoring)
            ? cJSON_GetObjectItem(authoring, "camera_path")
            : NULL;
        camera_path_depth = cJSON_IsObject(authoring)
            ? cJSON_GetObjectItem(authoring, "camera_path_depth")
            : NULL;
        TEST_ASSERT(cJSON_IsObject(camera_path));
        TEST_ASSERT(cJSON_IsObject(camera_path_depth));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera_path, "camera_id")->valuestring,
                           "cam_review_path") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera_path_depth, "camera_id")->valuestring,
                           "cam_review_path") == 0);
        points = cJSON_GetObjectItem(camera_path, "points");
        samples = cJSON_GetObjectItem(camera_path_depth, "samples");
        TEST_ASSERT(cJSON_IsArray(points));
        TEST_ASSERT(cJSON_GetArraySize(points) == 2);
        TEST_ASSERT(cJSON_IsArray(samples));
        TEST_ASSERT(cJSON_GetArraySize(samples) == 2);

        cJSON_Delete(root);
    }

    remove(path);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_emits_canonical_camera_and_light_paths(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    LineDrawingSceneAuthoringOptions options = {
        .light_id = "light_sweep",
        .light_type = "point",
        .camera_id = "cam_orbit",
        .camera_type = "perspective",
        .camera_path_id = "path_camera_orbit",
        .camera_path_label = "Camera Orbit",
        .light_path_id = "path_light_sweep",
        .light_path_label = "Light Sweep",
    };

    Layout_AddWall3(layout, (Vec3){ -2.0f, -1.0f, 0.0f }, (Vec3){ 2.0f, 1.0f, 2.0f });

    char* scene_json =
        LineDrawingCanonicalScene_ExportLayoutToStringWithOptions(layout, "scene_canonical_paths", &options);
    TEST_ASSERT(scene_json != NULL);

    cJSON* root = cJSON_Parse(scene_json);
    TEST_ASSERT(root != NULL);
    free(scene_json);

    {
        cJSON* paths = cJSON_GetObjectItem(root, "paths");
        cJSON* cameras = cJSON_GetObjectItem(root, "cameras");
        cJSON* lights = cJSON_GetObjectItem(root, "lights");
        cJSON* camera = cJSON_GetArrayItem(cameras, 0);
        cJSON* light = cJSON_GetArrayItem(lights, 0);
        cJSON* camera_path = ld_test_find_path_by_id(paths, "path_camera_orbit");
        cJSON* light_path = ld_test_find_path_by_id(paths, "path_light_sweep");
        cJSON* camera_control_points = NULL;
        cJSON* light_control_points = NULL;

        TEST_ASSERT(cJSON_IsArray(paths));
        TEST_ASSERT(cJSON_GetArraySize(paths) == 2);
        TEST_ASSERT(cJSON_IsObject(camera));
        TEST_ASSERT(cJSON_IsObject(light));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera, "camera_id")->valuestring, "cam_orbit") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera, "path_id")->valuestring, "path_camera_orbit") == 0);
        TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(camera, "transform")));
        TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(camera, "orientation")));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(
                               cJSON_GetObjectItem(camera, "orientation"), "mode")->valuestring,
                           "path_facing") == 0);
        TEST_ASSERT(cJSON_IsNumber(cJSON_GetObjectItem(camera, "vertical_fov_degrees")));
        TEST_ASSERT(cJSON_IsNumber(cJSON_GetObjectItem(camera, "near_clip")));
        TEST_ASSERT(cJSON_IsNumber(cJSON_GetObjectItem(camera, "far_clip")));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light, "light_id")->valuestring, "light_sweep") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light, "path_id")->valuestring, "path_light_sweep") == 0);

        TEST_ASSERT(cJSON_IsObject(camera_path));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera_path, "path_kind")->valuestring, "camera") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera_path, "label")->valuestring, "Camera Orbit") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera_path, "camera_id")->valuestring, "cam_orbit") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera_path, "curve_type")->valuestring, "bezier") == 0);
        camera_control_points = cJSON_GetObjectItem(camera_path, "control_points");
        TEST_ASSERT(cJSON_IsArray(camera_control_points));
        TEST_ASSERT(cJSON_GetArraySize(camera_control_points) == 3);

        TEST_ASSERT(cJSON_IsObject(light_path));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light_path, "path_kind")->valuestring, "light") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light_path, "label")->valuestring, "Light Sweep") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light_path, "light_id")->valuestring, "light_sweep") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light_path, "curve_type")->valuestring, "bezier") == 0);
        light_control_points = cJSON_GetObjectItem(light_path, "control_points");
        TEST_ASSERT(cJSON_IsArray(light_control_points));
        TEST_ASSERT(cJSON_GetArraySize(light_control_points) == 3);
    }

    cJSON_Delete(root);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_emits_preview_mode_authoring_state(void) {
    static const char* const modes[] = { "bounds", "wireframe", "flat", "material" };
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Layout_AddWall3(layout, (Vec3){ 0.0f, 0.0f, 0.0f }, (Vec3){ 2.0f, 1.0f, 1.0f });

    for (size_t i = 0u; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        LineDrawingSceneAuthoringOptions options = {
            .preview_mode = modes[i],
        };
        char* scene_json =
            LineDrawingCanonicalScene_ExportLayoutToStringWithOptions(layout, "scene_preview_mode", &options);
        cJSON* root = NULL;
        cJSON* extensions = NULL;
        cJSON* line_drawing = NULL;
        cJSON* authoring = NULL;
        cJSON* preview_mode = NULL;

        TEST_ASSERT(scene_json != NULL);
        root = cJSON_Parse(scene_json);
        free(scene_json);
        TEST_ASSERT(root != NULL);

        extensions = cJSON_GetObjectItem(root, "extensions");
        line_drawing = cJSON_IsObject(extensions)
            ? cJSON_GetObjectItem(extensions, "line_drawing")
            : NULL;
        authoring = cJSON_IsObject(line_drawing)
            ? cJSON_GetObjectItem(line_drawing, "authoring")
            : NULL;
        preview_mode = cJSON_IsObject(authoring)
            ? cJSON_GetObjectItem(authoring, "preview_mode")
            : NULL;
        TEST_ASSERT(cJSON_IsString(preview_mode));
        TEST_ASSERT(strcmp(preview_mode->valuestring, modes[i]) == 0);

        cJSON_Delete(root);
    }

    {
        char* scene_json =
            LineDrawingCanonicalScene_ExportLayoutToString(layout, "scene_preview_default");
        cJSON* root = NULL;
        cJSON* extensions = NULL;
        cJSON* line_drawing = NULL;
        cJSON* authoring = NULL;
        cJSON* preview_mode = NULL;

        TEST_ASSERT(scene_json != NULL);
        root = cJSON_Parse(scene_json);
        free(scene_json);
        TEST_ASSERT(root != NULL);
        extensions = cJSON_GetObjectItem(root, "extensions");
        line_drawing = cJSON_IsObject(extensions)
            ? cJSON_GetObjectItem(extensions, "line_drawing")
            : NULL;
        authoring = cJSON_IsObject(line_drawing)
            ? cJSON_GetObjectItem(line_drawing, "authoring")
            : NULL;
        preview_mode = cJSON_IsObject(authoring)
            ? cJSON_GetObjectItem(authoring, "preview_mode")
            : NULL;
        TEST_ASSERT(cJSON_IsString(preview_mode));
        TEST_ASSERT(strcmp(preview_mode->valuestring, "wireframe") == 0);
        cJSON_Delete(root);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_preserves_existing_scene_id_and_canonical_object_ids(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    const char* path = "/tmp/line_drawing_scene_export_id_immutability.json";

    const char* seed_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_existing_immutable\","
        "\"space_mode_intent\":\"2d\","
        "\"space_mode_default\":\"2d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":["
            "{"
                "\"object_id\":\"obj_mutated_layout\","
                "\"object_type\":\"curve_path\","
                "\"space_mode_intent\":\"2d\","
                "\"dimensional_mode\":\"plane_locked\""
            "},"
            "{"
                "\"object_id\":\"obj_mutated_anchor_set\","
                "\"object_type\":\"point_set\","
                "\"space_mode_intent\":\"2d\","
                "\"dimensional_mode\":\"plane_locked\""
            "},"
            "{"
                "\"object_id\":\"obj_mutated_wall_set\","
                "\"object_type\":\"edge_set\","
                "\"space_mode_intent\":\"2d\","
                "\"dimensional_mode\":\"plane_locked\""
            "}"
        "],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";

    {
        FILE* f = fopen(path, "wb");
        TEST_ASSERT(f != NULL);
        TEST_ASSERT(fwrite(seed_json, 1u, strlen(seed_json), f) == strlen(seed_json));
        TEST_ASSERT(fclose(f) == 0);
    }

    Layout_AddWall(layout, (Vec2){ 0.0f, 0.0f }, (Vec2){ 4.0f, 0.0f });
    TEST_ASSERT(LineDrawingCanonicalScene_ExportLayoutToFile(layout, "scene_attempted_mutation", path));

    {
        FILE* f = fopen(path, "rb");
        long len = 0;
        char* buf = NULL;
        cJSON* root = NULL;
        cJSON* scene_id = NULL;
        cJSON* objects = NULL;

        TEST_ASSERT(f != NULL);
        TEST_ASSERT(fseek(f, 0, SEEK_END) == 0);
        len = ftell(f);
        TEST_ASSERT(len > 0);
        TEST_ASSERT(fseek(f, 0, SEEK_SET) == 0);
        buf = (char*)malloc((size_t)len + 1u);
        TEST_ASSERT(buf != NULL);
        TEST_ASSERT(fread(buf, 1u, (size_t)len, f) == (size_t)len);
        buf[len] = '\0';
        TEST_ASSERT(fclose(f) == 0);

        root = cJSON_Parse(buf);
        free(buf);
        TEST_ASSERT(root != NULL);

        scene_id = cJSON_GetObjectItem(root, "scene_id");
        TEST_ASSERT(cJSON_IsString(scene_id));
        TEST_ASSERT(strcmp(scene_id->valuestring, "scene_existing_immutable") == 0);

        objects = cJSON_GetObjectItem(root, "objects");
        TEST_ASSERT(cJSON_IsArray(objects));
        TEST_ASSERT(cJSON_GetArraySize(objects) == 3);
        TEST_ASSERT(ld_test_find_object_by_id(objects, "obj_line_drawing_layout") != NULL);
        TEST_ASSERT(ld_test_find_object_by_id(objects, "obj_line_drawing_anchor_set") != NULL);
        TEST_ASSERT(ld_test_find_object_by_id(objects, "obj_line_drawing_wall_set") != NULL);
        TEST_ASSERT(ld_test_find_object_by_id(objects, "obj_mutated_layout") == NULL);
        TEST_ASSERT(ld_test_find_object_by_id(objects, "obj_mutated_anchor_set") == NULL);
        TEST_ASSERT(ld_test_find_object_by_id(objects, "obj_mutated_wall_set") == NULL);

        cJSON_Delete(root);
    }

    remove(path);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_includes_scene3d_and_primitive_payloads(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    PlanePrimitiveCreateParams plane_params;
    RectPrismPrimitiveCreateParams prism_params;
    uint32_t plane_id = 0u;
    uint32_t prism_id = 0u;
    char plane_object_id[64];
    char prism_object_id[64];

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.clampOnEdit = true;
    layout->scene3d.bounds.min = (Vec3){ -6.0f, -5.0f, -4.0f };
    layout->scene3d.bounds.max = (Vec3){ 6.0f, 5.0f, 4.0f };
    Layout_ConstructionPlane3D_SetFromViewPlane(&layout->scene3d.constructionPlane,
                                                (ViewPlane){ .axis = VIEW_PLANE_YZ, .offset = 0.0f });

    Layout_PlanePrimitiveCreateParams_SetDefaults(&plane_params);
    plane_params.width = 4.0f;
    plane_params.height = 2.0f;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &plane_params, &plane_id, NULL));
    {
        Object3D* plane_object = Layout_ObjectStore_Find(&layout->objectStore, plane_id);
        TEST_ASSERT(plane_object != NULL);
        TEST_ASSERT(core_object_promote_to_full_3d(&plane_object->coreMeta).code == CORE_OK);
        TEST_ASSERT(plane_object->coreMeta.dimensional_mode == CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D);
    }

    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&prism_params);
    prism_params.width = 3.0f;
    prism_params.height = 2.0f;
    prism_params.depth = 1.5f;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &prism_params, &prism_id, NULL));

    snprintf(plane_object_id, sizeof(plane_object_id), "obj3d_%u", plane_id);
    snprintf(prism_object_id, sizeof(prism_object_id), "obj3d_%u", prism_id);

    char* scene_json = LineDrawingCanonicalScene_ExportLayoutToString(layout, "scene_primitives");
    TEST_ASSERT(scene_json != NULL);

    cJSON* root = cJSON_Parse(scene_json);
    TEST_ASSERT(root != NULL);
    free(scene_json);

    {
        cJSON* space_mode_intent = cJSON_GetObjectItem(root, "space_mode_intent");
        cJSON* space_mode_default = cJSON_GetObjectItem(root, "space_mode_default");
        cJSON* hierarchy = cJSON_GetObjectItem(root, "hierarchy");
        cJSON* objects = cJSON_GetObjectItem(root, "objects");
        cJSON* extensions = cJSON_GetObjectItem(root, "extensions");
        cJSON* line_drawing_ext = cJSON_IsObject(extensions)
            ? cJSON_GetObjectItem(extensions, "line_drawing")
            : NULL;
        cJSON* physics_sim_ext = cJSON_IsObject(extensions)
            ? cJSON_GetObjectItem(extensions, "physics_sim")
            : NULL;
        cJSON* scene3d = cJSON_IsObject(line_drawing_ext)
            ? cJSON_GetObjectItem(line_drawing_ext, "scene3d")
            : NULL;
        cJSON* bounds = cJSON_IsObject(scene3d)
            ? cJSON_GetObjectItem(scene3d, "bounds")
            : NULL;
        cJSON* authoring_bounds = cJSON_IsObject(scene3d)
            ? cJSON_GetObjectItem(scene3d, "authoring_bounds")
            : NULL;
        cJSON* construction_plane = cJSON_IsObject(scene3d)
            ? cJSON_GetObjectItem(scene3d, "construction_plane")
            : NULL;
        cJSON* scene_domain = cJSON_IsObject(physics_sim_ext)
            ? cJSON_GetObjectItem(physics_sim_ext, "scene_domain")
            : NULL;
        cJSON* plane_obj = NULL;
        cJSON* prism_obj = NULL;

        TEST_ASSERT(cJSON_IsString(space_mode_intent));
        TEST_ASSERT(cJSON_IsString(space_mode_default));
        TEST_ASSERT(strcmp(space_mode_intent->valuestring, "3d") == 0);
        TEST_ASSERT(strcmp(space_mode_default->valuestring, "3d") == 0);

        TEST_ASSERT(cJSON_IsArray(hierarchy));
        TEST_ASSERT(cJSON_GetArraySize(hierarchy) == 4);
        TEST_ASSERT(cJSON_IsArray(objects));
        TEST_ASSERT(cJSON_GetArraySize(objects) == 5);

        TEST_ASSERT(cJSON_IsObject(line_drawing_ext));
        TEST_ASSERT(cJSON_IsNumber(cJSON_GetObjectItem(line_drawing_ext, "active_object3d_count")));
        TEST_ASSERT(cJSON_GetObjectItem(line_drawing_ext, "active_object3d_count")->valueint == 2);

        TEST_ASSERT(cJSON_IsObject(bounds));
        TEST_ASSERT(cJSON_IsTrue(cJSON_GetObjectItem(bounds, "enabled")));
        TEST_ASSERT(cJSON_IsTrue(cJSON_GetObjectItem(bounds, "clamp_on_edit")));
        TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(bounds, "min")));
        TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(bounds, "max")));
        TEST_ASSERT(ld_test_json_bounds_matches(bounds, layout->scene3d.bounds));
        TEST_ASSERT(ld_test_json_bounds_matches(authoring_bounds, layout->scene3d.bounds));
        TEST_ASSERT(cJSON_IsObject(scene_domain));
        TEST_ASSERT(cJSON_IsTrue(cJSON_GetObjectItem(scene_domain, "active")));
        TEST_ASSERT(cJSON_IsFalse(cJSON_GetObjectItem(scene_domain, "seeded_from_retained_bounds")));
        TEST_ASSERT(ld_test_json_vec3_matches(cJSON_GetObjectItem(scene_domain, "min"),
                                              layout->scene3d.bounds.min));
        TEST_ASSERT(ld_test_json_vec3_matches(cJSON_GetObjectItem(scene_domain, "max"),
                                              layout->scene3d.bounds.max));

        TEST_ASSERT(cJSON_IsObject(construction_plane));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(construction_plane, "mode")->valuestring, "axis_aligned") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(construction_plane, "axis")->valuestring, "yz") == 0);
        TEST_ASSERT(ld_test_nearly_equal((float)cJSON_GetObjectItem(construction_plane, "offset")->valuedouble, 0.0f));

        plane_obj = ld_test_find_object_by_id(objects, plane_object_id);
        prism_obj = ld_test_find_object_by_id(objects, prism_object_id);
        TEST_ASSERT(cJSON_IsObject(plane_obj));
        TEST_ASSERT(cJSON_IsObject(prism_obj));

        TEST_ASSERT(strcmp(cJSON_GetObjectItem(plane_obj, "object_type")->valuestring, "plane_primitive") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(plane_obj, "dimensional_mode")->valuestring, "plane_locked") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(plane_obj, "locked_plane")->valuestring, "yz") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(plane_obj, "projection_policy")->valuestring, "plane_yz_lock") == 0);

        {
            cJSON* plane_canonical = cJSON_GetObjectItem(plane_obj, "primitive");
            cJSON* plane_ext = cJSON_GetObjectItem(cJSON_GetObjectItem(plane_obj, "extensions"), "line_drawing");
            cJSON* plane_payload = cJSON_IsObject(plane_ext)
                ? cJSON_GetObjectItem(plane_ext, "primitive_payload")
                : NULL;
            TEST_ASSERT(cJSON_IsObject(plane_canonical));
            TEST_ASSERT(strcmp(cJSON_GetObjectItem(plane_canonical, "kind")->valuestring,
                               "plane_primitive") == 0);
            TEST_ASSERT(ld_test_nearly_equal((float)cJSON_GetObjectItem(plane_canonical, "width")->valuedouble, 4.0f));
            TEST_ASSERT(ld_test_nearly_equal((float)cJSON_GetObjectItem(plane_canonical, "height")->valuedouble, 2.0f));
            TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(plane_canonical, "frame")));
            TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(cJSON_GetObjectItem(plane_canonical, "frame"),
                                                           "axis_u")));
            TEST_ASSERT(cJSON_IsObject(plane_payload));
            TEST_ASSERT(strcmp(cJSON_GetObjectItem(plane_ext, "primitive_kind")->valuestring, "plane") == 0);
            TEST_ASSERT(ld_test_nearly_equal((float)cJSON_GetObjectItem(plane_payload, "width")->valuedouble, 4.0f));
            TEST_ASSERT(ld_test_nearly_equal((float)cJSON_GetObjectItem(plane_payload, "height")->valuedouble, 2.0f));
            TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(plane_payload, "frame")));
        }

        TEST_ASSERT(strcmp(cJSON_GetObjectItem(prism_obj, "object_type")->valuestring,
                           "rect_prism_primitive") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(prism_obj, "dimensional_mode")->valuestring, "full_3d") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(prism_obj, "space_mode_intent")->valuestring, "3d") == 0);

        {
            cJSON* prism_canonical = cJSON_GetObjectItem(prism_obj, "primitive");
            cJSON* prism_ext = cJSON_GetObjectItem(cJSON_GetObjectItem(prism_obj, "extensions"), "line_drawing");
            cJSON* prism_payload = cJSON_IsObject(prism_ext)
                ? cJSON_GetObjectItem(prism_ext, "primitive_payload")
                : NULL;
            TEST_ASSERT(cJSON_IsObject(prism_canonical));
            TEST_ASSERT(strcmp(cJSON_GetObjectItem(prism_canonical, "kind")->valuestring,
                               "rect_prism_primitive") == 0);
            TEST_ASSERT(ld_test_nearly_equal((float)cJSON_GetObjectItem(prism_canonical, "depth")->valuedouble, 1.5f));
            TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(prism_canonical, "frame")));
            TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(cJSON_GetObjectItem(prism_canonical, "frame"),
                                                           "axis_u")));
            TEST_ASSERT(cJSON_IsObject(prism_payload));
            TEST_ASSERT(strcmp(cJSON_GetObjectItem(prism_ext, "primitive_kind")->valuestring, "rect_prism") == 0);
            TEST_ASSERT(ld_test_nearly_equal((float)cJSON_GetObjectItem(prism_payload, "depth")->valuedouble, 1.5f));
            TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(prism_payload, "frame")));
        }
    }

    cJSON_Delete(root);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_fixture_pure_3d_primitives_export_contract(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    char* fixture_json = ld_test_read_text_file("tests/fixtures/ld3d3_primitive_layout_fixture.json");
    TEST_ASSERT(fixture_json != NULL);
    TEST_ASSERT(Layout_LoadFromString(layout, fixture_json));
    free(fixture_json);

    TEST_ASSERT(Layout_ObjectStore_LiveCount(&layout->objectStore) == 2u);
    TEST_ASSERT(layout->scene3d.bounds.enabled == true);
    TEST_ASSERT(layout->scene3d.constructionPlane.axisAligned.axis == VIEW_PLANE_YZ);

    {
        const Object3D* plane = Layout_ObjectStore_FindConst(&layout->objectStore, 1u);
        const Object3D* prism = Layout_ObjectStore_FindConst(&layout->objectStore, 2u);
        char* scene_json = NULL;
        cJSON* root = NULL;
        cJSON* objects = NULL;
        cJSON* plane_obj = NULL;
        cJSON* prism_obj = NULL;

        TEST_ASSERT(plane != NULL);
        TEST_ASSERT(prism != NULL);
        TEST_ASSERT(plane->kind == OBJECT3D_KIND_PLANE);
        TEST_ASSERT(prism->kind == OBJECT3D_KIND_RECT_PRISM);
        TEST_ASSERT(ld_test_nearly_equal(plane->plane.width, 4.5f));
        TEST_ASSERT(ld_test_nearly_equal(prism->rectPrism.depth, 1.5f));

        scene_json = LineDrawingCanonicalScene_ExportLayoutToString(layout, "scene_fixture_pure_3d");
        TEST_ASSERT(scene_json != NULL);
        root = cJSON_Parse(scene_json);
        TEST_ASSERT(root != NULL);
        free(scene_json);

        TEST_ASSERT(strcmp(cJSON_GetObjectItem(root, "space_mode_default")->valuestring, "3d") == 0);
        objects = cJSON_GetObjectItem(root, "objects");
        TEST_ASSERT(cJSON_IsArray(objects));
        plane_obj = ld_test_find_object_by_id(objects, "obj3d_1");
        prism_obj = ld_test_find_object_by_id(objects, "obj3d_2");
        TEST_ASSERT(cJSON_IsObject(plane_obj));
        TEST_ASSERT(cJSON_IsObject(prism_obj));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(plane_obj, "primitive"), "kind")->valuestring,
                           "plane_primitive") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(prism_obj, "primitive"), "kind")->valuestring,
                           "rect_prism_primitive") == 0);
        cJSON_Delete(root);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_fixture_mixed_2d_3d_export_contract(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    char* fixture_json = ld_test_read_text_file("tests/fixtures/ld3d4_mixed_layout_fixture.json");
    TEST_ASSERT(fixture_json != NULL);
    TEST_ASSERT(Layout_LoadFromString(layout, fixture_json));
    free(fixture_json);

    TEST_ASSERT(layout->anchorCount == 4u);
    TEST_ASSERT(layout->wallCount == 4u);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&layout->objectStore) == 1u);

    {
        char* scene_json = LineDrawingCanonicalScene_ExportLayoutToString(layout, "scene_fixture_mixed");
        cJSON* root = NULL;
        cJSON* objects = NULL;
        cJSON* layout_obj = NULL;
        cJSON* prism_obj = NULL;
        TEST_ASSERT(scene_json != NULL);
        root = cJSON_Parse(scene_json);
        TEST_ASSERT(root != NULL);
        free(scene_json);

        TEST_ASSERT(strcmp(cJSON_GetObjectItem(root, "space_mode_default")->valuestring, "3d") == 0);
        objects = cJSON_GetObjectItem(root, "objects");
        TEST_ASSERT(cJSON_IsArray(objects));
        layout_obj = ld_test_find_object_by_id(objects, "obj_line_drawing_layout");
        prism_obj = ld_test_find_object_by_id(objects, "obj3d_7");
        TEST_ASSERT(cJSON_IsObject(layout_obj));
        TEST_ASSERT(cJSON_IsObject(prism_obj));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(layout_obj, "object_type")->valuestring, "curve_path") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(prism_obj, "primitive"), "kind")->valuestring,
                           "rect_prism_primitive") == 0);
        cJSON_Delete(root);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_applies_scene_authoring_options(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    LineDrawingSceneAuthoringOptions options = {
        .material_id = "mat_custom",
        .material_type = "flat_color",
        .light_id = "light_custom",
        .light_type = "point",
        .camera_id = "cam_custom",
        .camera_type = "perspective",
        .world_scale = 1.25,
        .unit_system = "meters",
        .conversion_policy = "explicit_only",
    };

    Layout_AddWall3(layout, (Vec3){ 0.0f, 0.0f, 0.0f }, (Vec3){ 0.0f, 2.0f, 2.0f });

    char* scene_json =
        LineDrawingCanonicalScene_ExportLayoutToStringWithOptions(layout, "scene_options", &options);
    TEST_ASSERT(scene_json != NULL);

    cJSON* root = cJSON_Parse(scene_json);
    TEST_ASSERT(root != NULL);
    free(scene_json);

    {
        cJSON* materials = cJSON_GetObjectItem(root, "materials");
        cJSON* lights = cJSON_GetObjectItem(root, "lights");
        cJSON* cameras = cJSON_GetObjectItem(root, "cameras");
        cJSON* objects = cJSON_GetObjectItem(root, "objects");
        cJSON* unit_system = cJSON_GetObjectItem(root, "unit_system");
        cJSON* conversion_policy = cJSON_GetObjectItem(root, "conversion_policy");
        cJSON* world_scale = cJSON_GetObjectItem(root, "world_scale");
        TEST_ASSERT(cJSON_IsArray(materials));
        TEST_ASSERT(cJSON_IsArray(lights));
        TEST_ASSERT(cJSON_IsArray(cameras));
        TEST_ASSERT(cJSON_IsArray(objects));
        TEST_ASSERT(cJSON_IsString(unit_system));
        TEST_ASSERT(cJSON_IsString(conversion_policy));
        TEST_ASSERT(cJSON_IsNumber(world_scale));
        TEST_ASSERT(cJSON_GetArraySize(materials) == 1);
        TEST_ASSERT(cJSON_GetArraySize(lights) == 1);
        TEST_ASSERT(cJSON_GetArraySize(cameras) == 1);
        TEST_ASSERT(strcmp(unit_system->valuestring, "meters") == 0);
        TEST_ASSERT(strcmp(conversion_policy->valuestring, "explicit_only") == 0);
        TEST_ASSERT(world_scale->valuedouble == 1.25);

        cJSON* material = cJSON_GetArrayItem(materials, 0);
        cJSON* light = cJSON_GetArrayItem(lights, 0);
        cJSON* camera = cJSON_GetArrayItem(cameras, 0);
        TEST_ASSERT(cJSON_IsObject(material));
        TEST_ASSERT(cJSON_IsObject(light));
        TEST_ASSERT(cJSON_IsObject(camera));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(material, "material_id")->valuestring, "mat_custom") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(material, "material_type")->valuestring, "flat_color") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light, "light_id")->valuestring, "light_custom") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light, "light_type")->valuestring, "point") == 0);
        TEST_ASSERT(cJSON_IsNumber(cJSON_GetObjectItem(light, "radius")));
        TEST_ASSERT(fabs(cJSON_GetObjectItem(light, "radius")->valuedouble) < 0.001);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera, "camera_id")->valuestring, "cam_custom") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera, "camera_type")->valuestring, "perspective") == 0);

        cJSON* obj = ld_test_find_object_by_id(objects, "obj_line_drawing_layout");
        TEST_ASSERT(cJSON_IsObject(obj));
        cJSON* mat_ref = cJSON_GetObjectItem(obj, "material_ref");
        TEST_ASSERT(cJSON_IsObject(mat_ref));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(mat_ref, "id")->valuestring, "mat_custom") == 0);
    }

    cJSON_Delete(root);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_emits_material_binding_authoring_data(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    PlanePrimitiveCreateParams plane_params;
    RectPrismPrimitiveCreateParams prism_params;
    uint32_t plane_id = 0u;
    uint32_t prism_id = 0u;
    char plane_object_id[64];
    char prism_object_id[64];
    LineDrawingSceneMaterialBindingOption binding_options[2] = {
        {
            .binding_id = "bind_plane_surface",
            .target_kind = "face",
            .object_id = plane_object_id,
            .face_role = "plane_surface",
            .material_id = "mat_s5_base",
            .binding_role = "face_override",
        },
        {
            .binding_id = "bind_prism_surface_group",
            .target_kind = "surface_group",
            .object_id = prism_object_id,
            .surface_group_id = "face_18",
            .material_id = "mat_s5_base",
            .binding_role = "surface_group_override",
        },
    };
    LineDrawingSceneAuthoringOptions options = {
        .material_id = "mat_s5_base",
        .material_type = "flat_color",
        .material_bindings = binding_options,
        .material_binding_count = 2u,
    };

    Layout_PlanePrimitiveCreateParams_SetDefaults(&plane_params);
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &plane_params, &plane_id, NULL));
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&prism_params);
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &prism_params, &prism_id, NULL));

    snprintf(plane_object_id, sizeof(plane_object_id), "obj3d_%u", plane_id);
    snprintf(prism_object_id, sizeof(prism_object_id), "obj3d_%u", prism_id);

    char* scene_json =
        LineDrawingCanonicalScene_ExportLayoutToStringWithOptions(layout, "scene_material_bindings", &options);
    TEST_ASSERT(scene_json != NULL);
    cJSON* root = cJSON_Parse(scene_json);
    TEST_ASSERT(root != NULL);
    free(scene_json);

    {
        cJSON* materials = cJSON_GetObjectItem(root, "materials");
        cJSON* material_bindings = cJSON_GetObjectItem(root, "material_bindings");
        cJSON* objects = cJSON_GetObjectItem(root, "objects");
        cJSON* material = cJSON_GetArrayItem(materials, 0);
        cJSON* plane_obj = ld_test_find_object_by_id(objects, plane_object_id);
        cJSON* prism_obj = ld_test_find_object_by_id(objects, prism_object_id);
        cJSON* default_layout_binding = NULL;
        cJSON* default_plane_binding = NULL;
        cJSON* face_binding = NULL;
        cJSON* surface_binding = NULL;

        TEST_ASSERT(cJSON_IsArray(materials));
        TEST_ASSERT(cJSON_IsObject(material));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(material, "material_id")->valuestring, "mat_s5_base") == 0);
        TEST_ASSERT(cJSON_IsArray(material_bindings));
        TEST_ASSERT(cJSON_IsObject(plane_obj));
        TEST_ASSERT(cJSON_IsObject(prism_obj));

        default_layout_binding =
            ld_test_find_material_binding(material_bindings,
                                          "object",
                                          "obj_line_drawing_layout",
                                          "mat_s5_base",
                                          NULL,
                                          NULL);
        default_plane_binding =
            ld_test_find_material_binding(material_bindings,
                                          "object",
                                          plane_object_id,
                                          "mat_s5_base",
                                          NULL,
                                          NULL);
        face_binding =
            ld_test_find_material_binding(material_bindings,
                                          "face",
                                          plane_object_id,
                                          "mat_s5_base",
                                          "face_role",
                                          "plane_surface");
        surface_binding =
            ld_test_find_material_binding(material_bindings,
                                          "surface_group",
                                          prism_object_id,
                                          "mat_s5_base",
                                          "surface_group_id",
                                          "face_18");

        TEST_ASSERT(cJSON_IsObject(default_layout_binding));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(default_layout_binding, "binding_role")->valuestring,
                           "default") == 0);
        TEST_ASSERT(cJSON_IsObject(default_plane_binding));
        TEST_ASSERT(cJSON_IsObject(face_binding));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(face_binding, "binding_id")->valuestring,
                           "bind_plane_surface") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(face_binding, "binding_role")->valuestring,
                           "face_override") == 0);
        TEST_ASSERT(cJSON_IsObject(surface_binding));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(surface_binding, "binding_id")->valuestring,
                           "bind_prism_surface_group") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(surface_binding, "binding_role")->valuestring,
                           "surface_group_override") == 0);
        TEST_ASSERT(cJSON_GetObjectItem(face_binding, "extensions") == NULL);
        TEST_ASSERT(cJSON_GetObjectItem(surface_binding, "extensions") == NULL);
    }

    cJSON_Delete(root);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_uses_live_scene_authoring_records(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    LineDrawingSceneAuthoringState* authoring = &layout->sceneAuthoring;
    size_t material_index = 0u;
    Vec3 edited_point = { 3.25f, -4.5f, 6.75f };

    Layout_AddWall3(layout, (Vec3){ -1.0f, -1.0f, 0.0f }, (Vec3){ 1.0f, 1.0f, 1.0f });
    TEST_ASSERT(Layout_SceneAuthoringState_AddDefaultMaterial(authoring, &material_index));
    TEST_ASSERT(material_index == 1u);
    TEST_ASSERT(Layout_SceneAuthoringState_SetPathControlPoint(authoring,
                                                                     0u,
                                                                     1u,
                                                                     edited_point));
    authoring->lights[0].kind = LINE_DRAWING_SCENE_LIGHT_AREA;
    authoring->lights[0].aim_target = (Vec3){2.0f, 3.0f, 4.0f};
    authoring->lights[0].color_rgb[0] = 1.0f;
    authoring->lights[0].color_rgb[1] = 0.78f;
    authoring->lights[0].color_rgb[2] = 0.55f;
    authoring->lights[0].intensity = 5.0f;
    authoring->lights[0].radius = 1.0f;
    authoring->lights[0].area_size = (Vec2){4.0f, 8.0f};
    authoring->lights[0].inner_cone_degrees = 35.0f;
    authoring->lights[0].outer_cone_degrees = 60.0f;
    authoring->lights[0].falloff = LINE_DRAWING_SCENE_LIGHT_FALLOFF_LINEAR;

    char* scene_json =
        LineDrawingCanonicalScene_ExportLayoutToString(layout, "scene_live_authoring");
    TEST_ASSERT(scene_json != NULL);
    cJSON* root = cJSON_Parse(scene_json);
    TEST_ASSERT(root != NULL);
    free(scene_json);

    {
        cJSON* materials = cJSON_GetObjectItem(root, "materials");
        cJSON* material_bindings = cJSON_GetObjectItem(root, "material_bindings");
        cJSON* lights = cJSON_GetObjectItem(root, "lights");
        cJSON* cameras = cJSON_GetObjectItem(root, "cameras");
        cJSON* paths = cJSON_GetObjectItem(root, "paths");
        cJSON* material0 = cJSON_GetArrayItem(materials, 0);
        cJSON* material1 = cJSON_GetArrayItem(materials, 1);
        cJSON* light = ld_test_find_light_by_id(lights, "light_key");
        cJSON* camera = ld_test_find_camera_by_id(cameras, "camera_main");
        cJSON* path = ld_test_find_path_by_id(paths, "path_camera_main");
        cJSON* light_path = ld_test_find_path_by_id(paths, "path_light_key");
        cJSON* control_points = NULL;
        cJSON* tangent_modes = NULL;
        cJSON* edited_point_json = NULL;
        cJSON* default_binding = NULL;

        TEST_ASSERT(cJSON_IsArray(materials));
        TEST_ASSERT(cJSON_GetArraySize(materials) == 2);
        TEST_ASSERT(cJSON_IsObject(material0));
        TEST_ASSERT(cJSON_IsObject(material1));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(material0, "material_id")->valuestring,
                           "mat_default") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(material1, "material_id")->valuestring,
                           "mat_002") == 0);
        TEST_ASSERT(cJSON_IsArray(material_bindings));
        default_binding =
            ld_test_find_material_binding(material_bindings,
                                          "object",
                                          "obj_line_drawing_layout",
                                          "mat_default",
                                          NULL,
                                          NULL);
        TEST_ASSERT(cJSON_IsObject(default_binding));

        TEST_ASSERT(cJSON_IsArray(lights));
        TEST_ASSERT(cJSON_IsObject(light));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light, "light_type")->valuestring,
                           "area") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light, "path_id")->valuestring,
                           "path_light_key") == 0);
        TEST_ASSERT(cJSON_IsTrue(cJSON_GetObjectItem(light, "enabled")));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light, "position_mode")->valuestring,
                           "independent") == 0);
        TEST_ASSERT(ld_test_json_vec3_matches(cJSON_GetObjectItem(light, "aim_target"),
                                              (Vec3){2.0f, 3.0f, 4.0f}));
        TEST_ASSERT(ld_test_json_vec3_matches(cJSON_GetObjectItem(light, "color"),
                                              (Vec3){1.0f, 0.78f, 0.55f}));
        TEST_ASSERT(fabs(cJSON_GetObjectItem(light, "intensity")->valuedouble - 5.0) < 0.001);
        TEST_ASSERT(fabs(cJSON_GetObjectItem(light, "radius")->valuedouble - 1.0) < 0.001);
        TEST_ASSERT(fabs(cJSON_GetObjectItem(
            cJSON_GetObjectItem(light, "area_size"), "width")->valuedouble - 4.0) < 0.001);
        TEST_ASSERT(fabs(cJSON_GetObjectItem(
            cJSON_GetObjectItem(light, "area_size"), "height")->valuedouble - 8.0) < 0.001);
        TEST_ASSERT(fabs(cJSON_GetObjectItem(
            cJSON_GetObjectItem(light, "cone"), "outer_degrees")->valuedouble - 60.0) < 0.001);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light, "falloff")->valuestring,
                           "linear") == 0);

        TEST_ASSERT(cJSON_IsArray(cameras));
        TEST_ASSERT(cJSON_IsObject(camera));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera, "path_id")->valuestring,
                           "path_camera_main") == 0);

        TEST_ASSERT(cJSON_IsArray(paths));
        TEST_ASSERT(cJSON_IsObject(path));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(path, "path_kind")->valuestring,
                           "camera") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(path, "curve_type")->valuestring,
                           "bezier") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(path, "camera_id")->valuestring,
                           "camera_main") == 0);
        TEST_ASSERT(cJSON_GetObjectItem(path, "light_id") == NULL);
        TEST_ASSERT(cJSON_IsObject(light_path));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light_path, "path_kind")->valuestring,
                           "light") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light_path, "light_id")->valuestring,
                           "light_key") == 0);
        control_points = cJSON_GetObjectItem(path, "control_points");
        TEST_ASSERT(cJSON_IsArray(control_points));
        TEST_ASSERT(cJSON_GetArraySize(control_points) == 4);
        tangent_modes = cJSON_GetObjectItem(path, "tangent_modes");
        TEST_ASSERT(cJSON_IsArray(tangent_modes));
        TEST_ASSERT(cJSON_GetArraySize(tangent_modes) == 2);
        TEST_ASSERT(strcmp(cJSON_GetArrayItem(tangent_modes, 0)->valuestring,
                           "smooth") == 0);
        edited_point_json = cJSON_GetArrayItem(control_points, 1);
        TEST_ASSERT(ld_test_json_vec3_matches(edited_point_json, edited_point));
    }

    cJSON_Delete(root);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_emits_mesh_asset_instance_reference(void) {
    const char* runtime_path = "/tmp/ld_mesh_asset_instance_scene_export.runtime.json";
    const char* runtime_json =
        "{"
        "\"schema_variant\":\"mesh_asset_runtime_v1\","
        "\"asset_id\":\"asset_scene_mesh\","
        "\"source_asset_id\":\"source_scene_mesh\","
        "\"vertex_count\":8,"
        "\"triangle_count\":12,"
        "\"local_bounds\":{"
            "\"min\":{\"x\":-0.5,\"y\":-1.0,\"z\":-1.5},"
            "\"max\":{\"x\":0.5,\"y\":1.0,\"z\":1.5}"
        "}"
        "}";
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    Transform3D transform = Layout_Transform3D_Default();
    uint32_t object_id = 0u;
    char diagnostics[256] = {0};

    transform.position = (Vec3){ 4.0f, 5.0f, 6.0f };
    TEST_ASSERT(ld_test_write_text_file_basic(runtime_path, runtime_json));
    TEST_ASSERT(Layout_CreateMeshAssetInstanceFromRuntimeAsset(layout,
                                                              runtime_path,
                                                              &transform,
                                                              &object_id,
                                                              diagnostics,
                                                              sizeof(diagnostics)));
    TEST_ASSERT(object_id == 1u);

    char* scene_json = LineDrawingCanonicalScene_ExportLayoutToString(layout, "scene_mesh_asset_instance");
    TEST_ASSERT(scene_json != NULL);
    cJSON* root = cJSON_Parse(scene_json);
    TEST_ASSERT(root != NULL);
    free(scene_json);

    cJSON* objects = cJSON_GetObjectItem(root, "objects");
    TEST_ASSERT(cJSON_IsArray(objects));
    cJSON* obj = ld_test_find_object_by_id(objects, "obj3d_1");
    TEST_ASSERT(cJSON_IsObject(obj));
    TEST_ASSERT(strcmp(cJSON_GetObjectItem(obj, "object_type")->valuestring, "mesh_asset_instance") == 0);
    {
        cJSON* geometry_ref = cJSON_GetObjectItem(obj, "geometry_ref");
        cJSON* extensions = cJSON_GetObjectItem(obj, "extensions");
        cJSON* line_ext = cJSON_IsObject(extensions) ? cJSON_GetObjectItem(extensions, "line_drawing") : NULL;
        cJSON* local_bounds = cJSON_IsObject(line_ext) ? cJSON_GetObjectItem(line_ext, "local_bounds") : NULL;
        TEST_ASSERT(cJSON_IsObject(geometry_ref));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(geometry_ref, "kind")->valuestring, "mesh_asset") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(geometry_ref, "id")->valuestring, "asset_scene_mesh") == 0);
        TEST_ASSERT(cJSON_IsObject(line_ext));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(line_ext, "geometry_source")->valuestring,
                           "mesh_asset_instance") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(line_ext, "runtime_mesh_path")->valuestring,
                           runtime_path) == 0);
        TEST_ASSERT(cJSON_GetObjectItem(line_ext, "runtime_triangle_count")->valueint == 12);
        TEST_ASSERT(cJSON_IsObject(local_bounds));
        TEST_ASSERT(ld_test_json_vec3_matches(cJSON_GetObjectItem(local_bounds, "min"),
                                              (Vec3){ -0.5f, -1.0f, -1.5f }));
        TEST_ASSERT(ld_test_json_vec3_matches(cJSON_GetObjectItem(local_bounds, "max"),
                                              (Vec3){ 0.5f, 1.0f, 1.5f }));
    }

    cJSON_Delete(root);
    remove(runtime_path);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_rejects_invalid_scene_authoring_options(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    const char* path = "/tmp/line_drawing_scene_export_invalid_options.json";
    LineDrawingSceneAuthoringOptions bad_options = {
        .material_id = "bad id",
        .material_type = "flat_color",
        .light_id = "light_custom",
        .light_type = "point",
        .camera_id = "cam_custom",
        .camera_type = "perspective",
        .world_scale = 1.0,
        .unit_system = "meters",
        .conversion_policy = "explicit_only",
    };

    Layout_AddWall(layout, (Vec2){ 0.0f, 0.0f }, (Vec2){ 1.0f, 1.0f });
    TEST_ASSERT(LineDrawingCanonicalScene_ExportLayoutToStringWithOptions(
                    layout, "scene_bad_options", &bad_options) == NULL);
    TEST_ASSERT(!LineDrawingCanonicalScene_ExportLayoutToFileWithOptions(
        layout, "scene_bad_options", path, &bad_options));
    {
        LineDrawingSceneAuthoringOptions bad_preview_options = {
            .preview_mode = "raytrace_final",
        };
        TEST_ASSERT(LineDrawingCanonicalScene_ExportLayoutToStringWithOptions(
                        layout, "scene_bad_preview_options", &bad_preview_options) == NULL);
    }
    {
        LineDrawingSceneMaterialBindingOption bad_binding = {
            .target_kind = "face",
            .object_id = "obj3d_1",
            .material_id = "mat_default",
        };
        LineDrawingSceneAuthoringOptions bad_binding_options = {
            .material_bindings = &bad_binding,
            .material_binding_count = 1u,
        };
        TEST_ASSERT(LineDrawingCanonicalScene_ExportLayoutToStringWithOptions(
                        layout, "scene_bad_binding_options", &bad_binding_options) == NULL);
    }
    remove(path);
    ld_test_shutdown_runtime();
    return true;
}

bool test_layout_scene_export_run_tests(void) {
    const TestCase cases[] = {
        { "ShapeExportProjectionAxisMapping", test_shape_export_projection_axis_mapping },
        { "SpaceModeToggleContract2DResetsPlaneAndFreeView",
          test_space_mode_toggle_contract_2d_resets_plane_and_free_view },
        { "CanonicalSceneExport2DPayload", test_canonical_scene_export_2d_payload },
        { "CanonicalSceneExport3DPayload", test_canonical_scene_export_3d_payload },
        { "CanonicalSceneExportPreservesExistingExtensions",
          test_canonical_scene_export_preserves_existing_extensions },
        { "CanonicalSceneExportPreservesCameraPathAuthoringMetadata",
          test_canonical_scene_export_preserves_camera_path_authoring_metadata },
        { "CanonicalSceneExportEmitsCanonicalCameraAndLightPaths",
          test_canonical_scene_export_emits_canonical_camera_and_light_paths },
        { "CanonicalSceneExportEmitsPreviewModeAuthoringState",
          test_canonical_scene_export_emits_preview_mode_authoring_state },
        { "CanonicalSceneExportPreservesExistingSceneIdAndCanonicalObjectIds",
          test_canonical_scene_export_preserves_existing_scene_id_and_canonical_object_ids },
        { "CanonicalSceneExportIncludesScene3DAndPrimitivePayloads",
          test_canonical_scene_export_includes_scene3d_and_primitive_payloads },
        { "LayoutFixturePure3DPrimitivesExportContract",
          test_layout_fixture_pure_3d_primitives_export_contract },
        { "LayoutFixtureMixed2D3DExportContract",
          test_layout_fixture_mixed_2d_3d_export_contract },
        { "CanonicalSceneExportAppliesSceneAuthoringOptions",
          test_canonical_scene_export_applies_scene_authoring_options },
        { "CanonicalSceneExportEmitsMaterialBindingAuthoringData",
          test_canonical_scene_export_emits_material_binding_authoring_data },
        { "CanonicalSceneExportUsesLiveSceneAuthoringRecords",
          test_canonical_scene_export_uses_live_scene_authoring_records },
        { "CanonicalSceneExportEmitsMeshAssetInstanceReference",
          test_canonical_scene_export_emits_mesh_asset_instance_reference },
        { "CanonicalSceneExportRejectsInvalidSceneAuthoringOptions",
          test_canonical_scene_export_rejects_invalid_scene_authoring_options }
    };

    return run_test_cases("LayoutSceneExport", cases, sizeof(cases) / sizeof(cases[0]));
}
