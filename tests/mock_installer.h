#ifndef AM_MOCK_INSTALLER_H
#define AM_MOCK_INSTALLER_H
#include <stddef.h>
#include <stdint.h>
typedef size_t SceSize;
typedef struct { unsigned buttons; } SceCtrlData;
typedef struct { size_t size;void *base;unsigned pitch,format,width,height; } SceDisplayFrameBuf;
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW 1
#define SCE_DISPLAY_PIXELFORMAT_A8B8G8R8 1
#define SCE_DISPLAY_SETBUF_NEXTFRAME 1
#define SCE_CTRL_CROSS 1
int sceKernelAllocMemBlock(const char *name,int type,size_t size,void *opt);
int sceKernelGetMemBlockBase(int block,void **base);
int sceDisplaySetFrameBuf(const SceDisplayFrameBuf *frame,int mode);
int sceDisplayWaitVblankStart(void);
int sceAppMgrDestroyAppByName(const char *title);
int sceKernelCreateThread(const char *name,int (*entry)(SceSize,void*),int priority,size_t stack,int attr,int cpu,void *opt);
int sceKernelStartThread(int thread,SceSize argc,void *args);
int sceKernelWaitThreadEnd(int thread,void *status,void *timeout);
int sceKernelDeleteThread(int thread);
int sceCtrlPeekBufferPositive(int port,SceCtrlData *pad,int count);
uint32_t sceKernelGetProcessTimeLow(void);
uint64_t sceKernelGetProcessTimeWide(void);
int sceKernelExitProcess(int status);
#endif
