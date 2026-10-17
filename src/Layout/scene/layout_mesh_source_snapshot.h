#pragma once
#include <stdbool.h>
#include <stdatomic.h>
#include <sys/types.h>
#include <sys/stat.h>
/* Internal app-owned filesystem boundary. Child processes never access app state.
 * TMPDIR must name an existing absolute invocation-owned directory. No fallback
 * or directory creation occurs. On success the caller owns *output and must
 * unlink it and free the allocated path after the last reader has finished.
 * On failure *output is NULL and no snapshot file remains. */
bool Layout_MeshSourceSnapshot(const char *source, const struct stat *expected,
                               const atomic_bool *canceled, char **output);
bool Layout_MeshSourceWait(pid_t child, const atomic_bool *canceled, unsigned timeout_ms);
