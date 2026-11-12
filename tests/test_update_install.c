#define _DEFAULT_SOURCE
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock_update_install.h"
#include "../src/app/update_install.h"
static struct { DIR *dir;char path[512]; } dirs[32];
int sceIoDopen(const char *p) { for (int i=1;i<32;++i) if (!dirs[i].dir) { dirs[i].dir=opendir(p);snprintf(dirs[i].path,512,"%s",p);return dirs[i].dir?i:-1; }return -1; }
int sceIoDread(int d,SceIoDirent *e) { struct dirent *ent=readdir(dirs[d].dir);if (!ent) return 0;snprintf(e->d_name,256,"%s",ent->d_name);char path[1024];snprintf(path,1024,"%s/%s",dirs[d].path,ent->d_name);struct stat st;assert(lstat(path,&st)==0);e->d_stat.st_mode=st.st_mode;return 1; }
int sceIoDclose(int d) { closedir(dirs[d].dir);dirs[d].dir=NULL;return 0; }
int sceIoMkdir(const char *p,int mode) { int r=mkdir(p,mode);return r<0 && errno==EEXIST?(int)0x80010011:r; }
int sceIoRmdir(const char *p) { return rmdir(p); }
int sceIoRemove(const char *p) { return unlink(p); }
int sceIoOpen(const char *p,int flags,int mode) { return open(p,flags,mode); }
int sceIoWrite(int fd,const void *p,int n) { return (int)write(fd,p,n); }
int sceIoClose(int fd) { return close(fd); }
static int promoted,promotion_result;
static unsigned progress_events,last_phase,last_done,last_total;
static void progress(void *context,unsigned phase,unsigned done,unsigned total) {
    (void)context;assert(phase<=AM_UPDATE_PREPARE);assert(!total || done<=total);
    if (progress_events && last_phase==phase) assert(done>=last_done);
    ++progress_events;last_phase=phase;last_done=done;last_total=total;
}
int am_package_promote(const char *path) {
    assert(!strcmp(path,AM_UPDATE_HELPER));
    assert(access(AM_UPDATE_READY,F_OK)==0);
    ++promoted;
    return promotion_result;
}
int main(int argc,char **argv) {
    assert(argc==3);assert(chdir(argv[1])==0);
    if (!strcmp(argv[2],"promotion_failed")) promotion_result=-1;
    char source[1024];snprintf(source,sizeof(source),"%s/%s.vpk",argv[1],promotion_result?"good":argv[2]);
    FILE *in=fopen(source,"rb"),*out=fopen(AM_UPDATE_VPK,"wb");assert(in && out);char b[8192];size_t n;
    while ((n=fread(b,1,sizeof(b),in))) assert(fwrite(b,1,n,out)==n);
    fclose(in);fclose(out);
    FILE *receipt=fopen(AM_UPDATE_RESULT,"wb");
    assert(receipt && fputs("00000000",receipt)>=0 && fclose(receipt)==0);
    char reason[192];int result=am_update_prepare(!strcmp(argv[2],"multi_digit")?"v.1.2.10":"v.1.1.0",reason,sizeof(reason),progress,NULL);
    if (promotion_result<0) {
        assert(result<0 && promoted==1);
        assert(access(AM_UPDATE_READY,F_OK)<0);
        assert(access(AM_UPDATE_STAGE "/eboot.bin",F_OK)==0);
        assert(access(AM_UPDATE_HELPER "/eboot.bin",F_OK)==0);
    }
    else if (!strcmp(argv[2],"multi_digit")) { assert(result==0 && promoted==1);assert(am_package_helper_release(AM_UPDATE_HELPER,"1.2.10","01.30")==0);assert(am_package_helper_release(AM_UPDATE_HELPER,"1.2.9","01.30")<0); }
    else if (!strcmp(argv[2],"good")) { assert(result==0 && promoted==1);
        assert(am_package_helper_release(AM_UPDATE_HELPER,"1.1.0","01.10")==0);
        char helper[32]={0};FILE *h=fopen(AM_UPDATE_HELPER "/eboot.bin","rb");assert(h);assert(fread(helper,1,sizeof(helper)-1,h)==10);fclose(h);assert(!strcmp(helper,"new helper"));assert(progress_events>=4 && last_phase==AM_UPDATE_PREPARE && !last_total);char package[6];assert(am_package_release(AM_UPDATE_STAGE,"1.1.0",package)==0 && !strcmp(package,"01.10"));assert(am_package_release(AM_UPDATE_STAGE,"1.1.1",package)<0); }
    else {
        assert(result<0 && promoted==0);
        assert(access(AM_UPDATE_READY,F_OK)<0);
        assert(access(AM_UPDATE_STAGE,F_OK)<0);
        assert(access(AM_UPDATE_HELPER,F_OK)<0);
    }
    receipt=fopen(AM_UPDATE_RESULT,"rb");
    assert(receipt && fread(b,1,sizeof(b),receipt)==8 && !memcmp(b,"00000000",8));
    fclose(receipt);
    assert(access("escape",F_OK)<0);
    printf("Package staging: %s passed\n",argv[2]);return 0;
}
