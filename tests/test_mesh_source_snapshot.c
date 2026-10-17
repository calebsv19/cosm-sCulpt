#include "test_artifact_helpers.h"
#include "test_framework.h"
#include "Layout/scene/layout_mesh_source_snapshot.h"
#include "core_time.h"
#include <errno.h>
#include <dirent.h>
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
    char source[PATH_MAX]; TEST_ASSERT(ld_test_artifact_temp_path(source, sizeof(source), "ld-mesh-snapshot-XXXXXX"));
    char *output = NULL;
    int fd = mkstemp(source); TEST_ASSERT(fd >= 0);
    const char bytes[] = "immutable mesh source";
    TEST_ASSERT(write(fd, bytes, sizeof(bytes)) == sizeof(bytes)); close(fd);
    struct stat expected; TEST_ASSERT(!stat(source, &expected));
    atomic_bool cancel = ATOMIC_VAR_INIT(false);
    TEST_ASSERT(Layout_MeshSourceSnapshot(source, &expected, &cancel, &output));
    char actual[sizeof(bytes)] = {0}; fd = open(output, O_RDONLY); TEST_ASSERT(fd >= 0);
    TEST_ASSERT(read(fd, actual, sizeof(actual)) == sizeof(actual)); close(fd);
    TEST_ASSERT(!memcmp(bytes, actual, sizeof(bytes))); unlink(output); free(output); output = NULL;
    ++expected.st_size;
    TEST_ASSERT(!Layout_MeshSourceSnapshot(source, &expected, &cancel, &output));
    TEST_ASSERT(!output);
    atomic_store(&cancel, true);
    TEST_ASSERT(!Layout_MeshSourceSnapshot(source, &expected, &cancel, &output));
    unlink(source); return true;
}
static bool special_file_rejected_without_wait(void) {
    char source[PATH_MAX]; TEST_ASSERT(ld_test_artifact_temp_path(source, sizeof(source), "ld-mesh-fifo-XXXXXX"));
    char *output = NULL;
    int fd = mkstemp(source); TEST_ASSERT(fd >= 0); close(fd); unlink(source);
    TEST_ASSERT(!mkfifo(source, 0600));
    struct stat expected; TEST_ASSERT(!stat(source, &expected));
    atomic_bool cancel = ATOMIC_VAR_INIT(false);
    CoreTimeNs start = core_time_now_ns();
    TEST_ASSERT(!Layout_MeshSourceSnapshot(source, &expected, &cancel, &output));
    TEST_ASSERT(core_time_diff_ns(core_time_now_ns(), start) < 1000000000u);
    TEST_ASSERT(!output); unlink(source); return true;
}
typedef struct {
    char root[PATH_MAX], source[PATH_MAX], occupied[PATH_MAX];
    struct stat stamp;
    char *first, *second;
} SnapshotFixture;

static bool fixture_open(SnapshotFixture *f) {
    memset(f, 0, sizeof(*f));
    if (!ld_test_artifact_temp_path(f->root, sizeof(f->root), "ld-snapshot-roots-XXXXXX") ||
        !mkdtemp(f->root) ||
        !ld_test_artifact_join_path(f->source, sizeof(f->source), f->root, "source") ||
        !ld_test_artifact_write_text_file(f->source, "unchanged source") ||
        stat(f->source, &f->stamp)) return false;
    return true;
}

static void fixture_close(SnapshotFixture *f) {
    if (f->first) { unlink(f->first); free(f->first); }
    if (f->second) { unlink(f->second); free(f->second); }
    if (f->occupied[0]) unlink(f->occupied);
    unlink(f->source);
    rmdir(f->root);
}

static bool in_temp_root(const char *root, bool (*run)(void *), void *data) {
    const char *prior = getenv("TMPDIR");
    char *saved = prior ? strdup(prior) : NULL;
    if (prior && !saved) return false;
    int changed = root ? setenv("TMPDIR", root, 1) : unsetenv("TMPDIR");
    bool ok = changed == 0 && run(data);
    int restored = saved ? setenv("TMPDIR", saved, 1) : unsetenv("TMPDIR");
    free(saved);
    return ok && restored == 0;
}

static int directory_entries(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return -1;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)))
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) ++count;
    closedir(dir);
    return count;
}

static bool snapshot_in_long_root(void *data) {
    SnapshotFixture *f = data;
    atomic_bool cancel = ATOMIC_VAR_INIT(false);
    TEST_ASSERT(Layout_MeshSourceSnapshot(f->source, &f->stamp, &cancel, &f->first));
    const char *root = getenv("TMPDIR");
    TEST_ASSERT(strlen(f->first) > 128 && !strncmp(f->first, root, strlen(root)));
    struct stat source_after, snapshot;
    TEST_ASSERT(!stat(f->source, &source_after) && !stat(f->first, &snapshot));
    TEST_ASSERT(source_after.st_ino == f->stamp.st_ino && source_after.st_size == f->stamp.st_size);
    TEST_ASSERT((snapshot.st_mode & 0777) == 0600);
    FILE *file = fopen(f->first, "rb");
    TEST_ASSERT(file);
    char bytes[32] = {0};
    size_t n = fread(bytes, 1, sizeof(bytes) - 1, file);
    TEST_ASSERT(fclose(file) == 0 && n == strlen("unchanged source") && !strcmp(bytes, "unchanged source"));
    TEST_ASSERT(unlink(f->first) == 0);
    free(f->first); f->first = NULL;
    TEST_ASSERT(directory_entries(root) == 0);
    return true;
}

