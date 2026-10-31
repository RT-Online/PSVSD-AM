#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "mock_artwork.h"
#include "../src/app/artwork.h"
#undef rename
static int fail_rename;
int am_test_rename(const char *from,const char *to) {
    if (fail_rename && strstr(from,".am-new")) { fail_rename=0;return -1; }
    if (access(to,F_OK)==0) return -1; /* Model Vita rename without overwrite. */
    return rename(from,to);
}
static int loaded,loads,unloads,refreshes,refresh_result;
int sceIoMkdir(const char *p,int mode) { int r=mkdir(p,mode);return r<0 && errno==EEXIST?0:r; }
int sceSysmoduleIsLoaded(int module) { assert(module==0x23);return loaded?0:-1; }
int sceSysmoduleLoadModule(int module) { assert(module==0x23);++loads;loaded=1;return 0; }
int sceSysmoduleUnloadModule(int module) { assert(module==0x23);++unloads;loaded=0;return 0; }
int sceLiveAreaReplaceAllForTitleSync(const char *p,const char *id,const void *modified) {
    assert(!strcmp(p,"app0:sce_sys/livearea/contents"));assert(!strcmp(id,"RTPSV0001"));assert(!modified);
    ++refreshes;return refresh_result;
}
static void write_file(const char *p,const char *s) { FILE *f=fopen(p,"wb");assert(f);assert(fputs(s,f)>=0);assert(fclose(f)==0); }
static void check(const char *p,const char *s) { char b[64]={0};FILE *f=fopen(p,"rb");assert(f);size_t n=fread(b,1,sizeof(b)-1,f);assert(n==strlen(s));assert(!ferror(f));fclose(f);assert(!strcmp(b,s)); }
int main(void) {
    char root[]="/tmp/am-artwork-XXXXXX";assert(mkdtemp(root));assert(chdir(root)==0);
    const char *dirs[]={"app0:sce_sys","app0:sce_sys/livearea","app0:sce_sys/livearea/contents","ur0:appmeta","ur0:data"};
    for (unsigned i=0;i<sizeof(dirs)/sizeof(dirs[0]);++i) assert(mkdir(dirs[i],0700)==0);
    write_file("app0:sce_sys/icon0.png","icon-new");
    write_file("app0:sce_sys/livearea/contents/template.xml","template");
    write_file("app0:sce_sys/livearea/contents/bg.png","background");
    write_file("app0:sce_sys/livearea/contents/startup.png","startup");
    assert(am_artwork_refresh()==1);assert(refreshes==1 && loads==1 && unloads==1);
    check("ur0:appmeta/RTPSV0001/icon0.png","icon-new");check("ur0:appmeta/RTPSV0001/livearea/contents/startup.png","startup");
    assert(am_artwork_refresh()==0 && refreshes==1);
    write_file("app0:sce_sys/icon0.png","next-icon");refresh_result=-77;
    assert(am_artwork_refresh()==-77 && refreshes==2);
    assert(access("ur0:data/PSVSD_Accessory/artwork-pending",F_OK)==0);
    refresh_result=0;loaded=1;int before_loads=loads,before_unloads=unloads;
    assert(am_artwork_refresh()==1 && refreshes==3);assert(loads==before_loads && unloads==before_unloads);
    check("ur0:appmeta/RTPSV0001/icon0.png","next-icon");
    assert(access("ur0:data/PSVSD_Accessory/artwork-pending",F_OK)<0);
    write_file("app0:sce_sys/livearea/contents/bg.png","changed-background");
    assert(am_artwork_refresh()==1 && refreshes==4);
    write_file("app0:sce_sys/icon0.png","third-icon");fail_rename=1;
    assert(am_artwork_refresh()<0 && refreshes==4);check("ur0:appmeta/RTPSV0001/icon0.png","next-icon");
    assert(am_artwork_refresh()==1 && refreshes==5);check("ur0:appmeta/RTPSV0001/icon0.png","third-icon");
    assert(remove("app0:sce_sys/icon0.png")==0);assert(am_artwork_refresh()<0 && refreshes==5);
    check("ur0:appmeta/RTPSV0001/icon0.png","third-icon");
    write_file("app0:sce_sys/icon0.png","third-icon");
    write_file("ur0:data/PSVSD_Accessory/artwork-version","1.1.5");
    assert(am_artwork_refresh()==1 && refreshes==6);
    check("ur0:data/PSVSD_Accessory/artwork-version","1.1.6");
    assert(am_artwork_refresh()==0 && refreshes==6);
    puts("Artwork: cache replacement, unchanged skip, retry, module ownership and missing source passed");return 0;
}
