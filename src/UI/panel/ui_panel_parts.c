#include "UI/ui_panel_parts.h"
#include "UI/ui_panel_shell.h"
#include "UI/ui_panel_right_scroll.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_visual_style.h"
#include "UI/font_manager.h"
#include "Core/global_state.h"
#include "Editor/editor_numeric_edit.h"
#include "Layout/layout_engineering.h"
#include "Layout/layout_relationships.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* types[]={"PhysicalObject","Panel","StructuralMember","Device","Controller","Sensor","Cable","Pipe","Tank","Battery","KeepoutVolume","ServiceVolume"};
static const char* property_types[]={"Text","Number","Boolean","Length"};
#include "UI/ui_panel_parts_surface.h"
#include "UI/ui_panel_spatial.h"
static Layout* layout(void) {return &Global_Get()->layout;}
static const LayoutEntityInfo* selected(void) {return Layout_EntityInfo(&layout()->objectStore,UIPanel_Get()->parts.id);}
static void select_entity(const char* id) {
    UIPanelState* ui=UIPanel_Get();UIPanel_PartsStopInput();
    char selected_id[64];snprintf(selected_id,sizeof(selected_id),"%s",id);
    snprintf(ui->parts.id,sizeof(ui->parts.id),"%s",selected_id);
    const LayoutEntityInfo* info=selected();
    if (info) {ui->parts.draft=ui->parts.observed=*info;ui->parts.observed_valid=true;}
    else {memset(&ui->parts.draft,0,sizeof(ui->parts.draft));ui->parts.observed_valid=false;}
    ui->parts.creating=false;ui->parts.delete_pending=false;ui->parts.message[0]=0;
}
bool UIPanel_PartsSelectEntity(const char* id) {
    if(!Layout_EntityInfo(&layout()->objectStore,id))return false;
    UIPanel_SetActiveRightTab(UIPanel_Get(),UI_PANEL_RIGHT_TAB_PARTS);
    UIPanel_Get()->parts.mode=Layout_FindAssembly(&layout()->objectStore,id) ? 1 : 0;
    if(UIPanel_Get()->parts.mode==0)for(size_t i=0;i<layout()->objectStore.count;++i)
        if(!layout()->objectStore.items[i].isDeleted && !strcmp(layout()->objectStore.items[i].coreMeta.object_id,id))Global_Get()->editor.selectedObject3DId=layout()->objectStore.items[i].objectId;
    select_entity(id);return true;
}
static void label(const char* id,char* text,size_t capacity) {
    const LayoutEntityInfo* info=Layout_EntityInfo(&layout()->objectStore,id);
    if (info && info->label[0]) snprintf(text,capacity,"%s",info->label);
    else if (id && id[0]) snprintf(text,capacity,"%s",id);
    else snprintf(text,capacity,"Choose object");
}
static const char* link_label(LayoutRelationshipKind kind) {
    static const char* labels[]={"Attached to","Supported by","Contained by"};
    return Layout_RelationshipType(kind) ? labels[kind] : "Choose relationship";
}
static void select_link_source(void) {
    GlobalState* state=Global_Get();const char* id=UIPanel_Get()->parts.link.source;
    state->editor.selectedObject3DId=0;
    for (size_t i=0;i<state->layout.objectStore.count;++i) {
        const Object3D* object=&state->layout.objectStore.items[i];
        if (!object->isDeleted && !strcmp(object->coreMeta.object_id,id))state->editor.selectedObject3DId=object->objectId;
    }
}
static void new_link(const char* source) {
    UIPanelState* ui=UIPanel_Get();
    char id[64];snprintf(id,sizeof(id),"%s",source ? source : "");
    memset(&ui->parts.link,0,sizeof(ui->parts.link));
    snprintf(ui->parts.link.source,64,"%s",id);
    ui->parts.link_observed=false;ui->parts.link_remove_pending=false;
    select_link_source();
}
static void choose_link(const LayoutRelationship* relationship) {
    UIPanelState* ui=UIPanel_Get();
    ui->parts.link=ui->parts.observed_link=*relationship;
    ui->parts.link_observed=true;ui->parts.link_remove_pending=false;
    ui->parts.message[0]=0;
    select_link_source();
}
static void links_form(PartsPane* p) {
    UIPanelState* ui=UIPanel_Get();LayoutObjectStore* store=&layout()->objectStore;
    char text[256],name[96];
    label(ui->parts.link.source,name,sizeof(name));
    snprintf(text,sizeof(text),"Part: %s",ui->parts.link.source[0]?name:"Choose object or assembly");row(p,PARTS_LINK_SOURCE,text,true);
    row(p,PARTS_LINK_TYPE,link_label(ui->parts.link.kind),true);
    label(ui->parts.link.target,name,sizeof(name));
    snprintf(text,sizeof(text),"Target: %s",ui->parts.link.target[0]?name:"Choose object or assembly");row(p,PARTS_LINK_TARGET,text,true);
    row(p,PARTS_LINK_SAVE,ui->parts.link.id[0]?"Update link":"Create link",ui->parts.link.source[0] && ui->parts.link.target[0]);
    cell(p,PARTS_LINK_NEW,"New link",NULL,0,2,true,false);
    cell(p,PARTS_LINK_REMOVE,"Remove link...",NULL,1,2,ui->parts.link.id[0]!=0,false);p->y+=p->h;
    if (ui->parts.link_remove_pending) {
        row(p,PARTS_LINK_CONFIRM_REMOVE,"Confirm remove link",true);row(p,PARTS_LINK_CANCEL,"Cancel",true);
    }
    note(p,"Links describe the system; they do not move geometry.");
    if (ui->parts.link.id[0]) {snprintf(text,sizeof(text),"ID: %s",ui->parts.link.id);row(p,0,text,true);}
    size_t count=Layout_QueryRelationships(store,ui->parts.link.source,-1,0,NULL,0);
    snprintf(text,sizeof(text),"%zu links for this part",count);row(p,0,text,true);
    if (!ui->parts.link.source[0]) {note(p,"Choose a part to inspect its links.");return;}
    for (size_t i=0; i<store->relationship_count; ++i) {
        const LayoutRelationship* r=&store->relationships[i];
        bool outgoing=!strcmp(r->source,ui->parts.link.source);
        if (!outgoing && strcmp(r->target,ui->parts.link.source)) continue;
        label(outgoing?r->target:r->source,name,sizeof(name));
        if (outgoing) snprintf(text,sizeof(text),"%s: %.160s",link_label(r->kind),name);
        else snprintf(text,sizeof(text),"%s: %.150s",(const char*[]){"Attached from","Supports","Contains"}[r->kind],name);
        row(p,7000+(int)i,text,true);
    }
}
/* Choosers use stable IDs, including entities hidden by a view filter. */
static bool choose_link_value(int chooser, int index) {
    UIPanelState* ui=UIPanel_Get();LayoutObjectStore* store=&layout()->objectStore;
    if (chooser==7) {
        if (index<0 || index>LAYOUT_RELATIONSHIP_CONTAINED_BY) return false;
        ui->parts.link.kind=(LayoutRelationshipKind)index;
    } else if (chooser==5 || chooser==6) {
        const char* id=NULL;
        if (index>=0 && (size_t)index<store->count && !store->items[index].isDeleted) id=store->items[index].coreMeta.object_id;
        else if (index>=0 && (size_t)index>=store->count && (size_t)index-store->count<store->assembly_count) id=store->assemblies[(size_t)index-store->count].id;
        if (!id) return false;
        snprintf(chooser==5?ui->parts.link.source:ui->parts.link.target,64,"%s",id);
        ui->parts.link_remove_pending=false;
        if (chooser==5)select_link_source();
    } else return false;
    return true;
}
static bool save_link(void) {
    UIPanelState* ui=UIPanel_Get();
    bool creating=!ui->parts.link.id[0];
    bool ok=Layout_EditRelationship(layout(),&ui->parts.link,NULL,Layout_GeometryHistory,NULL);
    if (ok) {
        const LayoutRelationship* saved=creating ? &layout()->objectStore.relationships[layout()->objectStore.relationship_count-1] : Layout_FindRelationship(&layout()->objectStore,ui->parts.link.id);
        if (saved) choose_link(saved);
    }
    snprintf(ui->parts.message,sizeof(ui->parts.message),"%s",ok?"Link saved; geometry unchanged.":layout()->geometryMessage);
    return ok;
}
static PartsPane build(SDL_Renderer* renderer,int x,int y,int wanted) {
    UIPanelState* ui=UIPanel_Get();PartsPane p={.renderer=renderer,.font=FontManager_Get(FONT_DEFAULT),
        .body=ui->rightBodyRect,.x=x,.click_y=y,.wanted=wanted};
    (void)UIPanelVisual_ResolvePalette(&p.palette);
    p.h=(p.font ? TTF_FontHeight(p.font) : 18)+18;p.y=p.body.y+8-(int)UIPanel_RightScrollOffset(ui);
    if (ui->activeRightTab!=UI_PANEL_RIGHT_TAB_PARTS) return p;
    if (Global_GetWorkspaceMode()!=LINE_DRAWING_WORKSPACE_MODE_SCENE) {note(&p,"Parts are edited in the Scene workspace.");return p;}
    for (int i=0;i<6;++i) {
        cell(&p,(const int[]){PARTS_OBJECTS,PARTS_ASSEMBLIES,PARTS_FILTERS,PARTS_LINKS,PARTS_VOLUMES,PARTS_CHECKS}[i],(const char*[]){"Objects","Assemblies","Filters","Links","Volumes","Checks"}[i],NULL,i%2,2,true,ui->parts.mode==i);
        if (i%2) p.y+=p.h;
    }
    p.y+=5;char text[256];LayoutObjectStore* store=&layout()->objectStore;
    if (ui->parts.mode>=4) {UIPanel_SpatialBuild(&p);return p;}
    if (ui->parts.mode==3) links_form(&p);
    else if (ui->parts.mode==2) {
        note(&p,"Show matching objects. Filters do not delete geometry.");
        snprintf(text,sizeof(text),"Type: %s",ui->parts.filter.entity_type[0]?ui->parts.filter.entity_type:"All");row(&p,PARTS_TYPE,text,true);
        snprintf(text,sizeof(text),"Role: %s",(const char*[]){"All","Design","Reference"}[ui->parts.filter.designation]);row(&p,PARTS_ROLE,text,true);
        label(ui->parts.filter.assembly_id,text,sizeof(text));char parent[256];snprintf(parent,sizeof(parent),"Assembly: %.220s",ui->parts.filter.assembly_id[0]?text:"All");row(&p,PARTS_PARENT,parent,true);
        field(&p,PARTS_KEY,"Property",ui->parts.key);field(&p,PARTS_VALUE,"Equals",ui->parts.value);
        row(&p,PARTS_APPLY_FILTER,"Apply filter",true);row(&p,PARTS_CLEAR_FILTER,"Show all",true);
        size_t shown=0;for(size_t i=0;i<store->count;++i)if(Layout_ObjectShown(store,&store->items[i]))++shown;
        snprintf(text,sizeof(text),"Showing %zu / %zu objects",shown,Layout_ObjectStore_LiveCount(store));row(&p,0,text,true);
        note(&p,"All fields combine. Empty Equals matches any value. Length filters use meters.");
    } else {
        label(ui->parts.id,text,sizeof(text));row(&p,PARTS_SELECT,ui->parts.creating?"New assembly":text,true);
        if (ui->parts.mode==1 && !ui->parts.movement_open) row(&p,PARTS_NEW,"New assembly",true);
        bool has=selected()!=NULL || ui->parts.creating;
        if (has) {
            if (!ui->parts.movement_open || ui->parts.mode!=1) {
            field(&p,PARTS_NAME,"Name",ui->parts.draft.label);
            if (ui->parts.mode==0) {snprintf(text,sizeof(text),"Type: %s",Layout_EntityType(&ui->parts.draft));row(&p,PARTS_TYPE,text,true);}
            snprintf(text,sizeof(text),"Role: %s",ui->parts.draft.reference?"Reference":"Design");row(&p,PARTS_ROLE,text,true);
            char parent[96];label(ui->parts.draft.parent_id,parent,sizeof(parent));snprintf(text,sizeof(text),"Parent: %s",ui->parts.draft.parent_id[0]?parent:"Scene root");row(&p,PARTS_PARENT,text,true);
            if(ui->parts.creating)row(&p,PARTS_SAVE,"Create assembly",true);
            else {cell(&p,PARTS_SAVE,"Apply details",NULL,0,2,true,false);cell(&p,PARTS_CANCEL,"Reset details",NULL,1,2,true,false);p.y+=p.h;}
            if (ui->parts.id[0]) {snprintf(text,sizeof(text),"ID: %s",ui->parts.id);row(&p,0,text,true);}
            row(&p,PARTS_PROPERTIES,ui->parts.properties_open?"- Properties":"+ Properties",true);
            if (ui->parts.properties_open) {
                for (size_t i=0;i<ui->parts.draft.property_count;++i) {
                    const LayoutProperty* property=&ui->parts.draft.properties[i];
                    if (property->kind==LAYOUT_PROPERTY_TEXT) snprintf(text,sizeof(text),"%s = %s",property->key,property->text);
                    else if (property->kind==LAYOUT_PROPERTY_BOOL) snprintf(text,sizeof(text),"%s = %s",property->key,property->number?"true":"false");
                    else snprintf(text,sizeof(text),"%s = %.6g%s",property->key,property->number,property->kind==LAYOUT_PROPERTY_LENGTH?" m":"");
                    row(&p,4000+(int)i,text,true);
                }
                field(&p,PARTS_KEY,"Key",ui->parts.key);snprintf(text,sizeof(text),"Value type: %s",property_types[ui->parts.property_kind]);row(&p,PARTS_PROPERTY_KIND,text,true);
                field(&p,PARTS_VALUE,"Value",ui->parts.value);
                cell(&p,PARTS_SET_PROPERTY,"Set",NULL,0,2,true,false);cell(&p,PARTS_REMOVE_PROPERTY,"Remove",NULL,1,2,ui->parts.key[0]!=0,false);p.y+=p.h;
                note(&p,"Set stages a property. Apply details saves it.");
            }
            if (ui->parts.creating) note(&p,"Origin uses the selected object center, or world zero.");
            }
            if (ui->parts.mode==1 && !ui->parts.creating) {
                const LayoutAssembly* assembly=Layout_FindAssembly(store,ui->parts.id);
                if(assembly){double origin[3]={0};(void)UIPanel_ConvertWorldToDisplay(assembly->frame.origin.x,&origin[0]);
                    (void)UIPanel_ConvertWorldToDisplay(assembly->frame.origin.y,&origin[1]);(void)UIPanel_ConvertWorldToDisplay(assembly->frame.origin.z,&origin[2]);
                    snprintf(text,sizeof(text),"Origin (%s): %.4g, %.4g, %.4g",UIPanel_GetDisplayUnitSymbol(),origin[0],origin[1],origin[2]);note(&p,text);}
                size_t members=0;for(size_t i=0;i<store->count;++i)if(!store->items[i].isDeleted && Layout_IsDescendant(store,store->items[i].info.parent_id,ui->parts.id))++members;
                snprintf(text,sizeof(text),"%zu objects in this assembly tree",members);row(&p,0,text,true);
                const Object3D* member=Layout_ObjectStore_FindConst(store,Global_Get()->editor.selectedObject3DId);
                if (!ui->parts.movement_open) {
                    snprintf(text,sizeof(text),"Add selected: %.160s",member ? (member->info.label[0]?member->info.label:member->coreMeta.object_id) : "none");
                    row(&p,PARTS_ADD_SELECTED,text,member!=NULL);
                }
                row(&p,PARTS_MOVE,ui->parts.movement_open?"Back to details":"Move / rotate assembly",true);
                if (ui->parts.movement_open) {
                    for (int k=0;k<3;++k) field(&p,PARTS_DX+k,(const char*[]){"Move X","Move Y","Move Z"}[k],ui->parts.move[k]);
                    snprintf(text,sizeof(text),"Rotate about world %s",(const char*[]){"X","Y","Z"}[ui->parts.rotation_axis]);row(&p,PARTS_AXIS,text,true);
                    field(&p,PARTS_ANGLE,"Degrees",ui->parts.angle);
                    bool pending=memcmp(&ui->parts.draft,&ui->parts.observed,sizeof(ui->parts.draft))!=0;
                    row(&p,PARTS_APPLY_MOVE,"Move assembly",!pending);
                    if(pending)note(&p,"Apply or reset details before moving.");
                    snprintf(text,sizeof(text),"Lengths use %s or unit suffixes.",UIPanel_GetDisplayUnitSymbol());note(&p,text);
                    note(&p,"Relative move about the assembly origin; all descendants follow.");
                }
                if (!ui->parts.movement_open) {
                for (size_t i=0;i<store->assembly_count;++i) if (!strcmp(store->assemblies[i].info.parent_id,ui->parts.id)) {
                    snprintf(text,sizeof(text),"Assembly: %s",store->assemblies[i].info.label[0]?store->assemblies[i].info.label:store->assemblies[i].id);row(&p,5000+(int)i,text,true);
                }
                for (size_t i=0;i<store->count;++i) if (!store->items[i].isDeleted && !strcmp(store->items[i].info.parent_id,ui->parts.id)) {
                    label(store->items[i].coreMeta.object_id,text,sizeof(text));row(&p,6000+(int)i,text,true);
                }
                if (ui->parts.delete_pending) {note(&p,"Delete empty assembly? Geometry stays in place.");row(&p,PARTS_CONFIRM_DELETE,"Delete assembly",true);row(&p,PARTS_CANCEL,"Cancel",true);}
                else row(&p,PARTS_DELETE,"Delete empty assembly...",true);
                }
            }
        } else note(&p,ui->parts.mode==1?"Create an assembly, then choose it as an object's Parent. Assemblies can have parents too.":"Select an object here or in the viewport.");
    }
    if (ui->parts.chooser) {
        /* One open chooser replaces the form below the mode buttons. */
        p.y=p.body.y+8-(int)UIPanel_RightScrollOffset(ui)+3*p.h+5;
        if (renderer) {SDL_SetRenderDrawColor(renderer,p.palette.pane_fill.r,p.palette.pane_fill.g,p.palette.pane_fill.b,255);SDL_Rect cover={p.body.x,p.y,p.body.w,p.body.h};SDL_RenderFillRect(renderer,&cover);}
        p.hit=0;
        row(&p,PARTS_CANCEL,"Close choices",true);
        int chooser=ui->parts.chooser;
        if (chooser==1) {
            if (ui->parts.mode==1) for (size_t i=0;i<store->assembly_count;++i) {label(store->assemblies[i].id,text,sizeof(text));row(&p,1000+(int)i,text,true);}
            else for (size_t i=0;i<store->count;++i) if (!store->items[i].isDeleted) {label(store->items[i].coreMeta.object_id,text,sizeof(text));row(&p,1000+(int)i,text,true);}
        } else if (chooser==2) {
            row(&p,1000,ui->parts.mode==2?"All types":"Custom type...",true);
            if(ui->parts.mode==2)row(&p,1001,"Custom type...",true);
            for (size_t i=0;i<sizeof(types)/sizeof(types[0]);++i) row(&p,1001+(ui->parts.mode==2)+(int)i,types[i],true);
        } else if (chooser==3) {
            row(&p,1000,ui->parts.mode==2?"All assemblies":"Scene root",true);
            for (size_t i=0;i<store->assembly_count;++i) {label(store->assemblies[i].id,text,sizeof(text));row(&p,1001+(int)i,text,true);}
        } else if (chooser==4) for (int i=0;i<6;++i) row(&p,1000+i,property_types[i],true);
        else if (chooser==5 || chooser==6) {
            for (size_t i=0;i<store->count+store->assembly_count;++i) {
                const char* id;
                if (i<store->count) {if(store->items[i].isDeleted)continue;id=store->items[i].coreMeta.object_id;}
                else id=store->assemblies[i-store->count].id;
                label(id,text,sizeof(text));char choice[256];snprintf(choice,sizeof(choice),"[%s] %.160s",id,text);
                row(&p,1000+(int)i,choice,true);
            }
        } else if (chooser==7) for (int i=0;i<3;++i) row(&p,1000+i,link_label((LayoutRelationshipKind)i),true);
    }
    if (ui->parts.message[0] && !ui->parts.chooser) note(&p,ui->parts.message);
    return p;
}
void UIPanel_PartsStopInput(void) {
    UIPanelState* ui=UIPanel_Get();
    if (ui->parts.input) SDL_StopTextInput();
    ui->parts.input=0;ui->parts.chooser=0;
}
static char* input_buffer(size_t* capacity) {
    UIPanelState* ui=UIPanel_Get();
    char* spatial=UIPanel_SpatialInputBuffer(capacity);if(spatial)return spatial;
    switch(ui->parts.input) {
        case PARTS_NAME:*capacity=sizeof(ui->parts.draft.label);return ui->parts.draft.label;
        case PARTS_TYPE:*capacity=64;return ui->parts.mode==2 ? ui->parts.filter.entity_type : ui->parts.draft.entity_type;
        case PARTS_KEY:*capacity=sizeof(ui->parts.key);return ui->parts.key;
        case PARTS_VALUE:*capacity=sizeof(ui->parts.value);return ui->parts.value;
        case PARTS_ANGLE:*capacity=sizeof(ui->parts.angle);return ui->parts.angle;
        default:
            if(ui->parts.input>=PARTS_DX && ui->parts.input<=PARTS_DZ) {*capacity=64;return ui->parts.move[ui->parts.input-PARTS_DX];}
            return NULL;
    }
}
static void focus(int action) {
    UIPanel_PartsStopInput();UIPanel_Get()->parts.input=action;UIPanel_Get()->parts.replace_text=true;SDL_StartTextInput();
}
void UIPanel_LayoutParts(void) {
    UIPanelState* ui=UIPanel_Get();
    if (ui->activeRightTab!=UI_PANEL_RIGHT_TAB_PARTS) return;
    UIPanel_SpatialRefresh();
    if (ui->parts.mode==3) {
        const LayoutRelationship* saved=Layout_FindRelationship(&layout()->objectStore,ui->parts.link.id);
        if (ui->parts.link_observed && (!saved || memcmp(saved,&ui->parts.observed_link,sizeof(*saved)))) {
            if (saved) choose_link(saved);
            else new_link(ui->parts.link.source);
            snprintf(ui->parts.message,160,"Links refreshed from the document.");
        }
        if (ui->parts.link.source[0] && !Layout_EntityInfo(&layout()->objectStore,ui->parts.link.source))new_link(NULL);
        if (ui->parts.link.target[0] && !Layout_EntityInfo(&layout()->objectStore,ui->parts.link.target))ui->parts.link.target[0]=0;
    }
    if (ui->parts.mode==0) {
        const Object3D* o=Layout_ObjectStore_FindConst(&layout()->objectStore,Global_Get()->editor.selectedObject3DId);
        if (o && strcmp(o->coreMeta.object_id,ui->parts.id)) select_entity(o->coreMeta.object_id);
    }
    const LayoutEntityInfo* info=selected();
    if (ui->parts.observed_valid && (!info || memcmp(info,&ui->parts.observed,sizeof(*info)))) {
        char id[64];snprintf(id,sizeof(id),"%s",ui->parts.id);select_entity(info?id:"");
        snprintf(ui->parts.message,sizeof(ui->parts.message),"Details refreshed from the document.");
    }
    PartsPane p=build(NULL,-1,-1,0);
    UIPanel_RightScrollSetContentHeight(ui,(float)(p.y-ui->rightBodyRect.y)+UIPanel_RightScrollOffset(ui)+8);
}
void UIPanel_RenderParts(SDL_Renderer* renderer) {
    if (!renderer || UIPanel_Get()->activeRightTab!=UI_PANEL_RIGHT_TAB_PARTS) return;
    SDL_Rect old;bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&old);
    SDL_RenderSetClipRect(renderer,&UIPanel_Get()->rightBodyRect);(void)build(renderer,-1,-1,0);SDL_RenderSetClipRect(renderer,clipped?&old:NULL);
}
bool UIPanel_PartsControlRect(int action,SDL_Rect* rect) {
    PartsPane p=build(NULL,-1,-1,action);if(rect)*rect=p.found;return p.found.w>0;
}
static bool save_details(void) {
    UIPanelState* ui=UIPanel_Get();bool ok;
    if (ui->parts.mode==1) {
        const LayoutAssembly* saved=Layout_FindAssembly(&layout()->objectStore,ui->parts.id);
        LayoutAssembly a=saved ? *saved : (LayoutAssembly){.frame={.axisU={1,0,0},.axisV={0,1,0},.normal={0,0,1}}};
        a.info=ui->parts.draft;snprintf(a.info.entity_type,64,"Assembly");
        if (ui->parts.creating) {
            if (!a.info.label[0]) {snprintf(ui->parts.message,160,"Enter an assembly name.");return false;}
            const Object3D* o=Layout_ObjectStore_FindConst(&layout()->objectStore,Global_Get()->editor.selectedObject3DId);
            if (o) a.frame.origin=o->transform.position;
        }
        ok=Layout_EditAssembly(layout(),&a,NULL,Layout_GeometryHistory,NULL);
        if (ok && ui->parts.creating) {char id[64];snprintf(id,64,"%s",layout()->objectStore.assemblies[layout()->objectStore.assembly_count-1].id);select_entity(id);}
    } else ok=Layout_SetEntityInfo(layout(),ui->parts.id,&ui->parts.draft,Layout_GeometryHistory,NULL);
    if (ok) {const LayoutEntityInfo* info=selected();if(info)ui->parts.observed=ui->parts.draft=*info;ui->parts.observed_valid=info!=NULL;}
    snprintf(ui->parts.message,160,"%s",ok?"Details saved.":layout()->geometryMessage);
    return ok;
}
static bool property_edit(bool remove) {
    UIPanelState* ui=UIPanel_Get();LayoutEntityInfo* info=&ui->parts.draft;size_t index=info->property_count;
    for(size_t i=0;i<info->property_count;++i)if(!strcmp(info->properties[i].key,ui->parts.key))index=i;
    if(remove) {
        if(index==info->property_count) {snprintf(ui->parts.message,160,"Choose an existing property.");return false;}
        memmove(&info->properties[index],&info->properties[index+1],(info->property_count-index-1)*sizeof(LayoutProperty));
        memset(&info->properties[--info->property_count],0,sizeof(LayoutProperty));
    } else {
        if(!ui->parts.key[0] || index==LAYOUT_MAX_PROPERTIES) {snprintf(ui->parts.message,160,"Enter a key; each entity supports up to 8 properties.");return false;}
        LayoutProperty p={.kind=(LayoutPropertyKind)ui->parts.property_kind};snprintf(p.key,sizeof(p.key),"%s",ui->parts.key);
        if(p.kind==LAYOUT_PROPERTY_TEXT)snprintf(p.text,sizeof(p.text),"%s",ui->parts.value);
        else if(p.kind==LAYOUT_PROPERTY_BOOL) {
            if(strcmp(ui->parts.value,"true") && strcmp(ui->parts.value,"false")) {snprintf(ui->parts.message,160,"Boolean value must be true or false.");return false;}
            p.number=!strcmp(ui->parts.value,"true");
        } else if(p.kind==LAYOUT_PROPERTY_LENGTH) {
            if(!Editor_ParseLength(ui->parts.value,UIPanel_GetDisplayUnit(),&p.number)) {snprintf(ui->parts.message,160,"Enter a length such as 12.7 mm.");return false;}
        } else {
            char* end; p.number=strtod(ui->parts.value,&end);
            if(end==ui->parts.value || *end || !isfinite(p.number)) {snprintf(ui->parts.message,160,"Enter a finite number.");return false;}
        }
        info->properties[index]=p;if(index==info->property_count)++info->property_count;
    }
    snprintf(ui->parts.message,160,"Property staged. Apply details to save.");return true;
}
static bool move(void) {
    UIPanelState* ui=UIPanel_Get();double delta[3];
    for(int k=0;k<3;++k)if(!Editor_ParseLength(ui->parts.move[k][0]?ui->parts.move[k]:"0",UIPanel_GetDisplayUnit(),&delta[k])) {
        snprintf(ui->parts.message,160,"Enter physical movement lengths, e.g. 25 mm.");return false;
    }
    double angle=0;const char* text=ui->parts.angle[0]?ui->parts.angle:"0";char* end;
    angle=strtod(text,&end);while(*end==' ')++end;
    if(end==text || (*end && strcmp(end,"deg")) || !isfinite(angle) || fabs(angle)>180) {snprintf(ui->parts.message,160,"Rotation must be within -180 to 180 degrees.");return false;}
    Vec3 rotation={0};if(ui->parts.rotation_axis==0)rotation.x=(float)angle;else if(ui->parts.rotation_axis==1)rotation.y=(float)angle;else rotation.z=(float)angle;
    bool ok=Layout_MoveAssembly(layout(),ui->parts.id,delta,rotation,Layout_GeometryHistory,NULL);
    snprintf(ui->parts.message,160,"%s",ok?"Assembly moved.":layout()->geometryMessage);
    if(ok){for(int k=0;k<3;++k)snprintf(ui->parts.move[k],64,"0");snprintf(ui->parts.angle,64,"0");}
    return ok;
}
bool UIPanel_PartsClick(int x,int y) {
    UIPanelState* ui=UIPanel_Get();if(ui->activeRightTab!=UI_PANEL_RIGHT_TAB_PARTS || !contains(ui->rightBodyRect,x,y))return false;
    PartsPane p=build(NULL,x,y,0);int action=p.hit,chooser=ui->parts.chooser;
    if(!action)return true;
    if(is_field(action)) {focus(action);return true;}
    UIPanel_PartsStopInput();
    if (ui->parts.mode>=4 && action!=PARTS_VOLUMES && action!=PARTS_CHECKS && !(action>=PARTS_OBJECTS && action<=PARTS_LINKS)) {
        (void)UIPanel_SpatialClick(action,chooser);UIPanel_LayoutParts();return true;
    }
    if (chooser>=5 && chooser<=7 && action>=1000) {
        (void)choose_link_value(chooser,action-1000);UIPanel_LayoutParts();return true;
    }
    if (action==PARTS_LINK_SOURCE || action==PARTS_LINK_TARGET || action==PARTS_LINK_TYPE) {
        ui->parts.chooser=action==PARTS_LINK_SOURCE?5:action==PARTS_LINK_TARGET?6:7;
        ui->rightScroll[UI_PANEL_RIGHT_TAB_PARTS].scrollOffsetPx=0;
    } else if (action==PARTS_LINK_SAVE) (void)save_link();
    else if (action==PARTS_LINK_NEW)new_link(ui->parts.link.source);
    else if (action==PARTS_LINK_REMOVE)ui->parts.link_remove_pending=true;
    else if (action==PARTS_LINK_CANCEL)ui->parts.link_remove_pending=false;
    else if (action==PARTS_LINK_CONFIRM_REMOVE) {
        bool ok=Layout_EditRelationship(layout(),NULL,ui->parts.link.id,Layout_GeometryHistory,NULL);
        if (ok)new_link(ui->parts.link.source);
        snprintf(ui->parts.message,160,"%s",ok?"Link removed; geometry unchanged.":layout()->geometryMessage);
    } else if (ui->parts.mode==3 && !chooser && action>=7000) {
        size_t i=(size_t)(action-7000);
        if (i<layout()->objectStore.relationship_count)choose_link(&layout()->objectStore.relationships[i]);
    } else if((action>=PARTS_OBJECTS && action<=PARTS_LINKS) || action==PARTS_VOLUMES || action==PARTS_CHECKS) {
        if (action==PARTS_LINKS) {
            const Object3D* object=Layout_ObjectStore_FindConst(&layout()->objectStore,Global_Get()->editor.selectedObject3DId);
            new_link(Layout_EntityInfo(&layout()->objectStore,ui->parts.id)?ui->parts.id:object?object->coreMeta.object_id:NULL);
        }
        ui->parts.mode=action==PARTS_VOLUMES?4:action==PARTS_CHECKS?5:action-PARTS_OBJECTS;ui->parts.properties_open=false;ui->parts.movement_open=false;ui->parts.creating=false;ui->parts.id[0]=0;ui->parts.observed_valid=false;ui->parts.message[0]=0;ui->parts.delete_pending=false;ui->spatial.remove_pending=false;
        ui->rightScroll[UI_PANEL_RIGHT_TAB_PARTS].scrollOffsetPx=0;
        if(ui->parts.mode==4)UIPanel_SpatialEnterVolumes();
        if(ui->parts.mode==2){ui->parts.filter=layout()->objectStore.view_query;snprintf(ui->parts.key,48,"%s",ui->parts.filter.property_key);snprintf(ui->parts.value,128,"%s",ui->parts.filter.property_value);}
        else {ui->parts.key[0]=ui->parts.value[0]=0;}
    } else if(action==PARTS_SELECT)ui->parts.chooser=chooser==1?0:1;
    else if(action==PARTS_TYPE)ui->parts.chooser=chooser==2?0:2;
    else if(action==PARTS_PARENT)ui->parts.chooser=chooser==3?0:3;
    else if(action==PARTS_PROPERTY_KIND)ui->parts.chooser=chooser==4?0:4;
    else if(action==PARTS_ROLE) {if(ui->parts.mode==2)ui->parts.filter.designation=(ui->parts.filter.designation+1)%3;else ui->parts.draft.reference=!ui->parts.draft.reference;}
    else if(action==PARTS_PROPERTIES)ui->parts.properties_open=!ui->parts.properties_open;
    else if(action==PARTS_MOVE) {
        ui->parts.movement_open=!ui->parts.movement_open;
        ui->rightScroll[UI_PANEL_RIGHT_TAB_PARTS].scrollOffsetPx=0;
    }
    else if(action==PARTS_AXIS)ui->parts.rotation_axis=(ui->parts.rotation_axis+1)%3;
    else if(action==PARTS_NEW) {ui->parts.movement_open=false;select_entity("");ui->parts.creating=true;snprintf(ui->parts.draft.entity_type,64,"Assembly");}
    else if(action==PARTS_SAVE)(void)save_details();
    else if(action==PARTS_SET_PROPERTY || action==PARTS_REMOVE_PROPERTY)(void)property_edit(action==PARTS_REMOVE_PROPERTY);
    else if(action==PARTS_APPLY_MOVE)(void)move();
    else if(action==PARTS_DELETE)ui->parts.delete_pending=true;
    else if(action==PARTS_CONFIRM_DELETE) {
        bool ok=Layout_EditAssembly(layout(),NULL,ui->parts.id,Layout_GeometryHistory,NULL);
        if(ok)select_entity("");
        snprintf(ui->parts.message,160,"%s",ok?"Assembly deleted.":layout()->geometryMessage);ui->parts.delete_pending=false;
    } else if(action==PARTS_CANCEL) {
        ui->parts.delete_pending=false;
        if(!chooser && selected())ui->parts.draft=*selected();
    } else if(action==PARTS_ADD_SELECTED) {
        const Object3D* object=Layout_ObjectStore_FindConst(&layout()->objectStore,Global_Get()->editor.selectedObject3DId);
        if(object){LayoutEntityInfo info=object->info;snprintf(info.parent_id,64,"%s",ui->parts.id);
            bool ok=Layout_SetEntityInfo(layout(),object->coreMeta.object_id,&info,Layout_GeometryHistory,NULL);
            snprintf(ui->parts.message,160,"%s",ok?"Selected object added; its world pose is unchanged.":layout()->geometryMessage);}
    }
    else if(action==PARTS_APPLY_FILTER || action==PARTS_CLEAR_FILTER) {
        if(action==PARTS_CLEAR_FILTER){memset(&ui->parts.filter,0,sizeof(ui->parts.filter));ui->parts.key[0]=ui->parts.value[0]=0;}
        snprintf(ui->parts.filter.property_key,48,"%s",ui->parts.key);snprintf(ui->parts.filter.property_value,128,"%s",ui->parts.value);
        layout()->objectStore.view_query=ui->parts.filter;Global_Get()->layoutDirty=true;Global_FlagHitboxesDirty();
        snprintf(ui->parts.message,160,"%s",action==PARTS_CLEAR_FILTER?"Showing all authored visible objects.":"Filter applied.");
    } else if(!chooser && action>=6000) {
        size_t i=(size_t)(action-6000);if(i<layout()->objectStore.count){ui->parts.mode=0;Global_Get()->editor.selectedObject3DId=layout()->objectStore.items[i].objectId;select_entity(layout()->objectStore.items[i].coreMeta.object_id);}
    } else if(!chooser && action>=5000) {
        size_t i=(size_t)(action-5000);if(i<layout()->objectStore.assembly_count)select_entity(layout()->objectStore.assemblies[i].id);
    } else if(!chooser && action>=4000) {
        size_t i=(size_t)(action-4000);if(i<ui->parts.draft.property_count) {
            const LayoutProperty* property=&ui->parts.draft.properties[i];ui->parts.property_kind=property->kind;snprintf(ui->parts.key,48,"%s",property->key);
            if(property->kind==LAYOUT_PROPERTY_TEXT)snprintf(ui->parts.value,128,"%s",property->text);
            else if(property->kind==LAYOUT_PROPERTY_BOOL)snprintf(ui->parts.value,128,"%s",property->number?"true":"false");
            else snprintf(ui->parts.value,128,"%.12g%s",property->number,property->kind==LAYOUT_PROPERTY_LENGTH?" m":"");
        }
    } else if(action>=1000) {
        int i=action-1000;
        if(chooser==1) {
            if(ui->parts.mode==1 && (size_t)i<layout()->objectStore.assembly_count)select_entity(layout()->objectStore.assemblies[i].id);
            else if(ui->parts.mode==0 && (size_t)i<layout()->objectStore.count){Global_Get()->editor.selectedObject3DId=layout()->objectStore.items[i].objectId;select_entity(layout()->objectStore.items[i].coreMeta.object_id);}
        } else if(chooser==2) {
            if((i==0 && ui->parts.mode!=2) || (i==1 && ui->parts.mode==2))focus(PARTS_TYPE);
            else {char* type=ui->parts.mode==2?ui->parts.filter.entity_type:ui->parts.draft.entity_type;snprintf(type,64,"%s",i>0?types[i-1-(ui->parts.mode==2)]:"");}
        } else if(chooser==3) {
            char* parent=ui->parts.mode==2?ui->parts.filter.assembly_id:ui->parts.draft.parent_id;snprintf(parent,64,"%s",i>0?layout()->objectStore.assemblies[i-1].id:"");
        } else if(chooser==4)ui->parts.property_kind=i;
    }
    Global_FlagHitboxesDirty();UIPanel_LayoutParts();return true;
}
bool UIPanel_PartsEvent(const SDL_Event* event) {
    UIPanelState* ui=UIPanel_Get();if(ui->activeRightTab!=UI_PANEL_RIGHT_TAB_PARTS)return false;
    if(event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT && contains(ui->rightBodyRect,event->button.x,event->button.y))return UIPanel_PartsClick(event->button.x,event->button.y);
    if(ui->parts.chooser && event->type==SDL_KEYDOWN) {if(event->key.keysym.sym==SDLK_ESCAPE)UIPanel_PartsStopInput();return true;}
    if(event->type==SDL_MOUSEBUTTONDOWN && ui->parts.input)UIPanel_PartsStopInput();
    if(!ui->parts.input)return false;
    if(event->type==SDL_KEYDOWN) {
        SDL_Keycode key=event->key.keysym.sym;
        if(key==SDLK_RETURN || key==SDLK_KP_ENTER || key==SDLK_ESCAPE)UIPanel_PartsStopInput();
        else if(key==SDLK_a && (event->key.keysym.mod & (KMOD_CTRL|KMOD_GUI)))ui->parts.replace_text=true;
        else if(key==SDLK_BACKSPACE){size_t capacity;char* text=input_buffer(&capacity);if(text && text[0]){if(ui->parts.replace_text)text[0]=0;else{size_t n=strlen(text)-1;while(n && ((unsigned char)text[n]&0xc0)==0x80)--n;text[n]=0;}ui->parts.replace_text=false;}}
        return true;
    }
    if(event->type==SDL_TEXTINPUT) {
        size_t capacity;char* text=input_buffer(&capacity);if(text){if(ui->parts.replace_text)text[0]=0;size_t n=strlen(text),add=strlen(event->text.text);if(n+add<capacity)memcpy(text+n,event->text.text,add+1);else snprintf(ui->parts.message,160,"Text exceeds field capacity.");ui->parts.replace_text=false;}return true;
    }
    if(event->type==SDL_WINDOWEVENT && event->window.event==SDL_WINDOWEVENT_FOCUS_LOST)UIPanel_PartsStopInput();
    return false;
}
