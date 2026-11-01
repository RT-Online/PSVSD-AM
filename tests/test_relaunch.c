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
static int launch_result,launch_calls;
static const char *expected_title;
int sceRtcGetCurrentTick(SceRtcTick *tick) { tick->tick=clock_tick;return 0; }
int sceAppMgrLaunchAppByName(int flags,const char *name,const char *param) {
    assert(expected_title && !strcmp(name,expected_title) && !param && flags==0x70000);
    ++launch_calls;return launch_result;
}
static int status_result,status_running=1,delete_calls,delete_result;
int _sceAppMgrGetStatusByName(const char *name,SceAppMgrAppStatus *status,const SceAppMgrGetStatusByNameOpt *opt) {
    assert(!strcmp(name,"RTPSVUPD1") && opt);status->app_id=42;status->is_running=status_running;return status_result;
}
int _sceAppMgrLaunchAppByUri2(const char *uri,const SceAppMgrLaunchAppByUri2Param *p,const SceAppMgrLaunchAppByUri2Opt *opt) {
    assert(!strcmp(expected_title,"RTPSV0001") && !strcmp(uri,"psgm:play?titleid=RTPSV0001"));
    assert(p->size==sizeof(*p) && p->flags==0x50000 && p->parent_app_id==42);
    assert(p->app_launch_callback_id==-1 && p->get_param_callback_id==-1 && !p->launch_mode && opt);
    ++launch_calls;return launch_result;
}
int am_package_remove_helper(void) { ++delete_calls;return delete_result; }
static void put(const char *path,const char *text) { FILE *f=fopen(path,"wb");assert(f);assert(fputs(text,f)>=0);assert(fclose(f)==0); }
static void check_ack(const char *expected) {
    char text[96]={0};FILE *f=fopen(AM_UPDATE_ROOT "/launch-ack","rb");assert(f);
    size_t n=fread(text,1,sizeof(text)-1,f);assert(!ferror(f) && n==strlen(expected));fclose(f);assert(!strcmp(text,expected));
}
static int no_ack(void) { return access(AM_UPDATE_ROOT "/launch-ack",F_OK)<0; }
int main(void) {
    char root[]="/tmp/am-relaunch-XXXXXX";assert(mkdtemp(root));assert(chdir(root)==0);
    assert(mkdir("ux0:data",0700)==0);assert(mkdir("ux0:data/PSVSD_AM",0700)==0);assert(mkdir(AM_UPDATE_ROOT,0700)==0);
    expected_title="RTPSVUPD1";assert(am_update_launch_installer()==0 && launch_calls==1);
    launch_result=-20;assert(am_update_launch_installer()==-20 && launch_calls==2);
    expected_title="RTPSV0001";launch_result=0;
    assert(am_update_relaunch_confirm()==0);assert(am_update_relaunch_start(0)<0 && launch_calls==2);
    assert(am_update_relaunch_start(123)==0 && launch_calls==3 && no_ack());
    assert(am_update_relaunch_confirm()==1);check_ack("0000007B 1.2.1 0\n");
    assert(am_update_relaunch_start(456)==0 && no_ack());
    assert(am_update_relaunch_confirm()==1);check_ack("000001C8 1.2.1 0\n");
    am_update_relaunch_cancel();assert(am_update_relaunch_confirm()==0 && no_ack());
    put(AM_UPDATE_ROOT "/launch-request","junk");assert(am_update_relaunch_confirm()<0);
    put(AM_UPDATE_ROOT "/launch-request","00000000");assert(am_update_relaunch_confirm()<0);
    launch_result=-10;int before=launch_calls;
    assert(am_update_relaunch_start(789)==-10 && launch_calls==before+1);
    assert(am_update_relaunch_confirm()==0 && no_ack());
    launch_result=0;
    assert(am_update_relaunch_start(999)==0);clock_tick+=AM_RELAUNCH_WAIT_US+1;
    assert(am_update_relaunch_confirm()<0 && no_ack());
    assert(am_update_relaunch_start(1000)==0);--clock_tick;assert(am_update_relaunch_confirm()<0);
    assert(am_update_relaunch_start(1001)==0);clock_tick+=AM_RELAUNCH_WAIT_US;
    assert(am_update_relaunch_confirm()==1);check_ack("000003E9 1.2.1 300000000\n");
    assert(am_update_relaunch_confirm()==0); /* Receipt consumed once. */
    before=launch_calls;status_result=-40;assert(am_update_launch_app()==-40 && launch_calls==before);
    status_result=0;assert(am_update_cleanup_return()==1 && !delete_calls);
    status_running=0;delete_result=-50;assert(am_update_cleanup_return()==-50 && delete_calls==1);
    status_result=-40;assert(am_update_cleanup_return()==-40 && delete_calls==1);
    status_result=(int)0x80802012;delete_result=0;assert(am_update_cleanup_return()==0 && delete_calls==2);
    puts("Relaunch: outbound title launch, explicit exit-to arguments, negative results, one-shot receipts and guarded cleanup passed");return 0;
}
