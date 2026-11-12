/* Bounded, restartable migration of known app-owned data. GPL-3.0-or-later. */
#include "migration.h"
#include "../common/paths.h"
#ifdef PSVSD_AM_HOST_TEST
#include "mock_migration.h"
#else
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#endif
#include <stdio.h>
#include <string.h>
#define MISSING ((int)0x80010002)
#define EXISTS ((int)0x80010011)
#define MAX_FILE (16u*1024u*1024u)
static int present(const char *path) {
    SceIoStat st;int ret=sceIoGetstat(path,&st);
    return ret==MISSING?0:ret<0?ret:1;
}
static int mkdir_ok(const char *path) {
    int ret=sceIoMkdir(path,0777);return ret==EXISTS?0:ret;
}
static int same(const char *a,const char *b) {
    FILE *in=fopen(a,"rb"),*out=fopen(b,"rb");int ok=0;
    if (in && out) {
        unsigned char x[4096],y[4096];size_t n,m;
        do {
            n=fread(x,1,sizeof(x),in);m=fread(y,1,sizeof(y),out);
            if (n!=m || memcmp(x,y,n)) goto done;
        } while (n);
        ok=!ferror(in) && !ferror(out);
    }
done:
    if (in && fclose(in)) ok=0;
    if (out && fclose(out)) ok=0;
    return ok;
}
static int copy_missing(const char *from,const char *to) {
    int found=present(to);if (found) return found<0?found:0;
    found=present(from);if (found<=0) return found;
    char temp[256];if (snprintf(temp,sizeof(temp),"%s.migrate-new",to)>=(int)sizeof(temp)) return -1;
    FILE *in=fopen(from,"rb");if (!in) return -1;
    FILE *out=fopen(temp,"wb");int ok=out!=NULL;size_t total=0;
    if (out) {
        unsigned char bytes[4096];size_t n;
        while ((n=fread(bytes,1,sizeof(bytes),in))) {
            if (total>MAX_FILE-n || fwrite(bytes,1,n,out)!=n) { ok=0;break; }
            total+=n;
        }
        if (ferror(in)) ok=0;
        if (fclose(out)) ok=0;
    }
    if (fclose(in)) ok=0;
    if (ok) ok=same(from,temp);
    if (ok) ok=sceIoRename(temp,to)>=0;
    if (!ok) { sceIoRemove(temp);return -1; }
    return 1;
}
static int complete(const char *path) {
    char bytes[2];FILE *f=fopen(path,"rb");if (!f) return 0;
    size_t n=fread(bytes,1,sizeof(bytes),f);int ok=!ferror(f) && n==1 && bytes[0]=='1';
    if (fclose(f)) ok=0;
    return ok;
}
static int mark_complete(const char *path) {
    FILE *f=fopen(path,"wb");if (!f) return -1;
    int ok=fputc('1',f)!=EOF;if (fclose(f)) ok=0;return ok?0:-1;
}
int am_migrate_data(void) {
    if (mkdir_ok("ux0:data")<0 || mkdir_ok(ACCESSORY_DATA_DIR)<0 ||
        mkdir_ok("ur0:data")<0 || mkdir_ok(ACCESSORY_KERNEL_DIR)<0) return -1;
    static const char *user_files[]={"test.log","kernel.log"};
    static const char *system_files[]={"settings.ini","settings.ini.psvsdam.bak",
        "app.log","kernel.log","artwork-version","artwork-pending"};
    int result=0;
    for (unsigned group=0;group<2;++group) {
        const char *target=group?ACCESSORY_KERNEL_DIR:ACCESSORY_DATA_DIR;
        char marker[256];snprintf(marker,sizeof(marker),"%s/legacy-migration.done",target);
        if (complete(marker)) continue;
        const char *const *files=group?system_files:user_files;
        unsigned count=group?sizeof(system_files)/sizeof(system_files[0]):sizeof(user_files)/sizeof(user_files[0]);
        int group_error=0;
        for (unsigned i=0;i<count;++i) {
            char from[256],to[256];
            snprintf(from,sizeof(from),"%s/%s",group?AM_LEGACY_KERNEL_DIR:AM_LEGACY_DATA_DIR,files[i]);
            snprintf(to,sizeof(to),"%s/%s",target,files[i]);
            int ret=copy_missing(from,to);if (ret<0) group_error=ret;
        }
        if (!group_error) group_error=mark_complete(marker);
        if (group_error<0) result=group_error;else if (!result) result=1;
    }
    return result;
}
static int ready_for_this_helper(const char *path) {
    int found=present(path);if (found<=0) return found;
    char text[8]={0};
    FILE *f=fopen(path,"rb");if (!f) return -1;
    size_t n=fread(text,1,sizeof(text),f);int ok=!ferror(f);if (fclose(f)) ok=0;
    return ok && (n==5 || (n==6 && !text[5])) && !memcmp(text,ACCESSORY_PACKAGE_VERSION,5)?1:-1;
}
int am_migrate_update(void) {
    const char *old=AM_LEGACY_DATA_DIR "/update",*current=AM_UPDATE_ROOT;
    const char *backup=ACCESSORY_DATA_DIR "/update.pre-migration";
    int active=ready_for_this_helper(AM_UPDATE_READY);if (active==1) return 0;
    int legacy=ready_for_this_helper(AM_LEGACY_DATA_DIR "/update/ready");
    if (legacy<=0) return legacy<0 || active<0?-1:0;
    if (mkdir_ok(ACCESSORY_DATA_DIR)<0) return -1;
    int had=present(current);if (had<0) return had;
    if (had && (present(backup)!=0 || sceIoRename(current,backup)<0)) return -1;
    int ret=sceIoRename(old,current);
    if (ret<0) { if (had) sceIoRename(backup,current);return ret; }
    return 1;
}
