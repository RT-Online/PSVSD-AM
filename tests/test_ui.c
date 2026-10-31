#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../src/app/ui.h"

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
        if (argc>1) { const char *names[]={"Overview","Settings","Test","Log","Credits","Reboot","Exit"};dump(argv[1],names[page]); }
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
    puts("ui: menu/log pages, render bounds, save choices, Circle/clear confirmation and busy copy/verification views passed");
    return 0;
}
