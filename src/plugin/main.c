/*
 * PSVSD/AM 1.1.1 - PCH-1000 accessory-port storage.
 * Derived from USBMC / VitaShell, Copyright (C) 2015-2017 TheFloW.
 * USBMC integration by Yifan Lu; accessory adaptation for RefurbedTech, 2026.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is distributed WITHOUT ANY WARRANTY. See COPYING.
 */
#ifdef USBMC_HOST_TEST
#include "mock_vita.h"
#else
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/iofilemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/dirent.h>
#include <psp2kern/io/stat.h>
#include <taihen.h>
#endif
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../common/paths.h"
#include "../common/settings.h"

#ifndef USBMC_ACCESSORY_PROBE
#define USBMC_ACCESSORY_PROBE 0
#endif
#ifndef USBMC_MANAGED_BOOT
#define USBMC_MANAGED_BOOT 0
#endif
#ifndef USBMC_STATUS_ONLY
#define USBMC_STATUS_ONLY 0
#endif
#if (USBMC_ACCESSORY_PROBE + USBMC_MANAGED_BOOT + USBMC_STATUS_ONLY) != 1
#error "Select exactly one PSVSD/AM kernel role"
#endif

#define LOG_PATH ACCESSORY_KERNEL_LOG
#define RAW_USB "sdstor0:uma-lp-act-entire"
#define RAW_USB_PART "sdstor0:uma-pp-act-a"
#define UX0_MOUNT 0x800
#define UMA0_MOUNT 0xF00

/* Private mount structures and offsets are inherited from USBMC. */
typedef struct {
    const char *dev, *dev2, *blkdev, *blkdev2;
    int id;
} UsbmcIoDevice;
typedef struct {
    int id;
    const char *dev_unix;
    int unk, dev_major, dev_minor;
    const char *dev_filesystem;
    int unk2;
    UsbmcIoDevice *dev;
    int unk3;
    UsbmcIoDevice *dev2;
    int unk4, unk5, unk6, unk7;
} UsbmcMountPoint;

int module_get_export_func(SceUID, const char *, uint32_t, uint32_t, uintptr_t *);
int module_get_offset(SceUID, SceUID, int, size_t, uintptr_t *);

static SceUID log_fd = -1;
static SceUID appmgr_patches[2] = {-1, -1};
static UsbmcMountPoint *(*find_mount)(int) = NULL;
static UsbmcIoDevice usb_ux0 = {
    "ux0:", "exfatux0", "sdstor0:uma-pp-act-a", RAW_USB, UX0_MOUNT
};
static int redirected;
static UsbmcIoDevice usb_uma[2];
static int uma_owned;
static const char *active_log_path = LOG_PATH;
static SceUID resume_flag = -1, resume_thread = -1, resume_handler = -1;
static unsigned resume_enabled, suspended, resume_epoch, resume_queued = 1;
static unsigned resume_count;
static int configured_target=AM_UMA0;

static int active_mount_id(void) { return redirected ? UX0_MOUNT : UMA0_MOUNT; }
static const char *active_mount_path(void) { return redirected ? "ux0:" : "uma0:"; }

/* The callback only signals this worker. It performs no filesystem I/O and
 * does not sleep, activate USB or override the accessory supply. Event 0x100000
 * is the late resume stage used by YAMT/StorageMgr's USB remount support. */
static int resume_event(int resume, int event_id, void *param, void *arg) {
    (void)param; (void)arg;
    if (!__atomic_load_n(&resume_enabled, __ATOMIC_ACQUIRE)) return 0;
    if (!resume) {
        __atomic_store_n(&suspended, 1, __ATOMIC_RELEASE);
        __atomic_add_fetch(&resume_epoch, 1, __ATOMIC_ACQ_REL);
        __atomic_store_n(&resume_queued, 0, __ATOMIC_RELEASE);
    } else if (event_id == 0x100000) {
        __atomic_store_n(&suspended, 0, __ATOMIC_RELEASE);
        if (!__atomic_exchange_n(&resume_queued, 1, __ATOMIC_ACQ_REL))
            ksceKernelSetEventFlag(resume_flag, 1);
    }
    return 0;
}

