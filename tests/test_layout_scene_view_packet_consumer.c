#include "test_layout_internal.h"

#include "Layout/scene/layout_scene_view_packet_consumer.h"

#include <string.h>

static const char* kSceneViewPacketFixture =
    "{"
    "\"schema_family\":\"codework_scene_view\","
    "\"schema_variant\":\"ray_tracing_scene_view_packet_v0\","
    "\"focused_object_index\":4,"
    "\"preview_quality\":\"material_preview\","
    "\"preview_quality_id\":2,"
    "\"degraded_reason\":\"none\","
    "\"degraded_reason_id\":0,"
    "\"projected\":true,"
    "\"complete\":true,"
    "\"triangle_count\":2,"
    "\"face_group_count\":6,"
    "\"triangles\":["
    "{"
    "\"pick_id\":{"
    "\"scene_object_index\":4,"
    "\"primitive_index\":0,"
    "\"triangle_index\":0,"
    "\"local_triangle_index\":0,"
    "\"face_group_index\":0"
    "},"
    "\"p0\":[0,0,0],"
    "\"p1\":[1,0,0],"
    "\"p2\":[1,1,0],"
    "\"screen0\":[10,10],"
    "\"screen1\":[20,10],"
    "\"screen2\":[20,20],"
    "\"depth\":0.5,"
    "\"rgba\":[80,120,200,96],"
    "\"display_flags\":9"
    "},"
    "{"
    "\"pick_id\":{"
    "\"scene_object_index\":4,"
    "\"primitive_index\":0,"
    "\"triangle_index\":11,"
    "\"local_triangle_index\":1,"
    "\"face_group_index\":5"
    "},"
    "\"p0\":[0,0,1],"
    "\"p1\":[1,0,1],"
    "\"p2\":[1,1,1],"
    "\"screen0\":[30,30],"
    "\"screen1\":[40,30],"
    "\"screen2\":[40,40],"
    "\"depth\":0.25,"
    "\"rgba\":[90,130,210,255],"
    "\"display_flags\":0"
    "}"
    "]"
    "}";

static const char* kSceneViewPlanePacketFixture =
    "{"
    "\"schema_family\":\"codework_scene_view\","
    "\"schema_variant\":\"ray_tracing_scene_view_packet_v0\","
    "\"focused_object_index\":1,"
    "\"preview_quality\":\"flat_solid\","
    "\"preview_quality_id\":1,"
    "\"degraded_reason\":\"none\","
    "\"degraded_reason_id\":0,"
    "\"projected\":true,"
    "\"complete\":true,"
    "\"triangle_count\":1,"
    "\"face_group_count\":1,"
    "\"triangles\":["
    "{"
    "\"pick_id\":{"
    "\"scene_object_index\":1,"
    "\"primitive_index\":0,"
    "\"triangle_index\":0,"
    "\"local_triangle_index\":0,"
    "\"face_group_index\":0"
    "},"
    "\"p0\":[0,0,0],"
    "\"p1\":[1,0,0],"
    "\"p2\":[1,1,0],"
    "\"screen0\":[10,10],"
    "\"screen1\":[20,10],"
    "\"screen2\":[20,20],"
    "\"depth\":0.5,"
    "\"rgba\":[80,120,200,255],"
    "\"display_flags\":0"
    "}"
    "]"
    "}";

static bool test_ray_tracing_scene_view_packet_readback_contract(void) {
    LayoutSceneViewPacketReadback readback = {0};

    TEST_ASSERT(Layout_SceneViewPacketReadbackFromJsonString(kSceneViewPacketFixture,
                                                             &readback));
    TEST_ASSERT(readback.valid);
    TEST_ASSERT(readback.focusedObjectIndex == 4);
    TEST_ASSERT(readback.previewQuality == LAYOUT_SCENE_VIEW_PREVIEW_MATERIAL);
    TEST_ASSERT(readback.degradedReason == LAYOUT_SCENE_VIEW_DEGRADED_NONE);
    TEST_ASSERT(readback.projected);
    TEST_ASSERT(readback.complete);
    TEST_ASSERT(readback.triangleCount == 2);
    TEST_ASSERT(readback.faceGroupCount == 6);
    TEST_ASSERT(readback.firstPickId.sceneObjectIndex == 4);
    TEST_ASSERT(readback.firstPickId.faceGroupIndex == 0);
    TEST_ASSERT(readback.lastPickId.triangleIndex == 11);
    TEST_ASSERT(readback.lastPickId.faceGroupIndex == 5);
    TEST_ASSERT(readback.firstAlpha == 96u);
    TEST_ASSERT((readback.firstDisplayFlags & LAYOUT_SCENE_VIEW_DISPLAY_TRANSPARENT) != 0u);
    TEST_ASSERT((readback.firstDisplayFlags & LAYOUT_SCENE_VIEW_DISPLAY_TEXTURED) != 0u);

    return true;
}

