#include "Core/construction_reference_export.h"
#include "Core/global_state.h"
#include "Layout/layout_construction_reference.h"
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <SDL2/SDL.h>
#include <unistd.h>
static char last[4096];
const char* ConstructionReference_LastDirectory(void){return last;}
bool ConstructionReference_Export(const char* collection,char* message,size_t capacity) {
    GlobalState* g=Global_Get();const char* root=g->dataPaths.output_root;struct stat st;
    last[0]=0;if(stat(root,&st)||!S_ISDIR(st.st_mode)){snprintf(message,capacity,"Output root unavailable. Choose an existing output directory in File.");return false;}
    for(unsigned i=1;i<10000;++i){char path[4096];if(snprintf(path,sizeof(path),"%s/construction_reference_%04u",root,i)>=(int)sizeof(path))break;
        if(lstat(path,&st)==0)continue;if(errno!=ENOENT)break;
        if(!Layout_ExportConstructionReference(&g->layout,collection,g->currentConfigPath,path,message,capacity))return false;
        snprintf(last,sizeof(last),"%s",path);snprintf(message,capacity,"Exported construction_reference_%04u in File output root. Open sheets below.",i);return true;
    }
    snprintf(message,capacity,"Cannot allocate a fresh reference output directory.");return false;
}

bool ConstructionReference_OpenLast(void){
    if(!last[0])return false;char absolute[8192],cwd[4096];if(last[0]=='/')snprintf(absolute,sizeof(absolute),"%s/reference_sheets.html",last);
    else{if(!getcwd(cwd,sizeof(cwd)))return false;if(snprintf(absolute,sizeof(absolute),"%s/%s/reference_sheets.html",cwd,last)>=(int)sizeof(absolute))return false;}
    char uri[25000];size_t n=7;memcpy(uri,"file://",7);const char* hex="0123456789ABCDEF";
    for(const unsigned char* p=(const unsigned char*)absolute;*p;++p){if((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='/'||*p=='-'||*p=='_'||*p=='.')uri[n++]=(char)*p;else{uri[n++]='%';uri[n++]=hex[*p>>4];uri[n++]=hex[*p&15];}}uri[n]=0;return SDL_OpenURL(uri)==0;
}
