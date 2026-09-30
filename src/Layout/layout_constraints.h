#pragma once
#include "Layout/layout_reference.h"
#include "cjson/cJSON.h"

typedef bool (*LayoutGeometryMutation)(Layout* candidate, void* context);
typedef bool (*LayoutGeometryBeforePublish)(const Layout* original, void* context);
/* Full object-store candidate transaction. No side effects until validation and
 * optional history reservation succeed. Raw callbacks may only edit object/rule data; never resize the store. */
bool Layout_RunGeometryEdit(Layout* layout, uint32_t edited_id,
    LayoutGeometryMutation mutate, void* context,
    LayoutGeometryBeforePublish before_publish, void* history_context);
bool Layout_ReplaceGeometryObject(Layout* layout, const Object3D* object,
    LayoutGeometryBeforePublish before_publish, void* context);
bool Layout_ConstraintEdit(Layout* layout, const LayoutConstraint* rule, const char* remove_id,
    LayoutGeometryBeforePublish before_publish, void* context);
/* Initialize a travel rule from current references. Captures transverse separation
 * and orientation without changing geometry; target/home start at the current pose. */
bool Layout_InitLinearTravel(const Layout* layout, LayoutConstraint* rule,
    double minimum_meters, double maximum_meters);
/* Capture a single-plane hinge. Directions must be coplanar, limits in (-180,180].
 * Creation joins the authored pivots; home is the current relative angle. */
bool Layout_InitAngularTravel(const Layout* layout, LayoutConstraint* rule,
    double minimum_degrees, double maximum_degrees);
/* Position is meters for linear travel and degrees for angular travel. */
bool Layout_SetTravelPosition(Layout* layout, const char* rule_id, double position,
    LayoutGeometryBeforePublish before_publish, void* context);
typedef struct LayoutConstraintFeedback {
    bool satisfied;
    LayoutMeasurementResult position;
    LayoutMeasurementResult angle;
} LayoutConstraintFeedback;
LayoutConstraintFeedback Layout_ConstraintFeedback(const Layout* layout, const LayoutConstraint* rule);
bool Layout_ValidateConstraints(const Layout* layout, char* message, size_t capacity);
bool Layout_CanDeleteObject(const LayoutObjectStore* store, uint32_t id);
bool Layout_HasConstraintParticipant(const LayoutObjectStore* store, uint32_t id);
/* Existing active-layout mutators use this hook for constrained edit history. */
void Layout_SetGeometryHistoryHook(LayoutGeometryBeforePublish hook);
bool Layout_GeometryHistory(const Layout* layout, void* context);
bool Layout_ConstraintsWriteJson(const Layout* layout, cJSON* root);
bool Layout_ConstraintsReadJson(Layout* layout, const cJSON* root, bool required);

/* One accepted drag gesture owns one snapshot; failed/no-op increments own none. */
void Layout_BeginGeometryGesture(Layout* layout);
void Layout_EndGeometryGesture(Layout* layout);
