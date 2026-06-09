#include "Tools/agent_scene_material_flow.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char* json_string_or(const cJSON* object, const char* key, const char* fallback) {
    const cJSON* item = object ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : fallback;
}

static cJSON* ensure_object_member(cJSON* object, const char* key) {
    cJSON* child = NULL;
    if (!object || !key || !key[0]) return NULL;
    child = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsObject(child)) return child;
    if (child) cJSON_DeleteItemFromObjectCaseSensitive(object, key);
    child = cJSON_CreateObject();
    if (!child) return NULL;
    cJSON_AddItemToObject(object, key, child);
    return child;
}

static cJSON* ensure_array_member(cJSON* object, const char* key) {
    cJSON* child = NULL;
    if (!object || !key || !key[0]) return NULL;
    child = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsArray(child)) return child;
    if (child) cJSON_DeleteItemFromObjectCaseSensitive(object, key);
    child = cJSON_CreateArray();
    if (!child) return NULL;
    cJSON_AddItemToObject(object, key, child);
    return child;
}

static const char* object_material_prompt(const cJSON* item) {
    const cJSON* ray = NULL;
    const char* prompt = json_string_or(item, "material_prompt", NULL);
    if (prompt && prompt[0]) return prompt;
    prompt = json_string_or(item, "material_description", NULL);
    if (prompt && prompt[0]) return prompt;
    ray = cJSON_GetObjectItemCaseSensitive(item, "ray_tracing");
    if (cJSON_IsObject(ray)) {
        prompt = json_string_or(ray, "material_prompt", NULL);
        if (prompt && prompt[0]) return prompt;
        prompt = json_string_or(ray, "material_description", NULL);
        if (prompt && prompt[0]) return prompt;
    }
    return NULL;
}

static const cJSON* object_explicit_stack(const cJSON* item) {
    const cJSON* ray = NULL;
    const cJSON* stack = cJSON_GetObjectItemCaseSensitive(item, "material_texture_stack");
    if (cJSON_IsObject(stack)) return stack;
    stack = cJSON_GetObjectItemCaseSensitive(item, "materialTextureStack");
    if (cJSON_IsObject(stack)) return stack;
    ray = cJSON_GetObjectItemCaseSensitive(item, "ray_tracing");
    if (cJSON_IsObject(ray)) {
        stack = cJSON_GetObjectItemCaseSensitive(ray, "material_texture_stack");
        if (cJSON_IsObject(stack)) return stack;
        stack = cJSON_GetObjectItemCaseSensitive(ray, "materialTextureStack");
        if (cJSON_IsObject(stack)) return stack;
    }
    return NULL;
}

static bool text_contains(const char* text, const char* needle) {
    size_t needle_len = needle ? strlen(needle) : 0u;
    if (!text || !needle || needle_len == 0u) return false;
    for (const char* p = text; *p; ++p) {
        size_t i = 0u;
        while (i < needle_len &&
               p[i] &&
               (char)tolower((unsigned char)p[i]) ==
                   (char)tolower((unsigned char)needle[i])) {
            ++i;
        }
        if (i == needle_len) return true;
    }
    return false;
}

static void add_layer_parameters(cJSON* layer,
                                 int pattern_mode,
                                 double coverage,
                                 double grain,
                                 double edge_softness,
                                 double contrast,
                                 double flow,
                                 double color_depth,
                                 double surface_damage,
                                 int seed) {
    cJSON* params = cJSON_CreateObject();
    if (!layer || !params) {
        cJSON_Delete(params);
        return;
    }
    cJSON_AddItemToObject(layer, "parameters", params);
    cJSON_AddNumberToObject(params, "pattern_mode", pattern_mode);
    cJSON_AddNumberToObject(params, "coverage", coverage);
    cJSON_AddNumberToObject(params, "grain", grain);
    cJSON_AddNumberToObject(params, "edge_softness", edge_softness);
    cJSON_AddNumberToObject(params, "contrast", contrast);
    cJSON_AddNumberToObject(params, "flow", flow);
    cJSON_AddNumberToObject(params, "color_depth", color_depth);
    cJSON_AddNumberToObject(params, "surface_damage", surface_damage);
    cJSON_AddNumberToObject(params, "seed", seed);
}

