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

/* View-only inspection overrides; authored visibility, save bytes and history stay intact. */
bool Layout_SetEntityHidden(LayoutObjectStore* store,const char* id,bool hidden);
bool Layout_IsolateEntity(LayoutObjectStore* store,const char* id);
void Layout_RestoreEntityVisibility(LayoutObjectStore* store);
/* -1 hidden, 0 inherit, +1 shown; Only is a hard outer boundary. */
int Layout_EntityInspectionOverride(const LayoutObjectStore* store,const char* id);
/* Effective visible live objects in an entity subtree; 0 hidden, 1 shown, 2 mixed. */
int Layout_EntityVisibilityState(const LayoutObjectStore* store,const char* id);
int Layout_EntityInspectionOverrideResolved(const LayoutObjectStore* store,const char* id,
    const LayoutEntityInfo* info,const Object3D* object);
bool Layout_EntityShownResolved(const LayoutObjectStore* store,const char* id,
    const LayoutEntityInfo* info,const Object3D* object);
/* Show respects exclusions; Reveal is a scoped exception. Only and guide/reference
 * layers remain hard boundaries. No authored-hidden obsolete parts are revived. */
bool Layout_EntityRevealed(const LayoutObjectStore* store,const Object3D* object);
bool Layout_RevealEntity(LayoutObjectStore* store,const char* id);
void Layout_VisibilityExplain(const LayoutObjectStore* store,const char* id,char* out,size_t capacity);
bool Layout_InspectionContains(const LayoutObjectStore* store,const char* group,const char* id);
bool Layout_EntityInspectionShown(const LayoutObjectStore* store,const char* id);
/* Explicit document action; whole assembly subtree uses one native transaction/history item. */
bool Layout_SetAuthoredVisibility(Layout* layout,const char* id,bool visible,
    LayoutGeometryBeforePublish history,void* context);
