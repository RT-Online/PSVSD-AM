#define USBMC_STATUS_ONLY 1
#define main shared_regression_cases
#include "test_plugin.c"
#undef main
static void query(void) {
    AccessoryRequest req={ACCESSORY_REQUEST_MAGIC,0xABCDEF01};
    assert(module_start(sizeof(req),&req)==SCE_KERNEL_START_NO_RESIDENT);
    assert(!loads && !activates && !patches && !mount_uma0 && !mount_ux0 && !handler_calls);
    assert(strstr(runtime_text,"Request: ABCDEF01"));
}
int main(void) {
    reset();cfg.usb_after=0;original_uma1=usb_ux0;original_uma2=usb_ux0;
    original_uma1.dev=original_uma2.dev="uma0:";uma_mounted=1;query();assert(strstr(runtime_text,"Mount: uma0:"));
    reset();cfg.usb_after=0;mock_mount.dev=mock_mount.dev2=&usb_ux0;ux0_mounted=1;
    query();assert(strstr(runtime_text,"Mount: ux0:"));
    reset();cfg.usb_after=0;uma_mounted=1;query();assert(strstr(runtime_text,"Mount: none"));
    reset();cfg.usb_after=0;original_uma1=usb_ux0;original_uma2=usb_ux0;
    original_uma1.dev=original_uma2.dev="uma0:";mock_mount.dev=mock_mount.dev2=&usb_ux0;
    uma_mounted=ux0_mounted=1;query();assert(strstr(runtime_text,"Mount: none"));
    reset();assert(module_start(0,NULL)==1 && !runtime_text[0]);
    reset();cfg.io_nid=0xBAD;query();assert(strstr(runtime_text,"Mount: none"));
    assert(module_stop(0,NULL)==SCE_KERNEL_STOP_SUCCESS);
    printf("status: %d read-only mapping and stale/ambiguous-state cases passed\n",total_cases);
    return 0;
}
