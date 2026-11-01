/* Both update handoffs request a process start, without a LiveArea fallback. */
#include "update_install.h"
#include <stdio.h>
#ifdef PSVSD_AM_HOST_TEST
#include "mock_update_install.h"
#else
#include <psp2/appmgr.h>
#endif
static int start_target(const char *title) {
    /* LIVEAREA_OFF (0x10000) plus the SDK's usual 0x60000 launch flags. */
    FILE *f=fopen(AM_UPDATE_ROOT "/handoff.log","a");
    if (f) { fprintf(f,"Request process start: %s flags=00070000\n",title);fclose(f); }
    int ret=sceAppMgrLaunchAppByName(0x70000,title,NULL);
    f=fopen(AM_UPDATE_ROOT "/handoff.log","a");
    if (f) { fprintf(f,"Process-start result: %s %08X\n",title,(unsigned)ret);fclose(f); }
    return ret;
}
int am_update_launch_installer(void) { return start_target("RTPSVUPD1"); }
int am_update_launch_app(void) { return start_target("RTPSV0001"); }
