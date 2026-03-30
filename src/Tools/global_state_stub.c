// src/Tools/global_state_stub.c
//
// Minimal stub implementations of the Core/global_state API so the
// standalone shape_tool can reuse the Layout and Layout JSON modules
// without pulling in the entire application framework.

#include "Core/global_state.h"

static GlobalState g_stubState;

GlobalState* Global_Get(void) {
    return &g_stubState;
}

void Global_Init(int screenWidth, int screenHeight) {
    g_stubState.screenWidth = screenWidth;
    g_stubState.screenHeight = screenHeight;
    g_stubState.spaceMode = SPACE_MODE_3D;
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
