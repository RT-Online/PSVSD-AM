/* Settings, bounded file verification and diagnostic export. GPL-3.0-or-later. */
#include "platform.h"
#include "manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#define ENOENT_VALUE ((int)0x80010002)
#define EEXIST_VALUE ((int)0x80010011)
#define IO_ERROR ((int)0x80010005)
#define MAX_CONFIG 65536
#define BEGIN_MARK "# BEGIN PSVSD/AM"
#define END_MARK "# END PSVSD/AM"
static uint32_t expected_query;
void am_expect_query(uint32_t token) { expected_query=token; }

static int directory(const char *p) { int fd=sceIoDopen(p);if (fd<0) return 0;sceIoDclose(fd);return 1; }
static int present(const char *p) { SceIoStat st;return sceIoGetstat(p,&st)>=0; }
static int raw_present(const char *p) { int fd=sceIoOpen(p,SCE_O_RDONLY,0);if (fd<0) return 0;sceIoClose(fd);return 1; }
static int mkdir_ok(const char *p) { int r=sceIoMkdir(p,0777);return r==EEXIST_VALUE ? 0:r; }
static int write_all(int fd,const void *p,size_t n) {
    const unsigned char *b=p;
    while (n) { int r=sceIoWrite(fd,b,n);if (r<=0) return r<0?r:IO_ERROR;b+=r;n-=r; }
    return 0;
}
static int read_all(int fd,void *p,size_t n) {
    unsigned char *b=p;
    while (n) { int r=sceIoRead(fd,b,n);if (r<=0) return r<0?r:IO_ERROR;b+=r;n-=r; }
    return 0;
}
int am_read_text(const char *path,char *out,size_t cap) {
    if (!cap) return IO_ERROR;
    int fd=sceIoOpen(path,SCE_O_RDONLY,0);if (fd<0) { out[0]=0;return fd; }
    size_t used=0;int r=0;
    while (used+1<cap && (r=sceIoRead(fd,out+used,cap-1-used))>0) used+=r;
    if (r>=0 && used+1==cap) { char extra;int e=sceIoRead(fd,&extra,1);if (e!=0) r=e<0?e:IO_ERROR; }
    int closed=sceIoClose(fd);out[used]=0;
    return r<0?r:closed<0?closed:(int)used;
}
int am_read_log(const char *path,char *out,size_t cap) {
    if (cap<2) return IO_ERROR;
    SceIoStat st;int ret=sceIoGetstat(path,&st);if (ret<0) { out[0]=0;return ret; }
    int fd=sceIoOpen(path,SCE_O_RDONLY,0);if (fd<0) { out[0]=0;return fd; }
    int tail=st.st_size>=cap;
    if (tail && sceIoLseek(fd,-(int64_t)(cap-1),SCE_SEEK_END)<0) { sceIoClose(fd);out[0]=0;return IO_ERROR; }
    size_t used=0;
    while (used+1<cap && (ret=sceIoRead(fd,out+used,cap-1-used))>0) used+=ret;
    sceIoClose(fd);out[used]=0;
    if (tail) { char *p=strchr(out,'\n');if (p) { size_t n=strlen(p+1);memmove(out,p+1,n+1);used=n; } }
    return ret<0?ret:(int)used;
}
static int read_bytes(const char *path,unsigned char **out,size_t *size) {
    SceIoStat st;int ret=sceIoGetstat(path,&st);if (ret<0) return ret;
    if (st.st_size>1024*1024) return IO_ERROR;
    *size=(size_t)st.st_size;*out=malloc(*size+1);if (!*out) return IO_ERROR;
    int fd=sceIoOpen(path,SCE_O_RDONLY,0);if (fd<0) { free(*out);*out=NULL;return fd; }
    size_t off=0;
    while (off<*size) { ret=sceIoRead(fd,*out+off,*size-off);if (ret<=0) break;off+=ret; }
    int closed=sceIoClose(fd);
    if (off!=*size || closed<0) { free(*out);*out=NULL;return ret<0?ret:closed<0?closed:IO_ERROR; }
    (*out)[*size]=0;return 0;
}
static int create_file(const char *path,const void *bytes,size_t size,int exclusive) {
    int fd=sceIoOpen(path,SCE_O_WRONLY|SCE_O_CREAT|(exclusive?SCE_O_EXCL:SCE_O_TRUNC),0666);
    if (fd<0) return fd;
    int ret=write_all(fd,bytes,size),closed=sceIoClose(fd);
    return ret<0?ret:closed;
}
int am_replace_file(const char *path,const void *bytes,size_t size) {
    char temp[192],swap[192],backup[192];
    if (snprintf(temp,sizeof(temp),"%s.psvsdam.new",path)>=(int)sizeof(temp) ||
        snprintf(swap,sizeof(swap),"%s.psvsdam.swap",path)>=(int)sizeof(swap) ||
        snprintf(backup,sizeof(backup),"%s.psvsdam.bak",path)>=(int)sizeof(backup)) return IO_ERROR;
    if (present(swap)) { am_log("Unfinished save retained: %s\n",swap);return IO_ERROR; }
    int ret=create_file(temp,bytes,size,0);if (ret<0) { sceIoRemove(temp);return ret; }
    unsigned char *check=NULL;size_t n=0;ret=read_bytes(temp,&check,&n);
    if (ret>=0 && (n!=size || memcmp(check,bytes,size))) ret=IO_ERROR;
    free(check);if (ret<0) { sceIoRemove(temp);return ret; }
    int had=present(path);
    if (had) {
        unsigned char *old=NULL;size_t old_size=0;ret=read_bytes(path,&old,&old_size);
        if (ret>=0 && !present(backup)) ret=create_file(backup,old,old_size,1);
        free(old);if (ret<0) { sceIoRemove(temp);return ret; }
        ret=sceIoRename(path,swap);if (ret<0) { sceIoRemove(temp);return ret; }
    }
    ret=sceIoRename(temp,path);
    if (ret<0) {
        int restore=had?sceIoRename(swap,path):0;
        am_log("Replace failed %s: %08X; restore=%08X\n",path,(unsigned)ret,(unsigned)restore);
        sceIoRemove(temp);return ret;
    }
    if (had) sceIoRemove(swap);
    return 0;
}
void am_log(const char *fmt,...) {
    mkdir_ok("ur0:data");mkdir_ok(ACCESSORY_KERNEL_DIR);
    int fd=sceIoOpen(AM_APP_LOG,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_APPEND,0666);if (fd<0) return;
    char b[512];va_list ap;va_start(ap,fmt);int n=vsnprintf(b,sizeof(b),fmt,ap);va_end(ap);
    if (n>0) write_all(fd,b,(size_t)n<sizeof(b)?(size_t)n:sizeof(b)-1);
    sceIoClose(fd);
}
int am_load_settings(AmSettings *s) {
    *s=(AmSettings){0,AM_UMA0};char text[256];int r=am_read_text(AM_SETTINGS_PATH,text,sizeof(text));
    if (r==ENOENT_VALUE) r=am_read_text(AM_LEGACY_SETTINGS_PATH,text,sizeof(text));
    if (r==ENOENT_VALUE) return 0;
    return r<0?r:am_settings_parse(text,s);
}
static int append_text(char *out,size_t cap,size_t *used,const char *p,size_t n) {
    if (*used+n>=cap) return -1;
    memcpy(out+*used,p,n);*used+=n;out[*used]=0;return 0;
}
static int line_equal(const char *p,size_t n,const char *q) { return n==strlen(q) && !memcmp(p,q,n); }
int am_tai_transform(const char *input,int enabled,char *out,size_t cap) {
    if (!cap) return -1;
    size_t used=0;int inside=0,found=0,inserted=0;out[0]=0;
    for (const char *p=input;*p;) {
        const char *e=strchr(p,'\n');size_t original=e?(size_t)(e-p)+1:strlen(p),n=original;
        while (n && (p[n-1]=='\n'||p[n-1]=='\r'||p[n-1]==' '||p[n-1]=='\t')) --n;
        const char *trim=p;while (n && (*trim==' '||*trim=='\t')) { ++trim;--n; }
        if (line_equal(trim,n,BEGIN_MARK)) { if (inside||found) return -1;inside=found=1; }
        else if (line_equal(trim,n,END_MARK)) { if (!inside) return -1;inside=0; }
        else if (inside && n && *trim!='#' && !line_equal(trim,n,AM_BOOT_PLUGIN)) return -1;
        else if (!inside && !line_equal(trim,n,AM_BOOT_PLUGIN)) {
            if (append_text(out,cap,&used,p,original)<0) return -1;
            if (enabled && !inserted && line_equal(trim,n,"*KERNEL")) {
                if (!used || out[used-1]!='\n') if (append_text(out,cap,&used,"\n",1)<0) return -1;
                const char *block=BEGIN_MARK "\n" AM_BOOT_PLUGIN "\n" END_MARK "\n";
                if (append_text(out,cap,&used,block,strlen(block))<0) return -1;
                inserted=1;
            }
        }
        p+=original;
    }
    if (inside) return -1;
    if (enabled && !inserted) {
        if (used && out[used-1]!='\n') if (append_text(out,cap,&used,"\n",1)<0) return -1;
        const char *block="*KERNEL\n" BEGIN_MARK "\n" AM_BOOT_PLUGIN "\n" END_MARK "\n";
        if (append_text(out,cap,&used,block,strlen(block))<0) return -1;
    }
    return (int)used;
}
typedef struct { int storagemgr,yamt,gamesd; char usb[64]; } StoragePlugins;
static void lower_ascii(char *s) {
    for (;*s;++s) if (*s>='A' && *s<='Z') *s=(char)(*s-'A'+'a');
}
static void scan_storage_plugins(const char *text,int boot,StoragePlugins *out) {
    int kernel=boot;
    while (*text) {
        const char *end=strchr(text,'\n');size_t n=end?(size_t)(end-text):strlen(text);
        const char *comment=memchr(text,'#',n);if (comment) n=(size_t)(comment-text);
        char line[512];if (n>=sizeof(line)) n=sizeof(line)-1;
        memcpy(line,text,n);line[n]=0;lower_ascii(line);
        char *p=line;while (*p==' '||*p=='\t'||*p=='\r') ++p;
        if (!boot && *p=='*') {
            char section[32]={0};sscanf(p,"%31s",section);kernel=!strcmp(section,"*kernel");
        } else if (kernel) {
            while (*p) {
                while (*p==' '||*p=='\t'||*p=='\r'||*p=='\"'||*p=='\'') ++p;
                if (!*p) break;
                char token[256];size_t used=0;
                while (*p && *p!=' ' && *p!='\t' && *p!='\r' && *p!='\"' && *p!='\'') {
                    if (used+1<sizeof(token)) token[used++]=*p;
                    ++p;
                }
                token[used]=0;
                const char *base=strrchr(token,'/');
                if (!base) base=strrchr(token,':');
                base=base?base+1:token;
                if (!strcmp(base,"storagemgr.skprx")) out->storagemgr=1;
                if (!strcmp(base,"yamt.skprx")||!strcmp(base,"yamt_helper.skprx")) out->yamt=1;
                if (!strcmp(base,"gamesd.skprx")) out->gamesd=1;
                if (!strcmp(base,"usbmc.skprx")||!strcmp(base,"usbmc_accessory.skprx")||!strcmp(base,"usbmc_accessory_probe.skprx"))
                    snprintf(out->usb,sizeof(out->usb),"%.63s",base);
            }
        }
        if (!end) break;
        text=end+1;
    }
}
static int storagemgr_conflict(int target,char *reason,size_t cap) {
    char text[8192];int ret=am_read_text("ur0:tai/storage_config.txt",text,sizeof(text));
    if (ret==ENOENT_VALUE) {
        am_log("StorageMgr: no storage_config.txt; no saved assignments.\n");return 0;
    }
    if (ret<0) {
        snprintf(reason,cap,"StorageMgr config unreadable. See Log.");
        am_log("Cannot inspect ur0:tai/storage_config.txt: %08X\n",(unsigned)ret);return 1;
    }
    for (const char *p=text;*p;) {
        const char *end=strchr(p,'\n');size_t n=end?(size_t)(end-p):strlen(p);
        const char *hash=memchr(p,'#',n);if (hash) n=(size_t)(hash-p);
        char line[256],compact[256];if (n>=sizeof(line)) n=sizeof(line)-1;
        memcpy(line,p,n);line[n]=0;lower_ascii(line);size_t j=0;
        for (size_t i=0;line[i];++i) if (line[i]!=' ' && line[i]!='\t' && line[i]!='\r') compact[j++]=line[i];
        compact[j]=0;
        char *equal=strchr(compact,'=');
        if (equal) {
            *equal=0;const char *mount=equal+1;
            int known=!strcmp(compact,"uma")||!strcmp(compact,"mcd")||!strcmp(compact,"gcd")||!strcmp(compact,"int");
            if (known) am_log("StorageMgr assignment: %s=%s\n",compact,mount);
            if (!strcmp(compact,"uma")) {
                snprintf(reason,cap,"StorageMgr already manages USB. See Log.");return 1;
            }
            if (known && !strcmp(mount,am_target_name(target)) ) { /* Colon form is accepted below too. */
                snprintf(reason,cap,"StorageMgr reserves %s. See Log.",am_target_name(target));return 1;
            }
            if (known && !strcmp(mount,target==AM_UX0?"ux0":"uma0")) {
                snprintf(reason,cap,"StorageMgr reserves %s. See Log.",am_target_name(target));return 1;
            }
        }
        if (!end) break;
        p=end+1;
    }
    return 0;
}
static int yamt_conflict(int target,char *reason,size_t cap) {
    unsigned char *data=NULL;size_t size=0;
    int ret=read_bytes("ur0:tai/yamt.cfg",&data,&size);
    /* Stock YAMT Lite v4 stores four ASCII digits, not the full binary struct:
       version, enabled, ux0 device, uma0 device. Device 0 leaves that target
       unchanged; 1=SD2Vita, 2=Memory Card, 3=Internal Storage. */
    if (ret>=0 && size==4 && data[0]=='4' && (data[1]=='0'||data[1]=='1') &&
        data[2]>='0' && data[2]<='3' && data[3]>='0' && data[3]<='3') {
        static const char *devices[]={"Default","SD2Vita","Memory Card","Internal Storage"};
        unsigned ux=data[2]-'0',uma=data[3]-'0';int enabled=data[1]=='1';
        unsigned chosen=target==AM_UX0?ux:uma;
        am_log("YAMT Lite v4 (4 bytes): enabled=%d ux0=%s uma0=%s\n",enabled,devices[ux],devices[uma]);
        int conflict=enabled && chosen!=0;
        if (conflict) {
            snprintf(reason,cap,"YAMT Lite maps %s to %s. See Log.",am_target_name(target),devices[chosen]);
            am_log("YAMT Lite reserves requested target %s; no changes made to YAMT.\n",am_target_name(target));
        }
        free(data);return conflict;
    }
    /* Full YAMT v4: 8-byte header, sixteen packed 12-byte ARM32 entries. */
    if (ret<0 || size!=200 || data[0]!=4 || data[1]>16) {
        snprintf(reason,cap,"YAMT settings need inspection. See Log.");
        if (ret<0) am_log("YAMT: cannot read ur0:tai/yamt.cfg (error %08X).\n",(unsigned)ret);
        else {
            char hex[49]={0};
            for (size_t i=0;i<size && i<16;++i) snprintf(hex+i*3,sizeof(hex)-i*3,"%02X ",data[i]);
            am_log("YAMT: unsupported config format (%u bytes; first bytes: %s%s).\n",(unsigned)size,hex,size>16?"...":"");
        }
        free(data);return 1;
    }
    if (!data[2]) { free(data);am_log("YAMT mapping is disabled.\n");return 0; }
    if (data[4]) {
        free(data);snprintf(reason,cap,"YAMT USB support is enabled. See Log.");am_log("YAMT drv[0] enables USB ownership.\n");return 1;
    }
    int conflict=0;
    for (unsigned i=0;i<data[1];++i) {
        const unsigned char *e=data+8+i*12;
        unsigned mid=e[4],device=e[8];if (device==255) continue;
        am_log("YAMT entry %u: mount=%02X00 device=%u mode=%u\n",i,mid,device,e[5]);
        if (device>5 || e[5]>1) {
            free(data);snprintf(reason,cap,"YAMT entry format is unsupported. See Log.");return 1;
        }
        if (device==5 || mid==(target==AM_UX0?8u:15u)) conflict=1;
    }
    free(data);
    if (conflict) snprintf(reason,cap,"YAMT owns USB or %s. See Log.",am_target_name(target));
    return conflict;
}
static int storage_conflict(const char *tai,const char *boot,int target,char *reason,size_t cap) {
    StoragePlugins plugins={0};scan_storage_plugins(tai,0,&plugins);scan_storage_plugins(boot,1,&plugins);
    am_log("Storage scan: target=%s StorageMgr=%d YAMT=%d gamesd=%d USB=%s\n",
           am_target_name(target),plugins.storagemgr,plugins.yamt,plugins.gamesd,plugins.usb[0]?plugins.usb:"none");
    if (plugins.usb[0]) {
        snprintf(reason,cap,"Conflict: %s. See Log.",plugins.usb);return 1;
    }
    if (plugins.storagemgr && storagemgr_conflict(target,reason,cap)) return 1;
    if (plugins.yamt && yamt_conflict(target,reason,cap)) return 1;
    if (plugins.gamesd && target==AM_UX0) {
        snprintf(reason,cap,"gamesd also controls ux0. See Log.");return 1;
    }
    return 0;
}
/* Compare executable payloads without the bounded configuration-reader limit. */
static int file_read_exact(int fd,unsigned char *bytes,size_t size) {
    while (size) {
        int ret=sceIoRead(fd,bytes,size);if (ret<=0) return 0;
        bytes+=ret;size-=(size_t)ret;
    }
    return 1;
}
static int same_file(const char *a,const char *b) {
    SceIoStat first,second;
    if (sceIoGetstat(a,&first)<0 || sceIoGetstat(b,&second)<0 ||
        !SCE_S_ISREG(first.st_mode) || !SCE_S_ISREG(second.st_mode) ||
        (int64_t)first.st_size<0 || first.st_size!=second.st_size) return 0;
    int in=sceIoOpen(a,SCE_O_RDONLY,0);if (in<0) return 0;
    int out=sceIoOpen(b,SCE_O_RDONLY,0);if (out<0) { sceIoClose(in);return 0; }
    unsigned char x[4096],y[4096];uint64_t left=(uint64_t)first.st_size;int same=1;
    while (left) {
        size_t n=left>sizeof(x)?sizeof(x):(size_t)left;
        if (!file_read_exact(in,x,n) || !file_read_exact(out,y,n) || memcmp(x,y,n)) { same=0;break; }
        left-=n;
    }
    /* Detect growth after stat, read errors and close failures too. */
    if (same && (sceIoRead(in,x,1)!=0 || sceIoRead(out,y,1)!=0)) same=0;
    int closed_in=sceIoClose(in),closed_out=sceIoClose(out);
    return same && closed_in>=0 && closed_out>=0;
}
static int prepared_apps(void) {
    if (!present("uma0:app/VITASHELL/eboot.bin")) { am_log("Destination app check: missing VitaShell executable.\n");return 0; }
    const char *files[]={"eboot.bin","psvsd_am.skprx","psvsd_am_status.skprx","usbmc_accessory_probe.skprx"};
    for (unsigned i=0;i<4;++i) {
        char source[128],target[128];
        snprintf(source,sizeof(source),"app0:%s",files[i]);
        snprintf(target,sizeof(target),"uma0:app/RTPSV0001/%s",files[i]);
        if (!same_file(source,target)) { am_log("Destination app check: missing or mismatched %s\n",target);return 0; }
    }
    return 1;
}
static int configs_unchanged(const char *tai,const char *boot) {
    char *text=malloc(MAX_CONFIG);if (!text) return 0;
    int r=am_read_text(AM_TAI_CONFIG,text,MAX_CONFIG),same=r>=0 && !strcmp(text,tai);
    if (same) {
        r=am_read_text("ur0:tai/boot_config.txt",text,MAX_CONFIG);
        same=(r>=0 && !strcmp(text,boot)) || (r==ENOENT_VALUE && !boot[0]);
    }
    free(text);return same;
}
int am_save_settings_with_callbacks(const AmSettings *s,const AmSaveCallbacks *callbacks,char *message,size_t cap) {
    if (!cap) return IO_ERROR;
    message[0]=0;
    char settings[128];int settings_size=am_settings_format(s,settings,sizeof(settings));
    if (settings_size<0) return -1;
    mkdir_ok("ur0:data");int ret=mkdir_ok(ACCESSORY_KERNEL_DIR);
    if (ret<0) goto error;
    char *old_tai=malloc(MAX_CONFIG),*new_tai=malloc(MAX_CONFIG+256);
    if (!old_tai || !new_tai) { free(old_tai);free(new_tai);ret=IO_ERROR;goto error; }
    int read=am_read_text(AM_TAI_CONFIG,old_tai,MAX_CONFIG);
    if (read<0) { ret=read;snprintf(message,cap,"Cannot read ur0:tai/config.txt (%08X)",(unsigned)ret);goto done; }
    if (s->enabled && present("ux0:tai/config.txt")) {
        ret=-1;snprintf(message,cap,"Use ur0 tai configuration before enabling.");goto done;
    }
    char boot_config[8192];int br=am_read_text("ur0:tai/boot_config.txt",boot_config,sizeof(boot_config));
    if (s->enabled && br<0 && br!=ENOENT_VALUE) {
        ret=br;snprintf(message,cap,"Boot configuration unreadable. See Log.");goto done;
    }
    if (br<0) boot_config[0]=0;
    if (s->enabled && storage_conflict(old_tai,boot_config,s->target,message,cap)) { ret=-1;goto done; }
    if (s->enabled && s->target==AM_UMA0) {
        AmStatus current;am_status(&current);
        if (current.uma0 && !(current.verified_mount && (current.actual_target==AM_UMA0 || (current.actual_target==AM_UX0 && current.original_ready)))) {
            ret=-1;snprintf(message,cap,"uma0 is already occupied. See Log.");
            am_log("Existing uma0 is not a freshly verified PSVSD/AM USB mount; preserving it.\n");goto done;
        }
    }
    AmStatus before;am_status(&before);
    AmSettings saved;
    if (!(s->enabled && s->target==AM_UX0) && am_load_settings(&saved)>=0 && saved.enabled && saved.target==AM_UX0 && !before.verified_mount) {
        ret=-1;snprintf(message,cap,"Current USB mapping could not be verified. Settings unchanged. See Log.");goto done;
    }
    if (before.verified_mount && before.actual_target==AM_UX0 && !(s->enabled && s->target==AM_UX0)) {
        if (!before.original_ready && callbacks && callbacks->request_original) {
            ret=callbacks->request_original(callbacks->ctx,message,cap);if (ret<0) goto done;
            am_status(&before);
        }
        if (!before.original_ready) {
            ret=-1;snprintf(message,cap,"Original storage unavailable. Save/reboot with USB ux0 once to refresh the boot plugin, then retry.");goto done;
        }
        ret=am_prepare_original_apps(callbacks?callbacks->progress:NULL,callbacks?callbacks->ctx:NULL,message,cap);if (ret<0) goto done;
        am_status(&before);
        if (!before.original_ready || !configs_unchanged(old_tai,boot_config) || !prepared_apps()) {
            ret=-1;snprintf(message,cap,"Apps could not be confirmed on original storage. Settings unchanged. See Log.");goto done;
        }
        if (s->enabled && storage_conflict(old_tai,boot_config,s->target,message,cap)) { ret=-1;goto done; }
    }
    if (s->enabled && s->target==AM_UX0) {
        AmStatus status;am_status(&status);
        int already=status.verified_mount && status.actual_target==AM_UX0;
        if (!already) {
            if (am_tai_transform(old_tai,1,new_tai,MAX_CONFIG+256)<0) {
                ret=-1;snprintf(message,cap,"Cannot safely update the PSVSD/AM config block.");goto done;
            }
            if (!(status.verified_mount && status.actual_target==AM_UMA0) && callbacks && callbacks->request_usb) {
                ret=callbacks->request_usb(callbacks->ctx,message,cap);if (ret<0) goto done;
                am_status(&status);
            }
            if (!(status.verified_mount && status.actual_target==AM_UMA0)) {
                ret=-1;snprintf(message,cap,"Run the USB test to verify uma0 before preparing ux0.");goto done;
            }
            ret=am_prepare_usb_apps(callbacks?callbacks->progress:NULL,callbacks?callbacks->ctx:NULL,message,cap);if (ret<0) goto done;
            if (!configs_unchanged(old_tai,boot_config)) {
                ret=-1;snprintf(message,cap,"Storage configuration changed during copy. Retry Save.");goto done;
            }
            if (storage_conflict(old_tai,boot_config,s->target,message,cap)) { ret=-1;goto done; }
            am_status(&status);
            if (!status.verified_mount || status.actual_target!=AM_UMA0 || !prepared_apps()) {
                ret=-1;snprintf(message,cap,"Copied apps could not be confirmed on USB. See Log.");goto done;
            }
        }
    }
    int tai_size=am_tai_transform(old_tai,s->enabled,new_tai,MAX_CONFIG+256);
    if (tai_size<0) { ret=-1;snprintf(message,cap,"Cannot safely update the PSVSD/AM config block.");goto done; }
    if (s->enabled) {
        unsigned char *binary=NULL;size_t bytes=0;ret=read_bytes(AM_BOOT_SOURCE,&binary,&bytes);
        if (ret>=0 && (bytes<4 || memcmp(binary,"SCE\0",4))) ret=IO_ERROR;
        if (ret>=0) ret=am_replace_file(AM_BOOT_PLUGIN,binary,bytes);
        free(binary);if (ret<0) goto done;
    }
    unsigned char *previous=NULL;size_t previous_size=0;
    int previous_ret=read_bytes(AM_SETTINGS_PATH,&previous,&previous_size);
    if (previous_ret<0 && previous_ret!=ENOENT_VALUE) { ret=previous_ret;goto done; }
    ret=am_replace_file(AM_SETTINGS_PATH,settings,(size_t)settings_size);
    if (ret>=0) {
        ret=am_replace_file(AM_TAI_CONFIG,new_tai,(size_t)tai_size);
        if (ret<0) {
            int rollback=previous ? am_replace_file(AM_SETTINGS_PATH,previous,previous_size) : sceIoRemove(AM_SETTINGS_PATH);
            am_log("Save failed %08X; settings rollback %08X\n",(unsigned)ret,(unsigned)rollback);
        }
    }
    free(previous);
    if (ret>=0) snprintf(message,cap,"Saved. Reboot to apply %s at %s",s->enabled?"enabled":"disabled",am_target_name(s->target));
done:
    free(old_tai);free(new_tai);
    if (ret<0 && !message[0]) snprintf(message,cap,"Save failed: %08X. See log.",(unsigned)ret);
    am_log("Save: enabled=%d target=%s result=%08X %s\n",s->enabled,am_target_name(s->target),(unsigned)ret,message);
    return ret;
error:
    snprintf(message,cap,"Save failed: %08X",(unsigned)ret);return ret;
}
static int module_present(const char *name) { unsigned char work[8]={0};return _vshKernelSearchModuleByName(name,work); }
int am_stage_status(void) {
    mkdir_ok("ux0:data");int ret=mkdir_ok(ACCESSORY_DATA_DIR);if (ret<0) return ret;
    unsigned char *data=NULL;size_t size=0;ret=read_bytes(AM_STATUS_SOURCE,&data,&size);
    if (ret>=0 && (size<4 || memcmp(data,"SCE\0",4))) ret=IO_ERROR;
    if (ret>=0) ret=am_replace_file(AM_STATUS_STAGED,data,size);
    free(data);return ret;
}
void am_status(AmStatus *s) {
    memset(s,0,sizeof(*s));s->actual_target=-1;
    s->ux0=directory("ux0:");s->uma0=directory("uma0:");s->gamecard=directory("gro0:app");
    s->raw_usb=raw_present("sdstor0:uma-pp-act-a") || raw_present("sdstor0:uma-lp-act-entire");
    s->module_id=module_present(AM_KERNEL_NAME);
    if (s->module_id<0) s->module_id=module_present(AM_PROBE_NAME);
    s->loaded=s->module_id>=0;
    s->legacy_loaded=module_present("USBMCAccessoryProbe")>=0 || module_present("USBMCAccessory")>=0;
    if (!s->loaded || !s->raw_usb || !expected_query) return;
    char runtime[256];int r=am_read_text(ACCESSORY_KERNEL_DIR "/runtime.txt",runtime,sizeof(runtime));
    if (r>=0) {
        unsigned token=0;char target[8]={0};
        if (sscanf(runtime,"PSVSD/AM " ACCESSORY_VERSION "\nRequest: %X\nMount: %7s",&token,target)==2 && token==expected_query) {
            if (!strcmp(target,"uma0:") && s->uma0) s->actual_target=AM_UMA0;
            if (!strcmp(target,"ux0:") && s->ux0) s->actual_target=AM_UX0;
            s->verified_mount=s->actual_target>=0;
            s->original_ready=s->verified_mount && s->actual_target==AM_UX0 && s->uma0 && strstr(runtime,"\nOriginal: uma0:\n")!=NULL;
        }
    }
}
static uint32_t crc_update(uint32_t crc,const unsigned char *p,size_t n) {
    while (n--) { crc^=*p++;for (int j=0;j<8;++j) crc=(crc>>1)^(0xEDB88320u & (0u-(crc&1u))); }return crc;
}
static void pattern(unsigned char *p,size_t n,uint32_t offset) {
    uint32_t v=0xA8355EEDu^offset;
    for (size_t i=0;i<n;++i) { v^=v<<13;v^=v>>17;v^=v<<5;p[i]=(unsigned char)v; }
}
static int open_test_file(int target,const char *prefix,int mode,char *path,size_t cap) {
    AmStatus status;am_status(&status);
    if (!status.verified_mount || status.actual_target!=target) return -1;
    char dir[80];snprintf(dir,sizeof(dir),"%sdata",am_target_name(target));
    int ret=mkdir_ok(dir);if (ret<0) return ret;
    snprintf(dir,sizeof(dir),"%sdata/PSVSD_AM",am_target_name(target));
    ret=mkdir_ok(dir);if (ret<0) return ret;
    int fd=-1;uint32_t token=sceKernelGetProcessTimeLow();
    for (unsigned i=0;i<8;++i) {
        snprintf(path,cap,"%s/%s-%08X.bin",dir,prefix,token+i);
        fd=sceIoOpen(path,mode|SCE_O_CREAT|SCE_O_EXCL,0666);
        if (fd!=EEXIST_VALUE) break;
    }
    return fd;
}
int am_test_io(int target,AmTestResult *r,AmProgress progress,void *ctx) {
    memset(r,0,sizeof(*r));
    int fd=open_test_file(target,"verify",SCE_O_WRONLY,r->path,sizeof(r->path));
    if (fd<0) return r->result=fd;
    int ret=0;
    enum { CHUNK=16384, TOTAL=4*1024*1024 };
    unsigned char data[CHUNK],expected[CHUNK];uint32_t write_crc=0xFFFFFFFFu,read_crc=0xFFFFFFFFu;
    for (uint32_t off=0;off<TOTAL;off+=CHUNK) {
        pattern(data,CHUNK,off);ret=write_all(fd,data,CHUNK);if (ret<0) break;
        r->written+=CHUNK;write_crc=crc_update(write_crc,data,CHUNK);
        if (progress && !(off%(CHUNK*16))) progress((unsigned)(50ull*r->written/TOTAL),"Writing test file",ctx);
    }
    int closed=sceIoClose(fd);if (ret>=0 && closed<0) ret=closed;
    if (ret>=0) { r->sync_result=sceIoSync(am_target_name(target),0);if (r->sync_result<0) ret=r->sync_result; }
    if (ret>=0) {
        fd=sceIoOpen(r->path,SCE_O_RDONLY,0);if (fd<0) ret=fd;
        else {
            for (uint32_t off=0;off<TOTAL;off+=CHUNK) {
                ret=read_all(fd,data,CHUNK);if (ret<0) break;
                pattern(expected,CHUNK,off);if (memcmp(data,expected,CHUNK)) { ret=IO_ERROR;break; }
                r->read+=CHUNK;read_crc=crc_update(read_crc,data,CHUNK);
                if (progress && !(off%(CHUNK*16))) progress(50+(unsigned)(50ull*r->read/TOTAL),"Verifying file contents",ctx);
            }
            closed=sceIoClose(fd);if (ret>=0 && closed<0) ret=closed;
        }
    }
    r->crc=read_crc^0xFFFFFFFFu;
    if (ret>=0 && (r->written!=TOTAL || r->read!=TOTAL || write_crc!=read_crc)) ret=IO_ERROR;
    r->cleanup_result=sceIoRemove(r->path);if (ret>=0 && r->cleanup_result<0) ret=r->cleanup_result;
    r->result=ret;
    am_log("Verification file: %s\n",r->path);
    am_log("Read/write test %s: written=%u read=%u CRC32=%08X sync=%08X cleanup=%08X result=%08X\n",
           am_target_name(target),r->written,r->read,r->crc,(unsigned)r->sync_result,(unsigned)r->cleanup_result,(unsigned)r->result);
    if (progress) progress(100,ret>=0?"Read/write verification passed":"Test failed - see log",ctx);
    return ret;
}

