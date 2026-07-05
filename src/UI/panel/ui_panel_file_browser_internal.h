#pragma once

#include "UI/ui_panel.h"

float UIPanel_LoadMenuContentHeight(const UIPanelState* ui);
int UIPanel_LoadMenuRowHeight(const UIPanelState* ui, int index);
SDL_Rect UIPanel_GetLoadMenuListClipRect(const UIPanelState* ui);
SDL_Rect UIPanel_GetLoadMenuScrollTrackRect(const UIPanelState* ui);
float UIPanel_LoadMenuMaxScrollOffset(const UIPanelState* ui);
bool UIPanel_LoadMenuHasScrollableContent(const UIPanelState* ui);
void UIPanel_LoadMenuClampScroll(UIPanelState* ui);
void UIPanel_LoadMenuScrollIndexIntoView(UIPanelState* ui, int index);
SDL_Rect UIPanel_GetLoadMenuScrollThumbRect(const UIPanelState* ui);
SDL_Rect UIPanel_GetLoadMenuSetDirectoryButtonRect(const UIPanelState* ui);
int UIPanel_LoadMenuIndexAtPoint(const UIPanelState* ui, int mouseX, int mouseY);

SDL_Rect UIPanel_GetLoadMenuRect(const UIPanelState* ui);
SDL_Rect UIPanel_GetLoadMenuPaneClipRect(const UIPanelState* ui);

bool UIPanel_LoadRememberedEntryPath(UILoadMenuMode mode,
                                     char* out_path,
                                     size_t out_path_size);
const char* UIPanel_GetActiveSessionPathForMode(const UIPanelState* ui);
int UIPanel_FindRememberedLoadMenuIndex(const UIPanelState* ui);
int UIPanel_FindLoadProgressIndex(const UIPanelState* ui);

const char* UIPanel_FileStatusDisplayBaseName(const char* path);
const char* UIPanel_FileStatusBrowseModeName(UILoadMenuMode mode);
const char* UIPanel_FileStatusSummaryModeName(UILoadMenuMode mode);
bool UIPanel_FileStatusWriteMessage(char* out_text,
                                    size_t out_text_size,
                                    const char* prefix,
                                    const char* format,
                                    ...);
