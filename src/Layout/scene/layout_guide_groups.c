#include "Layout/layout_guide_groups.h"
#include "Layout/layout_motion.h"
#include <string.h>

/* Explicit presentation intent also covers legacy geometry without a volume role.
 * Never infer nonphysical space from missing material or a generic "envelope" label:
 * hardware and measured device proxies legitimately have both. */
static int explicit_group(const LayoutEntityInfo* info) {
    static const char* groups[]={"motion","walkways","keepouts","service","corridors","guides","annotations"};
    for (size_t i=0;i<info->property_count;++i) {
        const LayoutProperty* p=&info->properties[i];
        if (p->kind!=LAYOUT_PROPERTY_TEXT || strcmp(p->key,"view_group")) continue;
        for (int c=0;c<7;++c) if (!strcmp(p->text,groups[c])) return c+1;
    }
    return 0;
}
static int legacy_group(const LayoutEntityInfo* info) {
    /* These native motion carriers are editor geometry, not fabricated parts. */
    const char* carrier=" attachment frame (native motion carrier)";
    size_t n=strlen(info->label),suffix=strlen(carrier);
    if (n>=suffix && !strcmp(info->label+n-suffix,carrier)) return 7;
    if (info->entity_type[0]) return 0;
    /* Narrow compatibility rules for earlier authored, untyped drawing helpers. */
    if (!strncmp(info->label,"Fan ",4) &&
        (strstr(info->label," open envelope ") || strstr(info->label," rear-clear and opening envelope edge"))) return 1;
    if (!strncmp(info->label,"Solar cable run reservation ",28) ||
        !strncmp(info->label,"Cable descent reservation ",26) ||
        !strcmp(info->label,"Junction cable crosslink reservation")) return 5;
    if (!strcmp(info->label,"Rail top T-slot schematic") ||
        (!strncmp(info->label,"Panel ",6) && strstr(info->label," cell division schematic"))) return 7;
    return 0;
}
int Layout_GuideCategory(const LayoutObjectStore* store, const Object3D* object) {
    if (!object) return 0;
    const LayoutEntityInfo* info=&object->info;
    const char* type=Layout_EntityType(info);
    if (!strcmp(type,"MotionEnvelope") || (store && Layout_FindMotionEnvelope(store,object->objectId))) return 1;
    int group=explicit_group(info);
    if (group) return group;
    if (!Layout_EntityIsSpatialGuide(info)) return legacy_group(info);
    if (!strcmp(type,"Walkway")) return 2;
    if (!strcmp(type,"ServiceVolume") || info->volume_role==LAYOUT_VOLUME_SERVICE) return 4;
    if (!strcmp(type,"RoutingCorridor")) return 5;
    if (!strcmp(type,"KeepoutVolume") || info->volume_role==LAYOUT_VOLUME_KEEPOUT) return 3;
    return 6;
}
const char* Layout_GuideLabel(int category) {
    static const char* labels[]={"","Motion zones","Walkways","Keep-clear / keepouts","Service / access space","Routing corridors","Other spatial guides","Drawing annotations / bounds"};
    return category>=1&&category<=7?labels[category]:"";
}

static int query_category(const LayoutEntityQuery* q) {
    if (q->designation || q->assembly_id[0]) return -1;
    static const char* groups[]={"motion","walkways","keepouts","service","corridors","guides","annotations"};
    if (!q->entity_type[0] && !strcmp(q->property_key,"guide_category") && !strcmp(q->property_value,"all")) return 0;
    if (!q->entity_type[0] && (!strcmp(q->property_key,"view_group") || !strcmp(q->property_key,"guide_category")))
        for (int i=0;i<7;++i) if (!strcmp(q->property_value,groups[i])) return i+1;
    if (!q->property_key[0]) {
        static const char* types[]={"MotionEnvelope","Walkway","KeepoutVolume","ServiceVolume","RoutingCorridor"};
        for (int i=0;i<5;++i) if (!strcmp(q->entity_type,types[i])) return i+1;
    }
    return -1;
}
bool Layout_IsGuideView(const LayoutSavedView* view) {
    if (!view || !view->query_count) return false;
    for (size_t i=0;i<view->query_count;++i) if (query_category(&view->queries[i])<0) return false;
    return true;
}
bool Layout_GuideViewMatches(const LayoutObjectStore* store,const Object3D* object,const LayoutSavedView* view) {
    int category=Layout_GuideCategory(store,object);
    if (!category || !Layout_IsGuideView(view)) return false;
    /* Legacy aggregate guide presets omitted annotations and untyped helpers. */
    if (!strcmp(view->name,"Guides") || !strcmp(view->name,"Guides / nonphysical space")) return true;
    for (size_t i=0;i<view->query_count;++i) {
        int wanted=query_category(&view->queries[i]);
        if (!wanted || wanted==category) return true;
    }
    return false;
}
bool Layout_GuideHiddenByPreset(const LayoutObjectStore* store,const Object3D* object) {
    if (!store || !object || !store->hidden_view_count || !Layout_GuideCategory(store,object)) return false;
    for (size_t i=0;i<store->saved_view_count;++i) {
        const LayoutSavedView* view=&store->saved_views[i];
        if (!Layout_GuideViewMatches(store,object,view)) continue;
        for (size_t j=0;j<store->hidden_view_count;++j)
            if (!strcmp(view->id,store->hidden_view_ids[j])) return true;
    }
    return false;
}
void Layout_ShowGuidePresets(LayoutObjectStore* store) {
    for (size_t i=0;i<store->hidden_view_count;) {
        bool guide=false;
        for (size_t j=0;j<store->saved_view_count;++j)
            if (!strcmp(store->hidden_view_ids[i],store->saved_views[j].id) && Layout_IsGuideView(&store->saved_views[j])) guide=true;
        if (guide) memmove(store->hidden_view_ids[i],store->hidden_view_ids[i+1],(--store->hidden_view_count-i)*64);
        else ++i;
    }
}
