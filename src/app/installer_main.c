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
#include "font_data.h"
static void show(const char *message) {
    void *base=NULL;int block=sceKernelAllocMemBlock("AMInstaller",SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,4*1024*1024,NULL);
    if (block<0 || sceKernelGetMemBlockBase(block,&base)<0) return;
    uint32_t *pixels=base;for (unsigned i=0;i<960*544;++i) pixels[i]=0xff25180d;
    int x=48,y=150;
    for (const unsigned char *p=(const unsigned char *)message;*p;++p) {
        if (*p=='\n') { x=48;y+=48;continue; }
        unsigned c=*p>=32 && *p<127?*p:'?';const UiGlyph *g=&ui_glyphs[ui_font_first[2]+c-32];
        if (x+g->advance>910) { x=48;y+=48; }
        for (unsigned row=0;row<g->h;++row) for (unsigned col=0;col<g->w;++col) {
            int px=x+g->x+(int)col,py=y+g->y+(int)row;
            if (px>=0 && px<960 && py>=0 && py<544 && ui_font_pixels[g->offset+row*g->w+col]>80) pixels[py*960+px]=0xffe8f5ed;
        }
        x+=g->advance;
    }
    SceDisplayFrameBuf frame={sizeof(frame),base,960,SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,960,544};sceDisplaySetFrameBuf(&frame,SCE_DISPLAY_SETBUF_NEXTFRAME);
}
int main(void) {
    show("PSVSD/AM Update\nInstalling verified package...\nPlease keep the system powered on.");
    sceAppMgrDestroyOtherApp();
    char expected[7]={0};FILE *f=fopen(AM_UPDATE_ROOT "/ready","rb");int ready=0;
    if (f) { ready=fread(expected,1,7,f)==6;fclose(f); }
    int result=-1;
    if (ready && am_package_validate(AM_UPDATE_STAGE,expected)==0) result=am_package_promote(AM_UPDATE_STAGE);
    remove(AM_UPDATE_ROOT "/ready");
    f=fopen(AM_UPDATE_ROOT "/result","wb");if (f) { fprintf(f,"%08X",(unsigned)result);fclose(f); }
    if (result<0) {
        char message[256];snprintf(message,sizeof(message),"PSVSD/AM Update\nInstallation failed: %08X\nPress X to return to PSVSD/AM.\nDetails are saved in the update log.",(unsigned)result);show(message);
        for (;;) { SceCtrlData pad;memset(&pad,0,sizeof(pad));sceCtrlPeekBufferPositive(0,&pad,1);if (pad.buttons&SCE_CTRL_CROSS) break;sceKernelDelayThread(20000); }
    }
    sceAppMgrLaunchAppByUri(0xFFFFF,"psgm:play?titleid=RTPSV0001");sceKernelExitProcess(result<0?1:0);return 0;
}
