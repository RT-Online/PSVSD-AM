/* Play-URI handoff with the paired requests used by AutoPlugin2 2.20. */
#include "update_install.h"
#include <stdio.h>
#ifdef PSVSD_AM_HOST_TEST
#include "mock_update_install.h"
#else
#include <psp2/appmgr.h>
#include <psp2/kernel/threadmgr.h>
#endif

enum { PLAY_DIRECT=0x20000, PLAY_COMPATIBLE=0xFFFFF };

static int start_target(const char *title,int flags) {
    char uri[64];
    snprintf(uri,sizeof(uri),"psgm:play?titleid=%s",title);
    sceKernelDelayThread(10000);
    int first=sceAppMgrLaunchAppByUri(flags,uri);
    sceKernelDelayThread(10000);
    int second=sceAppMgrLaunchAppByUri(flags,uri);
    FILE *f=fopen(AM_UPDATE_ROOT "/handoff.log","a");
    if (f) { fprintf(f,"Play URI results: %s flags=%08X %08X %08X\n",title,(unsigned)flags,(unsigned)first,(unsigned)second);fclose(f); }
    return second<0 && first>=0?first:second;
}
int am_update_launch_installer(void) { return start_target("RTPSVUPD1",PLAY_COMPATIBLE); }
int am_update_launch_app(void) {
    /* Launch the main title without an exit-to association with the helper. */
    int ret=start_target("RTPSV0001",PLAY_DIRECT);
    return ret>=0?ret:start_target("RTPSV0001",PLAY_COMPATIBLE);
}
