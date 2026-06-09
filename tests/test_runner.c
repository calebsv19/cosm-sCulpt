#include "test_framework.h"

#include <stdio.h>
#include <string.h>

bool layout_run_tests(void);
bool test_layout_core_run_tests(void);
bool test_layout_object3d_run_tests(void);
bool test_layout_object3d_store_run_tests(void);
bool test_layout_object3d_resize_run_tests(void);
bool test_layout_hitbox_run_tests(void);
bool test_layout_scene_export_run_tests(void);
bool math_run_tests(void);
bool shape_dataset_run_tests(void);
bool scene_export_run_tests(void);
bool ui_panel_scene_menu_run_tests(void);
bool ui_panel_scene_list_run_tests(void);
bool ui_panel_view_summary_run_tests(void);
bool ui_panel_create_summary_run_tests(void);
bool ui_panel_object_inspector_run_tests(void);
bool ui_panel_file_summary_run_tests(void);
bool ui_panel_file_browser_run_tests(void);
bool input_policy_run_tests(void);
bool pane_host_run_tests(void);
bool host_menu_run_tests(void);
bool recent_contexts_run_tests(void);
bool scene_catalog_run_tests(void);
bool catalog_preview_run_tests(void);
bool root_browser_run_tests(void);
bool workspace_authoring_host_run_tests(void);
bool object_authoring_run_tests(void);
bool object_face_sketch_run_tests(void);

typedef struct TestGroup {
    const char* name;
    TestFunction fn;
    bool run_by_default;
} TestGroup;

static const TestGroup kTestGroups[] = {
    {"Layout", layout_run_tests, true},
    {"LayoutCore", test_layout_core_run_tests, false},
    {"LayoutObject3D", test_layout_object3d_run_tests, false},
    {"LayoutObject3DStore", test_layout_object3d_store_run_tests, false},
    {"LayoutObject3DResize", test_layout_object3d_resize_run_tests, false},
    {"LayoutHitbox", test_layout_hitbox_run_tests, false},
    {"LayoutSceneExport", test_layout_scene_export_run_tests, false},
    {"Math", math_run_tests, true},
    {"ShapeDataset", shape_dataset_run_tests, true},
    {"SceneExport", scene_export_run_tests, true},
    {"UIPanelSceneMenu", ui_panel_scene_menu_run_tests, true},
    {"UIPanelSceneList", ui_panel_scene_list_run_tests, true},
    {"UIPanelViewSummary", ui_panel_view_summary_run_tests, true},
    {"UIPanelCreateSummary", ui_panel_create_summary_run_tests, true},
    {"UIPanelObjectInspector", ui_panel_object_inspector_run_tests, true},
    {"UIPanelFileSummary", ui_panel_file_summary_run_tests, true},
    {"UIPanelFileBrowser", ui_panel_file_browser_run_tests, true},
    {"InputPolicy", input_policy_run_tests, true},
    {"PaneHost", pane_host_run_tests, true},
    {"HostMenu", host_menu_run_tests, true},
    {"RecentContexts", recent_contexts_run_tests, true},
    {"SceneCatalog", scene_catalog_run_tests, true},
    {"CatalogPreview", catalog_preview_run_tests, true},
    {"RootBrowser", root_browser_run_tests, true},
    {"WorkspaceAuthoringHost", workspace_authoring_host_run_tests, true},
    {"object_authoring", object_authoring_run_tests, true},
    {"ObjectFaceSketch", object_face_sketch_run_tests, true},
};

static bool test_group_name_matches(const char* lhs, const char* rhs) {
    if (!lhs || !rhs) return false;
    return strcmp(lhs, rhs) == 0;
}

int main(int argc, char** argv) {
    bool ok = true;
    const char* filter = (argc > 1) ? argv[1] : NULL;
    bool ran_any = false;
    const size_t group_count = sizeof(kTestGroups) / sizeof(kTestGroups[0]);

    for (size_t i = 0; i < group_count; ++i) {
        if (!filter && !kTestGroups[i].run_by_default) {
            continue;
        }
        if (filter && !test_group_name_matches(filter, kTestGroups[i].name)) {
            continue;
        }
        ran_any = true;
        ok &= kTestGroups[i].fn();
    }

    if (!ran_any) {
        fprintf(stderr, "Unknown test group: %s\n", filter ? filter : "");
        fprintf(stderr, "Available test groups:");
        for (size_t i = 0; i < group_count; ++i) {
            fprintf(stderr, " %s", kTestGroups[i].name);
        }
        fprintf(stderr, "\n");
        return 2;
    }

    if (ok) {
        printf("All tests passed\n");
        return 0;
    }

    fprintf(stderr, "Tests failed\n");
    return 1;
}
