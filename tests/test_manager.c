#define _XOPEN_SOURCE 700
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "../src/app/manager.c"
#include "../src/app/prepare.c"

/* Convenience adapters belong to the tests, not the shipped manager API. */
static int am_save_settings(const AmSettings *s,char *message,size_t cap) {
    return am_save_settings_with_callbacks(s,NULL,message,cap);
}
static int am_save_settings_with_progress(const AmSettings *s,AmCopyCallback progress,void *ctx,char *message,size_t cap) {
    AmSaveCallbacks callbacks={progress,NULL,ctx};
    return am_save_settings_with_callbacks(s,&callbacks,message,cap);
}

static char root[160],fd_paths[2048][256];
static int cases,progress_calls;
static DIR *dir_handles[2048];
static struct {
    int module,legacy,raw,partial,corrupt,full,sync_fail,write_failed,rename_failed;
    const char *fail_write,*fail_rename,*fail_remove;
    unsigned last_percent;
    int copy_full,copy_corrupt,final_corrupt,copy_callbacks;
    unsigned copy_percent;
    const char *fail_open;
    int zero_copy_write,copy_close_fail,dir_error,invalid_dir_name;
    int source_change,tai_change,boot_change,yamt_change,lose_usb;
    int request_calls,request_error,request_no_mount;
} cfg;
static int err(void) { return (int)(0x80010000u|(unsigned)errno); }
static char *mapped(const char *p,char out[512]) { snprintf(out,512,"%s/%s",root,p);return out; }
static void fixture(const char *path,const void *bytes,size_t n) {
    char p[512];FILE *f=fopen(mapped(path,p),"wb");assert(f);assert(fwrite(bytes,1,n,f)==n);fclose(f);
}
static void put(const char *p,const char *s) { fixture(p,s,strlen(s)); }
static void get(const char *p,char out[65536]) {
    char path[512];FILE *f=fopen(mapped(p,path),"rb");assert(f);size_t n=fread(out,1,65535,f);out[n]=0;fclose(f);
}
static int local_exists(const char *p) { char path[512];struct stat st;return stat(mapped(p,path),&st)==0; }
int sceIoOpen(const char *p,int flags,int mode) {
    assert(strncmp(p,"gro0:",5) && strncmp(p,"grw0:",5));
    if (!strncmp(p,"sdstor0:",8)) { if (!cfg.raw) return (int)0x80010013;p="raw.bin"; }
    if (cfg.fail_open && !strcmp(p,cfg.fail_open)) return (int)0x80010005;
    char path[512];int fd=open(mapped(p,path),flags,mode);if (fd<0) return err();
    assert(fd<2048);snprintf(fd_paths[fd],sizeof(fd_paths[fd]),"%s",p);return fd;
}
int sceIoClose(int fd) {
    int r=close(fd)<0?err():0;
    if (cfg.copy_close_fail && strstr(fd_paths[fd],"/new/")) { cfg.copy_close_fail=0;return (int)0x80010005; }
    return r;
}
int sceIoRead(int fd,void *p,size_t n) {
    if (cfg.partial && n>513) n=513;
    int ret=(int)read(fd,p,n);
    if (ret>0 && cfg.corrupt && strstr(fd_paths[fd],"verify-")) { ((unsigned char *)p)[0]^=1;cfg.corrupt=0; }
    if (ret>0 && cfg.copy_corrupt && strstr(fd_paths[fd],"/prepare-") && strstr(fd_paths[fd],"/new/")) { ((unsigned char *)p)[0]^=1;cfg.copy_corrupt=0; }
    if (ret>0 && cfg.final_corrupt && !strncmp(fd_paths[fd],"uma0:app/",9)) { ((unsigned char *)p)[0]^=1;cfg.final_corrupt=0; }
    return ret<0?err():ret;
}
int sceIoWrite(int fd,const void *p,size_t n) {
    assert(strncmp(fd_paths[fd],"ux0:app/",8)); /* Source app files are never opened for writes. */
    if (cfg.fail_write && !cfg.write_failed && !strcmp(fd_paths[fd],cfg.fail_write)) { cfg.write_failed=1;return (int)0x8001001C; }
    if (cfg.full && strstr(fd_paths[fd],"verify-") && lseek(fd,0,SEEK_CUR)>=32768) return (int)0x8001001C;
    if (cfg.copy_full && strstr(fd_paths[fd],"/prepare-") && strstr(fd_paths[fd],"/new/") && lseek(fd,0,SEEK_CUR)>=32768) return (int)0x8001001C;
    if (cfg.zero_copy_write && strstr(fd_paths[fd],"/prepare-") && strstr(fd_paths[fd],"/new/")) return 0;
    if (cfg.partial && n>513) n=513;
    int r=(int)write(fd,p,n);return r<0?err():r;
}
int sceIoMkdir(const char *p,int mode) { char path[512];return mkdir(mapped(p,path),mode)<0?err():0; }
int sceIoGetstat(const char *p,SceIoStat *out) { char path[512];struct stat st;if (lstat(mapped(p,path),&st)<0) return err();out->st_size=st.st_size;out->st_mode=st.st_mode;return 0; }
int sceIoRemove(const char *p) {
    if (cfg.fail_remove && !strcmp(p,cfg.fail_remove)) return (int)0x8001001E;
    char path[512];return unlink(mapped(p,path))<0?err():0;
}
int sceIoRename(const char *a,const char *b) {
    if (cfg.fail_rename && !cfg.rename_failed && !strcmp(b,cfg.fail_rename)) { cfg.rename_failed=1;return (int)0x80010005; }
    char x[512],y[512];return rename(mapped(a,x),mapped(b,y))<0?err():0;
}
int sceIoDopen(const char *p) {
    char path[512];DIR *d=opendir(mapped(p,path));if (!d) return err();
    int fd=dirfd(d);assert(fd<2048 && !dir_handles[fd]);dir_handles[fd]=d;return fd;
}
int sceIoDread(int fd,SceIoDirent *out) {
    if (cfg.dir_error) { cfg.dir_error=0;return (int)0x80010005; }
    if (cfg.invalid_dir_name) { cfg.invalid_dir_name=0;memset(out,0,sizeof(*out));strcpy(out->d_name,"../escape");return 1; }
    assert(fd<2048 && dir_handles[fd]);errno=0;struct dirent *e=readdir(dir_handles[fd]);
    if (!e) return errno?err():0;
    struct stat st;if (fstatat(fd,e->d_name,&st,AT_SYMLINK_NOFOLLOW)<0) return err();
    memset(out,0,sizeof(*out));snprintf(out->d_name,sizeof(out->d_name),"%s",e->d_name);
    out->d_stat.st_size=st.st_size;out->d_stat.st_mode=st.st_mode;return 1;
}
int sceIoDclose(int fd) { assert(fd<2048 && dir_handles[fd]);DIR *d=dir_handles[fd];dir_handles[fd]=NULL;return closedir(d)<0?err():0; }
int sceIoRmdir(const char *p) { char path[512];return rmdir(mapped(p,path))<0?err():0; }
int64_t sceIoLseek(int fd,int64_t off,int whence) { return lseek(fd,off,whence); }
int sceIoSync(const char *p,unsigned flags) { assert((!strcmp(p,"uma0:")||!strcmp(p,"ux0:")) && !flags);return cfg.sync_fail?(int)0x80010005:0; }
uint32_t sceKernelGetProcessTimeLow(void) { return 0x11223344; }
int _vshKernelSearchModuleByName(const char *name,const void *work) {
    assert(work);
    if ((!strcmp(name,AM_PROBE_NAME)||!strcmp(name,AM_KERNEL_NAME)) && cfg.module) return 0x1234;
    if ((!strcmp(name,"USBMCAccessoryProbe")||!strcmp(name,"USBMCAccessory")) && cfg.legacy) return 0x5678;
    return -1;
}
static void reset(void) {
    for (unsigned i=0;i<2048;++i) assert(!dir_handles[i]);
    am_expect_query(0x1234);memset(&cfg,0,sizeof(cfg));cfg.module=cfg.raw=1;progress_calls=0;
    char template[]="/tmp/psvsdam-manager-XXXXXX";char *p=mkdtemp(template);assert(p);snprintf(root,sizeof(root),"%s",p);
    const char *dirs[]={"ur0:data",ACCESSORY_KERNEL_DIR,"ur0:tai","ux0:","uma0:","ux0:data","uma0:data","app0:"};
    for (unsigned i=0;i<sizeof(dirs)/sizeof(dirs[0]);++i) { char path[512];assert(!mkdir(mapped(dirs[i],path),0700)); }
    put(AM_TAI_CONFIG,"# Original\n*KERNEL\nur0:tai/nonpdrm.skprx\n*ALL\nur0:tai/custom.suprx\n");
    fixture(AM_BOOT_SOURCE,"SCE\0a-test-self",15);
    fixture(AM_STATUS_SOURCE,"SCE\0status-self",15);
    fixture(ACCESSORY_SOURCE_PROBE,"SCE\0probe-self",14);
    put("app0:eboot.bin","app");
    put("raw.bin","RAW");
    put(ACCESSORY_KERNEL_DIR "/runtime.txt","PSVSD/AM " ACCESSORY_VERSION "\nRequest: 00001234\nMount: uma0:\n");
    ++cases;
}
static void progress_cb(unsigned n,const char *stage,void *p) {
    (void)p;assert(stage && n>=cfg.last_percent && n<=100);cfg.last_percent=n;++progress_calls;
}
static void prepare_main(void) {
    char path[512];const char *dirs[]={"uma0:app","uma0:app/RTPSV0001","uma0:app/VITASHELL",
        "ux0:app","ux0:app/RTPSV0001","ux0:app/VITASHELL"};
    for (unsigned i=0;i<sizeof(dirs)/sizeof(dirs[0]);++i) assert(!mkdir(mapped(dirs[i],path),0700));
    put("uma0:app/RTPSV0001/eboot.bin","app");put("uma0:app/VITASHELL/eboot.bin","shell");
    put("ux0:app/RTPSV0001/eboot.bin","app");put("ux0:app/VITASHELL/eboot.bin","shell");
    const char *names[]={"psvsd_am.skprx","psvsd_am_status.skprx","usbmc_accessory_probe.skprx"};
    for (unsigned i=0;i<3;++i) {
        char source[128],target[128];snprintf(source,sizeof(source),"app0:%s",names[i]);
        snprintf(target,sizeof(target),"uma0:app/RTPSV0001/%s",names[i]);
        unsigned char *data=NULL;size_t n=0;assert(read_bytes(source,&data,&n)==0);fixture(target,data,n);free(data);
        snprintf(target,sizeof(target),"ux0:app/RTPSV0001/%s",names[i]);
        data=NULL;n=0;assert(read_bytes(source,&data,&n)==0);fixture(target,data,n);free(data);
    }
}
static void copy_fixture(void) {
    prepare_main();char p[512];
    assert(!mkdir(mapped("ux0:app/RTPSV0001/assets",p),0700));
    assert(!mkdir(mapped("ux0:app/VITASHELL/resources",p),0700));
    assert(!mkdir(mapped("ux0:app/VITASHELL/resources/empty",p),0700));
    put("ux0:app/RTPSV0001/assets/.hidden","hidden content");
    put("ux0:app/VITASHELL/resources/zero","");
    unsigned char data[70001];for (unsigned i=0;i<sizeof(data);++i) data[i]=(unsigned char)(i*73);
    fixture("ux0:app/VITASHELL/resources/data.bin",data,sizeof(data));
    put("uma0:app/RTPSV0001/eboot.bin","old app");put("uma0:app/VITASHELL/eboot.bin","old shell");
    put("uma0:app/VITASHELL/user-extra","keep this extra destination file");
}
static void copy_cb(const AmCopyProgress *p,void *ctx) {
    (void)ctx;assert(p->percent>=cfg.copy_percent && p->percent<=100);
    assert(p->bytes_done<=p->total_bytes && p->files_done<=p->total_files && p->phase[0]);
    cfg.copy_percent=p->percent;++cfg.copy_callbacks;
    if (cfg.source_change && !strcmp(p->phase,"Copying app files") && !strcmp(p->path,"VITASHELL/resources/data.bin")) {
        char file[512];FILE *f=fopen(mapped("ux0:app/VITASHELL/resources/data.bin",file),"ab");assert(f);fputc(1,f);fclose(f);cfg.source_change=0;
    }
    if (cfg.lose_usb && !strcmp(p->phase,"Copying app files")) { cfg.module=0;cfg.lose_usb=0; }
    if (p->percent==100) {
        if (cfg.tai_change) put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/changed-elsewhere.skprx\n");
        if (cfg.boot_change) put("ur0:tai/boot_config.txt","load ur0:tai/changed-elsewhere.skprx\n");
        if (cfg.yamt_change) fixture("ur0:tai/yamt.cfg","4110",4);
    }
}
static int request_cb(void *ctx,char *message,size_t cap) {
    (void)ctx;++cfg.request_calls;
    if (cfg.request_error) { snprintf(message,cap,"USB request failed");return COPY_ERROR; }
    if (!cfg.request_no_mount) {
        cfg.module=1;put(ACCESSORY_KERNEL_DIR "/runtime.txt","PSVSD/AM " ACCESSORY_VERSION "\nRequest: 00001234\nMount: uma0:\n");
    }
    return 0;
}
int main(void) {
    AmSettings s;AmTestResult r;AmStatus status;char message[192],out[65536],other[65536];
    reset();assert(am_load_settings(&s)==0 && !s.enabled && s.target==AM_UMA0);
    reset();put(AM_SETTINGS_PATH,"invalid");assert(am_load_settings(&s)<0 && !s.enabled);
    reset();s=(AmSettings){1,AM_UMA0};assert(am_save_settings(&s,message,sizeof(message))==0);
    assert(am_load_settings(&s)==0 && s.enabled && s.target==AM_UMA0);
    get(AM_TAI_CONFIG,out);assert(strstr(out,AM_BOOT_PLUGIN) && strstr(out,"nonpdrm.skprx") && strstr(out,"custom.suprx"));
    get(AM_TAI_CONFIG ".psvsdam.bak",other);assert(!strstr(other,AM_BOOT_PLUGIN));
    assert(am_save_settings(&s,message,sizeof(message))==0);get(AM_TAI_CONFIG,other);assert(!strcmp(out,other));
    s.enabled=0;assert(am_save_settings(&s,message,sizeof(message))==0);get(AM_TAI_CONFIG,out);
    assert(!strstr(out,AM_BOOT_PLUGIN) && strstr(out,"nonpdrm.skprx") && strstr(out,"custom.suprx"));
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/StorageMgr.skprx\n");put("ur0:tai/storage_config.txt","UMA=grw0\n");s=(AmSettings){1,AM_UMA0};
    assert(am_save_settings(&s,message,sizeof(message))<0 && !local_exists(AM_SETTINGS_PATH));
    reset();put(AM_TAI_CONFIG,"*KERNEL\n# ur0:tai/storagemgr.skprx\n");assert(am_save_settings(&s,message,sizeof(message))==0);
    reset();char path[512];assert(!mkdir(mapped("ux0:tai",path),0700));put("ux0:tai/config.txt","config");
    assert(am_save_settings(&s,message,sizeof(message))<0 && strstr(message,"ur0"));
    reset();put("ur0:tai/boot_config.txt","# path\nload ur0:tai/yamt.skprx\n");
    assert(am_save_settings(&s,message,sizeof(message))<0 && strstr(message,"YAMT"));
    reset();put(AM_TAI_CONFIG,"*KERNEL\n# BEGIN PSVSD/AM\nur0:tai/other.skprx\n# END PSVSD/AM\n");
    assert(am_save_settings(&s,message,sizeof(message))<0);get(AM_TAI_CONFIG,out);assert(strstr(out,"other.skprx"));
    reset();cfg.fail_write=AM_TAI_CONFIG ".psvsdam.new";
    get(AM_TAI_CONFIG,out);assert(am_save_settings(&s,message,sizeof(message))<0);
    get(AM_TAI_CONFIG,other);assert(!strcmp(out,other) && !local_exists(AM_SETTINGS_PATH));
    reset();put(AM_SETTINGS_PATH,"version=1\nenabled=0\nmount=ux0\n");cfg.fail_rename=AM_TAI_CONFIG;
    get(AM_TAI_CONFIG,out);assert(am_save_settings(&s,message,sizeof(message))<0);
    get(AM_TAI_CONFIG,other);assert(!strcmp(out,other));
    assert(am_load_settings(&s)==0 && !s.enabled && s.target==AM_UX0);
    assert(!local_exists(AM_TAI_CONFIG ".psvsdam.swap"));
    reset();put(AM_TAI_CONFIG ".psvsdam.swap","unfinished");s=(AmSettings){1,AM_UMA0};
    assert(am_save_settings(&s,message,sizeof(message))<0 && local_exists(AM_TAI_CONFIG ".psvsdam.swap"));
    reset();s=(AmSettings){1,AM_UX0};assert(am_save_settings(&s,message,sizeof(message))<0 && strstr(message,"copy failed"));
    reset();prepare_main();assert(am_save_settings(&s,message,sizeof(message))==0);
    assert(am_load_settings(&s)==0 && s.target==AM_UX0);
    reset();prepare_main();put("uma0:app/RTPSV0001/eboot.bin","old version");
    assert(am_save_settings(&s,message,sizeof(message))==0);get("uma0:app/RTPSV0001/eboot.bin",out);assert(!strcmp(out,"app"));
    reset();assert(am_stage_status()==0 && local_exists(AM_STATUS_STAGED));
    reset();cfg.partial=1;s=(AmSettings){1,AM_UMA0};assert(am_save_settings(&s,message,sizeof(message))==0);
    reset();am_status(&status);assert(status.verified_mount && status.actual_target==AM_UMA0);
    reset();put(ACCESSORY_KERNEL_DIR "/runtime.txt","PSVSD/AM " ACCESSORY_VERSION "\nRequest: 00009999\nMount: uma0:\n");
    am_status(&status);assert(status.loaded && !status.verified_mount);
    assert(am_test_io(AM_UMA0,&r,NULL,NULL)<0 && !local_exists("uma0:data/PSVSD_AM"));
    reset();cfg.module=0;cfg.legacy=1;am_status(&status);assert(!status.loaded && status.legacy_loaded && !status.verified_mount);
    reset();assert(am_test_io(AM_UMA0,&r,progress_cb,NULL)==0);
    assert(r.written==4*1024*1024 && r.read==r.written && !r.cleanup_result && progress_calls>1 && cfg.last_percent==100);
    assert(!local_exists(r.path));
    reset();cfg.partial=1;assert(am_test_io(AM_UMA0,&r,NULL,NULL)==0 && r.read==4*1024*1024);
    reset();cfg.corrupt=1;assert(am_test_io(AM_UMA0,&r,NULL,NULL)<0 && !local_exists(r.path));
    reset();cfg.full=1;assert(am_test_io(AM_UMA0,&r,NULL,NULL)<0 && r.written==32768 && !local_exists(r.path));
    reset();cfg.sync_fail=1;assert(am_test_io(AM_UMA0,&r,NULL,NULL)<0 && r.sync_result<0 && !local_exists(r.path));
    reset();assert(am_test_io(AM_UX0,&r,NULL,NULL)<0 && !local_exists("ux0:data/PSVSD_AM"));
    reset();assert(!mkdir(mapped("uma0:data/PSVSD_AM",path),0700));
    for (unsigned i=0;i<8;++i) { char f[128];snprintf(f,sizeof(f),"uma0:data/PSVSD_AM/verify-%08X.bin",0x11223344+i);put(f,"KEEP"); }
    assert(am_test_io(AM_UMA0,&r,NULL,NULL)==EEXIST_VALUE);get(r.path,out);assert(!strcmp(out,"KEEP"));
    reset();put(ACCESSORY_KERNEL_DIR "/runtime.txt","PSVSD/AM " ACCESSORY_VERSION "\nRequest: 00001234\nMount: ux0:\n");
    assert(am_test_io(AM_UX0,&r,NULL,NULL)==0 && r.read==4*1024*1024);
    reset();put(ACCESSORY_KERNEL_LOG,"KERNEL RESULT\n");am_log("APPLICATION RESULT\n");
    assert(am_export_report("EXTRA",message,sizeof(message))==0);get(ACCESSORY_USER_LOG,out);
    assert(strstr(out,"KERNEL RESULT") && strstr(out,"APPLICATION RESULT") && strstr(out,"EXTRA"));
    reset();char large[20001];memset(large,'A',sizeof(large)-1);large[19990]='\n';large[20000]=0;
    memcpy(large+19991,"TAIL END",8);put(AM_APP_LOG,large);
    assert(am_read_log(AM_APP_LOG,message,sizeof(message))>=0 && strstr(message,"TAIL END"));
    assert(am_read_text(AM_APP_LOG,message,sizeof(message))<0);
    reset();assert(am_tai_transform("*KERNEL\r\nur0:tai/keep.skprx\r\n*ALL\r\n",1,out,sizeof(out))>=0);
    assert(strstr(out,"ur0:tai/keep.skprx\r\n*ALL\r\n"));
    assert(am_tai_transform(out,0,other,sizeof(other))>=0 && !strstr(other,AM_BOOT_PLUGIN));
    reset();assert(am_tai_transform("# BEGIN PSVSD/AM\n",0,out,sizeof(out))<0);
    assert(am_tai_transform("# END PSVSD/AM\n",0,out,sizeof(out))<0);
    assert(am_tai_transform("*KERNEL\n",1,out,2)<0 && am_tai_transform("",1,out,0)<0);
    reset();assert(am_settings_parse("version=1\nenabled=1\nmount=ux0\n",&s)==0 && s.enabled && s.target==AM_UX0);
    assert(am_settings_parse("version=1\nenabled=1\nenabled=0\nmount=uma0\n",&s)<0);
    assert(am_settings_parse("version=1\nenabled=2\nmount=uma0\n",&s)<0);
    assert(am_settings_parse("version=1\nenabled=1\nmount=gro0\n",&s)<0);
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/StorageMgr.skprx\n");
    put("ur0:tai/storage_config.txt","GCD=ux0\nMCD=xmc0\nINT=imc0\n");s=(AmSettings){1,AM_UMA0};
    assert(am_save_settings(&s,message,sizeof(message))==0);get(AM_TAI_CONFIG,out);
    assert(strstr(out,"StorageMgr.skprx") && strstr(out,AM_BOOT_PLUGIN));
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/storagemgr.skprx\n");
    put("ur0:tai/storage_config.txt","GCD=ux0\nMCD=uma0\n");
    assert(am_save_settings(&s,message,sizeof(message))<0 && strstr(message,"StorageMgr reserves"));
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/nonpdrm.skprx # usbmc.skprx reference\n*ALL\nur0:tai/usbmc.skprx\n");
    assert(am_save_settings(&s,message,sizeof(message))==0);
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/my-usbmc.skprx\n");assert(am_save_settings(&s,message,sizeof(message))==0);
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/USBMC.skprx\n");
    assert(am_save_settings(&s,message,sizeof(message))<0 && strstr(message,"usbmc.skprx"));
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/gamesd.skprx\n");assert(am_save_settings(&s,message,sizeof(message))==0);
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/storagemgr.skprx\n");
    put("ur0:tai/storage_config.txt"," GCD = ux0  # main storage\n# UMA=grw0\nMCD=xmc0\n");
    assert(am_save_settings(&s,message,sizeof(message))==0);
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/storagemgr.skprx\n");put("ur0:tai/storage_config.txt","GCD=ux0\n");
    s.target=AM_UX0;assert(am_save_settings(&s,message,sizeof(message))<0 && strstr(message,"reserves ux0"));s.target=AM_UMA0;
    unsigned char yamt[200]={0};yamt[0]=4;yamt[1]=1;yamt[2]=1;yamt[12]=8;yamt[13]=1;yamt[16]=2;
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/yamt_helper.skprx\n");fixture("ur0:tai/yamt.cfg",yamt,sizeof(yamt));
    assert(am_save_settings(&s,message,sizeof(message))==0);
    reset();put("ur0:tai/boot_config.txt","load ur0:tai/yamt.skprx\n");fixture("ur0:tai/yamt.cfg",yamt,sizeof(yamt));
    assert(am_save_settings(&s,message,sizeof(message))==0);
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/yamt_helper.skprx\n");yamt[4]=1;fixture("ur0:tai/yamt.cfg",yamt,sizeof(yamt));
    assert(am_save_settings(&s,message,sizeof(message))<0 && strstr(message,"USB support"));yamt[4]=0;
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/yamt_helper.skprx\n");yamt[12]=15;fixture("ur0:tai/yamt.cfg",yamt,sizeof(yamt));
    assert(am_save_settings(&s,message,sizeof(message))<0 && strstr(message,"YAMT owns"));yamt[12]=8;
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/yamt_helper.skprx\n");fixture("ur0:tai/yamt.cfg",yamt,16);
    assert(am_save_settings(&s,message,sizeof(message))<0 && strstr(message,"YAMT settings"));
    reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/yamt_helper.skprx\n");yamt[2]=0;fixture("ur0:tai/yamt.cfg",yamt,sizeof(yamt));
    assert(am_save_settings(&s,message,sizeof(message))==0);yamt[2]=1;
    /* The Lite settings UI writes ASCII '4', enabled, ux0 device, uma0 device.
       Exercise both targets and all source devices through the real Save path. */
    static const struct { const char *cfg;int target,blocked;const char *device; } lite[]={
        {"4100",AM_UX0,0,NULL},{"4100",AM_UMA0,0,NULL},
        {"4110",AM_UX0,1,"SD2Vita"},{"4110",AM_UMA0,0,NULL},
        {"4101",AM_UX0,0,NULL},{"4101",AM_UMA0,1,"SD2Vita"},
        {"4120",AM_UX0,1,"Memory Card"},{"4120",AM_UMA0,0,NULL},
        {"4102",AM_UX0,0,NULL},{"4102",AM_UMA0,1,"Memory Card"},
        {"4130",AM_UX0,1,"Internal Storage"},{"4130",AM_UMA0,0,NULL},
        {"4103",AM_UX0,0,NULL},{"4103",AM_UMA0,1,"Internal Storage"},
        {"4112",AM_UX0,1,"SD2Vita"},{"4112",AM_UMA0,1,"Memory Card"},
        {"4012",AM_UX0,0,NULL},{"4012",AM_UMA0,0,NULL},{"4000",AM_UMA0,0,NULL}
    };
    for (unsigned i=0;i<sizeof(lite)/sizeof(lite[0]);++i) {
        reset();put("ur0:tai/boot_config.txt","# YAMT\n- load\tur0:tai/yamt.skprx\n");
        fixture("ur0:tai/yamt.cfg",lite[i].cfg,4);s=(AmSettings){1,lite[i].target};
        if (s.target==AM_UX0) prepare_main();
        get(AM_TAI_CONFIG,out);
        int result=am_save_settings(&s,message,sizeof(message));
        if (lite[i].blocked) {
            assert(result<0 && strstr(message,"YAMT Lite maps") && strstr(message,lite[i].device));
            assert(strstr(message,am_target_name(s.target)) && !local_exists(AM_SETTINGS_PATH));
            get(AM_TAI_CONFIG,other);assert(!strcmp(out,other));
            assert(!local_exists(AM_BOOT_PLUGIN));
        } else assert(result==0);
        unsigned char *check=NULL;size_t n=0;assert(read_bytes("ur0:tai/yamt.cfg",&check,&n)==0);
        assert(n==4 && !memcmp(check,lite[i].cfg,4));free(check);
        get(AM_APP_LOG,other);assert(strstr(other,"YAMT Lite v4 (4 bytes)"));
    }
    static const struct { unsigned char cfg[8];size_t size; } invalid_lite[]={
        {{'4','1','0'},3},{{'4','1','0','0','0'},5},{{'3','1','0','0'},4},
        {{'4','2','0','0'},4},{{'4','1','4','0'},4},{{'4','1','0',255},4},
        {{4,1,0,0},4},{{0},0}
    };
    for (unsigned i=0;i<sizeof(invalid_lite)/sizeof(invalid_lite[0]);++i) {
        reset();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/yamt_helper.skprx\n");
        fixture("ur0:tai/yamt.cfg",invalid_lite[i].cfg,invalid_lite[i].size);s=(AmSettings){1,AM_UMA0};
        assert(am_save_settings(&s,message,sizeof(message))<0 && strstr(message,"YAMT settings"));
        assert(!local_exists(AM_SETTINGS_PATH) && !local_exists(AM_BOOT_PLUGIN));
        get(AM_APP_LOG,other);assert(strstr(other,"unsupported config format") && strstr(other,"first bytes:"));
    }
    s=(AmSettings){1,AM_UMA0};
    reset();put(ACCESSORY_KERNEL_DIR "/runtime.txt","stale");
    assert(am_save_settings(&s,message,sizeof(message))<0 && strstr(message,"already occupied"));
    reset();assert(sceIoMkdir(ACCESSORY_DATA_DIR,0700)==0);
    const char *logs[]={AM_APP_LOG,ACCESSORY_KERNEL_LOG,ACCESSORY_KERNEL_FALLBACK,ACCESSORY_USER_LOG,
        ACCESSORY_USER_LOG ".psvsdam.bak",ACCESSORY_USER_LOG ".psvsdam.new",ACCESSORY_USER_LOG ".psvsdam.swap"};
    for (unsigned i=0;i<sizeof(logs)/sizeof(logs[0]);++i) put(logs[i],"old diagnostic data");
    put(AM_SETTINGS_PATH,"settings retained");put(AM_SETTINGS_PATH ".psvsdam.bak","settings backup retained");
    put(AM_TAI_CONFIG ".psvsdam.bak","tai backup retained");put(AM_BOOT_PLUGIN,"plugin retained");
    put(ACCESSORY_DATA_DIR "/unrelated.txt","unrelated retained");get(AM_TAI_CONFIG,out);
    assert(am_clear_logs(message,sizeof(message))==0 && strstr(message,"Logs cleared"));
    for (unsigned i=0;i<sizeof(logs)/sizeof(logs[0]);++i) assert(!local_exists(logs[i]));
    get(AM_SETTINGS_PATH,other);assert(!strcmp(other,"settings retained"));
    get(AM_SETTINGS_PATH ".psvsdam.bak",other);assert(!strcmp(other,"settings backup retained"));
    get(AM_TAI_CONFIG ".psvsdam.bak",other);assert(!strcmp(other,"tai backup retained"));
    get(AM_TAI_CONFIG,other);assert(!strcmp(out,other));get(AM_BOOT_PLUGIN,other);assert(!strcmp(other,"plugin retained"));
    get(ACCESSORY_DATA_DIR "/unrelated.txt",other);assert(!strcmp(other,"unrelated retained"));
    get(ACCESSORY_KERNEL_DIR "/runtime.txt",other);assert(strstr(other,"Request: 00001234"));
    reset();assert(am_clear_logs(message,sizeof(message))==0);assert(am_clear_logs(message,sizeof(message))==0);
    reset();assert(sceIoMkdir(ACCESSORY_DATA_DIR,0700)==0);
    put(ACCESSORY_KERNEL_LOG,"kernel retained on error");put(ACCESSORY_USER_LOG,"report");cfg.fail_remove=ACCESSORY_KERNEL_LOG;
    assert(am_clear_logs(message,sizeof(message))<0 && strstr(message,"Clear Log failed"));
    assert(local_exists(ACCESSORY_KERNEL_LOG) && !local_exists(ACCESSORY_USER_LOG));
    get(AM_APP_LOG,other);assert(strstr(other,ACCESSORY_KERNEL_LOG) && strstr(other,"8001001E"));
    cfg.fail_remove=NULL;assert(am_clear_logs(message,sizeof(message))==0 && !local_exists(ACCESSORY_KERNEL_LOG) && !local_exists(AM_APP_LOG));
    reset();put(AM_APP_LOG,"preserve on invalid arguments");
    assert(am_clear_logs(message,0)<0);get(AM_APP_LOG,other);assert(!strcmp(other,"preserve on invalid arguments"));
    /* Save-to-ux0 integration: copy both complete installed app folders,
       overwrite existing files, preserve extras, verify, then commit settings. */
    reset();copy_fixture();s=(AmSettings){1,AM_UX0};cfg.partial=1;get(AM_TAI_CONFIG,out);
    assert(am_save_settings_with_progress(&s,copy_cb,NULL,message,sizeof(message))==0);
    assert(cfg.copy_callbacks>10 && cfg.copy_percent==100 && am_load_settings(&s)==0 && s.enabled && s.target==AM_UX0);
    get("uma0:app/RTPSV0001/eboot.bin",other);assert(!strcmp(other,"app"));
    get("uma0:app/VITASHELL/eboot.bin",other);assert(!strcmp(other,"shell"));
    get("uma0:app/VITASHELL/user-extra",other);assert(!strcmp(other,"keep this extra destination file"));
    get("uma0:app/RTPSV0001/assets/.hidden",other);assert(!strcmp(other,"hidden content"));
    SceIoStat copied;assert(sceIoGetstat("uma0:app/VITASHELL/resources/zero",&copied)==0 && !copied.st_size);
    assert(sceIoGetstat("uma0:app/VITASHELL/resources/empty",&copied)==0 && SCE_S_ISDIR(copied.st_mode));
    assert(same_file("ux0:app/VITASHELL/resources/data.bin","uma0:app/VITASHELL/resources/data.bin"));
    assert(!local_exists("uma0:data/PSVSD_AM/prepare-11223344"));
    get("ux0:app/RTPSV0001/eboot.bin",other);assert(!strcmp(other,"app"));
    get(AM_TAI_CONFIG ".psvsdam.bak",other);assert(!strcmp(out,other));
    reset();copy_fixture();assert(sceIoRename("uma0:app","uma0:keep-original-app")==0);s=(AmSettings){1,AM_UX0};
    assert(am_save_settings(&s,message,sizeof(message))==0 && local_exists("uma0:app/RTPSV0001/eboot.bin"));
    get("uma0:keep-original-app/RTPSV0001/eboot.bin",other);assert(!strcmp(other,"old app"));
    for (unsigned failure=0;failure<10;++failure) {
        reset();copy_fixture();s=(AmSettings){1,AM_UX0};get(AM_TAI_CONFIG,out);
        switch (failure) {
            case 0:cfg.copy_full=1;break;
            case 1:cfg.copy_corrupt=1;break;
            case 2:cfg.sync_fail=1;break;
            case 3:cfg.fail_rename="uma0:app/VITASHELL/eboot.bin";break;
            case 4:cfg.final_corrupt=1;break;
            case 5:cfg.zero_copy_write=1;break;
            case 6:cfg.copy_close_fail=1;break;
            case 7:cfg.fail_open="ux0:app/VITASHELL/resources/data.bin";break;
            case 8:cfg.dir_error=1;break;
            case 9:cfg.invalid_dir_name=1;break;
        }
        assert(am_save_settings_with_progress(&s,copy_cb,NULL,message,sizeof(message))<0);
        assert(!local_exists(AM_SETTINGS_PATH) && !local_exists(AM_BOOT_PLUGIN));
        get(AM_TAI_CONFIG,other);assert(!strcmp(out,other));
        get("uma0:app/RTPSV0001/eboot.bin",other);assert(!strcmp(other,"old app"));
        get("uma0:app/VITASHELL/eboot.bin",other);assert(!strcmp(other,"old shell"));
        get("uma0:app/VITASHELL/user-extra",other);assert(!strcmp(other,"keep this extra destination file"));
        assert(!local_exists("uma0:data/PSVSD_AM/prepare-11223344"));
    }
    reset();copy_fixture();cfg.source_change=1;s=(AmSettings){1,AM_UX0};
    assert(am_save_settings_with_progress(&s,copy_cb,NULL,message,sizeof(message))<0 && !local_exists(AM_SETTINGS_PATH));
    get("uma0:app/RTPSV0001/eboot.bin",other);assert(!strcmp(other,"old app"));
    reset();copy_fixture();assert(sceIoRemove("ux0:app/VITASHELL/eboot.bin")==0);s=(AmSettings){1,AM_UX0};
    assert(am_save_settings(&s,message,sizeof(message))<0 && !local_exists(AM_SETTINGS_PATH));
    reset();copy_fixture();assert(sceIoRemove("uma0:app/RTPSV0001/eboot.bin")==0);
    assert(sceIoMkdir("uma0:app/RTPSV0001/eboot.bin",0700)==0);s=(AmSettings){1,AM_UX0};
    assert(am_save_settings(&s,message,sizeof(message))<0 && !local_exists(AM_SETTINGS_PATH));
    assert(sceIoGetstat("uma0:app/RTPSV0001/eboot.bin",&copied)==0 && SCE_S_ISDIR(copied.st_mode));
    reset();copy_fixture();put(ACCESSORY_KERNEL_DIR "/runtime.txt","stale");s=(AmSettings){1,AM_UX0};
    assert(am_save_settings_with_progress(&s,copy_cb,NULL,message,sizeof(message))<0 && !cfg.copy_callbacks);
    assert(!local_exists("uma0:data/PSVSD_AM"));
    reset();put(ACCESSORY_KERNEL_DIR "/runtime.txt","PSVSD/AM " ACCESSORY_VERSION "\nRequest: 00001234\nMount: ux0:\n");s=(AmSettings){1,AM_UX0};
    assert(am_save_settings_with_progress(&s,copy_cb,NULL,message,sizeof(message))==0 && !cfg.copy_callbacks);
    reset();copy_fixture();put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/yamt_helper.skprx\n");fixture("ur0:tai/yamt.cfg","4110",4);s=(AmSettings){1,AM_UX0};
    assert(am_save_settings_with_progress(&s,copy_cb,NULL,message,sizeof(message))<0 && !cfg.copy_callbacks);
    assert(!local_exists("uma0:data/PSVSD_AM"));
    for (unsigned changed=0;changed<3;++changed) {
        reset();copy_fixture();s=(AmSettings){1,AM_UX0};
        if (changed==0) cfg.tai_change=1;
        if (changed==1) cfg.boot_change=1;
        if (changed==2) { cfg.yamt_change=1;put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/yamt_helper.skprx\n");fixture("ur0:tai/yamt.cfg","4000",4); }
        assert(am_save_settings_with_progress(&s,copy_cb,NULL,message,sizeof(message))<0);
        assert(cfg.copy_percent==100 && !local_exists(AM_SETTINGS_PATH) && !local_exists(AM_BOOT_PLUGIN));
        if (changed==0) { get(AM_TAI_CONFIG,other);assert(strstr(other,"changed-elsewhere")); }
        if (changed==1) { get("ur0:tai/boot_config.txt",other);assert(strstr(other,"changed-elsewhere")); }
        if (changed==2) assert(strstr(message,"YAMT Lite maps"));
    }
    reset();copy_fixture();cfg.lose_usb=1;s=(AmSettings){1,AM_UX0};
    assert(am_save_settings_with_progress(&s,copy_cb,NULL,message,sizeof(message))<0 && !local_exists(AM_SETTINGS_PATH));
    assert(local_exists("uma0:data/PSVSD_AM/prepare-11223344"));
    get("uma0:app/RTPSV0001/eboot.bin",other);assert(!strcmp(other,"old app"));
    reset();copy_fixture();char deep[400]="ux0:app/VITASHELL";
    for (unsigned i=0;i<26;++i) { strcat(deep,"/d");assert(sceIoMkdir(deep,0700)==0); }
    s=(AmSettings){1,AM_UX0};assert(am_save_settings(&s,message,sizeof(message))<0 && !local_exists(AM_SETTINGS_PATH));
    reset();copy_fixture();char linkpath[512],linktarget[512];
    assert(!symlink(mapped("raw.bin",linktarget),mapped("ux0:app/VITASHELL/linked",linkpath)));
    s=(AmSettings){1,AM_UX0};assert(am_save_settings(&s,message,sizeof(message))<0 && !local_exists(AM_SETTINGS_PATH));
    reset();copy_fixture();size_t big_size=2*1024*1024+13;unsigned char *big=malloc(big_size);assert(big);
    for (size_t i=0;i<big_size;++i) big[i]=(unsigned char)(i*19);
    fixture("ux0:app/VITASHELL/resources/large.bin",big,big_size);s=(AmSettings){1,AM_UX0};
    assert(am_save_settings_with_progress(&s,copy_cb,NULL,message,sizeof(message))==0);
    char big_path[512];FILE *large_file=fopen(mapped("uma0:app/VITASHELL/resources/large.bin",big_path),"rb");assert(large_file);
    unsigned char chunk[8192];size_t position=0,amount;
    while ((amount=fread(chunk,1,sizeof(chunk),large_file))) { assert(!memcmp(chunk,big+position,amount));position+=amount; }
    assert(position==big_size && !ferror(large_file));fclose(large_file);free(big);
    AmSaveCallbacks callbacks={copy_cb,request_cb,NULL};
    reset();copy_fixture();cfg.module=0;s=(AmSettings){1,AM_UX0};
    assert(am_save_settings_with_callbacks(&s,&callbacks,message,sizeof(message))==0 && cfg.request_calls==1 && cfg.copy_percent==100);
    reset();copy_fixture();cfg.module=0;cfg.request_error=1;s=(AmSettings){1,AM_UX0};
    assert(am_save_settings_with_callbacks(&s,&callbacks,message,sizeof(message))<0 && cfg.request_calls==1 && !cfg.copy_callbacks);
    assert(!local_exists(AM_SETTINGS_PATH) && !local_exists("uma0:data/PSVSD_AM"));
    reset();copy_fixture();cfg.module=0;cfg.request_no_mount=1;s=(AmSettings){1,AM_UX0};
    assert(am_save_settings_with_callbacks(&s,&callbacks,message,sizeof(message))<0 && cfg.request_calls==1 && !cfg.copy_callbacks);
    assert(!local_exists(AM_SETTINGS_PATH));
    reset();copy_fixture();cfg.module=0;put(AM_TAI_CONFIG,"*KERNEL\nur0:tai/usbmc.skprx\n");s=(AmSettings){1,AM_UX0};
    assert(am_save_settings_with_callbacks(&s,&callbacks,message,sizeof(message))<0 && !cfg.request_calls && !cfg.copy_callbacks);
    reset();copy_fixture();put(ACCESSORY_KERNEL_DIR "/runtime.txt","PSVSD/AM " ACCESSORY_VERSION "\nRequest: 00001234\nMount: ux0:\n");s=(AmSettings){1,AM_UX0};
    assert(am_save_settings_with_callbacks(&s,&callbacks,message,sizeof(message))==0 && !cfg.request_calls && !cfg.copy_callbacks);
    printf("manager: %d configuration/status/read-write cases passed\n",cases);
    return 0;
}
