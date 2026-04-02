// src/Editor/render_editor.h
#pragma once
#include "Core/SDLApp/sdl_app_framework.h"
#include "Editor/editor.h"

// High-level editor rendering (calls all subcomponents)
void Render_EditorOverlay(EditorState* editor, AppContext* ctx);

// Modular sub-renders
void Render_Editor_Anchor(EditorState* editor, AppContext* ctx);
void Render_Editor_GhostWall(EditorState* editor, AppContext* ctx);
void Render_Editor_SelectionBox(EditorState* editor, AppContext* ctx);
void Render_Editor_AxisGizmo(EditorState* editor, AppContext* ctx);
// Future:
void Render_Editor_SelectedWall(EditorState* editor, AppContext* ctx);
