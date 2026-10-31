/* PSVSD/AM CPU renderer, 960x544. GPL-3.0-or-later. */
#include "ui.h"
#include "touch.h"
#include <stdio.h>
#include <string.h>
#include "font_data.h"
#define COLOR(r,g,b) (0xff000000u|((b)<<16)|((g)<<8)|(r))
#define PANEL COLOR(15,28,46)
#define CARD COLOR(20,37,58)
#define BORDER COLOR(34,55,79)
#define TEXT COLOR(237,245,255)
#define MUTED COLOR(145,166,190)
#define ACCENT COLOR(70,220,199)
#define BAD COLOR(255,139,137)
static uint32_t *screen;
static void rect(int x,int y,int w,int h,uint32_t c) {
    if (x<0) { w+=x;x=0; }if (y<0) { h+=y;y=0; }
    if (x+w>960) w=960-x;
    if (y+h>544) h=544-y;
    for (int j=0;j<h;++j) for (int i=0;i<w;++i) screen[(y+j)*960+x+i]=c;
}
static void round_rect(int x,int y,int w,int h,int radius,uint32_t c) {
    if (w<=0 || h<=0) return;
    if (radius>w/2) radius=w/2;
    if (radius>h/2) radius=h/2;
    rect(x+radius,y,w-2*radius,h,c);rect(x,y+radius,w,h-2*radius,c);
    for (int dy=0;dy<radius;++dy) for (int dx=0;dx<radius;++dx)
        if ((radius-dx)*(radius-dx)+(radius-dy)*(radius-dy)<=radius*radius) {
            rect(x+dx,y+dy,1,1,c);rect(x+w-1-dx,y+dy,1,1,c);
            rect(x+dx,y+h-1-dy,1,1,c);rect(x+w-1-dx,y+h-1-dy,1,1,c);
        }
}
static int width(const char *s,int f) {
    int w=0;while (*s) { unsigned c=(unsigned char)*s++;if (c>=32 && c<=126) w+=ui_glyphs[ui_font_first[f]+c-32].advance; }return w;
}
static void text(int x,int y,int f,uint32_t c,const char *s) {
    for (;*s;++s) {
        unsigned code=(unsigned char)*s;if (code<32 || code>126) continue;
        const UiGlyph *g=&ui_glyphs[ui_font_first[f]+code-32];
        for (unsigned j=0;j<g->h;++j) for (unsigned i=0;i<g->w;++i) {
            int xx=x+g->x+(int)i,yy=y+g->y+(int)j;
            if (xx<0 || xx>=960 || yy<0 || yy>=544) continue;
            unsigned a=ui_font_pixels[g->offset+j*g->w+i];if (!a) continue;
            uint32_t old=screen[yy*960+xx];
            unsigned r=((c&255)*a+(old&255)*(255-a))/255;
            unsigned gg=(((c>>8)&255)*a+((old>>8)&255)*(255-a))/255;
            unsigned b=(((c>>16)&255)*a+((old>>16)&255)*(255-a))/255;
            screen[yy*960+xx]=COLOR(r,gg,b);
        }
        x+=g->advance;
    }
}
static const char *wrap_line(const char *s,char *out,size_t cap,int max_width,int font) {
    size_t n=0;int w=0;
    while (*s && *s!='\n' && n+1<cap) {
        unsigned c=(unsigned char)*s;int advance=c>=32&&c<=126 ? ui_glyphs[ui_font_first[font]+c-32].advance:0;
        if (n && w+advance>max_width) break;
        out[n++]=*s++;w+=advance;
    }
    if (*s=='\n') ++s;
    out[n]=0;return s;
}
static int wrapped(int x,int y,const char *s,int max_width,int rows,int font,uint32_t c) {
    for (int i=0;*s && i<rows;++i) { char line[256];s=wrap_line(s,line,sizeof(line),max_width,font);text(x,y,font,c,line);y+=font==1?29:24; }
    return y;
}
static void pill(int x,int y,const char *s,int good) {
    int w=width(s,0)+24;round_rect(x,y,w,28,9,good?COLOR(17,67,63):COLOR(43,54,71));
    text(x+12,y+2,0,good?ACCENT:MUTED,s);
}
static void title(const char *name,const char *description) {
    text(276,126,2,TEXT,name);text(278,169,0,MUTED,description);
}
static void label_value(int x,int y,const char *label,const char *value,uint32_t c) {
    text(x,y,0,MUTED,label);text(x,y+25,1,c,value);
}
static void button(int x,int y,int w,int h,const char *label,int enabled) {
    round_rect(x,y,w,h,9,enabled?COLOR(21,61,73):CARD);
    text(x+(w-width(label,0))/2,y+(h-24)/2,0,enabled?ACCENT:MUTED,label);
}
unsigned am_ui_log_pages(const char *s) {
    unsigned rows=0;while (*s) { char line[256];s=wrap_line(s,line,sizeof(line),620,0);++rows; }
    return rows?(rows+AM_LOG_ROWS-1)/AM_LOG_ROWS:1;
}
unsigned am_ui_update_pages(const char *s) {
    unsigned rows=0;
    while (*s) { char line[256];s=wrap_line(s,line,sizeof(line),600,0);++rows; }
    return rows?(rows+AM_UPDATE_ROWS-1)/AM_UPDATE_ROWS:1;
}
static void updates_view(const AmUi *m) {
    title("Updates","Official releases from RT-Online/PSVSD-AM");
    button(AM_UPDATE_X,AM_UPDATE_Y,AM_UPDATE_W,AM_UPDATE_H,"Check for Updates",!m->busy);
    round_rect(270,205,640,257,12,CARD);
    const AmUpdate *u=&m->update;
    const char *state=u->state==UPDATE_UNCHECKED?"Check when connected to Wi-Fi.":
        u->state==UPDATE_CHECKING?"Checking Wi-Fi and GitHub...":u->message;
    wrapped(290,220,state,600,2,1,u->state==UPDATE_ERROR?BAD:ACCENT);
    char info[128];
    snprintf(info,sizeof(info),"Installed: %s    Latest: %s",ACCESSORY_VERSION,u->version[0]?u->version:"Not checked");
    text(290,284,0,TEXT,info);
    if (u->notes[0]) {
        const char *s=u->notes;unsigned row=0;
        while (*s) {
            char line[256];s=wrap_line(s,line,sizeof(line),600,0);
            if (row/AM_UPDATE_ROWS==m->update_page) text(290,316+(int)(row%AM_UPDATE_ROWS)*24,0,TEXT,line);
            ++row;
        }
        snprintf(info,sizeof(info),"Release notes: page %u of %u",m->update_page+1,am_ui_update_pages(u->notes));
        text(290,441,0,MUTED,info);
    } else {
        wrapped(290,326,"Manual check only. Connect Wi-Fi in system settings.\nThis checks versions; it does not install updates.",600,3,0,MUTED);
    }
}
static void overview(const AmUi *m) {
    title("Accessory storage","Manage your internal SD card and boot settings.");
    round_rect(276,211,306,111,14,CARD);round_rect(598,211,306,111,14,CARD);
    const char *live=m->status.verified_mount?am_target_name(m->status.actual_target):m->status.legacy_loaded?"Reboot required":"Not mounted";
    label_value(296,229,"CURRENT USB STORAGE",live,m->status.verified_mount?ACCENT:TEXT);
    label_value(618,229,"SAVED BOOT SETTING",m->saved.enabled?"Enabled":"Disabled",TEXT);
    char target[72];snprintf(target,sizeof(target),"Target %s  |  %s",am_target_name(m->saved.target),m->reboot_pending?"restart pending":"on next boot");text(618,290,0,MUTED,target);
    round_rect(276,332,628,101,14,CARD);
    label_value(296,344,"GAMECARD",m->status.gamecard?"Accessible":"No accessible cartridge",m->status.gamecard?ACCENT:TEXT);
    text(296,404,0,MUTED,"The accessory mount leaves the gamecard paths unchanged.");
}
static void settings(const AmUi *m) {
    title("Mount setup","Save your choices, then reboot to apply them.");
    const char *names[]={"Automatic mounting","USB storage location"};
    const char *values[]={m->pending.enabled?"Enabled":"Disabled",m->pending.target==AM_UX0?"ux0 - Main storage":"uma0 - Extra storage"};
    for (int i=0;i<2;++i) {
        int y=214+i*86;uint32_t color=m->editing && m->field==i ? COLOR(23,61,74):CARD;
        round_rect(276,y,628,72,12,color);text(294,y+12,1,TEXT,names[i]);
        int x=888-width(values[i],1);text(x,y+13,1,ACCENT,values[i]);
        if (m->editing && m->field==i) rect(277,y+14,3,43,ACCENT);
    }
    wrapped(282,384,m->pending.target==AM_UX0?
       "Save copies and verifies PSVSD/AM and VitaShell on USB.\nCopy other apps and saves separately.":
       "uma0 keeps your current main storage.\nDisabling takes effect after reboot; manual tests remain available.",610,2,0,MUTED);
}
static void test_view(const AmUi *m) {
    title("Read / write test","A 4 MiB file is written, read back, checked, then removed.");
    round_rect(276,211,628,157,14,CARD);
    const char *label=m->busy?m->stage:m->test_attempted?(m->test_passed?"Verification passed":"Test needs attention"):"Ready when you are";
    text(296,230,1,m->test_attempted&&!m->test_passed?BAD:TEXT,label);
    char b[160];snprintf(b,sizeof(b),"Written %.2f MiB   |   Read %.2f MiB",m->test.written/1048576.0,m->test.read/1048576.0);text(296,277,0,MUTED,b);
    round_rect(296,322,588,8,4,BORDER);round_rect(296,322,(int)(588*m->progress/100),8,4,ACCENT);
    if (m->test_attempted && !m->busy) snprintf(b,sizeof(b),"CRC32 %08X   |   Result %08X",m->test.crc,(unsigned)m->test.result);
    else snprintf(b,sizeof(b),"%s",m->busy?"Verification in progress":"No verification result yet");
    text(282,372,0,MUTED,b);
    button(AM_TEST_X,AM_TEST_Y,AM_TEST_W,AM_TEST_H,"Run test",!m->busy);
    wrapped(282,414,"X verifies USB storage. Your live ux0 mapping is kept.",600,1,0,MUTED);
}
static void log_view(const AmUi *m) {
    char count[80];unsigned pages=am_ui_log_pages(m->log);snprintf(count,sizeof(count),"Page %u of %u   |   LEFT / RIGHT to browse",m->log_page+1,pages);
    int cleared=strstr(m->message,"Logs cleared")==m->message;
    int clear_failed=strstr(m->message,"Clear Log failed")==m->message;
    title("Diagnostic log",cleared||clear_failed?m->message:count);
    if (m->message[0]) {
        int good=cleared||strstr(m->message,"Report saved")!=NULL;
        const char *label=cleared?"Cleared":clear_failed?"Clear failed":good?"Saved":"Save failed";
        pill(766,164,label,good);
    }
    round_rect(270,205,640,257,12,CARD);
    const char *p=m->log;unsigned row=0;
    while (*p) {
        char line[256];p=wrap_line(p,line,sizeof(line),620,0);
        if (row/AM_LOG_ROWS==m->log_page) text(282,215+(int)(row%AM_LOG_ROWS)*24,0,TEXT,line);
        ++row;
    }
    if (!m->log[0]) text(292,234,1,MUTED,"No report yet. Run a test or refresh.");
}
static void credits(void) {
    title("Built together.","PSVSD/AM - Accessory Storage Mount");
    round_rect(276,211,628,249,14,CARD);
    text(298,224,1,TEXT,"RefurbedTech");
    text(298,255,0,MUTED,"Concept, hardware, testing and product direction");
    text(298,290,1,TEXT,"Yifan Lu / TheFloW");
    text(298,321,0,MUTED,"USBMC, VitaShell and the storage foundations");
    text(298,357,0,MUTED,"taiHEN / VitaSDK / YAMT / StorageMgr");
    text(298,386,0,MUTED,"Implementation assistance: OpenAI Codex");
    text(298,415,0,MUTED,"GPL-3.0-or-later | DejaVu Sans font license included");
}
static void action_view(const AmUi *m) {
    int reboot=m->nav==VIEW_REBOOT;
    title(reboot?"Reboot":"Exit PSVSD/AM","Choose whether to save your pending settings.");
    const char *choices[]={reboot?"Save and Reboot":"Save and Exit",
                           reboot?"Reboot without Save":"Exit without Save"};
    for (int i=0;i<AM_CHOICE_COUNT;++i) {
        int y=AM_CHOICE_Y+i*AM_CHOICE_STEP;
        int selected=m->editing && m->field==i;
        round_rect(AM_CHOICE_X,y,AM_CHOICE_W,AM_CHOICE_H,12,selected?COLOR(23,61,74):CARD);
        text(298,y+22,1,selected?ACCENT:TEXT,choices[i]);
        if (selected) rect(277,y+14,3,44,ACCENT);
    }
    char note[120];snprintf(note,sizeof(note),"%s | Pending: %s, %s",m->dirty?"Unsaved changes":"Settings unchanged",
                           m->pending.enabled?"Enabled":"Disabled",am_target_name(m->pending.target));
    text(282,407,0,MUTED,note);
}
static void clear_log_confirmation(const AmUi *m) {
    round_rect(258,154,666,300,18,BORDER);round_rect(260,156,662,296,16,PANEL);
    text(284,177,2,TEXT,"Are you sure?");
    wrapped(284,238,"Clear saved diagnostic logs and exported reports?",604,2,1,TEXT);
    text(284,310,0,MUTED,"Mount settings are kept. New events will still be logged.");
    const char *choices[]={"Clear Log","Cancel"};
    for (int i=0;i<2;++i) {
        int x=i?AM_CANCEL_X:AM_CONFIRM_X;
        round_rect(x,AM_CONFIRM_Y,AM_CONFIRM_W,AM_CONFIRM_H,12,m->clear_choice==i?COLOR(23,61,74):CARD);
        text(x+(AM_CONFIRM_W-width(choices[i],1))/2,AM_CONFIRM_Y+17,1,m->clear_choice==i?ACCENT:MUTED,choices[i]);
    }
}
static void copy_view(const AmUi *m) {
    const AmCopyProgress *p=&m->copy;char b[160];
    title("Preparing USB storage","Copying the PSVSD/AM and VitaShell app folders to uma0.");
    round_rect(270,205,640,257,12,CARD);
    text(290,224,1,ACCENT,p->phase);
    snprintf(b,sizeof(b),"%u%%",p->percent);text(884-width(b,2),220,2,TEXT,b);
    wrapped(290,275,p->path,596,3,0,TEXT);
    snprintf(b,sizeof(b),"Files completed: %u / %u",p->files_done,p->total_files);text(290,357,0,MUTED,b);
    snprintf(b,sizeof(b),"%.2f / %.2f MiB",p->bytes_done/1048576.0,p->total_bytes/1048576.0);text(290,386,1,TEXT,b);
    round_rect(290,434,594,10,5,BORDER);round_rect(290,434,(int)(594*(p->percent>100?100:p->percent)/100),10,5,ACCENT);
}
void am_ui_render(uint32_t *pixels,const AmUi *m) {
    screen=pixels;
    for (int y=0;y<544;++y) rect(0,y,960,1,COLOR(8+(544-y)/120,15+(544-y)/90,27+(544-y)/65));
    text(32,20,3,TEXT,"PSVSD");text(32+width("PSVSD",3),20,3,ACCENT,"/AM");
    text(34,75,0,MUTED,"Accessory Storage Mount");
    pill(816,36,"v" ACCESSORY_VERSION,0);rect(26,106,908,1,BORDER);
    const char *items[]={"Overview","Mount setup","Read / write test","Log","Updates","Credits","Reboot","Exit"};
    for (int i=0;i<AM_NAV_COUNT;++i) {
        int y=AM_NAV_Y(i);
        if (i==m->nav) { round_rect(24,y,218,38,10,COLOR(21,61,73));rect(25,y+10,3,18,ACCENT); }
        text(42,y+4,1,i==m->nav?TEXT:MUTED,items[i]);
        if (i==VIEW_SETTINGS && m->dirty) pill(194,y+4,"*",1);
    }
    rect(36,376,190,1,BORDER);
    round_rect(258,118,666,AM_CONTENT_BOTTOM-118,18,PANEL);
    if (m->copy_active) copy_view(m);
    else switch (m->nav) {
        case VIEW_SETTINGS:settings(m);break;
        case VIEW_TEST:test_view(m);break;
        case VIEW_LOG:log_view(m);break;
        case VIEW_UPDATES:updates_view(m);break;
        case VIEW_CREDITS:credits();break;
        case VIEW_REBOOT:case VIEW_EXIT:action_view(m);break;
        default:overview(m);break;
    }
    if (m->message[0] && m->nav!=VIEW_LOG && !m->copy_active) {
        round_rect(274,440,634,32,8,COLOR(23,43,63));
        wrapped(284,444,m->message,614,1,0,TEXT);
    }
    if (m->clear_confirm) clear_log_confirmation(m);
    rect(26,AM_FOOTER_Y,908,1,BORDER);
    text(32,501,0,MUTED,m->nav==VIEW_LOG?"Triangle: clear log":m->touch_available?"Touch + D-pad":"D-pad / X / O");
    if (m->nav==VIEW_UPDATES) {
        button(AM_PREV_X,AM_FOOTER_BUTTON_Y,AM_PAGE_W,AM_FOOTER_BUTTON_H,"Prev",!m->busy && m->update_page>0);
        button(AM_NEXT_X,AM_FOOTER_BUTTON_Y,AM_PAGE_W,AM_FOOTER_BUTTON_H,"Next",!m->busy && m->update_page+1<am_ui_update_pages(m->update.notes));
    }
    if (m->nav==VIEW_LOG) {
        button(AM_PREV_X,AM_FOOTER_BUTTON_Y,AM_PAGE_W,AM_FOOTER_BUTTON_H,"Prev",!m->clear_confirm && m->log_page>0);
        button(AM_NEXT_X,AM_FOOTER_BUTTON_Y,AM_PAGE_W,AM_FOOTER_BUTTON_H,"Next",!m->clear_confirm && m->log_page+1<am_ui_log_pages(m->log));
        button(AM_CLEAR_X,AM_FOOTER_BUTTON_Y,AM_CLEAR_W,AM_FOOTER_BUTTON_H,"Clear Log",!m->busy && !m->clear_confirm);
    }
    button(AM_REFRESH_X,AM_FOOTER_BUTTON_Y,AM_REFRESH_W,AM_FOOTER_BUTTON_H,m->nav==VIEW_LOG?"Export log":"Refresh",!m->busy && !m->clear_confirm);
    button(AM_BACK_X,AM_FOOTER_BUTTON_Y,AM_BACK_W,AM_FOOTER_BUTTON_H,"Back",!m->busy);
}
