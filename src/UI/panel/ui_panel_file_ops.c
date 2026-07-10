#include "UI/ui_panel_internal.h"

#include "Core/global_state.h"
#include "Layout/asset/layout_imported_mesh_asset.h"
#include "Layout/asset/layout_object_asset_mesh_authoring.h"
#include "Layout/layout.h"
#include "Layout/layout_json.h"
#include "Layout/scene/layout_mesh_preview_sidecar.h"
#include "Editor/editor.h"
#include "ObjectAuthoring/object_authoring_mesh_compile.h"
#include "Tools/scene_import.h"
#include "Tools/scene_export.h"
#include "Tools/shape_export.h"

#include <SDL2/SDL.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static const char* k_legacy_layout_root = "config";
static const float kRuntimeMeshAutoFitDefaultSpan = 4.0f;
static const float kRuntimeMeshAutoFitMaxSceneSpan = 48.0f;
static const float kRuntimeMeshAutoFitSceneBoundsFraction = 0.35f;
static const Uint32 kFilePaneActionStatusTtlMs = 2500u;

void UIPanel_SetFilePaneActionStatus(const char* status) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui) return;
    snprintf(ui->filePane.actionStatus,
             sizeof(ui->filePane.actionStatus),
             "%s",
             status && status[0] ? status : "");
    ui->filePane.actionStatusSetTicks = ui->filePane.actionStatus[0] ? SDL_GetTicks() : 0u;
}

bool UIPanel_FilePaneActionStatusIsLive(const UIPanelState* ui) {
    if (!ui || !ui->filePane.actionStatus[0] || ui->filePane.actionStatusSetTicks == 0u) {
        return false;
    }
    return (Uint32)(SDL_GetTicks() - ui->filePane.actionStatusSetTicks) < kFilePaneActionStatusTtlMs;
}

static bool UIPanel_IsObjectWorkspace(void) {
    return Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT;
}

static bool UIPanel_LayoutWorldAabb(const Layout* layout, Vec3* out_min, Vec3* out_max) {
    bool found = false;
    if (!layout || !out_min || !out_max) return false;
    for (size_t i = 0u; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        Vec3 min = {0};
        Vec3 max = {0};
        if (!Layout_Object3D_ComputeWorldAABB(object, &min, &max)) continue;
        if (!found) {
            *out_min = min;
            *out_max = max;
            found = true;
        } else {
            out_min->x = fminf(out_min->x, min.x);
            out_min->y = fminf(out_min->y, min.y);
            out_min->z = fminf(out_min->z, min.z);
            out_max->x = fmaxf(out_max->x, max.x);
            out_max->y = fmaxf(out_max->y, max.y);
            out_max->z = fmaxf(out_max->z, max.z);
        }
    }
    for (size_t i = 0u; i < layout->anchorCount; ++i) {
        const Anchor* anchor = &layout->anchors[i];
        if (anchor->isDeleted) continue;
        if (!found) {
            *out_min = anchor->pos;
            *out_max = anchor->pos;
            found = true;
        } else {
            out_min->x = fminf(out_min->x, anchor->pos.x);
            out_min->y = fminf(out_min->y, anchor->pos.y);
            out_min->z = fminf(out_min->z, anchor->pos.z);
            out_max->x = fmaxf(out_max->x, anchor->pos.x);
            out_max->y = fmaxf(out_max->y, anchor->pos.y);
            out_max->z = fmaxf(out_max->z, anchor->pos.z);
        }
    }
    return found;
}

void UIPanel_ResetEditorTransientSelection(EditorState* editor) {
    if (!editor) return;
    editor->selectedAnchorIndex = -1;
    editor->selectedWallIndex = -1;
    editor->selectedObject3DId = 0u;
    editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->hoveredAnchorIndex = -1;
    editor->hoveredWallIndex = -1;
    editor->hoveredObject3DId = 0u;
    editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->hoveredHandleAnchor = -1;
    editor->hoveredHandleComponent = -1;
    editor->hoveredGizmoAxis = -1;
    editor->hoveredObject3DGizmoAxis = -1;
    editor->activeObject3DGizmoAxis = -1;
    editor->selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_NONE;
}

void UIPanel_RefreshViewportAfterSceneDocumentLoad(GlobalState* state) {
    Vec3 min = {0};
    Vec3 max = {0};
    if (!state) return;
    state->activePlane = Layout_ConstructionPlane3D_ToViewPlane(&state->layout.scene3d.constructionPlane);
    if (state->freeViewCamera.enabled &&
        UIPanel_LayoutWorldAabb(&state->layout, &min, &max)) {
        state->freeViewCamera.target = (Vec3){
            .x = (min.x + max.x) * 0.5f,
            .y = (min.y + max.y) * 0.5f,
            .z = (min.z + max.z) * 0.5f
        };
    }
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();
}

static void SanitizeBuffer(char* buffer) {
    size_t len = strlen(buffer);
    while (len > 0 && isspace((unsigned char)buffer[len - 1])) {
        buffer[--len] = '\0';
    }
}

#if defined(__APPLE__)
static void EscapeAppleScriptString(const char* input, char* output, size_t output_size) {
    size_t out_index = 0;
    if (!output || output_size == 0) return;
    output[0] = '\0';
    if (!input) return;

    for (size_t i = 0; input[i] != '\0' && out_index + 1 < output_size; ++i) {
        const char c = input[i];
        if ((c == '\\' || c == '"') && out_index + 2 < output_size) {
            output[out_index++] = '\\';
        }
        if (out_index + 1 >= output_size) break;
        output[out_index++] = c;
    }
    output[out_index] = '\0';
}
#endif

static void UIPanel_GetDefaultLoadDirectory(char* out_dir, size_t out_dir_size) {
    const char* current_path = Global_GetCurrentConfigPath();
    const char* input_root = Global_GetInputRoot();
    if (!out_dir || out_dir_size == 0) return;

    out_dir[0] = '\0';
    if (current_path && current_path[0] != '\0') {
        const char* last_slash = strrchr(current_path, '/');
        if (last_slash && last_slash != current_path) {
            const size_t len = (size_t)(last_slash - current_path);
            const size_t copy_len = len < out_dir_size - 1 ? len : out_dir_size - 1;
            memcpy(out_dir, current_path, copy_len);
            out_dir[copy_len] = '\0';
            return;
        }
    }

    if (input_root && input_root[0] != '\0') {
        snprintf(out_dir, out_dir_size, "%s", input_root);
        return;
    }

    snprintf(out_dir, out_dir_size, "%s", k_legacy_layout_root);
}

