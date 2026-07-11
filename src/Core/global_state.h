// src/Core/global_state.h
#pragma once

#include "Core/data_paths.h"
#include "Core/recent_contexts.h"
#include "Core/line_drawing_pane_host.h"
#include "Layout/Grid/grid.h"
#include "Layout/layout.h"
#include "Editor/editor.h"
#include "ObjectAuthoring/object_authoring_session.h"
#include "UI/workspace_authoring/line_drawing_workspace_authoring_types.h"


#define ANCHOR_RENDER_RADIUS 5

typedef enum {
    SPACE_MODE_2D = 0,
    SPACE_MODE_3D = 1
} SpaceMode;

typedef enum {
    LINE_DRAWING_WORKSPACE_MODE_SCENE = 0,
    LINE_DRAWING_WORKSPACE_MODE_OBJECT = 1
} LineDrawingWorkspaceMode;

typedef enum {
    LINE_DRAWING_PREVIEW_MODE_WIREFRAME = 0,
    LINE_DRAWING_PREVIEW_MODE_FLAT = 1,
    LINE_DRAWING_PREVIEW_MODE_MATERIAL = 2
} LineDrawingPreviewMode;

typedef struct {
    char* layoutSnapshot;
    char* savedSnapshot;
    bool layoutDirtySinceSave;
    bool hasViewportState;
    uint32_t workspaceSourceSceneObjectId;
    uint32_t selectedObjectId;
    uint32_t selectedAssetBodyId;
    Object3DFaceKind selectedAssetFace;
    Grid grid;
    ViewPlane activePlane;
    FreeViewCamera freeViewCamera;
    char currentConfigPath[LINE_DRAWING_PATH_CAP];
    char currentSceneAuthoringPath[LINE_DRAWING_PATH_CAP];
    char currentObjectAssetPath[LINE_DRAWING_PATH_CAP];
    ObjectAuthoringSession objectAuthoring;
    bool hasObjectAuthoringState;
} LineDrawingWorkspaceDocumentState;

typedef struct GlobalState {
    Grid grid;
    Layout layout;
    EditorState editor;
    SpaceMode spaceMode;
    LineDrawingWorkspaceMode workspaceMode;
    LineDrawingPreviewMode previewMode;
    ViewPlane activePlane;
    FreeViewCamera freeViewCamera;
    bool centerCrosshairEnabled;

    int screenWidth;
    int screenHeight;
    LineDrawingPaneHost paneHost;
    LineDrawingWorkspaceAuthoringHostState workspaceAuthoring;

    bool layoutDirty;
    bool hitboxDirty;

    char currentConfigPath[LINE_DRAWING_PATH_CAP];
    char currentSceneAuthoringPath[LINE_DRAWING_PATH_CAP];
    char currentObjectAssetPath[LINE_DRAWING_PATH_CAP];
    char objectRuntimeMeshStatus[160];
    char lastObjectRuntimeMeshPath[LINE_DRAWING_PATH_CAP];
    char lastLayoutPath[LINE_DRAWING_PATH_CAP];
    char lastSceneAuthoringPath[LINE_DRAWING_PATH_CAP];
    char lastObjectAssetPath[LINE_DRAWING_PATH_CAP];
    LineDrawingDataPaths dataPaths;
    LineDrawingRecentContexts recentContexts;
    bool layoutDirtySinceSave;
    char* lastSavedSnapshot;
    ObjectAuthoringSession objectAuthoring;
    LineDrawingWorkspaceDocumentState sceneWorkspaceDocument;
    LineDrawingWorkspaceDocumentState objectWorkspaceDocument;
} GlobalState;

extern GlobalState* Global_Get(void);
LineDrawingPaneHost* Global_GetPaneHost(void);
const LineDrawingPaneHost* Global_GetPaneHostConst(void);
void Global_Init(int screenWidth, int screenHeight);
void Global_Shutdown(void);


void Global_TickSystems(AppContext* ctx);
void Global_SetWindowSize(int w, int h);

void Global_FlagLayoutChanged(void);
void Global_FlagGridChanged(void);
void Global_FlagHitboxesDirty(void);
void Global_RebuildHitboxesIfDirty(void);

int Global_GetScreenWidth(void);
int Global_GetScreenHeight(void);

void Global_OnLayoutSaved(const char* path);
void Global_OnLayoutLoaded(const char* path);
void Global_OnSceneLoaded(const char* scene_authoring_path, const char* layout_path_hint);
void Global_OnObjectAssetSaved(const char* path);
void Global_OnObjectAssetLoaded(const char* path);
const char* Global_GetCurrentConfigPath(void);
const char* Global_GetCurrentSceneAuthoringPath(void);
const char* Global_GetCurrentObjectAssetPath(void);
const char* Global_GetObjectRuntimeMeshStatus(void);
const char* Global_GetLastObjectRuntimeMeshPath(void);
bool Global_SetObjectRuntimeMeshStatus(const char* status);
bool Global_RecordObjectRuntimeMeshPath(const char* path);
bool Global_RecordObjectRuntimeMeshResult(const char* path, const char* status);
const char* Global_GetLastLayoutPath(void);
const char* Global_GetLastSceneAuthoringPath(void);
const char* Global_GetLastObjectAssetPath(void);
const LineDrawingRecentContexts* Global_GetRecentContexts(void);
bool Global_IsLayoutDirty(void);
const char* Global_GetInputRoot(void);
const char* Global_GetOutputRoot(void);
const char* Global_GetLayoutRoot(void);
const char* Global_GetObjectAssetRoot(void);
bool Global_SetInputRoot(const char* path, bool persist);
bool Global_SetOutputRoot(const char* path, bool persist);
bool Global_SetLayoutRoot(const char* path, bool persist);
bool Global_SetObjectAssetRoot(const char* path, bool persist);
bool Global_LoadDataRoots(void);
bool Global_SaveDataRoots(void);
bool Global_LoadRecentContexts(void);
bool Global_SaveRecentContexts(void);
SpaceMode Global_GetSpaceMode(void);
const char* Global_GetSpaceModeLabel(SpaceMode mode);
bool Global_SetSpaceMode(SpaceMode mode, bool persist);
bool Global_ToggleSpaceMode(bool persist);
bool Global_LoadSpaceMode(void);
bool Global_SaveSpaceMode(void);
LineDrawingWorkspaceMode Global_GetWorkspaceMode(void);
const char* Global_GetWorkspaceModeLabel(LineDrawingWorkspaceMode mode);
bool Global_SetWorkspaceMode(LineDrawingWorkspaceMode mode);
bool Global_ToggleWorkspaceMode(void);
LineDrawingPreviewMode Global_GetPreviewMode(void);
const char* Global_GetPreviewModeLabel(LineDrawingPreviewMode mode);
const char* Global_GetPreviewModeExportValue(LineDrawingPreviewMode mode);
bool Global_SetPreviewMode(LineDrawingPreviewMode mode);
bool Global_TogglePreviewMode(void);
bool Global_IsCenterCrosshairEnabled(void);
bool Global_SetCenterCrosshairEnabled(bool enabled);
bool Global_ToggleCenterCrosshair(void);
