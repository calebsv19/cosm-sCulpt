#ifndef LINE_DRAWING_LINE_DRAWING_APP_MAIN_H
#define LINE_DRAWING_LINE_DRAWING_APP_MAIN_H

#include <stdbool.h>

bool line_drawing_app_bootstrap(void);
bool line_drawing_app_config_load(void);
bool line_drawing_app_state_seed(void);
bool line_drawing_app_subsystems_init(void);
bool line_drawing_runtime_start(void);
void line_drawing_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv));
int line_drawing_app_run_loop(void);
void line_drawing_app_shutdown(void);

int line_drawing_app_main(int argc, char **argv);
int line_drawing_app_main_legacy(int argc, char **argv);

#endif
