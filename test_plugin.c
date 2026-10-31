#include <assert.h>
#include <stdlib.h>
#include "../src/plugin/main.c"

static struct {
    uint32_t io_nid, app_nid;
    int driver_present, exports_fail, boot_fail, loads_fail, start_fail;
    int segment_small, offset_fail, null_mount;
    int patch_fail_at, activation_fail, usb_after, uma_exists;
    int ux0_mount_fail, log_fail, write_chunk;
    int raw_mask, uma_mount_fail, uma_retry_failures, uma_null, uma_bad_alias;
    int read_fail;
    int primary_log_fail;
    int accessory_lookup_fail;
    int flag_fail, thread_fail, handler_fail, thread_start_fail;
    int suspend_at_delay, ownership_at_delay, uma_other_error;
    const char *config_text;
    int shell_live,usbserv_after,boot_thread_fail,boot_start_fail,runtime_info;
} cfg;
static int patches, releases, unloads, starts, loads, activates;
static int delays, mount_ux0, mount_uma0, unmounts;
static char captured_log[16384];
static size_t log_size;
static UsbmcIoDevice original1, original2;
static UsbmcMountPoint mock_mount;
static UsbmcIoDevice original_uma1, original_uma2;
static UsbmcMountPoint mock_uma;
static int uma_mounted,ux0_mounted;
static int total_cases;
static int event_bits, signals, deleted_flags, deleted_threads, unregistered, io_calls;
static int handler_calls, short_delays;
static int boot_threads,boot_starts;
static char runtime_text[256];