static int resume_cancelled(unsigned epoch) {
    return __atomic_load_n(&suspended, __ATOMIC_ACQUIRE) ||
           __atomic_load_n(&resume_epoch, __ATOMIC_ACQUIRE) != epoch;
}

static int directory_exists(const char *path) {
    SceUID fd = ksceIoDopen(path);
    if (fd < 0) return 0;
    ksceIoDclose(fd);
    return 1;
}

static void log_line(const char *fmt, ...) {
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (len > 0 && log_fd >= 0) {
        if ((size_t)len >= sizeof(line)) len = sizeof(line) - 1;
        int off = 0;
        while (off < len) {
            int n = ksceIoWrite(log_fd, line + off, len - off);
            if (n <= 0) break;
            off += n;
        }
    }
}

static void close_log(void) {
    if (log_fd >= 0) {
        ksceIoClose(log_fd);
        log_fd = -1;
    }
}

static int exists(const char *path) {
    SceUID fd = ksceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) return 0;
    ksceIoClose(fd);
    return 1;
}

static int usb_exists(void) {
    return exists(RAW_USB_PART) || exists(RAW_USB);
}

static void log_usb_paths(const char *stage) {
    log_line("USB paths %s: partition=%s entire=%s\n", stage,
             exists(RAW_USB_PART) ? "FOUND" : "absent",
             exists(RAW_USB) ? "FOUND" : "absent");
}

static void log_sector(const char *path) {
    unsigned char b[512] __attribute__((aligned(64)));
    SceUID fd = ksceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) { log_line("Raw open %s: %08X\n", path, (unsigned)fd); return; }
    int n = ksceIoRead(fd, b, sizeof(b));
    ksceIoClose(fd);
    log_line("Raw read %s: %08X\n", path, (unsigned)n);
    if (n != (int)sizeof(b)) return;
    const char *kind = "unrecognized boot sector";
    if (!memcmp(b + 3, "EXFAT   ", 8)) kind = "exFAT volume boot sector";
    else if (!memcmp(b + 82, "FAT32   ", 8)) kind = "FAT32 volume boot sector";
    else if (!memcmp(b + 54, "FAT16   ", 8)) kind = "FAT16 volume boot sector";
    else if (!memcmp(b + 54, "FAT12   ", 8)) kind = "FAT12 volume boot sector";
    else if (b[510] == 0x55 && b[511] == 0xAA)
        kind = "MBR/other signed boot sector";
    log_line("Sector: %s; signature=%s\n", kind,
             b[510] == 0x55 && b[511] == 0xAA ? "55AA" : "absent");
    if (!strcmp(kind, "MBR/other signed boot sector"))
        log_line("MBR partition types: %02X %02X %02X %02X\n",
                 b[450], b[466], b[482], b[498]);
}

static int get_info(const char *name, tai_module_info_t *info) {
    memset(info, 0, sizeof(*info));
    info->size = sizeof(*info);
    return taiGetModuleInfoForKernel(KERNEL_PID, name, info);
}

static void release_appmgr_patches(void) {
    for (int i = 1; i >= 0; --i) {
        if (appmgr_patches[i] >= 0) {
            taiInjectReleaseForKernel(appmgr_patches[i]);
            appmgr_patches[i] = -1;
        }
    }
}

static int prepare_mount_lookup(void) {
    tai_module_info_t info;
    if (get_info("SceIofilemgr", &info) < 0) return -1;
    log_line("SceIofilemgr NID: %08X\n", (unsigned)info.module_nid);
    size_t offset;
    switch (info.module_nid) {
        case 0x9642948C: offset = 0x138C1; break; /* 3.60 */
        case 0xA96ACE9D: /* 3.65 */
        case 0x3347A95F: /* 3.67 */
        case 0x90DA33DE: offset = 0x182F5; break; /* 3.68 */
        default:
            log_line("Unsupported firmware/module; no patches applied.\n");
            return -1;
    }
    return module_get_offset(KERNEL_PID, info.modid, 0, offset,
                             (uintptr_t *)&find_mount);
}

