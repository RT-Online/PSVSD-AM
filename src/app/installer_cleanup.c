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

static int cleanup_installer(int returning_from_update) {
    SceIoStat info;
    int present=sceIoGetstat(HELPER_EXECUTABLE,&info);
    if (present<0) return (unsigned)present==FILE_NOT_FOUND?0:present;
    if (returning_from_update) {
        int ready=sceIoGetstat(AM_UPDATE_ROOT "/ready",&info);
        if (ready>=0) return AM_INSTALLER_CLEANUP_DEFERRED;
        if ((unsigned)ready!=FILE_NOT_FOUND) return ready;
        /* AutoPlugin2 releases the previous app context before deleting
         * its installer. Do this only during a return from our installer. */
        int stopped=sceAppMgrDestroyOtherApp();
        if (stopped<0 && (unsigned)stopped!=AM_APPMGR_NOT_RUNNING) return stopped;
    } else if (wait_for_helper_exit()!=0) {
        int stopped=sceAppMgrDestroyAppByName(HELPER_TITLE);
        if (stopped<0 && (unsigned)stopped!=AM_APPMGR_NOT_RUNNING) return stopped;
        if (stopped>=0) {
            int running=wait_for_helper_exit();
            if (running!=0) return running<0?running:(int)SCE_APPMGR_ERROR_BUSY;
        }
    }
    int ret=am_package_remove_helper();
    if (ret<0) return ret;
    for (unsigned i=0;i<=CLEANUP_POLLS;++i) {
        present=sceIoGetstat(HELPER_EXECUTABLE,&info);
        if (present<0) {
            if ((unsigned)present!=FILE_NOT_FOUND) return present;
            if (returning_from_update) return AM_INSTALLER_REMOVED;
            remove(AM_UPDATE_ROOT "/result");
            am_update_relaunch_cancel();
            return 0;
        }
        if (i<CLEANUP_POLLS) sceKernelDelayThread(CLEANUP_POLL_US);
    }
    return (int)SCE_APPMGR_ERROR_BUSY;
}

int am_update_cleanup_installer(void) { return cleanup_installer(0); }
int am_update_cleanup_installer_on_return(void) { return cleanup_installer(1); }
