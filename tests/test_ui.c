#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../src/app/touch.h"

static struct { uint32_t before[16];uint32_t pixels[960*544];uint32_t after[16]; } buffer;
static void dump(const char *dir,const char *name) {
    char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);FILE *f=fopen(path,"wb");assert(f);
    fprintf(f,"P6\n960 544\n255\n");
    for (unsigned i=0;i<960*544;++i) {
        unsigned char rgb[]={buffer.pixels[i]&255,(buffer.pixels[i]>>8)&255,(buffer.pixels[i]>>16)&255};
        assert(fwrite(rgb,1,3,f)==3);
    }
    fclose(f);
}
int main(int argc,char **argv) {
    AmUi m;memset(&m,0,sizeof(m));m.status.ux0=m.status.uma0=m.status.raw_usb=1;
    m.status.actual_target=AM_UX0;m.status.verified_mount=m.status.gamecard=1;
    m.saved=(AmSettings){1,AM_UX0};m.pending=m.saved;m.touch_available=1;
    for (unsigned i=0;i<16;++i) buffer.before[i]=buffer.after[i]=0xDEADBEEF;
    strcpy(m.log,"PSVSD/AM " ACCESSORY_VERSION "\nSample diagnostic report\nSaved boot target: ux0:\nActual USB mount: ux0:\nApp-folder copy verification complete\n");
    for (int page=0;page<AM_NAV_COUNT;++page) {
        m.nav=page;am_ui_render(buffer.pixels,&m);
        for (unsigned i=0;i<16;++i) assert(buffer.before[i]==0xDEADBEEF && buffer.after[i]==0xDEADBEEF);
        if (argc>1) { const char *names[]={"Overview","Settings","Test","Log","Updates","Credits","Reboot","Exit"};dump(argv[1],names[page]); }
    }
    m.saved=(AmSettings){1,AM_UMA0};m.pending=m.saved;
    m.nav=VIEW_SETTINGS;assert(am_ui_input(&m,AM_CROSS)==ACTION_NONE && m.editing);
    assert(am_ui_input(&m,AM_LEFT)==ACTION_NONE && !m.pending.enabled && m.dirty && m.saved.enabled);
    assert(am_ui_input(&m,AM_DOWN)==ACTION_NONE && m.field==1);
    am_ui_input(&m,AM_RIGHT);assert(m.pending.target==AM_UX0 && m.saved.target==AM_UMA0);
    am_ui_input(&m,AM_CIRCLE);assert(!m.editing);
    am_ui_navigate(&m,VIEW_REBOOT);assert(am_ui_input(&m,AM_CROSS)==ACTION_NONE && m.editing);
    assert(am_ui_input(&m,AM_CROSS)==ACTION_SAVE_REBOOT);
    am_ui_input(&m,AM_DOWN);assert(am_ui_input(&m,AM_CROSS)==ACTION_REBOOT);
    am_ui_input(&m,AM_DOWN);assert(am_ui_input(&m,AM_CROSS)==ACTION_SAVE_REBOOT && m.field==0);
    am_ui_input(&m,AM_CIRCLE);assert(m.nav==VIEW_SETTINGS);
    am_ui_navigate(&m,VIEW_EXIT);am_ui_input(&m,AM_CROSS);
    assert(am_ui_input(&m,AM_CROSS)==ACTION_SAVE_EXIT);
    am_ui_input(&m,AM_DOWN);assert(am_ui_input(&m,AM_CROSS)==ACTION_EXIT);
    for (int view=0;view<AM_NAV_COUNT;++view) {
        am_ui_navigate(&m,view);assert(am_ui_input(&m,AM_CIRCLE)==ACTION_NONE);
        assert(m.dirty); /* Back preserves pending edits and never exits. */
    }
    m.busy=1;assert(am_ui_input(&m,AM_CROSS|AM_CIRCLE|AM_UP)==ACTION_NONE);m.busy=0;
    am_ui_navigate(&m,VIEW_LOG);
    assert(am_ui_input(&m,AM_TRIANGLE)==ACTION_NONE && m.clear_confirm && m.clear_choice==1);
    am_ui_render(buffer.pixels,&m);
    if (argc>1) dump(argv[1],"ClearLogConfirm");
    assert(am_ui_input(&m,AM_SQUARE)==ACTION_NONE && m.clear_confirm);
    assert(am_ui_input(&m,AM_CROSS)==ACTION_NONE && !m.clear_confirm && m.nav==VIEW_LOG);
    am_ui_input(&m,AM_TRIANGLE);assert(am_ui_input(&m,AM_CIRCLE)==ACTION_NONE && !m.clear_confirm && m.nav==VIEW_LOG);
    am_ui_input(&m,AM_TRIANGLE);am_ui_input(&m,AM_LEFT);
    assert(am_ui_input(&m,AM_CROSS)==ACTION_CLEAR_LOG && !m.clear_confirm);
    am_ui_navigate(&m,VIEW_OVERVIEW);assert(am_ui_input(&m,AM_TRIANGLE)==ACTION_NONE && !m.clear_confirm);
    am_ui_navigate(&m,VIEW_TEST);
    assert(am_ui_input(&m,AM_CROSS)==ACTION_TEST);
    assert(am_ui_input(&m,AM_TRIANGLE)==ACTION_SPEED_TEST);
    m.speed_active=m.busy=1;
    assert(am_ui_input(&m,AM_TRIANGLE|AM_CROSS|AM_CIRCLE)==ACTION_NONE && m.nav==VIEW_TEST);
    m.speed_progress=AM_SPEED_PREPARING;am_ui_render(buffer.pixels,&m);
    m.speed_progress=73;am_ui_render(buffer.pixels,&m);
    if (argc>1) dump(argv[1],"SpeedTestProgress");
    m.speed_active=m.busy=0;
    m.test_attempted=m.test_passed=1;m.test.crc=0x28AC71E8;
    m.speed_attempted=1;
    m.speed=(AmSpeedResult){.target=AM_UMA0,.rates={{8.40,0},{16.12,0},{2.08,532},{1.67,428}}};
    am_ui_render(buffer.pixels,&m);if (argc>1) dump(argv[1],"SpeedTestResults");
    m.speed.result=-1;am_ui_render(buffer.pixels,&m);
    for (unsigned i=0;i<15000;++i) m.log[i]=(i%150==149)?'\n':'A';
    m.log[15000]=0;
    unsigned count=am_ui_log_pages(m.log);assert(count>1);m.nav=VIEW_LOG;
    for (m.log_page=0;m.log_page<count;++m.log_page) am_ui_render(buffer.pixels,&m);
    m.log_page=count-1;am_ui_input(&m,AM_RIGHT);assert(m.log_page==count-1);
    for (int view=VIEW_REBOOT;view<=VIEW_EXIT;++view) {
        m.nav=view;m.editing=1;
        for (m.field=0;m.field<2;++m.field) am_ui_render(buffer.pixels,&m);
    }
    m.nav=VIEW_REBOOT;m.copy_active=1;m.busy=1;m.clear_confirm=0;
    m.copy=(AmCopyProgress){.bytes_done=5*1024*1024,.total_bytes=12*1024*1024,
        .files_done=8,.total_files=20,.percent=19};
    strcpy(m.copy.phase,"Copying app files");strcpy(m.copy.path,"VITASHELL/resources/theme/default.png");
    am_ui_render(buffer.pixels,&m);if (argc>1) dump(argv[1],"CopyingApps");
    assert(am_ui_input(&m,AM_CROSS|AM_CIRCLE|AM_TRIANGLE|AM_DOWN)==ACTION_NONE);
    m.copy.bytes_done=m.copy.total_bytes;m.copy.files_done=m.copy.total_files;m.copy.percent=99;
    strcpy(m.copy.phase,"Verifying installed files");am_ui_render(buffer.pixels,&m);
    if (argc>1) dump(argv[1],"VerifyingApps");
    for (unsigned i=0;i<16;++i) assert(buffer.before[i]==0xDEADBEEF && buffer.after[i]==0xDEADBEEF);
    am_ui_navigate(&m,VIEW_UPDATES);m.busy=0;m.copy_active=0;
    assert(am_ui_input(&m,AM_CROSS)==ACTION_CHECK_UPDATES);
    m.update.state=UPDATE_OFFLINE;m.update.message[0]=0;
    am_ui_render(buffer.pixels,&m);if (argc>1) dump(argv[1],"UpdatesOffline");
    m.update.state=UPDATE_ERROR;m.update.error=60;m.update.tls_verify=0x20;
    strcpy(m.update.stage,"Send HTTPS request");strcpy(m.update.message,"Could not check GitHub. See Log.");
    am_ui_render(buffer.pixels,&m);if (argc>1) dump(argv[1],"UpdatesError");
    m.update.state=UPDATE_DATE_ERROR;m.update_date_notice=1;
    strcpy(m.update.message,"Check the Vita's date and time.");
    am_ui_render(buffer.pixels,&m);if (argc>1) dump(argv[1],"UpdateDateNotice");
    assert(am_ui_input(&m,AM_DOWN|AM_SQUARE)==ACTION_NONE && m.update_date_notice && m.nav==VIEW_UPDATES);
    assert(am_ui_input(&m,AM_CROSS)==ACTION_NONE && !m.update_date_notice);
    assert(am_ui_input(&m,AM_CROSS)==ACTION_CHECK_UPDATES);
    m.update_date_notice=1;
    assert(am_ui_input(&m,AM_CIRCLE)==ACTION_NONE && !m.update_date_notice && m.nav==VIEW_UPDATES);
    m.update.state=UPDATE_AVAILABLE;strcpy(m.update.version,"v1.0.4");
    strcpy(m.update.message,"A newer release is available.");
    for (unsigned i=0;i<3000;++i) m.update.notes[i]=i%60==59?'\n':'A';
    m.update.notes[3000]=0;
    am_ui_input(&m,AM_RIGHT);assert(m.update_scroll==AM_UPDATE_NOTE_LINE);
    am_ui_input(&m,AM_RIGHT);assert(m.update_scroll==2*AM_UPDATE_NOTE_LINE);
    am_ui_render(buffer.pixels,&m);
    assert(am_ui_scroll_updates(&m,INT_MAX) && m.update_scroll>2*AM_UPDATE_NOTE_LINE);
    assert(!am_ui_scroll_updates(&m,INT_MAX));
    am_ui_render(buffer.pixels,&m);
    assert(am_ui_scroll_updates(&m,INT_MIN) && !m.update_scroll);
    am_ui_input(&m,AM_LEFT);assert(!m.update_scroll);
    assert(am_ui_input(&m,AM_TRIANGLE)==ACTION_TOGGLE_AUTO_UPDATE);
    for (unsigned i=0;i<16;++i) assert(buffer.before[i]==0xDEADBEEF && buffer.after[i]==0xDEADBEEF);
    memset(&m,0,sizeof(m));m.nav=VIEW_UPDATES;m.update.state=UPDATE_AVAILABLE;
    strcpy(m.update.version,"v1.2.3");strcpy(m.update.asset_url,"https://github.com/RT-Online/PSVSD-AM/releases/download/v1.2.3/app.vpk");
    assert(am_ui_input(&m,AM_CROSS)==ACTION_CHECK_UPDATES && !m.update_confirm);
    m.nav=VIEW_OVERVIEW;
    am_ui_render(buffer.pixels,&m);
    unsigned banner_pixel=(AM_UPDATE_NOTICE_Y+8)*960+AM_UPDATE_NOTICE_X+12;
    uint32_t background=buffer.pixels[banner_pixel];
    memset(m.update.sha256,'a',64);
    am_ui_render(buffer.pixels,&m);assert(buffer.pixels[banner_pixel]!=background);
    if (argc>1) dump(argv[1],"UpdateNotice");
    assert(!am_ui_auto_update_ready(&m));
    m.auto_update=1;assert(am_ui_auto_update_ready(&m));
    m.dirty=1;assert(!am_ui_auto_update_ready(&m));m.dirty=0;
    m.editing=1;assert(!am_ui_auto_update_ready(&m));m.editing=0;
    m.reboot_pending=1;assert(!am_ui_auto_update_ready(&m));m.reboot_pending=0;
    m.busy=1;assert(!am_ui_auto_update_ready(&m));m.busy=0;
    m.nav=VIEW_REBOOT;assert(!am_ui_auto_update_ready(&m));
    m.nav=VIEW_UPDATES;
    assert(am_ui_input(&m,AM_CROSS)==ACTION_NONE && m.update_confirm && m.update_choice==1);
    assert(!am_ui_auto_update_ready(&m));
    assert(am_ui_input(&m,AM_TRIANGLE)==ACTION_NONE && m.update_confirm);
    am_ui_render(buffer.pixels,&m);if (argc>1) dump(argv[1],"UpdateConfirm");
    assert(am_ui_input(&m,AM_CROSS)==ACTION_NONE && !m.update_confirm);
    am_ui_input(&m,AM_CROSS);am_ui_input(&m,AM_LEFT);
    assert(am_ui_input(&m,AM_CROSS)==ACTION_INSTALL_UPDATE && !m.update_confirm);
    am_ui_input(&m,AM_CROSS);assert(am_ui_input(&m,AM_CIRCLE)==ACTION_NONE && !m.update_confirm);
    m.update_confirm=0;m.update.state=UPDATE_INSTALLING;m.busy=1;
    for (unsigned phase=AM_UPDATE_DOWNLOAD;phase<=AM_UPDATE_INSTALL_ERROR;++phase) {
        m.update_progress=(AmUpdateProgress){phase,512*1024,1024*1024};m.update_animation=79;
        am_ui_render(buffer.pixels,&m);
    }
    m.update_progress=(AmUpdateProgress){AM_UPDATE_DOWNLOAD,512*1024,1024*1024};
    am_ui_render(buffer.pixels,&m);if (argc>1) dump(argv[1],"UpdateDownloadProgress");
    m.update_progress=(AmUpdateProgress){AM_UPDATE_DOWNLOAD,512*1024,0};
    am_ui_render(buffer.pixels,&m);if (argc>1) dump(argv[1],"UpdateUnknownSize");
    m.update_progress=(AmUpdateProgress){AM_UPDATE_EXTRACT,7,12};
    am_ui_render(buffer.pixels,&m);if (argc>1) dump(argv[1],"UpdateExtractProgress");
    m.update_progress=(AmUpdateProgress){AM_UPDATE_SYSTEM_INSTALL,0,0};
    am_ui_render(buffer.pixels,&m);if (argc>1) dump(argv[1],"InstallerSharedUI");
    m.update_progress=(AmUpdateProgress){AM_UPDATE_COMPLETE,1,1};
    am_ui_render(buffer.pixels,&m);if (argc>1) dump(argv[1],"InstallerComplete");
    m.update_progress=(AmUpdateProgress){AM_UPDATE_REOPEN,0,0};
    am_ui_render(buffer.pixels,&m);if (argc>1) dump(argv[1],"InstallerOpeningApp");
    m.update_progress=(AmUpdateProgress){AM_UPDATE_INSTALL_ERROR,0,0};m.update.error=-123;
    strcpy(m.update.stage,"Could not reopen PSVSD/AM");
    strcpy(m.update.reason,"Launch request failed.\nPress X to retry opening PSVSD/AM.");
    am_ui_render(buffer.pixels,&m);if (argc>1) dump(argv[1],"InstallerRelaunchError");
    m.update_progress=(AmUpdateProgress){AM_UPDATE_VERIFY,0xffffffffu,1};am_ui_render(buffer.pixels,&m);
    for (unsigned i=0;i<16;++i) assert(buffer.before[i]==0xDEADBEEF && buffer.after[i]==0xDEADBEEF);
    puts("ui: release note scroll, availability badge, automatic install guards, render bounds and confirmations passed");
    return 0;
}
