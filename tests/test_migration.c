#define _DEFAULT_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mock_migration.h"
#include "../src/app/migration.h"
#include "../src/common/paths.h"
static const char *fail_rename;
static int err(void) { return (int)(0x80010000u|(unsigned)errno); }
int sceIoGetstat(const char *p,SceIoStat *out) { struct stat st;if (stat(p,&st)) return err();out->st_size=st.st_size;return 0; }
int sceIoMkdir(const char *p,int mode) { return mkdir(p,mode)?err():0; }
int sceIoRename(const char *from,const char *to) {
    if (fail_rename && !strcmp(from,fail_rename)) return -55;
    if (access(to,F_OK)==0) return (int)0x80010011;
    return rename(from,to)?err():0;
}
int sceIoRemove(const char *p) { return unlink(p)?err():0; }
static void put(const char *p,const char *s) { FILE *f=fopen(p,"wb");assert(f);assert(fputs(s,f)>=0);assert(fclose(f)==0); }
static void check(const char *p,const char *s) { char text[128]={0};FILE *f=fopen(p,"rb");assert(f);assert(fread(text,1,sizeof(text)-1,f)==strlen(s));fclose(f);assert(!strcmp(text,s)); }
int main(void) {
    char root[]="/tmp/am-migration-XXXXXX";assert(mkdtemp(root));assert(chdir(root)==0);
    assert(mkdir("ux0:data",0700)==0 && mkdir("ur0:data",0700)==0);
    assert(mkdir(AM_LEGACY_DATA_DIR,0700)==0 && mkdir(AM_LEGACY_KERNEL_DIR,0700)==0);
    put(AM_LEGACY_SETTINGS_PATH,"saved settings");put(AM_LEGACY_KERNEL_DIR "/app.log","legacy app log");
    put(AM_LEGACY_DATA_DIR "/test.log","legacy report");
    assert(am_migrate_data()==1);check(AM_SETTINGS_PATH,"saved settings");
    check(AM_APP_LOG,"legacy app log");check(ACCESSORY_USER_LOG,"legacy report");check(AM_LEGACY_SETTINGS_PATH,"saved settings");
    put(AM_SETTINGS_PATH,"new current settings");put(AM_LEGACY_SETTINGS_PATH,"older settings");
    assert(am_migrate_data()==0);check(AM_SETTINGS_PATH,"new current settings");
    assert(unlink(AM_APP_LOG)==0);
    assert(am_migrate_data()==0 && access(AM_APP_LOG,F_OK)<0); /* Clear Log stays clear. */
    assert(unlink(ACCESSORY_KERNEL_DIR "/legacy-migration.done")==0);
    fail_rename=AM_APP_LOG ".migrate-new";
    assert(am_migrate_data()<0 && access(AM_APP_LOG,F_OK)<0);check(AM_LEGACY_KERNEL_DIR "/app.log","legacy app log");
    assert(access(AM_APP_LOG ".migrate-new",F_OK)<0);fail_rename=NULL;
    assert(am_migrate_data()==1);check(AM_APP_LOG,"legacy app log");assert(am_migrate_data()==0);
    assert(mkdir(AM_LEGACY_DATA_DIR "/update",0700)==0);
    put(AM_LEGACY_DATA_DIR "/update/ready","01.18");put(AM_LEGACY_DATA_DIR "/update/package-data","verified package");
    assert(mkdir(ACCESSORY_DATA_DIR "/update",0700)==0);put(ACCESSORY_DATA_DIR "/update/old-result","saved diagnostics");
    put(ACCESSORY_DATA_DIR "/update/ready","01.17");
    fail_rename=AM_LEGACY_DATA_DIR "/update";assert(am_migrate_update()<0);
    check(ACCESSORY_DATA_DIR "/update/old-result","saved diagnostics");check(AM_LEGACY_DATA_DIR "/update/package-data","verified package");
    fail_rename=NULL;assert(am_migrate_update()==1);
    check(ACCESSORY_DATA_DIR "/update/ready","01.18");check(ACCESSORY_DATA_DIR "/update/package-data","verified package");
    check(ACCESSORY_DATA_DIR "/update.pre-migration/old-result","saved diagnostics");assert(am_migrate_update()==0);
    /* A current request wins; an old leftover must not replace it. */
    assert(mkdir(AM_LEGACY_DATA_DIR "/update",0700)==0);put(AM_LEGACY_DATA_DIR "/update/ready","stale");
    assert(am_migrate_update()==0);check(ACCESSORY_DATA_DIR "/update/ready","01.18");
    puts("Migration: settings/log preservation, current precedence, failed-copy retry, legacy installer bridge, rename rollback and idempotence passed");return 0;
}
