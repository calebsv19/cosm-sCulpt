/*
 * core_scene_compile_bundle.c
 * Part of the CodeWork Shared Libraries
 */

#if defined(__linux__)
#define _GNU_SOURCE
#endif

#include "core_scene_compile.h"

#include "core_io.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/syscall.h>
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1u << 0)
#endif
#endif

static void write_diag(char *diagnostics, size_t size, const char *message) {
    if (diagnostics && size > 0u) snprintf(diagnostics, size, "%s", message ? message : "");
}

static bool build_path(const char *root, const char *leaf, char *out, size_t out_size) {
    int written = snprintf(out, out_size, "%s/%s", root, leaf);
    return written > 0 && (size_t)written < out_size;
}

static void cleanup_staging(const char *dir, const char *authoring, const char *runtime, const char *receipt) {
    if (receipt && receipt[0]) (void)unlink(receipt);
    if (runtime && runtime[0]) (void)unlink(runtime);
    if (authoring && authoring[0]) (void)unlink(authoring);
    if (dir && dir[0]) (void)rmdir(dir);
}

static bool sync_file(const char *path) {
    int fd = open(path, O_RDONLY);
    int result;
    if (fd < 0) return false;
    result = fsync(fd);
    (void)close(fd);
    return result == 0;
}

static bool sync_directory(const char *path) {
    int fd = open(path, O_RDONLY);
    int result;
    if (fd < 0) return false;
    result = fsync(fd);
    (void)close(fd);
    return result == 0;
}

static int publish_directory_create_only(const char *staging_dir, const char *final_dir) {
#if defined(__APPLE__)
    return renameatx_np(AT_FDCWD, staging_dir, AT_FDCWD, final_dir, RENAME_EXCL);
#elif defined(__linux__)
    return (int)syscall(SYS_renameat2,
                        AT_FDCWD,
                        staging_dir,
                        AT_FDCWD,
                        final_dir,
                        RENAME_NOREPLACE);
#else
    (void)staging_dir;
    (void)final_dir;
    errno = ENOTSUP;
    return -1;
#endif
}

