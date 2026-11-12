/* Bounded release JSON decoding and numeric version comparison.
 * Release metadata and optional asset selection. GPL-3.0-or-later. */
#include "updates.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { const char *p,*end;unsigned depth; } Json;
static void space(Json *j) {
    while (j->p<j->end && (*j->p==' ' || *j->p=='\n' || *j->p=='\r' || *j->p=='\t')) ++j->p;
}
static int take(Json *j,char c) {
    space(j);if (j->p==j->end || *j->p!=c) return -1;
    ++j->p;return 0;
}
static int string(Json *j,char *out,size_t cap,int *truncated) {
    if (take(j,'"')<0) return -1;
    size_t used=0;
    while (j->p<j->end) {
        unsigned char c=(unsigned char)*j->p++;
        if (c=='"') { if (cap) out[used]=0;return 0; }
        if (c<32) return -1;
        if (c=='\\') {
            if (j->p==j->end) return -1;
            c=(unsigned char)*j->p++;
            if (c=='u') {
                unsigned code=0;
                for (unsigned i=0;i<4;++i) {
                    if (j->p==j->end) return -1;
                    unsigned char h=(unsigned char)*j->p++;unsigned n;
                    if (h>='0' && h<='9') n=h-'0';
                    else if (h>='a' && h<='f') n=h-'a'+10;
                    else if (h>='A' && h<='F') n=h-'A'+10;
                    else return -1;
                    code=code*16+n;
                }
                c=code>=32 && code<=126?(unsigned char)code:'?';
            } else if (c=='n') c='\n';
            else if (c=='r' || c=='t' || c=='b' || c=='f') c=' ';
            else if (c!='"' && c!='\\' && c!='/') return -1;
        }
        if (c>=128) c='?'; /* Current renderer's glyph range is ASCII. */
        if (cap && used+1<cap) out[used++]=(char)c;
        else if (cap && truncated) *truncated=1;
    }
    return -1;
}
static int literal(Json *j,const char *s) {
    space(j);size_t n=strlen(s);
    if ((size_t)(j->end-j->p)<n || memcmp(j->p,s,n)) return -1;
    j->p+=n;return 0;
}
static int value(Json *j) {
    space(j);if (j->p==j->end || j->depth>=16) return -1;
    if (*j->p=='"') return string(j,NULL,0,NULL);
    if (*j->p=='{' || *j->p=='[') {
        int object=*j->p=='{';char close=object?'}':']';++j->p;++j->depth;
        space(j);
        if (j->p<j->end && *j->p==close) { ++j->p;--j->depth;return 0; }
        for (;;) {
            if (object && (string(j,NULL,0,NULL)<0 || take(j,':')<0)) return -1;
            if (value(j)<0) return -1;
            space(j);if (j->p==j->end) return -1;
            if (*j->p==close) { ++j->p;--j->depth;return 0; }
            if (take(j,',')<0) return -1;
        }
    }
    if (*j->p=='t') return literal(j,"true");
    if (*j->p=='f') return literal(j,"false");
    if (*j->p=='n') return literal(j,"null");
    if (*j->p=='-') ++j->p;
    if (j->p==j->end) return -1;
    if (*j->p=='0') ++j->p;
    else {
        if (*j->p<'1' || *j->p>'9') return -1;
        while (j->p<j->end && *j->p>='0' && *j->p<='9') ++j->p;
    }
    if (j->p<j->end && *j->p=='.') {
        ++j->p;const char *start=j->p;
        while (j->p<j->end && *j->p>='0' && *j->p<='9') ++j->p;
        if (start==j->p) return -1;
    }
    if (j->p<j->end && (*j->p=='e' || *j->p=='E')) {
        ++j->p;if (j->p<j->end && (*j->p=='+' || *j->p=='-')) ++j->p;
        const char *start=j->p;
        while (j->p<j->end && *j->p>='0' && *j->p<='9') ++j->p;
        if (start==j->p) return -1;
    }
    return 0;
}
static int version(const char *s,unsigned out[3]) {
    if (*s=='v') {
        ++s;
        if (*s=='.') ++s; /* Also accept tags such as v.1.0.3. */
    }
    for (unsigned i=0;i<3;++i) {
        const char *start=s;unsigned n=0;
        if (*s<'0' || *s>'9') return -1;
        while (*s>='0' && *s<='9') {
            unsigned digit=(unsigned)(*s++-'0');
            if (n>(UINT32_MAX-digit)/10) return -1;
            n=n*10+digit;
        }
        if (s-start>1 && *start=='0') return -1;
        out[i]=n;
        if (i<2 && *s++!='.') return -1;
    }
    return *s?-1:0;
}