static void UIPanel_GetDefaultObjectAssetSelectionDirectory(char* out_dir, size_t out_dir_size) {
    const char* current_asset_path = Global_GetCurrentObjectAssetPath();
    const char* asset_root = Global_GetObjectAssetRoot();
    if (!out_dir || out_dir_size == 0) return;
    out_dir[0] = '\0';

    if (current_asset_path && current_asset_path[0] != '\0') {
        const char* last_slash = strrchr(current_asset_path, '/');
        if (last_slash && last_slash != current_asset_path) {
            const size_t len = (size_t)(last_slash - current_asset_path);
            const size_t copy_len = len < out_dir_size - 1 ? len : out_dir_size - 1;
            memcpy(out_dir, current_asset_path, copy_len);
            out_dir[copy_len] = '\0';
            return;
        }
    }

    if (asset_root && asset_root[0] != '\0') {
        snprintf(out_dir, out_dir_size, "%s", asset_root);
        return;
    }

    snprintf(out_dir, out_dir_size, "%s", k_legacy_layout_root);
}

static void UIPanel_GetDefaultSceneSelectionDirectory(char* out_dir, size_t out_dir_size) {
    const char* current_scene_path = Global_GetCurrentSceneAuthoringPath();
    const char* input_root = Global_GetInputRoot();
    if (!out_dir || out_dir_size == 0) return;
    out_dir[0] = '\0';

    if (current_scene_path && current_scene_path[0] != '\0') {
        const char* last_slash = strrchr(current_scene_path, '/');
        if (last_slash && last_slash != current_scene_path) {
            const size_t len = (size_t)(last_slash - current_scene_path);
            const size_t copy_len = len < out_dir_size - 1 ? len : out_dir_size - 1;
            memcpy(out_dir, current_scene_path, copy_len);
            out_dir[copy_len] = '\0';
            return;
        }
    }

    if (input_root && input_root[0] != '\0') {
        snprintf(out_dir, out_dir_size, "%s", input_root);
        return;
    }

    snprintf(out_dir, out_dir_size, "%s", k_legacy_layout_root);
}

static void UIPanel_PopulateDefaultFilename(UIPanelState* ui) {
    const char* path = UIPanel_IsObjectWorkspace()
                           ? Global_GetCurrentObjectAssetPath()
                           : Global_GetCurrentConfigPath();
    ui->saveDialog.buffer[0] = '\0';
    ui->saveDialog.length = 0;
    ui->saveDialog.cursor = 0;

    if (!path || !*path) return;

    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;

    size_t len = strlen(base);
    if (len >= 5 && strcasecmp(base + len - 5, ".json") == 0) {
        len -= 5;
    }
    if (len >= sizeof(ui->saveDialog.buffer)) len = sizeof(ui->saveDialog.buffer) - 1;

    memcpy(ui->saveDialog.buffer, base, len);
    ui->saveDialog.buffer[len] = '\0';
    ui->saveDialog.length = len;
    ui->saveDialog.cursor = len;
}

static bool UIPanel_DeriveLayoutHintFromScenePath(const char* scene_path,
                                                  char* out_path,
                                                  size_t out_path_size) {
    const char* last_slash = NULL;
    const char* dir_name = NULL;
    size_t dir_len = 0u;
    size_t dir_name_len = 0u;
    if (!scene_path || !scene_path[0] || !out_path || out_path_size == 0u) return false;

    last_slash = strrchr(scene_path, '/');
    if (!last_slash || last_slash == scene_path) return false;
    dir_len = (size_t)(last_slash - scene_path);

    dir_name = last_slash;
    while (dir_name > scene_path && dir_name[-1] != '/') {
        --dir_name;
    }
    dir_name_len = (size_t)(last_slash - dir_name);
    if (dir_name_len == 0u) return false;

    if (snprintf(out_path,
                 out_path_size,
                 "%.*s/%.*s.json",
                 (int)dir_len,
                 scene_path,
                 (int)dir_name_len,
                 dir_name) >= (int)out_path_size) {
        return false;
    }
    return true;
}

static float UIPanel_SceneRuntimeMeshTargetSpan(const Layout* layout) {
    float target = kRuntimeMeshAutoFitDefaultSpan;
    if (layout &&
        layout->scene3d.bounds.enabled &&
        Layout_SceneBounds3D_IsValid(&layout->scene3d.bounds)) {
        const Vec3 min = layout->scene3d.bounds.min;
        const Vec3 max = layout->scene3d.bounds.max;
        const float span_x = fabsf(max.x - min.x);
        const float span_y = fabsf(max.y - min.y);
        const float span_z = fabsf(max.z - min.z);
        const float scene_span = fmaxf(span_x, fmaxf(span_y, span_z));
        if (scene_span > 0.0f) {
            target = fminf(kRuntimeMeshAutoFitMaxSceneSpan,
                           scene_span * kRuntimeMeshAutoFitSceneBoundsFraction);
        }
    }
    return fmaxf(target, kRuntimeMeshAutoFitDefaultSpan);
}

static bool UIPanel_ResolveRuntimeMeshAutoScale(const Layout* layout,
                                                const char* runtime_mesh_path,
                                                bool allow_scale_up,
                                                float* out_scale) {
    MeshAssetInstance3D instance = {0};
    char diagnostics[128];
    float span_x = 0.0f;
    float span_y = 0.0f;
    float span_z = 0.0f;
    float max_span = 0.0f;
    float target_span = 0.0f;
    if (out_scale) *out_scale = 1.0f;
    if (!layout || !runtime_mesh_path || !runtime_mesh_path[0] || !out_scale) return false;
    diagnostics[0] = '\0';
    if (!Layout_MeshPreviewSidecarReadInstance(runtime_mesh_path,
                                               &instance,
                                               diagnostics,
                                               sizeof(diagnostics))) {
        return false;
    }
    span_x = fabsf(instance.localBoundsMax.x - instance.localBoundsMin.x);
    span_y = fabsf(instance.localBoundsMax.y - instance.localBoundsMin.y);
    span_z = fabsf(instance.localBoundsMax.z - instance.localBoundsMin.z);
    max_span = fmaxf(span_x, fmaxf(span_y, span_z));
    target_span = UIPanel_SceneRuntimeMeshTargetSpan(layout);
    if (max_span <= 1e-5f) return true;
    if (!allow_scale_up && max_span <= target_span) return true;
    *out_scale = target_span / max_span;
    return isfinite(*out_scale) && *out_scale > 0.0f;
}

