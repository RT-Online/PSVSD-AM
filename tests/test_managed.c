#define USBMC_MANAGED_BOOT 1
#define main shared_regression_cases
#include "test_plugin.c"
#undef main
int main(void) {
    static const char *uma="version=1\nenabled=1\nmount=uma0\n";
    static const char *ux="version=1\nenabled=1\nmount=ux0\n";
    reset();cfg.config_text=NULL;assert(module_start(0,NULL)==1);unchanged();assert(!loads && !activates);
    reset();cfg.config_text="version=1\nenabled=0\nmount=uma0\n";
    assert(module_start(0,NULL)==1 && !boot_threads);unchanged();
    reset();cfg.config_text="version=1\nenabled=1\nmount=gro0\n";
    assert(module_start(0,NULL)==1 && !loads);unchanged();
    reset();cfg.config_text=uma;cfg.runtime_info=1;cfg.usbserv_after=3;
    assert(module_start(0,NULL)==0 && boot_threads==1 && boot_starts==1 && !activates);
    secondary_boot_worker(0,NULL);unchanged();assert(mount_uma0==1 && resume_enabled && activates==1);
    assert(strstr(runtime_text,"Kernel: 00001234") && strstr(runtime_text,"Mount: uma0:"));
    reset();cfg.config_text=uma;cfg.boot_thread_fail=1;
    assert(module_start(0,NULL)==1 && !boot_starts && !loads);
    reset();cfg.config_text=uma;cfg.boot_start_fail=1;
    assert(module_start(0,NULL)==1 && deleted_threads==1 && !loads);
    reset();cfg.legacy_config=1;cfg.config_text=ux;
    assert(module_start(0,NULL)==0 && redirected && mount_ux0==1 && !mount_uma0);
    assert(resume_enabled);ux0_mounted=0;wake();assert(mount_ux0==2 && ux0_mounted);
    reset();cfg.config_text=ux;cfg.shell_live=1;
    assert(module_start(0,NULL)==1 && !loads && !activates);unchanged();
    reset();cfg.config_text=ux;cfg.ux0_mount_fail=1;
    assert(module_start(0,NULL)==0 && !redirected && mount_uma0==1);
    assert(mock_mount.dev==&original1 && mock_mount.dev2==&original2);
    reset();cfg.config_text=uma;cfg.usbserv_after=100;cfg.accessory_lookup_fail=1;
    assert(module_start(0,NULL)==0);secondary_boot_worker(0,NULL);
    assert(!mount_uma0 && !mount_ux0 && delays==75 && !resume_enabled);
    reset();cfg.config_text=ux;cfg.uma_exists=1;
    assert(module_start(0,NULL)==0 && !redirected && !mount_ux0);unchanged();
    printf("managed boot: %d configuration, boot-order and primary recovery cases passed\n",total_cases);
    return 0;
}
