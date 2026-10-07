#pragma once
#include "Layout/scene/layout_scene_camera_authoring.h"
#include "Layout/layout_section.h"
#include <SDL2/SDL.h>
#include <stdint.h>

typedef struct GlobalState GlobalState;
typedef struct {
    bool active, focused, roll_mouse, previous_2d;
    unsigned held;
    size_t selected;
    char source_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    Vec3 eye;
    float viewport[4]; /* Solved canvas rect; projection never links UI into scene tools. */
    float yaw, pitch, roll, fov, near_clip, far_clip, speed_mps;
    LayoutSectionView previous_section;
    char message[128];
    /* Path session is declared separately to keep scene tools independent of UI. */
    char path_source[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    char anchor_source[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    size_t path_anchor;
    uint64_t path_signature;
    bool path_playing, path_modified;
    float path_time;
} CameraViewSession;

bool CameraView_Enter(GlobalState *state, bool from_saved);
void CameraView_Exit(GlobalState *state);
void CameraView_RefreshViewport(GlobalState *state);
void CameraView_ResetInput(GlobalState *state);
/* History can remove a saved source without changing the temporary observer pose. */
void CameraView_ReconcileHistory(GlobalState *state);
bool CameraView_Save(GlobalState *state, bool update);
void CameraView_Basis(const CameraViewSession *view, Vec3 *forward, Vec3 *right, Vec3 *up);
bool CameraView_Step(GlobalState *state, float seconds);
int CameraView_NextUpdateDelayMs(const GlobalState *state);
void CameraView_RenderMarkers(SDL_Renderer *renderer, const GlobalState *state);
bool CameraView_HandleEvent(GlobalState *state, const SDL_Event *event);
