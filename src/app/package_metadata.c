/* Bounded SFO validation before promotion. GPL-3.0-or-later. */
#include "update_install.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned u16(const unsigned char *p) { return p[0]|p[1]<<8; }
static uint32_t u32(const unsigned char *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static int validate(const char *path,const char *expected,const char *title,const char *content) {
    char name[256];if (snprintf(name,sizeof(name),"%s/sce_sys/param.sfo",path)>=(int)sizeof(name)) return -1;
    FILE *f=fopen(name,"rb");if (!f) return -1;
    unsigned char b[16385];size_t size=fread(b,1,sizeof(b),f);int err=ferror(f);fclose(f);
    if (err || size<20 || size>16384 || u32(b)!=0x46535000) return -1;
    uint32_t keys=u32(b+8),values=u32(b+12),count=u32(b+16);
    if (count>128 || 20+count*16>size || keys<20+count*16 || keys>values || values>=size) return -1;
    unsigned found=0;
    for (unsigned i=0;i<count;++i) {
        const unsigned char *e=b+20+16*i;uint32_t ko=u16(e),vo=u32(e+12),len=u32(e+4);
        if (ko>=values-keys || vo>=size-values || !len || len>size-values-vo || !memchr(b+keys+ko,0,values-keys-ko)) return -1;
        const char *k=(char *)b+keys+ko,*v=(char *)b+values+vo;
        if (!strcmp(k,"TITLE_ID") || !strcmp(k,"APP_VER") || !strcmp(k,"CONTENT_ID")) {
            if (!memchr(v,0,len)) return -1;
            unsigned bit=!strcmp(k,"TITLE_ID")?1:!strcmp(k,"APP_VER")?2:4;
            if (found&bit) return -1;
            if ((bit==1 && strcmp(v,title)) || (bit==2 && strcmp(v,expected)) ||
                (bit==4 && strcmp(v,content))) return -1;
            found|=bit;
        }
    }
    return found==7?0:-1;
}

int am_package_validate(const char *path,const char *expected) {
    return validate(path,expected,"RTPSV0001","HB0001-RTPSV0001_00-0000000000000000");
}
int am_package_validate_helper(const char *path,const char *expected) {
    return validate(path,expected,"RTPSVUPD1","HB0001-RTPSVUPD1_00-0000000000000000");
}
