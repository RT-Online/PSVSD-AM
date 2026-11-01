#define _DEFAULT_SOURCE
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#define main installer_entry
#include "../src/app/installer_main.c"
#undef main
static uint32_t memory[1024*1024];
static void *scanned,*queued;
static uint64_t now;
static int frames,submitted,checks,ack_after,launch_error,promote_error,exit_status;
static int flip_delay,flip_left,get_error,submit_error,render_count;
static jmp_buf stop;
int sceKernelAllocMemBlock(const char *name,int type,size_t size,void *opt) { (void)name;assert(type==1 && size==sizeof(memory) && !opt);return 1; }
int sceKernelGetMemBlockBase(int block,void **base) { assert(block==1);*base=memory;return 0; }
int sceDisplaySetFrameBuf(const SceDisplayFrameBuf *frame,int mode) {
    assert(mode==1 && frame->base!=scanned);assert(!queued);
    if (submit_error) return submit_error;
    queued=frame->base;flip_left=flip_delay;++frames;return 0;
}
int sceDisplayWaitVblankStart(void) {
    if (queued && --flip_left<=0) { scanned=queued;queued=NULL; }
    now+=16667;return 0;
}
int sceDisplayWaitSetFrameBuf(void) { return sceDisplayWaitVblankStart(); }
int sceDisplayGetFrameBuf(SceDisplayFrameBuf *frame,int mode) {
    assert(mode==0);if (get_error) return get_error;frame->base=scanned;return 0;
}
int sceKernelDelayThread(unsigned usec) { now+=usec;return 0; }
void am_ui_render(uint32_t *pixels,const AmUi *ui) { assert(pixels!=scanned && pixels!=queued);assert(ui->busy);++render_count;pixels[0]=frames; }
int sceAppMgrDestroyAppByName(const char *title) { assert(!strcmp(title,"RTPSV0001"));return 0; }
int sceKernelCreateThread(const char *name,int (*entry)(SceSize,void*),int priority,size_t stack,int attr,int cpu,void *opt) {
    (void)name;(void)priority;(void)stack;(void)attr;(void)cpu;assert(entry==install_worker && !opt);return 2;
}
int sceKernelStartThread(int thread,SceSize argc,void *args) { assert(thread==2);return install_worker(argc,args); }
int sceKernelWaitThreadEnd(int thread,void *status,void *timeout) { assert(thread==2 && !status && !timeout);return 0; }
int sceKernelDeleteThread(int thread) { assert(thread==2);return 0; }
int sceCtrlPeekBufferPositive(int port,SceCtrlData *pad,int count) {
    assert(!port && count==1);pad->buttons=0;
    assert(model.update_progress.phase==AM_UPDATE_INSTALL_ERROR);
    if (promote_error) { assert(submitted==0);longjmp(stop,4); }
    if (launch_error) { assert(submitted==1 && checks==0);assert(install_result==launch_error);longjmp(stop,3); }
    assert(now>=AM_RELAUNCH_WAIT_US && submitted==1 && checks>1);longjmp(stop,2);
}
uint32_t sceKernelGetProcessTimeLow(void) { return (uint32_t)now; }
uint64_t sceKernelGetProcessTimeWide(void) { return now; }
int sceKernelExitProcess(int status) { exit_status=status;longjmp(stop,1); }
int am_package_validate(const char *path,const char *expected) { assert(!strcmp(path,AM_UPDATE_STAGE) && !strcmp(expected,"01.17"));return 0; }
int am_package_promote(const char *path) { assert(!strcmp(path,AM_UPDATE_STAGE));return promote_error; }
int am_update_relaunch_start(uint32_t token) { assert(token);++submitted;return launch_error; }
int am_update_relaunch_check(uint32_t token) { assert(token);++checks;return ack_after>0 && checks>=ack_after; }
void am_update_relaunch_cancel(void) {}
static void reset(void) {
    scanned=queued=NULL;now=0;frames=submitted=checks=ack_after=launch_error=promote_error=0;
    pending=presented=display_error_logged=install_done=install_phase=0;
    flip_delay=1;flip_left=get_error=submit_error=render_count=0;install_result=-1;memset(&model,0,sizeof(model));failure[0]=0;
    FILE *f=fopen(AM_UPDATE_ROOT "/ready","wb");assert(f);assert(fwrite("01.17",1,6,f)==6);fclose(f);
}
int main(void) {
    char root[]="/tmp/am-installer-XXXXXX";assert(mkdtemp(root));assert(chdir(root)==0);
    assert(mkdir("ux0:data",0700)==0);assert(mkdir("ux0:data/PSVSD_Accessory",0700)==0);assert(mkdir(AM_UPDATE_ROOT,0700)==0);
    reset();assert(init_display()==0);for (unsigned i=0;i<120;++i) show(AM_UPDATE_SYSTEM_INSTALL,i);
    assert(frames==120 && now==240*16667u); /* Real renderer never touches scanout. */
    reset();assert(init_display()==0);flip_delay=4;
    for (unsigned i=0;i<120;++i) show(AM_UPDATE_SYSTEM_INSTALL,i);
    assert(frames>20 && frames<120 && render_count==frames);
    get_error=-66;int before=render_count;
    for (unsigned i=0;i<6;++i) show(AM_UPDATE_SYSTEM_INSTALL,i);
    assert(render_count==before);get_error=0;
    for (unsigned i=0;i<6;++i) show(AM_UPDATE_SYSTEM_INSTALL,i);
    assert(render_count>before);
    reset();assert(init_display()==0);submit_error=-77;show(AM_UPDATE_SYSTEM_INSTALL,0);
    assert(!frames && !pending);submit_error=0;show(AM_UPDATE_SYSTEM_INSTALL,1);assert(frames==1);
    reset();ack_after=3;int result=setjmp(stop);if (!result) installer_entry();
    assert(result==1 && exit_status==0 && submitted==1 && checks==3);
    reset();result=setjmp(stop);if (!result) installer_entry();assert(result==2);
    reset();launch_error=-77;result=setjmp(stop);if (!result) installer_entry();assert(result==3);
    reset();promote_error=-44;result=setjmp(stop);if (!result) installer_entry();assert(result==4);
    puts("Installer: verified immediate/delayed flips, display error recovery, acknowledged launch, accepted-but-unstarted timeout, launch failure and install failure passed");return 0;
}
