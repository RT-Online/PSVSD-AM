/* Stage only a digest-verified release; never extract into the live app.
 * GPL-3.0-or-later. */
#include "update_install.h"
#include "../vendor/minizip/unzip.h"
#ifdef PSVSD_AM_HOST_TEST
#include "mock_update_install.h"
#else
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#include <psp2/appmgr.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static int clean(const char *path,unsigned depth) {
    if (depth>16) return -1;
    int dir=sceIoDopen(path);
    if (dir<0) { sceIoRemove(path);return 0; }
    SceIoDirent entry;int ret=0,n;
    for (;;) {
        memset(&entry,0,sizeof(entry));n=sceIoDread(dir,&entry);if (n<=0) { if (n<0) ret=-1;break; }
        if (!strcmp(entry.d_name,".") || !strcmp(entry.d_name,"..")) continue;
        char next[512];if (snprintf(next,sizeof(next),"%s/%s",path,entry.d_name)>=(int)sizeof(next)) { ret=-1;break; }
        if (SCE_S_ISDIR(entry.d_stat.st_mode)) { if (clean(next,depth+1)<0) { ret=-1;break; } }
        else if (sceIoRemove(next)<0) { ret=-1;break; }
    }
    sceIoDclose(dir);if (ret>=0 && sceIoRmdir(path)<0) ret=-1;return ret;
}
static int parents(char *path) {
    for (char *p=strchr(path,':')+1;*p;++p) if (*p=='/') { *p=0;int r=sceIoMkdir(path,0777);*p='/';if (r<0 && r!=(int)0x80010011) return r; }
    return 0;
}
static int copy(const char *from,const char *to) {
    char path[512];snprintf(path,sizeof(path),"%s",to);if (parents(path)<0) return -1;
    FILE *in=fopen(from,"rb");if (!in) return -1;
    FILE *out=fopen(to,"wb");if (!out) { fclose(in);return -1; }
    char bytes[8192];size_t n;int ret=0;
    while ((n=fread(bytes,1,sizeof(bytes),in))>0) if (fwrite(bytes,1,n,out)!=n) { ret=-1;break; }
    if (ferror(in)) ret=-1;
    fclose(in);if (fclose(out)) ret=-1;return ret;
}
static int safe_name(const char *name) {
    if (!*name || *name=='/' || strlen(name)>200 || strchr(name,':') || strchr(name,'\\')) return 0;
    unsigned depth=0;const char *p=name;
    while (*p) {
        const char *end=strchr(p,'/');size_t n=end?(size_t)(end-p):strlen(p);
        if (!n || (n==1 && *p=='.') || (n==2 && p[0]=='.' && p[1]=='.') || ++depth>12) return 0;
        for (size_t i=0;i<n;++i) if ((unsigned char)p[i]<32) return 0;
        if (!end) break;
        p=end+1;
    }
    return 1;
}
static int extract(AmUpdateProgressCallback progress,void *context) {
    unzFile zip=unzOpen(AM_UPDATE_VPK);if (!zip) return -1;
    unz_global_info info_total;
    if (unzGetGlobalInfo(zip,&info_total)!=UNZ_OK || !info_total.number_entry || info_total.number_entry>512) { unzClose(zip);return -1; }
    if (progress) progress(context,AM_UPDATE_EXTRACT,0,(unsigned)info_total.number_entry);
    int ret=-1,next=UNZ_OK;uint64_t total=0;unsigned entries=0;
    if (unzGoToFirstFile(zip)!=UNZ_OK) goto done;
    do {
        char name[256],path[512];unz_file_info info;
        if (++entries>512 || unzGetCurrentFileInfo(zip,&info,name,sizeof(name),NULL,0,NULL,0)!=UNZ_OK ||
            info.size_filename>=sizeof(name) || !safe_name(name) || (info.flag&1) ||
            (info.compression_method!=0 && info.compression_method!=8) ||
            ((info.external_fa>>16)&0170000)==0120000 || info.uncompressed_size>32u*1024u*1024u) goto done;
        total+=info.uncompressed_size;if (total>96u*1024u*1024u) goto done;
        if (snprintf(path,sizeof(path),"%s/%s",AM_UPDATE_STAGE,name)>=(int)sizeof(path) || parents(path)<0) goto done;
        if (name[strlen(name)-1]=='/') { sceIoMkdir(path,0777);if (progress) progress(context,AM_UPDATE_EXTRACT,entries,(unsigned)info_total.number_entry);continue; }
        /* Exclusive create rejects duplicate/overlapping archive entries. */
        int fd=sceIoOpen(path,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_EXCL,0777);if (fd<0) goto done;
        if (unzOpenCurrentFile(zip)!=UNZ_OK) { sceIoClose(fd);goto done; }
        char bytes[16384];int n,ok=1;uint32_t written=0;
        while ((n=unzReadCurrentFile(zip,bytes,sizeof(bytes)))>0) {
            if ((uint32_t)n>info.uncompressed_size-written || sceIoWrite(fd,bytes,n)!=n) { ok=0;break; }written+=(unsigned)n;
        }
        if (n<0 || written!=info.uncompressed_size) ok=0;
        if (sceIoClose(fd)<0) ok=0;
        if (unzCloseCurrentFile(zip)!=UNZ_OK) ok=0; /* Includes CRC validation. */
        if (!ok) goto done;
        if (progress) progress(context,AM_UPDATE_EXTRACT,entries,(unsigned)info_total.number_entry);
    } while ((next=unzGoToNextFile(zip))==UNZ_OK);
    ret=next==UNZ_END_OF_LIST_OF_FILE?0:-1;
done:
    unzClose(zip);return ret;
}
int am_update_prepare(const char *version,char *reason,unsigned cap,AmUpdateProgressCallback progress,void *context) {
    int discard_staging=0;
    remove(AM_UPDATE_READY);
    if (clean(AM_UPDATE_STAGE,0)<0 || clean(AM_UPDATE_HELPER,0)<0) goto bad;
    discard_staging=1;
    sceIoMkdir(AM_UPDATE_ROOT,0777);sceIoMkdir(AM_UPDATE_STAGE,0777);
    const char *v=version;if (*v=='v') { ++v;if (*v=='.') ++v; }
    char expected[6];
    if (extract(progress,context)<0 || am_package_release(AM_UPDATE_STAGE,v,expected)<0) goto bad;
    if (progress) progress(context,AM_UPDATE_PREPARE,0,0);
    if (copy(AM_UPDATE_STAGE "/main-head.bin",AM_UPDATE_STAGE "/sce_sys/package/head.bin")<0 ||
        copy(AM_UPDATE_STAGE "/installer/eboot.bin",AM_UPDATE_HELPER "/eboot.bin")<0 ||
        copy(AM_UPDATE_STAGE "/installer/param.sfo",AM_UPDATE_HELPER "/sce_sys/param.sfo")<0 ||
        copy(AM_UPDATE_STAGE "/installer/head.bin",AM_UPDATE_HELPER "/sce_sys/package/head.bin")<0 ||
        copy(AM_UPDATE_STAGE "/sce_sys/icon0.png",AM_UPDATE_HELPER "/sce_sys/icon0.png")<0) goto bad;
    if (am_package_helper_release(AM_UPDATE_HELPER,v,expected)<0) goto bad;
    FILE *f=fopen(AM_UPDATE_READY,"wb");
    if (!f) goto bad;
    int ok=fwrite(expected,1,6,f)==6;
    if (fclose(f)) ok=0;
    if (!ok) goto bad;
    /* Preserve staging for recovery after a system promotion attempt. */
    discard_staging=0;
    if (am_package_promote(AM_UPDATE_HELPER)<0) goto bad;
    return 0;
bad:
    remove(AM_UPDATE_READY);
    if (discard_staging) {
        clean(AM_UPDATE_STAGE,0);
        clean(AM_UPDATE_HELPER,0);
    }
    snprintf(reason,cap,"Package validation or installer preparation failed. Existing app retained.");
    return -1;
}
