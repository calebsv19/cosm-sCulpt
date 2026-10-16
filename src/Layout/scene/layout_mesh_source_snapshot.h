#pragma once
#include <stdbool.h>
#include <stdatomic.h>
#include <sys/types.h>
#include <sys/stat.h>
/* Internal app-owned filesystem boundary. Child processes never access app state. */
bool Layout_MeshSourceSnapshot(const char *source, const struct stat *expected,
                               const atomic_bool *canceled, char output[128]);
bool Layout_MeshSourceWait(pid_t child, const atomic_bool *canceled, unsigned timeout_ms);
