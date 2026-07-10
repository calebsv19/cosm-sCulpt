#include "Core/workspace/line_drawing_workspace_mode_handoff.h"

#include "Editor/editor.h"
#include "Layout/layout.h"
#include "Layout/asset/layout_object_asset_mesh_authoring.h"
#include "Layout/layout_json.h"
#include "Layout/scene/layout_object_faces.h"
#include "Core/workspace/line_drawing_object_workspace_view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static char* WorkspaceHandoff_DupString(const char* text) {
    size_t len = 0u;
    char* copy = NULL;
    if (!text) return NULL;
    len = strlen(text);
    copy = (char*)malloc(len + 1u);
    if (!copy) return NULL;
    memcpy(copy, text, len + 1u);
    return copy;
}

static void WorkspaceHandoff_SetPath(char* dst, size_t dst_size, const char* src) {
    size_t len = 0u;
    if (!dst || dst_size == 0u) return;
    if (!src) src = "";
    len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1u;
    }
    if (len > 0u && dst != src) {
        memcpy(dst, src, len);
    }
    dst[len] = '\0';
}

static bool WorkspaceHandoff_PathIsRegularFile(const char* path) {
    struct stat st = {0};
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static bool WorkspaceHandoff_CopyParentDirectoryPath(const char* path,
                                                     char* out_dir,
                                                     size_t out_dir_size) {
    const char* slash = NULL;
    size_t len = 0u;
    if (!path || !path[0] || !out_dir || out_dir_size == 0u) return false;
    slash = strrchr(path, '/');
    if (!slash || slash == path) return false;
    len = (size_t)(slash - path);
    if (len >= out_dir_size) len = out_dir_size - 1u;
    memcpy(out_dir, path, len);
    out_dir[len] = '\0';
    return out_dir[0] != '\0';
}

static bool WorkspaceHandoff_DeriveSourceAssetPathFromMeshInstance(
    const MeshAssetInstance3D* mesh,
    char* out_path,
    size_t out_path_size) {
    const char* runtime_suffix = ".runtime.json";
    const size_t runtime_suffix_len = strlen(runtime_suffix);
    size_t runtime_len = 0u;
    char dir[LINE_DRAWING_PATH_CAP];
    if (!mesh || !out_path || out_path_size == 0u) return false;
    out_path[0] = '\0';

    if (mesh->runtimePath[0]) {
        runtime_len = strlen(mesh->runtimePath);
        if (runtime_len > runtime_suffix_len &&
            strcasecmp(mesh->runtimePath + runtime_len - runtime_suffix_len,
                       runtime_suffix) == 0) {
            const size_t prefix_len = runtime_len - runtime_suffix_len;
            if (prefix_len + 5u < out_path_size) {
                memcpy(out_path, mesh->runtimePath, prefix_len);
                memcpy(out_path + prefix_len, ".json", 6u);
                if (WorkspaceHandoff_PathIsRegularFile(out_path)) {
                    return true;
                }
            }
        }
    }

    if (mesh->runtimePath[0] &&
        mesh->sourceAssetId[0] &&
        WorkspaceHandoff_CopyParentDirectoryPath(mesh->runtimePath,
                                                 dir,
                                                 sizeof(dir)) &&
        snprintf(out_path,
                 out_path_size,
                 "%s/%s.json",
                 dir,
                 mesh->sourceAssetId) < (int)out_path_size &&
        WorkspaceHandoff_PathIsRegularFile(out_path)) {
        return true;
    }

    out_path[0] = '\0';
    return false;
}

static void WorkspaceHandoff_DocumentStateFree(LineDrawingWorkspaceDocumentState* doc) {
    if (!doc) return;
    Layout_FreeString(doc->layoutSnapshot);
    doc->layoutSnapshot = NULL;
    free(doc->savedSnapshot);
    doc->savedSnapshot = NULL;
    doc->layoutDirtySinceSave = false;
    doc->hasViewportState = false;
    doc->workspaceSourceSceneObjectId = 0u;
    doc->selectedObjectId = 0u;
    doc->selectedAssetBodyId = 0u;
    doc->selectedAssetFace = OBJECT3D_FACE_NONE;
    memset(&doc->grid, 0, sizeof(doc->grid));
    doc->activePlane = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f };
    doc->freeViewCamera = (FreeViewCamera){
        .enabled = false,
        .yawDeg = 35.0f,
        .pitchDeg = 20.0f,
        .target = {0.0f, 0.0f, 0.0f}
    };
    doc->currentConfigPath[0] = '\0';
    doc->currentSceneAuthoringPath[0] = '\0';
    doc->currentObjectAssetPath[0] = '\0';
    ObjectAuthoringSession_Clear(&doc->objectAuthoring);
    doc->hasObjectAuthoringState = false;
}

