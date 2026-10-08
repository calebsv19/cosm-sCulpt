#include "Layout/layout_surface_material.h"
#include <stdio.h>
#include <string.h>

static const LayoutSurfaceMaterial presets[] = {
    {"wood", "Wood grain", "wood", 0xCFAD77, .58, .08},
    {"painted_metal", "Painted shell", "solid", 0xD9DDDF, .38, .16},
    {"brushed_metal", "Bare brushed metal", "brushed_metal", 0xAEB9C0, .30, .68},
    {"fabric", "Fabric", "solid", 0x7C858D, .92, .02},
    {"foam", "Foam", "solid", 0xA4BD96, .95, .01},
    {"rubber", "Rubber", "solid", 0x303438, .88, .03},
    {"plastic", "Plastic", "solid", 0xDBD7CC, .48, .08}
};
const LayoutSurfaceMaterial *Layout_SurfaceMaterialFind(const char *id) {
    if (id) for (size_t i=0;i<sizeof(presets)/sizeof(*presets);++i)
        if (!strcmp(id,presets[i].id)) return &presets[i];
    return NULL;
}
const char *Layout_SurfaceMaterialId(const LayoutEntityInfo *info) {
    return info ? info->surface_material : "";
}
bool Layout_SurfaceMaterialValid(const LayoutEntityInfo *info) {
    if (!info || !memchr(info->surface_material,0,sizeof(info->surface_material))) return false;
    return !info->surface_material[0] || Layout_SurfaceMaterialFind(info->surface_material);
}
const char *Layout_SurfaceMaterialNext(const LayoutEntityInfo *info) {
    const char *id=Layout_SurfaceMaterialId(info);
    for (size_t i=0;i<sizeof(presets)/sizeof(*presets);++i)
        if (!strcmp(id,presets[i].id)) return i+1<sizeof(presets)/sizeof(*presets) ? presets[i+1].id : "";
    return presets[0].id;
}
bool Layout_SetSurfaceMaterial(Layout *layout,const char *id,const char *preset,
    LayoutGeometryBeforePublish history,void *context) {
    if (!layout || !id || !preset || (preset[0] && !Layout_SurfaceMaterialFind(preset))) return false;
    const LayoutEntityInfo *old=Layout_EntityInfo(&layout->objectStore,id);
    bool physical=false;
    for (size_t i=0;i<layout->objectStore.count;++i) {
        const Object3D *o=&layout->objectStore.items[i];
        if (!o->isDeleted && !strcmp(o->coreMeta.object_id,id) && !Layout_EntityIsSpatialGuide(&o->info)) physical=true;
    }
    if (!old || !physical) return false;
    if (!strcmp(Layout_SurfaceMaterialId(old),preset)) return true;
    LayoutEntityInfo next=*old;
    snprintf(next.surface_material,sizeof(next.surface_material),"%s",preset);
    return Layout_SetEntityInfo(layout,id,&next,history,context);
}
