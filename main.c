/* PSVSD/AM application. GPL-3.0-or-later. */
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/touch.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "session.h"
#include "touch.h"

static AmUi model;
static AccessorySession session;
static uint32_t *framebuffers[2];
static unsigned front;
static int redraw=1;
static AmTouchState touch_state;
static AmTouchBounds touch_bounds;
static SceTouchSamplingState previous_touch_state;
static int previous_touch_valid,front_sampling_started;

static void draw(void) {
    unsigned next=front^1u;am_ui_render(framebuffers[next],&model);
    SceDisplayFrameBuf f={sizeof(f),framebuffers[next],960,SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,960,544};
    sceDisplaySetFrameBuf(&f,SCE_DISPLAY_SETBUF_NEXTFRAME);sceDisplayWaitVblankStart();front=next;redraw=0;
}
static void query_status(void) {
    am_status(&model.status);
    if (model.status.loaded) {
        uint32_t token=sceKernelGetProcessTimeLow();if (!token) token=1;
        char old[128];unsigned old_token=0;
        if (am_read_text(ACCESSORY_KERNEL_DIR "/runtime.txt",old,sizeof(old))>=0 &&
            sscanf(old,"PSVSD/AM " ACCESSORY_VERSION "\nRequest: %X",&old_token)==1 && token==old_token) {
            ++token;if (!token) token=1;
        }
        AccessoryQuery q={-1,-1,-1,-1};int stage=am_stage_status(),ret=stage;
        if (ret>=0) ret=accessory_status_query(token,&q);
        am_expect_query(ret>=0?token:0);
        am_log("Mount query: request=%08X stage=%08X load=%08X start=%08X entry=%08X unload=%08X result=%08X\n",
               token,(unsigned)stage,(unsigned)q.load,(unsigned)q.start,(unsigned)q.entry,(unsigned)q.unload,(unsigned)ret);
        am_status(&model.status);
    }
}
static void refresh_log(void) {
    char part[8192];model.log[0]=0;
    snprintf(model.log,sizeof(model.log),"PSVSD/AM " ACCESSORY_VERSION "\n\n");
    const char *paths[]={ACCESSORY_KERNEL_LOG,AM_APP_LOG};
    for (unsigned i=0;i<2;++i) if (am_read_log(paths[i],part,sizeof(part))>=0) {
        size_t used=strlen(model.log);snprintf(model.log+used,sizeof(model.log)-used,"%s\n%s\n",i?"APPLICATION":"KERNEL",part);
    }
    unsigned pages=am_ui_log_pages(model.log);if (model.log_page>=pages) model.log_page=pages-1;
}
static void progress(unsigned percent,const char *stage,void *ctx) {
    (void)ctx;model.progress=percent;snprintf(model.stage,sizeof(model.stage),"%s",stage);draw();
}
static void copy_progress(const AmCopyProgress *p,void *ctx) {
    uint32_t *last=ctx,now=sceKernelGetProcessTimeLow();
    int changed=strcmp(model.copy.phase,p->phase)!=0;
    model.copy_active=1;model.copy=*p;
    sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
    if (changed || p->percent==100 || now-*last>=100000) { draw();*last=now; }
}
static int request_usb(void *ctx,char *message,size_t cap) {
    (void)ctx;model.copy_active=1;memset(&model.copy,0,sizeof(model.copy));
    snprintf(model.copy.phase,sizeof(model.copy.phase),"Detecting USB card");
    snprintf(model.copy.path,sizeof(model.copy.path),"Preparing uma0 for the app copy");draw();
    am_status(&model.status);
    if (model.status.legacy_loaded) {
        snprintf(message,cap,"An older USB test is resident. Reboot before saving.");return -1;
    }
    if (!model.status.loaded && !session.attempted) {
        accessory_session_run(&session);
        if (session.log_result<0) am_log("Session report write failed: %08X\n",(unsigned)session.log_result);
        am_log("Save preparation probe: stage=%08X load=%08X start=%08X entry=%08X\n",
               (unsigned)session.stage_result,(unsigned)session.load_result,(unsigned)session.start_result,(unsigned)session.entry_result);
    }
    query_status();
    if (!model.status.verified_mount || model.status.actual_target!=AM_UMA0) {
        snprintf(message,cap,"USB card could not be verified at uma0. See Log.");return -1;
    }
    return 0;
}
static int save(void) {
    model.busy=1;model.copy_active=0;model.message[0]=0;memset(&model.copy,0,sizeof(model.copy));draw();
    query_status();
    uint32_t last=0;
    AmSaveCallbacks callbacks={copy_progress,request_usb,&last};
    int r=am_save_settings_with_callbacks(&model.pending,&callbacks,model.message,sizeof(model.message));
    model.copy_active=0;model.busy=0;
    if (r>=0) { model.saved=model.pending;model.dirty=0;model.reboot_pending=1; }
    char exported[192];am_export_report(NULL,exported,sizeof(exported));
    refresh_log();redraw=1;
    return r;
}
static void test(void) {
    model.busy=1;model.test_attempted=0;model.progress=0;
    memset(&model.test,0,sizeof(model.test));model.test.result=-1;
    snprintf(model.stage,sizeof(model.stage),"Checking accessory storage");draw();
    am_status(&model.status);
    if (model.status.legacy_loaded) {
        snprintf(model.message,sizeof(model.message),"An older test is resident. Reboot before testing.");goto finish;
    }
    if (!model.status.loaded) {
        if (session.attempted) {
            snprintf(model.message,sizeof(model.message),"Probe already requested. Export Log, then reboot.");goto finish;
        }
        accessory_session_run(&session);
        if (session.log_result<0) am_log("Session report write failed: %08X\n",(unsigned)session.log_result);
        am_log("Manual probe: stage=%08X load=%08X start=%08X entry=%08X\n",
               (unsigned)session.stage_result,(unsigned)session.load_result,(unsigned)session.start_result,(unsigned)session.entry_result);
        am_status(&model.status);
    }
    query_status();
    if (!model.status.verified_mount) {
        snprintf(model.message,sizeof(model.message),"USB mount not verified. Open Log and export the report.");goto finish;
    }
    model.test_attempted=1;
    int ret=am_test_io(model.status.actual_target,&model.test,progress,NULL);
    model.test_passed=ret>=0;
    snprintf(model.message,sizeof(model.message),ret>=0?"4 MiB read/write verification passed.":"Read/write test failed (%08X). See Log.",(unsigned)ret);
finish:
    model.busy=0;refresh_log();
    char note[192];am_export_report(session.attempted?session.text:NULL,note,sizeof(note));
    redraw=1;
}
static int init_display(void) {
    SceUID block=sceKernelAllocMemBlock("PSVSDAMDisplay",SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,4*1024*1024,NULL);
    if (block<0) return block;
    int ret=sceKernelGetMemBlockBase(block,(void **)&framebuffers[0]);if (ret<0) return ret;
    framebuffers[1]=framebuffers[0]+(2*1024*1024/sizeof(uint32_t));
    memset(framebuffers[0],0,4*1024*1024);
    SceDisplayFrameBuf f={sizeof(f),framebuffers[0],960,SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,960,544};
    return sceDisplaySetFrameBuf(&f,SCE_DISPLAY_SETBUF_NEXTFRAME);
}
int main(void) {
    if (init_display()<0) return 1;
    int loaded=am_load_settings(&model.saved);model.pending=model.saved;
    if (loaded<0) snprintf(model.message,sizeof(model.message),"Settings unreadable; automatic mounting defaults off.");
    query_status();refresh_log();
    am_log("Opened PSVSD/AM " ACCESSORY_VERSION "; saved enabled=%d target=%s\n",model.saved.enabled,am_target_name(model.saved.target));
    SceTouchPanelInfo panel;memset(&panel,0,sizeof(panel));
    previous_touch_valid=sceTouchGetSamplingState(SCE_TOUCH_PORT_FRONT,&previous_touch_state)>=0;
    int touch_start=sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_START);
    front_sampling_started=touch_start>=0;
    int touch_info=sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT,&panel);
    touch_bounds=(AmTouchBounds){panel.minDispX,panel.minDispY,panel.maxDispX,panel.maxDispY};
    model.touch_available=front_sampling_started && touch_info>=0 && panel.maxDispX>panel.minDispX && panel.maxDispY>panel.minDispY;
    am_touch_block(&touch_state);
    am_log("Front touch: sampling=%08X panel=%08X bounds=%d,%d..%d,%d enabled=%d\n",
           (unsigned)touch_start,(unsigned)touch_info,panel.minDispX,panel.minDispY,panel.maxDispX,panel.maxDispY,model.touch_available);
    unsigned previous=0,ticks=0;int exiting=0;
    while (!exiting) {
        if (redraw) draw();
        SceCtrlData pad;memset(&pad,0,sizeof(pad));sceCtrlPeekBufferPositive(0,&pad,1);
        unsigned pressed=pad.buttons&~previous;previous=pad.buttons;
        unsigned keys=0;
        if (pressed&SCE_CTRL_UP) keys|=AM_UP;
        if (pressed&SCE_CTRL_DOWN) keys|=AM_DOWN;
        if (pressed&SCE_CTRL_LEFT) keys|=AM_LEFT;
        if (pressed&SCE_CTRL_RIGHT) keys|=AM_RIGHT;
        if (pressed&SCE_CTRL_CROSS) keys|=AM_CROSS;
        if (pressed&SCE_CTRL_CIRCLE) keys|=AM_CIRCLE;
        if (pressed&SCE_CTRL_SQUARE) keys|=AM_SQUARE;
        if (pressed&SCE_CTRL_TRIANGLE) keys|=AM_TRIANGLE;
        if (keys) redraw=1;
        int action=am_ui_input(&model,keys);
        if (keys) am_touch_block(&touch_state);
        else if (model.touch_available) {
            SceTouchData data;memset(&data,0,sizeof(data));
            int read=sceTouchPeek(SCE_TOUCH_PORT_FRONT,&data,1);
            if (read<0) am_touch_block(&touch_state);
            else if (read>0) {
                int x=0,y=0,contacts=(int)data.reportNum,id=0;
                if (contacts==1) {
                    id=data.report[0].id;
                    if (!am_touch_point(&touch_bounds,data.report[0].x,data.report[0].y,&x,&y)) contacts=-1;
                }
                int active=touch_state.active;
                action=am_touch_sample(&touch_state,&model,contacts,id,x,y);
                if (active!=touch_state.active || !contacts) redraw=redraw || active;
            }
        }
        if (action!=ACTION_NONE) redraw=1;
        if (action==ACTION_TEST) test();
        else if (action==ACTION_LOG) refresh_log();
        else if (action==ACTION_CLEAR_LOG) {
            am_clear_logs(model.message,sizeof(model.message));accessory_session_clear_log(&session);
            model.log_page=0;refresh_log();
        }
        else if (action==ACTION_EXPORT) {
            query_status();if (session.attempted) accessory_session_refresh(&session);
            refresh_log();am_export_report(NULL,model.message,sizeof(model.message));
        }
        if (action==ACTION_SAVE_REBOOT || action==ACTION_SAVE_EXIT) {
            int reboot=action==ACTION_SAVE_REBOOT;
            action=save()>=0?(reboot?ACTION_REBOOT:ACTION_EXIT):ACTION_NONE;
            am_touch_block(&touch_state);
        }
        if (action==ACTION_REBOOT) {
            am_log("User requested reboot.\n");
            int r=scePowerRequestColdReset();
            snprintf(model.message,sizeof(model.message),"Reboot request: %08X",(unsigned)r);
        } else if (action==ACTION_EXIT) exiting=1;
        if (action!=ACTION_NONE) am_touch_block(&touch_state);
        if (++ticks%25==0) { am_status(&model.status);redraw=1; }
        sceKernelDelayThread(20000);
    }
    if (front_sampling_started) sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,
                         previous_touch_valid?previous_touch_state:SCE_TOUCH_SAMPLING_STATE_STOP);
    am_log("Exit. Unsaved=%d\n",model.dirty);sceKernelExitProcess(0);return 0;
}
