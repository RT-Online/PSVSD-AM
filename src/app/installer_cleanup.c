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
    if (ret<0) return ret;
    return pid>0?1:(int)SCE_APPMGR_ERROR_BUSY;
}

static int wait_for_helper_exit(void) {
    int running=helper_running();
    for (unsigned i=0;running>0 && i<CLEANUP_POLLS;++i) {
        sceKernelDelayThread(CLEANUP_POLL_US);
        running=helper_running();
    }
    return running;
}

static int cleanup_installer(int startup) {
    SceIoStat info;
    int present=sceIoGetstat(HELPER_EXECUTABLE,&info);
    if (present<0) return (unsigned)present==FILE_NOT_FOUND?0:present;
    if (startup) {
        int ready=sceIoGetstat(AM_UPDATE_ROOT "/ready",&info);
        if (ready>=0) return AM_INSTALLER_CLEANUP_DEFERRED;
        if ((unsigned)ready!=FILE_NOT_FOUND) return ready;
    }
    /* The helper normally exits after handing off to the updated app. */
    int running=wait_for_helper_exit();
    if (running!=0) {
        if (startup) return running>0?AM_INSTALLER_CLEANUP_DEFERRED:running;
        int stopped=sceAppMgrDestroyAppByName(HELPER_TITLE);
        if (stopped<0 && (unsigned)stopped!=AM_APPMGR_NOT_RUNNING) return stopped;
        if (stopped>=0 && wait_for_helper_exit()>0) return (int)SCE_APPMGR_ERROR_BUSY;
    }
    int ret=am_package_remove_helper(startup);
    if (ret<0) return ret;
    for (unsigned i=0;i<=CLEANUP_POLLS;++i) {
        present=sceIoGetstat(HELPER_EXECUTABLE,&info);
        if (present<0) {
            if ((unsigned)present!=FILE_NOT_FOUND) return present;
            remove(AM_UPDATE_ROOT "/result");
            am_update_relaunch_cancel();
            return startup?AM_INSTALLER_REMOVED:0;
        }
        if (i<CLEANUP_POLLS) sceKernelDelayThread(CLEANUP_POLL_US);
    }
    return (int)SCE_APPMGR_ERROR_BUSY;
}

int am_update_cleanup_installer(void) { return cleanup_installer(0); }
int am_update_cleanup_installer_on_launch(void) { return cleanup_installer(1); }
