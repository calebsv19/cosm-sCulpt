#include "test_layout_internal.h"
#include "Layout/scene/layout_folding_brace.h"
#include "Layout/layout_constraints.h"
static bool unequal_upward_endpoint_contract(void){
    const double h=.025,ha=.035,r=.30,p=.36,d=hypot(ha-h,p-r)-.001,D=hypot(r+ha,p-h),l1=(D+d)/2,l2=(D-d)/2;
    Vec3 a={0,-ha,-p},normal={-1,0,0};
    for(int sample=0;sample<=900;++sample){double angle=sample*.1*.017453292519943295;Vec3 b={0,r*cos(angle)-h*sin(angle),-r*sin(angle)-h*cos(angle)},e;
        TEST_ASSERT(Layout_FoldingElbow(a,b,normal,l1,l2,-1,&e));TEST_ASSERT(fabs(Vec3_Distance(a,e)-l1)<2e-6&&fabs(Vec3_Distance(e,b)-l2)<2e-6);TEST_ASSERT(e.z>a.z&&e.y>=-.045);}
    /* The rejected provisional mounting is unreachable near its stowed endpoint. */
    Vec3 old_a={0,-.023,-.36},b={0,.32*cos(89.5*.017453292519943295)-.023*sin(89.5*.017453292519943295),-.32*sin(89.5*.017453292519943295)-.023*cos(89.5*.017453292519943295)},e;
    double old_D=hypot(.343,.337);TEST_ASSERT(!Layout_FoldingElbow(old_a,b,normal,(old_D+.04)/2,(old_D-.04)/2,-1,&e));return true;
}
static bool native_manual_lock_history_and_roundtrip(void){
    const char* scene=getenv("SCULPT_SINK_SUPPORT_V2_LAYOUT");if(!scene)return true;ld_test_init_runtime();GlobalState* s=Global_Get();TEST_ASSERT(Layout_LoadFromFile(&s->layout,scene));Global_OnLayoutLoaded(scene);Editor_ClearHistory(&s->editor);char* original=Layout_SaveToString(&s->layout);TEST_ASSERT(original);
    TEST_ASSERT(!Layout_SetTravelPosition(&s->layout,"worktop_hinge",45,Layout_GeometryHistory,NULL));TEST_ASSERT(strstr(s->layout.geometryMessage,"Release"));char* after=Layout_SaveToString(&s->layout);TEST_ASSERT(!strcmp(original,after)&&Editor_UndoCount(&s->editor)==0);free(after);
    TEST_ASSERT(Layout_FoldingBraceSetLock(&s->layout,"worktop_hinge",false,Layout_GeometryHistory,NULL));
    for(int i=0;i<3;++i){TEST_ASSERT(Layout_SetTravelPosition(&s->layout,"worktop_hinge",i*45,Layout_GeometryHistory,NULL));char* saved=Layout_SaveToString(&s->layout);Layout loaded={0};Layout_Init(&loaded,.05f);TEST_ASSERT(saved&&Layout_LoadFromString(&loaded,saved)&&Layout_FoldingBracesValidate(&loaded,true));TEST_ASSERT(!loaded.objectStore.constraints[1].folding_brace.lock_engaged);Layout_Free(&loaded);free(saved);}
    size_t history=Editor_UndoCount(&s->editor);char* saved=Layout_SaveToString(&s->layout);TEST_ASSERT(!Layout_FoldingBraceSetLock(&s->layout,"worktop_hinge",true,Layout_GeometryHistory,NULL));after=Layout_SaveToString(&s->layout);TEST_ASSERT(!strcmp(saved,after)&&history==Editor_UndoCount(&s->editor)&&strstr(s->layout.geometryMessage,"Deploy"));free(saved);free(after);
    TEST_ASSERT(Layout_SetTravelPosition(&s->layout,"worktop_hinge",0,Layout_GeometryHistory,NULL)&&Layout_FoldingBraceSetLock(&s->layout,"worktop_hinge",true,Layout_GeometryHistory,NULL));
    while(Editor_UndoCount(&s->editor))TEST_ASSERT(Editor_Undo(&s->editor,&s->layout));after=Layout_SaveToString(&s->layout);TEST_ASSERT(!strcmp(original,after));free(original);free(after);ld_test_shutdown_runtime();return true;
}
bool folding_support_v2_run_tests(void){const TestCase cases[]={{"unequal_upward_range_and_rejected_mount",unequal_upward_endpoint_contract},{"native_manual_lock_history_roundtrip",native_manual_lock_history_and_roundtrip}};return run_test_cases("FoldingSupportV2",cases,sizeof(cases)/sizeof(cases[0]));}
