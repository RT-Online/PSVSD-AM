/* Start the target process, rather than merely opening its LiveArea gate. */
#include "update_install.h"
#ifdef PSVSD_AM_HOST_TEST
#include "mock_update_install.h"
#else
#include <psp2/appmgr.h>
#endif
int am_update_launch_installer(void) {
    return sceAppMgrLaunchAppByUri(0x20000,"psgm:play?titleid=RTPSVUPD1");
}
int am_update_launch_app(void) {
    return sceAppMgrLaunchAppByUri(0x20000,"psgm:play?titleid=RTPSV0001");
}
