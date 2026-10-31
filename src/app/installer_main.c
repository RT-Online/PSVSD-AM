/* Separate app handoff avoids promoting over a running PSVSD/AM process.
 * Uses VitaShell's updater/promotion pattern. GPL-3.0-or-later. */
#include "update_install.h"
#include <psp2/appmgr.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/display.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "ui.h"
static void *base;
static int install_result=-1;
static unsigned install_done,install_phase;
static void show(int frame) {
    if (!base) {
        int block=sceKernelAllocMemBlock("AMInstaller",SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,4*1024*1024,NULL);
        if (block<0 || sceKernelGetMemBlockBase(block,&base)<0) return;
    }
    AmUi model={0};model.nav=VIEW_UPDATES;model.busy=1;model.update.state=UPDATE_INSTALLING;
    model.update_animation=(unsigned)(frame<0?0:frame);
    model.update_progress.phase=frame<0?AM_UPDATE_INSTALL_ERROR:frame==1000?AM_UPDATE_COMPLETE:
        __atomic_load_n(&install_phase,__ATOMIC_ACQUIRE)?AM_UPDATE_SYSTEM_INSTALL:AM_UPDATE_PREPARE;
    model.update.error=frame<0?install_result:0;
    if (frame==1000) model.update_progress.done=model.update_progress.total=1;
    am_ui_render(base,&model);
    SceDisplayFrameBuf display={sizeof(display),base,960,SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,960,544};
    sceDisplaySetFrameBuf(&display,SCE_DISPLAY_SETBUF_NEXTFRAME);
}
static int install_worker(SceSize argc,void *args) {
    (void)argc;(void)args;
    char expected[7]={0};FILE *f=fopen(AM_UPDATE_ROOT "/ready","rb");int ready=0;
    if (f) { ready=fread(expected,1,7,f)==6;fclose(f); }
    if (ready && am_package_validate(AM_UPDATE_STAGE,expected)==0) {
        __atomic_store_n(&install_phase,1,__ATOMIC_RELEASE);
        install_result=am_package_promote(AM_UPDATE_STAGE);
    }
    __atomic_store_n(&install_done,1,__ATOMIC_RELEASE);return 0;
}
int main(void) {
    show(0);
    sceAppMgrDestroyOtherApp();
    int worker=sceKernelCreateThread("AMInstallerWorker",install_worker,0x10000100,0x10000,0,0,NULL);
    int start=worker;if (start>=0) start=sceKernelStartThread(worker,0,NULL);
    if (start<0) { install_result=start;__atomic_store_n(&install_done,1,__ATOMIC_RELEASE); }
    unsigned tick=0;
    while (!__atomic_load_n(&install_done,__ATOMIC_ACQUIRE)) {
        show((int)(tick++%100));sceKernelDelayThread(50000);
    }
    if (worker>=0) { if (start>=0) sceKernelWaitThreadEnd(worker,NULL,NULL);sceKernelDeleteThread(worker); }
    int result=install_result;FILE *f;
    remove(AM_UPDATE_ROOT "/ready");
    f=fopen(AM_UPDATE_ROOT "/result","wb");if (f) { fprintf(f,"%08X",(unsigned)result);fclose(f); }
    if (result<0) {
        show(-1);
        for (;;) { SceCtrlData pad;memset(&pad,0,sizeof(pad));sceCtrlPeekBufferPositive(0,&pad,1);if (pad.buttons&SCE_CTRL_CROSS) break;sceKernelDelayThread(20000); }
    } else {
        show(1000);sceKernelDelayThread(500000);
    }
    sceAppMgrLaunchAppByUri(0xFFFFF,"psgm:play?titleid=RTPSV0001");sceKernelExitProcess(result<0?1:0);return 0;
}