static bool WorkspaceHandoff_Capture(GlobalState* state,
                                     LineDrawingWorkspaceDocumentState* out_doc) {
    char* layout_snapshot = NULL;
    char* saved_snapshot = NULL;
    uint32_t workspace_source_scene_object_id = 0u;
    if (!state || !out_doc) return false;

    layout_snapshot = Layout_SaveToString(&state->layout);
    if (!layout_snapshot) return false;

    if (state->lastSavedSnapshot) {
        saved_snapshot = WorkspaceHandoff_DupString(state->lastSavedSnapshot);
        if (!saved_snapshot) {
            Layout_FreeString(layout_snapshot);
            return false;
        }
    }

    workspace_source_scene_object_id =
        (state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_OBJECT)
            ? state->objectWorkspaceDocument.workspaceSourceSceneObjectId
            : 0u;

    WorkspaceHandoff_DocumentStateFree(out_doc);
    out_doc->layoutSnapshot = layout_snapshot;
    out_doc->savedSnapshot = saved_snapshot;
    out_doc->layoutDirtySinceSave = state->layoutDirtySinceSave;
    out_doc->hasViewportState = true;
    out_doc->workspaceSourceSceneObjectId = workspace_source_scene_object_id;
    out_doc->selectedObjectId = state->editor.selectedObject3DId;
    out_doc->selectedAssetBodyId = state->editor.selectedObjectAssetBodyId;
    out_doc->selectedAssetFace = state->editor.selectedObjectAssetFace;
    out_doc->grid = state->grid;
    out_doc->activePlane = state->activePlane;
    out_doc->freeViewCamera = state->freeViewCamera;
    if (state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_OBJECT &&
        state->objectAuthoring.attached) {
        if (!ObjectAuthoringSession_Copy(&out_doc->objectAuthoring,
                                         &state->objectAuthoring)) {
            WorkspaceHandoff_DocumentStateFree(out_doc);
            return false;
        }
        out_doc->hasObjectAuthoringState = true;
    }
    WorkspaceHandoff_SetPath(out_doc->currentConfigPath,
                             sizeof(out_doc->currentConfigPath),
                             state->currentConfigPath);
    WorkspaceHandoff_SetPath(out_doc->currentSceneAuthoringPath,
                             sizeof(out_doc->currentSceneAuthoringPath),
                             state->currentSceneAuthoringPath);
    WorkspaceHandoff_SetPath(out_doc->currentObjectAssetPath,
                             sizeof(out_doc->currentObjectAssetPath),
                             state->currentObjectAssetPath);
    return true;
}

static void WorkspaceHandoff_FreeGlobalSavedSnapshot(GlobalState* state) {
    if (!state) return;
    Layout_FreeString(state->lastSavedSnapshot);
    state->lastSavedSnapshot = NULL;
}

static void WorkspaceHandoff_ResetGlobalDocumentIdentity(GlobalState* state) {
    if (!state) return;
    state->currentConfigPath[0] = '\0';
    state->currentSceneAuthoringPath[0] = '\0';
    state->currentObjectAssetPath[0] = '\0';
    state->layoutDirtySinceSave = false;
    WorkspaceHandoff_FreeGlobalSavedSnapshot(state);
}

