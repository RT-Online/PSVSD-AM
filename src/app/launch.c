/* Play-URI handoff with the paired requests used by AutoPlugin2 2.20. */
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
    sceKernelDelayThread(10000);
    int first=sceAppMgrLaunchAppByUri(0xFFFFF,uri);
    sceKernelDelayThread(10000);
    int second=sceAppMgrLaunchAppByUri(0xFFFFF,uri);
    if (second>=0) return second;
    if (first>=0) return first;
    FILE *f=fopen(AM_UPDATE_HANDOFF_LOG,"a");
    if (f) { fprintf(f,"Play URI failed: %s %08X %08X\n",title,(unsigned)first,(unsigned)second);fclose(f); }
    return second;
}
int am_update_launch_installer(void) { return start_target(AM_INSTALLER_TITLE_ID); }
int am_update_launch_app(void) { return start_target(AM_APP_TITLE_ID); }
