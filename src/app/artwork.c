/* Repair this app's source-image cache after installs and storage changes.
 * No app deletion, SQL writes or global database rebuild. GPL-3.0-or-later. */
#include "artwork.h"
#include "../common/paths.h"
#include <stdio.h>
#include <string.h>
#ifdef PSVSD_AM_HOST_TEST
#include "mock_artwork.h"
#else
#include <psp2/io/stat.h>
#include <psp2/livearea.h>
#include <psp2/sysmodule.h>
#endif
#define CACHE "ur0:appmeta/RTPSV0001"
#define PENDING ACCESSORY_KERNEL_DIR "/artwork-pending"
static const char *files[]={"icon0.png","livearea/contents/template.xml",
    "livearea/contents/bg.png","livearea/contents/startup.png"};
static int same(const char *a,const char *b) {
    FILE *in=fopen(a,"rb"),*out=fopen(b,"rb");int equal=0;
    if (in && out) {
        unsigned char x[4096],y[4096];size_t n,m;
        do { n=fread(x,1,sizeof(x),in);m=fread(y,1,sizeof(y),out);
            if (n!=m || memcmp(x,y,n)) goto done;
        } while (n);
        equal=!ferror(in) && !ferror(out);
    }
done:
    if (in && fclose(in)) equal=0;
    if (out && fclose(out)) equal=0;
    return equal;
}
static int exists(const char *path) {
    FILE *f=fopen(path,"rb");if (!f) return 0;fclose(f);return 1;
}
static int replace(const char *source,const char *target) {
    char temp[208],backup[208];
    if (snprintf(temp,sizeof(temp),"%s.am-new",target)>=(int)sizeof(temp) ||
        snprintf(backup,sizeof(backup),"%s.am-old",target)>=(int)sizeof(backup)) return -1;
    /* Vita rename need not replace an existing destination. Keep the old file
     * until the verified new file has been renamed into place. */
    if (exists(backup)) {
        if (!exists(target)) { if (rename(backup,target)) return -1; }
        else if (remove(backup)) return -1;
    }
    FILE *in=fopen(source,"rb");if (!in) return -1;
    FILE *out=fopen(temp,"wb");int ok=out!=NULL;
    if (out) {
        unsigned char bytes[4096];size_t n;
        while ((n=fread(bytes,1,sizeof(bytes),in))) if (fwrite(bytes,1,n,out)!=n) { ok=0;break; }
        if (ferror(in)) ok=0;
        if (fclose(out)) ok=0;
    }
    if (fclose(in)) ok=0;
    if (ok) ok=same(source,temp);
    int saved=0;
    if (ok && exists(target)) { ok=rename(target,backup)==0;saved=ok; }
    if (ok && rename(temp,target)) {
        ok=0;if (saved) rename(backup,target); /* Retain backup if restore fails. */
    }
    if (!ok) remove(temp);
    else if (saved) remove(backup);
    return ok?0:-1;
}
int am_artwork_refresh(void) {
    int changed=0;
    for (unsigned i=0;i<sizeof(files)/sizeof(files[0]);++i) {
        char source[192],target[192];
        snprintf(source,sizeof(source),"app0:sce_sys/%s",files[i]);
        snprintf(target,sizeof(target),CACHE "/%s",files[i]);
        if (!same(source,target)) changed=1;
    }
    FILE *pending=fopen(PENDING,"rb");
    if (pending) fclose(pending);
    if (!changed && !pending) return 0;
    sceIoMkdir(ACCESSORY_KERNEL_DIR,0777);
    FILE *marker=fopen(PENDING,"wb");if (!marker) return -1;
    int ok=fputs("refresh\n",marker)>=0;if (fclose(marker)) ok=0;if (!ok) return -1;
    sceIoMkdir(CACHE,0777);sceIoMkdir(CACHE "/livearea",0777);sceIoMkdir(CACHE "/livearea/contents",0777);
    for (unsigned i=0;i<sizeof(files)/sizeof(files[0]);++i) {
        char source[192],target[192];
        snprintf(source,sizeof(source),"app0:sce_sys/%s",files[i]);
        snprintf(target,sizeof(target),CACHE "/%s",files[i]);
        if (!same(source,target) && replace(source,target)<0) return -1;
    }
    int owned=sceSysmoduleIsLoaded(SCE_SYSMODULE_LIVEAREA)!=0;
    int ret=owned?sceSysmoduleLoadModule(SCE_SYSMODULE_LIVEAREA):0;
    if (ret<0) return ret;
    ret=sceLiveAreaReplaceAllForTitleSync("app0:sce_sys/livearea/contents","RTPSV0001",NULL);
    if (owned) sceSysmoduleUnloadModule(SCE_SYSMODULE_LIVEAREA);
    if (ret<0) return ret;
    if (remove(PENDING)) return -1;
    return 1;
}
