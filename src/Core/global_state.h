// src/Core/global_state.h
#pragma once

#include "Layout/grid.h"
#include "Layout/layout.h"
#include "Editor/editor.h"

typedef struct {
    Grid grid;
    Layout layout;
    EditorState editor;
} GlobalState;

extern GlobalState* Global_Get(void);
void Global_Init(void);
void Global_Shutdown(void);

