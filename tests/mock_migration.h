#ifndef AM_MOCK_MIGRATION_H
#define AM_MOCK_MIGRATION_H
#include <stdint.h>
typedef struct { uint64_t st_size; } SceIoStat;
int sceIoGetstat(const char *path,SceIoStat *st);
int sceIoMkdir(const char *path,int mode);
int sceIoRename(const char *from,const char *to);
int sceIoRemove(const char *path);
#endif
