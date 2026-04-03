#include <stdbool.h>

bool input_policy_run_tests(void);

int main(void) {
    return input_policy_run_tests() ? 0 : 1;
}