static cJSON* make_layer(const char* id,
                         const char* name,
                         const char* kind,
                         const char* blend,
                         double opacity,
                         double scale,
                         double strength) {
    cJSON* layer = cJSON_CreateObject();
    cJSON* placement = cJSON_CreateObject();
    if (!layer || !placement) {
        cJSON_Delete(layer);
        cJSON_Delete(placement);
        return NULL;
    }
    cJSON_AddStringToObject(layer, "id", id);
    cJSON_AddStringToObject(layer, "name", name);
    cJSON_AddStringToObject(layer, "kind", kind);
    cJSON_AddStringToObject(layer, "blend", blend);
    cJSON_AddBoolToObject(layer, "enabled", 1);
    cJSON_AddNumberToObject(layer, "opacity", opacity);
    cJSON_AddItemToObject(layer, "placement", placement);
    cJSON_AddNumberToObject(placement, "scale", scale);
    cJSON_AddNumberToObject(placement, "strength", strength);
    return layer;
}

static const char* prompt_base_kind(const char* prompt) {
    if (text_contains(prompt, "brushed metal") ||
        text_contains(prompt, "metal")) {
        return "brushed_metal";
    }
    if (text_contains(prompt, "wood")) return "wood";
    if (text_contains(prompt, "brick")) return "brick";
    if (text_contains(prompt, "concrete")) return "concrete";
    if (text_contains(prompt, "stone")) return "stone";
    return "solid";
}

static bool prompt_mentions_rough(const char* prompt) {
    return text_contains(prompt, "rough") ||
           text_contains(prompt, "scratched") ||
           text_contains(prompt, "scratches") ||
           text_contains(prompt, "abraded") ||
           text_contains(prompt, "worn");
}

static bool prompt_mentions_grime(const char* prompt) {
    return text_contains(prompt, "grime") ||
           text_contains(prompt, "dirty") ||
           text_contains(prompt, "dirt") ||
           text_contains(prompt, "soot");
}

static cJSON* build_material_stack_from_prompt(const char* prompt) {
    cJSON* stack = cJSON_CreateObject();
    cJSON* layers = cJSON_CreateArray();
    cJSON* base = NULL;
    if (!stack || !layers) goto fail;
    cJSON_AddItemToObject(stack, "layers", layers);

    base = make_layer("agent_base", "Agent Base", prompt_base_kind(prompt), "replace", 1.0, 1.0, 1.0);
    if (!base) goto fail;
    add_layer_parameters(base, 3, 0.62, 0.58, 0.48, 0.64, 0.34, 0.72, 0.28, 131);
    cJSON_AddItemToArray(layers, base);
    base = NULL;

    if (prompt_mentions_rough(prompt)) {
        cJSON* rough = make_layer("agent_rough_layer",
                                  "Rough Layer",
                                  "scratches",
                                  "overlay_damage",
                                  0.68,
                                  1.35,
                                  0.82);
        if (!rough) goto fail;
        add_layer_parameters(rough, 4, 0.64, 0.78, 0.36, 0.82, 0.18, 0.42, 0.58, 211);
        cJSON_AddNumberToObject(rough, "roughness_influence", 0.68);
        cJSON_AddNumberToObject(rough, "reflectivity_influence", -0.30);
        cJSON_AddNumberToObject(rough, "specular_influence", -0.24);
        cJSON_AddItemToArray(layers, rough);
    }

    if (prompt_mentions_grime(prompt)) {
        cJSON* grime = make_layer("agent_grime_overlay",
                                  "Grime Overlay",
                                  "grime",
                                  "multiply",
                                  0.72,
                                  1.18,
                                  0.76);
        if (!grime) goto fail;
        add_layer_parameters(grime, 3, 0.70, 0.62, 0.56, 0.78, 0.46, 0.50, 0.64, 307);
        cJSON_AddNumberToObject(grime, "roughness_influence", 0.70);
        cJSON_AddNumberToObject(grime, "reflectivity_influence", -0.48);
        cJSON_AddNumberToObject(grime, "specular_influence", -0.40);
        cJSON_AddItemToArray(layers, grime);
    }

    if (text_contains(prompt, "oil") || text_contains(prompt, "oily")) {
        cJSON* oil = make_layer("agent_oil_overlay",
                                "Oil Overlay",
                                "oil",
                                "transparent_specular",
                                0.44,
                                1.0,
                                0.62);
        if (!oil) goto fail;
        add_layer_parameters(oil, 3, 0.38, 0.20, 0.72, 0.34, 0.80, 0.24, 0.12, 401);
        cJSON_AddItemToArray(layers, oil);
    }

    if (text_contains(prompt, "rust") || text_contains(prompt, "rusted")) {
        cJSON* rust = make_layer("agent_rust_overlay",
                                 "Rust Overlay",
                                 "rust",
                                 "overlay_damage",
                                 0.58,
                                 1.0,
                                 0.74);
        if (!rust) goto fail;
        add_layer_parameters(rust, 4, 0.46, 0.78, 0.34, 0.74, 0.10, 0.76, 0.84, 509);
        cJSON_AddItemToArray(layers, rust);
    }

    return stack;

fail:
    cJSON_Delete(stack);
    cJSON_Delete(base);
    return NULL;
}

