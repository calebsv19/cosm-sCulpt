#pragma once
#include "Layout/layout_engineering.h"

bool Layout_ValidateSavedViews(const Layout* layout);
bool Layout_EditSavedView(Layout* layout, const LayoutSavedView* view, const char* remove_id,
    LayoutGeometryBeforePublish history, void* context);
bool Layout_InstallDefaultViews(Layout* layout, LayoutGeometryBeforePublish history, void* context);
bool Layout_EntityShown(const LayoutObjectStore* store, const char* id);
bool Layout_ViewHidden(const LayoutObjectStore* store, const char* id);
void Layout_ToggleView(LayoutObjectStore* store, const char* id);
void Layout_IsolateView(LayoutObjectStore* store, const char* id);
void Layout_ShowAllViews(LayoutObjectStore* store);
void Layout_RestoreViewVisibility(LayoutObjectStore* store, const LayoutObjectStore* previous);
bool Layout_SavedViewsWriteJson(const Layout* layout, cJSON* engineering);
bool Layout_SavedViewsReadJson(Layout* layout, const cJSON* engineering, bool required);
