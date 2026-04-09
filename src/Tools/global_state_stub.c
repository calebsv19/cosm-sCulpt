// src/Tools/global_state_stub.c
//
// Minimal stub implementations of the Core/global_state API so the
// standalone shape_tool can reuse the Layout and Layout JSON modules
// without pulling in the entire application framework.

#include "Core/global_state.h"
#include <stdio.h>

static GlobalState g_stubState;

GlobalState* Global_Get(void) {
    return &g_stubState;
}

void Global_Init(int screenWidth, int screenHeight) {
    g_stubState.screenWidth = screenWidth;
    g_stubState.screenHeight = screenHeight;
    g_stubState.spaceMode = SPACE_MODE_3D;
    LineDrawingDataPaths_SetDefaults(&g_stubState.dataPaths);
    (void)snprintf(g_stubState.currentConfigPath,
                   sizeof(g_stubState.currentConfigPath),
                   "%s/layout_config.json",
                   g_stubState.dataPaths.layout_root);
}

void Global_Shutdown(void) {
}

void Global_TickSystems(AppContext* ctx) {
    (void)ctx;
}

void Global_SetWindowSize(int w, int h) {
    g_stubState.screenWidth = w;
    g_stubState.screenHeight = h;
}

void Global_FlagLayoutChanged(void) {
    g_stubState.layoutDirty = true;
    g_stubState.layoutDirtySinceSave = true;
}

void Global_FlagGridChanged(void) {
}

void Global_FlagHitboxesDirty(void) {
}

void Global_RebuildHitboxesIfDirty(void) {
}

int Global_GetScreenWidth(void) {
    return g_stubState.screenWidth;
}

int Global_GetScreenHeight(void) {
    return g_stubState.screenHeight;
}

void Global_OnLayoutSaved(const char* path) {
    (void)path;
    g_stubState.layoutDirty = false;
    g_stubState.layoutDirtySinceSave = false;
}

void Global_OnLayoutLoaded(const char* path) {
    (void)path;
    g_stubState.layoutDirty = false;
    g_stubState.layoutDirtySinceSave = false;
}

const char* Global_GetCurrentConfigPath(void) {
    return g_stubState.currentConfigPath;
}

bool Global_IsLayoutDirty(void) {
    return g_stubState.layoutDirtySinceSave;
}

const char* Global_GetInputRoot(void) {
    return g_stubState.dataPaths.input_root;
}

const char* Global_GetOutputRoot(void) {
    return g_stubState.dataPaths.output_root;
}

const char* Global_GetLayoutRoot(void) {
    return g_stubState.dataPaths.layout_root;
}

bool Global_SetInputRoot(const char* path, bool persist) {
    (void)persist;
    if (!path || !path[0]) return false;
    return snprintf(g_stubState.dataPaths.input_root,
                    sizeof(g_stubState.dataPaths.input_root),
                    "%s",
                    path) < (int)sizeof(g_stubState.dataPaths.input_root);
}

bool Global_SetOutputRoot(const char* path, bool persist) {
    (void)persist;
    if (!path || !path[0]) return false;
    return snprintf(g_stubState.dataPaths.output_root,
                    sizeof(g_stubState.dataPaths.output_root),
                    "%s",
                    path) < (int)sizeof(g_stubState.dataPaths.output_root);
}

bool Global_SetLayoutRoot(const char* path, bool persist) {
    (void)persist;
    if (!path || !path[0]) return false;
    if (snprintf(g_stubState.dataPaths.layout_root,
                 sizeof(g_stubState.dataPaths.layout_root),
                 "%s",
                 path) >= (int)sizeof(g_stubState.dataPaths.layout_root)) {
        return false;
    }
    return snprintf(g_stubState.currentConfigPath,
                    sizeof(g_stubState.currentConfigPath),
                    "%s/layout_config.json",
                    g_stubState.dataPaths.layout_root) < (int)sizeof(g_stubState.currentConfigPath);
}

bool Global_LoadDataRoots(void) {
    return true;
}

bool Global_SaveDataRoots(void) {
    return true;
}

SpaceMode Global_GetSpaceMode(void) {
    return g_stubState.spaceMode;
}

const char* Global_GetSpaceModeLabel(SpaceMode mode) {
    return mode == SPACE_MODE_2D ? "2D" : "3D";
}

bool Global_SetSpaceMode(SpaceMode mode, bool persist) {
    (void)persist;
    if (mode != SPACE_MODE_2D && mode != SPACE_MODE_3D) return false;
    g_stubState.spaceMode = mode;
    if (mode == SPACE_MODE_2D) {
        g_stubState.activePlane.axis = VIEW_PLANE_XY;
        g_stubState.activePlane.offset = 0.0f;
        g_stubState.freeViewCamera.enabled = false;
    }
    return true;
}

bool Global_ToggleSpaceMode(bool persist) {
    (void)persist;
    SpaceMode next = (g_stubState.spaceMode == SPACE_MODE_2D) ? SPACE_MODE_3D : SPACE_MODE_2D;
    return Global_SetSpaceMode(next, false);
}

bool Global_LoadSpaceMode(void) {
    return true;
}

bool Global_SaveSpaceMode(void) {
    return true;
}