static cJSON* find_or_create_object_material(cJSON* object_materials, const char* object_id) {
    cJSON* entry = NULL;
    if (!object_materials || !object_id || !object_id[0]) return NULL;
    cJSON_ArrayForEach(entry, object_materials) {
        if (strcmp(json_string_or(entry, "object_id", ""), object_id) == 0) {
            return entry;
        }
    }
    entry = cJSON_CreateObject();
    if (!entry) return NULL;
    cJSON_AddStringToObject(entry, "object_id", object_id);
    cJSON_AddNumberToObject(entry, "material_id", 0);
    cJSON_AddItemToArray(object_materials, entry);
    return entry;
}

static void apply_prompt_surface_defaults(cJSON* entry, const char* prompt) {
    const char* base_kind = prompt_base_kind(prompt);
    if (!cJSON_GetObjectItemCaseSensitive(entry, "object_color")) {
        if (strcmp(base_kind, "brushed_metal") == 0) {
            cJSON_AddNumberToObject(entry, "object_color", 0xB8BCC0);
        } else if (strcmp(base_kind, "stone") == 0 || strcmp(base_kind, "concrete") == 0) {
            cJSON_AddNumberToObject(entry, "object_color", 0x8B8E88);
        } else {
            cJSON_AddNumberToObject(entry, "object_color", 0xC8C0B6);
        }
    }
    if (!cJSON_GetObjectItemCaseSensitive(entry, "reflectivity")) {
        cJSON_AddNumberToObject(entry, "reflectivity", strcmp(base_kind, "brushed_metal") == 0 ? 0.68 : 0.22);
    }
    if (!cJSON_GetObjectItemCaseSensitive(entry, "roughness")) {
        cJSON_AddNumberToObject(entry, "roughness", prompt_mentions_rough(prompt) ? 0.78 : 0.42);
    }
}

static bool apply_material_for_object(cJSON* object_materials, const cJSON* item) {
    const char* kind = json_string_or(item, "kind", "");
    const char* object_id = json_string_or(item, "id", NULL);
    const char* prompt = object_material_prompt(item);
    const cJSON* explicit_stack = object_explicit_stack(item);
    cJSON* entry = NULL;
    cJSON* stack = NULL;

    if (strcmp(kind, "mesh_asset_instance") != 0) return true;
    if (!object_id || !object_id[0] || (!prompt && !explicit_stack)) return true;

    entry = find_or_create_object_material(object_materials, object_id);
    if (!entry) return false;
    if (!cJSON_GetObjectItemCaseSensitive(entry, "material_id")) {
        cJSON_AddNumberToObject(entry, "material_id", 0);
    }

    if (explicit_stack) {
        stack = cJSON_Duplicate(explicit_stack, 1);
    } else {
        apply_prompt_surface_defaults(entry, prompt);
        stack = build_material_stack_from_prompt(prompt);
    }
    if (!stack) return false;
    cJSON_DeleteItemFromObjectCaseSensitive(entry, "material_texture_stack");
    cJSON_DeleteItemFromObjectCaseSensitive(entry, "materialTextureStack");
    cJSON_AddItemToObject(entry, "material_texture_stack", stack);
    return true;
}

bool AgentSceneMaterialFlow_ApplyObjectPrompts(cJSON* authoring, const cJSON* request) {
    cJSON* extensions = NULL;
    cJSON* ray_tracing = NULL;
    cJSON* ray_authoring = NULL;
    cJSON* object_materials = NULL;
    const cJSON* objects = NULL;
    const cJSON* item = NULL;

    if (!authoring || !request) return false;
    objects = cJSON_GetObjectItemCaseSensitive(request, "objects");
    if (!cJSON_IsArray(objects)) return true;

    extensions = ensure_object_member(authoring, "extensions");
    ray_tracing = ensure_object_member(extensions, "ray_tracing");
    ray_authoring = ensure_object_member(ray_tracing, "authoring");
    object_materials = ensure_array_member(ray_authoring, "object_materials");
    if (!extensions || !ray_tracing || !ray_authoring || !object_materials) return false;

    cJSON_ArrayForEach(item, objects) {
        if (!apply_material_for_object(object_materials, item)) return false;
    }
    return true;
}
