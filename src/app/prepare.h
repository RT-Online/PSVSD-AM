#ifndef PSVSD_AM_PREPARE_H
#define PSVSD_AM_PREPARE_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint64_t bytes_done,total_bytes;
    unsigned files_done,total_files,percent;
    int restoring;
    char phase[40],path[512];
} AmCopyProgress;
typedef void (*AmCopyCallback)(const AmCopyProgress *progress,void *ctx);
int am_prepare_original_apps(AmCopyCallback progress,void *ctx,char *message,size_t cap);
int am_prepare_usb_apps(AmCopyCallback progress,void *ctx,char *message,size_t cap);
#endif