static void UIPanel_BuildObjectRuntimeMeshAssetId(const char* source_asset_path,
                                                  const char* fallback_name,
                                                  char* out_asset_id,
                                                  size_t out_asset_id_size) {
    const char* base = NULL;
    size_t len = 0u;
    if (!out_asset_id || out_asset_id_size == 0u) return;
    out_asset_id[0] = '\0';
    base = source_asset_path && source_asset_path[0] ? strrchr(source_asset_path, '/') : NULL;
    base = base ? base + 1 : source_asset_path;
    if (!base || !base[0]) base = fallback_name && fallback_name[0] ? fallback_name : "object_asset";
    len = strlen(base);
    if (len > 5u && strcasecmp(base + len - 5u, ".json") == 0) {
        len -= 5u;
    }
    if (len == 0u) {
        snprintf(out_asset_id, out_asset_id_size, "object_asset");
        return;
    }
    if (len >= out_asset_id_size) len = out_asset_id_size - 1u;
    memcpy(out_asset_id, base, len);
    out_asset_id[len] = '\0';
}

static bool UIPanel_BuildObjectRuntimeMeshPath(GlobalState* state,
                                               char* out_source_asset_id,
                                               size_t out_source_asset_id_size,
                                               char* out_runtime_asset_id,
                                               size_t out_runtime_asset_id_size,
                                               char* out_path,
                                               size_t out_path_size) {
    char base_asset_id[64];
    char filename[160];
    const char* current_path = state ? Global_GetCurrentObjectAssetPath() : NULL;
    const char* root = Global_GetObjectAssetRoot();
    if (!state || !out_source_asset_id || !out_runtime_asset_id || !out_path ||
        out_source_asset_id_size == 0u || out_runtime_asset_id_size == 0u ||
        out_path_size == 0u) {
        return false;
    }
    UIPanel_BuildObjectRuntimeMeshAssetId(current_path,
                                          "object_asset",
                                          base_asset_id,
                                          sizeof(base_asset_id));
    snprintf(out_source_asset_id, out_source_asset_id_size, "%s", base_asset_id);
    snprintf(out_runtime_asset_id, out_runtime_asset_id_size, "%s_runtime", base_asset_id);
    snprintf(filename, sizeof(filename), "%s.runtime.json", base_asset_id);
    return LineDrawingDataPaths_BuildPath(out_path,
                                          out_path_size,
                                          root && root[0] ? root : k_legacy_layout_root,
                                          filename);
}

static const char* UIPanel_GetRootValue(UIRootDialogTarget target) {
    switch (target) {
        case UI_ROOT_TARGET_INPUT:
            return Global_GetInputRoot();
        case UI_ROOT_TARGET_OUTPUT:
            return Global_GetOutputRoot();
        case UI_ROOT_TARGET_OBJECT_ASSET:
            return Global_GetObjectAssetRoot();
        default:
            return NULL;
    }
}

static bool UIPanel_SetRootValue(UIRootDialogTarget target, const char* value) {
    switch (target) {
        case UI_ROOT_TARGET_INPUT:
            return Global_SetInputRoot(value, true);
        case UI_ROOT_TARGET_OUTPUT:
            return Global_SetOutputRoot(value, true);
        case UI_ROOT_TARGET_OBJECT_ASSET:
            return Global_SetObjectAssetRoot(value, true);
        default:
            return false;
    }
}

static bool UIPanel_ShouldRefreshBrowserForEditedRoot(const UIPanelState* ui,
                                                      UIRootDialogTarget target) {
    if (!ui) return false;
    if (!ui->loadMenu.visible || ui->loadMenu.mode == UI_LOAD_MENU_MODE_NONE) return true;
    if (!ui->loadMenu.rootPath[0]) return true;

    if (target == UI_ROOT_TARGET_INPUT) {
        return ui->loadMenu.mode != UI_LOAD_MENU_MODE_OBJECT &&
               strcmp(ui->loadMenu.rootPath, Global_GetInputRoot()) == 0;
    }
    if (target == UI_ROOT_TARGET_OBJECT_ASSET) {
        return (ui->loadMenu.mode == UI_LOAD_MENU_MODE_OBJECT ||
                ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH ||
                ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT) &&
               strcmp(ui->loadMenu.rootPath, Global_GetObjectAssetRoot()) == 0;
    }
    return false;
}

static bool UIPanel_SelectFolderWithPrompt(const char* prompt, char* out_path, size_t out_path_size) {
#if defined(__APPLE__)
    FILE* pipe = NULL;
    char command[512];
    if (!prompt || !out_path || out_path_size == 0) return false;
    out_path[0] = '\0';
    snprintf(command,
             sizeof(command),
             "/usr/bin/osascript -e 'POSIX path of (choose folder with prompt \"%s\")'",
             prompt);
    pipe = popen(command, "r");
    if (!pipe) return false;
    if (!fgets(out_path, (int)out_path_size, pipe)) {
        (void)pclose(pipe);
        out_path[0] = '\0';
        return false;
    }
    (void)pclose(pipe);
    SanitizeBuffer(out_path);
    return out_path[0] != '\0';
#else
    (void)prompt;
    (void)out_path;
    (void)out_path_size;
    return false;
#endif
}

