/* Remove only the separately installed helper; embedded updater files stay. */
#include "update_install.h"
#ifdef PSVSD_AM_HOST_TEST
#include "mock_cleanup.h"
#else
#include <psp2/appmgr.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#endif
#include <stdio.h>

#define HELPER_TITLE "RTPSVUPD1"
#define HELPER_EXECUTABLE "ux0:app/" HELPER_TITLE "/eboot.bin"
#define FILE_NOT_FOUND 0x80010002u
enum { CLEANUP_POLLS=25,CLEANUP_POLL_US=20000 };

static int helper_running(void) {
    SceUID pid=-1;
    int ret=sceAppMgrGetIdByName(&pid,HELPER_TITLE);
    if ((unsigned)ret==AM_APPMGR_NOT_RUNNING) return 0;
    return ret>=0 && pid>0?1:-1;
}

static int wait_for_helper_exit(void) {
    int running=helper_running();
    for (unsigned i=0;running>0 && i<CLEANUP_POLLS;++i) {
        sceKernelDelayThread(CLEANUP_POLL_US);
        running=helper_running();
    }
    return running;
}

int am_update_cleanup_installer(void) {
    SceIoStat info;
    int present=sceIoGetstat(HELPER_EXECUTABLE,&info);
    if (present<0) return (unsigned)present==FILE_NOT_FOUND?0:present;
    /* The helper normally exits after handing off to the updated app. */
    if (wait_for_helper_exit()!=0) {
        int stopped=sceAppMgrDestroyAppByName(HELPER_TITLE);
        if (stopped<0 && (unsigned)stopped!=AM_APPMGR_NOT_RUNNING) return stopped;
        if (stopped>=0 && wait_for_helper_exit()>0) return (int)SCE_APPMGR_ERROR_BUSY;
    }
    int ret=am_package_remove_helper();
    if (ret<0) return ret;
    for (unsigned i=0;i<=CLEANUP_POLLS;++i) {
        present=sceIoGetstat(HELPER_EXECUTABLE,&info);
        if (present<0) {
            if ((unsigned)present!=FILE_NOT_FOUND) return present;
            remove(AM_UPDATE_ROOT "/result");
            am_update_relaunch_cancel();
            return 0;
        }
        if (i<CLEANUP_POLLS) sceKernelDelayThread(CLEANUP_POLL_US);
    }
    return (int)SCE_APPMGR_ERROR_BUSY;
}
