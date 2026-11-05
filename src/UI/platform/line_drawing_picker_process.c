#define _POSIX_C_SOURCE 200809L
#include "UI/platform/line_drawing_picker_process.h"
#if defined(__linux__) || defined(LINE_DRAWING_FOLDER_PICKER_FORCE_LINUX)
#include <SDL2/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

/* A private group lets cancellation also stop helpers forked by the dialog.
 * Keep the leader waitable until the final group signal so its ID cannot recycle. */
static int launch(const char* const argv[], int pipefd[2], pid_t* child) {
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    int err = posix_spawn_file_actions_init(&actions);
    if (err) return err;
    err = posix_spawnattr_init(&attr);
    if (!err) {
        err = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
        if (!err) err = posix_spawnattr_setpgroup(&attr, 0);
        if (!err) err = posix_spawn_file_actions_addclose(&actions, pipefd[0]);
        if (!err) err = posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
        if (!err && pipefd[1] != STDOUT_FILENO)
            err = posix_spawn_file_actions_addclose(&actions, pipefd[1]);
        if (!err) err = posix_spawnp(child, argv[0], &actions, &attr, (char* const*)argv, environ);
        posix_spawnattr_destroy(&attr);
    }
    posix_spawn_file_actions_destroy(&actions);
    return err;
}

static bool quit_requested(void) {
    if (!SDL_WasInit(SDL_INIT_EVENTS)) return false;
    SDL_PumpEvents();
    return SDL_HasEvent(SDL_QUIT); /* Leave quit for the normal event loop. */
}

static LineDrawingFolderPickerResult collect(pid_t child, int fd, char* path, size_t capacity) {
    size_t used = 0;
    bool eof = false;
    LineDrawingFolderPickerResult result = LINE_DRAWING_FOLDER_PICKER_FAILED;
    for (;;) {
        if (quit_requested()) { result = LINE_DRAWING_FOLDER_PICKER_CANCELLED; break; }
        siginfo_t info = {0};
        if (waitid(P_PID, (id_t)child, &info, WEXITED|WNOHANG|WNOWAIT) < 0) {
            if (errno == EINTR) continue;
            if (errno == ECHILD) return result;
            break;
        }
        if (!eof) {
            char chunk[512];
            ssize_t n = read(fd, chunk, sizeof(chunk));
            if (n > 0) {
                for (ssize_t i = 0; i < n; ++i) {
                    if (used + 1 < capacity && chunk[i]) path[used++] = chunk[i];
                    else if (chunk[i] != '\r' && chunk[i] != '\n') goto finish;
                }
                continue;
            }
            if (n < 0 && errno == EINTR) continue;
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) break;
            eof = n == 0;
        }
        if (info.si_pid == child) {
            if (info.si_code == CLD_EXITED) {
                if (info.si_status == 127) result = LINE_DRAWING_FOLDER_PICKER_UNAVAILABLE;
                else if (info.si_status == 1) result = LINE_DRAWING_FOLDER_PICKER_CANCELLED;
                else if (!info.si_status) {
                    while (used && (path[used-1]=='\n' || path[used-1]=='\r')) used--;
                    path[used] = 0;
                    result = used ? LINE_DRAWING_FOLDER_PICKER_SELECTED : LINE_DRAWING_FOLDER_PICKER_CANCELLED;
                }
            }
            break;
        }
        struct pollfd readable = {.fd=eof ? -1 : fd, .events=POLLIN};
        if (poll(&readable,1,25) < 0 && errno != EINTR) break;
    }
finish:
    (void)kill(-child,SIGKILL);
    pid_t reaped;
    do { reaped = waitpid(child,NULL,0); } while (reaped < 0 && errno == EINTR);
    return reaped == child ? result : LINE_DRAWING_FOLDER_PICKER_FAILED;
}

LineDrawingFolderPickerResult LineDrawing_PickerProcess_Run(const char* const argv[],
                                                           char* path, size_t capacity) {
    int pipes[2];
    if (pipe(pipes)) return LINE_DRAWING_FOLDER_PICKER_FAILED;
    pid_t child;
    int flags = fcntl(pipes[0], F_GETFL);
    int err = flags < 0 || fcntl(pipes[0], F_SETFL, flags|O_NONBLOCK) < 0 ||
              fcntl(pipes[0], F_SETFD, FD_CLOEXEC) < 0 || fcntl(pipes[1], F_SETFD, FD_CLOEXEC) < 0
            ? errno : launch(argv,pipes,&child);
    close(pipes[1]);
    LineDrawingFolderPickerResult result;
    if (err) result = err == ENOENT ? LINE_DRAWING_FOLDER_PICKER_UNAVAILABLE : LINE_DRAWING_FOLDER_PICKER_FAILED;
    else result = collect(child,pipes[0],path,capacity);
    close(pipes[0]);
    if (result != LINE_DRAWING_FOLDER_PICKER_SELECTED) path[0] = 0;
    return result;
}
#endif
