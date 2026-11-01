#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../src/app/update_install.h"
#include "../src/common/paths.h"
#include "mock_update_install.h"
static uint64_t clock_tick=100000000;
int sceRtcGetCurrentTick(SceRtcTick *tick) { tick->tick=clock_tick;return 0; }
static int direct_result,uri_result,direct_calls,uri_calls;
int sceAppMgrLaunchAppByName(int flags,const char *name,const char *param) {
    assert(!strcmp(name,"RTPSV0001") && !param && flags==0x70000);++direct_calls;return direct_result;
}
int sceAppMgrLaunchAppByUri(int flags,const char *uri) {
    assert(flags==0x20000);assert(!strcmp(uri,"psgm:play?titleid=RTPSVUPD1") || !strcmp(uri,"psgm:play?titleid=RTPSV0001"));
    ++uri_calls;return uri_result;
}
static void put(const char *path,const char *text) { FILE *f=fopen(path,"wb");assert(f);assert(fputs(text,f)>=0);assert(fclose(f)==0); }
int main(void) {
    char root[]="/tmp/am-relaunch-XXXXXX";assert(mkdtemp(root));assert(chdir(root)==0);
    assert(mkdir("ux0:data",0700)==0);assert(mkdir("ux0:data/PSVSD_AM",0700)==0);assert(mkdir(AM_UPDATE_ROOT,0700)==0);
    assert(am_update_launch_installer()==0 && uri_calls==1);
    assert(am_update_relaunch_confirm()==0);assert(am_update_relaunch_start(0)<0);
    assert(am_update_relaunch_start(123)==0 && direct_calls==1 && uri_calls==1);
    assert(am_update_relaunch_check(123)==0); /* Accepted is not acknowledged. */
    put(AM_UPDATE_ROOT "/launch-ack","0000007B");assert(am_update_relaunch_check(123)==0);
    put(AM_UPDATE_ROOT "/launch-ack","0000007A 1.1.6\n");assert(am_update_relaunch_check(123)==0);
    assert(am_update_relaunch_confirm()==1 && am_update_relaunch_check(123)==1);
    assert(am_update_relaunch_start(456)==0 && am_update_relaunch_check(123)==0 && am_update_relaunch_check(456)==0);
    assert(am_update_relaunch_confirm()==1 && am_update_relaunch_check(456)==1);
    am_update_relaunch_cancel();assert(am_update_relaunch_confirm()==0 && am_update_relaunch_check(456)==0);
    put(AM_UPDATE_ROOT "/launch-request","junk");assert(am_update_relaunch_confirm()<0);
    put(AM_UPDATE_ROOT "/launch-request","00000000");assert(am_update_relaunch_confirm()<0);
    direct_result=-10;uri_result=0;int before=uri_calls;
    assert(am_update_relaunch_start(789)==0 && uri_calls==before+1);
    assert(am_update_relaunch_check(789)==0); /* No repeated submission while waiting. */
    direct_result=-10;uri_result=-20;
    assert(am_update_relaunch_start(890)==-20);assert(am_update_relaunch_confirm()==0);
    assert(am_update_launch_installer()==-20);
    direct_result=uri_result=0;
    assert(am_update_relaunch_start(999)==0);clock_tick+=AM_RELAUNCH_WAIT_US+1;
    assert(am_update_relaunch_confirm()<0 && am_update_relaunch_check(999)==0);
    assert(am_update_relaunch_start(1000)==0);--clock_tick;
    assert(am_update_relaunch_confirm()<0);
    assert(am_update_relaunch_start(1001)==0);clock_tick+=AM_RELAUNCH_WAIT_US;
    assert(am_update_relaunch_confirm()==1 && am_update_relaunch_check(1001)==1);
    puts("Relaunch: direct launch, fallback, failure propagation, startup receipt, stale/partial receipt rejection and cleanup passed");return 0;
}
