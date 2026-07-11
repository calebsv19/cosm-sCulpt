#include "UI/ui_panel.h"

#include "Core/global_state.h"
#include "Editor/editor.h"

#include <SDL2/SDL_log.h>

static void UIPanel_ClearEditorSelectionForSceneAuthoring(GlobalState* state) {
    EditorState* editor = NULL;
    if (!state) return;
    editor = &state->editor;
    Editor_ClearAnchorSelection(editor);
    editor->selectedObject3DId = 0u;
    editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
    editor->selectedWallIndex = -1;
    editor->selectedHandleAnchor = -1;
    editor->selectedHandleComponent = -1;
    editor->hoveredObject3DId = 0u;
    Global_FlagHitboxesDirty();
}

bool UIPanel_CreateSceneAuthoringLight(void) {
    GlobalState* state = Global_Get();
    size_t index = 0u;
    if (!state) return false;
    if (!Layout_SceneAuthoringState_AddDefaultLight(&state->layout.sceneAuthoring, &index)) {
        SDL_Log("[UI] Light creation blocked: authoring light limit reached.");
        return false;
    }
    UIPanel_ClearEditorSelectionForSceneAuthoring(state);
    SDL_Log("[UI] Scene authoring light created (%s)",
            state->layout.sceneAuthoring.lights[index].light_id);
    return true;
}

bool UIPanel_CreateSceneAuthoringCameraPath(void) {
    GlobalState* state = Global_Get();
    size_t index = 0u;
    if (!state) return false;
    if (!Layout_SceneAuthoringState_AddDefaultCameraPath(&state->layout.sceneAuthoring, &index)) {
        SDL_Log("[UI] Camera path creation blocked: authoring path limit reached.");
        return false;
    }
    UIPanel_ClearEditorSelectionForSceneAuthoring(state);
    SDL_Log("[UI] Scene authoring camera path created (%s)",
            state->layout.sceneAuthoring.camera_paths[index].path_id);
    return true;
}

bool UIPanel_CreateSceneAuthoringMaterial(void) {
    GlobalState* state = Global_Get();
    size_t index = 0u;
    if (!state) return false;
    if (!Layout_SceneAuthoringState_AddDefaultMaterial(&state->layout.sceneAuthoring, &index)) {
        SDL_Log("[UI] Material creation blocked: authoring material limit reached.");
        return false;
    }
    UIPanel_ClearEditorSelectionForSceneAuthoring(state);
    SDL_Log("[UI] Scene authoring material created (%s)",
            state->layout.sceneAuthoring.materials[index].material_id);
    return true;
}
