/*
 * core_scene_compile_dependencies.c
 * Canonical dependency-manifest construction and inspection.
 */

#include "core_scene_compile.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char k_manifest_prefix[] =
    "{\"schema_family\":\"codework_scene_dependencies\","
    "\"schema_variant\":\"scene_dependency_manifest_v1\","
    "\"schema_version\":1,\"dependencies\":[";
static const char k_manifest_suffix[] = "]}\n";

static void dependency_diag(char *diagnostics, size_t size, const char *message) {
    if (diagnostics && size > 0u) snprintf(diagnostics, size, "%s", message ? message : "");
}

static bool is_sha256_hex(const char *value) {
    if (!value || strlen(value) != 64u) return false;
    for (size_t i = 0u; i < 64u; ++i) {
        if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f'))) return false;
    }
    return true;
}

static bool is_manifest_token(const char *value) {
    size_t length;
    if (!value || !value[0]) return false;
    length = strlen(value);
    if (length > 255u) return false;
    for (size_t i = 0u; i < length; ++i) {
        const unsigned char ch = (unsigned char)value[i];
        if (ch < 0x20u || ch > 0x7eu || ch == '"' || ch == '\\') return false;
    }
    return true;
}

static int compare_dependencies(const void *lhs_ptr, const void *rhs_ptr) {
    const CoreSceneCompileDependency *lhs = (const CoreSceneCompileDependency *)lhs_ptr;
    const CoreSceneCompileDependency *rhs = (const CoreSceneCompileDependency *)rhs_ptr;
    int comparison = strcmp(lhs->kind, rhs->kind);
    return comparison != 0 ? comparison : strcmp(lhs->identity, rhs->identity);
}

CoreResult core_scene_compile_dependency_manifest_build(
    const CoreSceneCompileDependency *dependencies,
    size_t dependency_count,
    char **out_manifest_json,
    char out_digest_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE],
    char *diagnostics,
    size_t diagnostics_size) {
    CoreSceneCompileDependency *sorted = NULL;
    char *manifest = NULL;
    size_t capacity = sizeof(k_manifest_prefix) + sizeof(k_manifest_suffix);
    size_t offset = 0u;
    int written;

    if (!out_manifest_json || !out_digest_sha256 || (!dependencies && dependency_count > 0u)) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid dependency manifest arguments" };
    }
    *out_manifest_json = NULL;
    dependency_diag(diagnostics, diagnostics_size, NULL);
    if (dependency_count > 0u) {
        sorted = (CoreSceneCompileDependency *)core_alloc(dependency_count * sizeof(*sorted));
        if (!sorted) return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        memcpy(sorted, dependencies, dependency_count * sizeof(*sorted));
        for (size_t i = 0u; i < dependency_count; ++i) {
            if (!is_manifest_token(sorted[i].kind) || !is_manifest_token(sorted[i].identity) ||
                !is_sha256_hex(sorted[i].content_sha256)) {
                core_free(sorted);
                dependency_diag(diagnostics, diagnostics_size, "dependency entries require printable kind/identity and lowercase SHA-256");
                return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid dependency entry" };
            }
            capacity += strlen(sorted[i].kind) + strlen(sorted[i].identity) + 160u;
        }
        qsort(sorted, dependency_count, sizeof(*sorted), compare_dependencies);
        for (size_t i = 1u; i < dependency_count; ++i) {
            if (compare_dependencies(&sorted[i - 1u], &sorted[i]) == 0) {
                core_free(sorted);
                dependency_diag(diagnostics, diagnostics_size, "dependency kind and identity must be unique");
                return (CoreResult){ CORE_ERR_FORMAT, "duplicate dependency" };
            }
        }
    }
    manifest = (char *)core_alloc(capacity);
    if (!manifest) {
        core_free(sorted);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    memcpy(manifest, k_manifest_prefix, sizeof(k_manifest_prefix) - 1u);
    offset = sizeof(k_manifest_prefix) - 1u;
    for (size_t i = 0u; i < dependency_count; ++i) {
        written = snprintf(manifest + offset, capacity - offset,
                           "%s{\"kind\":\"%s\",\"identity\":\"%s\",\"sha256\":\"%s\",\"bytes\":%zu}",
                           i == 0u ? "" : ",", sorted[i].kind, sorted[i].identity,
                           sorted[i].content_sha256, sorted[i].content_bytes);
        if (written <= 0 || (size_t)written >= capacity - offset) {
            core_free(sorted);
            core_free(manifest);
            return (CoreResult){ CORE_ERR_FORMAT, "failed to render dependency manifest" };
        }
        offset += (size_t)written;
    }
    memcpy(manifest + offset, k_manifest_suffix, sizeof(k_manifest_suffix));
    offset += sizeof(k_manifest_suffix) - 1u;
    core_free(sorted);
    if (core_scene_compile_sha256(manifest, offset, out_digest_sha256).code != CORE_OK) {
        core_free(manifest);
        return (CoreResult){ CORE_ERR_FORMAT, "failed to digest dependency manifest" };
    }
    *out_manifest_json = manifest;
    return core_result_ok();
}