static void reset(void) {
    memset(&cfg, 0, sizeof(cfg));
    cfg.io_nid = 0xA96ACE9D;
    cfg.app_nid = 0x1C9879D6;
    cfg.usb_after = 1;
    cfg.raw_mask = 1;
    if (USBMC_MANAGED_BOOT) cfg.config_text="version=1\nenabled=1\nmount=ux0\n";
    patches = releases = unloads = starts = loads = activates = 0;
    delays = mount_ux0 = mount_uma0 = unmounts = 0;
    log_size = 0; captured_log[0] = 0;
    memset(&mock_mount, 0, sizeof(mock_mount));
    mock_mount.dev = &original1;
    mock_mount.dev2 = &original2;
    log_fd = -1; appmgr_patches[0] = appmgr_patches[1] = -1;
    find_mount = NULL; redirected = 0;
    uma_owned=0; active_log_path=LOG_PATH;
    resume_flag=resume_thread=resume_handler=-1;
    resume_enabled=suspended=resume_epoch=resume_count=0;resume_queued=1;
    event_bits=signals=deleted_flags=deleted_threads=unregistered=io_calls=0;
    handler_calls=short_delays=0;
    configured_target=AM_UMA0;boot_threads=boot_starts=0;runtime_text[0]=0;
    original_uma1 = (UsbmcIoDevice){"uma0:", "exfatuma0", "old:partition", "old:entire", UMA0_MOUNT};
    original_uma2 = original_uma1;
    memset(&mock_uma, 0, sizeof(mock_uma));
    mock_uma.dev = &original_uma1;
    mock_uma.dev2 = &original_uma2;
    uma_mounted = ux0_mounted = 0;
    ++total_cases;
}
static void unchanged(void) {
    assert(mock_mount.dev == &original1);
    assert(mock_mount.dev2 == &original2);
    assert(mount_ux0 == 0 && unmounts == 0);
}
int ksceIoOpen(const char *path, int flags, int mode) {
    (void)mode;
    ++io_calls;
    if (!strcmp(path,AM_SETTINGS_PATH)) return cfg.config_text?3:-1;
    if (!strcmp(path,ACCESSORY_KERNEL_DIR "/runtime.txt")) return 4;
    if (flags & SCE_O_WRONLY)
        return cfg.log_fail || (cfg.primary_log_fail && !strcmp(path,LOG_PATH)) ? -1 : 1;
    if (!strcmp(path, RAW_USB)) return (cfg.raw_mask & 1) && delays >= cfg.usb_after ? 2 : -1;
    if (!strcmp(path, RAW_USB_PART)) return (cfg.raw_mask & 2) && delays >= cfg.usb_after ? 2 : -1;
    if (!strcmp(path, "uma0:")) return cfg.uma_exists ? 2 : -1;
    return -1;
}
int ksceIoRemove(const char *p) { assert(p);return 0; }
int ksceIoClose(int fd) { assert(fd >= 0); return 0; }
int ksceIoMkdir(const char *p,int m) { assert(p && m==0777); return 0; }
int ksceIoDopen(const char *p) {
    ++io_calls;
    if (!strcmp(p,"ux0:")) return ux0_mounted ? 6 : -1;
    assert(!strcmp(p,"uma0:")); return cfg.uma_exists || uma_mounted ? 6 : -1;
}
int ksceIoDclose(int fd) { assert(fd==6); return 0; }
int ksceIoRead(int fd, void *p, SceSize n) {
    if (fd==3) { size_t bytes=strlen(cfg.config_text);if (bytes>n) bytes=n;memcpy(p,cfg.config_text,bytes);return (int)bytes; }
    assert(fd==2 && n==512);
    if (cfg.read_fail) return -5;
    memset(p,0,n); memcpy((unsigned char *)p+3,"EXFAT   ",8);
    ((unsigned char *)p)[510]=0x55; ((unsigned char *)p)[511]=0xAA;
    return n;
}
int ksceIoWrite(int fd, const void *p, size_t n) {
    if (fd==4) { assert(n<sizeof(runtime_text));memcpy(runtime_text,p,n);runtime_text[n]=0;return (int)n; }
    assert(fd == 1);
    if (cfg.write_chunk && n > (size_t)cfg.write_chunk) n = cfg.write_chunk;
    assert(log_size + n < sizeof(captured_log));
    memcpy(captured_log + log_size, p, n);
    log_size += n; captured_log[log_size] = 0;
    return n;
}
int ksceIoMount(int id, const char *p, int a, int b, int c, int d) {
    (void)p; (void)a; (void)b; (void)c; (void)d;
    if (id == UMA0_MOUNT) {
        ++mount_uma0;
        if (cfg.uma_other_error) return -55;
        const char *expected_primary = (cfg.raw_mask & 2) ? RAW_USB_PART : RAW_USB;
        const char *expected_fallback = (cfg.raw_mask & 1) ? RAW_USB : expected_primary;
        if (!resume_count) assert(mock_uma.dev == &usb_uma[0] && mock_uma.dev2 == &usb_uma[1]);
        assert(!strcmp(mock_uma.dev->blkdev,expected_primary));
        assert(!strcmp(mock_uma.dev2->blkdev2,expected_fallback));
        if (cfg.uma_mount_fail || mount_uma0 <= cfg.uma_retry_failures) return (int)0x80010013;
        uma_mounted=1; return 0;
    }
    assert(id == UX0_MOUNT); ++mount_ux0;
    if (cfg.ux0_mount_fail && mount_ux0 == 1) return -55;
    ux0_mounted=1;return 0;
}
int ksceIoUmount(int id, int a, int b, int c) {
    (void)a; (void)b; (void)c;
    assert(id == UX0_MOUNT); ++unmounts; return 0;
}
int taiGetModuleInfoForKernel(SceUID pid, const char *name, tai_module_info_t *i) {
    assert(pid == KERNEL_PID);
    if (!strcmp(name, "SceIofilemgr")) { i->modid=10; i->module_nid=cfg.io_nid; return 0; }
    if (!strcmp(name, "SceAppMgr")) { i->modid=11; i->module_nid=cfg.app_nid; return 0; }
    if (!strcmp(name, "SceUsbMass") && cfg.driver_present) { i->modid=12; return 0; }
    if (!strcmp(name,"SceUsbServ") && delays>=cfg.usbserv_after) { i->modid=13;return 0; }
    if (!strcmp(name,"SceShell") && cfg.shell_live) { i->modid=14;return 0; }
    if ((!strcmp(name,AM_KERNEL_NAME)||!strcmp(name,AM_PROBE_NAME)) && cfg.runtime_info) { i->modid=0x1234;return 0; }
    return -1;
}
int taiInjectDataForKernel(SceUID pid, SceUID mid, int seg, size_t off,
                           const void *p, size_t n) {
    assert(pid == KERNEL_PID && mid >= 0 && seg == 0 && p && n);
    assert(off == 0x1546 || off == 0x154C || off == 0xB338 || off == 0xB368 ||
           off == 0xB344 || off == 0xB374);
    ++patches;
    return patches == cfg.patch_fail_at ? -77 : 100 + patches;
}
int taiInjectReleaseForKernel(SceUID id) { assert(id >= 100); ++releases; return 0; }
static int mount_bootfs_mock(const char *path) {
    assert(!strcmp(path, "os0:kd/bootimage.skprx")); return cfg.boot_fail ? -1 : 0;
}
static int unmount_bootfs_mock(void) { return 0; }
int module_get_export_func(SceUID pid, const char *name, uint32_t lib,
                           uint32_t fn, uintptr_t *out) {
    assert(pid==KERNEL_PID);
    if (!strcmp(name,"SceUsbServ")) {
        assert(lib==TAI_ANY_LIBRARY && fn==0xAA6D4409);
        if (cfg.accessory_lookup_fail) return -1;
        *out=(uintptr_t)ksceUsbServAccessoryActivate;return 0;
    }
    assert(!strcmp(name,"SceKernelModulemgr"));
    if (cfg.exports_fail) return -1;
    if (fn == 0x01360661 || fn == 0x185FF1BC) *out = (uintptr_t)mount_bootfs_mock;
    else { assert(fn == 0x9C838A6B || fn == 0xBD61AD4D); *out = (uintptr_t)unmount_bootfs_mock; }
    return 0;
}
static UsbmcMountPoint *find_mount_mock(int id) {
    if (id == UMA0_MOUNT) {
        if (cfg.uma_bad_alias) original_uma1.dev="wrong0:";
        return cfg.uma_null ? NULL : &mock_uma;
    }
    assert(id == UX0_MOUNT); return cfg.null_mount ? NULL : &mock_mount;
}
int module_get_offset(SceUID p, SceUID m, int seg, size_t off, uintptr_t *out) {
    assert(p==KERNEL_PID && seg==0);
    if (m==12) {
        assert(off==0x1550);
        if (cfg.segment_small) return -1;
        *out=0x1550;return 0;
    }
    assert(m==10);
    assert(off == 0x138C1 || off == 0x182F5);
    if (cfg.offset_fail) return -1;
    *out = (uintptr_t)find_mount_mock; return 0;
}
int ksceKernelLoadModule(const char *p, int flags, void *o) {
    (void)flags; (void)o;
    assert(!strcmp(p, "os0:kd/umass.skprx") || !strcmp(p, "ux0:VitaShell/module/umass.skprx"));
    ++loads; return cfg.loads_fail ? -90 : 12;
}
int ksceKernelStartModule(SceUID m, SceSize n, void *p, int a, void *b, void *c) {
    (void)n; (void)p; (void)a; (void)b; (void)c;
    assert(m == 12); ++starts; return cfg.start_fail ? -91 : 0;
}
int ksceKernelUnloadModule(SceUID m, int f, void *o) {
    (void)f; (void)o; assert(m == 12); ++unloads; return 0;
}
int ksceUsbServAccessoryActivate(void) { ++activates; return cfg.activation_fail ? -92 : 0; }
int ksceKernelDelayThread(unsigned us) {
    assert(us == 200000 || us == 50000);
    if (us==50000) { ++short_delays; return 0; }
    ++delays;
    if (cfg.suspend_at_delay==delays) resume_event(0,0x100,NULL,NULL);
    if (cfg.ownership_at_delay==delays) mock_uma.dev=&original_uma1;
    return 0;
}
int ksceKernelCreateEventFlag(const char *n,int a,unsigned bits,void *p) {
    assert(!strcmp(n,"PSVSDResume") && !a && !bits && !p);return cfg.flag_fail?-11:70;
}
int ksceKernelDeleteEventFlag(int id) { assert(id==70);++deleted_flags;return 0; }
int ksceKernelSetEventFlag(int id,unsigned bits) { assert(id==70 && bits==1);event_bits|=bits;++signals;return 0; }
int ksceKernelWaitEventFlag(int id,unsigned bits,int mode,unsigned *out,unsigned *timeout) {
    assert(id==70 && bits==1 && mode==(SCE_EVENT_WAITOR|SCE_EVENT_WAITCLEAR_PAT) && !timeout);
    if (!event_bits) return -1;
    *out=event_bits;event_bits=0;return 0;
}
int ksceKernelCreateThread(const char *n,int (*fn)(SceSize,void *),int pri,int stack,int a,int b,void *p) {
    if (!strcmp(n,"PSVSDAMBoot")) {
        assert(fn==secondary_boot_worker && pri==0x40 && stack==0x8000);++boot_threads;return cfg.boot_thread_fail?-12:73;
    }
    assert(!strcmp(n,"PSVSDResume") && fn==resume_worker && pri==0x40 && stack==0x4000 && !a && !b && !p);
    return cfg.thread_fail?-12:71;
}
int ksceKernelStartThread(int id,SceSize n,void *p) {
    if (id==73) { ++boot_starts;return cfg.boot_start_fail?-13:0; }
    assert(id==71 && !n && !p);return cfg.thread_start_fail?-13:0;
}
int ksceKernelDeleteThread(int id) { assert(id==71 || id==73);++deleted_threads;return 0; }
int ksceKernelExitDeleteThread(int result) { assert(!result);return 0; }
int ksceKernelRegisterSysEventHandler(const char *n,int (*fn)(int,int,void *,void *),void *p) {
    assert(!strcmp(n,"PSVSDResume") && fn==resume_event && !p);++handler_calls;return cfg.handler_fail?-14:72;
}
int ksceKernelUnregisterSysEventHandler(int id) { assert(id==72);++unregistered;return 0; }

