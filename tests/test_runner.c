#include "test_framework.h"

bool layout_run_tests(void);
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
bool object_face_sketch_run_tests(void);

int main(void) {
    bool ok = true;
    ok &= layout_run_tests();
    ok &= math_run_tests();
    ok &= shape_dataset_run_tests();
    ok &= scene_export_run_tests();
    ok &= ui_panel_scene_menu_run_tests();
    ok &= ui_panel_scene_list_run_tests();
    ok &= ui_panel_view_summary_run_tests();
    ok &= ui_panel_create_summary_run_tests();
    ok &= ui_panel_object_inspector_run_tests();
    ok &= ui_panel_file_summary_run_tests();
    ok &= ui_panel_file_browser_run_tests();
    ok &= input_policy_run_tests();
    ok &= pane_host_run_tests();
    ok &= host_menu_run_tests();
    ok &= recent_contexts_run_tests();
    ok &= scene_catalog_run_tests();
    ok &= catalog_preview_run_tests();
    ok &= root_browser_run_tests();
    ok &= workspace_authoring_host_run_tests();
    ok &= object_face_sketch_run_tests();

    if (ok) {
        printf("All tests passed\n");
        return 0;
    }

    fprintf(stderr, "Tests failed\n");
    return 1;
}
