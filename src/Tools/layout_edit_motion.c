#include "Tools/layout_edit_motion.h"
#include "Layout/layout_constraints.h"
#include "Layout/layout_motion.h"
#include "Layout/scene/layout_folding_brace.h"
#include "Layout/scene/layout_telescoping_stay.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const cJSON *field(const cJSON *o, const char *key) {return cJSON_GetObjectItemCaseSensitive(o,key);}
static const char *text(const cJSON *o, const char *key) {
    const cJSON *v=field(o,key);return cJSON_IsString(v) ? v->valuestring : NULL;
}
static bool number(const cJSON *o, const char *key, double *out) {
    const cJSON *v=field(o,key);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble)) return false;
    *out=v->valuedouble;return true;
}
static bool copy(char *out, size_t capacity, const char *in) {
    if (!in || !in[0] || strlen(in)>=capacity) return false;
    snprintf(out,capacity,"%s",in);return true;
}
static bool triple(const cJSON *a, double out[3]) {
    if (!cJSON_IsArray(a) || cJSON_GetArraySize(a)!=3) return false;
    for (int i=0;i<3;++i) {
        const cJSON *n=cJSON_GetArrayItem(a,i);
        if (!cJSON_IsNumber(n) || !isfinite(n->valuedouble) || fabs(n->valuedouble)>1e6) return false;
        out[i]=n->valuedouble;
    }
    return true;
}
static const LayoutConstraint *rule(const Layout *l, const char *id) {
    if (!id) return NULL;
    for (size_t i=0;i<l->objectStore.constraintCount;++i)
        if (!strcmp(id,l->objectStore.constraints[i].id)) return &l->objectStore.constraints[i];
    return NULL;
}
static bool reference(const cJSON *o, LayoutGeometricReference *out) {
    const char *feature=text(o,"feature");
    if (!copy(out->entity_id,sizeof(out->entity_id),text(o,"entity_id")) || !feature) return false;
    if (!strcmp(feature,"axis_u")) out->kind=LAYOUT_REFERENCE_AXIS_U;
    else if (!strcmp(feature,"axis_v")) out->kind=LAYOUT_REFERENCE_AXIS_V;
    else if (!strcmp(feature,"axis_n")) out->kind=LAYOUT_REFERENCE_AXIS_N;
    else {
        const char *faces[]={"face_pos_u","face_neg_u","face_pos_v","face_neg_v","face_pos_n","face_neg_n"};
        const Object3DFaceKind kinds[]={OBJECT3D_FACE_RECT_PRISM_POS_U,OBJECT3D_FACE_RECT_PRISM_NEG_U,OBJECT3D_FACE_RECT_PRISM_POS_V,
            OBJECT3D_FACE_RECT_PRISM_NEG_V,OBJECT3D_FACE_RECT_PRISM_POS_N,OBJECT3D_FACE_RECT_PRISM_NEG_N};
        bool found=false;
        for (size_t i=0;i<6;++i) if (!strcmp(feature,faces[i])) {out->kind=LAYOUT_REFERENCE_FACE;out->face=kinds[i];found=true;break;}
        if (!found) return false;
    }
    return !field(o,"offset_m") || triple(field(o,"offset_m"),out->local_offset_meters);
}
bool LayoutEditMotion_Handles(const char *op) {
    if(op && !strcmp(op,"set_motion_scope"))return true;
    return op && (!strcmp(op,"set_motion_interlock") || !strcmp(op,"set_travel_limits") || !strcmp(op,"update_telescoping_stay") || !strcmp(op,"attach_telescoping_stay") || !strcmp(op,"detach_telescoping_stay") || !strcmp(op,"set_hinge_offsets") || !strcmp(op,"detach_folding_brace") || !strcmp(op,"set_hinge_fixed_offset") || !strcmp(op,"set_brace_lock") || !strcmp(op,"attach_folding_brace") || !strcmp(op,"create_hinge") || !strcmp(op,"create_travel") || !strcmp(op,"set_travel_position") || !strcmp(op,"generate_motion_envelope"));
}
bool LayoutEditMotion_Apply(Layout *l, const cJSON *row, cJSON *result) {
    const char *op=text(row,"operation"),*id=text(row,"rule_id");
    if (!l || !op || !id || !id[0]) return false;
    if(!strcmp(op,"set_motion_scope")) {
        const LayoutConstraint* old=rule(l,id);const char* scope=text(row,"motion_assembly");
        if(!old || (old->kind!=LAYOUT_CONSTRAINT_ANGULAR_TRAVEL && old->kind!=LAYOUT_CONSTRAINT_LINEAR_TRAVEL) ||
           !scope || strlen(scope)>=sizeof(old->motion_assembly))return false;
        LayoutConstraint c=*old;snprintf(c.motion_assembly,sizeof(c.motion_assembly),"%s",scope);
        return Layout_ConstraintEdit(l,&c,NULL,NULL,NULL);
    }
    if(!strcmp(op,"detach_telescoping_stay")) {
        const LayoutConstraint* old=rule(l,id);
        if(!old || !old->telescoping_stay.enabled)return false;
        LayoutConstraint c=*old;memset(&c.telescoping_stay,0,sizeof(c.telescoping_stay));
        return Layout_ConstraintEdit(l,&c,NULL,NULL,NULL);
    }
    if(!strcmp(op,"set_hinge_offsets")) {
        const LayoutConstraint* old=rule(l,id);double fixed[3],moving[3];
        if(!old || old->kind!=LAYOUT_CONSTRAINT_ANGULAR_TRAVEL || old->folding_brace.enabled || old->telescoping_stay.enabled ||
           !triple(field(row,"fixed_offset_m"),fixed) || !triple(field(row,"moving_offset_m"),moving))return false;
        /* Change both references atomically so a relocated barrel retains the closed leaf pose. */
        LayoutConstraint c=*old;memcpy(c.a.local_offset_meters,fixed,sizeof(fixed));memcpy(c.b.local_offset_meters,moving,sizeof(moving));
        return Layout_ConstraintEdit(l,&c,NULL,NULL,NULL);
    }
    if(!strcmp(op,"set_motion_interlock")) {
        const LayoutConstraint* old=rule(l,id);const char* neighbor=text(row,"closed_neighbor");
        if(!old||!neighbor)return false;
        LayoutConstraint c=*old;if(!copy(c.closed_neighbor,sizeof(c.closed_neighbor),neighbor))return false;
        return Layout_ConstraintEdit(l,&c,NULL,NULL,NULL);
    }
    if(!strcmp(op,"attach_telescoping_stay")||!strcmp(op,"update_telescoping_stay")) {
        const LayoutConstraint* old=rule(l,id);bool update=!strcmp(op,"update_telescoping_stay");
        if(!old||old->telescoping_stay.enabled!=update||old->folding_brace.enabled)return false;
        LayoutConstraint c=*old;return Layout_TelescopingStayRead(field(row,"stay"),&c.telescoping_stay)&&Layout_ConstraintEdit(l,&c,NULL,NULL,NULL);
    }
    if(!strcmp(op,"detach_folding_brace")){const LayoutConstraint* old=rule(l,id);if(!old||!old->folding_brace.enabled)return false;LayoutConstraint c=*old;memset(&c.folding_brace,0,sizeof(c.folding_brace));memset(&c.paired_folding_brace,0,sizeof(c.paired_folding_brace));return Layout_ConstraintEdit(l,&c,NULL,NULL,NULL);}
    if(!strcmp(op,"set_brace_lock")) {const cJSON* engaged=field(row,"engaged");return cJSON_IsBool(engaged)&&Layout_FoldingBraceSetLock(l,id,cJSON_IsTrue(engaged),NULL,NULL);}
    if(!strcmp(op,"set_hinge_fixed_offset")){const LayoutConstraint* old=rule(l,id);double offset[3];if(!old||old->kind!=LAYOUT_CONSTRAINT_ANGULAR_TRAVEL||old->folding_brace.enabled||!triple(field(row,"offset_m"),offset))return false;LayoutConstraint c=*old;memcpy(c.a.local_offset_meters,offset,sizeof(offset));return Layout_ConstraintEdit(l,&c,NULL,NULL,NULL);}
    if (!strcmp(op,"attach_folding_brace")) {
        const LayoutConstraint* old=rule(l,id);
        if(!old || old->folding_brace.enabled || old->telescoping_stay.enabled) return false;
        LayoutConstraint c=*old;
        const cJSON* paired=field(row,"paired_brace");
        return Layout_FoldingBraceRead(field(row,"brace"),&c.folding_brace) &&
            (!paired || Layout_FoldingBraceRead(paired,&c.paired_folding_brace)) && Layout_ConstraintEdit(l,&c,NULL,NULL,NULL);
    }
    if (!strcmp(op,"create_hinge") || !strcmp(op,"create_travel")) {
        bool angular=!strcmp(op,"create_hinge");
        LayoutConstraint c={0};double min,max,position,axis[3];
        if (!copy(c.id,sizeof(c.id),id) || rule(l,id) ||
            !reference(field(row,"fixed"),&c.a) || !reference(field(row,"moving"),&c.b) ||
            !number(row,angular?"min_deg":"min_m",&min) || !number(row,angular?"max_deg":"max_m",&max) || !number(row,angular?"position_deg":"position_m",&position)) return false;
        const char *plane=text(row,"plane");
        if (field(row,"axis")) {
            if (field(row,"plane") || !triple(field(row,"axis"),axis)) return false;
            double norm=hypot(hypot(axis[0],axis[1]),axis[2]);
            if (norm<=1e-12) return false;
            c.axis=(Vec3){axis[0]/norm,axis[1]/norm,axis[2]/norm};
        } else if (plane && !strcmp(plane,"xy")) c.axis=(Vec3){0,0,1};
        else if (plane && !strcmp(plane,"yz")) c.axis=(Vec3){1,0,0};
        else if (plane && !strcmp(plane,"zx")) c.axis=(Vec3){0,1,0};
        else return false;
        const cJSON *scope=field(row,"motion_assembly");
        if (scope) {
            if (!cJSON_IsString(scope) || strlen(scope->valuestring)>=sizeof(c.motion_assembly)) return false;
            snprintf(c.motion_assembly,sizeof(c.motion_assembly),"%s",scope->valuestring);
        }
        if (!(angular ? Layout_InitAngularTravel(l,&c,min,max) : Layout_InitLinearTravel(l,&c,min,max))) return false;
        c.target=position;
        if (!Layout_ConstraintEdit(l,&c,NULL,NULL,NULL)) return false;
        cJSON_AddStringToObject(result,"rule_id",id);
        cJSON_AddNumberToObject(result,angular?"home_deg":"home_m",c.travel_home);
        cJSON_AddNumberToObject(result,angular?"position_deg":"position_m",position);return true;
    }
    if(!strcmp(op,"set_travel_limits")) {
        const LayoutConstraint* old=rule(l,id);double min,max;
        if(!old||(old->kind!=LAYOUT_CONSTRAINT_LINEAR_TRAVEL&&old->kind!=LAYOUT_CONSTRAINT_ANGULAR_TRAVEL))return false;
        bool angular=old->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL;
        if(field(row,angular?"min_m":"min_deg")||field(row,angular?"max_m":"max_deg")||!number(row,angular?"min_deg":"min_m",&min)||!number(row,angular?"max_deg":"max_m",&max))return false;
        LayoutConstraint c=*old;c.travel_min=min;c.travel_max=max;
        return Layout_ConstraintEdit(l,&c,NULL,NULL,NULL);
    }
    if (!strcmp(op,"set_travel_position")) {
        const LayoutConstraint *c=rule(l,id);double position;
        if (!c) return false;
        const char *key=c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL ? "position_deg" : "position_m";
        const char *other=c->kind==LAYOUT_CONSTRAINT_ANGULAR_TRAVEL ? "position_m" : "position_deg";
        if (field(row,other) || !number(row,key,&position) || !Layout_SetTravelPosition(l,id,position,NULL,NULL)) return false;
        cJSON_AddStringToObject(result,"rule_id",id);cJSON_AddNumberToObject(result,key,position);return true;
    }
    if (!strcmp(op,"generate_motion_envelope")) {
        double samples;
        if (!number(row,"samples",&samples) || samples<2 || samples>129 || floor(samples)!=samples ||
            !Layout_GenerateMotionEnvelope(l,id,(uint32_t)samples,NULL,NULL)) return false;
        const LayoutMotionEnvelope *e=Layout_FindRuleEnvelope(&l->objectStore,id);
        const Object3D *o=e ? Layout_ObjectStore_FindConst(&l->objectStore,e->object_id) : NULL;
        if (!o) return false;
        cJSON_AddStringToObject(result,"rule_id",id);cJSON_AddStringToObject(result,"object_id",o->coreMeta.object_id);
        cJSON_AddBoolToObject(result,"current",Layout_MotionEnvelopeCurrent(l,e));return true;
    }
    return false;
}
