#include "Layout/scene/layout_scene_view_packet_consumer.h"

#include <string.h>

bool Layout_SceneViewPacketReadbackFromJsonString(
    const char* json,
    LayoutSceneViewPacketReadback* out_readback) {
    return core_scene_view_packet_readback_from_json_string(json, out_readback).code == CORE_OK;
}

void Layout_SceneViewPacketReadoutInit(LayoutSceneViewPacketReadout* readout) {
    if (!readout) return;
    memset(readout, 0, sizeof(*readout));
    readout->object_kind = OBJECT3D_KIND_UNKNOWN;
    readout->first_face = OBJECT3D_FACE_NONE;
    readout->last_face = OBJECT3D_FACE_NONE;
}

bool Layout_SceneViewPacketMapPlaneFaceGroup(size_t face_group_index,
                                             Object3DFaceKind* out_face) {
    if (out_face) *out_face = OBJECT3D_FACE_NONE;
    if (!out_face || face_group_index != 0u) return false;
    *out_face = OBJECT3D_FACE_PLANE_SURFACE;
    return true;
}

bool Layout_SceneViewPacketMapRectPrismFaceGroup(size_t face_group_index,
                                                 Object3DFaceKind* out_face) {
    static const Object3DFaceKind kFaces[6] = {
        OBJECT3D_FACE_RECT_PRISM_NEG_N,
        OBJECT3D_FACE_RECT_PRISM_POS_N,
        OBJECT3D_FACE_RECT_PRISM_NEG_V,
        OBJECT3D_FACE_RECT_PRISM_POS_V,
        OBJECT3D_FACE_RECT_PRISM_NEG_U,
        OBJECT3D_FACE_RECT_PRISM_POS_U
    };

    if (out_face) *out_face = OBJECT3D_FACE_NONE;
    if (!out_face || face_group_index >= (sizeof(kFaces) / sizeof(kFaces[0]))) return false;
    *out_face = kFaces[face_group_index];
    return true;
}

static bool layout_scene_view_packet_map_face_for_kind(Object3DKind object_kind,
                                                       int face_group_index,
                                                       Object3DFaceKind* out_face) {
    if (out_face) *out_face = OBJECT3D_FACE_NONE;
    if (!out_face || face_group_index < 0) return false;

    switch (object_kind) {
        case OBJECT3D_KIND_PLANE:
            return Layout_SceneViewPacketMapPlaneFaceGroup((size_t)face_group_index,
                                                           out_face);
        case OBJECT3D_KIND_RECT_PRISM:
            return Layout_SceneViewPacketMapRectPrismFaceGroup((size_t)face_group_index,
                                                               out_face);
        case OBJECT3D_KIND_MESH_ASSET_INSTANCE:
        case OBJECT3D_KIND_UNKNOWN:
        default:
            return false;
    }
}

bool Layout_SceneViewPacketReadoutFromJsonString(
    const char* json,
    Object3DKind object_kind,
    LayoutSceneViewPacketReadout* out_readout) {
    LayoutSceneViewPacketReadout readout;
    bool first_mapped = false;
    bool last_mapped = false;

    if (!out_readout) return false;
    Layout_SceneViewPacketReadoutInit(out_readout);
    Layout_SceneViewPacketReadoutInit(&readout);
    readout.object_kind = object_kind;

    if (!Layout_SceneViewPacketReadbackFromJsonString(json, &readout.packet)) {
        return false;
    }

    first_mapped = layout_scene_view_packet_map_face_for_kind(
        object_kind,
        readout.packet.firstPickId.faceGroupIndex,
        &readout.first_face);
    last_mapped = layout_scene_view_packet_map_face_for_kind(
        object_kind,
        readout.packet.lastPickId.faceGroupIndex,
        &readout.last_face);
    readout.face_mapping_supported = first_mapped && last_mapped;
    readout.face_mapping_degraded = !readout.face_mapping_supported;

    *out_readout = readout;
    return true;
}
