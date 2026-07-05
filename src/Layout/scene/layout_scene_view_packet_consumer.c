#include "Layout/scene/layout_scene_view_packet_consumer.h"

bool Layout_SceneViewPacketReadbackFromJsonString(
    const char* json,
    LayoutSceneViewPacketReadback* out_readback) {
    return core_scene_view_packet_readback_from_json_string(json, out_readback).code == CORE_OK;
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
