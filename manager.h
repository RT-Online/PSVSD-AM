#ifndef PSVSD_AM_MANAGER_H
#define PSVSD_AM_MANAGER_H
#include "../common/paths.h"
#include "../common/settings.h"
#include "prepare.h"
#include <stddef.h>
#include <stdint.h>
typedef struct {
    int ux0,uma0,raw_usb,gamecard,loaded,legacy_loaded;
    int actual_target,verified_mount;
    int module_id;
} AmStatus;
typedef struct {
    int result,cleanup_result,sync_result;
    uint32_t written,read,crc;
    char path[128];
} AmTestResult;
typedef void (*AmProgress)(unsigned percent,const char *stage,void *ctx);
typedef int (*AmUsbRequest)(void *ctx,char *message,size_t cap);
typedef struct { AmCopyCallback progress;AmUsbRequest request_usb;void *ctx; } AmSaveCallbacks;
int am_read_text(const char *path,char *out,size_t cap);
int am_read_log(const char *path,char *out,size_t cap);
int am_load_settings(AmSettings *s);
int am_save_settings_with_callbacks(const AmSettings *s,const AmSaveCallbacks *callbacks,char *message,size_t cap);
int am_tai_transform(const char *input,int enabled,char *out,size_t cap);
int am_replace_file(const char *path,const void *bytes,size_t size);
void am_status(AmStatus *s);
void am_expect_query(uint32_t token);
int am_stage_status(void);
int am_test_io(int target,AmTestResult *r,AmProgress progress,void *ctx);
int am_export_report(const char *extra,char *message,size_t cap);
int am_clear_logs(char *message,size_t cap);
void am_log(const char *fmt,...);
#endif