static bool UIPanel_SelectFolderWithPromptAndDefault(const char* prompt,
                                                     const char* default_dir,
                                                     char* out_path,
                                                     size_t out_path_size) {
#if defined(__APPLE__)
    FILE* pipe = NULL;
    char command[1024];
    char escaped_prompt[256];
    char escaped_dir[LINE_DRAWING_PATH_CAP * 2];

    if (!prompt || !default_dir || !default_dir[0] || !out_path || out_path_size == 0) return false;

    out_path[0] = '\0';
    EscapeAppleScriptString(prompt, escaped_prompt, sizeof(escaped_prompt));
    EscapeAppleScriptString(default_dir, escaped_dir, sizeof(escaped_dir));
    snprintf(command,
             sizeof(command),
             "/usr/bin/osascript "
             "-e 'set defaultDir to POSIX file \"%s\"' "
             "-e 'POSIX path of (choose folder with prompt \"%s\" default location defaultDir)'",
             escaped_dir,
             escaped_prompt);
    pipe = popen(command, "r");
    if (!pipe) return false;
    if (!fgets(out_path, (int)out_path_size, pipe)) {
        (void)pclose(pipe);
        out_path[0] = '\0';
        return false;
    }
    (void)pclose(pipe);
    SanitizeBuffer(out_path);
    return out_path[0] != '\0';
#else
    (void)prompt;
    (void)default_dir;
    (void)out_path;
    (void)out_path_size;
    return false;
#endif
}

void UIPanel_BeginRootDialog(UIRootDialogTarget target) {
    UIPanelState* ui = UIPanel_Get();
    const char* current = UIPanel_GetRootValue(target);
    ui->rootDialog.active = true;
    ui->rootDialog.target = target;
    if (current && *current) {
        snprintf(ui->rootDialog.buffer, sizeof(ui->rootDialog.buffer), "%s", current);
    } else {
        ui->rootDialog.buffer[0] = '\0';
    }
    SanitizeBuffer(ui->rootDialog.buffer);
    ui->rootDialog.length = strlen(ui->rootDialog.buffer);
    ui->rootDialog.cursor = ui->rootDialog.length;

    UIPanel_CloseFileBrowser(ui);
    UIPanel_CloseSaveDialog(ui);
    UIPanel_ClosePrismDimensionDialog(ui);
    UIPanel_CloseSceneBoundsDialog(ui);
    UIPanel_CloseConstructionPlaneDialog(ui);
    UIPanel_CloseObjectTransformDialog(ui);
    if (!SDL_IsTextInputActive()) SDL_StartTextInput();
}

bool UIPanel_ApplyRootDialog(UIPanelState* ui) {
    if (!ui || !ui->rootDialog.active) return false;
    SanitizeBuffer(ui->rootDialog.buffer);
    ui->rootDialog.length = strlen(ui->rootDialog.buffer);
    if (ui->rootDialog.length == 0) {
        SDL_Log("[UI] Root update aborted: path is empty.");
        return false;
    }
    if (!UIPanel_SetRootValue(ui->rootDialog.target, ui->rootDialog.buffer)) {
        SDL_Log("[UI] Root update failed for path '%s'", ui->rootDialog.buffer);
        return false;
    }
    if (UIPanel_ShouldRefreshBrowserForEditedRoot(ui, ui->rootDialog.target)) {
        UIPanel_RefreshConfigList();
    }
    SDL_Log("[UI] Root updated: %s", ui->rootDialog.buffer);
    UIPanel_CloseRootDialog(ui);
    return true;
}

