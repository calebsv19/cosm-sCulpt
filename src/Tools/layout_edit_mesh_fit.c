#include "Tools/layout_edit_mesh_fit.h"
#include <string.h>
#include <stdio.h>

typedef struct {uint32_t id;const char* path;} Replace;
static bool replace(Layout* l,void* opaque) {
    Replace* r=opaque;const Object3D* old=Layout_ObjectStore_FindConst(&l->objectStore,r->id);
    if(!old||old->isDeleted||old->coreMeta.flags.locked||old->kind!=OBJECT3D_KIND_MESH_ASSET_INSTANCE)return false;
    Transform3D transform=old->transform;size_t count=l->objectStore.count;uint32_t next=l->objectStore.nextObjectId;
    uint32_t temporary=0;char diagnostic[256];
    if(!Layout_CreateMeshAssetInstanceFromRuntimeAsset(l,r->path,&transform,&temporary,diagnostic,sizeof(diagnostic))) {
        snprintf(l->geometryMessage,sizeof(l->geometryMessage),"%.150s",diagnostic);return false;
    }
    const Object3D* fresh=Layout_ObjectStore_FindConst(&l->objectStore,temporary);
    if(!fresh||l->objectStore.count!=count+1)return false;
    MeshAssetInstance3D mesh=fresh->meshInstance;
    Object3D* target=Layout_ObjectStore_Find(&l->objectStore,r->id);if(!target)return false;
    target->meshInstance=mesh; /* Keep identity, pose, parent, visibility and material. */
    memset(&l->objectStore.items[count],0,sizeof(Object3D));l->objectStore.count=count;l->objectStore.nextObjectId=next;
    return true;
}
bool LayoutEditMeshFit_Apply(Layout* l,const cJSON* row) {
    const cJSON* id=cJSON_GetObjectItemCaseSensitive(row,"object_id"),*path=cJSON_GetObjectItemCaseSensitive(row,"runtime_mesh");
    if(!cJSON_IsString(id)||!cJSON_IsString(path)||path->valuestring[0]!='/')return false;
    const Object3D* found=NULL;for(size_t i=0;i<l->objectStore.count;++i)if(!l->objectStore.items[i].isDeleted&&!strcmp(l->objectStore.items[i].coreMeta.object_id,id->valuestring)){if(found)return false;found=&l->objectStore.items[i];}
    if(!found)return false;Replace request={found->objectId,path->valuestring};
    return Layout_RunGeometryEdit(l,found->objectId,replace,&request,NULL,NULL);
}
