#include "Layout/scene/layout_mesh_prepared_store.h"
#include "core_pack.h"
#include "core_scene_compile.h"
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define STORE_VERSION 2u
const size_t Layout_MeshPreviewTriangleLimits[4]={256,2000,8000,18000};
#define SOURCE_LIMIT (8u * 1024u * 1024u)
#define FILE_LIMIT (8u * 1024u * 1024u)
#define STORE_LIMIT (256u * 1024u * 1024u)
/* ABI marker prevents reuse across scalar representation or byte order changes. */
typedef struct {
    uint32_t version, endian, vector_bytes;
    uint32_t budgets[4];
    uint64_t vertices[4], triangles[4], source_vertices[4], source_triangles[4];
    int32_t resolution[4];
    CoreObjectVec3 min[4], max[4];
    char digest[8][65];
} Header;
static const char* chunks[8]={"CVTX","CIDX","LVTX","LIDX","MVTX","MIDX","SVTX","SIDX"};
static bool valid(const CoreMeshPreviewLodMesh* m) {
    if(!m->vertices || !m->indices || !m->vertex_count || !m->triangle_count ||
       m->vertex_count>54000 || m->triangle_count>18000)return false;
    for(size_t i=0;i<m->vertex_count;++i)
        if(!isfinite(m->vertices[i].x) || !isfinite(m->vertices[i].y) || !isfinite(m->vertices[i].z))return false;
    for(size_t i=0;i<m->triangle_count*3;++i)if(m->indices[i]>=m->vertex_count)return false;
    return true;
}
bool Layout_MeshPreparedStoreKey(const char* source,char path[1024]) {
    struct stat stamp;
    if(stat(source,&stamp) || !S_ISREG(stamp.st_mode) || stamp.st_size<=0 || stamp.st_size>SOURCE_LIMIT)return false;
    FILE* f=fopen(source,"rb");if(!f)return false;
    size_t size=(size_t)stamp.st_size;char* data=malloc(size);char digest[65];
    bool ok=data && fread(data,1,size,f)==size && fgetc(f)==EOF;
    fclose(f);
    if(ok)ok=core_scene_compile_sha256(data,size,digest).code==CORE_OK;
    free(data);if(!ok)return false;
    const char* root=getenv("LINE_DRAWING_PREVIEW_STORE");
    if(!root || !*root) { (void)mkdir(".cache",0700);root=".cache/mesh_preview"; }
    if(strlen(root)>900 || (mkdir(root,0700) && stat(root,&stamp)))return false;
    return snprintf(path,1024,"%s/v%u-256-2k-8k-18k-%zu-%s.pack",root,STORE_VERSION,sizeof(CoreObjectVec3),digest)<1024;
}
static bool read_chunk(CorePackReader* r,int index,void* data,size_t size,const char digest[65]) {
    CorePackChunkInfo c;char actual[65];
    return core_pack_reader_find_chunk(r,chunks[index],0,&c).code==CORE_OK && c.size==size &&
        core_pack_reader_read_chunk_data(r,&c,data,size).code==CORE_OK &&
        core_scene_compile_sha256(data,size,actual).code==CORE_OK && !memcmp(actual,digest,65);
}
bool Layout_MeshPreparedStoreLoad(const char* path,CoreMeshPreviewLodMesh meshes[4]) {
    struct stat st;if(stat(path,&st) || st.st_size<=0 || st.st_size>FILE_LIMIT)return false;
    CorePackReader r;Header h={0};CorePackChunkInfo c;char expected[65],actual[65];
    if(core_pack_reader_open(path,&r).code!=CORE_OK)return false;
    bool ok=core_pack_reader_chunk_count(&r)==10 &&
        core_pack_reader_find_chunk(&r,"META",0,&c).code==CORE_OK && c.size==sizeof(h) &&
        core_pack_reader_read_chunk_data(&r,&c,&h,sizeof(h)).code==CORE_OK &&
        core_pack_reader_find_chunk(&r,"HASH",0,&c).code==CORE_OK && c.size==sizeof(expected) &&
        core_pack_reader_read_chunk_data(&r,&c,expected,sizeof(expected)).code==CORE_OK &&
        core_scene_compile_sha256(&h,sizeof(h),actual).code==CORE_OK && !memcmp(expected,actual,65) &&
        h.version==STORE_VERSION && h.endian==0x01020304 && h.vector_bytes==sizeof(CoreObjectVec3) &&
        h.budgets[0]==256 && h.budgets[1]==2000 && h.budgets[2]==8000 && h.budgets[3]==18000;
    for(int i=0;i<4 && ok;++i) {
        CoreMeshPreviewLodMesh* m=&meshes[i];
        ok=h.vertices[i]>0 && h.vertices[i]<=54000 && h.triangles[i]>0 && h.triangles[i]<=h.budgets[i];
        if(!ok)break;
        m->vertex_count=(size_t)h.vertices[i];m->triangle_count=(size_t)h.triangles[i];
        m->source_vertex_count=(size_t)h.source_vertices[i];m->source_triangle_count=(size_t)h.source_triangles[i];
        m->cluster_resolution=h.resolution[i];
        m->vertices=malloc(m->vertex_count*sizeof(*m->vertices));
        m->indices=malloc(m->triangle_count*3*sizeof(*m->indices));
        ok=m->vertices && m->indices && read_chunk(&r,i*2,m->vertices,m->vertex_count*sizeof(*m->vertices),h.digest[i*2]) &&
            read_chunk(&r,i*2+1,m->indices,m->triangle_count*3*sizeof(*m->indices),h.digest[i*2+1]) && valid(m);
    }
    core_pack_reader_close(&r);
    if(!ok)for(int i=0;i<4;++i)core_mesh_preview_lod_mesh_free(&meshes[i]);
    return ok;
}
static bool room(const char* path,size_t incoming) {
    char root[1024];snprintf(root,sizeof(root),"%s",path);char* slash=strrchr(root,'/');if(!slash)return false;*slash=0;
    DIR* d=opendir(root);if(!d)return false;
    size_t total=incoming;struct dirent* e;
    while((e=readdir(d)) && total<=STORE_LIMIT) {
        char file[1400];struct stat st;
        if(!strstr(e->d_name,".pack") || snprintf(file,sizeof(file),"%s/%s",root,e->d_name)>=(int)sizeof(file))continue;
        if(!stat(file,&st) && S_ISREG(st.st_mode) && st.st_size>0)total+=(size_t)st.st_size;
    }
    closedir(d);return total<=STORE_LIMIT;
}
bool Layout_MeshPreparedStoreSave(const char* path,const CoreMeshPreviewLodMesh meshes[4]) {
    for(int i=0;i<4;++i)if(!valid(&meshes[i]) || meshes[i].triangle_count>Layout_MeshPreviewTriangleLimits[i])return false;
    Header h={.version=STORE_VERSION,.endian=0x01020304,.vector_bytes=sizeof(CoreObjectVec3),.budgets={256,2000,8000,18000}};
    const void* data[8];size_t sizes[8];size_t total=sizeof(h)+4096;
    for(int i=0;i<4;++i) {
        const CoreMeshPreviewLodMesh* m=&meshes[i];
        h.vertices[i]=m->vertex_count;h.triangles[i]=m->triangle_count;
        h.source_vertices[i]=m->source_vertex_count;h.source_triangles[i]=m->source_triangle_count;h.resolution[i]=m->cluster_resolution;
        h.min[i]=h.max[i]=m->vertices[0];
        for(size_t j=1;j<m->vertex_count;++j) {
            CoreObjectVec3 v=m->vertices[j];
            h.min[i].x=fmin(h.min[i].x,v.x);h.min[i].y=fmin(h.min[i].y,v.y);h.min[i].z=fmin(h.min[i].z,v.z);
            h.max[i].x=fmax(h.max[i].x,v.x);h.max[i].y=fmax(h.max[i].y,v.y);h.max[i].z=fmax(h.max[i].z,v.z);
        }
        data[i*2]=m->vertices;sizes[i*2]=m->vertex_count*sizeof(*m->vertices);
        data[i*2+1]=m->indices;sizes[i*2+1]=m->triangle_count*3*sizeof(*m->indices);
        for(int j=i*2;j<i*2+2;++j) {total+=sizes[j];if(core_scene_compile_sha256(data[j],sizes[j],h.digest[j]).code!=CORE_OK)return false;}
    }
    char header_digest[65];
    if(core_scene_compile_sha256(&h,sizeof(h),header_digest).code!=CORE_OK || total>FILE_LIMIT || !room(path,total))return false;
    char tmp[1100];snprintf(tmp,sizeof(tmp),"%s.XXXXXX",path);int fd=mkstemp(tmp);if(fd<0)return false;close(fd);
    CorePackWriter writer;bool ok=core_pack_writer_open(tmp,&writer).code==CORE_OK;
    if(ok) {
        ok=core_pack_writer_add_chunk(&writer,"META",&h,sizeof(h)).code==CORE_OK;
        if(ok)ok=core_pack_writer_add_chunk(&writer,"HASH",header_digest,sizeof(header_digest)).code==CORE_OK;
        for(int i=0;i<8 && ok;++i)ok=core_pack_writer_add_chunk(&writer,chunks[i],data[i],sizes[i]).code==CORE_OK;
        bool closed=core_pack_writer_close(&writer).code==CORE_OK;ok=ok && closed;
    }
    if(ok)ok=!rename(tmp,path);
    if(!ok)unlink(tmp);
    return ok;
}