CoreResult core_scene_compile_publish_bundle(const char *authoring_json,
                                             const CoreSceneCompileOptions *options,
                                             const char *final_scene_dir,
                                             CoreSceneCompileBundlePaths *out_paths,
                                             char *diagnostics,
                                             size_t diagnostics_size) {
    CoreSceneCompileProvenance provenance = {0};
    char staging_dir[1024] = {0};
    char authoring_path[1024] = {0};
    char runtime_path[1024] = {0};
    char receipt_path[1024] = {0};
    char bundle_input[512];
    char bundle_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char receipt[2048];
    char *runtime_json = NULL;
    struct stat st;
    CoreResult result;
    int written;
    long long published_at_ns;

    if (!authoring_json || !final_scene_dir || !final_scene_dir[0]) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }
    if (out_paths) memset(out_paths, 0, sizeof(*out_paths));
    write_diag(diagnostics, diagnostics_size, NULL);
    if (stat(final_scene_dir, &st) == 0 || errno != ENOENT) {
        write_diag(diagnostics, diagnostics_size, "scene bundle destination already exists");
        return (CoreResult){ CORE_ERR_IO, "bundle destination exists" };
    }
    written = snprintf(staging_dir, sizeof(staging_dir), "%s.staging-%ld", final_scene_dir, (long)getpid());
    if (written <= 0 || (size_t)written >= sizeof(staging_dir) || mkdir(staging_dir, 0755) != 0) {
        write_diag(diagnostics, diagnostics_size, "failed to create scene bundle staging directory");
        return (CoreResult){ CORE_ERR_IO, "failed to create staging directory" };
    }
    if (!build_path(staging_dir, "scene_authoring.json", authoring_path, sizeof(authoring_path)) ||
        !build_path(staging_dir, "scene_runtime.json", runtime_path, sizeof(runtime_path)) ||
        !build_path(staging_dir, "scene_export_receipt.json", receipt_path, sizeof(receipt_path))) {
        cleanup_staging(staging_dir, authoring_path, runtime_path, receipt_path);
        return (CoreResult){ CORE_ERR_INVALID_ARG, "bundle path too long" };
    }

    result = core_scene_compile_authoring_to_runtime_with_provenance(authoring_json,
                                                                     options,
                                                                     &runtime_json,
                                                                     &provenance,
                                                                     diagnostics,
                                                                     diagnostics_size);
    if (result.code != CORE_OK) {
        cleanup_staging(staging_dir, authoring_path, runtime_path, receipt_path);
        return result;
    }
    written = snprintf(bundle_input, sizeof(bundle_input),
                       "authoring:%s\nruntime:%s\ndependencies:%s\ncompiler:%s\nnormalization:%s\n",
                       provenance.authoring_sha256,
                       provenance.runtime_sha256,
                       provenance.dependency_sha256,
                       provenance.compiler_version,
                       provenance.normalization_version);
    if (written <= 0 || (size_t)written >= sizeof(bundle_input) ||
        core_scene_compile_sha256(bundle_input, (size_t)written, bundle_sha256).code != CORE_OK) {
        core_free(runtime_json);
        cleanup_staging(staging_dir, authoring_path, runtime_path, receipt_path);
        return (CoreResult){ CORE_ERR_FORMAT, "failed to calculate bundle digest" };
    }
    published_at_ns = (long long)time(NULL) * 1000000000LL;
    written = snprintf(receipt, sizeof(receipt),
        "{\n  \"schema_family\":\"codework_scene_export\",\n"
        "  \"schema_variant\":\"scene_export_receipt_v1\",\n  \"schema_version\":1,\n"
        "  \"publication\":{\"mode\":\"create_only_atomic_directory\",\"published_at_ns\":%lld},\n"
        "  \"compiler\":{\"name\":\"core_scene_compile\",\"version\":\"%s\",\"normalization\":\"%s\"},\n"
        "  \"authoring\":{\"path\":\"scene_authoring.json\",\"sha256\":\"%s\",\"bytes\":%zu},\n"
        "  \"runtime\":{\"path\":\"scene_runtime.json\",\"sha256\":\"%s\",\"bytes\":%zu},\n"
        "  \"dependencies\":{\"sha256\":\"%s\",\"count\":%zu},\n"
        "  \"bundle_sha256\":\"%s\"\n}\n",
        published_at_ns, provenance.compiler_version, provenance.normalization_version,
        provenance.authoring_sha256, strlen(authoring_json), provenance.runtime_sha256,
        strlen(runtime_json), provenance.dependency_sha256, provenance.dependency_count, bundle_sha256);
    if (written <= 0 || (size_t)written >= sizeof(receipt) ||
        core_io_write_all_atomic(authoring_path, authoring_json, strlen(authoring_json)).code != CORE_OK ||
        core_io_write_all_atomic(runtime_path, runtime_json, strlen(runtime_json)).code != CORE_OK ||
        core_io_write_all_atomic(receipt_path, receipt, (size_t)written).code != CORE_OK ||
        !sync_file(authoring_path) || !sync_file(runtime_path) || !sync_file(receipt_path) ||
        !sync_directory(staging_dir)) {
        core_free(runtime_json);
        cleanup_staging(staging_dir, authoring_path, runtime_path, receipt_path);
        write_diag(diagnostics, diagnostics_size, "failed to durably write scene bundle staging files");
        return (CoreResult){ CORE_ERR_IO, "failed to write bundle" };
    }
    core_free(runtime_json);
    if (publish_directory_create_only(staging_dir, final_scene_dir) != 0) {
        cleanup_staging(staging_dir, authoring_path, runtime_path, receipt_path);
        write_diag(diagnostics, diagnostics_size, "failed to atomically publish scene bundle");
        return (CoreResult){ CORE_ERR_IO, "failed to publish bundle" };
    }
    if (out_paths) {
        snprintf(out_paths->scene_dir, sizeof(out_paths->scene_dir), "%s", final_scene_dir);
        build_path(final_scene_dir, "scene_authoring.json", out_paths->authoring_path, sizeof(out_paths->authoring_path));
        build_path(final_scene_dir, "scene_runtime.json", out_paths->runtime_path, sizeof(out_paths->runtime_path));
        build_path(final_scene_dir, "scene_export_receipt.json", out_paths->receipt_path, sizeof(out_paths->receipt_path));
        memcpy(out_paths->bundle_sha256, bundle_sha256, sizeof(bundle_sha256));
    }
    return core_result_ok();
}
