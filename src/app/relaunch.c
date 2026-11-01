/* A launch request being accepted does not prove the target app started.
 * Match a per-attempt receipt before letting the helper finish. */
#include "update_install.h"
#include "../common/paths.h"
#include <stdio.h>
#include <string.h>
#ifdef PSVSD_AM_HOST_TEST
#include "mock_update_install.h"
#else
#include <psp2/rtc.h>
#endif
#define REQUEST AM_UPDATE_ROOT "/launch-request"
#define ACK AM_UPDATE_ROOT "/launch-ack"
static int request(uint32_t *token,uint64_t *started) {
    char text[64]={0};FILE *f=fopen(REQUEST,"rb");if (!f) return 0;
    size_t n=fread(text,1,sizeof(text)-1,f);int ok=!ferror(f);if (fclose(f)) ok=0;
    unsigned value=0;unsigned long long time=0;char tail;
    if (!ok || n!=25 || sscanf(text,"%8X %16llX%c",&value,&time,&tail)!=2 || !value) return -1;
    *token=value;*started=time;return 1;
}
void am_update_relaunch_cancel(void) { remove(REQUEST);remove(ACK); }
int am_update_relaunch_start(uint32_t token) {
    if (!token) return -1;
    am_update_relaunch_cancel();
    SceRtcTick tick;if (sceRtcGetCurrentTick(&tick)<0) return -1;
    FILE *f=fopen(REQUEST,"wb");if (!f) return -1;
    int ok=fprintf(f,"%08X %016llX",(unsigned)token,(unsigned long long)tick.tick)==25;if (fclose(f)) ok=0;
    if (!ok) { am_update_relaunch_cancel();return -1; }
    int ret=am_update_launch_app();
    if (ret<0) am_update_relaunch_cancel();
    return ret;
}
int am_update_relaunch_confirm(void) {
    uint32_t token;uint64_t started;int ret=request(&token,&started);if (ret<=0) return ret;
    SceRtcTick tick;
    if (sceRtcGetCurrentTick(&tick)<0 || tick.tick<started || tick.tick-started>AM_RELAUNCH_WAIT_US) {
        am_update_relaunch_cancel();return -1;
    }
    FILE *f=fopen(ACK,"wb");if (!f) return -1;
    int ok=fprintf(f,"%08X " ACCESSORY_VERSION " %llu\n",(unsigned)token,
        (unsigned long long)(tick.tick-started))>0;
    if (fclose(f)) ok=0;
    if (ok) {
        f=fopen(AM_UPDATE_ROOT "/handoff.log","a");
        if (f) { fprintf(f,"Startup receipt: token=%08X elapsed_us=%llu version=" ACCESSORY_VERSION "\n",
            (unsigned)token,(unsigned long long)(tick.tick-started));fclose(f); }
    }
    return ok?1:-1;
}
int am_update_relaunch_check(uint32_t token) {
    char text[96]={0},version[32],tail;unsigned found;unsigned long long elapsed;
    FILE *f=fopen(ACK,"rb");if (!f) return 0;
    size_t n=fread(text,1,sizeof(text)-1,f);int ok=!ferror(f);if (fclose(f)) ok=0;
    if (!ok || n==sizeof(text)-1 || sscanf(text,"%8X %31s %llu %c",&found,version,&elapsed,&tail)!=3 ||
        found!=token || elapsed>AM_RELAUNCH_WAIT_US) return 0;
    return 1;
}