enum { SPEED_MIB=1024*1024,SPEED_FILE_BYTES=AM_SPEED_FILE_MIB*SPEED_MIB,
       SPEED_SEQ_BLOCK=128*1024,SPEED_RANDOM_BLOCK=4096,SPEED_RANDOM_OPS=512 };

static int measure_speed(int fd,unsigned phase,unsigned char *data,const uint16_t *pages,
                         AmSpeedRate *rate,AmSpeedProgress progress,void *ctx) {
    const int random=phase==AM_SPEED_RANDOM_READ || phase==AM_SPEED_RANDOM_WRITE;
    const int writing=phase==AM_SPEED_SEQ_WRITE || phase==AM_SPEED_RANDOM_WRITE;
    const unsigned block=random?SPEED_RANDOM_BLOCK:SPEED_SEQ_BLOCK;
    const unsigned count=random?SPEED_RANDOM_OPS:SPEED_FILE_BYTES/SPEED_SEQ_BLOCK;
    if (phase==AM_SPEED_SEQ_READ) {
        int64_t position=sceIoLseek(fd,0,SCE_SEEK_SET);
        if (position!=0) return position<0?(int)position:IO_ERROR;
    }
    if (progress) progress(phase*AM_SPEED_PHASE_PERCENT,ctx);
    int64_t started=sceKernelGetSystemTimeWide();
    if (started<0) return IO_ERROR;
    for (unsigned i=0;i<count;++i) {
        if (random) {
            int64_t offset=(int64_t)pages[i]*SPEED_RANDOM_BLOCK;
            int bytes=writing?sceIoPwrite(fd,data,block,offset):sceIoPread(fd,data,block,offset);
            if (bytes!=(int)block) return bytes<0?bytes:IO_ERROR;
        } else {
            int ret=writing?write_all(fd,data,block):read_all(fd,data,block);
            if (ret<0) return ret;
        }
        if (progress && (i+1)%16==0)
            progress(phase*AM_SPEED_PHASE_PERCENT+(AM_SPEED_PHASE_PERCENT-1)*(i+1)/count,ctx);
    }
    if (writing) {
        int ret=sceIoSyncByFd(fd,0);
        if (ret<0) return ret;
    }
    int64_t finished=sceKernelGetSystemTimeWide();
    if (finished<=started) return IO_ERROR;
    double seconds=(finished-started)/1000000.0;
    rate->mib_s=(double)count*block/(SPEED_MIB*seconds);
    if (random) rate->iops=count/seconds;
    return 0;
}

