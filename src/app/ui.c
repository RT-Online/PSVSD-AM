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
static void update_progress_view(const AmUi *m) {
    const AmUpdateProgress *p=&m->update_progress;
    const char *phase=p->phase==AM_UPDATE_VERIFY?"Verifying download":p->phase==AM_UPDATE_EXTRACT?"Extracting update":
        p->phase==AM_UPDATE_PREPARE?"Preparing installer":p->phase==AM_UPDATE_SYSTEM_INSTALL?"Installing update":
        p->phase==AM_UPDATE_REOPEN?"Opening PSVSD/AM":p->phase==AM_UPDATE_COMPLETE?"Update installed":p->phase==AM_UPDATE_INSTALL_ERROR?(m->update.stage[0]?m->update.stage:"Installation failed"):"Downloading update";
    round_rect(270,205,640,257,12,CARD);text(290,224,1,ACCENT,phase);
    unsigned done=p->total && p->done>p->total?p->total:p->done;
    char detail[160];
    if (p->total) {
        unsigned percent=(unsigned)((uint64_t)done*100/p->total);
        snprintf(detail,sizeof(detail),"%u%%",percent);text(884-width(detail,2),220,2,TEXT,detail);
        if (p->phase==AM_UPDATE_COMPLETE) snprintf(detail,sizeof(detail),"Reopening PSVSD/AM...");
        else if (p->phase==AM_UPDATE_EXTRACT) snprintf(detail,sizeof(detail),"Files: %u / %u",done,p->total);
        else snprintf(detail,sizeof(detail),"%.2f / %.2f MiB",done/1048576.0,p->total/1048576.0);
    } else if (p->phase==AM_UPDATE_INSTALL_ERROR) snprintf(detail,sizeof(detail),"Error: %08X",(unsigned)m->update.error);
    else if (p->phase==AM_UPDATE_DOWNLOAD) snprintf(detail,sizeof(detail),"%.2f MiB received - waiting for total size",done/1048576.0);
    else snprintf(detail,sizeof(detail),"Please wait...");
    text(290,295,0,TEXT,detail);
    wrapped(290,338,p->phase==AM_UPDATE_INSTALL_ERROR?(m->update.reason[0]?m->update.reason:"Press X to return to PSVSD/AM. Details are saved in the update log."):
        "Keep the system powered on.\nPSVSD/AM will reopen when finished.",594,3,0,MUTED);
    round_rect(290,420,594,12,5,BORDER);
    if (p->total) round_rect(290,420,(int)((uint64_t)594*done/p->total),12,5,ACCENT);
    else if (p->phase!=AM_UPDATE_INSTALL_ERROR) round_rect(290+(int)(m->update_animation%80)*6,420,114,12,5,ACCENT);
}
static void updates_view(const AmUi *m) {
    title("Updates","Official releases from RT-Online/PSVSD-AM");
    char action[64];snprintf(action,sizeof(action),"Update to %s",m->update.version);
    button(AM_UPDATE_X,AM_UPDATE_Y,AM_UPDATE_W,AM_UPDATE_H,
        m->update.state==UPDATE_INSTALLING?"Updating...":m->update.state==UPDATE_AVAILABLE && m->update.asset_url[0]?action:"Check for Updates",!m->busy && m->update.state!=UPDATE_CHECKING);
    if (m->update.state==UPDATE_INSTALLING) { update_progress_view(m);return; }
    round_rect(270,205,640,257,12,CARD);
    const AmUpdate *u=&m->update;
    const char *state=u->state==UPDATE_UNCHECKED?"Check for a newer public release.":
        u->state==UPDATE_OFFLINE?"Updates not checked.":
        u->state==UPDATE_CHECKING?"Checking Wi-Fi and GitHub...":u->message;
    int failed=u->state==UPDATE_ERROR || u->state==UPDATE_DATE_ERROR;
    wrapped(290,220,state,600,2,1,failed?BAD:ACCENT);
    char info[128];
    snprintf(info,sizeof(info),"Installed: %s    Latest: %s",ACCESSORY_VERSION,u->version[0]?u->version:"Not checked");
    text(290,284,0,TEXT,info);
    if (failed) {
        char diagnostic[640];am_update_diagnostic(u,diagnostic,sizeof(diagnostic));
        wrapped(290,312,diagnostic,600,6,0,TEXT);
    } else if (u->notes[0]) {
        const char *s=u->notes;unsigned row=0;
        while (*s) {
            char line[256];s=wrap_line(s,line,sizeof(line),600,0);
            if (row/AM_UPDATE_ROWS==m->update_page) text(290,316+(int)(row%AM_UPDATE_ROWS)*24,0,TEXT,line);
            ++row;
        }
        snprintf(info,sizeof(info),"Release notes: page %u of %u",m->update_page+1,am_ui_update_pages(u->notes));
        text(290,441,0,MUTED,info);
    } else {
        wrapped(290,326,"Checks quietly at launch when Wi-Fi is connected.\nInstallation is optional and requires confirmation.",600,3,0,MUTED);
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
static void speed_results(const AmSpeedResult *r) {
    const int read_col=513,write_col=702;
    char value[96];
    text(296,269,1,TEXT,"Speed Test results");
    snprintf(value,sizeof(value),"PSVSD %s",am_target_name(r->target));
    text(884-width(value,0),273,0,ACCENT,value);
    text(read_col,305,0,MUTED,"Read");
    text(write_col,305,0,MUTED,"Write");
    text(296,329,0,TEXT,"Sequential");
    text(296,353,0,TEXT,"Random 4 KiB");
    text(296,377,0,TEXT,"Random IOPS");
    const unsigned seq[]={AM_SPEED_SEQ_READ,AM_SPEED_SEQ_WRITE};
    const unsigned random[]={AM_SPEED_RANDOM_READ,AM_SPEED_RANDOM_WRITE};
    for (unsigned col=0;col<2;++col) {
        int x=col?write_col:read_col;
        snprintf(value,sizeof(value),"%.2f MiB/s",r->rates[seq[col]].mib_s);
        text(x,329,0,TEXT,value);
        snprintf(value,sizeof(value),"%.2f MiB/s",r->rates[random[col]].mib_s);
        text(x,353,0,TEXT,value);
        snprintf(value,sizeof(value),"%.0f",r->rates[random[col]].iops);
        text(x,377,0,TEXT,value);
    }
    text(296,413,0,MUTED,"Reads may be cached. Writes include a flush.");
}
static void test_view(const AmUi *m) {
    title("Read / write test","X runs the 4 MiB verification. Triangle starts Speed Test.");
    button(AM_TEST_X,AM_TEST_Y,AM_TEST_W,AM_TEST_H,"Verify",!m->busy);
    button(AM_SPEED_TEST_X,AM_TEST_Y,AM_TEST_W,AM_TEST_H,"Speed Test",!m->busy);
    char value[120];
    if (m->busy && !m->speed_active) {
        round_rect(276,211,628,157,14,CARD);
        text(296,230,1,TEXT,m->stage);
        snprintf(value,sizeof(value),"Written %.2f MiB   |   Read %.2f MiB",m->test.written/1048576.0,m->test.read/1048576.0);
        text(296,277,0,MUTED,value);
        round_rect(296,322,588,8,4,BORDER);
        round_rect(296,322,(int)(588*m->progress/100),8,4,ACCENT);
        return;
    }
    round_rect(276,204,628,45,12,CARD);
    text(296,215,0,TEXT,"4 MiB verification");
    if (m->test_attempted) {
        snprintf(value,sizeof(value),m->test_passed?"Passed   |   CRC32 %08X":"Failed   |   %08X",
                 m->test_passed?m->test.crc:(unsigned)m->test.result);
        text(884-width(value,0),215,0,m->test_passed?ACCENT:BAD,value);
    } else text(884-width("Ready",0),215,0,MUTED,"Ready");
    round_rect(276,260,628,178,12,CARD);
    if (m->speed_active) {
        const char *phases[]={"Sequential write","Sequential read","Random 4 KiB read","Random 4 KiB write"};
        unsigned percent=m->speed_progress;
        unsigned phase=percent/AM_SPEED_PHASE_PERCENT;
        text(296,274,1,ACCENT,phase<AM_SPEED_PHASE_COUNT?phases[phase]:"Preparing Speed Test");
        if (phase>=AM_SPEED_PHASE_COUNT) percent=0;
        snprintf(value,sizeof(value),"%u%%",percent);
        text(884-width(value,1),274,1,TEXT,value);
        round_rect(296,333,588,10,4,BORDER);
        round_rect(296,333,(int)(588*percent/100),10,4,ACCENT);
        text(296,378,0,MUTED,"Leave PSVSD/AM open until the test finishes.");
    } else if (m->speed_attempted && m->speed.result>=0) speed_results(&m->speed);
    else if (m->speed_attempted) {
        text(296,274,1,BAD,"Speed Test failed");
        snprintf(value,sizeof(value),"Error %08X. Open Log for details.",(unsigned)m->speed.result);
        text(296,325,0,MUTED,value);
    } else {
        text(296,274,1,TEXT,"Sequential and random I/O");
        snprintf(value,sizeof(value),"Speed Test uses a temporary %u MiB file on PSVSD.",AM_SPEED_FILE_MIB);
        text(296,325,0,MUTED,value);
    }
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
        round_rect(x,AM_CONFIRM_Y,AM_CONFIRM_W,AM_CONFIRM_H,12,i?CARD:COLOR(23,61,74));
        if (m->clear_choice==i) rect(x+12,AM_CONFIRM_Y+AM_CONFIRM_H-5,AM_CONFIRM_W-24,2,ACCENT);
        text(x+(AM_CONFIRM_W-width(choices[i],1))/2,AM_CONFIRM_Y+17,1,m->clear_choice==i?ACCENT:MUTED,choices[i]);
    }
}
static void update_date_notice(void) {
    round_rect(258,154,666,300,18,BORDER);round_rect(260,156,662,296,16,PANEL);
    text(284,177,2,TEXT,"Check the Vita's date and time");
    wrapped(284,246,"Open Settings > Date & Time > Date & Time Settings.\n"
        "Choose Set via Internet > Set Now, or set it manually.\n"
        "Return to PSVSD/AM and check for updates again.",604,3,0,TEXT);
    button(AM_DATE_OK_X,AM_CONFIRM_Y,AM_DATE_OK_W,AM_CONFIRM_H,"OK",1);
}
static void copy_view(const AmUi *m) {
    const AmCopyProgress *p=&m->copy;char b[160];
    title(m->copy.restoring?"Preparing original storage":"Preparing USB storage","Copying and verifying PSVSD/AM and VitaShell for the next ux0.");
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
    if (m->update_date_notice) update_date_notice();
    else if (m->clear_confirm) clear_log_confirmation(m);
    else if (m->update_confirm) {
        round_rect(258,154,666,300,18,BORDER);round_rect(260,156,662,296,16,PANEL);
        text(284,177,2,TEXT,"Install update?");
        char b[160];snprintf(b,sizeof(b),"Download and install %s? The app will close and reopen.",m->update.version);
        wrapped(284,238,b,604,3,1,TEXT);
        text(284,320,0,MUTED,"Saved mount settings are retained. No update is forced.");
        const char *labels[]={"Install Update","Cancel"};
        for (int i=0;i<2;++i) {
            int x=i?AM_CANCEL_X:AM_CONFIRM_X;
            round_rect(x,AM_CONFIRM_Y,AM_CONFIRM_W,AM_CONFIRM_H,12,i?CARD:COLOR(23,61,74));
            if (m->update_choice==i) rect(x+12,AM_CONFIRM_Y+AM_CONFIRM_H-5,AM_CONFIRM_W-24,2,ACCENT);
            text(x+(AM_CONFIRM_W-width(labels[i],1))/2,AM_CONFIRM_Y+17,1,m->update_choice==i?ACCENT:MUTED,labels[i]);
        }
    }
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
