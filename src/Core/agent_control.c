#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "Core/agent_control.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

enum { LIMIT=65536 };
static struct {
    int fd; SDL_Thread* thread; SDL_mutex* mutex; SDL_cond* ready;
    bool stop,pending; char input[LIMIT]; char* output;
    char socket_path[108],descriptor[160];
} server={.fd=-1};
static bool stopping(void) {
    SDL_LockMutex(server.mutex);bool stop=server.stop;SDL_UnlockMutex(server.mutex);return stop;
}
static int serve(void* unused) {
    (void)unused;
    while(!stopping()) {
        struct pollfd p={.fd=server.fd,.events=POLLIN};
        if(poll(&p,1,100)<=0)continue;
        int client=accept(server.fd,NULL,NULL);if(client<0)continue;
        (void)fcntl(client,F_SETFD,FD_CLOEXEC);
#ifdef SO_NOSIGPIPE
        int no_sigpipe=1;(void)setsockopt(client,SOL_SOCKET,SO_NOSIGPIPE,&no_sigpipe,sizeof(no_sigpipe));
#endif
        size_t used=0;bool complete=false;Uint32 start=SDL_GetTicks();
        while(!stopping()&&SDL_GetTicks()-start<5000&&used<LIMIT-1) {
            struct pollfd q={.fd=client,.events=POLLIN};if(poll(&q,1,100)<=0)continue;
            ssize_t n=recv(client,server.input+used,LIMIT-1-used,0);if(n<=0)break;
            used+=(size_t)n;server.input[used]=0;
            char* end=memchr(server.input,'\n',used);if(end){*end=0;complete=true;break;}
        }
        if(complete&&!stopping()) {
            SDL_LockMutex(server.mutex);server.pending=true;
            SDL_Event wake={.type=SDL_USEREVENT};(void)SDL_PushEvent(&wake);
            while(server.pending&&!server.stop)SDL_CondWaitTimeout(server.ready,server.mutex,100);
            char* output=server.output;server.output=NULL;SDL_UnlockMutex(server.mutex);
            if(output) {
                size_t sent=0,total=strlen(output);start=SDL_GetTicks();
                while(sent<total&&SDL_GetTicks()-start<5000) {
                    struct pollfd q={.fd=client,.events=POLLOUT};if(poll(&q,1,100)<=0)continue;
                    ssize_t n=send(client,output+sent,total-sent,0);if(n<=0)break;sent+=(size_t)n;
                }
                free(output);
            }
        }
        close(client);
    }
    return 0;
}
void SculptAgent_Start(void) {
    if(server.fd>=0)return;
    snprintf(server.socket_path,sizeof(server.socket_path),"/tmp/sculpt-agent-%u-%ld.sock",(unsigned)getuid(),(long)getpid());
    snprintf(server.descriptor,sizeof(server.descriptor),"/tmp/sculpt-agent-%u-%ld.json",(unsigned)getuid(),(long)getpid());
    server.mutex=SDL_CreateMutex();server.ready=SDL_CreateCond();
    if(!server.mutex||!server.ready){SculptAgent_Stop();return;}
    server.fd=socket(AF_UNIX,SOCK_STREAM,0);
    struct sockaddr_un address={0};address.sun_family=AF_UNIX;snprintf(address.sun_path,sizeof(address.sun_path),"%s",server.socket_path);
    if(server.fd<0||bind(server.fd,(struct sockaddr*)&address,sizeof(address))||chmod(server.socket_path,0600)||listen(server.fd,4)) {
        if(server.fd>=0)close(server.fd);server.fd=-1;SculptAgent_Stop();return;
    }
    (void)fcntl(server.fd,F_SETFD,FD_CLOEXEC);
    int descriptor=open(server.descriptor,O_WRONLY|O_CREAT|O_EXCL,0600);
    if(descriptor<0){close(server.fd);server.fd=-1;unlink(server.socket_path);SculptAgent_Stop();return;}
    char text[400];int count=snprintf(text,sizeof(text),"{\"schema\":\"sculpt_live_control_v1\",\"pid\":%ld,\"socket\":\"%s\"}\n",(long)getpid(),server.socket_path);
    bool ok=write(descriptor,text,(size_t)count)==count;close(descriptor);
    if(!ok){SculptAgent_Stop();return;}
    server.stop=false;server.thread=SDL_CreateThread(serve,"sculpt-control",NULL);
    if(!server.thread)SculptAgent_Stop();
}
bool SculptAgent_Poll(AppContext* app) {
    if(server.fd<0||!server.mutex)return false;
    SDL_LockMutex(server.mutex);bool work=server.pending;
    if(work){server.output=SculptAgent_Command(app,server.input);server.pending=false;SDL_CondBroadcast(server.ready);}
    SDL_UnlockMutex(server.mutex);return work;
}
void SculptAgent_Stop(void) {
    if(server.mutex){SDL_LockMutex(server.mutex);server.stop=true;if(server.ready)SDL_CondBroadcast(server.ready);SDL_UnlockMutex(server.mutex);}
    if(server.thread)SDL_WaitThread(server.thread,NULL);
    if(server.fd>=0){close(server.fd);unlink(server.socket_path);unlink(server.descriptor);}
    if(server.ready)SDL_DestroyCond(server.ready);
    if(server.mutex)SDL_DestroyMutex(server.mutex);
    free(server.output);memset(&server,0,sizeof(server));server.fd=-1;
}
