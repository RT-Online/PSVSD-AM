#ifndef ACCESSORY_PATHS_H
#define ACCESSORY_PATHS_H
#include <stdint.h>
#define ACCESSORY_VERSION "1.3.1"
/* Monotonic Vita NN.NN revision, independent of semantic version digits. */
#define ACCESSORY_PACKAGE_VERSION "01.33"
#define AM_APP_TITLE_ID "RTPSV0001"
#define AM_INSTALLER_TITLE_ID "RTPSVUPD1"
#define ACCESSORY_DATA_DIR "ux0:data/PSVSD_AM"
#define AM_UPDATE_ROOT ACCESSORY_DATA_DIR "/update"
#define AM_UPDATE_VPK AM_UPDATE_ROOT "/release.vpk"
#define AM_UPDATE_STAGE AM_UPDATE_ROOT "/package"
#define AM_UPDATE_HELPER AM_UPDATE_ROOT "/helper"
#define AM_UPDATE_READY AM_UPDATE_ROOT "/ready"
#define AM_UPDATE_RESULT AM_UPDATE_ROOT "/result"
#define AM_UPDATE_HANDOFF_LOG AM_UPDATE_ROOT "/handoff.log"
#define AM_LEGACY_DATA_DIR "ux0:data/PSVSD_Accessory"
#define AM_LEGACY_KERNEL_DIR "ur0:data/PSVSD_Accessory"
#define AM_LEGACY_SETTINGS_PATH AM_LEGACY_KERNEL_DIR "/settings.ini"
#define ACCESSORY_USER_LOG ACCESSORY_DATA_DIR "/test.log"
#define ACCESSORY_STAGED_PROBE ACCESSORY_DATA_DIR "/usbmc_accessory_probe.skprx"
#define ACCESSORY_SOURCE_PROBE "app0:usbmc_accessory_probe.skprx"
#define ACCESSORY_KERNEL_DIR "ur0:data/PSVSD_AM"
#define ACCESSORY_KERNEL_LOG ACCESSORY_KERNEL_DIR "/kernel.log"
#define ACCESSORY_KERNEL_FALLBACK ACCESSORY_DATA_DIR "/kernel.log"
#define ACCESSORY_ORIGINAL_MAGIC 0x50534F52u
#define AM_ORIGINAL_EXPORT_NID 0x2A4C28E2u
#define ACCESSORY_REQUEST_MAGIC 0x50535641u
#define AM_SETTINGS_PATH ACCESSORY_KERNEL_DIR "/settings.ini"
#define AM_AUTO_UPDATE_PATH ACCESSORY_KERNEL_DIR "/auto-update"
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
