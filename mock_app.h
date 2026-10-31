#ifndef MOCK_APP_H
#define MOCK_APP_H
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#define SCE_O_RDONLY O_RDONLY
#define SCE_O_WRONLY O_WRONLY
#define SCE_O_CREAT O_CREAT
#define SCE_O_TRUNC O_TRUNC
#define KERNEL_PID 0x10005
typedef struct { uint64_t st_size;uint32_t st_mode; } SceIoStat;
typedef struct { size_t size; char versionString[28]; uint32_t version; } SceKernelSystemSwVersion;
typedef struct { size_t size; int pid; size_t args; void *argp; int flags; } tai_module_args_t;
int sceIoOpen(const char *,int,int);
int sceIoClose(int);
int sceIoRead(int,void *,size_t);
int sceIoWrite(int,const void *,size_t);
int sceIoMkdir(const char *,int);
int sceIoGetstat(const char *,SceIoStat *);
int sceIoRemove(const char *);
int sceIoRename(const char *,const char *);
int sceIoDevctl(const char *,int,void *,int,void *,int);
uint32_t sceKernelGetProcessTimeLow(void);
int sceKernelGetSystemSwVersion(SceKernelSystemSwVersion *);
int taiLoadKernelModule(const char *,int,void *);
int taiUnloadKernelModule(int,int,void *);
int taiStartKernelModuleForUser(int,tai_module_args_t *,void *,int *);
#endif
