/* Separate installer; only its worker promotes the stopped main app.
 * GPL-3.0-or-later. */
#include "update_install.h"
#include "../common/paths.h"
#include "ui.h"
#ifdef PSVSD_AM_HOST_TEST
#include "mock_installer.h"
#else
#include <psp2/appmgr.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/display.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static uint32_t *buffers[2];
static unsigned pending,presented,display_error_logged;
static AmUi model;
static int install_result=-1;
static unsigned install_done,install_phase;
static const char *failure_title="Installation failed";
static char failure[192];
static void handoff_log(const char *stage,int result) {
    FILE *f=fopen(AM_UPDATE_ROOT "/handoff.log","a");
    if (f) { fprintf(f,"%s: %08X\n",stage,(unsigned)result);fclose(f); }
}
static int init_display(void) {
    void *base=NULL;
    int block=sceKernelAllocMemBlock("AMInstaller",SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,4*1024*1024,NULL);
    if (block<0) return block;
    int ret=sceKernelGetMemBlockBase(block,&base);if (ret<0) return ret;
    memset(base,0,4*1024*1024);buffers[0]=base;
    buffers[1]=buffers[0]+2*1024*1024/sizeof(uint32_t);
    model.nav=VIEW_UPDATES;model.busy=1;model.update.state=UPDATE_INSTALLING;
    return 0;
}
static void display_error(const char *stage,int error) {
    if (error<0 && !display_error_logged) { handoff_log(stage,error);display_error_logged=1; }
}
static void pace(void) {
    int ret=sceDisplayWaitVblankStart();
    if (ret<0) { display_error("Display pace",ret);sceKernelDelayThread(16667); }
}
static int finish_flip(void) {
    int ret=sceDisplayWaitSetFrameBuf();
    if (ret<0) { display_error("Wait for framebuffer",ret);return 0; }
    SceDisplayFrameBuf actual={0};actual.size=sizeof(actual);
    ret=sceDisplayGetFrameBuf(&actual,SCE_DISPLAY_SETBUF_IMMEDIATE);
    if (ret<0) { display_error("Read presented framebuffer",ret);return 0; }
    if (actual.base!=buffers[pending-1]) return 0;
    pending=0;presented=1;return 1;
}
static void show(unsigned phase,unsigned tick) {
    /* Do not submit another frame or reuse either buffer until the last flip
     * is observed. A successful generic vblank wait alone did not prove that. */
    if (pending && !finish_flip()) { pace();return; }
    unsigned next=0;
    if (presented) {
        SceDisplayFrameBuf actual={0};actual.size=sizeof(actual);
        int ret=sceDisplayGetFrameBuf(&actual,SCE_DISPLAY_SETBUF_IMMEDIATE);
        if (ret<0) { display_error("Read active framebuffer",ret);pace();return; }
        next=actual.base==buffers[0]?1:0;
    }
    model.update_animation=tick;model.update_progress=(AmUpdateProgress){phase,0,0};
    model.update.error=phase==AM_UPDATE_INSTALL_ERROR?install_result:0;
    snprintf(model.update.reason,sizeof(model.update.reason),"%s",failure);
    snprintf(model.update.stage,sizeof(model.update.stage),"%s",failure_title);
    if (phase==AM_UPDATE_COMPLETE) model.update_progress.done=model.update_progress.total=1;
    am_ui_render(buffers[next],&model);
    SceDisplayFrameBuf display={sizeof(display),buffers[next],960,SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,960,544};
    int ret=sceDisplaySetFrameBuf(&display,SCE_DISPLAY_SETBUF_NEXTFRAME);
    if (ret<0) display_error("Submit framebuffer",ret);
    else { pending=next+1;finish_flip(); }
    pace();
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
    int display=init_display();
    if (display<0) { handoff_log("Installer display failed",display);return 1; }
    unsigned tick=0;show(AM_UPDATE_PREPARE,tick++);
    handoff_log("Installer " ACCESSORY_VERSION " started",0);
    int closed=sceAppMgrDestroyAppByName("RTPSV0001");
    handoff_log("Close previous PSVSD/AM",closed);
    int worker=sceKernelCreateThread("AMInstallerWorker",install_worker,0x10000100,0x10000,0,0,NULL);
    int start=worker;if (start>=0) start=sceKernelStartThread(worker,0,NULL);
    if (start<0) { install_result=start;__atomic_store_n(&install_done,1,__ATOMIC_RELEASE); }
    while (!__atomic_load_n(&install_done,__ATOMIC_ACQUIRE)) {
        unsigned phase=__atomic_load_n(&install_phase,__ATOMIC_ACQUIRE)?AM_UPDATE_SYSTEM_INSTALL:AM_UPDATE_PREPARE;
        show(phase,tick++);
    }
    if (worker>=0) { if (start>=0) sceKernelWaitThreadEnd(worker,NULL,NULL);sceKernelDeleteThread(worker); }
    int result=install_result;FILE *f;
    handoff_log("Install result",result);remove(AM_UPDATE_ROOT "/ready");
    f=fopen(AM_UPDATE_ROOT "/result","wb");if (f) { fprintf(f,"%08X",(unsigned)result);fclose(f); }
    if (result>=0) for (unsigned i=0;i<15;++i) show(AM_UPDATE_COMPLETE,tick++);
    if (result<0) snprintf(failure,sizeof(failure),"Installation did not complete.\nPress X to return to PSVSD/AM. Details are in Log.");
    int wait=result<0,held=1;
    for (;;) {
        if (wait) {
            show(AM_UPDATE_INSTALL_ERROR,tick++);
            SceCtrlData pad;memset(&pad,0,sizeof(pad));sceCtrlPeekBufferPositive(0,&pad,1);
            int pressed=(pad.buttons&SCE_CTRL_CROSS)!=0;
            if (!pressed || held) { held=pressed;continue; }
        }
        uint32_t token=sceKernelGetProcessTimeLow();if (!token) token=1;
        int launch=am_update_relaunch_start(token);
        handoff_log("Submit main-app launch",launch);
        if (launch>=0) {
            uint64_t deadline=sceKernelGetProcessTimeWide()+AM_RELAUNCH_WAIT_US;
            do {
                show(AM_UPDATE_REOPEN,tick++);
                if (am_update_relaunch_check(token)>0) {
                    handoff_log("Main-app startup acknowledged",0);am_update_relaunch_cancel();
                    sceKernelExitProcess(result<0?1:0);return 0;
                }
            } while (sceKernelGetProcessTimeWide()<deadline);
            launch=-1;handoff_log("Main-app startup acknowledgment timed out",launch);
        }
        am_update_relaunch_cancel();install_result=launch;wait=held=1;
        failure_title="Could not reopen PSVSD/AM";
        snprintf(failure,sizeof(failure),"No startup confirmation received.\nPress X to retry opening PSVSD/AM.");
    }
}