static bool WorkspaceHandoff_CopyObjectToObjectWorkspace(Layout* layout,
                                                         const Object3D* source_object,
                                                         uint32_t* out_object_id) {
    Transform3D transform = Layout_Transform3D_Default();
    Object3D* object = NULL;
    uint32_t new_object_id = 0u;

    if (out_object_id) *out_object_id = 0u;
    if (!layout || !source_object || !Layout_ObjectStore_ValidateObject(source_object)) {
        return false;
    }

    transform = source_object->transform;
    transform.position = (Vec3){0.0f, 0.0f, 0.0f};
    new_object_id = Layout_ObjectStore_Create(&layout->objectStore,
                                              source_object->kind,
                                              &transform,
                                              source_object->coreMeta.object_type,
                                              source_object->coreMeta.dimensional_mode,
                                              source_object->coreMeta.locked_plane);
    object = Layout_ObjectStore_Find(&layout->objectStore, new_object_id);
    if (!object || new_object_id == 0u) {
        return false;
    }

    *object = *source_object;
    object->objectId = new_object_id;
    object->transform = transform;
    object->isDeleted = false;
    object->coreMeta = source_object->coreMeta;
    if (source_object->kind == OBJECT3D_KIND_PLANE) {
        object->plane.frame.origin = transform.position;
    } else if (source_object->kind == OBJECT3D_KIND_RECT_PRISM) {
        object->rectPrism.frame.origin = transform.position;
    }

    if (!Layout_ObjectStore_ValidateObject(object)) {
        (void)Layout_ObjectStore_Delete(&layout->objectStore, object->objectId);
        return false;
    }

    if (out_object_id) *out_object_id = new_object_id;
    return true;
}

static void WorkspaceHandoff_ApplyEditorSelection(GlobalState* state,
                                                  uint32_t selected_object_id,
                                                  uint32_t selected_asset_body_id,
                                                  Object3DFaceKind selected_asset_face) {
    if (!state) return;
    Editor_ResetDocumentState(&state->editor);
    if (selected_object_id != 0u &&
        Layout_ObjectStore_FindConst(&state->layout.objectStore, selected_object_id)) {
        state->editor.selectedObject3DId = selected_object_id;
    }
    if (selected_asset_body_id != 0u &&
        Layout_ObjectStore_FindConst(&state->layout.objectStore, selected_asset_body_id)) {
        state->editor.selectedObjectAssetBodyId = selected_asset_body_id;
        state->editor.selectedObjectAssetFace = selected_asset_face;
    } else if (state->editor.selectedObject3DId != 0u) {
        state->editor.selectedObjectAssetBodyId = state->editor.selectedObject3DId;
    }
    if (state->objectAuthoring.attached) {
        (void)ObjectAuthoringSession_SetSelection(&state->objectAuthoring,
                                                  state->editor.selectedObjectAssetBodyId,
                                                  state->editor.selectedObjectAssetFace);
    }
    Editor_HistoryCapture(&state->editor, &state->layout);
}