static int patch_appmgr(void) {
    tai_module_info_t info;
    if (get_info("SceAppMgr", &info) < 0) return -1;
    log_line("SceAppMgr NID: %08X\n", (unsigned)info.module_nid);
    size_t first, second;
    switch (info.module_nid) {
        case 0xDBB29DB7: /* 3.60 */
        case 0x1C9879D6: first = 0xB338; second = 0xB368; break; /* 3.65 */
        case 0x54E2E984: /* 3.67 */
        case 0xC3C538DE: first = 0xB344; second = 0xB374; break; /* 3.68 */
        default: return -1;
    }
    const uint32_t nops = 0xBF00BF00;
    appmgr_patches[0] = taiInjectDataForKernel(KERNEL_PID, info.modid, 0,
                                               first, &nops, 4);
    if (appmgr_patches[0] < 0) return -1;
    appmgr_patches[1] = taiInjectDataForKernel(KERNEL_PID, info.modid, 0,
                                               second, &nops, 2);
    if (appmgr_patches[1] < 0) {
        release_appmgr_patches();
        return -1;
    }
    return 0;
}

static int resolve_bootfs(uint32_t func, uint32_t fallback, uintptr_t *out) {
    int ret = module_get_export_func(KERNEL_PID, "SceKernelModulemgr",
                                     0xC445FA63, func, out);
    if (ret < 0)
        ret = module_get_export_func(KERNEL_PID, "SceKernelModulemgr",
                                      0x92C9FFC2, fallback, out);
    return ret;
}

static int start_mass_driver(void) {
    tai_module_info_t existing;
    if (get_info("SceUsbMass", &existing) >= 0) {
        /* Do not inject into, stop or restart another plugin's live driver. */
        log_line("SceUsbMass already loaded; reusing existing driver.\n");
        return 0;
    }
    int (*mount_bootfs)(const char *) = NULL;
    int (*unmount_bootfs)(void) = NULL;
    SceUID modid = -1, patch1 = -1, patch2 = -1;
    if (resolve_bootfs(0x01360661, 0x185FF1BC,
                      (uintptr_t *)&mount_bootfs) >= 0 &&
        resolve_bootfs(0x9C838A6B, 0xBD61AD4D,
                      (uintptr_t *)&unmount_bootfs) >= 0 &&
        mount_bootfs("os0:kd/bootimage.skprx") >= 0) {
        modid = ksceKernelLoadModule("os0:kd/umass.skprx", 0x800, NULL);
        unmount_bootfs();
    }
    if (modid < 0) {
        log_line("Boot image unavailable; trying VitaShell's umass.\n");
        modid = ksceKernelLoadModule("ux0:VitaShell/module/umass.skprx", 0, NULL);
    }
    log_line("Load SceUsbMass: %08X\n", (unsigned)modid);
    if (modid < 0) return modid;

    /* USBMC's two retail-model checks. Offsets retained, not newly inferred. */
    /* taiHEN handles the firmware-specific Modulemgr ABI internally. A direct
     * SceModulemgrForKernel import would require a different library/NID on
     * 3.63+, preventing this module from starting before it can log anything. */
    uintptr_t segment_end;
    int ret = module_get_offset(KERNEL_PID, modid, 0, 0x1550, &segment_end);
    if (ret < 0) {
        log_line("Unexpected SceUsbMass text segment; refusing patch.\n");
        ksceKernelUnloadModule(modid, 0, NULL);
        return -1;
    }
    const unsigned char allow_check[] = {0x01, 0x20, 0x01, 0x20};
    patch1 = taiInjectDataForKernel(KERNEL_PID, modid, 0, 0x1546,
                                    allow_check, sizeof(allow_check));
    if (patch1 < 0) goto fail;
    patch2 = taiInjectDataForKernel(KERNEL_PID, modid, 0, 0x154C,
                                    allow_check, sizeof(allow_check));
    if (patch2 < 0) goto fail;
    ret = ksceKernelStartModule(modid, 0, NULL, 0, NULL, NULL);
    log_line("Start SceUsbMass: %08X\n", (unsigned)ret);
    taiInjectReleaseForKernel(patch2);
    taiInjectReleaseForKernel(patch1);
    if (ret < 0) ksceKernelUnloadModule(modid, 0, NULL);
    return ret;
fail:
    log_line("SceUsbMass patch failed: %08X / %08X\n",
             (unsigned)patch1, (unsigned)patch2);
    if (patch2 >= 0) taiInjectReleaseForKernel(patch2);
    if (patch1 >= 0) taiInjectReleaseForKernel(patch1);
    ksceKernelUnloadModule(modid, 0, NULL);
    return -1;
}