int am_speed_test(int target,AmSpeedResult *r,AmSpeedProgress progress,void *ctx) {
    memset(r,0,sizeof(*r));r->target=target;
    unsigned char *data=malloc(SPEED_SEQ_BLOCK);
    if (!data) {
        am_log("Speed Test could not allocate the I/O buffer.\n");
        return r->result=IO_ERROR;
    }
    int fd=open_test_file(target,"speed",SCE_O_RDWR,r->path,sizeof(r->path));
    if (fd<0) {
        am_log("Speed Test file setup failed on %s (%08X).\n",am_target_name(target),(unsigned)fd);
        free(data);return r->result=fd;
    }

    /* The generator visits all pages before repeating in the power-of-two file. */
    uint16_t pages[2*SPEED_RANDOM_OPS];
    uint32_t random=0x41C64E6Du;
    for (unsigned i=0;i<2*SPEED_RANDOM_OPS;++i) {
        random=random*1664525u+1013904223u;
        pages[i]=(uint16_t)(random%(SPEED_FILE_BYTES/SPEED_RANDOM_BLOCK));
    }
    pattern(data,SPEED_SEQ_BLOCK,0);
    unsigned phase=0;
    int ret=0;
    for (;phase<AM_SPEED_PHASE_COUNT;++phase) {
        const uint16_t *offsets=pages;
        if (phase==AM_SPEED_RANDOM_WRITE) {
            offsets+=SPEED_RANDOM_OPS;
            pattern(data,SPEED_RANDOM_BLOCK,SPEED_FILE_BYTES);
        }
        ret=measure_speed(fd,phase,data,offsets,&r->rates[phase],progress,ctx);
        if (ret<0) break;
    }
    int closed=sceIoClose(fd);
    free(data);
    if (ret>=0 && closed<0) ret=closed;
    r->cleanup_result=sceIoRemove(r->path);
    if (ret>=0 && r->cleanup_result<0) ret=r->cleanup_result;
    r->result=ret;
    am_log("Speed Test %s, %s: phases=%u/%u close=%08X cleanup=%08X result=%08X\n",
           am_target_name(target),r->path,phase,(unsigned)AM_SPEED_PHASE_COUNT,
           (unsigned)closed,(unsigned)r->cleanup_result,(unsigned)ret);
    if (ret>=0) {
        am_log("Sequential read %.2f MiB/s, write %.2f MiB/s (flushed)\n",
               r->rates[AM_SPEED_SEQ_READ].mib_s,r->rates[AM_SPEED_SEQ_WRITE].mib_s);
        am_log("4 KiB random read %.2f MiB/s (%.0f IOPS), write %.2f MiB/s (%.0f IOPS, flushed). Reads may be cached.\n",
               r->rates[AM_SPEED_RANDOM_READ].mib_s,r->rates[AM_SPEED_RANDOM_READ].iops,
               r->rates[AM_SPEED_RANDOM_WRITE].mib_s,r->rates[AM_SPEED_RANDOM_WRITE].iops);
    }
    return ret;
}