static bool WorkspaceHandoff_Restore(GlobalState* state,
                                     const LineDrawingWorkspaceDocumentState* doc) {
    if (!state || !doc || !doc->layoutSnapshot) return false;
    if (!Layout_LoadFromString(&state->layout, doc->layoutSnapshot)) return false;

    WorkspaceHandoff_ResetGlobalDocumentIdentity(state);
    WorkspaceHandoff_SetPath(state->currentConfigPath,
                             sizeof(state->currentConfigPath),
                             doc->currentConfigPath);
    WorkspaceHandoff_SetPath(state->currentSceneAuthoringPath,
                             sizeof(state->currentSceneAuthoringPath),
                             doc->currentSceneAuthoringPath);
    WorkspaceHandoff_SetPath(state->currentObjectAssetPath,
                             sizeof(state->currentObjectAssetPath),
                             doc->currentObjectAssetPath);
    state->layoutDirtySinceSave = doc->layoutDirtySinceSave;
    if (doc->savedSnapshot) {
        state->lastSavedSnapshot = WorkspaceHandoff_DupString(doc->savedSnapshot);
    }
    if (doc->hasObjectAuthoringState) {
        if (!ObjectAuthoringSession_Copy(&state->objectAuthoring,
                                         &doc->objectAuthoring)) {
            return false;
        }
        (void)ObjectAuthoringSession_MirrorBodiesFromLayout(&state->objectAuthoring,
                                                            &state->layout);
    } else {
        ObjectAuthoringSession_Clear(&state->objectAuthoring);
    }
    WorkspaceHandoff_ApplyEditorSelection(state,
                                          doc->selectedObjectId,
                                          doc->selectedAssetBodyId,
                                          doc->selectedAssetFace);
    if (doc->hasViewportState && doc->grid.gridSize > 0.0f) {
        state->grid = doc->grid;
        state->activePlane = doc->activePlane;
        state->freeViewCamera = doc->freeViewCamera;
    }
    state->layoutDirty = false;
    state->hitboxDirty = true;
    return true;
}

static void WorkspaceHandoff_SeedObjectWorkspaceViewport(GlobalState* state,
                                                         uint32_t selected_object_id) {
    if (!state) return;
    (void)LineDrawingObjectWorkspaceView_EnterFreeView(state, selected_object_id);
}

static void WorkspaceHandoff_SelectDefaultObjectFace(GlobalState* state,
                                                     uint32_t selected_object_id) {
    const Object3D* object = NULL;
    SpaceViewContext view_ctx = {0};
    Object3DFaceKind face = OBJECT3D_FACE_NONE;
    PlaneFrame3 frame = {0};

    if (!state || selected_object_id == 0u) return;
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore, selected_object_id);
    if (!object) return;

    view_ctx = SpaceAdapter_BuildViewContext(state);
    if (!Layout_Object3D_DefaultAuthoringFaceForView(object, &view_ctx, &face)) return;

    state->editor.selectedObjectAssetBodyId = selected_object_id;
    state->editor.selectedObjectAssetFace = face;
    if (Layout_Object3DFace_GetFrame(object, face, &frame)) {
        state->layout.scene3d.constructionPlane.mode = CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME;
        state->layout.scene3d.constructionPlane.customFrame = frame;
    }
    if (state->objectAuthoring.attached) {
        (void)ObjectAuthoringSession_SetSelection(&state->objectAuthoring,
                                                  selected_object_id,
                                                  face);
    }
}

static uint32_t WorkspaceHandoff_FirstLiveObjectId(const Layout* layout) {
    if (!layout) return 0u;
    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        if (!object->isDeleted && Layout_ObjectStore_ValidateObject(object)) {
            return object->objectId;
        }
    }
    return 0u;
}

