/* On-demand public release checks. GPL-3.0-or-later. */
#ifndef PSVSD_AM_UPDATES_H
#define PSVSD_AM_UPDATES_H
#include <stddef.h>
#define AM_RELEASE_API "https://api.github.com/repos/RT-Online/PSVSD-AM/releases/latest"
#define AM_RELEASE_PAGE "https://github.com/RT-Online/PSVSD-AM/releases/latest"
#define AM_RELEASE_MAX_BYTES 65536u
#define AM_UPDATE_INTERVAL_US 60000000u
enum { UPDATE_UNCHECKED, UPDATE_CHECKING, UPDATE_OFFLINE, UPDATE_CURRENT,
       UPDATE_AVAILABLE, UPDATE_AHEAD, UPDATE_ERROR };
typedef struct {
    int state,error,http_status;
    char version[32],notes[4096],message[160];
} AmUpdate;
/* The transport is injectable so offline gating and response failures can be
 * tested without contacting GitHub or mocking storage code. */
typedef struct {
    void *ctx;
    int (*wifi)(void *ctx);
    int (*get)(void *ctx,char *body,size_t cap,size_t *used,int *http_status);
} AmUpdateTransport;
int am_update_parse(const char *json,size_t size,const char *current,AmUpdate *out);
void am_update_check_with_transport(AmUpdate *out,const char *current,const AmUpdateTransport *transport);
void am_update_check(AmUpdate *out);
#endif