static int set_version(AmUpdate *out,const char *current) {
    unsigned latest[3],installed[3];
    if (version(out->version,latest)<0 || version(current,installed)<0) return -1;
    out->state=UPDATE_CURRENT;
    for (unsigned i=0;i<3;++i) if (latest[i]!=installed[i]) {
        out->state=latest[i]>installed[i]?UPDATE_AVAILABLE:UPDATE_AHEAD;
        break;
    }
    snprintf(out->message,sizeof(out->message),"%s",out->state==UPDATE_AVAILABLE?"A newer release is available.":
        out->state==UPDATE_AHEAD?"Installed version is newer than the public release.":"You have the latest public release.");
    return 0;
}

static int assets(Json *j,AmUpdate *out) {
    if (take(j,'[')<0) return -1;
    space(j);if (j->p<j->end && *j->p==']') { ++j->p;return 0; }
    for (unsigned count=0;count<128;++count) {
        char name[128]={0},url[512]={0},digest[80]={0};unsigned seen=0;
        if (take(j,'{')<0) return -1;
        for (;;) {
            char key[64];int cut=0;space(j);
            if (j->p<j->end && *j->p=='}') { ++j->p;break; }
            if (string(j,key,sizeof(key),&cut)<0 || take(j,':')<0) return -1;
            unsigned f=!cut && !strcmp(key,"name")?1:!cut && !strcmp(key,"browser_download_url")?2:!cut && !strcmp(key,"digest")?4:0;
            if (f && (seen&f)) return -1;
            seen|=f;int truncated=0;
            if (f) {
                space(j);
                if (f==4 && j->p<j->end && *j->p=='n') { if (literal(j,"null")<0) return -1; }
                else if (string(j,f==1?name:f==2?url:digest,f==1?sizeof(name):f==2?sizeof(url):sizeof(digest),&truncated)<0 || truncated) return -1;
            } else if (value(j)<0) return -1;
            space(j);if (j->p<j->end && *j->p=='}') { ++j->p;break; }
            if (take(j,',')<0) return -1;
            space(j);if (j->p<j->end && *j->p=='}') return -1;
        }
        size_t len=strlen(name);
        if (len>13 && !strncmp(name,"PSVSD_AM_",9) && !strcmp(name+len-4,".vpk")) {
            const char *prefix=AM_RELEASE_DOWNLOAD_PREFIX;
            int valid=!strncmp(url,prefix,strlen(prefix)) && !strncmp(digest,"sha256:",7) && strlen(digest)==71;
            for (unsigned i=7;valid && i<71;++i) valid=(digest[i]>='0' && digest[i]<='9') || (digest[i]>='a' && digest[i]<='f');
            if (valid) {
                if (out->asset_url[0]) return -1; /* Ambiguous packages are not installable. */
                snprintf(out->asset_url,sizeof(out->asset_url),"%s",url);
                memcpy(out->sha256,digest+7,65);
            }
        }
        space(j);if (j->p<j->end && *j->p==']') { ++j->p;return 0; }
        if (take(j,',')<0) return -1;
    }
    return -1;
}

