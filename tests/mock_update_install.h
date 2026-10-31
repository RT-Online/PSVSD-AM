#ifndef AM_MOCK_UPDATE_INSTALL_H
#define AM_MOCK_UPDATE_INSTALL_H
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#define SCE_O_WRONLY O_WRONLY
#define SCE_O_CREAT O_CREAT
#define SCE_O_EXCL O_EXCL
#define SCE_S_ISDIR S_ISDIR
typedef struct { char d_name[256];struct { unsigned st_mode; } d_stat; } SceIoDirent;
int sceIoDopen(const char *p);
int sceIoDread(int d,SceIoDirent *e);
int sceIoDclose(int d);
int sceIoMkdir(const char *p,int mode);
int sceIoRmdir(const char *p);
int sceIoRemove(const char *p);
int sceIoOpen(const char *p,int flags,int mode);
int sceIoWrite(int fd,const void *p,int n);
int sceIoClose(int fd);
int sceAppMgrLaunchAppByName2(const char *name,const char *param,void *opt);
int sceAppMgrLaunchAppByUri(int flags,const char *uri);
#endif
