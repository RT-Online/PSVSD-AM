#define _XOPEN_SOURCE 700
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "test_temp.h"
#include "../src/app/session.c"

static char root[128];
static char fd_path[2048][256];
static int load_calls,start_calls,unload_calls,cases;
static struct {
    int source_missing,stage_fail,invalid_self,size_mismatch,short_io;
    int load_error,start_error,entry_error,missing_kernel,stale_kernel;
    int user_log_fail,data_dir_fail,unsafe,write_zero,status_mode,original_mode;
} cfg;
static int sce_error(void) { return (int)(0x80010000u | (unsigned)errno); }
static const char *mapped(const char *path,char out[512]) {
    assert(strlen(root)+strlen(path)+2<512);
    snprintf(out,512,"%s/%s",root,path);return out;
}
static int is_source(int fd) { return !strcmp(fd_path[fd],ACCESSORY_SOURCE_PROBE); }
int sceIoOpen(const char *path,int flags,int mode) {
    if (!strcmp(path,ACCESSORY_USER_LOG) && flags&O_WRONLY && cfg.user_log_fail) return (int)0x8001001E;
    if (!strcmp(path,ACCESSORY_STAGED_PROBE ".new") && flags&O_WRONLY && cfg.stage_fail) return (int)0x8001001C;
    char p[512];int fd=open(mapped(path,p),flags,mode);
    if (fd<0) return sce_error();
    assert(fd<2048);snprintf(fd_path[fd],sizeof(fd_path[fd]),"%s",path);return fd;
}
int sceIoClose(int fd) { return close(fd)<0?sce_error():0; }
int sceIoRead(int fd,void *p,size_t n) {
    if (cfg.short_io && is_source(fd) && n>3) n=3;
    int r=read(fd,p,n);return r<0?sce_error():r;
}
int sceIoWrite(int fd,const void *p,size_t n) {
    if (cfg.write_zero && strstr(fd_path[fd],".new")) return 0;
    if (cfg.short_io && n>7) n=7;
    int r=write(fd,p,n);return r<0?sce_error():r;
}
int sceIoMkdir(const char *path,int mode) {
    if (cfg.data_dir_fail && !strcmp(path,ACCESSORY_DATA_DIR)) return (int)0x8001001E;
    char p[512];return mkdir(mapped(path,p),mode)<0?sce_error():0;
}
int sceIoGetstat(const char *path,SceIoStat *s) {
    char p[512];struct stat st;
    if (stat(mapped(path,p),&st)<0) return sce_error();
    s->st_size=st.st_size+(cfg.size_mismatch?8:0);return 0;
}
int sceIoRemove(const char *path) { char p[512];return unlink(mapped(path,p))<0?sce_error():0; }
int sceIoRename(const char *a,const char *b) {
    char x[512],y[512];return rename(mapped(a,x),mapped(b,y))<0?sce_error():0;
}
int sceIoDevctl(const char *p,int cmd,void *a,int b,void *c,int d) {
    (void)a;(void)b;(void)c;(void)d;assert(!strcmp(p,"ux0:") && cmd==0x3001);
    return cfg.unsafe?(int)0x80010030:0;
}
uint32_t sceKernelGetProcessTimeLow(void) { return 0x12345678; }
int sceKernelGetSystemSwVersion(SceKernelSystemSwVersion *v) {
    assert(v->size==sizeof(*v));strcpy(v->versionString,"3.65");v->version=0x03650000;return 0;
}
static void read_text(const char *path,char buf[16384]) {
    char p[512];FILE *f=fopen(mapped(path,p),"rb");assert(f);
    size_t n=fread(buf,1,16383,f);buf[n]=0;fclose(f);
}
int taiLoadKernelModule(const char *path,int flags,void *opt) {
    if (cfg.status_mode) { assert(!strcmp(path,AM_STATUS_STAGED));++load_calls;return 42; }
    assert(!strcmp(path,ACCESSORY_STAGED_PROBE));assert(!strstr(path,"app0:"));
    assert(flags==0 && !opt);++load_calls;
    char buf[16384];
    read_text(path,buf);assert(!memcmp(buf,"SCE\0",4));
    if (!cfg.user_log_fail) { read_text(ACCESSORY_USER_LOG,buf);assert(strstr(buf,"Stage result: 00000000")); }
    return cfg.load_error?(int)0x80010013:42;
}
int taiStartKernelModuleForUser(int id,tai_module_args_t *args,void *opt,int *result) {
    assert(id==42 && !opt && args->size==sizeof(*args) && args->pid==KERNEL_PID);
    assert(args->args==sizeof(AccessoryRequest));
    AccessoryRequest *r=args->argp;assert(r && r->magic==(cfg.original_mode?ACCESSORY_ORIGINAL_MAGIC:ACCESSORY_REQUEST_MAGIC));
    assert(r->request_id==0x12345678);++start_calls;
    if (cfg.start_error) return (int)0x8002D003;
    if (cfg.status_mode) { *result=cfg.entry_error?0:1;return 0; }
    *result=cfg.entry_error?1:0;
    if (!cfg.missing_kernel) {
        char p[512];FILE *f=fopen(mapped(ACCESSORY_KERNEL_FALLBACK,p),"wb");assert(f);
        fprintf(f,"USBMC Accessory " ACCESSORY_VERSION "\nRequest: %08X\nMount uma0: 80010013\n",
                cfg.stale_kernel?0xAABBCCDD:r->request_id);fclose(f);
    }
    return 0;
}
int taiUnloadKernelModule(int id,int f,void *o) { assert(id==42 && !f && !o);++unload_calls;return 0; }
static void reset(void) {
    memset(&cfg,0,sizeof(cfg));memset(fd_path,0,sizeof(fd_path));load_calls=start_calls=unload_calls=0;
    test_temp_dir(root,sizeof(root),"app");
    char p[512];assert(!mkdir(mapped("app0:",p),0700));
    assert(!mkdir(mapped("ux0:data",p),0700));++cases;
}
static void source(void) {
    if (cfg.source_missing) return;
    char p[512];FILE *f=fopen(mapped(ACCESSORY_SOURCE_PROBE,p),"wb");assert(f);
    unsigned char buf[5003];for (size_t i=0;i<sizeof(buf);++i) buf[i]=i&255;
    memcpy(buf,cfg.invalid_self?"FAIL":"SCE\0",4);
    assert(fwrite(buf,1,sizeof(buf),f)==sizeof(buf));fclose(f);
}
static void verify_log(const char *needle) {
    char buf[16384];read_text(ACCESSORY_USER_LOG,buf);assert(strstr(buf,needle));
}
int main(void) {
    AccessorySession s;
    reset();source();accessory_session_run(&s);
    assert((s.load_result==42) && s.started && (strstr(s.text,"Kernel report (")!=NULL) && !s.log_result);
    assert(load_calls==1 && start_calls==1);verify_log("Mount uma0: 80010013");
    reset();source();accessory_session_run(&s);assert(s.attempted && (s.load_result==42) && s.used);
    uint32_t request=s.request_id;accessory_session_clear_log(&s);
    assert(s.attempted && (s.load_result==42) && s.started && s.request_id==request);
    assert(!s.used && !s.kernel_offset && !(strstr(s.text,"Kernel report (")!=NULL) && !s.text[0]);
    assert(sceIoRemove(ACCESSORY_USER_LOG)==0);accessory_session_refresh(&s);
    char cleared_path[512];struct stat cleared_stat;
    assert(stat(mapped(ACCESSORY_USER_LOG,cleared_path),&cleared_stat)<0 && errno==ENOENT);
    assert(load_calls==1 && start_calls==1 && !unload_calls);
    reset();cfg.load_error=1;source();accessory_session_run(&s);
    assert(!(s.load_result==42) && !s.started && load_calls==1 && !start_calls && !s.log_result);
    verify_log("Kernel LOAD failed. The probe did not reach mounting.");
    reset();cfg.start_error=1;source();accessory_session_run(&s);
    assert((s.load_result==42) && !s.started && !s.log_result);verify_log("Kernel START failed");
    verify_log("Plugin entry result: unavailable (START API failed)");
    assert(!strstr(s.text,"Plugin entry result: 00000000"));
    reset();cfg.entry_error=1;source();accessory_session_run(&s);
    assert(s.started && s.entry_result==1 && (strstr(s.text,"Kernel report (")!=NULL));verify_log("did not remain resident");
    reset();cfg.source_missing=1;source();accessory_session_run(&s);
    assert(!(s.stage_result>=0) && !load_calls && !s.log_result);verify_log("Staging failed");
    reset();cfg.stage_fail=1;source();accessory_session_run(&s);
    assert(s.stage_result==(int)0x8001001C && !load_calls);verify_log("Staging failed");
    reset();cfg.invalid_self=1;source();accessory_session_run(&s);
    assert(s.stage_result==(int)0x80010008 && !load_calls);verify_log("Staging failed");
    reset();cfg.size_mismatch=1;source();accessory_session_run(&s);
    assert(s.stage_result==(int)0x80010005 && !load_calls);verify_log("Staging failed");
    reset();cfg.short_io=1;source();accessory_session_run(&s);
    assert((s.load_result==42) && s.started && !s.log_result);verify_log("Staged bytes: 5003 / 5003");
    reset();cfg.write_zero=1;source();accessory_session_run(&s);
    assert(s.stage_result==(int)0x80010005 && !load_calls);verify_log("Staging failed");
    reset();cfg.user_log_fail=1;source();accessory_session_run(&s);
    assert(s.log_result==(int)0x8001001E && s.started && strstr(s.text,"Kernel START API"));
    reset();cfg.data_dir_fail=1;source();accessory_session_run(&s);
    assert(s.log_result==(int)0x8001001E && !load_calls && strstr(s.text,"directory creation failed"));
    reset();cfg.unsafe=1;source();accessory_session_run(&s);
    assert(!load_calls && !s.log_result);verify_log("Enable Unsafe Homebrew");
    reset();cfg.missing_kernel=1;source();accessory_session_run(&s);
    assert(s.started && !(strstr(s.text,"Kernel report (")!=NULL) && !s.log_result);verify_log("No current kernel report");
    reset();cfg.stale_kernel=1;source();accessory_session_run(&s);
    assert(s.started && !(strstr(s.text,"Kernel report (")!=NULL));verify_log("older run");
    reset();source();accessory_session_run(&s);
    char p[512];FILE *f=fopen(mapped(ACCESSORY_KERNEL_FALLBACK,p),"ab");assert(f);
    fputs("Resume cycle 1\nResume uma0 directory: accessible\n",f);fclose(f);
    accessory_session_refresh(&s);verify_log("Resume uma0 directory: accessible");
    size_t used=s.used;accessory_session_refresh(&s);
    assert(s.used==used && load_calls==1 && start_calls==1); /* No duplicate report or module load. */
    reset();source();accessory_session_run(&s);
    f=fopen(mapped(ACCESSORY_KERNEL_FALLBACK,p),"wb");assert(f);
    fputs("USBMC Accessory\nRequest: AABBCCDD\nSTALE RESUME\n",f);fclose(f);
    accessory_session_refresh(&s);assert(!(strstr(s.text,"Kernel report (")!=NULL) && !strstr(s.text,"STALE RESUME"));
    verify_log("older run");
    reset();memset(&s,0,sizeof(s));accessory_session_refresh(&s);
    assert(!s.attempted && !load_calls && !start_calls);
    reset();cfg.status_mode=1;assert(accessory_status_query(0x12345678,NULL)==0 && unload_calls==1);
    reset();cfg.status_mode=1;cfg.start_error=1;assert(accessory_status_query(0x12345678,NULL)<0 && unload_calls==1);
    reset();cfg.status_mode=1;cfg.entry_error=1;assert(accessory_status_query(0x12345678,NULL)<0 && unload_calls==1);
    reset();cfg.status_mode=cfg.original_mode=1;assert(accessory_original_prepare(0x12345678,NULL)==0 && unload_calls==1);
    reset();cfg.status_mode=cfg.original_mode=1;cfg.start_error=1;assert(accessory_original_prepare(0x12345678,NULL)<0 && unload_calls==1);
    reset();cfg.status_mode=cfg.original_mode=1;cfg.entry_error=1;assert(accessory_original_prepare(0x12345678,NULL)<0 && unload_calls==1);
    printf("app: %d loader/logging cases passed\n",cases);
    assert(cases==25);return 0;
}
