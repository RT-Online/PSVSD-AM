/* One targeted cleanup attempt after a successful installer return. */
#include "update_install.h"
#ifdef PSVSD_AM_HOST_TEST
#include "mock_update_install.h"
#else
#include <psp2/appmgr.h>
#endif
#include <stdio.h>
#include <string.h>
int am_update_cleanup_return(void) {
    SceAppMgrAppStatus status;memset(&status,0,sizeof(status));
    SceAppMgrGetStatusByNameOpt opt={0};
    int ret=_sceAppMgrGetStatusByName("RTPSVUPD1",&status,&opt);
    if (ret>=0 && status.is_running) return 1; /* Never delete a live installer. */
    if (ret<0 && (unsigned)ret!=0x80802012u) return ret;
    ret=am_package_remove_helper();
    if (ret>=0) { remove(AM_UPDATE_ROOT "/result");am_update_relaunch_cancel(); }
    return ret;
}
