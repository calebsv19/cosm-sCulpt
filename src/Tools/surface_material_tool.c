#include "Layout/layout_json.h"
#include "Layout/layout_surface_material.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv) {
    if(argc!=5 || strcmp(argv[1],"apply")) {
        fprintf(stderr,"usage: surface_material_tool apply INPUT.layout.json ASSIGNMENTS.json NEW.layout.json\n");return 1;
    }
    Layout layout={0};Layout_Init(&layout,.05f);
    bool ok=Layout_LoadFromFile(&layout,argv[2]);
    FILE *input=ok ? fopen(argv[3],"rb") : NULL;
    char *text=NULL;cJSON *command=NULL;
    if(input) {
        if(!fseek(input,0,SEEK_END)) {
            long size=ftell(input);
            if(size>0 && size<=1024*1024 && !fseek(input,0,SEEK_SET)) {
                text=malloc((size_t)size+1);
                if(text && fread(text,1,(size_t)size,input)==(size_t)size) {text[size]=0;command=cJSON_Parse(text);}
            }
        }
        fclose(input);
    }
    const cJSON *schema=cJSON_GetObjectItemCaseSensitive(command,"schema");
    const cJSON *assignments=cJSON_GetObjectItemCaseSensitive(command,"assignments");
    ok=ok && cJSON_IsString(schema) && !strcmp(schema->valuestring,"line_drawing_surface_materials_v1") &&
        cJSON_IsArray(assignments) && cJSON_GetArraySize(assignments)>0;
    const cJSON *row=NULL;
    cJSON_ArrayForEach(row,assignments) {
        const cJSON *id=cJSON_GetObjectItemCaseSensitive(row,"object_id"),*preset=cJSON_GetObjectItemCaseSensitive(row,"preset");
        if(!ok || !cJSON_IsString(id) || !cJSON_IsString(preset)) {ok=false;break;}
        const cJSON *previous=NULL;
        cJSON_ArrayForEach(previous,assignments) {
            if(previous==row) break;
            const cJSON *v=cJSON_GetObjectItemCaseSensitive(previous,"object_id");
            if(cJSON_IsString(v) && !strcmp(v->valuestring,id->valuestring)) ok=false;
        }
        if(!ok || !Layout_SetSurfaceMaterial(&layout,id->valuestring,preset->valuestring,NULL,NULL)) {ok=false;break;}
    }
    char *saved=ok ? Layout_SaveToString(&layout) : NULL;
    FILE *output=saved ? fopen(argv[4],"wx") : NULL;
    ok=output && fputs(saved,output)>=0;
    if(output && fclose(output)) ok=false;
    if(output && !ok) (void)remove(argv[4]);
    Layout_FreeString(saved);free(text);cJSON_Delete(command);Layout_Free(&layout);
    if(!ok) fprintf(stderr,"Invalid assignment, unknown object/preset, full properties, or existing output; no candidate published.\n");
    return ok ? 0 : 1;
}
