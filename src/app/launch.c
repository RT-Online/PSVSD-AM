/* Start the target process, rather than merely opening its LiveArea gate. */
#include "update_install.h"
#include <stdio.h>
#ifdef PSVSD_AM_HOST_TEST
#include "mock_update_install.h"
#else
#include <psp2/appmgr.h>
#endif
int am_update_launch_installer(void) {
    return sceAppMgrLaunchAppByUri(0x20000,"psgm:play?titleid=RTPSVUPD1");
}
int am_update_launch_app(void) {
    /* LIVEAREA_OFF (0x10000) plus the SDK's usual 0x60000 launch flags. */
    FILE *f=fopen(AM_UPDATE_ROOT "/handoff.log","a");
    if (f) { fputs("Request main process start, flags=00070000\n",f);fclose(f); }
    int ret=sceAppMgrLaunchAppByName(0x70000,"RTPSV0001",NULL);
    f=fopen(AM_UPDATE_ROOT "/handoff.log","a");
    if (f) { fprintf(f,"Title-ID launch flags=00070000: %08X\n",(unsigned)ret);fclose(f); }
    if (ret<0) {
        ret=sceAppMgrLaunchAppByUri(0x20000,"psgm:play?titleid=RTPSV0001");
        f=fopen(AM_UPDATE_ROOT "/handoff.log","a");
        if (f) { fprintf(f,"URI fallback: %08X\n",(unsigned)ret);fclose(f); }
    }
    return ret;
}
