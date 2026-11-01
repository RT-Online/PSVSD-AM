#ifndef MOCK_MANAGER_H
#define MOCK_MANAGER_H
#include "mock_app.h"
#include <stdio.h>
#define SCE_O_APPEND O_APPEND
#define SCE_O_EXCL O_EXCL
#define SCE_SEEK_END SEEK_END
#define SCE_S_ISDIR(m) S_ISDIR(m)
#define SCE_S_ISREG(m) S_ISREG(m)
typedef struct { SceIoStat d_stat;char d_name[256]; } SceIoDirent;
int sceIoDopen(const char *);
int sceIoDread(int,SceIoDirent *);
int sceIoDclose(int);
int sceIoRmdir(const char *);
int64_t sceIoLseek(int,int64_t,int);
int sceIoSync(const char *,unsigned);
int _vshKernelSearchModuleByName(const char *,const void *);
#endif