static const char *parse_token(const char *cursor, const char *prefix, char *out, size_t out_size) {
    const char *end;
    size_t length;
    if (strncmp(cursor, prefix, strlen(prefix)) != 0) return NULL;
    cursor += strlen(prefix);
    end = strchr(cursor, '"');
    if (!end) return NULL;
    length = (size_t)(end - cursor);
    if (length == 0u || length >= out_size) return NULL;
    memcpy(out, cursor, length);
    out[length] = '\0';
    return end + 1;
}

CoreResult core_scene_compile_dependency_manifest_inspect(
    const char *manifest_json,
    size_t *out_dependency_count,
    char out_digest_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE],
    char *diagnostics,
    size_t diagnostics_size) {
    const char *cursor;
    char previous_kind[256] = {0};
    char previous_identity[256] = {0};
    size_t count = 0u;
    if (!manifest_json || !out_dependency_count || !out_digest_sha256) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid dependency manifest arguments" };
    }
    dependency_diag(diagnostics, diagnostics_size, NULL);
    if (strncmp(manifest_json, k_manifest_prefix, sizeof(k_manifest_prefix) - 1u) != 0) goto invalid;
    cursor = manifest_json + sizeof(k_manifest_prefix) - 1u;
    while (strncmp(cursor, k_manifest_suffix, sizeof(k_manifest_suffix) - 1u) != 0) {
        char kind[256];
        char identity[256];
        char sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
        char *end = NULL;
        unsigned long long byte_count;
        if (count > 0u) {
            if (*cursor != ',') goto invalid;
            ++cursor;
        }
        cursor = parse_token(cursor, "{\"kind\":\"", kind, sizeof(kind));
        if (!cursor) goto invalid;
        cursor = parse_token(cursor, ",\"identity\":\"", identity, sizeof(identity));
        if (!cursor) goto invalid;
        cursor = parse_token(cursor, ",\"sha256\":\"", sha256, sizeof(sha256));
        if (!cursor || strncmp(cursor, ",\"bytes\":", 9u) != 0) goto invalid;
        cursor += 9u;
        if (!isdigit((unsigned char)*cursor)) goto invalid;
        byte_count = strtoull(cursor, &end, 10);
        (void)byte_count;
        if (!end || *end != '}') goto invalid;
        cursor = end + 1;
        if (!is_manifest_token(kind) || !is_manifest_token(identity) || !is_sha256_hex(sha256)) goto invalid;
        if (count > 0u) {
            int comparison = strcmp(previous_kind, kind);
            if (comparison > 0 || (comparison == 0 && strcmp(previous_identity, identity) >= 0)) goto invalid;
        }
        snprintf(previous_kind, sizeof(previous_kind), "%s", kind);
        snprintf(previous_identity, sizeof(previous_identity), "%s", identity);
        ++count;
    }
    if (cursor[sizeof(k_manifest_suffix) - 1u] != '\0') goto invalid;
    if (core_scene_compile_sha256(manifest_json, strlen(manifest_json), out_digest_sha256).code != CORE_OK) {
        return (CoreResult){ CORE_ERR_FORMAT, "failed to digest dependency manifest" };
    }
    *out_dependency_count = count;
    return core_result_ok();

invalid:
    dependency_diag(diagnostics, diagnostics_size, "dependency manifest is not canonical scene_dependency_manifest_v1 JSON");
    return (CoreResult){ CORE_ERR_FORMAT, "invalid dependency manifest" };
}
