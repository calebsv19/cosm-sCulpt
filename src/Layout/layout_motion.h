#pragma once
#include "Layout/layout_spatial.h"

/* Read-only isolated solver sample. B only; downstream parts/assemblies are not enveloped. */
bool Layout_SampleMotion(const Layout* layout, const char* rule_id, double position, Object3D* pose);
const LayoutMotionEnvelope* Layout_FindMotionEnvelope(const LayoutObjectStore* store, uint32_t object_id);
const LayoutMotionEnvelope* Layout_FindRuleEnvelope(const LayoutObjectStore* store, const char* rule_id);
/* Fixed world AABB, padded to cover all intermediate hinge poses. Existing envelope
 * identity is retained on regeneration; one command owns one undo snapshot. */
bool Layout_GenerateMotionEnvelope(Layout* layout, const char* rule_id, uint32_t samples,
    LayoutGeometryBeforePublish history, void* context);
bool Layout_MotionEnvelopeCurrent(const Layout* layout, const LayoutMotionEnvelope* envelope);
bool Layout_ValidateMotionEnvelopes(const Layout* layout, char* message, size_t capacity);
bool Layout_MotionWriteJson(const Layout* layout, cJSON* engineering);
bool Layout_MotionReadJson(Layout* layout, const cJSON* engineering, bool required);