static bool WorkspaceHandoff_SeedObjectWorkspaceFromMeshInstance(
    GlobalState* state,
    const Object3D* selected_scene_object,
    uint32_t* out_selected_object_id) {
    Layout seeded_layout;
    ObjectAuthoringDocument loaded_authoring;
    bool has_authoring = false;
    uint32_t selected_object_id = 0u;
    char source_asset_path[LINE_DRAWING_PATH_CAP];
    char diagnostics[256];

    if (out_selected_object_id) *out_selected_object_id = 0u;
    if (!state || !selected_scene_object ||
        selected_scene_object->kind != OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
        return false;
    }
    if (!WorkspaceHandoff_DeriveSourceAssetPathFromMeshInstance(
            &selected_scene_object->meshInstance,
            source_asset_path,
            sizeof(source_asset_path))) {
        return false;
    }

    diagnostics[0] = '\0';
    Layout_Init(&seeded_layout, state->layout.gridSize);
    ObjectAuthoringDocument_Init(&loaded_authoring);
    if (!LayoutObjectAssetMeshAuthoring_LoadWithAuthoring(&seeded_layout,
                                                          &loaded_authoring,
                                                          &has_authoring,
                                                          source_asset_path,
                                                          diagnostics,
                                                          sizeof(diagnostics))) {
        ObjectAuthoringDocument_Free(&loaded_authoring);
        Layout_Free(&seeded_layout);
        fprintf(stderr,
                "[workspace] Mesh source asset reopen failed for %s%s%s%s\n",
                source_asset_path,
                diagnostics[0] ? " (" : "",
                diagnostics[0] ? diagnostics : "",
                diagnostics[0] ? ")" : "");
        return false;
    }

    selected_object_id = WorkspaceHandoff_FirstLiveObjectId(&seeded_layout);
    Layout_Free(&state->layout);
    state->layout = seeded_layout;
    state->objectWorkspaceDocument.workspaceSourceSceneObjectId = selected_scene_object->objectId;
    ObjectAuthoringSession_Clear(&state->objectAuthoring);
    if (has_authoring) {
        state->objectAuthoring.attached = true;
        state->objectAuthoring.sourceSceneObjectId = selected_scene_object->objectId;
        (void)ObjectAuthoringDocument_Copy(&state->objectAuthoring.document,
                                           &loaded_authoring);
    } else {
        (void)ObjectAuthoringSession_ResetFromLayout(&state->objectAuthoring,
                                                     &state->layout,
                                                     selected_scene_object->objectId);
    }
    ObjectAuthoringDocument_Free(&loaded_authoring);
    Global_OnObjectAssetLoaded(source_asset_path);
    state->layoutDirtySinceSave = false;
    state->layoutDirty = false;
    state->hitboxDirty = true;
    if (out_selected_object_id) *out_selected_object_id = selected_object_id;
    return true;
}

static bool WorkspaceHandoff_SeedObjectWorkspace(GlobalState* state) {
    const Object3D* selected_scene_object = NULL;
    uint32_t selected_scene_object_id = 0u;
    Layout seeded_layout;
    uint32_t selected_object_id = 0u;
    if (!state) return false;

    if (state->editor.selectedObject3DId != 0u) {
        selected_scene_object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                                             state->editor.selectedObject3DId);
        if (selected_scene_object) {
            selected_scene_object_id = selected_scene_object->objectId;
        }
    }

    if (selected_scene_object) {
        if (selected_scene_object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE &&
            WorkspaceHandoff_SeedObjectWorkspaceFromMeshInstance(state,
                                                                 selected_scene_object,
                                                                 &selected_object_id)) {
            WorkspaceHandoff_ApplyEditorSelection(state,
                                                  selected_object_id,
                                                  selected_object_id,
                                                  OBJECT3D_FACE_NONE);
            WorkspaceHandoff_SeedObjectWorkspaceViewport(state, selected_object_id);
            WorkspaceHandoff_SelectDefaultObjectFace(state, selected_object_id);
            return true;
        }
    }

    Layout_Init(&seeded_layout, state->layout.gridSize);
    if (selected_scene_object) {
        if (!WorkspaceHandoff_CopyObjectToObjectWorkspace(&seeded_layout,
                                                          selected_scene_object,
                                                          &selected_object_id)) {
            Layout_Free(&seeded_layout);
            return false;
        }
    }

    Layout_Free(&state->layout);
    state->layout = seeded_layout;
    state->objectWorkspaceDocument.workspaceSourceSceneObjectId = selected_scene_object_id;
    (void)ObjectAuthoringSession_ResetFromLayout(&state->objectAuthoring,
                                                 &state->layout,
                                                 selected_scene_object_id);
    WorkspaceHandoff_ResetGlobalDocumentIdentity(state);
    state->layoutDirtySinceSave = selected_object_id != 0u;
    WorkspaceHandoff_ApplyEditorSelection(state,
                                          selected_object_id,
                                          selected_object_id,
                                          OBJECT3D_FACE_NONE);
    WorkspaceHandoff_SeedObjectWorkspaceViewport(state, selected_object_id);
    WorkspaceHandoff_SelectDefaultObjectFace(state, selected_object_id);
    state->layoutDirty = false;
    state->hitboxDirty = true;
    return true;
}

