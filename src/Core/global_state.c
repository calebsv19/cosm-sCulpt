// src/Core/global_state.c
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Core/workspace/line_drawing_workspace_mode_handoff.h"
#include "Layout/hitbox_system.h"
#include "UI/ui_panel.h"
#include "UI/workspace_authoring/line_drawing_workspace_authoring_host.h"
#include "Layout/layout_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

static GlobalState* global = NULL;
static const char* k_space_mode_runtime_path = "data/runtime/space_mode.txt";
static const char* k_space_mode_legacy_path = "config/space_mode.txt";
static const char* k_default_layout_filename = "layout_config.json";
static const char* k_default_object_asset_filename = "object_asset.json";

static bool EnsureRuntimeDir(void) {
    if (mkdir("data", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    if (mkdir("data/runtime", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static bool Global_SetPathField(char* dst, size_t dst_size, const char* value) {
    size_t len = 0;
    if (!dst || dst_size == 0 || !value || value[0] == '\0') return false;
    len = strlen(value);
    while (len > 0 && isspace((unsigned char)value[len - 1])) {
        --len;
    }
    if (len == 0) return false;
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, value, len);
    dst[len] = '\0';
    return true;
}

static bool Global_DirExists(const char* path) {
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static void Global_BuildDefaultLayoutPath(const GlobalState* state,
                                          char* out_path,
                                          size_t out_path_size) {
    if (!out_path || out_path_size == 0u) return;
    out_path[0] = '\0';
    if (!state ||
        !LineDrawingDataPaths_BuildPath(out_path,
                                        out_path_size,
                                        state->dataPaths.input_root,
                                        k_default_layout_filename)) {
        snprintf(out_path, out_path_size, "config/layout_config.json");
    }
}

static void Global_BuildDefaultObjectAssetPath(const GlobalState* state,
                                               char* out_path,
                                               size_t out_path_size) {
    if (!out_path || out_path_size == 0u) return;
    out_path[0] = '\0';
    if (!state ||
        !LineDrawingDataPaths_BuildPath(out_path,
                                        out_path_size,
                                        state->dataPaths.object_asset_root,
                                        k_default_object_asset_filename)) {
        snprintf(out_path, out_path_size, "config/object_asset.json");
    }
}

static void Global_SetDefaultLayoutPath(GlobalState* state) {
    char default_path[LINE_DRAWING_PATH_CAP];
    if (!state) return;
    Global_BuildDefaultLayoutPath(state, default_path, sizeof(default_path));
    snprintf(state->currentConfigPath,
             sizeof(state->currentConfigPath),
             "%s",
             default_path);
    snprintf(state->lastLayoutPath,
             sizeof(state->lastLayoutPath),
                 "%s",
                 default_path);
}

static void Global_SetDefaultObjectAssetPath(GlobalState* state) {
    char default_path[LINE_DRAWING_PATH_CAP];
    if (!state) return;
    Global_BuildDefaultObjectAssetPath(state, default_path, sizeof(default_path));
    snprintf(state->currentObjectAssetPath,
             sizeof(state->currentObjectAssetPath),
             "%s",
             default_path);
    snprintf(state->lastObjectAssetPath,
             sizeof(state->lastObjectAssetPath),
             "%s",
             default_path);
}

static void Global_UpdateDefaultLayoutPathForInputRootChange(GlobalState* state,
                                                             const char* prior_input_root) {
    char old_default_path[LINE_DRAWING_PATH_CAP];
    char new_default_path[LINE_DRAWING_PATH_CAP];

    if (!state) return;

    old_default_path[0] = '\0';
    if (!LineDrawingDataPaths_BuildPath(old_default_path,
                                        sizeof(old_default_path),
                                        prior_input_root,
                                        k_default_layout_filename)) {
        snprintf(old_default_path, sizeof(old_default_path), "config/layout_config.json");
    }
    Global_BuildDefaultLayoutPath(state, new_default_path, sizeof(new_default_path));

    if (state->currentConfigPath[0] == '\0' ||
        strcmp(state->currentConfigPath, old_default_path) == 0) {
        snprintf(state->currentConfigPath,
                 sizeof(state->currentConfigPath),
                 "%s",
                 new_default_path);
    }
    if (state->lastLayoutPath[0] == '\0' ||
        strcmp(state->lastLayoutPath, old_default_path) == 0) {
        snprintf(state->lastLayoutPath,
                 sizeof(state->lastLayoutPath),
                 "%s",
                 new_default_path);
    }
}

static void Global_UpdateDefaultObjectAssetPathForRootChange(GlobalState* state,
                                                             const char* prior_root) {
    char old_default_path[LINE_DRAWING_PATH_CAP];
    char new_default_path[LINE_DRAWING_PATH_CAP];

    if (!state) return;

    old_default_path[0] = '\0';
    if (!LineDrawingDataPaths_BuildPath(old_default_path,
                                        sizeof(old_default_path),
                                        prior_root,
                                        k_default_object_asset_filename)) {
        snprintf(old_default_path, sizeof(old_default_path), "config/object_asset.json");
    }
    Global_BuildDefaultObjectAssetPath(state, new_default_path, sizeof(new_default_path));

    if (state->currentObjectAssetPath[0] == '\0' ||
        strcmp(state->currentObjectAssetPath, old_default_path) == 0) {
        snprintf(state->currentObjectAssetPath,
                 sizeof(state->currentObjectAssetPath),
                 "%s",
                 new_default_path);
    }
    if (state->lastObjectAssetPath[0] == '\0' ||
        strcmp(state->lastObjectAssetPath, old_default_path) == 0) {
        snprintf(state->lastObjectAssetPath,
                 sizeof(state->lastObjectAssetPath),
                 "%s",
                 new_default_path);
    }
}

static void Global_SeedLastSessionPathsFromRecents(GlobalState* state) {
    if (!state) return;
    if (state->recentContexts.layouts.count > 0 &&
        state->recentContexts.layouts.paths[0][0] != '\0') {
        snprintf(state->lastLayoutPath,
                 sizeof(state->lastLayoutPath),
                 "%s",
                 state->recentContexts.layouts.paths[0]);
    }
    if (state->recentContexts.scenes.count > 0 &&
        state->recentContexts.scenes.paths[0][0] != '\0') {
        snprintf(state->lastSceneAuthoringPath,
                 sizeof(state->lastSceneAuthoringPath),
                 "%s",
                 state->recentContexts.scenes.paths[0]);
    }
    if (state->recentContexts.object_assets.count > 0 &&
        state->recentContexts.object_assets.paths[0][0] != '\0') {
        snprintf(state->lastObjectAssetPath,
                 sizeof(state->lastObjectAssetPath),
                 "%s",
                 state->recentContexts.object_assets.paths[0]);
    }
}

static bool Global_ApplyStartupRootFallbacks(GlobalState* state) {
    bool changed = false;
    if (!state) return false;

    if (state->dataPaths.input_root[0] == '\0') {
        fprintf(stderr,
                "[startup] Input root unset; using '%s'.\n",
                LineDrawingDataPaths_DefaultInputRoot());
        snprintf(state->dataPaths.input_root,
                 sizeof(state->dataPaths.input_root),
                 "%s",
                 LineDrawingDataPaths_DefaultInputRoot());
        changed = true;
    } else if (!Global_DirExists(state->dataPaths.input_root)) {
        fprintf(stderr,
                "[startup] Startup fallback: input root '%s' missing; using '%s'.\n",
                state->dataPaths.input_root,
                LineDrawingDataPaths_DefaultInputRoot());
        snprintf(state->dataPaths.input_root,
                 sizeof(state->dataPaths.input_root),
                 "%s",
                 LineDrawingDataPaths_DefaultInputRoot());
        changed = true;
    }

    if (state->dataPaths.output_root[0] == '\0') {
        fprintf(stderr,
                "[startup] Output root unset; using '%s'.\n",
                LineDrawingDataPaths_DefaultOutputRoot());
        snprintf(state->dataPaths.output_root,
                 sizeof(state->dataPaths.output_root),
                 "%s",
                 LineDrawingDataPaths_DefaultOutputRoot());
        changed = true;
    } else if (!Global_DirExists(state->dataPaths.output_root)) {
        fprintf(stderr,
                "[startup] Startup fallback: output root '%s' missing; using '%s'.\n",
                state->dataPaths.output_root,
                LineDrawingDataPaths_DefaultOutputRoot());
        snprintf(state->dataPaths.output_root,
                 sizeof(state->dataPaths.output_root),
                 "%s",
                 LineDrawingDataPaths_DefaultOutputRoot());
        changed = true;
    }

    if (state->dataPaths.layout_root[0] == '\0') {
        fprintf(stderr,
                "[startup] Layout root unset; using '%s'.\n",
                LineDrawingDataPaths_DefaultLayoutRoot());
        snprintf(state->dataPaths.layout_root,
                 sizeof(state->dataPaths.layout_root),
                 "%s",
                 LineDrawingDataPaths_DefaultLayoutRoot());
        changed = true;
    } else if (!Global_DirExists(state->dataPaths.layout_root)) {
        fprintf(stderr,
                "[startup] Startup fallback: layout root '%s' missing; using '%s'.\n",
                state->dataPaths.layout_root,
                LineDrawingDataPaths_DefaultLayoutRoot());
        snprintf(state->dataPaths.layout_root,
                 sizeof(state->dataPaths.layout_root),
                 "%s",
                 LineDrawingDataPaths_DefaultLayoutRoot());
        changed = true;
    }

    if (state->dataPaths.object_asset_root[0] == '\0') {
        fprintf(stderr,
                "[startup] Object asset root unset; using '%s'.\n",
                LineDrawingDataPaths_DefaultObjectAssetRoot());
        snprintf(state->dataPaths.object_asset_root,
                 sizeof(state->dataPaths.object_asset_root),
                 "%s",
                 LineDrawingDataPaths_DefaultObjectAssetRoot());
        changed = true;
    } else if (!Global_DirExists(state->dataPaths.object_asset_root)) {
        fprintf(stderr,
                "[startup] Startup fallback: object asset root '%s' missing; using '%s'.\n",
                state->dataPaths.object_asset_root,
                LineDrawingDataPaths_DefaultObjectAssetRoot());
        snprintf(state->dataPaths.object_asset_root,
                 sizeof(state->dataPaths.object_asset_root),
                 "%s",
                 LineDrawingDataPaths_DefaultObjectAssetRoot());
        changed = true;
    }

    return changed;
}

static void Global_RecordRecentLayout(GlobalState* state, const char* path, bool persist) {
    bool changed = false;
    if (!state) return;
    changed = LineDrawingRecentContexts_TrackLayout(&state->recentContexts, path);
    if (persist && changed) {
        (void)Global_SaveRecentContexts();
    }
}

static void Global_RecordRecentScene(GlobalState* state, const char* path, bool persist) {
    bool changed = false;
    if (!state) return;
    changed = LineDrawingRecentContexts_TrackScene(&state->recentContexts, path);
    if (persist && changed) {
        (void)Global_SaveRecentContexts();
    }
}

static void Global_RecordRecentObjectAsset(GlobalState* state, const char* path, bool persist) {
    bool changed = false;
    if (!state) return;
    changed = LineDrawingRecentContexts_TrackObjectAsset(&state->recentContexts, path);
    if (persist && changed) {
        (void)Global_SaveRecentContexts();
    }
}

static void Global_RecordRecentInputRoot(GlobalState* state, const char* path, bool persist) {
    bool changed = false;
    if (!state) return;
    changed = LineDrawingRecentContexts_TrackInputRoot(&state->recentContexts, path);
    if (persist && changed) {
        (void)Global_SaveRecentContexts();
    }
}

static void Global_RecordRecentOutputRoot(GlobalState* state, const char* path, bool persist) {
    bool changed = false;
    if (!state) return;
    changed = LineDrawingRecentContexts_TrackOutputRoot(&state->recentContexts, path);
    if (persist && changed) {
        (void)Global_SaveRecentContexts();
    }
}

const char* Global_GetSpaceModeLabel(SpaceMode mode) {
    return mode == SPACE_MODE_2D ? "2D" : "3D";
}

const char* Global_GetWorkspaceModeLabel(LineDrawingWorkspaceMode mode) {
    return mode == LINE_DRAWING_WORKSPACE_MODE_OBJECT
               ? "Object Workspace"
               : "Scene Workspace";
}

static bool SpaceMode_Parse(const char* text, SpaceMode* out_mode) {
    if (!text || !out_mode) return false;

    char normalized[16];
    size_t count = 0;
    for (const char* p = text; *p && count + 1 < sizeof(normalized); ++p) {
        unsigned char c = (unsigned char)*p;
        if (isspace(c)) continue;
        normalized[count++] = (char)tolower(c);
    }
    normalized[count] = '\0';

    if (strcmp(normalized, "2d") == 0) {
        *out_mode = SPACE_MODE_2D;
        return true;
    }
    if (strcmp(normalized, "3d") == 0) {
        *out_mode = SPACE_MODE_3D;
        return true;
    }
    return false;
}

SpaceMode Global_GetSpaceMode(void) {
    if (!global) return SPACE_MODE_3D;
    return global->spaceMode;
}

bool Global_SaveSpaceMode(void) {
    if (!global) return false;
    if (!EnsureRuntimeDir()) return false;
    FILE* fp = fopen(k_space_mode_runtime_path, "wb");
    if (!fp) return false;
    const char* mode_text = (global->spaceMode == SPACE_MODE_2D) ? "2d\n" : "3d\n";
    const size_t mode_len = strlen(mode_text);
    const bool ok = fwrite(mode_text, 1, mode_len, fp) == mode_len;
    fclose(fp);
    return ok;
}

bool Global_SetSpaceMode(SpaceMode mode, bool persist) {
    if (!global) return false;
    if (mode != SPACE_MODE_2D && mode != SPACE_MODE_3D) return false;

    global->spaceMode = mode;
    if (mode == SPACE_MODE_2D) {
        // 2D mode is always XY at z=0 with no free-view camera controls.
        global->activePlane.axis = VIEW_PLANE_XY;
        global->activePlane.offset = 0.0f;
        Layout_ConstructionPlane3D_SetFromViewPlane(&global->layout.scene3d.constructionPlane,
                                                    global->activePlane);
        global->freeViewCamera.enabled = false;
    }

    Global_FlagHitboxesDirty();
    if (persist) {
        return Global_SaveSpaceMode();
    }
    return true;
}

bool Global_ToggleSpaceMode(bool persist) {
    if (!global) return false;
    SpaceMode next = (global->spaceMode == SPACE_MODE_2D) ? SPACE_MODE_3D : SPACE_MODE_2D;
    return Global_SetSpaceMode(next, persist);
}

bool Global_LoadSpaceMode(void) {
    if (!global) return false;
    FILE* fp = fopen(k_space_mode_runtime_path, "rb");
    if (!fp) {
        fp = fopen(k_space_mode_legacy_path, "rb");
    }
    if (!fp) return false;

    char buffer[32];
    const size_t count = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[count] = '\0';
    fclose(fp);

    SpaceMode loaded = SPACE_MODE_3D;
    if (!SpaceMode_Parse(buffer, &loaded)) return false;
    return Global_SetSpaceMode(loaded, false);
}

LineDrawingWorkspaceMode Global_GetWorkspaceMode(void) {
    if (!global) return LINE_DRAWING_WORKSPACE_MODE_SCENE;
    return global->workspaceMode;
}

bool Global_SetWorkspaceMode(LineDrawingWorkspaceMode mode) {
    if (!global) return false;
    if (mode != LINE_DRAWING_WORKSPACE_MODE_SCENE &&
        mode != LINE_DRAWING_WORKSPACE_MODE_OBJECT) {
        return false;
    }
    if (global->workspaceMode == mode) {
        return true;
    }

    if (!LineDrawingWorkspaceModeHandoff_Apply(global, mode)) {
        return false;
    }
    UIPanel_OnWindowResized(global->screenWidth, global->screenHeight);
    Global_FlagGridChanged();
    return true;
}

bool Global_ToggleWorkspaceMode(void) {
    if (!global) return false;
    return Global_SetWorkspaceMode(
        global->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_SCENE
            ? LINE_DRAWING_WORKSPACE_MODE_OBJECT
            : LINE_DRAWING_WORKSPACE_MODE_SCENE);
}

bool Global_IsCenterCrosshairEnabled(void) {
    if (!global) return false;
    return global->centerCrosshairEnabled;
}

bool Global_SetCenterCrosshairEnabled(bool enabled) {
    if (!global) return false;
    global->centerCrosshairEnabled = enabled;
    return true;
}

bool Global_ToggleCenterCrosshair(void) {
    if (!global) return false;
    global->centerCrosshairEnabled = !global->centerCrosshairEnabled;
    return true;
}

static void Global_UpdateSavedSnapshot(void) {
    if (!global) return;
    if (global->lastSavedSnapshot) {
        Layout_FreeString(global->lastSavedSnapshot);
        global->lastSavedSnapshot = NULL;
    }
    global->lastSavedSnapshot = Layout_SaveToString(&global->layout);
}

static void Global_ProcessLayoutChanges(GlobalState* state) {
    if (!state || !state->layoutDirty) return;
    Layout_CompactDeletedElements(&state->layout);
    state->layoutDirty = false;
    state->hitboxDirty = true;
}

GlobalState* Global_Get(void) {
    return global;
}

LineDrawingPaneHost* Global_GetPaneHost(void) {
    if (!global) return NULL;
    return &global->paneHost;
}

const LineDrawingPaneHost* Global_GetPaneHostConst(void) {
    if (!global) return NULL;
    return &global->paneHost;
}

static void Global_ApplyPaneChromeTargets(GlobalState* state) {
    UIPanelLayoutMetrics metrics;
    if (!state || !state->paneHost.initialized) return;
    UIPanel_GetLayoutMetrics(&metrics);
    LineDrawingPaneHost_SetChromeTargets(&state->paneHost,
                                         (float)metrics.desired_top_pane_height_px,
                                         (float)metrics.desired_left_pane_width_px,
                                         (float)metrics.desired_right_pane_width_px);
}

void Global_Init(int screenWidth, int screenHeight) {
    global = malloc(sizeof(GlobalState));
    memset(global, 0, sizeof(*global));
    global->screenWidth = screenWidth;
    global->screenHeight = screenHeight;
    global->layoutDirty = true;
    global->hitboxDirty = true;
    global->spaceMode = SPACE_MODE_3D;
    global->activePlane = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f };
    global->freeViewCamera = (FreeViewCamera){
        .enabled = false,
        .yawDeg = 35.0f,
        .pitchDeg = 20.0f,
        .target = {0.0f, 0.0f, 0.0f}
    };
    global->centerCrosshairEnabled = true;
    global->layoutDirtySinceSave = false;
    global->lastSavedSnapshot = NULL;
    memset(&global->sceneWorkspaceDocument, 0, sizeof(global->sceneWorkspaceDocument));
    memset(&global->objectWorkspaceDocument, 0, sizeof(global->objectWorkspaceDocument));
    ObjectAuthoringSession_Init(&global->objectAuthoring);
    ObjectAuthoringSession_Init(&global->sceneWorkspaceDocument.objectAuthoring);
    ObjectAuthoringSession_Init(&global->objectWorkspaceDocument.objectAuthoring);
    LineDrawingDataPaths_SetDefaults(&global->dataPaths);
    LineDrawingRecentContexts_Init(&global->recentContexts);
    Global_LoadDataRoots();
    Global_LoadRecentContexts();
    if (Global_ApplyStartupRootFallbacks(global)) {
        if (!Global_SaveDataRoots()) {
            fprintf(stderr, "[startup] Failed to persist corrected root fallbacks.\n");
        }
    } else {
        Global_SaveDataRoots();
    }
    Global_RecordRecentInputRoot(global, global->dataPaths.input_root, true);
    Global_RecordRecentOutputRoot(global, global->dataPaths.output_root, true);
    Global_SetDefaultLayoutPath(global);
    Global_SetDefaultObjectAssetPath(global);
    Global_SeedLastSessionPathsFromRecents(global);

    Grid_init(&global->grid, 1.0f, screenWidth, screenHeight);

    Layout_Init(&global->layout, 1.0f);
    global->spaceMode = SPACE_MODE_3D;
    global->workspaceMode = LINE_DRAWING_WORKSPACE_MODE_SCENE;
    global->activePlane = Layout_ConstructionPlane3D_ToViewPlane(&global->layout.scene3d.constructionPlane);
    Editor_Init(&global->editor);
    if (!LineDrawingPaneHost_Init(&global->paneHost, (float)screenWidth, (float)screenHeight)) {
        fprintf(stderr, "[Core] pane host init failed: %s\n", LineDrawingPaneHost_LastError(&global->paneHost));
    }
    LineDrawingWorkspaceAuthoringHost_Reset(&global->workspaceAuthoring);
    UIPanel_Init(screenWidth, screenHeight);
    Global_SetWindowSize(screenWidth, screenHeight);

    Global_SetSpaceMode(global->spaceMode, false);
    Global_LoadSpaceMode();
    Editor_HistoryCapture(&global->editor, &global->layout);
    Global_UpdateSavedSnapshot();
}


void Global_Shutdown(void) {
    if (global->lastSavedSnapshot) {
        Layout_FreeString(global->lastSavedSnapshot);
        global->lastSavedSnapshot = NULL;
    }
    Layout_FreeString(global->sceneWorkspaceDocument.layoutSnapshot);
    free(global->sceneWorkspaceDocument.savedSnapshot);
    Layout_FreeString(global->objectWorkspaceDocument.layoutSnapshot);
    free(global->objectWorkspaceDocument.savedSnapshot);
    ObjectAuthoringSession_Free(&global->objectAuthoring);
    ObjectAuthoringSession_Free(&global->sceneWorkspaceDocument.objectAuthoring);
    ObjectAuthoringSession_Free(&global->objectWorkspaceDocument.objectAuthoring);
    Editor_Free(&global->editor);
    Layout_Free(&global->layout);
    free(global);
    global = NULL;
}



void Global_TickSystems(AppContext* ctx) {
    (void)ctx;
    GlobalState* state = Global_Get();
    if (!state) return;

    Global_ProcessLayoutChanges(state);

    Global_RebuildHitboxesIfDirty();

    // Future: add tick handlers for other systems here (UI, animations, etc.)
}


void Global_SetWindowSize(int w, int h) {
    if (!global) return;
    global->screenWidth = w;
    global->screenHeight = h;
    if (global->paneHost.target_top_height <= 0.0f ||
        global->paneHost.target_left_width <= 0.0f ||
        global->paneHost.target_right_width <= 0.0f) {
        Global_ApplyPaneChromeTargets(global);
    }
    if (global->paneHost.initialized &&
        !LineDrawingPaneHost_Rebuild(&global->paneHost, (float)w, (float)h)) {
        fprintf(stderr, "[Core] pane host rebuild failed: %s\n",
                LineDrawingPaneHost_LastError(&global->paneHost));
    }
    UIPanel_OnWindowResized(w, h);
    Global_FlagGridChanged();
}

int Global_GetScreenWidth(void) { return global->screenWidth; }
int Global_GetScreenHeight(void) { return global->screenHeight; }

void Global_FlagLayoutChanged(void) {
    GlobalState* state = Global_Get();
    if (!state) return;
    state->layoutDirty = true;
    state->layoutDirtySinceSave = true;
    Global_FlagHitboxesDirty();
}

void Global_FlagGridChanged(void) {
    Global_FlagHitboxesDirty();
}

void Global_FlagHitboxesDirty(void) {
    GlobalState* state = Global_Get();
    if (!state) return;
    state->hitboxDirty = true;
}

void Global_RebuildHitboxesIfDirty(void) {
    GlobalState* state = Global_Get();
    ObjectFaceSketchHitboxState object_face_sketch = {0};
    const ObjectAuthoringDocument* object_topology = NULL;
    bool object_topology_edit_mode = false;
    if (!state) return;

    Global_ProcessLayoutChanges(state);

    if (!state->hitboxDirty) return;
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    bool gizmoEnabled = (state->spaceMode == SPACE_MODE_3D) &&
                        SpaceAdapter_IsFreeViewEnabled(&viewCtx);
    const bool has_object_authoring_sketch =
        state->objectAuthoring.attached &&
        ObjectAuthoringDocument_ActiveSketch(&state->objectAuthoring.document) != NULL;
    if (state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_OBJECT &&
        (state->editor.selectedObjectAssetFace != OBJECT3D_FACE_NONE ||
         state->editor.objectFaceSketchToolArmed ||
         state->editor.objectFaceSketchDragging ||
         state->editor.objectFaceSketchHasRectangle ||
         has_object_authoring_sketch ||
         state->editor.objectFaceExtrudeToolArmed ||
         state->editor.objectFaceExtrudeDragging)) {
        gizmoEnabled = false;
    }
    if (state->objectAuthoring.attached) {
        const ObjectAuthoringSketch* sketch =
            ObjectAuthoringDocument_ActiveSketch(&state->objectAuthoring.document);
        if (state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_OBJECT) {
            object_topology = &state->objectAuthoring.document;
            object_topology_edit_mode =
                state->editor.objectEditSelectionMode == OBJECT_EDIT_SELECTION_EDGE ||
                state->editor.objectEditSelectionMode == OBJECT_EDIT_SELECTION_VERTEX;
        }
        if (sketch &&
            state->editor.selectedObjectAssetBodyId == sketch->faceRef.bodyId &&
            state->editor.selectedObjectAssetFace == sketch->faceRef.primitiveFace) {
            object_face_sketch.visible = true;
            object_face_sketch.bodyId = sketch->faceRef.bodyId;
            object_face_sketch.frame = sketch->frame;
            object_face_sketch.minUV = sketch->minUV;
            object_face_sketch.maxUV = sketch->maxUV;
        }
    }
    HitboxSystem_Rebuild(&state->layout,
                         state->grid.scale,
                         state->grid.offsetX,
                         state->grid.offsetY,
                         viewCtx.plane,
                         SpaceAdapter_Camera(&viewCtx),
                         state->editor.selectedAnchorIndex,
                         state->editor.selectedObject3DId,
                         state->editor.selectedObject3DResizeHandle,
                         state->editor.selectedObject3DPrismHandle,
                         object_topology,
                         object_topology_edit_mode,
                         object_face_sketch.visible ? &object_face_sketch : NULL,
                         state->editor.selectedSceneBoundsHandle,
                         state->editor.sceneBoundsHandlesVisible,
                         gizmoEnabled);
    state->hitboxDirty = false;
}

void Global_OnLayoutSaved(const char* path) {
    GlobalState* state = Global_Get();
    if (!state) return;
    if (path && *path) {
        strncpy(state->currentConfigPath, path, sizeof(state->currentConfigPath) - 1);
        state->currentConfigPath[sizeof(state->currentConfigPath) - 1] = '\0';
        strncpy(state->lastLayoutPath, path, sizeof(state->lastLayoutPath) - 1);
        state->lastLayoutPath[sizeof(state->lastLayoutPath) - 1] = '\0';
        Global_RecordRecentLayout(state, state->currentConfigPath, true);
    }
    state->currentObjectAssetPath[0] = '\0';
    state->layoutDirtySinceSave = false;
    state->layoutDirty = false;
    state->hitboxDirty = true;
    Global_UpdateSavedSnapshot();
}

void Global_OnLayoutLoaded(const char* path) {
    GlobalState* state = Global_Get();
    if (!state) return;
    if (path && *path) {
        strncpy(state->currentConfigPath, path, sizeof(state->currentConfigPath) - 1);
        state->currentConfigPath[sizeof(state->currentConfigPath) - 1] = '\0';
        strncpy(state->lastLayoutPath, path, sizeof(state->lastLayoutPath) - 1);
        state->lastLayoutPath[sizeof(state->lastLayoutPath) - 1] = '\0';
        Global_RecordRecentLayout(state, state->currentConfigPath, true);
    }
    state->currentSceneAuthoringPath[0] = '\0';
    state->currentObjectAssetPath[0] = '\0';
    state->layoutDirtySinceSave = false;
    state->layoutDirty = false;
    state->hitboxDirty = true;
    Global_UpdateSavedSnapshot();
}

void Global_OnSceneLoaded(const char* scene_authoring_path, const char* layout_path_hint) {
    GlobalState* state = Global_Get();
    if (!state) return;
    if (scene_authoring_path && *scene_authoring_path) {
        strncpy(state->currentSceneAuthoringPath,
                scene_authoring_path,
                sizeof(state->currentSceneAuthoringPath) - 1);
        state->currentSceneAuthoringPath[sizeof(state->currentSceneAuthoringPath) - 1] = '\0';
        strncpy(state->lastSceneAuthoringPath,
                scene_authoring_path,
                sizeof(state->lastSceneAuthoringPath) - 1);
        state->lastSceneAuthoringPath[sizeof(state->lastSceneAuthoringPath) - 1] = '\0';
        Global_RecordRecentScene(state, state->currentSceneAuthoringPath, true);
    } else {
        state->currentSceneAuthoringPath[0] = '\0';
    }
    if (layout_path_hint && *layout_path_hint) {
        strncpy(state->currentConfigPath, layout_path_hint, sizeof(state->currentConfigPath) - 1);
        state->currentConfigPath[sizeof(state->currentConfigPath) - 1] = '\0';
    } else if (scene_authoring_path && *scene_authoring_path) {
        strncpy(state->currentConfigPath, scene_authoring_path, sizeof(state->currentConfigPath) - 1);
        state->currentConfigPath[sizeof(state->currentConfigPath) - 1] = '\0';
    }
    state->currentObjectAssetPath[0] = '\0';
    state->layoutDirtySinceSave = false;
    state->layoutDirty = false;
    state->hitboxDirty = true;
    Global_UpdateSavedSnapshot();
}

void Global_OnObjectAssetSaved(const char* path) {
    GlobalState* state = Global_Get();
    if (!state) return;
    if (path && *path) {
        strncpy(state->currentObjectAssetPath, path, sizeof(state->currentObjectAssetPath) - 1);
        state->currentObjectAssetPath[sizeof(state->currentObjectAssetPath) - 1] = '\0';
        strncpy(state->lastObjectAssetPath, path, sizeof(state->lastObjectAssetPath) - 1);
        state->lastObjectAssetPath[sizeof(state->lastObjectAssetPath) - 1] = '\0';
        strncpy(state->currentConfigPath, path, sizeof(state->currentConfigPath) - 1);
        state->currentConfigPath[sizeof(state->currentConfigPath) - 1] = '\0';
        Global_RecordRecentObjectAsset(state, state->currentObjectAssetPath, true);
    }
    state->currentSceneAuthoringPath[0] = '\0';
    state->layoutDirtySinceSave = false;
    state->layoutDirty = false;
    state->hitboxDirty = true;
    Global_UpdateSavedSnapshot();
}

void Global_OnObjectAssetLoaded(const char* path) {
    GlobalState* state = Global_Get();
    if (!state) return;
    if (path && *path) {
        strncpy(state->currentObjectAssetPath, path, sizeof(state->currentObjectAssetPath) - 1);
        state->currentObjectAssetPath[sizeof(state->currentObjectAssetPath) - 1] = '\0';
        strncpy(state->lastObjectAssetPath, path, sizeof(state->lastObjectAssetPath) - 1);
        state->lastObjectAssetPath[sizeof(state->lastObjectAssetPath) - 1] = '\0';
        strncpy(state->currentConfigPath, path, sizeof(state->currentConfigPath) - 1);
        state->currentConfigPath[sizeof(state->currentConfigPath) - 1] = '\0';
        Global_RecordRecentObjectAsset(state, state->currentObjectAssetPath, true);
    }
    state->currentSceneAuthoringPath[0] = '\0';
    state->layoutDirtySinceSave = false;
    state->layoutDirty = false;
    state->hitboxDirty = true;
    Global_UpdateSavedSnapshot();
}

const char* Global_GetCurrentConfigPath(void) {
    GlobalState* state = Global_Get();
    if (!state) return NULL;
    return state->currentConfigPath;
}

const char* Global_GetCurrentSceneAuthoringPath(void) {
    GlobalState* state = Global_Get();
    if (!state) return NULL;
    return state->currentSceneAuthoringPath;
}

const char* Global_GetCurrentObjectAssetPath(void) {
    GlobalState* state = Global_Get();
    if (!state) return NULL;
    return state->currentObjectAssetPath;
}

const char* Global_GetLastLayoutPath(void) {
    GlobalState* state = Global_Get();
    if (!state) return NULL;
    return state->lastLayoutPath;
}

const char* Global_GetLastSceneAuthoringPath(void) {
    GlobalState* state = Global_Get();
    if (!state) return NULL;
    return state->lastSceneAuthoringPath;
}

const char* Global_GetLastObjectAssetPath(void) {
    GlobalState* state = Global_Get();
    if (!state) return NULL;
    return state->lastObjectAssetPath;
}

const LineDrawingRecentContexts* Global_GetRecentContexts(void) {
    GlobalState* state = Global_Get();
    if (!state) return NULL;
    return &state->recentContexts;
}

bool Global_IsLayoutDirty(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    return state->layoutDirtySinceSave;
}

const char* Global_GetInputRoot(void) {
    GlobalState* state = Global_Get();
    if (!state) return NULL;
    return state->dataPaths.input_root;
}

const char* Global_GetOutputRoot(void) {
    GlobalState* state = Global_Get();
    if (!state) return NULL;
    return state->dataPaths.output_root;
}

const char* Global_GetLayoutRoot(void) {
    GlobalState* state = Global_Get();
    if (!state) return NULL;
    return state->dataPaths.layout_root;
}

const char* Global_GetObjectAssetRoot(void) {
    GlobalState* state = Global_Get();
    if (!state) return NULL;
    return state->dataPaths.object_asset_root;
}

bool Global_LoadDataRoots(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    return LineDrawingDataPaths_Load(&state->dataPaths);
}

bool Global_SaveDataRoots(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    return LineDrawingDataPaths_Save(&state->dataPaths);
}

bool Global_LoadRecentContexts(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    return LineDrawingRecentContexts_Load(&state->recentContexts);
}

bool Global_SaveRecentContexts(void) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    return LineDrawingRecentContexts_Save(&state->recentContexts);
}

bool Global_SetInputRoot(const char* path, bool persist) {
    GlobalState* state = Global_Get();
    char prior_input_root[LINE_DRAWING_PATH_CAP];
    bool ok = true;
    if (!state) return false;
    snprintf(prior_input_root,
             sizeof(prior_input_root),
             "%s",
             state->dataPaths.input_root);
    if (!Global_SetPathField(state->dataPaths.input_root, sizeof(state->dataPaths.input_root), path)) return false;
    Global_UpdateDefaultLayoutPathForInputRootChange(state, prior_input_root);
    Global_RecordRecentInputRoot(state, state->dataPaths.input_root, false);
    if (persist) {
        ok &= Global_SaveDataRoots();
        ok &= Global_SaveRecentContexts();
    }
    return ok;
}

bool Global_SetOutputRoot(const char* path, bool persist) {
    GlobalState* state = Global_Get();
    bool ok = true;
    if (!state) return false;
    if (!Global_SetPathField(state->dataPaths.output_root, sizeof(state->dataPaths.output_root), path)) return false;
    Global_RecordRecentOutputRoot(state, state->dataPaths.output_root, false);
    if (persist) {
        ok &= Global_SaveDataRoots();
        ok &= Global_SaveRecentContexts();
    }
    return ok;
}

bool Global_SetLayoutRoot(const char* path, bool persist) {
    GlobalState* state = Global_Get();
    if (!state) return false;
    if (!Global_SetPathField(state->dataPaths.layout_root, sizeof(state->dataPaths.layout_root), path)) return false;
    Global_SetDefaultLayoutPath(state);
    if (persist) return Global_SaveDataRoots();
    return true;
}

bool Global_SetObjectAssetRoot(const char* path, bool persist) {
    GlobalState* state = Global_Get();
    char prior_root[LINE_DRAWING_PATH_CAP];
    if (!state) return false;
    snprintf(prior_root, sizeof(prior_root), "%s", state->dataPaths.object_asset_root);
    if (!Global_SetPathField(state->dataPaths.object_asset_root,
                             sizeof(state->dataPaths.object_asset_root),
                             path)) {
        return false;
    }
    Global_UpdateDefaultObjectAssetPathForRootChange(state, prior_root);
    if (persist) return Global_SaveDataRoots();
    return true;
}
