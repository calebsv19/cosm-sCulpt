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
} GlobalState;

extern GlobalState* Global_Get(void);
void Global_Init(int screenWidth, int screenHeight);
void Global_Shutdown(void);


void Global_TickSystems(AppContext* ctx);
void Global_SetWindowSize(int w, int h);


int Global_GetScreenWidth(void);
int Global_GetScreenHeight(void);
