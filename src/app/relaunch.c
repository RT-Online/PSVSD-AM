/* A launch request being accepted does not prove the target app started.
 * Match a per-attempt receipt before letting the helper finish. */
#include "update_install.h"
#include "../common/paths.h"
#include <stdio.h>
#include <string.h>
#define REQUEST AM_UPDATE_ROOT "/launch-request"
#define ACK AM_UPDATE_ROOT "/launch-ack"
static int request(uint32_t *token) {
    char text[10]={0};FILE *f=fopen(REQUEST,"rb");if (!f) return 0;
    size_t n=fread(text,1,9,f);int ok=!ferror(f);if (fclose(f)) ok=0;
    unsigned value=0;char tail;
    if (!ok || n!=8 || sscanf(text,"%8X%c",&value,&tail)!=1 || !value) return -1;
    *token=value;return 1;
}
void am_update_relaunch_cancel(void) { remove(REQUEST);remove(ACK); }
int am_update_relaunch_start(uint32_t token) {
    if (!token) return -1;
    am_update_relaunch_cancel();
    FILE *f=fopen(REQUEST,"wb");if (!f) return -1;
    int ok=fprintf(f,"%08X",(unsigned)token)==8;if (fclose(f)) ok=0;
    if (!ok) { am_update_relaunch_cancel();return -1; }
    int ret=am_update_launch_app();
    if (ret<0) am_update_relaunch_cancel();
    return ret;
}
int am_update_relaunch_confirm(void) {
    uint32_t token;int ret=request(&token);if (ret<=0) return ret;
    FILE *f=fopen(ACK,"wb");if (!f) return -1;
    int ok=fprintf(f,"%08X " ACCESSORY_VERSION "\n",(unsigned)token)>0;
    if (fclose(f)) ok=0;
    return ok?1:-1;
}
int am_update_relaunch_check(uint32_t token) {
    char text[64]={0},version[32],tail;unsigned found;
    FILE *f=fopen(ACK,"rb");if (!f) return 0;
    size_t n=fread(text,1,sizeof(text)-1,f);int ok=!ferror(f);if (fclose(f)) ok=0;
    if (!ok || n==sizeof(text)-1 || sscanf(text,"%8X %31s %c",&found,version,&tail)!=2 || found!=token) return 0;
    return 1;
}
