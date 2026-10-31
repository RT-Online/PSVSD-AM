#include "settings.h"
#include <string.h>
#include <stdio.h>
const char *am_target_name(int target) { return target==AM_UX0 ? "ux0:" : "uma0:"; }
int am_settings_parse(const char *text, AmSettings *out) {
    AmSettings result={0,AM_UMA0};unsigned seen=0;
    while (*text) {
        const char *end=strchr(text,'\n');size_t n=end ? (size_t)(end-text) : strlen(text);
        while (n && (text[n-1]=='\r' || text[n-1]==' ' || text[n-1]=='\t')) --n;
        while (n && (*text==' ' || *text=='\t')) { ++text;--n; }
        unsigned field=0;
        if (!n || *text=='#') { /* comments */ }
        else if (n==9 && !memcmp(text,"version=1",9)) field=1;
        else if (n==9 && !memcmp(text,"enabled=",8) && (text[8]=='0' || text[8]=='1')) {
            field=2;result.enabled=text[8]-'0';
        } else if (n==10 && !memcmp(text,"mount=uma0",10)) { field=4;result.target=AM_UMA0; }
        else if (n==9 && !memcmp(text,"mount=ux0",9)) { field=4;result.target=AM_UX0; }
        else return -1;
        if (field && (seen&field)) return -1;
        seen|=field;if (!end) break;text=end+1;
    }
    if (seen!=7) return -1;
    *out=result;return 0;
}
int am_settings_format(const AmSettings *s,char *out,size_t cap) {
    if ((s->enabled!=0 && s->enabled!=1) || (s->target!=AM_UMA0 && s->target!=AM_UX0)) return -1;
    int n=snprintf(out,cap,"# PSVSD/AM\nversion=1\nenabled=%d\nmount=%s\n",s->enabled,s->target==AM_UX0 ? "ux0" : "uma0");
    return n>=0 && (size_t)n<cap ? n : -1;
}
