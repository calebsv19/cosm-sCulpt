#include "Core/agent_control.h"
#include "test_framework.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static unsigned executions;
char* SculptAgent_Command(AppContext* app, const char* request) {
    (void)app;
    executions++;
    if (!strcmp(request,"large")) {
        char* data = malloc(1024*1024);
        if (data) { memset(data,'x',1024*1024-1); data[1024*1024-1]=0; }
        return data;
    }
    return strdup("completed");
}

static int request(const char* body) {
    SculptAgent_Start();
    struct sockaddr_un addr = {.sun_family=AF_UNIX};
    snprintf(addr.sun_path,sizeof(addr.sun_path),"/tmp/sculpt-agent-%u-%ld.sock",
             (unsigned)getuid(),(long)getpid());
    int sock = socket(AF_UNIX,SOCK_STREAM,0);
    if (sock < 0) return -1;
    int small=4096; setsockopt(sock,SOL_SOCKET,SO_RCVBUF,&small,sizeof(small));
    if (connect(sock,(struct sockaddr*)&addr,sizeof(addr)) ||
        write(sock,body,strlen(body)) != (ssize_t)strlen(body)) { close(sock); return -1; }
    SDL_Event event;
    while (SDL_WaitEventTimeout(&event,2000)) if (event.type==SDL_USEREVENT) return sock;
    close(sock); return -1;
}

static bool pending(void) {
    int fd=request("pending\n"); TEST_ASSERT(fd>=0);
    SculptAgent_Stop();
    char buf[128]={0}; ssize_t n=read(fd,buf,sizeof(buf)-1); close(fd);
    TEST_ASSERT(n>0 && strstr(buf,"editor_stopping") && executions==0);
    return true;
}
static bool executed(void) {
    int fd=request("ordinary\n"); TEST_ASSERT(fd>=0);
    TEST_ASSERT(SculptAgent_Poll(NULL));
    SculptAgent_Stop();
    char buf[64]={0}; ssize_t n=read(fd,buf,sizeof(buf)-1); close(fd);
    TEST_ASSERT(n==9 && !strcmp(buf,"completed"));
    return true;
}
static bool blocked(void) {
    int fd=request("large\n"); TEST_ASSERT(fd>=0 && SculptAgent_Poll(NULL));
    char byte; TEST_ASSERT(read(fd,&byte,1)==1 && byte=='x');
    /* The client deliberately stops reading after actual reply transfer begins. */
    Uint64 before=SDL_GetTicks64();
    SculptAgent_Stop(); close(fd);
    TEST_ASSERT(SDL_GetTicks64()-before < 1500);
    return true;
}
static bool disconnected(void) {
    int fd=request("ordinary\n"); TEST_ASSERT(fd>=0);
    close(fd);
    TEST_ASSERT(SculptAgent_Poll(NULL));
    SculptAgent_Stop();
    return true;
}
int main(void) {
    alarm(12); /* A blocked server must fail the fixture instead of hanging its owner. */
    if (SDL_Init(SDL_INIT_TIMER|SDL_INIT_EVENTS)) return 2;
    const TestCase cases[]={{"pending",pending},{"executed",executed},{"backpressure",blocked},{"disconnected",disconnected},{"restart",executed}};
    bool ok=run_test_cases("LiveControlStop",cases,5);
    SculptAgent_Stop(); SDL_Quit(); return ok?0:1;
}
