/* Record the installer-to-app transition using the boot-relative timer.
 * A diagnostic write failure must not block reopening the app. */
#include "update_install.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef PSVSD_AM_HOST_TEST
#include "mock_update_install.h"
#else
#include <psp2/kernel/threadmgr.h>
#endif

#define REQUEST AM_UPDATE_ROOT "/launch-request"
#define LEGACY_ACK AM_UPDATE_ROOT "/launch-ack"

void am_update_relaunch_cancel(void) {
    remove(REQUEST);
    remove(LEGACY_ACK);
}

int am_update_relaunch_start(void) {
    am_update_relaunch_cancel();
    int64_t started=sceKernelGetSystemTimeWide();
    FILE *file=started>=0?fopen(REQUEST,"wb"):NULL;
    if (file) {
        int written=fprintf(file,"%016llX",(unsigned long long)started);
        if (fclose(file)!=0 || written!=16) remove(REQUEST);
    }
    int result=am_update_launch_app();
    if (result<0) remove(REQUEST);
    return result;
}

int am_update_relaunch_confirm(void) {
    remove(LEGACY_ACK);
    FILE *file=fopen(REQUEST,"rb");
    if (!file) return 0;
    char text[17]={0};
    size_t length=fread(text,1,sizeof(text),file);
    int read_ok=!ferror(file);
    int closed=fclose(file);
    int consumed=remove(REQUEST);
    if (length!=16 || !read_ok || closed!=0 || consumed!=0 ||
        strspn(text,"0123456789ABCDEF")!=16) return -1;

    uint64_t started=strtoull(text,NULL,16);
    int64_t now=sceKernelGetSystemTimeWide();
    if (now<0 || (uint64_t)now<started || (uint64_t)now-started>AM_RELAUNCH_WAIT_US) return -1;

    file=fopen(AM_UPDATE_HANDOFF_LOG,"a");
    if (file) {
        fprintf(file,"Startup receipt: elapsed_us=%llu version=" ACCESSORY_VERSION "\n",
            (unsigned long long)((uint64_t)now-started));
        fclose(file);
    }
    return 1;
}
