#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "mock_cleanup.h"
#include "../src/app/update_install.h"
static int stat_result,stop_result,delete_result,stop_calls,delete_calls,cancels;
int sceIoGetstat(const char *p,SceIoStat *s) { (void)s;assert(!strcmp(p,"ux0:app/RTPSVUPD1/eboot.bin"));return stat_result; }
int sceAppMgrDestroyAppByName(const char *title) { assert(!strcmp(title,"RTPSVUPD1"));++stop_calls;return stop_result; }
int am_package_remove_helper(void) { assert(stop_calls>delete_calls);++delete_calls;return delete_result; }
void am_update_relaunch_cancel(void) { ++cancels; }
int main(void) {
 stat_result=(int)0x80010002;assert(am_update_cleanup_installer()==0 && !stop_calls);
 stat_result=-5;assert(am_update_cleanup_installer()==-5 && !stop_calls);
 stat_result=0;stop_result=-6;assert(am_update_cleanup_installer()==-6 && !delete_calls);
 stop_result=(int)0x80802012;delete_result=-7;assert(am_update_cleanup_installer()==-7 && !cancels);
 delete_result=0;assert(am_update_cleanup_installer()==0 && cancels==1);
 stop_result=0;assert(am_update_cleanup_installer()==0 && cancels==2);
 puts("Installer cleanup: absent, stat/stop/delete failures, stopped and already-exited helper passed");
 return 0;
}
