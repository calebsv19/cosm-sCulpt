#pragma once
#include "Layout/layout_spatial.h"

/* Read-only isolated solver sample of B, including the saved assembly scope. */
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
bool Layout_MotionReadJson(Layout* layout, const cJSON* engineering, int schema_version);
/* Explicit rigid scope. Only design primitives without other member rules are supported. */
bool Layout_MotionScopeContains(const LayoutObjectStore* store, const LayoutConstraint* rule, const Object3D* object);
bool Layout_ValidateMotionScopes(const Layout* layout, char* message, size_t capacity);
bool Layout_ApplyMotionScope(Layout* candidate, const LayoutConstraint* rule, const Object3D* before_b);
bool Layout_SampleMotionSet(const Layout* layout, const char* rule_id, double position,
    Object3D* poses, size_t capacity, size_t* count);
typedef struct {
    bool hit;
    uint32_t sample_index, tested_samples;
    double position, gap_meters;
    char member_id[64];
} LayoutMotionInspection;
/* A sampled contact/clearance inspection, never continuous collision proof. */
bool Layout_InspectMotionObstruction(const Layout* layout, const LayoutMotionEnvelope* envelope,
    const char* target_id, double clearance_meters, bool clearance, LayoutMotionInspection* inspection);
