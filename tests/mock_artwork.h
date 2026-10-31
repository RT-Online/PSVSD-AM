#ifndef AM_MOCK_ARTWORK_H
#define AM_MOCK_ARTWORK_H
#define rename am_test_rename
int am_test_rename(const char *from,const char *to);
#define SCE_SYSMODULE_LIVEAREA 0x23
int sceIoMkdir(const char *path,int mode);
int sceSysmoduleIsLoaded(int module);
int sceSysmoduleLoadModule(int module);
int sceSysmoduleUnloadModule(int module);
int sceLiveAreaReplaceAllForTitleSync(const char *path,const char *title,const void *modified);
#endif