static void mount_secondary(void) {
    if (directory_exists("uma0:")) {
        log_line("uma0 already accessible; leaving it mounted.\n");
        return;
    }
    UsbmcMountPoint *mp = find_mount ? find_mount(UMA0_MOUNT) : NULL;
    if (!mp || !mp->dev || !mp->dev2) {
        log_line("uma0 mount descriptor unavailable; no mapping changed.\n");
        return;
    }
    UsbmcIoDevice *old1 = mp->dev, *old2 = mp->dev2;
    if (!old1->dev || !old2->dev || strcmp(old1->dev, "uma0:") ||
        strcmp(old2->dev, "uma0:")) {
        log_line("Unexpected uma0 descriptor aliases; refusing mapping change.\n");
        return;
    }
    log_line("Original uma0 paths: %.64s / %.64s\n",
             old1->blkdev ? old1->blkdev : "(null)",
             old1->blkdev2 ? old1->blkdev2 : "(null)");
    const char *primary = exists(RAW_USB_PART) ? RAW_USB_PART : RAW_USB;
    const char *fallback = exists(RAW_USB) ? RAW_USB : primary;
    usb_uma[0] = *old1;
    usb_uma[1] = *old2;
    for (int i = 0; i < 2; ++i) {
        usb_uma[i].blkdev = primary;
        usb_uma[i].blkdev2 = fallback;
    }
    mp->dev = &usb_uma[0];
    mp->dev2 = &usb_uma[1];
    log_line("Testing uma0 USB paths: %s / %s\n", primary, fallback);
    int ret = -1;
    for (int attempt = 0; attempt < 6; ++attempt) {
        ret = ksceIoMount(UMA0_MOUNT, NULL, 0, 0, 0, 0);
        if (ret != (int)0x80010013 || attempt == 5) break;
        ksceKernelDelayThread(200000);
    }
    log_line("Mount uma0: %08X\n", (unsigned)ret);
    if (ret < 0) {
        mp->dev = old1;
        mp->dev2 = old2;
        log_line("Mount failed; original uma0 mapping restored.\n");
    }
    if (ret >= 0 && directory_exists("uma0:")) uma_owned = 1;
    log_line("uma0 directory after mount: %s\n",
             directory_exists("uma0:") ? "accessible" : "not accessible");
}