static bool test_scene_view_packet_readout_maps_plane_faces(void) {
    LayoutSceneViewPacketReadout readout = {0};

    TEST_ASSERT(Layout_SceneViewPacketReadoutFromJsonString(kSceneViewPlanePacketFixture,
                                                            OBJECT3D_KIND_PLANE,
                                                            &readout));
    TEST_ASSERT(readout.packet.valid);
    TEST_ASSERT(readout.object_kind == OBJECT3D_KIND_PLANE);
    TEST_ASSERT(readout.face_mapping_supported);
    TEST_ASSERT(!readout.face_mapping_degraded);
    TEST_ASSERT(readout.first_face == OBJECT3D_FACE_PLANE_SURFACE);
    TEST_ASSERT(readout.last_face == OBJECT3D_FACE_PLANE_SURFACE);
    TEST_ASSERT(readout.packet.firstPickId.faceGroupIndex == 0);
    TEST_ASSERT(readout.packet.lastPickId.faceGroupIndex == 0);

    return true;
}

static bool test_scene_view_packet_readout_maps_rect_prism_faces(void) {
    LayoutSceneViewPacketReadout readout = {0};

    TEST_ASSERT(Layout_SceneViewPacketReadoutFromJsonString(kSceneViewPacketFixture,
                                                            OBJECT3D_KIND_RECT_PRISM,
                                                            &readout));
    TEST_ASSERT(readout.packet.valid);
    TEST_ASSERT(readout.object_kind == OBJECT3D_KIND_RECT_PRISM);
    TEST_ASSERT(readout.face_mapping_supported);
    TEST_ASSERT(!readout.face_mapping_degraded);
    TEST_ASSERT(readout.first_face == OBJECT3D_FACE_RECT_PRISM_NEG_N);
    TEST_ASSERT(readout.last_face == OBJECT3D_FACE_RECT_PRISM_POS_U);
    TEST_ASSERT(readout.packet.triangleCount == 2);
    TEST_ASSERT(readout.packet.faceGroupCount == 6);

    return true;
}

static bool test_scene_view_packet_readout_degrades_unsupported_mesh_mapping(void) {
    LayoutSceneViewPacketReadout readout = {0};

    TEST_ASSERT(Layout_SceneViewPacketReadoutFromJsonString(kSceneViewPacketFixture,
                                                            OBJECT3D_KIND_MESH_ASSET_INSTANCE,
                                                            &readout));
    TEST_ASSERT(readout.packet.valid);
    TEST_ASSERT(readout.object_kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE);
    TEST_ASSERT(!readout.face_mapping_supported);
    TEST_ASSERT(readout.face_mapping_degraded);
    TEST_ASSERT(readout.first_face == OBJECT3D_FACE_NONE);
    TEST_ASSERT(readout.last_face == OBJECT3D_FACE_NONE);

    TEST_ASSERT(Layout_SceneViewPacketReadoutFromJsonString(kSceneViewPacketFixture,
                                                            OBJECT3D_KIND_UNKNOWN,
                                                            &readout));
    TEST_ASSERT(readout.packet.valid);
    TEST_ASSERT(readout.object_kind == OBJECT3D_KIND_UNKNOWN);
    TEST_ASSERT(!readout.face_mapping_supported);
    TEST_ASSERT(readout.face_mapping_degraded);
    TEST_ASSERT(readout.first_face == OBJECT3D_FACE_NONE);
    TEST_ASSERT(readout.last_face == OBJECT3D_FACE_NONE);

    return true;
}

static bool test_scene_view_packet_readout_does_not_mutate_object(void) {
    Object3D object;
    Object3D before;
    LayoutSceneViewPacketReadout readout = {0};

    memset(&object, 0, sizeof(object));
    object.objectId = 42u;
    object.kind = OBJECT3D_KIND_RECT_PRISM;
    object.transform.position = (Vec3){1.0f, 2.0f, 3.0f};
    object.transform.scale = (Vec3){1.0f, 1.0f, 1.0f};
    object.rectPrism.width = 2.0f;
    object.rectPrism.height = 3.0f;
    object.rectPrism.depth = 4.0f;
    before = object;

    TEST_ASSERT(Layout_SceneViewPacketReadoutFromJsonString(kSceneViewPacketFixture,
                                                            object.kind,
                                                            &readout));
    TEST_ASSERT(readout.face_mapping_supported);
    TEST_ASSERT(memcmp(&object, &before, sizeof(object)) == 0);

    return true;
}

