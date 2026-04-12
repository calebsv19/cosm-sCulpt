#include "test_framework.h"

#include <stdio.h>

bool layout_run_tests(void);
bool math_run_tests(void);
bool shape_dataset_run_tests(void);
bool input_policy_run_tests(void);
bool pane_host_run_tests(void);

int main(void) {
    bool ok = true;
    ok &= layout_run_tests();
    ok &= math_run_tests();
    ok &= shape_dataset_run_tests();
    ok &= input_policy_run_tests();
    ok &= pane_host_run_tests();

    if (ok) {
        printf("All tests passed\n");
        return 0;
    }

    fprintf(stderr, "Tests failed\n");
    return 1;
}
