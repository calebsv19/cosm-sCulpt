#pragma once
#include "Layout/layout_engineering.h"

const LayoutSceneCollection* Layout_FindSceneCollection(const LayoutObjectStore* store,const char* id);
bool Layout_SceneCollectionContains(const LayoutObjectStore* store,const char* group,const char* entity);
bool Layout_SceneCollectionAssigned(const LayoutObjectStore* store,const char* entity);
bool Layout_ValidateSceneCollections(const Layout* layout);
/* Membership edits use native single-undo transactions; they never reparent geometry. */
bool Layout_EditSceneCollection(Layout* layout,const LayoutSceneCollection* group,const char* remove,
    LayoutGeometryBeforePublish history,void* context);
bool Layout_AssignSceneCollection(Layout* layout,const char* group,const char* entity,bool present,
    LayoutGeometryBeforePublish history,void* context);
bool Layout_SceneCollectionsWriteJson(const Layout* layout,cJSON* engineering);
bool Layout_SceneCollectionsReadJson(Layout* layout,const cJSON* engineering);
/* Applying inspection state never authors visibility or history. Saving a view does. */
bool Layout_SaveWorkingView(Layout* layout,const LayoutWorkingView* view,const char* remove,
    LayoutGeometryBeforePublish history,void* context);
bool Layout_CaptureWorkingView(const LayoutObjectStore* store,LayoutWorkingView* view,bool guides);
bool Layout_ApplyWorkingView(LayoutObjectStore* store,const char* id,bool* guides);

/* Invalidate after document/parent/member publication, not on camera movement. */
void Layout_SceneCollectionInvalidate(void);
