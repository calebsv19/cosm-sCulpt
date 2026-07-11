#pragma once

#include "Layout/layout.h"
#include "core_scene_view.h"

#include <stdbool.h>
#include <stddef.h>

#define LAYOUT_SCENE_VIEW_PACKET_SCHEMA_FAMILY CORE_SCENE_VIEW_SCHEMA_FAMILY
#define LAYOUT_SCENE_VIEW_PACKET_RAY_TRACING_VARIANT CORE_SCENE_VIEW_PACKET_RAY_TRACING_VARIANT

typedef CoreSceneViewPreviewQuality LayoutSceneViewPreviewQuality;

#define LAYOUT_SCENE_VIEW_PREVIEW_OUTLINE CORE_SCENE_VIEW_PREVIEW_OUTLINE
#define LAYOUT_SCENE_VIEW_PREVIEW_FLAT_SOLID CORE_SCENE_VIEW_PREVIEW_FLAT_SOLID
#define LAYOUT_SCENE_VIEW_PREVIEW_MATERIAL CORE_SCENE_VIEW_PREVIEW_MATERIAL
#define LAYOUT_SCENE_VIEW_PREVIEW_DIAGNOSTIC_OVERLAY \
    CORE_SCENE_VIEW_PREVIEW_DIAGNOSTIC_OVERLAY

typedef CoreSceneViewDegradedReason LayoutSceneViewDegradedReason;

#define LAYOUT_SCENE_VIEW_DEGRADED_NONE CORE_SCENE_VIEW_DEGRADED_NONE
#define LAYOUT_SCENE_VIEW_DEGRADED_NO_PRIMITIVE_SEED_STATE \
    CORE_SCENE_VIEW_DEGRADED_NO_PRIMITIVE_SEED_STATE
#define LAYOUT_SCENE_VIEW_DEGRADED_OBJECT_NOT_FOUND \
    CORE_SCENE_VIEW_DEGRADED_OBJECT_NOT_FOUND
#define LAYOUT_SCENE_VIEW_DEGRADED_TRIANGLE_CAP_REACHED \
    CORE_SCENE_VIEW_DEGRADED_TRIANGLE_CAP_REACHED
#define LAYOUT_SCENE_VIEW_DEGRADED_PROJECTION_UNAVAILABLE \
    CORE_SCENE_VIEW_DEGRADED_PROJECTION_UNAVAILABLE

typedef CoreSceneViewDisplayFlags LayoutSceneViewDisplayFlags;

#define LAYOUT_SCENE_VIEW_DISPLAY_TRANSPARENT CORE_SCENE_VIEW_DISPLAY_TRANSPARENT
#define LAYOUT_SCENE_VIEW_DISPLAY_EMISSIVE CORE_SCENE_VIEW_DISPLAY_EMISSIVE
#define LAYOUT_SCENE_VIEW_DISPLAY_MIRROR CORE_SCENE_VIEW_DISPLAY_MIRROR
#define LAYOUT_SCENE_VIEW_DISPLAY_TEXTURED CORE_SCENE_VIEW_DISPLAY_TEXTURED

typedef CoreSceneViewPickId LayoutSceneViewPickId;
typedef CoreSceneViewPacketReadback LayoutSceneViewPacketReadback;

typedef struct LayoutSceneViewPacketReadout {
    LayoutSceneViewPacketReadback packet;
    Object3DKind object_kind;
    bool face_mapping_supported;
    bool face_mapping_degraded;
    Object3DFaceKind first_face;
    Object3DFaceKind last_face;
} LayoutSceneViewPacketReadout;

bool Layout_SceneViewPacketReadbackFromJsonString(
    const char* json,
    LayoutSceneViewPacketReadback* out_readback);

void Layout_SceneViewPacketReadoutInit(LayoutSceneViewPacketReadout* readout);
bool Layout_SceneViewPacketReadoutFromJsonString(
    const char* json,
    Object3DKind object_kind,
    LayoutSceneViewPacketReadout* out_readout);

bool Layout_SceneViewPacketMapPlaneFaceGroup(size_t face_group_index,
                                             Object3DFaceKind* out_face);
bool Layout_SceneViewPacketMapRectPrismFaceGroup(size_t face_group_index,
                                                 Object3DFaceKind* out_face);