bool UIPanel_PerformSave(UIPanelState* ui) {
    SanitizeBuffer(ui->saveDialog.buffer);
    ui->saveDialog.length = strlen(ui->saveDialog.buffer);
    if (ui->saveDialog.cursor > ui->saveDialog.length) ui->saveDialog.cursor = ui->saveDialog.length;

    if (ui->saveDialog.length == 0) {
        SDL_Log("[UI] Save aborted: filename is empty.");
        UIPanel_SetFilePaneActionStatus("Save failed: filename is empty.");
        return false;
    }

    char filename[160];
    strncpy(filename, ui->saveDialog.buffer, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';

    size_t len = strlen(filename);
    if (len < 5 || strcasecmp(filename + len - 5, ".json") != 0) {
        if (len + 5 < sizeof(filename)) {
            strcat(filename, ".json");
        } else {
            SDL_Log("[UI] Save aborted: filename too long.");
            UIPanel_SetFilePaneActionStatus("Save failed: filename is too long.");
            return false;
        }
    }

    char path[256];
    char fallback_path[256];
    char diagnostics[256];
    const bool object_workspace = UIPanel_IsObjectWorkspace();
    const char* save_root = object_workspace ? Global_GetObjectAssetRoot() : Global_GetInputRoot();
    bool saved = false;

    GlobalState* state = Global_Get();
    diagnostics[0] = '\0';
    Layout_CompactDeletedElements(&state->layout);

    if (LineDrawingDataPaths_BuildPath(path,
                                       sizeof(path),
                                       save_root ? save_root : k_legacy_layout_root,
                                       filename)) {
        if (object_workspace
                ? LayoutObjectAssetMeshAuthoring_SaveWithAuthoring(
                      &state->layout,
                      state->objectAuthoring.attached ? &state->objectAuthoring.document : NULL,
                      path,
                      diagnostics,
                      sizeof(diagnostics))
                : Layout_SaveToFile(&state->layout, path)) {
            saved = true;
        } else {
            SDL_Log("[UI] Primary save failed at %s%s%s%s; trying legacy fallback.",
                    path,
                    diagnostics[0] ? " (" : "",
                    diagnostics[0] ? diagnostics : "",
                    diagnostics[0] ? ")" : "");
        }
    }

    if (!saved) {
        if (!LineDrawingDataPaths_BuildPath(fallback_path, sizeof(fallback_path), k_legacy_layout_root, filename)) {
            SDL_Log("[UI] Save failed: invalid legacy fallback path.");
            UIPanel_SetFilePaneActionStatus("Save failed: invalid fallback path.");
            return false;
        }
        if (!(object_workspace
                  ? LayoutObjectAssetMeshAuthoring_SaveWithAuthoring(
                        &state->layout,
                        state->objectAuthoring.attached ? &state->objectAuthoring.document : NULL,
                        fallback_path,
                        diagnostics,
                        sizeof(diagnostics))
                  : Layout_SaveToFile(&state->layout, fallback_path))) {
            SDL_Log("[UI] Failed to save %s to %s%s%s%s",
                    object_workspace ? "object asset" : "layout",
                    fallback_path,
                    diagnostics[0] ? " (" : "",
                    diagnostics[0] ? diagnostics : "",
                    diagnostics[0] ? ")" : "");
            {
                char status[160];
                snprintf(status,
                         sizeof(status),
                         "Save failed: %s",
                         diagnostics[0] ? diagnostics : "write error");
                UIPanel_SetFilePaneActionStatus(status);
            }
            return false;
        }
        snprintf(path, sizeof(path), "%s", fallback_path);
    }

    SDL_Log("[UI] %s saved to %s", object_workspace ? "Object asset" : "Layout", path);
    if (object_workspace) {
        Global_OnObjectAssetSaved(path);
    } else {
        Global_OnLayoutSaved(path);
    }
    Editor_ClearHistory(&state->editor);
    Editor_HistoryCapture(&state->editor, &state->layout);
    UIPanel_RefreshConfigList();
    ui->saveDialog.cursor = ui->saveDialog.length;
    UIPanel_CloseSaveDialog(ui);
    return true;
}

void UIPanel_BeginSaveDialog(void) {
    UIPanelState* ui = UIPanel_Get();
    ui->saveDialog.active = true;
    UIPanel_PopulateDefaultFilename(ui);
    UIPanel_CloseFileBrowser(ui);
    UIPanel_CloseRootDialog(ui);
    UIPanel_ClosePrismDimensionDialog(ui);
    UIPanel_CloseSceneBoundsDialog(ui);
    UIPanel_CloseConstructionPlaneDialog(ui);
    UIPanel_CloseObjectTransformDialog(ui);
    if (!SDL_IsTextInputActive()) SDL_StartTextInput();
}

bool UIPanel_LoadSceneFromPath(const char* path) {
    GlobalState* state = Global_Get();
    char diagnostics[256];
    char layout_hint[LINE_DRAWING_PATH_CAP];
    if (!state || !path || path[0] == '\0') return false;

    Editor_ClearHistory(&state->editor);

    if (!LineDrawingSceneImport_LoadLayoutFromAuthoringFile(&state->layout,
                                                            path,
                                                            diagnostics,
                                                            sizeof(diagnostics))) {
        SDL_Log("[UI] Failed to import scene %s (%s)", path, diagnostics[0] ? diagnostics : "unknown");
        {
            char status[320];
            snprintf(status,
                     sizeof(status),
                     "Load scene failed: %s",
                     diagnostics[0] ? diagnostics : "unknown error");
            UIPanel_SetFilePaneActionStatus(status);
        }
        return false;
    }

    layout_hint[0] = '\0';
    if (!UIPanel_DeriveLayoutHintFromScenePath(path, layout_hint, sizeof(layout_hint))) {
        snprintf(layout_hint, sizeof(layout_hint), "%s", path);
    }

    SDL_Log("[UI] Imported scene %s", path);
    Global_OnSceneLoaded(path, layout_hint);
    UIPanel_RememberLoadedEntry(UI_LOAD_MENU_MODE_SCENE, path);
    UIPanel_RefreshConfigList();
    UIPanel_ResetEditorTransientSelection(&state->editor);
    UIPanel_RefreshViewportAfterSceneDocumentLoad(state);
    Editor_HistoryCapture(&state->editor, &state->layout);
    return true;
}

bool UIPanel_OpenJsonFolderDialog(void) {
    char selected_folder[LINE_DRAWING_PATH_CAP];
    char default_dir[LINE_DRAWING_PATH_CAP];

    UIPanel_GetDefaultLoadDirectory(default_dir, sizeof(default_dir));
    if (!UIPanel_SelectFolderWithPromptAndDefault("Choose sCulpt JSON Root",
                                                  default_dir,
                                                  selected_folder,
                                                  sizeof(selected_folder))) {
        SDL_Log("[UI] JSON root selection canceled.");
        return false;
    }

    if (!UIPanel_LoadJsonFromFolderSelection(selected_folder, true)) {
        SDL_Log("[UI] JSON root selection rejected: %s", selected_folder);
        return false;
    }

    return true;
}

bool UIPanel_OpenSceneFolderDialog(void) {
    char selected_folder[LINE_DRAWING_PATH_CAP];
    char default_dir[LINE_DRAWING_PATH_CAP];

    UIPanel_GetDefaultSceneSelectionDirectory(default_dir, sizeof(default_dir));
    if (!UIPanel_SelectFolderWithPromptAndDefault("Choose sCulpt Scene Folder or Scene Root",
                                                  default_dir,
                                                  selected_folder,
                                                  sizeof(selected_folder))) {
        SDL_Log("[UI] Scene folder selection canceled.");
        return false;
    }

    if (!UIPanel_LoadSceneFromFolderSelection(selected_folder, true)) {
        SDL_Log("[UI] Scene folder selection rejected: %s", selected_folder);
        return false;
    }

    return true;
}

bool UIPanel_OpenObjectAssetFolderDialog(void) {
    char selected_folder[LINE_DRAWING_PATH_CAP];
    char default_dir[LINE_DRAWING_PATH_CAP];

    UIPanel_GetDefaultObjectAssetSelectionDirectory(default_dir, sizeof(default_dir));
    if (!UIPanel_SelectFolderWithPromptAndDefault("Choose sCulpt Object Asset Root",
                                                  default_dir,
                                                  selected_folder,
                                                  sizeof(selected_folder))) {
        SDL_Log("[UI] Object asset root selection canceled.");
        return false;
    }

    if (!UIPanel_LoadObjectAssetFromFolderSelection(selected_folder, true)) {
        SDL_Log("[UI] Object asset root selection rejected: %s", selected_folder);
        return false;
    }

    return true;
}

bool UIPanel_OpenStlFolderDialog(void) {
    char selected_folder[LINE_DRAWING_PATH_CAP];
    char default_dir[LINE_DRAWING_PATH_CAP];

    UIPanel_GetDefaultObjectAssetSelectionDirectory(default_dir, sizeof(default_dir));
    if (!UIPanel_SelectFolderWithPromptAndDefault("Choose sCulpt STL Import Root",
                                                  default_dir,
                                                  selected_folder,
                                                  sizeof(selected_folder))) {
        SDL_Log("[UI] STL import root selection canceled.");
        return false;
    }

    if (!UIPanel_LoadStlFromFolderSelection(selected_folder, true)) {
        SDL_Log("[UI] STL import root selection rejected: %s", selected_folder);
        return false;
    }

    return true;
}

bool UIPanel_OpenDirectoryDialogForActiveBrowser(void) {
    UIPanelState* ui = UIPanel_Get();
    if (!ui) return false;
    switch (ui->loadMenu.mode) {
        case UI_LOAD_MENU_MODE_JSON:
            return UIPanel_OpenJsonFolderDialog();
        case UI_LOAD_MENU_MODE_SCENE:
            return UIPanel_OpenSceneFolderDialog();
        case UI_LOAD_MENU_MODE_OBJECT:
        case UI_LOAD_MENU_MODE_RUNTIME_MESH:
            return UIPanel_OpenObjectAssetFolderDialog();
        case UI_LOAD_MENU_MODE_STL_IMPORT:
            return UIPanel_OpenStlFolderDialog();
        case UI_LOAD_MENU_MODE_NONE:
        default:
            return false;
    }
}

void UIPanel_ExportScene(void) {
    GlobalState* state = Global_Get();
    LineDrawingSceneExportPaths export_paths;
    char diagnostics[256];
    char layout_hint[LINE_DRAWING_PATH_CAP];
    char export_hint[LINE_DRAWING_PATH_CAP];
    const char* output_root = NULL;
    const char* authoring_path = NULL;
    if (!state) return;

    output_root = Global_GetOutputRoot();
    authoring_path = Global_GetCurrentSceneAuthoringPath();
    Layout_CompactDeletedElements(&state->layout);

    snprintf(export_hint,
             sizeof(export_hint),
             "%s",
             (authoring_path && authoring_path[0] != '\0')
                 ? authoring_path
                 : Global_GetCurrentConfigPath());

    if (!LineDrawingSceneExport_ExportLayoutToOutputRoot(&state->layout,
                                                         export_hint,
                                                         output_root,
                                                         &export_paths,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        SDL_Log("[UI] Scene export failed: %s", diagnostics[0] ? diagnostics : "unknown error");
        {
            char status[320];
            snprintf(status,
                     sizeof(status),
                     "Export Scene failed: %s",
                     diagnostics[0] ? diagnostics : "unknown error");
            UIPanel_SetFilePaneActionStatus(status);
        }
        return;
    }

    SDL_Log("[UI] Exported scene directory to %s", export_paths.scene_dir);
    SDL_Log("[UI] Exported authoring scene to %s", export_paths.authoring_path);
    SDL_Log("[UI] Exported runtime scene to %s", export_paths.runtime_path);

    layout_hint[0] = '\0';
    if (!UIPanel_DeriveLayoutHintFromScenePath(export_paths.authoring_path,
                                               layout_hint,
                                               sizeof(layout_hint))) {
        snprintf(layout_hint, sizeof(layout_hint), "%.255s", export_paths.authoring_path);
    }
    Global_OnSceneLoaded(export_paths.authoring_path, layout_hint);
    UIPanel_RememberLoadedEntry(UI_LOAD_MENU_MODE_SCENE, export_paths.authoring_path);
    {
        char status[640];
        snprintf(status,
                 sizeof(status),
                 "Export Scene OK -> %s",
                 export_paths.authoring_path);
        UIPanel_SetFilePaneActionStatus(status);
    }
    UIPanel_RefreshConfigList();
}

bool UIPanel_ExportObjectRuntimeMesh(void) {
    GlobalState* state = Global_Get();
    ObjectAuthoringRuntimeMesh runtime_mesh;
    char source_asset_id[64];
    char runtime_asset_id[64];
    char path[LINE_DRAWING_PATH_CAP];
    char diagnostics[256];

    diagnostics[0] = '\0';
    ObjectAuthoringRuntimeMesh_Init(&runtime_mesh);
    if (!state || Global_GetWorkspaceMode() != LINE_DRAWING_WORKSPACE_MODE_OBJECT) {
        UIPanel_SetFilePaneActionStatus("Mesh export failed: switch to object mode.");
        return false;
    }
    if (!state->objectAuthoring.attached || state->objectAuthoring.document.operationCount == 0u) {
        Global_SetObjectRuntimeMeshStatus("Mesh export failed: no authored operations.");
        UIPanel_SetFilePaneActionStatus("Mesh export failed: no authored operations.");
        SDL_Log("[UI] Object runtime mesh export failed: no authored operations.");
        return false;
    }
    if (!UIPanel_BuildObjectRuntimeMeshPath(state,
                                            source_asset_id,
                                            sizeof(source_asset_id),
                                            runtime_asset_id,
                                            sizeof(runtime_asset_id),
                                            path,
                                            sizeof(path))) {
        Global_SetObjectRuntimeMeshStatus("Mesh export failed: invalid output path.");
        UIPanel_SetFilePaneActionStatus("Mesh export failed: invalid output path.");
        SDL_Log("[UI] Object runtime mesh export failed: invalid output path.");
        return false;
    }
    if (!ObjectAuthoring_CompileRuntimeMesh(&state->objectAuthoring.document,
                                            runtime_asset_id,
                                            source_asset_id,
                                            &runtime_mesh,
                                            diagnostics,
                                            sizeof(diagnostics))) {
        char status[160];
        snprintf(status,
                 sizeof(status),
                 "Mesh export failed: %s",
                 diagnostics[0] ? diagnostics : "compile error");
        Global_SetObjectRuntimeMeshStatus(status);
        UIPanel_SetFilePaneActionStatus(status);
        SDL_Log("[UI] Object runtime mesh compile failed: %s",
                diagnostics[0] ? diagnostics : "unknown error");
        ObjectAuthoringRuntimeMesh_Free(&runtime_mesh);
        return false;
    }
    if (!ObjectAuthoringRuntimeMesh_SaveFile(&runtime_mesh,
                                             path,
                                             diagnostics,
                                             sizeof(diagnostics))) {
        char status[160];
        snprintf(status,
                 sizeof(status),
                 "Mesh export failed: %s",
                 diagnostics[0] ? diagnostics : "write error");
        Global_SetObjectRuntimeMeshStatus(status);
        UIPanel_SetFilePaneActionStatus(status);
        SDL_Log("[UI] Object runtime mesh write failed: %s",
                diagnostics[0] ? diagnostics : "unknown error");
        ObjectAuthoringRuntimeMesh_Free(&runtime_mesh);
        return false;
    }
    {
        char status[160];
        snprintf(status,
                 sizeof(status),
                 "Mesh exported: %zu verts / %zu tris",
                 runtime_mesh.vertexCount,
                 runtime_mesh.triangleCount);
        Global_RecordObjectRuntimeMeshResult(path, status);
    }
    SDL_Log("[UI] Exported object runtime mesh to %s", path);
    ObjectAuthoringRuntimeMesh_Free(&runtime_mesh);
    return true;
}

static bool UIPanel_PlaceRuntimeMeshAsSceneInstanceWithOptions(const char* runtime_mesh_path,
                                                               bool place_at_origin,
                                                               bool allow_scale_up) {
    GlobalState* state = Global_Get();
    Transform3D transform;
    uint32_t object_id = 0u;
    char resolved_path[LINE_DRAWING_PATH_CAP];
    char diagnostics[256];

    diagnostics[0] = '\0';
    resolved_path[0] = '\0';
    if (!state || Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT) {
        Global_SetObjectRuntimeMeshStatus("Mesh placement failed: switch to scene mode.");
        UIPanel_SetFilePaneActionStatus("Mesh placement failed: switch to scene mode.");
        return false;
    }
    if (!runtime_mesh_path || runtime_mesh_path[0] == '\0') {
        Global_SetObjectRuntimeMeshStatus("Mesh placement failed: choose or export a runtime mesh first.");
        UIPanel_SetFilePaneActionStatus("Mesh placement failed: choose or export a runtime mesh first.");
        SDL_Log("[UI] Mesh placement blocked: choose or export an object runtime mesh first.");
        return false;
    }
    snprintf(resolved_path, sizeof(resolved_path), "%s", runtime_mesh_path);

    transform = Layout_Transform3D_Default();
    {
        float auto_scale = 1.0f;
        if (UIPanel_ResolveRuntimeMeshAutoScale(&state->layout,
                                                resolved_path,
                                                allow_scale_up,
                                                &auto_scale) &&
            auto_scale > 0.0f &&
            fabsf(auto_scale - 1.0f) > 1e-4f) {
            transform.scale = (Vec3){ auto_scale, auto_scale, auto_scale };
        }
    }
    if (!place_at_origin &&
        Layout_ConstructionPlane3D_IsValid(&state->layout.scene3d.constructionPlane)) {
        const ConstructionPlane3D* plane = &state->layout.scene3d.constructionPlane;
        if (plane->mode == CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME) {
            transform.position = plane->customFrame.origin;
        } else {
            switch (plane->axisAligned.axis) {
                case VIEW_PLANE_YZ:
                    transform.position.x = plane->axisAligned.offset;
                    break;
                case VIEW_PLANE_XZ:
                    transform.position.y = plane->axisAligned.offset;
                    break;
                case VIEW_PLANE_XY:
                default:
                    transform.position.z = plane->axisAligned.offset;
                    break;
            }
        }
    }

    Editor_HistoryCapture(&state->editor, &state->layout);
    if (!Layout_CreateMeshAssetInstanceFromRuntimeAsset(&state->layout,
                                                        resolved_path,
                                                        &transform,
                                                        &object_id,
                                                        diagnostics,
                                                        sizeof(diagnostics))) {
        SDL_Log("[UI] Mesh placement failed: %s",
                diagnostics[0] ? diagnostics : "unknown error");
        {
            char status[160];
            snprintf(status,
                     sizeof(status),
                     "Mesh placement failed: %s",
                     diagnostics[0] ? diagnostics : "unknown error");
            Global_SetObjectRuntimeMeshStatus(status);
            UIPanel_SetFilePaneActionStatus(status);
        }
        return false;
    }

    Editor_ClearAnchorSelection(&state->editor);
    state->editor.selectedObject3DId = object_id;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    state->editor.primitivePlacementPreview = PRIMITIVE_PLACEMENT_PREVIEW_NONE;
    state->editor.selectedWallIndex = -1;
    state->editor.selectedHandleAnchor = -1;
    state->editor.selectedHandleComponent = -1;
    snprintf(diagnostics, sizeof(diagnostics), "Mesh placed: %s", resolved_path);
    Global_RecordObjectRuntimeMeshResult(resolved_path, diagnostics);
    Global_FlagHitboxesDirty();
    SDL_Log("[UI] Placed mesh asset instance id=%u from %s",
            object_id,
            resolved_path);
    return true;
}

bool UIPanel_PlaceRuntimeMeshAsSceneInstance(const char* runtime_mesh_path) {
    return UIPanel_PlaceRuntimeMeshAsSceneInstanceWithOptions(runtime_mesh_path, false, false);
}

bool UIPanel_ImportStlAndPlaceFromPath(const char* stl_path) {
    GlobalState* state = Global_Get();
    const char* asset_root = Global_GetObjectAssetRoot();
    char authoring_path[LINE_DRAWING_PATH_CAP];
    char runtime_path[LINE_DRAWING_PATH_CAP];
    char diagnostics[256];
    bool placed = false;

    diagnostics[0] = '\0';
    authoring_path[0] = '\0';
    runtime_path[0] = '\0';
    if (!state || !stl_path || !stl_path[0]) return false;
    if (Global_GetWorkspaceMode() == LINE_DRAWING_WORKSPACE_MODE_OBJECT) {
        Global_SetObjectRuntimeMeshStatus("STL import failed: switch to scene mode.");
        UIPanel_SetFilePaneActionStatus("STL import failed: switch to scene mode.");
        SDL_Log("[UI] STL import placement is scene-mode only for this slice.");
        return false;
    }
    if (!asset_root || !asset_root[0]) {
        Global_SetObjectRuntimeMeshStatus("STL import failed: object asset root is unset.");
        UIPanel_SetFilePaneActionStatus("STL import failed: object asset root is unset.");
        SDL_Log("[UI] STL import failed: object asset root is unset.");
        return false;
    }

    if (!LayoutImportedMeshAsset_ImportStlToRuntime(stl_path,
                                                    asset_root,
                                                    authoring_path,
                                                    sizeof(authoring_path),
                                                    runtime_path,
                                                    sizeof(runtime_path),
                                                    diagnostics,
                                                    sizeof(diagnostics))) {
        char status[160];
        snprintf(status,
                 sizeof(status),
                 "STL import failed: %s",
                 diagnostics[0] ? diagnostics : "import error");
        Global_SetObjectRuntimeMeshStatus(status);
        UIPanel_SetFilePaneActionStatus(status);
        SDL_Log("[UI] STL import failed for %s: %s",
                stl_path,
                diagnostics[0] ? diagnostics : "unknown error");
        return false;
    }

    placed = UIPanel_PlaceImportedStlRuntimeMesh(stl_path, authoring_path, runtime_path);
    return placed;
}

bool UIPanel_PlaceImportedStlRuntimeMesh(const char* stl_path,
                                         const char* authoring_path,
                                         const char* runtime_path) {
    GlobalState* state = Global_Get();
    bool placed = false;
    if (!state || !stl_path || !stl_path[0] || !runtime_path || !runtime_path[0]) {
        Global_SetObjectRuntimeMeshStatus("STL import failed: placement path is missing.");
        UIPanel_SetFilePaneActionStatus("STL import failed: placement path is missing.");
        return false;
    }
    placed = UIPanel_PlaceRuntimeMeshAsSceneInstanceWithOptions(runtime_path, true, true);
    if (!placed) return false;

    {
        char status[160];
        snprintf(status, sizeof(status), "STL imported at origin: %s", runtime_path);
        Global_RecordObjectRuntimeMeshResult(runtime_path, status);
    }
    SDL_Log("[UI] Imported STL %s to %s and %s",
            stl_path,
            authoring_path && authoring_path[0] ? authoring_path : "(unknown authoring path)",
            runtime_path);
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();
    return true;
}

bool UIPanel_PlaceLastRuntimeMeshAsSceneInstance(void) {
    char runtime_mesh_path[LINE_DRAWING_PATH_CAP];
    const char* last_runtime_mesh_path = Global_GetLastObjectRuntimeMeshPath();
    if (!last_runtime_mesh_path || last_runtime_mesh_path[0] == '\0') {
        SDL_Log("[UI] Mesh placement blocked: export an object runtime mesh first.");
        return false;
    }
    snprintf(runtime_mesh_path,
             sizeof(runtime_mesh_path),
             "%s",
             last_runtime_mesh_path);
    return UIPanel_PlaceRuntimeMeshAsSceneInstance(runtime_mesh_path);
}

void UIPanel_BeginInputRootDialog(void) {
    UIPanel_BeginRootDialog(UI_ROOT_TARGET_INPUT);
}

void UIPanel_BeginOutputRootDialog(void) {
    UIPanel_BeginRootDialog(UI_ROOT_TARGET_OUTPUT);
}

void UIPanel_BeginObjectAssetRootDialog(void) {
    UIPanel_BeginRootDialog(UI_ROOT_TARGET_OBJECT_ASSET);
}

bool UIPanel_OpenInputRootFolderDialog(void) {
    char path[256];
    UIPanelState* ui = UIPanel_Get();
    if (!UIPanel_SelectFolderWithPrompt("Choose sCulpt Session Input Root", path, sizeof(path))) {
        SDL_Log("[UI] Session input root selection canceled.");
        return false;
    }
    if (!Global_SetInputRoot(path, true)) {
        SDL_Log("[UI] Failed to set session input root to %s", path);
        return false;
    }
    if (UIPanel_ShouldRefreshBrowserForEditedRoot(ui, UI_ROOT_TARGET_INPUT)) {
        UIPanel_RefreshConfigList();
    }
    SDL_Log("[UI] Session input root updated: %s", path);
    return true;
}

bool UIPanel_OpenOutputRootFolderDialog(void) {
    char path[256];
    if (!UIPanel_SelectFolderWithPrompt("Choose sCulpt Output Root", path, sizeof(path))) {
        SDL_Log("[UI] Output root selection canceled.");
        return false;
    }
    if (!Global_SetOutputRoot(path, true)) {
        SDL_Log("[UI] Failed to set output root to %s", path);
        return false;
    }
    SDL_Log("[UI] Output root updated: %s", path);
    return true;
}

bool UIPanel_NewObjectAssetDocument(void) {
    GlobalState* state = Global_Get();
    char default_path[LINE_DRAWING_PATH_CAP];
    if (!state) return false;

    Layout_Free(&state->layout);
    Layout_Init(&state->layout, state->grid.gridSize > 0.0f ? state->grid.gridSize : 1.0f);
    (void)ObjectAuthoringSession_ResetFromLayout(&state->objectAuthoring,
                                                 &state->layout,
                                                 0u);
    state->activePlane = Layout_ConstructionPlane3D_ToViewPlane(&state->layout.scene3d.constructionPlane);
    Editor_ClearHistory(&state->editor);
    state->editor.selectedAnchorIndex = -1;
    state->editor.selectedWallIndex = -1;
    state->editor.selectedObject3DId = 0u;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    state->editor.hoveredAnchorIndex = -1;
    state->editor.hoveredWallIndex = -1;
    state->editor.hoveredObject3DId = 0u;
    state->editor.hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    state->editor.hoveredHandleAnchor = -1;
    state->editor.hoveredHandleComponent = -1;
    state->editor.hoveredGizmoAxis = -1;
    state->editor.hoveredObject3DGizmoAxis = -1;
    state->editor.activeObject3DGizmoAxis = -1;

    default_path[0] = '\0';
    if (!LineDrawingDataPaths_BuildPath(default_path,
                                        sizeof(default_path),
                                        Global_GetObjectAssetRoot() ? Global_GetObjectAssetRoot() : k_legacy_layout_root,
                                        "object_asset.json")) {
        snprintf(default_path, sizeof(default_path), "%s/object_asset.json", k_legacy_layout_root);
    }

    Global_OnObjectAssetLoaded(default_path);
    Editor_HistoryCapture(&state->editor, &state->layout);
    UIPanel_RefreshConfigList();
    return true;
}
