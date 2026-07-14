#pragma once

#include "Layout/scene/layout_scene_authoring.h"

/* Initializes a renderer-neutral editable light record with stable P5 defaults. */
void Layout_SceneLight_SetDefaults(LineDrawingSceneLight* light,
                                   const char* light_id,
                                   const char* label);

/* Converts light position-mode values to and from persistent names. */
const char* Layout_SceneLightPositionMode_Name(LineDrawingSceneLightPositionMode mode);
bool Layout_SceneLightPositionMode_FromName(const char* name,
                                           LineDrawingSceneLightPositionMode* out_mode);

/* Converts renderer-neutral falloff intent to and from persistent names. */
const char* Layout_SceneLightFalloff_Name(LineDrawingSceneLightFalloff falloff);
bool Layout_SceneLightFalloff_FromName(const char* name,
                                      LineDrawingSceneLightFalloff* out_falloff);

/* Evaluates the authoring position while keeping traversal beyond the path start deferred. */
Vec3 Layout_SceneLight_EffectivePosition(const LineDrawingSceneLight* light,
                                         const LineDrawingScenePath* path);
bool Layout_SceneLight_EvaluatePositionAtNormalizedDistance(
    const LineDrawingSceneLight* light,
    const LineDrawingScenePath* path,
    float normalized_distance,
    Vec3* out_position);

/* Resolves the direction from the persistent world-space aim target. */
Vec3 Layout_SceneLight_EffectiveDirection(const LineDrawingSceneLight* light,
                                          const LineDrawingScenePath* path);

/* Returns or updates the spatial aim endpoint represented by the light direction. */
Vec3 Layout_SceneLight_AimPoint(const LineDrawingSceneLight* light,
                               const LineDrawingScenePath* path);
bool Layout_SceneLight_SetAimPoint(LineDrawingSceneLight* light,
                                  const LineDrawingScenePath* path,
                                  Vec3 aim_point);

/* Cycles bounded inspector presets for the currently selected light. */
bool Layout_SceneAuthoringState_CycleSelectedLightPositionMode(
    LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedLightColor(
    LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedLightIntensity(
    LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedLightRadiusOrSize(
    LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedLightCone(
    LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedLightFalloff(
    LineDrawingSceneAuthoringState* state);