static int usb_mapping_matches(int mount_id,const char *alias) {
    UsbmcMountPoint *mp = find_mount ? find_mount(mount_id) : NULL;
    if (!mp || !mp->dev || !mp->dev2) return 0;
    UsbmcIoDevice *devices[] = {mp->dev, mp->dev2};
    for (unsigned i = 0; i < 2; ++i) {
        UsbmcIoDevice *d = devices[i];
        if (!d->dev || strcmp(d->dev,alias) || !d->blkdev || !d->blkdev2)
            return 0;
        if ((strcmp(d->blkdev, RAW_USB_PART) && strcmp(d->blkdev, RAW_USB)) ||
            (strcmp(d->blkdev2, RAW_USB_PART) && strcmp(d->blkdev2, RAW_USB)))
            return 0;
    }
    /* The system may restore its equivalent USB descriptor during resume.
     * Accept the same backing paths; never reassign a different device. */
    return 1;
}
static int own_uma_mapping(void) {
    return (uma_owned || redirected) && usb_mapping_matches(active_mount_id(),active_mount_path());
}

static void recover_uma(unsigned epoch) {
    if (resume_cancelled(epoch)) return;
    if (!own_uma_mapping()) {
        ksceIoRemove(ACCESSORY_KERNEL_DIR "/runtime.txt");
        log_line("Resume: mount ownership changed; leaving mapping alone.\n");
        return;
    }
    /* Give the system's late-resume handlers time to finish. */
    ksceKernelDelayThread(50000);
    if (resume_cancelled(epoch)) return;
    int found = 0;
    for (int i = 0; i < 26; ++i) {
        if (resume_cancelled(epoch)) return;
        if (usb_exists()) { found = 1; break; }
        ksceKernelDelayThread(200000);
    }
    log_line("Resume USB raw device: %s\n", found ? "FOUND" : "NOT FOUND (5.2s timeout)");
    if (!found || resume_cancelled(epoch)) return;
    if (directory_exists(active_mount_path())) {
        log_line("Resume: %s already accessible; no mount needed.\n",active_mount_path());
        return;
    }
    int ret = -1;
    for (int i = 0; i < 6; ++i) {
        if (resume_cancelled(epoch)) return;
        if (!own_uma_mapping()) {
            ksceIoRemove(ACCESSORY_KERNEL_DIR "/runtime.txt");
            log_line("Resume: mount ownership changed during wait; leaving mapping alone.\n");
            return;
        }
        if (directory_exists(active_mount_path())) break;
        ret = ksceIoMount(active_mount_id(), NULL, 0, 0, 0, 0);
        log_line("Resume mount %s [%d]: %08X\n", active_mount_path(),i + 1, (unsigned)ret);
        if (ret != (int)0x80010013 || i == 5) break;
        ksceKernelDelayThread(200000);
    }
    if (!resume_cancelled(epoch))
        log_line("Resume %s directory: %s\n", active_mount_path(),
                 directory_exists(active_mount_path()) ? "accessible" : "not accessible");
}

static int resume_worker(SceSize argc, void *args) {
    (void)argc; (void)args;
    for (;;) {
        unsigned bits = 0;
        int ret = ksceKernelWaitEventFlag(resume_flag, 1,
                      SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, &bits, NULL);
        if (ret < 0) break;
        unsigned epoch = __atomic_load_n(&resume_epoch, __ATOMIC_ACQUIRE);
        if (resume_cancelled(epoch)) continue;
        log_fd = ksceIoOpen(active_log_path, SCE_O_WRONLY | SCE_O_APPEND, 0666);
        log_line("\nResume cycle %u (event 00100000)\n", ++resume_count);
        recover_uma(epoch);
        if (resume_cancelled(epoch)) log_line("Resume: cancelled by a newer suspend cycle.\n");
        close_log();
    }
    return ksceKernelExitDeleteThread(0);
}

