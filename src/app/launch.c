/* Use the two play-URI requests observed in AutoPlugin2 2.20. */
#include "update_install.h"
#include <stdio.h>
#ifdef PSVSD_AM_HOST_TEST
#include "mock_update_install.h"
#else
#include <psp2/appmgr.h>
#include <psp2/kernel/threadmgr.h>
#endif

static int start_target(const char *title) {
    char uri[64];
    snprintf(uri,sizeof(uri),"psgm:play?titleid=%s",title);
    FILE *f=fopen(AM_UPDATE_ROOT "/handoff.log","a");
    if (f) { fprintf(f,"Request play URI: %s\n",title);fclose(f); }
    sceKernelDelayThread(10000);
    int first=sceAppMgrLaunchAppByUri(0xFFFFF,uri);
    sceKernelDelayThread(10000);
    int second=sceAppMgrLaunchAppByUri(0xFFFFF,uri);
    f=fopen(AM_UPDATE_ROOT "/handoff.log","a");
    if (f) { fprintf(f,"Play URI results: %s %08X %08X\n",title,(unsigned)first,(unsigned)second);fclose(f); }
    return second<0 && first>=0?first:second;
}
int am_update_launch_installer(void) { return start_target("RTPSVUPD1"); }
int am_update_launch_app(void) { return start_target("RTPSV0001"); }