int am_clear_logs(char *message,size_t cap) {
    if (!cap) return IO_ERROR;
    /* Fixed diagnostic-only paths. Runtime query tokens, settings, tai files,
       installed modules and their recovery files are deliberately excluded. */
    const char *paths[]={AM_APP_LOG,ACCESSORY_KERNEL_LOG,ACCESSORY_KERNEL_FALLBACK,ACCESSORY_USER_LOG,
        ACCESSORY_USER_LOG ".psvsdam.bak",ACCESSORY_USER_LOG ".psvsdam.new",ACCESSORY_USER_LOG ".psvsdam.swap"};
    int failed=0;const char *failed_path=NULL;
    for (unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);++i) {
        int ret=sceIoRemove(paths[i]);
        if (ret<0 && ret!=ENOENT_VALUE && !failed) { failed=ret;failed_path=paths[i]; }
    }
    if (failed) {
        snprintf(message,cap,"Clear Log failed (%08X). Some logs remain.",(unsigned)failed);
        am_log("Clear Log incomplete: %s returned %08X; other diagnostic files may have been cleared.\n",failed_path,(unsigned)failed);
    } else snprintf(message,cap,"Logs cleared. New events will appear here.");
    return failed;
}
int am_export_report(const char *extra,char *message,size_t cap) {
    char *report=malloc(49152),*part=malloc(16384);if (!report||!part) { free(report);free(part);return IO_ERROR; }
    AmStatus s;am_status(&s);
    int n=snprintf(report,49152,"PSVSD/AM " ACCESSORY_VERSION "\nKernel module: %08X\nActual USB mount: %s\nGamecard directory: %s\n%s\n",
        (unsigned)s.module_id,s.verified_mount?am_target_name(s.actual_target):"not verified",s.gamecard?"accessible":"no accessible cartridge",extra?extra:"");
    size_t used=n>0?(size_t)n:0;
    const char *paths[]={ACCESSORY_KERNEL_LOG,AM_APP_LOG};
    for (unsigned i=0;i<2;++i) if (am_read_log(paths[i],part,16384)>=0) {
        n=snprintf(report+used,49152-used,"\n--- %s ---\n%s",paths[i],part);
        if (n>0) used+=(size_t)n<49152-used?(size_t)n:49152-used-1;
    }
    mkdir_ok("ux0:data");mkdir_ok(ACCESSORY_DATA_DIR);
    int ret=am_replace_file(ACCESSORY_USER_LOG,report,used);
    snprintf(message,cap,ret>=0?"Report saved to " ACCESSORY_USER_LOG:"Report save failed: %08X",(unsigned)ret);
    free(report);free(part);return ret;
}
