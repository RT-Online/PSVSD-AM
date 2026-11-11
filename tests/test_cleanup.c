#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "mock_cleanup.h"
#include "../src/app/update_install.h"
static struct {
    int installed,running,natural_exit,removal_delay,permanent_removal;
    int stat_error,ready,ready_error,lookup_error,stop_error,stop_ignored,delete_error;
    int lookups,stops,deletes,startup_deletes,delays,cancels;
} test;
static void reset(void) {
    memset(&test,0,sizeof(test));
    test.installed=1;
}
int sceIoGetstat(const char *p,SceIoStat *s) {
    (void)s;
    if (!strcmp(p,AM_UPDATE_ROOT "/ready")) {
        if (test.ready_error) return test.ready_error;
        return test.ready?0:(int)0x80010002u;
    }
    assert(!strcmp(p,"ux0:app/RTPSVUPD1/eboot.bin"));
    if (test.stat_error) return test.stat_error;
    if (test.deletes && test.installed && !test.permanent_removal) {
        if (test.removal_delay>0) --test.removal_delay;
        else test.installed=0;
    }
    return test.installed?0:(int)0x80010002u;
}
int sceAppMgrGetIdByName(SceUID *pid,const char *title) {
    assert(!strcmp(title,"RTPSVUPD1"));++test.lookups;
    if (test.lookup_error) return test.lookup_error;
    if (test.natural_exit && --test.natural_exit==0) test.running=0;
    if (!test.running) return (int)AM_APPMGR_NOT_RUNNING;
    *pid=1;return 0;
}
int sceAppMgrDestroyAppByName(const char *title) {
    assert(!strcmp(title,"RTPSVUPD1"));++test.stops;
    if (!test.stop_error && !test.stop_ignored) test.running=0;
    return test.stop_error;
}
int am_package_remove_helper(int retain_modules) {
    assert(!test.running || test.lookup_error);
    if (retain_modules) ++test.startup_deletes;
    ++test.deletes;return test.delete_error;
}
int sceKernelDelayThread(unsigned us) { assert(us==20000);++test.delays;return 0; }
void am_update_relaunch_cancel(void) { assert(!test.installed);++test.cancels; }
int main(void) {
    reset();test.installed=0;
    assert(am_update_cleanup_installer()==0 && !test.lookups && !test.deletes);
    reset();test.stat_error=-5;
    assert(am_update_cleanup_installer()==-5 && !test.lookups && !test.deletes);
    reset();assert(am_update_cleanup_installer()==0 && !test.stops && test.deletes==1 && test.cancels==1);
    assert(am_update_cleanup_installer()==0 && test.deletes==1);
    reset();test.running=1;test.natural_exit=3;test.removal_delay=2;
    assert(am_update_cleanup_installer()==0 && !test.stops && test.deletes==1 && test.delays==4);
    reset();test.running=1;
    assert(am_update_cleanup_installer()==0 && test.stops==1 && test.deletes==1 && test.delays==25);
    reset();test.running=1;test.stop_error=-6;
    assert(am_update_cleanup_installer()==-6 && test.stops==1 && !test.deletes);
    reset();test.lookup_error=-8;test.stop_error=(int)AM_APPMGR_NOT_RUNNING;
    assert(am_update_cleanup_installer()==0 && test.stops==1 && test.deletes==1 && !test.delays);
    reset();test.running=1;test.stop_ignored=1;
    assert(am_update_cleanup_installer()==(int)SCE_APPMGR_ERROR_BUSY && !test.deletes);
    reset();test.delete_error=-7;
    assert(am_update_cleanup_installer()==-7 && test.deletes==1 && !test.cancels);
    reset();test.permanent_removal=1;
    assert(am_update_cleanup_installer()==(int)SCE_APPMGR_ERROR_BUSY && test.deletes==1 && !test.cancels);

    reset();test.installed=0;
    assert(am_update_cleanup_installer_on_launch()==0 && !test.lookups && !test.deletes);
    reset();test.ready=1;
    assert(am_update_cleanup_installer_on_launch()==AM_INSTALLER_CLEANUP_DEFERRED && !test.lookups && !test.stops && !test.deletes);
    reset();test.ready_error=-9;
    assert(am_update_cleanup_installer_on_launch()==-9 && !test.lookups && !test.deletes);
    reset();test.running=1;
    assert(am_update_cleanup_installer_on_launch()==AM_INSTALLER_CLEANUP_DEFERRED && !test.stops && !test.deletes);
    reset();test.lookup_error=-8;
    assert(am_update_cleanup_installer_on_launch()==-8 && !test.stops && !test.deletes);
    reset();test.running=1;test.natural_exit=3;test.removal_delay=1;
    assert(am_update_cleanup_installer_on_launch()==AM_INSTALLER_REMOVED);
    assert(!test.stops && test.deletes==1 && test.startup_deletes==1 && test.delays==3 && test.cancels==1);
    assert(am_update_cleanup_installer_on_launch()==0 && test.deletes==1);
    reset();test.delete_error=-7;
    assert(am_update_cleanup_installer_on_launch()==-7 && test.startup_deletes==1 && !test.cancels);
    puts("Installer cleanup: startup guards, normal exit, fallback stop, removal confirmation and failures passed");
    return 0;
}
