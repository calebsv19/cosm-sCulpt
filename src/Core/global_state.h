// src/Core/global_state.h
#pragma once

#include "Core/data_paths.h"
#include "Layout/Grid/grid.h"
#include "Layout/layout.h"
#include "Editor/editor.h"


#define ANCHOR_RENDER_RADIUS 5

typedef enum {
    SPACE_MODE_2D = 0,
    SPACE_MODE_3D = 1
} SpaceMode;

typedef struct GlobalState {
    Grid grid;
    Layout layout;
    EditorState editor;
    SpaceMode spaceMode;
    ViewPlane activePlane;
    FreeViewCamera freeViewCamera;
    bool centerCrosshairEnabled;

    int screenWidth;
    int screenHeight;

    bool layoutDirty;
    bool hitboxDirty;

    char currentConfigPath[LINE_DRAWING_PATH_CAP];
    LineDrawingDataPaths dataPaths;
    bool layoutDirtySinceSave;
    char* lastSavedSnapshot;
} GlobalState;

extern GlobalState* Global_Get(void);
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
const char* Global_GetCurrentConfigPath(void);
bool Global_IsLayoutDirty(void);
const char* Global_GetInputRoot(void);
const char* Global_GetOutputRoot(void);
const char* Global_GetLayoutRoot(void);
bool Global_SetInputRoot(const char* path, bool persist);
bool Global_SetOutputRoot(const char* path, bool persist);
bool Global_SetLayoutRoot(const char* path, bool persist);
bool Global_LoadDataRoots(void);
bool Global_SaveDataRoots(void);
SpaceMode Global_GetSpaceMode(void);
const char* Global_GetSpaceModeLabel(SpaceMode mode);
bool Global_SetSpaceMode(SpaceMode mode, bool persist);
bool Global_ToggleSpaceMode(bool persist);
bool Global_LoadSpaceMode(void);
bool Global_SaveSpaceMode(void);
bool Global_IsCenterCrosshairEnabled(void);
bool Global_SetCenterCrosshairEnabled(bool enabled);
bool Global_ToggleCenterCrosshair(void);