static int prepare_resume(void) {
    if (!own_uma_mapping()) {
        log_line("Automatic resume: disabled (no owned USB mount).\n");
        return 0;
    }
    resume_flag = ksceKernelCreateEventFlag("PSVSDResume", 0, 0, NULL);
    if (resume_flag < 0) {
        log_line("Resume event creation failed: %08X\n", (unsigned)resume_flag);
        return 0;
    }
    resume_thread = ksceKernelCreateThread("PSVSDResume", resume_worker,
                                          0x40, 0x4000, 0, 0, NULL);
    if (resume_thread < 0) goto fail;
    resume_handler = ksceKernelRegisterSysEventHandler("PSVSDResume", resume_event, NULL);
    if (resume_handler < 0) goto fail;
    int ret = ksceKernelStartThread(resume_thread, 0, NULL);
    if (ret < 0) {
        log_line("Resume thread start failed: %08X\n", (unsigned)ret);
        goto fail;
    }
    log_line("Automatic %s resume: ARMED (handler %08X).\n",active_mount_path(),(unsigned)resume_handler);
    log_line("Resume uses existing USB recovery; accessory activation remains startup-only.\n");
    return 1;
fail:
    log_line("Resume setup failed: thread=%08X handler=%08X\n",
             (unsigned)resume_thread, (unsigned)resume_handler);
    if (resume_handler >= 0) ksceKernelUnregisterSysEventHandler(resume_handler);
    if (resume_thread >= 0) ksceKernelDeleteThread(resume_thread);
    ksceKernelDeleteEventFlag(resume_flag);
    resume_handler = resume_thread = resume_flag = -1;
    return 0;
}

static int redirect_ux0(void) {
    if (directory_exists("uma0:")) {
        log_line("Secondary volume already mounted; refusing duplicate USB primary mount.\n");
        return -1;
    }
    if (find_mount == NULL) return -1;
    UsbmcMountPoint *mp = find_mount(UX0_MOUNT);
    if (mp == NULL) return -1;
    if (patch_appmgr() < 0) {
        log_line("AppMgr patch unavailable; ux0 unchanged.\n");
        return -1;
    }
    UsbmcIoDevice *old_dev = mp->dev, *old_dev2 = mp->dev2;
    mp->dev = mp->dev2 = &usb_ux0;
    ksceIoUmount(UX0_MOUNT, 0, 0, 0);
    ksceIoUmount(UX0_MOUNT, 1, 0, 0);
    int ret = ksceIoMount(UX0_MOUNT, NULL, 0, 0, 0, 0);
    log_line("Redirect/mount ux0: %08X\n", (unsigned)ret);
    if (ret < 0) {
        mp->dev = old_dev;
        mp->dev2 = old_dev2;
        int restore = ksceIoMount(UX0_MOUNT, NULL, 0, 0, 0, 0);
        log_line("Restore original ux0 mount: %08X\n", (unsigned)restore);
        release_appmgr_patches();
        return ret;
    }
    redirected = 1;
    /* Keep config on ur0 only: do not reload plugins from the new card. */
    return 0;
}

