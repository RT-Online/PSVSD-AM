/* Diagnostic app I/O and loader sequence. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef ACCESSORY_APP_TEST
#include "mock_app.h"
#else
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/io/devctl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/modulemgr.h>
#include <taihen.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "session.h"

static void append(AccessorySession *s, const char *fmt, ...) {
    size_t room=sizeof(s->text)-s->used;
    if (room<=1) return;
    va_list ap; va_start(ap,fmt);
    int n=vsnprintf(s->text+s->used,room,fmt,ap); va_end(ap);
    if (n>0) s->used+=(size_t)n<room?(size_t)n:room-1;
}
static int write_all(int fd, const void *data, size_t size) {
    const unsigned char *p=data;
    while (size) {
        int n=sceIoWrite(fd,p,size);
        if (n<0) return n;
        if (!n) return (int)0x80010005;
        p+=n; size-=n;
    }
    return 0;
}
static void save(AccessorySession *s) {
    int fd=sceIoOpen(ACCESSORY_USER_LOG,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if (fd<0) { s->log_result=fd; return; }
    int ret=write_all(fd,s->text,s->used);
    int close_ret=sceIoClose(fd);
    s->log_result=ret<0?ret:close_ret;
}
static int make_dir(const char *path) {
    int ret=sceIoMkdir(path,0777);
    return ret==(int)0x80010011?0:ret;
}
static int stage_probe(AccessorySession *s) {
    SceIoStat stat;
    int ret=sceIoGetstat(ACCESSORY_SOURCE_PROBE,&stat);
    if (ret<0) return ret;
    if (stat.st_size<4 || stat.st_size>1024*1024) return (int)0x80010008;
    int in=sceIoOpen(ACCESSORY_SOURCE_PROBE,SCE_O_RDONLY,0);
    if (in<0) return in;
    const char *temp=ACCESSORY_STAGED_PROBE ".new";
    int out=sceIoOpen(temp,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if (out<0) { sceIoClose(in); return out; }
    unsigned char buf[2048],magic[4]={0};
    size_t total=0;
    while ((ret=sceIoRead(in,buf,sizeof(buf)))>0) {
        if (total<4) {
            size_t n=4-total;
            if (n>(size_t)ret) n=ret;
            memcpy(magic+total,buf,n);
        }
        int wr=write_all(out,buf,ret);
        if (wr<0) { ret=wr; break; }
        total+=ret;
    }
    sceIoClose(in);
    int close_ret=sceIoClose(out);
    if (ret>=0 && close_ret<0) ret=close_ret;
    if (ret>=0 && total!=(size_t)stat.st_size) ret=(int)0x80010005;
    if (ret>=0 && memcmp(magic,"SCE\0",4)) ret=(int)0x80010008;
    if (ret>=0) {
        int rm=sceIoRemove(ACCESSORY_STAGED_PROBE);
        if (rm<0 && rm!=(int)0x80010002) ret=rm;
        else ret=sceIoRename(temp,ACCESSORY_STAGED_PROBE);
    }
    if (ret<0) sceIoRemove(temp);
    append(s,"Staged bytes: %u / %u\n",(unsigned)total,(unsigned)stat.st_size);
    return ret;
}
static void collect_kernel_log(AccessorySession *s) {
    const char *paths[]={ACCESSORY_KERNEL_LOG,ACCESSORY_KERNEL_FALLBACK};
    char expected[40];
    snprintf(expected,sizeof(expected),"Request: %08X\n",(unsigned)s->request_id);
    for (unsigned i=0;i<2;++i) {
        int fd=sceIoOpen(paths[i],SCE_O_RDONLY,0);
        if (fd<0) { append(s,"Kernel log open %s: %08X\n",paths[i],(unsigned)fd); continue; }
        char buf[16384]; size_t used=0; int n=0;
        while (used<sizeof(buf)-1 && (n=sceIoRead(fd,buf+used,sizeof(buf)-1-used))>0) used+=n;
        sceIoClose(fd); buf[used]=0;
        if (n<0 || !strstr(buf,expected)) {
            append(s,"Kernel log is unreadable or belongs to an older run.\n"); continue;
        }
        append(s,"\nKernel report (%s):\n%s",paths[i],buf);
        return;
    }
    append(s,"No current kernel report. LOAD/START results above are separate from mounting.\n");
}

void accessory_session_refresh(AccessorySession *s) {
    if (!s->attempted || !s->kernel_offset) return;
    s->used=s->kernel_offset; s->text[s->used]=0;
    collect_kernel_log(s); save(s);
}
void accessory_session_clear_log(AccessorySession *s) {
    /* Keep probe/load state so clearing diagnostics cannot trigger a second
       probe or restore old text on the next refresh/export. */
    s->used=s->kernel_offset=0;s->log_result=0;
    memset(s->text,0,sizeof(s->text));
}

