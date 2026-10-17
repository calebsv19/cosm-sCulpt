#include "Layout/scene/layout_mesh_source_snapshot.h"
#include "Layout/scene/layout_mesh_prepare_job.h"
#include "core_time.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define SOURCE_LIMIT (8u * 1024u * 1024u)

bool Layout_MeshSourceWait(pid_t child, const atomic_bool *canceled, unsigned timeout_ms) {
    if (child <= 0) return false;
    CoreTimeNs start = core_time_now_ns();
    for (;;) {
        int status = 0;
        pid_t done = waitpid(child, &status, WNOHANG);
        if (done == child) return WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
                                  !atomic_load(canceled);
        if (done < 0 && errno != EINTR) return false;
        if (atomic_load(canceled) ||
            core_time_diff_ns(core_time_now_ns(), start) >= (uint64_t)timeout_ms * 1000000u) {
            (void)kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
            return false;
        }
        struct timespec pause = {0, 5000000};
        (void)nanosleep(&pause, NULL);
    }
}

/* After fork in a threaded host, use only async-signal-safe POSIX calls. The
 * child has a private address space: cancel/kill never leaves a worker touching
 * freed jobs, SDL state or shared allocator locks. All decoding stays in parent. */
static void copy_child(const char *source, int output, const struct stat *expected) {
    int input = open(source, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    struct stat stamp;
    if (input < 0 || fstat(input, &stamp) || !S_ISREG(stamp.st_mode) ||
        !Layout_MeshSourceStampSame(expected, &stamp) || stamp.st_size < 0 ||
        stamp.st_size > SOURCE_LIMIT) _exit(1);
    char buffer[65536];
    size_t total = 0;
    for (;;) {
        ssize_t n = read(input, buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 || (size_t)n > SOURCE_LIMIT - total) _exit(1);
        if (!n) break;
        total += (size_t)n;
        size_t written = 0;
        while (written < (size_t)n) {
            ssize_t count = write(output, buffer + written, (size_t)n - written);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) _exit(1);
            written += (size_t)count;
        }
    }
    if (fstat(input, &stamp) || !Layout_MeshSourceStampSame(expected, &stamp) ||
        total != (size_t)expected->st_size) _exit(1);
    (void)close(input);
    if (close(output)) _exit(1);
    _exit(0);
}

bool Layout_MeshSourceSnapshot(const char *source, const struct stat *expected,
                               const atomic_bool *canceled, char **output) {
    if (!output) { errno = EINVAL; return false; }
    *output = NULL;
    if (!source || !expected || !canceled) { errno = EINVAL; return false; }
    if (atomic_load(canceled)) { errno = ECANCELED; return false; }
    const char *root = getenv("TMPDIR");
    if (!root || root[0] != '/') { errno = EINVAL; return false; }
    const char suffix[] = "sculpt-mesh-input-XXXXXX";
    size_t root_length = strlen(root);
    size_t separator = root[root_length - 1] == '/' ? 0u : 1u;
    if (root_length > SIZE_MAX - sizeof(suffix) - separator) {
        errno = ENAMETOOLONG;
        return false;
    }
    size_t capacity = root_length + separator + sizeof(suffix);
    char *path = malloc(capacity);
    if (!path) return false;
    memcpy(path, root, root_length);
    if (separator) path[root_length] = '/';
    memcpy(path + root_length + separator, suffix, sizeof(suffix));
    int fd = mkstemp(path);
    if (fd < 0) { int error = errno; free(path); errno = error; return false; }
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
        int error = errno;
        (void)close(fd); (void)unlink(path); free(path); errno = error;
        return false;
    }
    pid_t child = fork();
    int fork_error = errno;
    if (child == 0) copy_child(source, fd, expected);
    (void)close(fd);
    bool ok = child > 0 && Layout_MeshSourceWait(child, canceled, 2000);
    if (!ok) {
        int error = child < 0 ? fork_error : (atomic_load(canceled) ? ECANCELED : EIO);
        (void)unlink(path); free(path); errno = error;
        return false;
    }
    *output = path;
    return true;
}
