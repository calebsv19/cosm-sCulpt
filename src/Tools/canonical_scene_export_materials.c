#include "Tools/canonical_scene_export_materials.h"

#include <stdio.h>
#include <string.h>

static const size_t kMaxMaterialBindingOptions = 16u;

static bool material_binding_string_is_set(const char* value) {
    return value && value[0];
}

static bool material_binding_is_valid_token(const char* value) {
    if (!value || !value[0]) return false;
    for (const char* p = value; *p; ++p) {
        const char c = *p;
        const bool ok = (c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') ||
                        c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }
    return true;
}

static const char* material_binding_string_or(const char* value, const char* fallback) {
    return material_binding_string_is_set(value) ? value : fallback;
}

bool LineDrawingCanonicalScene_IsAllowedMaterialBindingTargetKind(const char* value) {
    return value &&
           (strcmp(value, "object") == 0 ||
            strcmp(value, "face") == 0 ||
            strcmp(value, "surface_group") == 0);
}

bool LineDrawingCanonicalScene_ValidateMaterialBindingOption(
    const LineDrawingSceneMaterialBindingOption* option) {
    const char* target_kind = NULL;
    if (!option) return false;
    target_kind = material_binding_string_or(option->target_kind, "object");
    if (!LineDrawingCanonicalScene_IsAllowedMaterialBindingTargetKind(target_kind)) return false;
    if (!material_binding_is_valid_token(option->object_id)) return false;
    if (option->binding_id && !material_binding_is_valid_token(option->binding_id)) return false;
    if (option->material_id && !material_binding_is_valid_token(option->material_id)) return false;
    if (option->binding_role && !material_binding_is_valid_token(option->binding_role)) return false;

    if (strcmp(target_kind, "face") == 0) {
        if (!material_binding_string_is_set(option->face_id) &&
            !material_binding_string_is_set(option->face_role)) {
            return false;
        }
        if (option->face_id && !material_binding_is_valid_token(option->face_id)) return false;
        if (option->face_role && !material_binding_is_valid_token(option->face_role)) return false;
    } else if (strcmp(target_kind, "surface_group") == 0) {
        if (!material_binding_is_valid_token(option->surface_group_id)) return false;
    }

    return true;
}

static bool append_material_binding_record(cJSON* material_bindings,
                                           const char* binding_id,
                                           const char* target_kind,
                                           const char* object_id,
                                           const char* face_id,
                                           const char* face_role,
                                           const char* surface_group_id,
                                           const char* material_id,
                                           const char* binding_role) {
    cJSON* binding = NULL;
    if (!material_bindings || !object_id || !object_id[0] ||
        !material_id || !material_id[0]) {
        return false;
    }

    binding = cJSON_CreateObject();
    if (!binding) return false;
    cJSON_AddItemToArray(material_bindings, binding);
    if (binding_id && binding_id[0]) {
        cJSON_AddStringToObject(binding, "binding_id", binding_id);
    }
    cJSON_AddStringToObject(binding, "target_kind", material_binding_string_or(target_kind, "object"));
    cJSON_AddStringToObject(binding, "object_id", object_id);
    if (face_id && face_id[0]) {
        cJSON_AddStringToObject(binding, "face_id", face_id);
    }
    if (face_role && face_role[0]) {
        cJSON_AddStringToObject(binding, "face_role", face_role);
    }
    if (surface_group_id && surface_group_id[0]) {
        cJSON_AddStringToObject(binding, "surface_group_id", surface_group_id);
    }
    cJSON_AddStringToObject(binding, "material_id", material_id);
    if (binding_role && binding_role[0]) {
        cJSON_AddStringToObject(binding, "binding_role", binding_role);
    }
    return true;
}

bool LineDrawingCanonicalScene_AppendDefaultObjectMaterialBinding(cJSON* material_bindings,
                                                                  const char* object_id,
                                                                  const char* material_id,
                                                                  const char* binding_role) {
    char binding_id[160];
    if (!object_id || !object_id[0] || !material_id || !material_id[0]) return false;
    snprintf(binding_id, sizeof(binding_id), "bind_%s_%s", object_id,
             binding_role && binding_role[0] ? binding_role : "default");
    return append_material_binding_record(material_bindings,
                                          binding_id,
                                          "object",
                                          object_id,
                                          NULL,
                                          NULL,
                                          NULL,
                                          material_id,
                                          material_binding_string_or(binding_role, "default"));
}

bool LineDrawingCanonicalScene_AppendMaterialBindingOptions(
    cJSON* material_bindings,
    const LineDrawingSceneMaterialBindingOption* options,
    size_t option_count,
    const char* default_material_id) {
    if (!material_bindings) return false;
    if (option_count == 0u) return true;
    if (!options || option_count > kMaxMaterialBindingOptions) return false;

    for (size_t i = 0u; i < option_count; ++i) {
        char fallback_binding_id[160];
        const LineDrawingSceneMaterialBindingOption* option = &options[i];
        const char* target_kind = material_binding_string_or(option->target_kind, "object");
        const char* material_id = material_binding_string_or(option->material_id,
                                                            default_material_id);
        if (!LineDrawingCanonicalScene_ValidateMaterialBindingOption(option)) return false;
        if (!material_binding_is_valid_token(material_id)) return false;
        snprintf(fallback_binding_id,
                 sizeof(fallback_binding_id),
                 "bind_%s_%zu",
                 option->object_id,
                 i);
        if (!append_material_binding_record(
                material_bindings,
                material_binding_string_or(option->binding_id, fallback_binding_id),
                target_kind,
                option->object_id,
                option->face_id,
                option->face_role,
                option->surface_group_id,
                material_id,
                option->binding_role)) {
            return false;
        }
    }

    return true;
}