static bool WorkspaceHandoff_HasSelectedSceneObject(const GlobalState* state) {
    if (!state) return false;
    if (state->editor.selectedObject3DId == 0u) return false;
    return Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                        state->editor.selectedObject3DId) != NULL;
}

static bool WorkspaceHandoff_ShouldReseedObjectWorkspace(const GlobalState* state,
                                                         bool has_selected_scene_object) {
    if (!state) return false;
    if (!has_selected_scene_object) {
        return state->objectWorkspaceDocument.layoutSnapshot == NULL;
    }
    if (state->objectWorkspaceDocument.layoutSnapshot == NULL) {
        return true;
    }
    return state->objectWorkspaceDocument.workspaceSourceSceneObjectId !=
           state->editor.selectedObject3DId;
}

static void WorkspaceHandoff_RefreshSceneMeshInstanceFromObjectWorkspace(
    GlobalState* state) {
    const uint32_t source_scene_object_id =
        state ? state->objectWorkspaceDocument.workspaceSourceSceneObjectId : 0u;
    Object3D* object = NULL;
    char runtime_path[LINE_DRAWING_PATH_CAP];
    char diagnostics[256] = {0};

    if (!state || source_scene_object_id == 0u) return;
    object = Layout_ObjectStore_Find(&state->layout.objectStore,
                                     source_scene_object_id);
    if (!object || object->kind != OBJECT3D_KIND_MESH_ASSET_INSTANCE) return;
    WorkspaceHandoff_SetPath(runtime_path,
                             sizeof(runtime_path),
                             object->meshInstance.runtimePath);

    if (!Layout_RefreshMeshAssetInstancesFromRuntimeAsset(&state->layout,
                                                          runtime_path,
                                                          NULL,
                                                          NULL,
                                                          diagnostics,
                                                          sizeof(diagnostics)) &&
        diagnostics[0]) {
        fprintf(stderr,
                "[Workspace] Mesh asset instance refresh skipped: %s\n",
                diagnostics);
    }
}

bool LineDrawingWorkspaceModeHandoff_Apply(GlobalState* state,
                                           LineDrawingWorkspaceMode next_mode) {
    if (!state) return false;
    if (next_mode == state->workspaceMode) return true;

    if (state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_SCENE) {
        const bool has_selected_scene_object =
            WorkspaceHandoff_HasSelectedSceneObject(state);
        const bool should_reseed_object_workspace =
            WorkspaceHandoff_ShouldReseedObjectWorkspace(state,
                                                         has_selected_scene_object);
        if (!WorkspaceHandoff_Capture(state, &state->sceneWorkspaceDocument)) {
            return false;
        }
        if (should_reseed_object_workspace) {
            if (!WorkspaceHandoff_SeedObjectWorkspace(state)) {
                return false;
            }
        } else if (state->objectWorkspaceDocument.layoutSnapshot) {
            if (!WorkspaceHandoff_Restore(state, &state->objectWorkspaceDocument)) {
                return false;
            }
            if (!state->objectWorkspaceDocument.hasViewportState) {
                WorkspaceHandoff_SeedObjectWorkspaceViewport(state,
                                                             state->editor.selectedObject3DId);
            }
            if (state->editor.selectedObject3DId != 0u &&
                state->editor.selectedObjectAssetFace == OBJECT3D_FACE_NONE) {
                WorkspaceHandoff_SelectDefaultObjectFace(state,
                                                         state->editor.selectedObject3DId);
            }
        } else if (!WorkspaceHandoff_SeedObjectWorkspace(state)) {
            return false;
        }
    } else {
        if (!WorkspaceHandoff_Capture(state, &state->objectWorkspaceDocument)) {
            return false;
        }
        if (!WorkspaceHandoff_Restore(state, &state->sceneWorkspaceDocument)) {
            return false;
        }
        WorkspaceHandoff_RefreshSceneMeshInstanceFromObjectWorkspace(state);
    }

    state->workspaceMode = next_mode;
    return true;
}
