#include "Layout/layout_scene_collections.h"
#include "Layout/layout_coverings.h"
#include "test_layout_internal.h"
#include "test_artifact_helpers.h"
#include "Layout/asset/layout_imported_mesh_asset.h"
#include "UI/panel/ui_panel_stl_options.h"
#include "UI/panel/ui_panel_file_browser_internal.h"
#include "Tools/scene_export.h"
#include "Core/scene_document_lifecycle.h"
#include "core_mesh_asset.h"

#include <math.h>

/* Offset tetrahedron, not a device proxy: known X/Y/Z spans and source origin. */
static bool write_offset_stl(const char* path) {
    const float points[4][3] = {{10,-20,30},{101,-20,30},{10,47,30},{10,-20,225}};
    const int faces[4][3] = {{0,2,1},{0,1,3},{0,3,2},{1,2,3}};
    FILE* f = fopen(path,"wb");
    if (!f) return false;
    fprintf(f,"solid dimensions\n");
    for (int i=0;i<4;++i) {
        fprintf(f,"facet normal 0 0 0\nouter loop\n");
        for (int j=0;j<3;++j) {
            const float* v=points[faces[i][j]];
            fprintf(f,"vertex %.9g %.9g %.9g\n",v[0],v[1],v[2]);
        }
        fprintf(f,"endloop\nendfacet\n");
    }
    fprintf(f,"endsolid dimensions\n");
    return fclose(f)==0;
}

static bool test_units_compile_and_cache(void) {
    char dir[]="/tmp/sculpt_units_XXXXXX";
    TEST_ASSERT(mkdtemp(dir)!=NULL);
    char stl[1024],a[1024],r[1024],preview[1024],d[256];
    snprintf(stl,sizeof(stl),"%s/device.stl",dir);
    TEST_ASSERT(write_offset_stl(stl));
    const CoreUnitKind units[]={CORE_UNIT_METER,CORE_UNIT_CENTIMETER,CORE_UNIT_MILLIMETER,CORE_UNIT_INCH,CORE_UNIT_FOOT};
    const double scales[]={1,.01,.001,.0254,.3048};
    for (size_t i=0;i<5;++i) {
        TEST_ASSERT(LayoutImportedMeshAsset_GetStlCacheStateForUnit(stl,units[i],dir,a,sizeof(a),r,sizeof(r),preview,sizeof(preview),d,sizeof(d))==LAYOUT_IMPORTED_MESH_STL_CACHE_MISSING);
        TEST_ASSERT(LayoutImportedMeshAsset_ImportStlToRuntimeWithProgressForUnit(stl,units[i],dir,a,sizeof(a),r,sizeof(r),d,sizeof(d),NULL,NULL));
        CoreMeshAssetRuntimeDocument mesh;
        core_mesh_asset_runtime_document_init(&mesh);
        TEST_ASSERT(core_mesh_asset_runtime_document_load_file(r,&mesh).code==CORE_OK);
        TEST_ASSERT(fabs(mesh.contract.local_bounds.min.x-10*scales[i])<1e-7);
        TEST_ASSERT(fabs(mesh.contract.local_bounds.min.y+20*scales[i])<1e-7);
        TEST_ASSERT(fabs(mesh.contract.local_bounds.max.z-225*scales[i])<1e-7);
        TEST_ASSERT(fabs(mesh.contract.local_bounds.max.x-mesh.contract.local_bounds.min.x-91*scales[i])<1e-7);
        core_mesh_asset_runtime_document_free(&mesh);
        CoreMeshAssetAuthoringDocument authoring;
        core_mesh_asset_authoring_document_init(&authoring);
        TEST_ASSERT(core_mesh_asset_authoring_document_load_file(a,&authoring).code==CORE_OK);
        TEST_ASSERT(authoring.imported_mesh_source.source_unit_kind==units[i]);
        TEST_ASSERT(fabs(authoring.imported_mesh_source.source_to_asset_scale-scales[i])<1e-12);
        TEST_ASSERT(!strcmp(authoring.imported_mesh_source.orientation_policy,"source_axes"));
        core_mesh_asset_authoring_document_free(&authoring);
        TEST_ASSERT(LayoutImportedMeshAsset_GetStlCacheStateForUnit(stl,units[i],dir,a,sizeof(a),r,sizeof(r),preview,sizeof(preview),d,sizeof(d))==LAYOUT_IMPORTED_MESH_STL_CACHE_FRESH);
    }
    TEST_ASSERT(!LayoutImportedMeshAsset_ImportStlToRuntimeWithProgressForUnit(stl,CORE_UNIT_UNKNOWN,dir,a,sizeof(a),r,sizeof(r),d,sizeof(d),NULL,NULL));
    char collision[1024];
    snprintf(collision,sizeof(collision),"%s/device_mm.stl",dir);
    TEST_ASSERT(write_offset_stl(collision));
    TEST_ASSERT(LayoutImportedMeshAsset_GetStlCacheStateForUnit(collision,CORE_UNIT_METER,dir,a,sizeof(a),r,sizeof(r),preview,sizeof(preview),d,sizeof(d))==LAYOUT_IMPORTED_MESH_STL_CACHE_STALE);
    TEST_ASSERT(!LayoutImportedMeshAsset_ImportStlToRuntimeWithProgressForUnit(collision,CORE_UNIT_METER,dir,a,sizeof(a),r,sizeof(r),d,sizeof(d),NULL,NULL));
    TEST_ASSERT(strstr(d,"another source or unit")!=NULL);
    unlink(collision);
    /* Remove only outputs created above. */
    for (size_t i=0;i<5;++i) {
        TEST_ASSERT(LayoutImportedMeshAsset_ResolveStlOutputPathsForUnit(stl,units[i],dir,a,sizeof(a),r,sizeof(r),preview,sizeof(preview),d,sizeof(d)));
        unlink(a);unlink(r);unlink(preview);
    }
    unlink(stl);TEST_ASSERT(rmdir(dir)==0);
    return true;
}

