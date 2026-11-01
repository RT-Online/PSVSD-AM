#ifndef AM_MOCK_CLEANUP_H
#define AM_MOCK_CLEANUP_H
typedef struct { unsigned mode; } SceIoStat;
int sceIoGetstat(const char *,SceIoStat *);
int sceAppMgrDestroyAppByName(const char *);
#endif
