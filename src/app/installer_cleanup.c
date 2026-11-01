/* Remove only the separately installed helper; embedded updater files stay. */
#include "update_install.h"
#ifdef PSVSD_AM_HOST_TEST
#include "mock_cleanup.h"
#else
#include <psp2/appmgr.h>
#include <psp2/io/stat.h>
#endif
#include <stdio.h>
int am_update_cleanup_installer(void) {
    SceIoStat info;
    int present=sceIoGetstat("ux0:app/RTPSVUPD1/eboot.bin",&info);
    if (present<0) return (unsigned)present==0x80010002u?0:present;
    int stopped=sceAppMgrDestroyAppByName("RTPSVUPD1");
    if (stopped<0 && (unsigned)stopped!=0x80802012u) return stopped;
    int ret=am_package_remove_helper();
    if (ret>=0) { remove(AM_UPDATE_ROOT "/result");am_update_relaunch_cancel(); }
    return ret;
}