static bool test_exact_size_history_reload_and_export(void) {
    char dir[]="/tmp/sculpt_exact_XXXXXX";
    TEST_ASSERT(mkdtemp(dir)!=NULL);
    char stl[1024],saved[1024],d[256];
    snprintf(stl,sizeof(stl),"%s/device.stl",dir);
    snprintf(saved,sizeof(saved),"%s/saved.layout.json",dir);
    TEST_ASSERT(write_offset_stl(stl));
    ld_test_init_runtime();
    GlobalState* state=Global_Get();
    UIPanelState* ui=UIPanel_Get();
    ui->loadMenu.stlSourceUnit=CORE_UNIT_MILLIMETER;
    LayoutSceneCollection group={.id="@scene.sink",.name="Sink cabinet"};
    TEST_ASSERT(Layout_EditSceneCollection(&state->layout,&group,NULL,NULL,NULL));
    snprintf(ui->loadMenu.stlCollection,64,"%s",group.id);ui->loadMenu.stlPartRole=1;
    state->layout.metersPerWorldUnit=.01;
    TEST_ASSERT(!ui->loadMenu.stlFitToScene);
    TEST_ASSERT(Global_SetObjectAssetRoot(dir,false));
    TEST_ASSERT(UIPanel_ImportStlAndPlaceFromPath(stl));
    uint32_t id=state->editor.selectedObject3DId;
    const Object3D* obj=Layout_ObjectStore_FindConst(&state->layout.objectStore,id);
    TEST_ASSERT(obj!=NULL);
    TEST_ASSERT(Layout_SceneCollectionContains(&state->layout.objectStore,group.id,obj->coreMeta.object_id));
    TEST_ASSERT(Layout_CoveringCollectionContains(&state->layout.objectStore,"@coverings.doors",obj->coreMeta.object_id));
    TEST_ASSERT(fabs(obj->transform.scale.x-100)<1e-5);
    TEST_ASSERT(obj->transform.position.x==0 && obj->transform.rotationDeg.x==0);
    TEST_ASSERT(fabs((obj->meshInstance.localBoundsMax.x-obj->meshInstance.localBoundsMin.x)*obj->transform.scale.x*Layout_WorldScale(&state->layout)-.091)<1e-7);
    TEST_ASSERT(fabs(obj->meshInstance.localBoundsMin.x-.010)<1e-7);
    TEST_ASSERT(Editor_Undo(&state->editor,&state->layout));
    TEST_ASSERT(Layout_ObjectStore_FindConst(&state->layout.objectStore,id)==NULL);
    TEST_ASSERT(Editor_Redo(&state->editor,&state->layout));
    TEST_ASSERT(Layout_SaveToFile(&state->layout,saved));
    TEST_ASSERT(Layout_LoadFromFile(&state->layout,saved));
    obj=Layout_ObjectStore_FindConst(&state->layout.objectStore,id);
    TEST_ASSERT(obj!=NULL && fabs(obj->transform.scale.x-100)<1e-5);
    TEST_ASSERT(Layout_SceneCollectionContains(&state->layout.objectStore,group.id,obj->coreMeta.object_id));
    SceneBounds3D bounds=state->layout.scene3d.bounds;
    state->layout.scene3d.bounds.enabled=true;
    state->layout.scene3d.bounds.min=(Vec3){1,1,1};
    state->layout.scene3d.bounds.max=(Vec3){3,3,3};
    size_t undo_count=Editor_UndoCount(&state->editor);
    TEST_ASSERT(!UIPanel_ImportStlAndPlaceFromPath(stl));
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore)==1);
    TEST_ASSERT(Editor_UndoCount(&state->editor)==undo_count);
    TEST_ASSERT(strstr(Global_GetObjectRuntimeMeshStatus(),"source origin")!=NULL);
    state->layout.scene3d.bounds=bounds;
    LineDrawingSceneExportPaths paths;
    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToOutputRoot(&state->layout,saved,dir,&paths,d,sizeof(d)));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(paths.runtime_path));
    LineDrawingSceneDocument_RemoveClonedBundle(paths.scene_dir);
    ld_test_shutdown_runtime();
    char a[1024],r[1024],p[1024];
    TEST_ASSERT(LayoutImportedMeshAsset_ResolveStlOutputPathsForUnit(stl,CORE_UNIT_MILLIMETER,dir,a,sizeof(a),r,sizeof(r),p,sizeof(p),d,sizeof(d)));
    unlink(a);unlink(r);unlink(p);unlink(stl);unlink(saved);
    /* Export also leaves root-level scene-index metadata, owned by this fixture. */
    LineDrawingSceneDocument_RemoveClonedBundle(dir);
    return true;
}

