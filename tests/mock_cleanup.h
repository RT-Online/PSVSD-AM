#ifndef AM_MOCK_CLEANUP_H
#define AM_MOCK_CLEANUP_H
#define SCE_APPMGR_ERROR_BUSY 0x80802000u
typedef int SceUID;
typedef struct { unsigned mode; } SceIoStat;
int sceIoGetstat(const char *,SceIoStat *);
int sceAppMgrGetIdByName(SceUID *,const char *);
int sceAppMgrDestroyAppByName(const char *);
int sceAppMgrDestroyOtherApp(void);
int sceKernelDelayThread(unsigned);
#endif
