#include "Layout/layout_saved_views.h"
#include "Layout/layout_scene_collections.h"
#include "Layout/layout_coverings.h"
#include "Layout/layout_guide_groups.h"
#include <stdio.h>
#include <string.h>

bool Layout_EntityRevealed(const LayoutObjectStore* s,const Object3D* object) {
    if(!s || !object || !object->coreMeta.flags.visible || Layout_GuideCategory(s,object))return false;
    for(size_t i=0;i<s->revealed_entity_count;++i)
        if(Layout_InspectionContains(s,s->revealed_entity_ids[i],object->coreMeta.object_id))return true;
    return false;
}

bool Layout_RevealEntity(LayoutObjectStore* s,const char* id) {
    if(!s || !id || strlen(id)>=64 || (!Layout_EntityInfo(s,id) &&
        !Layout_FindSceneCollection(s,id) && !Layout_CoveringCollectionValid(s,id)))return false;
    bool present=false;for(size_t i=0;i<s->revealed_entity_count;++i)if(!strcmp(s->revealed_entity_ids[i],id))present=true;
    if(!present&&s->revealed_entity_count==LAYOUT_MAX_WORKING_MASKS)return false;
    for(size_t i=0;i<s->hidden_entity_count;) {
        if(strcmp(s->hidden_entity_ids[i],"@layer.guides")&&strcmp(s->hidden_entity_ids[i],"@layer.references")&&
           Layout_InspectionContains(s,id,s->hidden_entity_ids[i]))
            memmove(s->hidden_entity_ids+i,s->hidden_entity_ids+i+1,(--s->hidden_entity_count-i)*64);
        else ++i;
    }
    if(!present)snprintf(s->revealed_entity_ids[s->revealed_entity_count++],64,"%s",id);
    return true;
}

static const char* label(const LayoutObjectStore* s,const char* id) {
    const LayoutSceneCollection* c=Layout_FindSceneCollection(s,id);
    if(c)return c->name;
    const LayoutEntityInfo* info=Layout_EntityInfo(s,id);
    if(info && info->label[0])return info->label;
    if(!strcmp(id,"@layer.guides"))return "Guides layer";
    if(!strcmp(id,"@layer.references"))return "References layer";
    if(!strcmp(id,"@coverings"))return "Coverings layer";
    if(!strcmp(id,"@coverings.drawers"))return "Drawers layer";
    if(!strcmp(id,"@coverings.doors"))return "Doors layer";
    if(!strcmp(id,"@coverings.access"))return "Access panels layer";
    return id;
}

void Layout_VisibilityExplain(const LayoutObjectStore* s,const char* id,char* out,size_t capacity) {
    if(!out || !capacity)return;
    snprintf(out,capacity,"Show respects exclusions. Reveal overrides area/cover filters.");
    if(!s || !id)return;
    for(size_t n=0;n<s->count;++n) {
        const Object3D* object=&s->items[n];const char* member=object->coreMeta.object_id;
        if(object->isDeleted || !Layout_InspectionContains(s,id,member) || Layout_ObjectShown(s,object))continue;
        if(!object->coreMeta.flags.visible)continue; /* obsolete authored-hidden parts stay hidden */
        if(s->isolated_entity_id[0] && !Layout_InspectionContains(s,s->isolated_entity_id,member)) {
            snprintf(out,capacity,"Hidden by Only: %.70s. Restore exits isolation.",label(s,s->isolated_entity_id));return;
        }
        for(size_t i=0;i<s->hidden_entity_count;++i) {
            const char* gate=s->hidden_entity_ids[i];
            if((!strcmp(gate,"@layer.guides") && Layout_GuideCategory(s,object)) ||
               (!strcmp(gate,"@layer.references") && object->info.reference)) {
                snprintf(out,capacity,"Hidden by %s. Enable that layer to show it.",label(s,gate));return;
            }
            if(Layout_InspectionContains(s,gate,member)) {
                snprintf(out,capacity,"Hidden by %.80s. Reveal is a scoped exception.",label(s,gate));return;
            }
        }
        snprintf(out,capacity,"Hidden by saved preset/filter. Reveal exposes physical members.");return;
    }
}
