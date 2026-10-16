#include "test_framework.h"
#include "Layout/scene/layout_mesh_source_snapshot.h"
#include "core_time.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static pid_t blocked_child(void) {
    int pipefd[2];
    if (pipe(pipefd)) return -1;
    pid_t pid = fork();
    if (!pid) { char c; (void)read(pipefd[0], &c, 1); _exit(0); }
    close(pipefd[0]); close(pipefd[1]);
    return pid;
}
static bool timeout_reaps_blocked_child(void) {
    atomic_bool cancel = ATOMIC_VAR_INIT(false);
    pid_t child = blocked_child(); TEST_ASSERT(child > 0);
    CoreTimeNs start = core_time_now_ns();
    TEST_ASSERT(!Layout_MeshSourceWait(child, &cancel, 30));
    TEST_ASSERT(core_time_diff_ns(core_time_now_ns(), start) < 1000000000u);
    TEST_ASSERT(waitpid(child, NULL, WNOHANG) == -1 && errno == ECHILD);
    return true;
}
static bool cancellation_reaps_blocked_child(void) {
    atomic_bool cancel = ATOMIC_VAR_INIT(true);
    pid_t child = blocked_child(); TEST_ASSERT(child > 0);
    CoreTimeNs start = core_time_now_ns();
    TEST_ASSERT(!Layout_MeshSourceWait(child, &cancel, 2000));
    TEST_ASSERT(core_time_diff_ns(core_time_now_ns(), start) < 1000000000u);
    TEST_ASSERT(waitpid(child, NULL, WNOHANG) == -1 && errno == ECHILD);
    return true;
}
static bool regular_snapshot_and_changed_stamp(void) {
    char source[] = "/private/tmp/ld-mesh-snapshot-XXXXXX", output[128];
    int fd = mkstemp(source); TEST_ASSERT(fd >= 0);
    const char bytes[] = "immutable mesh source";
    TEST_ASSERT(write(fd, bytes, sizeof(bytes)) == sizeof(bytes)); close(fd);
    struct stat expected; TEST_ASSERT(!stat(source, &expected));
    atomic_bool cancel = ATOMIC_VAR_INIT(false);
    TEST_ASSERT(Layout_MeshSourceSnapshot(source, &expected, &cancel, output));
    char actual[sizeof(bytes)] = {0}; fd = open(output, O_RDONLY); TEST_ASSERT(fd >= 0);
    TEST_ASSERT(read(fd, actual, sizeof(actual)) == sizeof(actual)); close(fd);
    TEST_ASSERT(!memcmp(bytes, actual, sizeof(bytes))); unlink(output);
    ++expected.st_size;
    TEST_ASSERT(!Layout_MeshSourceSnapshot(source, &expected, &cancel, output));
    TEST_ASSERT(!output[0]);
    atomic_store(&cancel, true);
    TEST_ASSERT(!Layout_MeshSourceSnapshot(source, &expected, &cancel, output));
    unlink(source); return true;
}
static bool special_file_rejected_without_wait(void) {
    char source[] = "/private/tmp/ld-mesh-fifo-XXXXXX", output[128];
    int fd = mkstemp(source); TEST_ASSERT(fd >= 0); close(fd); unlink(source);
    TEST_ASSERT(!mkfifo(source, 0600));
    struct stat expected; TEST_ASSERT(!stat(source, &expected));
    atomic_bool cancel = ATOMIC_VAR_INIT(false);
    CoreTimeNs start = core_time_now_ns();
    TEST_ASSERT(!Layout_MeshSourceSnapshot(source, &expected, &cancel, output));
    TEST_ASSERT(core_time_diff_ns(core_time_now_ns(), start) < 1000000000u);
    TEST_ASSERT(!output[0]); unlink(source); return true;
}
bool mesh_source_snapshot_run_tests(void) {
    const TestCase cases[] = {{"timeout_reaps_blocked_child", timeout_reaps_blocked_child},
        {"cancellation_reaps_blocked_child", cancellation_reaps_blocked_child},
        {"regular_snapshot_and_changed_stamp", regular_snapshot_and_changed_stamp},
        {"special_file_rejected_without_wait", special_file_rejected_without_wait}};
    return run_test_cases("MeshSourceSnapshot", cases, sizeof(cases)/sizeof(*cases));
}
