/* Public release checks and explicit downloads. GPL-3.0-or-later. */
#ifndef PSVSD_AM_UPDATES_H
#define PSVSD_AM_UPDATES_H
#include <stddef.h>
#define AM_RELEASE_API "https://api.github.com/repos/RT-Online/PSVSD-AM/releases/latest"
#define AM_RELEASE_DOWNLOAD_PREFIX "https://github.com/RT-Online/PSVSD-AM/releases/download/"
#define AM_DOWNLOAD_MAX_BYTES (64u*1024u*1024u)
#define AM_RELEASE_MAX_BYTES 65536u
#define AM_UPDATE_INTERVAL_US 60000000u
enum { UPDATE_UNCHECKED, UPDATE_CHECKING, UPDATE_OFFLINE, UPDATE_CURRENT,
       UPDATE_AVAILABLE, UPDATE_AHEAD, UPDATE_ERROR, UPDATE_INSTALLING, UPDATE_READY };
enum { AM_UPDATE_DOWNLOAD,AM_UPDATE_VERIFY,AM_UPDATE_EXTRACT,AM_UPDATE_PREPARE,
       AM_UPDATE_SYSTEM_INSTALL,AM_UPDATE_COMPLETE,AM_UPDATE_INSTALL_ERROR };
typedef struct { unsigned phase,done,total; } AmUpdateProgress;
typedef void (*AmUpdateProgressCallback)(void *context,unsigned phase,unsigned done,unsigned total);
typedef struct {
    int state,error,http_status,tls_verify;
    size_t received;
    char stage[48],reason[192];
    char asset_url[512],sha256[65];
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
int am_update_download(const AmUpdate *release,const char *path,AmUpdate *out,AmUpdateProgressCallback progress,void *context);
void am_update_diagnostic(const AmUpdate *result,char *text,size_t cap);
#endif
