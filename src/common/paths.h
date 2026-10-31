#ifndef ACCESSORY_PATHS_H
#define ACCESSORY_PATHS_H
#include <stdint.h>
#define ACCESSORY_VERSION "1.0.3"
#define ACCESSORY_DATA_DIR "ux0:data/PSVSD_Accessory"
#define ACCESSORY_USER_LOG ACCESSORY_DATA_DIR "/test.log"
#define ACCESSORY_STAGED_PROBE ACCESSORY_DATA_DIR "/usbmc_accessory_probe.skprx"
#define ACCESSORY_SOURCE_PROBE "app0:usbmc_accessory_probe.skprx"
#define ACCESSORY_KERNEL_DIR "ur0:data/PSVSD_Accessory"
#define ACCESSORY_KERNEL_LOG ACCESSORY_KERNEL_DIR "/kernel.log"
#define ACCESSORY_KERNEL_FALLBACK ACCESSORY_DATA_DIR "/kernel.log"
#define ACCESSORY_REQUEST_MAGIC 0x50535641u
#define AM_SETTINGS_PATH ACCESSORY_KERNEL_DIR "/settings.ini"
#define AM_APP_LOG ACCESSORY_KERNEL_DIR "/app.log"
#define AM_BOOT_PLUGIN "ur0:tai/psvsd_am.skprx"
#define AM_BOOT_SOURCE "app0:psvsd_am.skprx"
#define AM_TAI_CONFIG "ur0:tai/config.txt"
#define AM_KERNEL_NAME "PSVSDAMKernel"
#define AM_PROBE_NAME "PSVSDAMProbe"
#define AM_STATUS_SOURCE "app0:psvsd_am_status.skprx"
#define AM_STATUS_STAGED ACCESSORY_DATA_DIR "/psvsd_am_status.skprx"
typedef struct { uint32_t magic, request_id; } AccessoryRequest;
#endif
