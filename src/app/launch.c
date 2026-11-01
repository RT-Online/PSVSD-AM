/* Both update handoffs request a process start, without a LiveArea fallback. */
#include "update_install.h"
#include <stdio.h>
#include <string.h>
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
int am_update_launch_app(void) {
    /* Identify the departing application explicitly. A generic launch request
     * queues a normal Shell app switch before ExitProcess can retire us. */
    SceAppMgrAppStatus status;memset(&status,0,sizeof(status));
    SceAppMgrGetStatusByNameOpt status_opt={0};
    int ret=_sceAppMgrGetStatusByName("RTPSVUPD1",&status,&status_opt);
    if (ret<0) return ret;
    if (!status.is_running || status.app_id<0) return -1;
    SceAppMgrLaunchAppByUri2Param param;memset(&param,0,sizeof(param));
    param.size=sizeof(param);
    param.flags=SCE_APPMGR_LAUNCH_FLAG_LIVEAREA_OFF|SCE_APPMGR_LAUNCH_FLAG_EXIT_TO;
    param.app_launch_callback_id=param.get_param_callback_id=-1;
    param.parent_app_id=status.app_id;
    SceAppMgrLaunchAppByUri2Opt opt={{0}};
    FILE *f=fopen(AM_UPDATE_ROOT "/handoff.log","a");
    if (f) { fprintf(f,"Request exit-to main: parent_app=%08X flags=%08X\n",(unsigned)status.app_id,(unsigned)param.flags);fclose(f); }
    ret=_sceAppMgrLaunchAppByUri2("psgm:play?titleid=RTPSV0001",&param,&opt);
    f=fopen(AM_UPDATE_ROOT "/handoff.log","a");
    if (f) { fprintf(f,"Exit-to main result: %08X\n",(unsigned)ret);fclose(f); }
    return ret;
}