static bool test_mouse_options_and_worker_lock(void) {
    ld_test_init_runtime();
    UIPanelState* ui=UIPanel_Get();
    ui->activeLeftTab=UI_PANEL_LEFT_TAB_FILE;
    UIPanel_OnWindowResized(Global_Get()->screenWidth,Global_Get()->screenHeight);
    UIPanel_ActivateStlImportBrowser();
    ui->loadMenu.mode=UI_LOAD_MENU_MODE_STL_IMPORT;
    SDL_Rect rect=UIPanel_StlOptionRect(ui,0);
    SDL_Rect list=UIPanel_GetLoadMenuListClipRect(ui);
    TEST_ASSERT(rect.w>0 && rect.y+rect.h<=list.y);
    TEST_ASSERT(UIPanel_StlSourceUnit(ui)==CORE_UNIT_METER);
    TEST_ASSERT(UIPanel_HandleStlOptionsClick(ui,rect.x+2,rect.y+2));
    TEST_ASSERT(UIPanel_StlSourceUnit(ui)==CORE_UNIT_CENTIMETER);
    TEST_ASSERT(UIPanel_HandleStlOptionsClick(ui,rect.x+2,rect.y+2));
    TEST_ASSERT(UIPanel_StlSourceUnit(ui)==CORE_UNIT_MILLIMETER);
    ui->loadMenu.asyncStlActive=true;
    TEST_ASSERT(UIPanel_HandleStlOptionsClick(ui,rect.x+2,rect.y+2));
    TEST_ASSERT(UIPanel_StlSourceUnit(ui)==CORE_UNIT_MILLIMETER);
    ui->loadMenu.asyncStlActive=false;
    rect=UIPanel_StlOptionRect(ui,1);
    TEST_ASSERT(UIPanel_HandleStlOptionsClick(ui,rect.x+2,rect.y+2));
    TEST_ASSERT(ui->loadMenu.stlFitToScene);
    ld_test_shutdown_runtime();
    return true;
}

bool stl_physical_import_run_tests(void) {
    const TestCase cases[]={
        {"units_compile_cache",test_units_compile_and_cache},
        {"exact_history_reload_export",test_exact_size_history_reload_and_export},
        {"mouse_options_worker_lock",test_mouse_options_and_worker_lock}
    };
    return run_test_cases("STLPhysicalImport",cases,sizeof(cases)/sizeof(cases[0]));
}