int am_update_parse(const char *json,size_t size,const char *current,AmUpdate *out) {
    if (!json || !current || !out || size>AM_RELEASE_MAX_BYTES) return -1;
    AmUpdate result={0};Json j={json,json+size,0};unsigned seen=0;
    if (take(&j,'{')<0) return -1;
    for (;;) {
        char key[64];int cut=0;space(&j);
        if (j.p<j.end && *j.p=='}') { ++j.p;break; }
        if (string(&j,key,sizeof(key),&cut)<0 || take(&j,':')<0) return -1;
        unsigned field=!cut && !strcmp(key,"tag_name")?1:
            !cut && !strcmp(key,"body")?4:
            !cut && !strcmp(key,"draft")?8:!cut && !strcmp(key,"prerelease")?16:!cut && !strcmp(key,"assets")?32:0;
        if (field && (seen&field)) return -1;
        seen|=field;
        if (field==1) {
            int truncated=0;
            if (string(&j,result.version,sizeof(result.version),&truncated)<0 || truncated) return -1;
        } else if (field==4) {
            space(&j);
            if (j.p<j.end && *j.p=='n') { if (literal(&j,"null")<0) return -1; }
            else if (string(&j,result.notes,sizeof(result.notes),NULL)<0) return -1;
        } else if (field==32) {
            if (assets(&j,&result)<0) return -1;
        } else if (field==8 || field==16) {
            if (literal(&j,"false")<0) return -1; /* Stable published releases only. */
        } else if (value(&j)<0) return -1;
        space(&j);
        if (j.p<j.end && *j.p=='}') { ++j.p;break; }
        if (take(&j,',')<0) return -1;
        space(&j);if (j.p<j.end && *j.p=='}') return -1;
    }
    space(&j);if (j.p!=j.end || (seen&25)!=25) return -1;
    if (set_version(&result,current)<0) return -1;
    if (!result.notes[0]) snprintf(result.notes,sizeof(result.notes),"No release notes published.");
    *out=result;return 0;
}
int am_update_installable(const AmUpdate *release) {
    return release->state==UPDATE_AVAILABLE &&
        !strncmp(release->asset_url,AM_RELEASE_DOWNLOAD_PREFIX,sizeof(AM_RELEASE_DOWNLOAD_PREFIX)-1) &&
        !release->sha256[64] && strspn(release->sha256,"0123456789abcdef")==64;
}
void am_update_check_with_transport(AmUpdate *out,const char *current,const AmUpdateTransport *transport) {
    memset(out,0,sizeof(*out));
    if (transport->wifi(transport->ctx)<=0) {
        out->state=UPDATE_OFFLINE;
        return; /* Offline is a silent skip, not a request to connect. */
    }
    char *body=malloc(AM_RELEASE_MAX_BYTES+1);size_t used=0;int status=0;
    const char *stage=body?"HTTPS request":"Response buffer allocation";
    int ret=body?transport->get(transport->ctx,body,AM_RELEASE_MAX_BYTES,&used,&status):-1;
    if (ret>=0 && status==200 && used<=AM_RELEASE_MAX_BYTES) {
        stage="Release JSON / version";ret=am_update_parse(body,used,current,out);
        if (ret<0) snprintf(out->reason,sizeof(out->reason),"Release JSON or version tag is unsupported. Expected 1.2.3, v1.2.3 or v.1.2.3.");
    }
    else if (ret>=0) { stage=used>AM_RELEASE_MAX_BYTES?"Response size limit":"HTTP status";ret=-1; }
    free(body);out->http_status=status;out->received=used;
    if (ret<0) {
        memset(out->version,0,sizeof(out->version));out->notes[0]=0;
        out->state=UPDATE_ERROR;out->error=ret;
        snprintf(out->stage,sizeof(out->stage),"%s",stage);
        snprintf(out->message,sizeof(out->message),"%s",status==403 || status==429?"GitHub request blocked or rate-limited. Try later.":
            status==404?"No public release found.":"Could not check GitHub. See Log.");
    }
}

void am_update_diagnostic(const AmUpdate *result,char *text,size_t cap) {
    snprintf(text,cap,"Step: %s\nError: %08X  HTTP: %d\n%s\nTLS verify: %08X  Bytes: %u",
        result->stage[0]?result->stage:"Not reported",(unsigned)result->error,result->http_status,
        result->reason,(unsigned)result->tls_verify,(unsigned)result->received);
}

