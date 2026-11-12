#ifndef PSVSD_AM_MANAGER_H
#define PSVSD_AM_MANAGER_H
#include "../common/paths.h"
#include "../common/settings.h"
#include "prepare.h"
#include <stddef.h>
#include <stdint.h>
typedef struct {
    int ux0,uma0,raw_usb,gamecard,loaded,legacy_loaded;
    int actual_target,verified_mount,original_ready;
    int module_id;
} AmStatus;
typedef struct {
    int result,cleanup_result,sync_result;
    uint32_t written,read,crc;
    char path[128];
} AmTestResult;
enum { AM_SPEED_SEQ_WRITE,AM_SPEED_SEQ_READ,AM_SPEED_RANDOM_READ,AM_SPEED_RANDOM_WRITE,
       AM_SPEED_PHASE_COUNT,AM_SPEED_PREPARING=100 };
#define AM_SPEED_FILE_MIB 32u
#define AM_SPEED_PHASE_PERCENT (100u/AM_SPEED_PHASE_COUNT)
typedef struct { double mib_s,iops; } AmSpeedRate;
typedef struct {
    int result,cleanup_result,target;
    AmSpeedRate rates[AM_SPEED_PHASE_COUNT];
    char path[128];
} AmSpeedResult;
typedef void (*AmProgress)(unsigned percent,const char *stage,void *ctx);
typedef void (*AmSpeedProgress)(unsigned percent,void *ctx);
typedef int (*AmStorageRequest)(void *ctx,char *message,size_t cap);
typedef struct {
    AmCopyCallback progress;
    AmStorageRequest request_usb;
    void *ctx;
    AmStorageRequest request_original,before_copy;
} AmSaveCallbacks;
int am_read_text(const char *path,char *out,size_t cap);
int am_read_log(const char *path,char *out,size_t cap);
int am_load_settings(AmSettings *s);
int am_load_auto_update(void);
int am_save_auto_update(int enabled);
int am_save_settings_with_callbacks(const AmSettings *s,const AmSaveCallbacks *callbacks,char *message,size_t cap);
int am_tai_transform(const char *input,int enabled,char *out,size_t cap);
int am_replace_file(const char *path,const void *bytes,size_t size);
void am_status(AmStatus *s);
void am_expect_query(uint32_t token);
int am_stage_status(void);
int am_test_io(int target,AmTestResult *r,AmProgress progress,void *ctx);
int am_speed_test(int target,AmSpeedResult *r,AmSpeedProgress progress,void *ctx);
int am_export_report(const char *extra,char *message,size_t cap);
int am_clear_logs(char *message,size_t cap);
void am_log(const char *fmt,...);
#endif
