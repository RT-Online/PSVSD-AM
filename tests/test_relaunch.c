#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../src/app/update_install.h"
#include "mock_update_install.h"

#define REQUEST AM_UPDATE_ROOT "/launch-request"
#define ACK AM_UPDATE_ROOT "/launch-ack"

static int64_t clock_tick=100000000;
static int compatible_results[2],direct_results[2],launch_calls,delay_calls;
static int direct_calls,compatible_calls;
static const char *expected_uri;

int64_t sceKernelGetSystemTimeWide(void) { return clock_tick; }
int sceKernelDelayThread(unsigned usec) {
    assert(usec==10000);
    ++delay_calls;
    clock_tick+=usec;
    return 0;
}
int sceAppMgrLaunchAppByUri(int flags,const char *uri) {
    assert(!strcmp(uri,expected_uri));
    assert(delay_calls==launch_calls+1);
    ++launch_calls;
    if (flags==0x20000) {
        assert(!strcmp(uri,"psgm:play?titleid=RTPSV0001"));
        return direct_results[direct_calls++%2];
    }
    assert(flags==0xFFFFF);
    return compatible_results[compatible_calls++%2];
}
static void put(const char *path,const char *text) {
    FILE *file=fopen(path,"wb");
    assert(file && fputs(text,file)>=0 && fclose(file)==0);
}
static int missing(const char *path) { return access(path,F_OK)!=0; }

int main(int argc,char **argv) {
    assert(argc==2 && chdir(argv[1])==0);
    assert(mkdir("ux0:data",0700)==0);
    assert(mkdir(ACCESSORY_DATA_DIR,0700)==0);
    assert(mkdir(AM_UPDATE_ROOT,0700)==0);

    expected_uri="psgm:play?titleid=RTPSVUPD1";
    assert(am_update_launch_installer()==0 && launch_calls==2);
    compatible_results[1]=-20;
    assert(am_update_launch_installer()==0 && launch_calls==4);
    compatible_results[0]=-10;
    assert(am_update_launch_installer()==-20 && launch_calls==6);

    expected_uri="psgm:play?titleid=RTPSV0001";
    direct_results[0]=-30;
    direct_results[1]=-40;
    assert(am_update_relaunch_start()==-20 && launch_calls==10 && missing(REQUEST));
    compatible_results[1]=0;
    assert(am_update_relaunch_start()==0 && launch_calls==14 && direct_calls==4 && compatible_calls==10);
    assert(am_update_relaunch_confirm()==1 && missing(REQUEST));
    direct_results[1]=0;
    put(ACK,"legacy receipt");
    assert(am_update_relaunch_start()==0 && launch_calls==16 && direct_calls==6 && compatible_calls==10 && missing(ACK));
    assert(am_update_relaunch_confirm()==1 && missing(REQUEST));
    assert(am_update_relaunch_confirm()==0);
    FILE *log=fopen(AM_UPDATE_ROOT "/handoff.log","rb");
    char text[2048]={0};
    assert(log && fread(text,1,sizeof(text)-1,log)>0 && fclose(log)==0);
    assert(strstr(text,"Startup receipt: elapsed_us=20000 version=" ACCESSORY_VERSION));

    put(REQUEST,"legacy 1.2.4 request data");
    assert(am_update_relaunch_confirm()<0 && missing(REQUEST));
    assert(am_update_relaunch_start()==0);
    clock_tick+=AM_RELAUNCH_WAIT_US;
    assert(am_update_relaunch_confirm()<0 && missing(REQUEST));
    assert(am_update_relaunch_start()==0);
    clock_tick-=1000000;
    assert(am_update_relaunch_confirm()<0 && missing(REQUEST));
    assert(am_update_relaunch_start()==0);
    am_update_relaunch_cancel();
    assert(am_update_relaunch_confirm()==0);
    assert(chdir("..")==0);
    int before=launch_calls;
    assert(am_update_relaunch_start()==0 && launch_calls==before+2 && missing(REQUEST));
    puts("Relaunch: direct play requests, fallback, boot-relative marker and diagnostic write failure passed");
    return 0;
}
