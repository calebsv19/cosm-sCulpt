/* Offline route authoring uses the same native candidate validation as the UI.
 * Outputs are create-only; inspect never mutates the source document. */
#include "Layout/layout_json.h"
#include "Layout/layout_routes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* read_text(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    char* text = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        long size = ftell(f);
        if (size >= 0 && size <= 16 * 1024 * 1024 && fseek(f, 0, SEEK_SET) == 0) {
            text = malloc((size_t)size + 1);
            if (text && fread(text, 1, (size_t)size, f) == (size_t)size) text[size] = 0;
            else { free(text); text = NULL; }
        }
    }
    fclose(f);
    return text;
}
static int error(const char* reason) {
    cJSON* root = cJSON_CreateObject();
    if (root) {
        cJSON_AddBoolToObject(root, "ok", false);
        cJSON_AddStringToObject(root, "error", reason);
        char* text = cJSON_PrintUnformatted(root);
        if (text) { fprintf(stderr, "%s\n", text); free(text); }
        cJSON_Delete(root);
    }
    return 1;
}
int main(int argc, char** argv) {
    bool inspect = argc == 3 && !strcmp(argv[1], "inspect");
    bool edit = argc == 5 && !strcmp(argv[1], "edit");
    if (!inspect && !edit) return error("usage: physical_route_tool inspect input.layout.json | edit input.layout.json command.json NEW.layout.json");
    Layout layout = {0};
    Layout_Init(&layout, .05f);
    char* input = read_text(argv[2]);
    bool loaded = input && Layout_LoadFromString(&layout, input);
    free(input);
    if (!loaded) { Layout_Free(&layout); return error("Invalid input layout; no output written."); }
    bool ok = true;
    if (edit) {
        char* text = read_text(argv[3]);
        cJSON* command = text ? cJSON_Parse(text) : NULL;
        free(text);
        const cJSON* schema = cJSON_GetObjectItemCaseSensitive(command, "schema");
        const cJSON* operation = cJSON_GetObjectItemCaseSensitive(command, "operation");
        const cJSON* id = cJSON_GetObjectItemCaseSensitive(command, "id");
        ok = cJSON_IsString(schema) && !strcmp(schema->valuestring, "line_drawing_route_edit_v1") && cJSON_IsString(operation);
        if (ok && !strcmp(operation->valuestring, "upsert")) {
            LayoutPhysicalRoute route;
            ok = Layout_RouteFromJson(cJSON_GetObjectItemCaseSensitive(command, "route"), &route) &&
                Layout_EditRoute(&layout, &route, NULL, NULL, NULL);
        } else if (ok && cJSON_IsString(id) && !strcmp(operation->valuestring, "remove"))
            ok = Layout_EditRoute(&layout, NULL, id->valuestring, NULL, NULL);
        else if (ok && cJSON_IsString(id) && !strcmp(operation->valuestring, "refresh"))
            ok = Layout_RefreshRouteEndpoints(&layout, id->valuestring, NULL, NULL);
        else ok = false;
        cJSON_Delete(command);
        if (!ok) {
            char reason[256];
            snprintf(reason, sizeof(reason), "%s", layout.geometryMessage[0] ? layout.geometryMessage : "Invalid route command; no output written.");
            Layout_Free(&layout); return error(reason);
        }
    }
    cJSON* report = Layout_RoutesReportJson(&layout);
    char* report_text = report ? cJSON_Print(report) : NULL;
    cJSON_Delete(report);
    if (!report_text) { Layout_Free(&layout); return error("Cannot serialize route report."); }
    if (edit) {
        char* text = Layout_SaveToString(&layout);
        FILE* output = text ? fopen(argv[4], "wx") : NULL;
        ok = output && fputs(text, output) >= 0;
        if (output && fclose(output) != 0) ok = false;
        if (output && !ok) (void)remove(argv[4]);
        Layout_FreeString(text);
    }
    Layout_Free(&layout);
    if (!ok) { free(report_text); return error("Output exists or cannot be written; existing files preserved."); }
    puts(report_text); free(report_text);
    return 0;
}
