#include "Core/camera_path.h"
#include "Core/camera_view.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Input/input_mouse_internal.h"
#include "Input/input_viewport_navigation.h"
#include "UI/ui_panel.h"
#include "UI/workspace_authoring/line_drawing_workspace_authoring_host.h"
#include "Layout/scene/layout_mesh_solid_preview.h"
#include <stdio.h>
#include <string.h>

void CameraView_Basis(const CameraViewSession *c, Vec3 *forward, Vec3 *right, Vec3 *up) {
    FreeViewCamera camera = {.enabled = true, .yawDeg = c->yaw, .pitchDeg = c->pitch};
    Vec3 r = FreeView_Right(&camera), u = FreeView_Up(&camera);
    float angle = DegToRad(c->roll);
    if (forward)
        *forward = FreeView_Forward(&camera);
    if (right)
        *right = Vec3_Add(Vec3_Scale(r, cosf(angle)), Vec3_Scale(u, -sinf(angle)));
    if (up)
        *up = Vec3_Add(Vec3_Scale(u, cosf(angle)), Vec3_Scale(r, sinf(angle)));
}
void CameraView_RefreshViewport(GlobalState *state) {
    if (!state)
        return;
    CorePaneRect rect = {0, 0, (float)state->screenWidth, (float)state->screenHeight};
    (void)LineDrawingPaneHost_GetViewportRect(&state->paneHost, &rect);
    state->cameraView.viewport[0] = rect.x;
    state->cameraView.viewport[1] = rect.y;
    state->cameraView.viewport[2] = rect.width;
    state->cameraView.viewport[3] = rect.height;
}
void CameraView_ResetInput(GlobalState *state) {
    if (!state)
        return;
    state->cameraView.held = 0;
    state->cameraView.focused = false;
}
void CameraView_ReconcileHistory(GlobalState *state) {
    if (!state)
        return;
    CameraViewSession *c = &state->cameraView;
    size_t count = state->layout.sceneAuthoring.camera_count;
    if (c->selected >= count)
        c->selected = count ? count - 1 : 0;
    if (c->source_id[0] && !Layout_SceneAuthoringState_FindCameraByIdConst(
                               &state->layout.sceneAuthoring, c->source_id))
        c->source_id[0] = 0;
    c->message[0] = 0;
    CameraPath_Reconcile(state);
    CameraView_ResetInput(state);
}
bool CameraView_Enter(GlobalState *state, bool from_saved) {
    if (!state || state->workspaceMode != LINE_DRAWING_WORKSPACE_MODE_SCENE ||
        LineDrawingWorkspaceAuthoringHost_Active(state))
        return false;
    CameraViewSession *c = &state->cameraView;
    if(c->active && c->path_modified) {
        snprintf(c->message,sizeof(c->message),"Apply or Revert the draft first."); return false;
    }
    c->path_source[0]=0; c->anchor_source[0]=0; c->path_playing=false; c->path_modified=false;
    if (!c->active) {
        c->previous_section = state->sectionView;
        c->previous_2d = state->spaceMode == SPACE_MODE_2D;
    }
    c->active = true;
    c->eye = (Vec3){0, 0, (float)(1.6 / state->layout.metersPerWorldUnit)};
    c->yaw = -90;
    c->pitch = 0;
    c->roll = 0;
    c->fov = 65;
    c->near_clip = .01f;
    c->far_clip = 250;
    c->speed_mps = .5f;
    c->source_id[0] = 0;
    c->message[0] = 0;
    if (from_saved && c->selected < state->layout.sceneAuthoring.camera_count) {
        const LineDrawingSceneCamera *camera = &state->layout.sceneAuthoring.cameras[c->selected];
        const LineDrawingScenePath *path = NULL;
        for (size_t i = 0; i < state->layout.sceneAuthoring.path_count; ++i) {
            LineDrawingScenePath *candidate = &state->layout.sceneAuthoring.paths[i];
            if (!strcmp(candidate->path_id, camera->path_id)) {
                path = candidate;
                candidate->playing = false;
                break;
            }
        }
        LineDrawingSceneCameraPose pose;
        if (Layout_SceneCamera_EvaluatePoseAtNormalizedDistance(
                camera, path, path ? path->normalized_distance : 0, &pose)) {
            c->eye = pose.position;
            c->yaw = RadToDeg(atan2f(pose.forward.y, pose.forward.x));
            c->pitch = RadToDeg(asinf(fmaxf(-1, fminf(1, pose.forward.z))));
            FreeViewCamera basis = {.enabled = true, .yawDeg = c->yaw, .pitchDeg = c->pitch};
            c->roll = RadToDeg(atan2f(Vec3_Dot(pose.up, FreeView_Right(&basis)),
                                      Vec3_Dot(pose.up, FreeView_Up(&basis))));
            c->fov = camera->vertical_fov_degrees;
            c->near_clip = camera->near_clip;
            c->far_clip = camera->far_clip;
            snprintf(c->source_id, sizeof(c->source_id), "%s", camera->camera_id);
        }
    }
    CameraView_RefreshViewport(state);
    state->spaceMode = SPACE_MODE_3D;
    state->sectionView.mode = LAYOUT_SECTION_OFF;
    InputViewportNavigation_ResetGesture();
    CameraView_ResetInput(state);
    Global_FlagHitboxesDirty();
    return true;
}
void CameraView_Exit(GlobalState *state) {
    if (!state || !state->cameraView.active)
        return;
    state->sectionView = state->cameraView.previous_section;
    state->spaceMode = state->cameraView.previous_2d ? SPACE_MODE_2D : SPACE_MODE_3D;
    state->cameraView.active = false;
    CameraPath_Stop(state);
    state->cameraView.path_source[0]=0; state->cameraView.anchor_source[0]=0;
    state->cameraView.path_modified=false; state->cameraView.path_signature=0;
    CameraView_ResetInput(state);
    Global_FlagHitboxesDirty();
}
bool CameraView_Save(GlobalState *state, bool update) {
    if (!state || !state->cameraView.active)
        return false;
    CameraViewSession *c = &state->cameraView;
    LineDrawingSceneAuthoringState *a = &state->layout.sceneAuthoring;
    LineDrawingSceneCamera *camera =
        update ? Layout_SceneAuthoringState_FindCameraById(a, c->source_id) : NULL;
    if (update && (!camera || camera->path_id[0])) {
        snprintf(c->message, sizeof(c->message),
                 "Path cameras: use Save new to preserve the path.");
        return false;
    }
    if (!camera && a->camera_count >= LINE_DRAWING_SCENE_AUTHORING_MAX_CAMERAS) {
        snprintf(c->message, sizeof(c->message),
                 "Camera limit reached; update a standalone camera.");
        return false;
    }
    if (!Editor_TryHistoryCapture(&state->editor, &state->layout))
        return false;
    if (!camera) {
        char id[64];
        unsigned n = 1;
        do {
            snprintf(id, sizeof(id), "viewpoint_%u", n++);
        } while (Layout_SceneAuthoringState_FindCameraById(a, id));
        c->selected = a->camera_count;
        camera = &a->cameras[a->camera_count++];
        Layout_SceneCamera_SetDefaults(camera, id, id, "");
    }
    camera->position = c->eye;
    CameraView_Basis(c, &camera->fixed_forward, NULL, NULL);
    camera->look_at_target = Vec3_Add(c->eye, camera->fixed_forward);
    camera->orientation_mode = LINE_DRAWING_SCENE_CAMERA_ORIENTATION_FIXED;
    camera->roll_degrees = c->roll;
    camera->vertical_fov_degrees = c->fov;
    camera->near_clip = c->near_clip;
    camera->far_clip = c->far_clip;
    snprintf(c->source_id, sizeof(c->source_id), "%s", camera->camera_id);
    snprintf(c->message, sizeof(c->message), "Viewpoint added. Save scene to keep it.");
    Global_FlagLayoutChanged();
    return true;
}
bool CameraView_Step(GlobalState *state, float seconds) {
    if (!state || !state->cameraView.active || !state->cameraView.focused ||
        !state->cameraView.held || !isfinite(seconds) || seconds <= 0)
        return false;
    CameraViewSession *c = &state->cameraView;
    /* Cap wake delta: resuming a sleeping or stalled app must not teleport. */
    seconds = fminf(seconds, .05f);
    FreeViewCamera heading = {.enabled = true, .yawDeg = c->yaw};
    Vec3 f = FreeView_Forward(&heading), r = FreeView_Right(&heading), d = {0};
    if (c->held & 1)
        d = Vec3_Add(d, f);
    if (c->held & 2)
        d = Vec3_Sub(d, f);
    if (c->held & 4)
        d = Vec3_Sub(d, r);
    if (c->held & 8)
        d = Vec3_Add(d, r);
    if (c->held & 16)
        d.z -= 1;
    if (c->held & 32)
        d.z += 1;
    if (Vec3_Length(d) < 1e-6f)
        return false;
    CameraPath_NavigationChanged(state);
    c->eye =
        Vec3_Add(c->eye, Vec3_Scale(Vec3_Normalize(d), (float)(c->speed_mps * seconds /
                                                               state->layout.metersPerWorldUnit)));
    Global_FlagHitboxesDirty();
    return true;
}
int CameraView_NextUpdateDelayMs(const GlobalState *state) {
    return state && state->cameraView.active && ((state->cameraView.focused && state->cameraView.held) || state->cameraView.path_playing)
               ? 16
               : -1;
}
static unsigned key_bit(SDL_Keycode key) {
    switch (key) {
    case SDLK_w:
        return 1;
    case SDLK_s:
        return 2;
    case SDLK_a:
        return 4;
    case SDLK_d:
        return 8;
    case SDLK_q:
        return 16;
    case SDLK_e:
        return 32;
    default:
        return 0;
    }
}
bool CameraView_HandleEvent(GlobalState *state, const SDL_Event *e) {
    if (!state || !e || !state->cameraView.active)
        return false;
    CameraViewSession *c = &state->cameraView;
    if (state->workspaceMode != LINE_DRAWING_WORKSPACE_MODE_SCENE ||
        state->spaceMode != SPACE_MODE_3D) {
        CameraView_Exit(state);
        return false;
    }
    if (e->type == SDL_WINDOWEVENT &&
        (e->window.event == SDL_WINDOWEVENT_FOCUS_LOST || e->window.event == SDL_WINDOWEVENT_LEAVE))
        { CameraView_ResetInput(state); CameraPath_Stop(state); }
    /* Release is always handled, including after moving over a text field. */
    if (e->type == SDL_KEYUP && key_bit(e->key.keysym.sym)) {
        c->held &= ~key_bit(e->key.keysym.sym);
        return true;
    }
    if (UIPanel_IsCapturingKeyboard() || LineDrawingWorkspaceAuthoringHost_Active(state)) {
        CameraView_ResetInput(state);
        return false;
    }
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) {
            CameraView_ResetInput(state);
            return false;
        }
        if (e->key.keysym.sym == SDLK_ESCAPE) {
            CameraView_Exit(state);
            return true;
        }
        if (key_bit(e->key.keysym.sym)) {
            if (c->focused)
                c->held |= key_bit(e->key.keysym.sym);
            return true;
        }
        /* Observer mode cannot leak editing shortcuts into geometry. */
        return true;
    }
    if (e->type == SDL_MOUSEMOTION) {
        c->focused = ResolvePointerPaneLane(e->motion.x, e->motion.y) == POINTER_PANE_CENTER;
        if (!c->focused) {
            c->held = 0;
            return false;
        }
        if (SDL_GetModState() & KMOD_ALT) {
            CameraPath_NavigationChanged(state);
            if (c->roll_mouse || (SDL_GetModState() & KMOD_SHIFT))
                c->roll = Angle_NormalizeSignedDeg(c->roll + e->motion.xrel * .25f);
            else {
                c->yaw = Angle_NormalizeSignedDeg(c->yaw - e->motion.xrel * .25f);
                c->pitch = fmaxf(-89.5f, fminf(89.5f, c->pitch - e->motion.yrel * .25f));
            }
            Global_FlagHitboxesDirty();
        }
        return true;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
        if (ResolvePointerPaneLane(e->button.x, e->button.y) != POINTER_PANE_CENTER) {
            CameraView_ResetInput(state);
            return false;
        }
        c->focused = true;
        if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT &&
            !(SDL_GetModState() & KMOD_ALT)) {
            SpaceViewContext view = SpaceAdapter_BuildViewContext(state);
            uint32_t id = 0;
            if (Layout_SolidPreviewPick(&state->layout, &view, &state->grid, e->button.x,
                                        e->button.y, &id)) {
                state->editor.selectedObject3DId = id;
                Global_FlagHitboxesDirty();
            }
        }
        return true;
    }
    if (e->type == SDL_MOUSEWHEEL)
        return c->focused; /* No orthographic zoom in physical view. */
    return false;
}