static int storage_query(uint32_t request_id,uint32_t magic,AccessoryQuery *result) {
    AccessoryQuery q={-1,-1,-1,-1};
    int id=taiLoadKernelModule(AM_STATUS_STAGED,0,NULL);q.load=id;
    if (id<0) { if (result) *result=q;return id; }
    AccessoryRequest request={magic,request_id};
    tai_module_args_t args;memset(&args,0,sizeof(args));
    args.size=sizeof(args);args.pid=KERNEL_PID;args.args=sizeof(request);args.argp=&request;
    int entry=-1,ret=taiStartKernelModuleForUser(id,&args,NULL,&entry);
    q.start=ret;q.entry=ret>=0?entry:-1;
    /* The nonresident query module installs no hooks/callbacks. An explicit
     * original-storage request may ask the resident boot plugin to mount uma0. */
    q.unload=taiUnloadKernelModule(id,0,NULL);
    if (result) *result=q;
    return ret<0?ret:entry==1?0:(int)0x80010005;
}

int accessory_status_query(uint32_t request_id,AccessoryQuery *result) { return storage_query(request_id,ACCESSORY_REQUEST_MAGIC,result); }
int accessory_original_prepare(uint32_t request_id,AccessoryQuery *result) { return storage_query(request_id,ACCESSORY_ORIGINAL_MAGIC,result); }

void accessory_session_run(AccessorySession *s) {
    memset(s,0,sizeof(*s)); s->attempted=1;
    s->request_id=sceKernelGetProcessTimeLow();
    append(s,"PSVSD Accessory Test " ACCESSORY_VERSION "\nRequest: %08X\n",(unsigned)s->request_id);
    SceKernelSystemSwVersion version;memset(&version,0,sizeof(version));version.size=sizeof(version);
    int version_ret=sceKernelGetSystemSwVersion(&version);
    if (version_ret>=0) append(s,"Firmware: %s / %08X\n",version.versionString,(unsigned)version.version);
    else append(s,"Firmware query: %08X\n",(unsigned)version_ret);
    int ret=make_dir("ux0:data");
    if (ret>=0) ret=make_dir(ACCESSORY_DATA_DIR);
    if (ret<0) {
        s->stage_result=ret; s->log_result=ret;
        append(s,"Data directory creation failed: %08X\n",(unsigned)ret); return;
    }
    save(s); /* App log exists before any attempt to load the kernel plugin. */
    const char *raw_paths[]={"sdstor0:uma-pp-act-a","sdstor0:uma-lp-act-entire"};
    for (unsigned i=0;i<2;++i) {
        int fd=sceIoOpen(raw_paths[i],SCE_O_RDONLY,0);
        append(s,"User raw open %s: %08X\n",raw_paths[i],(unsigned)fd);
        if (fd>=0) sceIoClose(fd);
    }
    append(s,"Raw-node visibility above does not prove the kernel probe ran.\n");
    int unsafe=sceIoDevctl("ux0:",0x3001,NULL,0,NULL,0);
    if (unsafe==(int)0x80010030) {
        s->stage_result=unsafe;
        append(s,"Enable Unsafe Homebrew in HENkaku Settings. Kernel LOAD not attempted.\n");
        save(s); return;
    }
    s->stage_result=stage_probe(s);
    append(s,"Stage result: %08X\n",(unsigned)s->stage_result); save(s);
    if (s->stage_result<0) { append(s,"Staging failed. Kernel LOAD not attempted.\n"); save(s); return; }
    append(s,"Kernel LOAD path: " ACCESSORY_STAGED_PROBE "\n");
    s->load_result=taiLoadKernelModule(ACCESSORY_STAGED_PROBE,0,NULL);
    append(s,"Kernel LOAD result: %08X\n",(unsigned)s->load_result); save(s);
    if (s->load_result<0) {
        append(s,"Kernel LOAD failed. The probe did not reach mounting.\n"); save(s); return;
    }
    AccessoryRequest request={ACCESSORY_REQUEST_MAGIC,s->request_id};
    tai_module_args_t args; memset(&args,0,sizeof(args));
    args.size=sizeof(args); args.pid=KERNEL_PID; args.args=sizeof(request); args.argp=&request;
    s->entry_result=-1;
    s->start_result=taiStartKernelModuleForUser(s->load_result,&args,NULL,&s->entry_result);
    s->started=s->start_result>=0;
    append(s,"Kernel START API: %08X\n",(unsigned)s->start_result);
    if (s->started) append(s,"Plugin entry result: %08X\n",(unsigned)s->entry_result);
    else append(s,"Plugin entry result: unavailable (START API failed)\n");
    if (s->start_result<0) append(s,"Kernel START failed; mounting may not have been attempted.\n");
    else if (s->entry_result) append(s,"Probe initialization did not remain resident; see kernel report.\n");
    s->kernel_offset=s->used;
    accessory_session_refresh(s);
}
