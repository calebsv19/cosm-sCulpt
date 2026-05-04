#include "test_layout_internal.h"

bool test_layout_core_run_tests(void);
bool test_layout_object3d_run_tests(void);
bool test_layout_hitbox_run_tests(void);
bool test_layout_scene_export_run_tests(void);

bool layout_run_tests(void) {
    bool ok = true;
    ok &= test_layout_core_run_tests();
    ok &= test_layout_object3d_run_tests();
    ok &= test_layout_hitbox_run_tests();
    ok &= test_layout_scene_export_run_tests();
    return ok;
}