static bool test_scene_view_packet_face_group_mapping_matches_layout_faces(void) {
    Object3DFaceKind face = OBJECT3D_FACE_NONE;

    TEST_ASSERT(Layout_SceneViewPacketMapPlaneFaceGroup(0u, &face));
    TEST_ASSERT(face == OBJECT3D_FACE_PLANE_SURFACE);
    TEST_ASSERT(!Layout_SceneViewPacketMapPlaneFaceGroup(1u, &face));
    TEST_ASSERT(face == OBJECT3D_FACE_NONE);

    TEST_ASSERT(Layout_SceneViewPacketMapRectPrismFaceGroup(0u, &face));
    TEST_ASSERT(face == OBJECT3D_FACE_RECT_PRISM_NEG_N);
    TEST_ASSERT(Layout_SceneViewPacketMapRectPrismFaceGroup(1u, &face));
    TEST_ASSERT(face == OBJECT3D_FACE_RECT_PRISM_POS_N);
    TEST_ASSERT(Layout_SceneViewPacketMapRectPrismFaceGroup(2u, &face));
    TEST_ASSERT(face == OBJECT3D_FACE_RECT_PRISM_NEG_V);
    TEST_ASSERT(Layout_SceneViewPacketMapRectPrismFaceGroup(3u, &face));
    TEST_ASSERT(face == OBJECT3D_FACE_RECT_PRISM_POS_V);
    TEST_ASSERT(Layout_SceneViewPacketMapRectPrismFaceGroup(4u, &face));
    TEST_ASSERT(face == OBJECT3D_FACE_RECT_PRISM_NEG_U);
    TEST_ASSERT(Layout_SceneViewPacketMapRectPrismFaceGroup(5u, &face));
    TEST_ASSERT(face == OBJECT3D_FACE_RECT_PRISM_POS_U);
    TEST_ASSERT(!Layout_SceneViewPacketMapRectPrismFaceGroup(6u, &face));
    TEST_ASSERT(face == OBJECT3D_FACE_NONE);

    return true;
}

static bool test_scene_view_packet_rejects_wrong_schema(void) {
    static const char* bad_schema =
        "{\"schema_family\":\"codework_scene_view\","
        "\"schema_variant\":\"unexpected\","
        "\"triangles\":[]}";
    LayoutSceneViewPacketReadback readback = {0};
    LayoutSceneViewPacketReadout readout = {0};

    TEST_ASSERT(!Layout_SceneViewPacketReadbackFromJsonString(bad_schema, &readback));
    TEST_ASSERT(!readback.valid);
    TEST_ASSERT(!Layout_SceneViewPacketReadoutFromJsonString(bad_schema,
                                                             OBJECT3D_KIND_RECT_PRISM,
                                                             &readout));
    TEST_ASSERT(!readout.packet.valid);
    TEST_ASSERT(!readout.face_mapping_supported);
    TEST_ASSERT(!readout.face_mapping_degraded);
    return true;
}

bool test_layout_scene_view_packet_consumer_run_tests(void) {
    const TestCase cases[] = {
        {"RayTracingSceneViewPacketReadbackContract",
         test_ray_tracing_scene_view_packet_readback_contract},
        {"SceneViewPacketReadoutMapsPlaneFaces",
         test_scene_view_packet_readout_maps_plane_faces},
        {"SceneViewPacketReadoutMapsRectPrismFaces",
         test_scene_view_packet_readout_maps_rect_prism_faces},
        {"SceneViewPacketReadoutDegradesUnsupportedMeshMapping",
         test_scene_view_packet_readout_degrades_unsupported_mesh_mapping},
        {"SceneViewPacketReadoutDoesNotMutateObject",
         test_scene_view_packet_readout_does_not_mutate_object},
        {"SceneViewPacketFaceGroupMappingMatchesLayoutFaces",
         test_scene_view_packet_face_group_mapping_matches_layout_faces},
        {"SceneViewPacketRejectsWrongSchema",
         test_scene_view_packet_rejects_wrong_schema},
    };
    return run_test_cases("LayoutSceneViewPacketConsumer",
                          cases,
                          sizeof(cases) / sizeof(cases[0]));
}