static bool long_owned_root_and_cleanup(void) {
    SnapshotFixture f;
    TEST_ASSERT(fixture_open(&f));
    char paths[3][PATH_MAX];
    const char *parent = f.root;
    size_t created = 0;
    bool ok = true;
    for (size_t i = 0; i < 3; ++i) {
        if (!ld_test_artifact_join_path(paths[i], sizeof(paths[i]), parent,
                "long-owned-directory-for-release-preparation-snapshot-path-capacity") ||
            mkdir(paths[i], 0700)) { ok = false; break; }
        ++created; parent = paths[i];
    }
    if (ok) ok = in_temp_root(parent, snapshot_in_long_root, &f);
    while (created) { --created; if (rmdir(paths[created])) ok = false; }
    fixture_close(&f);
    return ok;
}

typedef struct { SnapshotFixture *fixture; int error; } Refusal;
static bool snapshot_root_refused(void *data) {
    Refusal *r = data;
    atomic_bool cancel = ATOMIC_VAR_INIT(false);
    errno = 0;
    TEST_ASSERT(!Layout_MeshSourceSnapshot(r->fixture->source, &r->fixture->stamp,
                                          &cancel, &r->fixture->first));
    TEST_ASSERT(!r->fixture->first && errno == r->error);
    return true;
}

static bool unavailable_and_unwritable_roots(void) {
    SnapshotFixture f;
    TEST_ASSERT(fixture_open(&f));
    char missing[PATH_MAX], readonly[PATH_MAX];
    TEST_ASSERT(ld_test_artifact_join_path(missing, sizeof(missing), f.root, "missing"));
    TEST_ASSERT(ld_test_artifact_join_path(readonly, sizeof(readonly), f.root, "readonly"));
    TEST_ASSERT(mkdir(readonly, 0500) == 0);
    Refusal r = {&f, EINVAL};
    bool ok = in_temp_root(NULL, snapshot_root_refused, &r);
    ok = in_temp_root("", snapshot_root_refused, &r) && ok;
    ok = in_temp_root("relative-root", snapshot_root_refused, &r) && ok;
    r.error = ENOENT; ok = in_temp_root(missing, snapshot_root_refused, &r) && ok;
    r.error = ENOTDIR; ok = in_temp_root(f.source, snapshot_root_refused, &r) && ok;
    r.error = EACCES; ok = in_temp_root(readonly, snapshot_root_refused, &r) && ok;
    if (chmod(readonly, 0700) || directory_entries(readonly) != 0 || rmdir(readonly)) ok = false;
    if (directory_entries(f.root) != 1) ok = false;
    fixture_close(&f);
    return ok;
}

static bool unique_outputs_preserve_collision_and_failure_cleanup(void *data) {
    SnapshotFixture *f = data;
    TEST_ASSERT(ld_test_artifact_join_path(f->occupied, sizeof(f->occupied), f->root,
                                          "sculpt-mesh-input-XXXXXX"));
    TEST_ASSERT(ld_test_artifact_write_text_file(f->occupied, "preserve collision"));
    atomic_bool cancel = ATOMIC_VAR_INIT(false);
    TEST_ASSERT(Layout_MeshSourceSnapshot(f->source, &f->stamp, &cancel, &f->first));
    TEST_ASSERT(Layout_MeshSourceSnapshot(f->source, &f->stamp, &cancel, &f->second));
    TEST_ASSERT(strcmp(f->first, f->second) && strcmp(f->first, f->occupied) && strcmp(f->second, f->occupied));
    struct stat a, b;
    TEST_ASSERT(!stat(f->first, &a) && !stat(f->second, &b) && a.st_ino != b.st_ino);
    TEST_ASSERT(directory_entries(f->root) == 4);
    struct stat changed = f->stamp; ++changed.st_size;
    char *failed = NULL;
    TEST_ASSERT(!Layout_MeshSourceSnapshot(f->source, &changed, &cancel, &failed));
    TEST_ASSERT(!failed && directory_entries(f->root) == 4);
    atomic_store(&cancel, true);
    TEST_ASSERT(!Layout_MeshSourceSnapshot(f->source, &f->stamp, &cancel, &failed));
    TEST_ASSERT(!failed && directory_entries(f->root) == 4);
    FILE *file = fopen(f->occupied, "rb");
    TEST_ASSERT(file);
    char bytes[32] = {0};
    size_t n = fread(bytes, 1, sizeof(bytes) - 1, file);
    TEST_ASSERT(fclose(file) == 0 && n == strlen("preserve collision") && !strcmp(bytes, "preserve collision"));
    TEST_ASSERT(!unlink(f->first) && !unlink(f->second));
    free(f->first); free(f->second); f->first = f->second = NULL;
    TEST_ASSERT(directory_entries(f->root) == 2);
    return true;
}

static bool unique_outputs_and_cleanup(void) {
    SnapshotFixture f;
    TEST_ASSERT(fixture_open(&f));
    bool ok = in_temp_root(f.root, unique_outputs_preserve_collision_and_failure_cleanup, &f);
    fixture_close(&f);
    return ok;
}

bool mesh_source_snapshot_run_tests(void) {
    const TestCase cases[] = {{"timeout_reaps_blocked_child", timeout_reaps_blocked_child},
        {"cancellation_reaps_blocked_child", cancellation_reaps_blocked_child},
        {"regular_snapshot_and_changed_stamp", regular_snapshot_and_changed_stamp},
        {"special_file_rejected_without_wait", special_file_rejected_without_wait},
        {"long_owned_root_and_cleanup", long_owned_root_and_cleanup},
        {"unavailable_and_unwritable_roots", unavailable_and_unwritable_roots},
        {"unique_outputs_and_cleanup", unique_outputs_and_cleanup}};
    return run_test_cases("MeshSourceSnapshot", cases, sizeof(cases)/sizeof(*cases));
}