void CameraView_RenderMarkers(SDL_Renderer *renderer, const GlobalState *state) {
    if (!renderer || !state || state->cameraView.active || state->inspectionView ||
        state->workspaceMode != LINE_DRAWING_WORKSPACE_MODE_SCENE ||
        state->sectionView.mode != LAYOUT_SECTION_OFF)
        return;
    SpaceViewContext view = SpaceAdapter_BuildViewContext(state);
    for (size_t i = 0; i < state->layout.sceneAuthoring.camera_count; ++i) {
        const LineDrawingSceneCamera *c = &state->layout.sceneAuthoring.cameras[i];
        if (c->path_id[0])
            continue; /* Path cameras keep their existing authoring handles. */
        LineDrawingSceneCameraPose pose;
        if (!Layout_SceneCamera_EvaluatePose(c, NULL, &pose))
            continue;
        Vec2 p = WorldToScreen(SpaceAdapter_ProjectToView(pose.position, &view), &state->grid);
        Vec2 q = WorldToScreen(
            SpaceAdapter_ProjectToView(Vec3_Add(pose.position, pose.forward), &view), &state->grid);
        float dx = q.x - p.x, dy = q.y - p.y, len = sqrtf(dx * dx + dy * dy);
        SDL_SetRenderDrawColor(renderer, 180, 150, 240, 255);
        SDL_Rect body = {(int)p.x - 6, (int)p.y - 5, 12, 10};
        SDL_RenderDrawRect(renderer, &body);
        if (len > 1e-5f) {
            dx = dx / len * 24;
            dy = dy / len * 24;
            SDL_RenderDrawLine(renderer, (int)p.x, (int)p.y, (int)(p.x + dx), (int)(p.y + dy));
            SDL_RenderDrawLine(renderer, (int)(p.x + dx), (int)(p.y + dy),
                               (int)(p.x + dx * .7f - dy * .15f),
                               (int)(p.y + dy * .7f + dx * .15f));
            SDL_RenderDrawLine(renderer, (int)(p.x + dx), (int)(p.y + dy),
                               (int)(p.x + dx * .7f + dy * .15f),
                               (int)(p.y + dy * .7f - dx * .15f));
        }
    }
}
