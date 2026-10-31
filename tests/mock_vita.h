#ifndef MOCK_VITA_H
#define MOCK_VITA_H
#include <stddef.h>
#include <stdint.h>
typedef int SceUID;
typedef unsigned SceSize;
#define KERNEL_PID 0x10005
#define TAI_ANY_LIBRARY 0xFFFFFFFF
#define SCE_O_RDONLY 1
#define SCE_O_WRONLY 2
#define SCE_O_CREAT 0x200
#define SCE_O_TRUNC 0x400
#define SCE_O_APPEND 0x100
#define SCE_EVENT_WAITOR 1
#define SCE_EVENT_WAITCLEAR_PAT 4
#define SCE_KERNEL_START_SUCCESS 0
#define SCE_KERNEL_START_NO_RESIDENT 1
#define SCE_KERNEL_STOP_CANCEL 1
#define SCE_KERNEL_STOP_SUCCESS 0
typedef struct {
    size_t size;
    SceUID modid;
    uint32_t module_nid;
} tai_module_info_t;
int ksceIoOpen(const char *, int, int);
int ksceIoClose(int);
int ksceIoRemove(const char *);
int ksceIoDopen(const char *);
int ksceIoDclose(int);
int ksceIoRead(int, void *, SceSize);
int ksceIoMkdir(const char *, int);
int ksceIoWrite(int, const void *, size_t);
int ksceIoMount(int, const char *, int, int, int, int);
int ksceIoUmount(int, int, int, int);
int taiGetModuleInfoForKernel(SceUID, const char *, tai_module_info_t *);
int taiInjectDataForKernel(SceUID, SceUID, int, size_t, const void *, size_t);
int taiInjectReleaseForKernel(SceUID);
int ksceKernelLoadModule(const char *, int, void *);
int ksceKernelStartModule(SceUID, SceSize, void *, int, void *, void *);
int ksceKernelUnloadModule(SceUID, int, void *);
int ksceUsbServAccessoryActivate(void);
int ksceKernelDelayThread(unsigned);
int ksceKernelCreateEventFlag(const char *, int, unsigned, void *);
int ksceKernelDeleteEventFlag(int);
int ksceKernelSetEventFlag(int, unsigned);
int ksceKernelWaitEventFlag(int, unsigned, int, unsigned *, unsigned *);
int ksceKernelCreateThread(const char *, int (*)(SceSize, void *), int, int, int, int, void *);
int ksceKernelStartThread(int, SceSize, void *);
int ksceKernelDeleteThread(int);
int ksceKernelExitDeleteThread(int);
int ksceKernelRegisterSysEventHandler(const char *, int (*)(int,int,void *,void *), void *);
int ksceKernelUnregisterSysEventHandler(int);
#endif