#ifndef USBMC_HOST_TEST
int _start(SceSize, void *) __attribute__((weak, alias("module_start")));
#endif
static int initialize_runtime(SceSize argc, void *args) {
    uint32_t request_id=0;
    if (argc==sizeof(AccessoryRequest) && args &&
        ((AccessoryRequest *)args)->magic==ACCESSORY_REQUEST_MAGIC)
        request_id=((AccessoryRequest *)args)->request_id;
    ksceIoMkdir("ur0:data",0777);
    ksceIoMkdir(ACCESSORY_KERNEL_DIR,0777);
    ksceIoRemove(ACCESSORY_KERNEL_DIR "/runtime.txt");
    log_fd = ksceIoOpen(LOG_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    active_log_path = LOG_PATH;
    int primary_log_result=log_fd;
    if (log_fd<0) {
        ksceIoMkdir("ux0:data",0777);
        ksceIoMkdir(ACCESSORY_DATA_DIR,0777);
        log_fd=ksceIoOpen(ACCESSORY_KERNEL_FALLBACK,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
        active_log_path=ACCESSORY_KERNEL_FALLBACK;
    }
    log_line("PSVSD/AM " ACCESSORY_VERSION "\nMode: %s\n",
             USBMC_ACCESSORY_PROBE ? "PROBE - preserve ux0" : "BOOT - saved target");
    log_line("Request: %08X\nPrimary log open: %08X\n",(unsigned)request_id,(unsigned)primary_log_result);
    if (USBMC_MANAGED_BOOT) {
        char config[256];int fd=ksceIoOpen(AM_SETTINGS_PATH,SCE_O_RDONLY,0),n=-1;
        if (fd>=0) { n=ksceIoRead(fd,config,sizeof(config)-1);ksceIoClose(fd); }
        AmSettings settings={0,AM_UMA0};
        if (n<0) {
            log_line("Managed settings unavailable; automatic mount disabled.\n");
            close_log();return SCE_KERNEL_START_NO_RESIDENT;
        }
        config[n]=0;
        if (n>=(int)sizeof(config)-1 || am_settings_parse(config,&settings)<0 || !settings.enabled) {
            log_line("Managed settings disabled or invalid; no mount changes.\n");
            close_log();return SCE_KERNEL_START_NO_RESIDENT;
        }
        configured_target=settings.target;
        log_line("PSVSD/AM saved mount: %s\n",am_target_name(configured_target));
        if (configured_target==AM_UX0) {
            tai_module_info_t shell;
            if (get_info("SceShell",&shell)>=0) {
                log_line("Primary mounting is boot-only; refusing a live ux0 change.\n");
                close_log();return SCE_KERNEL_START_NO_RESIDENT;
            }
        }
    }
    log_usb_paths("before init");
    if (prepare_mount_lookup() < 0 || start_mass_driver() < 0) {
        log_line("Initialization failed; ux0 unchanged.\n");
        close_log();
        return SCE_KERNEL_START_NO_RESIDENT;
    }
    /* This replaces the original SceUsbServ fake-safe-mode / 3G hook.
     * No modem registry changes, Syscon-power override or guessed ID handling.
     * No polling worker that could re-enable the supply during standby. */
    log_usb_paths("before activation");
    int (*activate_accessory)(void) = NULL;
    int ret = module_get_export_func(KERNEL_PID,"SceUsbServ",TAI_ANY_LIBRARY,
                                     0xAA6D4409,(uintptr_t *)&activate_accessory);
    log_line("Accessory export lookup: %08X\n",(unsigned)ret);
    if (ret<0 || !activate_accessory) {
        log_line("Accessory export unavailable; no activation or mount attempted.\n");
        close_log();
        return SCE_KERNEL_START_NO_RESIDENT;
    }
    ret = activate_accessory();
    log_line("AccessoryActivate: %08X\n", (unsigned)ret);
    if (ret < 0) {
        log_line("Accessory activation failed; ux0 unchanged.\n");
        close_log();
        return SCE_KERNEL_START_NO_RESIDENT;
    }
    int found = 0;
    for (int i = 0; i < 26; ++i) {
        if (usb_exists()) { found = 1; break; }
        ksceKernelDelayThread(200000);
    }
    log_line("USB raw block device: %s\n", found ? "FOUND" : "NOT FOUND (5.2s timeout)");
    if (found) {
        log_usb_paths("after detection");
        log_sector(exists(RAW_USB_PART) ? RAW_USB_PART : RAW_USB);
        if (USBMC_ACCESSORY_PROBE || configured_target==AM_UMA0) {
            log_line("Keeping existing ux0.\n");
            mount_secondary();
        } else if (redirect_ux0() < 0) {
            log_line("No ux0 redirection; trying secondary storage.\n");
            mount_secondary();
        }
    }
    if (own_uma_mapping() && directory_exists(active_mount_path())) {
        tai_module_info_t self;
        const char *name=USBMC_MANAGED_BOOT ? AM_KERNEL_NAME : AM_PROBE_NAME;
        if (get_info(name,&self)>=0) {
            char status[128];int n=snprintf(status,sizeof(status),"PSVSD/AM " ACCESSORY_VERSION "\nKernel: %08X\nMount: %s\n",(unsigned)self.modid,active_mount_path());
            int fd=ksceIoOpen(ACCESSORY_KERNEL_DIR "/runtime.txt",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
            if (fd>=0) { ksceIoWrite(fd,status,n);ksceIoClose(fd); }
            log_line("PSVSD/AM runtime kernel: %08X / %s\n",(unsigned)self.modid,active_mount_path());
        }
    }
    int arm_resume = prepare_resume();
    log_line("Startup complete. Automatic resume requires console validation.\n");
    close_log();
    /* Prevent callback/worker logging from overlapping the startup report. */
    if (arm_resume) __atomic_store_n(&resume_enabled, 1, __ATOMIC_RELEASE);
    return SCE_KERNEL_START_SUCCESS;
}

static int secondary_boot_worker(SceSize argc, void *args) {
    (void)argc; (void)args;
    /* Secondary storage can wait for normal service startup without delaying
     * taiHEN's boot loader. No power overrides or periodic sleep polling. */
    for (int i=0;i<75;++i) {
        tai_module_info_t usb,io;
        if (get_info("SceUsbServ",&usb)>=0 && get_info("SceIofilemgr",&io)>=0) break;
        ksceKernelDelayThread(200000);
    }
    initialize_runtime(0,NULL);
    return ksceKernelExitDeleteThread(0);
}

int module_start(SceSize argc, void *args) {
    if (USBMC_STATUS_ONLY) {
        if (argc!=sizeof(AccessoryRequest) || !args || ((AccessoryRequest *)args)->magic!=ACCESSORY_REQUEST_MAGIC)
            return SCE_KERNEL_START_NO_RESIDENT;
        uint32_t token=((AccessoryRequest *)args)->request_id;
        const char *mount="none";
        if (prepare_mount_lookup()>=0 && usb_exists()) {
            int uma=usb_mapping_matches(UMA0_MOUNT,"uma0:") && directory_exists("uma0:");
            int ux=usb_mapping_matches(UX0_MOUNT,"ux0:") && directory_exists("ux0:");
            if (uma && !ux) mount="uma0:";
            if (ux && !uma) mount="ux0:";
        }
        char status[128];int n=snprintf(status,sizeof(status),"PSVSD/AM " ACCESSORY_VERSION "\nRequest: %08X\nMount: %s\n",token,mount);
        int fd=ksceIoOpen(ACCESSORY_KERNEL_DIR "/runtime.txt",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
        if (fd>=0) { ksceIoWrite(fd,status,n);ksceIoClose(fd); }
        return SCE_KERNEL_START_NO_RESIDENT;
    }
    if (USBMC_MANAGED_BOOT) {
        char text[256];AmSettings s={0,AM_UMA0};
        int fd=ksceIoOpen(AM_SETTINGS_PATH,SCE_O_RDONLY,0),n=-1;
        if (fd>=0) { n=ksceIoRead(fd,text,sizeof(text)-1);ksceIoClose(fd); }
        if (n>=0) text[n]=0;
        if (n>=0 && n<(int)sizeof(text)-1 && am_settings_parse(text,&s)>=0 && s.enabled && s.target==AM_UMA0) {
            SceUID th=ksceKernelCreateThread("PSVSDAMBoot",secondary_boot_worker,0x40,0x8000,0,0,NULL);
            if (th<0) return SCE_KERNEL_START_NO_RESIDENT;
            int ret=ksceKernelStartThread(th,0,NULL);
            if (ret<0) { ksceKernelDeleteThread(th);return SCE_KERNEL_START_NO_RESIDENT; }
            return SCE_KERNEL_START_SUCCESS;
        }
    }
    return initialize_runtime(argc,args);
}

int module_stop(SceSize argc, void *args) {
    (void)argc; (void)args;
    /* Kernel storage ownership must survive the app. Reboot to remove/switch.
     * In boot mode mount structures may point into this module's data segment. */
    if (USBMC_STATUS_ONLY) return SCE_KERNEL_STOP_SUCCESS;
    return SCE_KERNEL_STOP_CANCEL;
}