static void mounted_for_resume(void) {
    cfg.config_text="version=1\nenabled=1\nmount=uma0\n";assert(initialize_runtime(0,NULL)==0);
    assert(uma_owned && resume_enabled && handler_calls==1 && mount_uma0==1);
    unchanged();
}
static void wake(void) {
    int old_io=io_calls;
    resume_event(0,0x100,NULL,NULL); resume_event(1,0x100000,NULL,NULL);
    assert(io_calls==old_io); /* Callback never performs storage I/O. */
    resume_worker(0,NULL);
}

int main(void) {
    const uint32_t ios[] = {0x9642948C,0xA96ACE9D,0x3347A95F,0x90DA33DE};
    const uint32_t apps[] = {0xDBB29DB7,0x1C9879D6,0x54E2E984,0xC3C538DE};
    for (size_t i=0; i<4; ++i) {
        reset(); cfg.io_nid=ios[i]; cfg.app_nid=apps[i];
        assert(initialize_runtime(0,NULL) == SCE_KERNEL_START_SUCCESS);
        assert(activates == 1 && starts == 1 && releases == 2);
        if (USBMC_ACCESSORY_PROBE) { unchanged(); assert(mount_uma0 == 1 && patches == 2); }
        else { assert(redirected && mock_mount.dev == &usb_ux0 && mount_ux0 == 1 && patches == 4); }
        assert(log_fd == -1 && strstr(captured_log,"FOUND"));
        assert(module_stop(0,NULL) == SCE_KERNEL_STOP_CANCEL);
    }
    reset(); cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL) == 0); unchanged(); assert(mount_uma0 == 1 && patches == 2);
    reset(); cfg.usb_after=100;
    assert(initialize_runtime(0,NULL) == 0); unchanged(); assert(delays==26 && !mount_uma0);
    assert(strstr(captured_log,"NOT FOUND"));
    reset(); cfg.io_nid=0xBAD;
    assert(initialize_runtime(0,NULL) == 1); unchanged(); assert(!patches && !activates && !loads);
    reset(); cfg.loads_fail=1;
    assert(initialize_runtime(0,NULL) == 1); unchanged(); assert(loads==2 && !patches && !activates);
    reset(); cfg.patch_fail_at=1;
    assert(initialize_runtime(0,NULL) == 1); unchanged(); assert(!releases && unloads==1 && !starts);
    reset(); cfg.patch_fail_at=2;
    assert(initialize_runtime(0,NULL) == 1); unchanged(); assert(releases==1 && unloads==1 && !starts);
    reset(); cfg.start_fail=1;
    assert(initialize_runtime(0,NULL) == 1); unchanged(); assert(releases==2 && unloads==1 && !activates);
    reset(); cfg.activation_fail=1;
    assert(initialize_runtime(0,NULL) == 1); unchanged(); assert(releases==2 && !mount_uma0 && log_fd==-1);
    reset(); cfg.accessory_lookup_fail=1;
    assert(initialize_runtime(0,NULL)==1); unchanged(); assert(!activates && !mount_uma0 && log_fd==-1);
    assert(strstr(captured_log,"Accessory export unavailable"));
    reset(); cfg.segment_small=1;
    assert(initialize_runtime(0,NULL) == 1); unchanged(); assert(!patches && unloads==1);
    reset(); cfg.exports_fail=1; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL) == 0); unchanged(); assert(loads==1 && starts==1);
    reset(); cfg.boot_fail=1; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL) == 0); unchanged(); assert(loads==1 && starts==1);
    reset(); cfg.driver_present=1; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL) == 0); unchanged(); assert(!patches && !loads && !starts && activates==1);
    reset(); cfg.uma_exists=1; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL) == 0); unchanged(); assert(!mount_uma0);
    reset(); cfg.write_chunk=7; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL) == 0); unchanged(); assert(strstr(captured_log,"AccessoryActivate: 00000000\n"));
    reset(); cfg.log_fail=1; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL) == 0); unchanged(); assert(activates==1 && !log_size);
    reset(); cfg.primary_log_fail=1; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL)==0); unchanged(); assert(strstr(captured_log,"Primary log open: FFFFFFFF"));
    reset(); cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    AccessoryRequest req={ACCESSORY_REQUEST_MAGIC,0x12345678};
    assert(initialize_runtime(sizeof(req),&req)==0); unchanged(); assert(strstr(captured_log,"Request: 12345678"));
    reset(); cfg.config_text="version=1\nenabled=1\nmount=uma0\n"; req.magic=0;
    assert(initialize_runtime(sizeof(req),&req)==0); unchanged(); assert(strstr(captured_log,"Request: 00000000"));
    reset(); cfg.usb_after=0; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL)==0); unchanged(); assert(delays==0 && mount_uma0==1);
    assert(strstr(captured_log,"USB paths before init: partition=absent entire=FOUND"));
    reset(); cfg.offset_fail=1;
    assert(initialize_runtime(0,NULL) == 1); unchanged(); assert(!loads && !activates);
    for (int mask=1;mask<=3;++mask) {
        reset(); cfg.raw_mask=mask; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
        assert(initialize_runtime(0,NULL)==0); unchanged(); assert(mount_uma0==1 && uma_mounted);
        assert(mock_uma.dev==&usb_uma[0] && mock_uma.dev2==&usb_uma[1]);
        assert(strstr(captured_log,"exFAT volume boot sector"));
    }
    reset(); cfg.uma_mount_fail=1; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL)==0); unchanged(); assert(mount_uma0==6 && !uma_mounted);
    assert(mock_uma.dev==&original_uma1 && mock_uma.dev2==&original_uma2);
    assert(strstr(captured_log,"Mount uma0: 80010013"));
    reset(); cfg.uma_retry_failures=2; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL)==0); unchanged(); assert(mount_uma0==3 && uma_mounted);
    reset(); cfg.uma_null=1; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL)==0); unchanged(); assert(!mount_uma0);
    reset(); cfg.uma_bad_alias=1; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL)==0); unchanged(); assert(!mount_uma0);
    reset(); cfg.read_fail=1; cfg.config_text="version=1\nenabled=1\nmount=uma0\n";
    assert(initialize_runtime(0,NULL)==0); unchanged(); assert(mount_uma0==1);
    assert(strstr(captured_log,"Raw read " RAW_USB ": FFFFFFFB"));
    if (!USBMC_ACCESSORY_PROBE) {
        reset(); cfg.app_nid=0xBAD;
        assert(initialize_runtime(0,NULL) == 0); unchanged(); assert(mount_uma0==1 && patches==2);
        reset(); cfg.patch_fail_at=4;
        assert(initialize_runtime(0,NULL) == 0); unchanged(); assert(mount_uma0==1 && releases==3);
        reset(); cfg.null_mount=1;
        assert(initialize_runtime(0,NULL) == 0); unchanged(); assert(mount_uma0==1 && patches==2);
        reset(); cfg.ux0_mount_fail=1;
        assert(initialize_runtime(0,NULL) == 0);
        assert(mock_mount.dev==&original1 && mock_mount.dev2==&original2);
        assert(mount_ux0==2 && mount_uma0==1 && releases==4 && !redirected);
    }
    reset(); mounted_for_resume();uma_mounted=0;wake();
    assert(mount_uma0==2 && uma_mounted && activates==1 && signals==1);
    assert(strstr(captured_log,"Resume uma0: directory: accessible"));unchanged();
    /* A second sleep/wake must queue a fresh cycle, not just the first one. */
    uma_mounted=0;wake();assert(mount_uma0==3 && resume_count==2 && activates==1);
    reset(); mounted_for_resume();wake();assert(mount_uma0==1 && short_delays==1);
    reset(); mounted_for_resume();uma_mounted=0;cfg.usb_after=delays+4;wake();
    assert(mount_uma0==2 && uma_mounted);
    reset(); mounted_for_resume();uma_mounted=0;cfg.usb_after=delays+100;wake();
    assert(mount_uma0==1 && !uma_mounted && activates==1);
    assert(strstr(captured_log,"Resume USB raw device: NOT FOUND"));
    reset(); mounted_for_resume();uma_mounted=0;cfg.uma_retry_failures=3;wake();
    assert(mount_uma0==4 && uma_mounted);
    reset(); mounted_for_resume();uma_mounted=0;cfg.uma_mount_fail=1;wake();
    assert(mount_uma0==7 && !uma_mounted);unchanged();
    reset(); mounted_for_resume();uma_mounted=0;cfg.uma_other_error=1;wake();
    assert(mount_uma0==2 && !uma_mounted);
    reset(); mounted_for_resume();uma_mounted=0;mock_uma.dev=&original_uma1;wake();
    assert(mount_uma0==1 && mock_uma.dev==&original_uma1);
    reset(); mounted_for_resume();uma_mounted=0;
    original_uma1=usb_uma[0];original_uma2=usb_uma[1];
    mock_uma.dev=&original_uma1;mock_uma.dev2=&original_uma2;wake();
    assert(mount_uma0==2 && uma_mounted && mock_uma.dev==&original_uma1);
    reset(); mounted_for_resume();uma_mounted=0;cfg.usb_after=delays+3;
    cfg.ownership_at_delay=delays+1;wake();assert(mount_uma0==1 && !uma_mounted);
    reset(); mounted_for_resume();uma_mounted=0;cfg.usb_after=delays+100;
    cfg.suspend_at_delay=delays+1;wake();assert(mount_uma0==1 && suspended);
    reset(); mounted_for_resume();uma_mounted=0;
    resume_event(0,0x100,NULL,NULL);resume_event(1,0x10000,NULL,NULL);
    assert(suspended && !signals);resume_worker(0,NULL);assert(mount_uma0==1);
    reset(); mounted_for_resume();uma_mounted=0;
    resume_event(0,0x100,NULL,NULL);resume_event(1,0x100000,NULL,NULL);
    resume_event(1,0x100000,NULL,NULL);assert(signals==1);
    resume_event(0,0x100,NULL,NULL);resume_worker(0,NULL);
    assert(mount_uma0==1 && !resume_count);
    reset();cfg.config_text="version=1\nenabled=1\nmount=uma0\n";cfg.flag_fail=1;assert(initialize_runtime(0,NULL)==0);
    assert(uma_owned && !resume_enabled && !deleted_flags && !handler_calls);
    reset();cfg.config_text="version=1\nenabled=1\nmount=uma0\n";cfg.thread_fail=1;assert(initialize_runtime(0,NULL)==0);
    assert(uma_owned && !resume_enabled && deleted_flags==1 && !deleted_threads);
    reset();cfg.config_text="version=1\nenabled=1\nmount=uma0\n";cfg.handler_fail=1;assert(initialize_runtime(0,NULL)==0);
    assert(!resume_enabled && deleted_flags==1 && deleted_threads==1 && !unregistered);
    reset();cfg.config_text="version=1\nenabled=1\nmount=uma0\n";cfg.thread_start_fail=1;assert(initialize_runtime(0,NULL)==0);
    assert(!resume_enabled && deleted_flags==1 && deleted_threads==1 && unregistered==1);
    reset();cfg.uma_exists=1;cfg.config_text="version=1\nenabled=1\nmount=uma0\n";assert(initialize_runtime(0,NULL)==0);
    assert(!uma_owned && !resume_enabled && !handler_calls);
    reset(); mounted_for_resume();uma_mounted=0;cfg.log_fail=1;wake();
    assert(mount_uma0==2 && uma_mounted && log_fd==-1);
    reset();cfg.primary_log_fail=1;mounted_for_resume();uma_mounted=0;wake();
    assert(mount_uma0==2 && !strcmp(active_log_path,ACCESSORY_KERNEL_FALLBACK));
    if (!USBMC_ACCESSORY_PROBE) {
        reset();assert(initialize_runtime(0,NULL)==0 && redirected && resume_enabled);
        ux0_mounted=0;wake();assert(ux0_mounted && mount_ux0==2 && !mount_uma0 && activates==1);
    }
    printf("%s: %d host-mocked cases passed\n",USBMC_ACCESSORY_PROBE?"probe":"boot",total_cases);
    return 0;
}
