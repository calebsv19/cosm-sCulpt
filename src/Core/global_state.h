// src/Core/global_state.h
#pragma once

#include "Layout/Grid/grid.h"
#include "Layout/layout.h"
#include "Editor/editor.h"


#define ANCHOR_RENDER_RADIUS 5

typedef struct {
    Grid grid;
    Layout layout;
    EditorState editor;

    int screenWidth;
    int screenHeight;

    bool layoutDirty;
    bool hitboxDirty;

    char currentConfigPath[256];
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
