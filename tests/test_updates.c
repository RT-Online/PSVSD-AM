#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/app/updates.h"
static int cases;
static const char *release="{\"tag_name\":\"v1.0.2\",\"draft\":false,\"prerelease\":false,\"body\":\"Notes\\n- Fixed copy\",\"assets\":[{\"tag_name\":\"v9.0.0\"}]}";
static struct { int connected,calls,status,fail,too_large;const char *json; } mock;
static int wifi(void *p) { (void)p;return mock.connected; }
static int get(void *p,char *body,size_t cap,size_t *used,int *status) {
    (void)p;++mock.calls;*status=mock.status;
    if (mock.fail) return -55;
    if (mock.too_large) { *used=cap+1;return 0; }
    size_t n=strlen(mock.json);assert(n<cap);memcpy(body,mock.json,n);*used=n;return 0;
}
static void parse(const char *tag,const char *installed,int state) {
    char json[512];AmUpdate out;
    snprintf(json,sizeof(json),"{\"tag_name\":\"%s\",\"draft\":false,\"prerelease\":false}",tag);
    assert(am_update_parse(json,strlen(json),installed,&out)==0 && out.state==state);++cases;
}
int main(int argc,char **argv) {
    parse("v1.0.2","1.0.2",UPDATE_CURRENT);
    parse("v1.1.0","1.1.0",UPDATE_CURRENT);
    parse("v1.1.1","1.1.1",UPDATE_CURRENT);
    parse("v1.1.2","1.1.2",UPDATE_CURRENT);
    parse("v1.1.3","1.1.3",UPDATE_CURRENT);
    parse("v1.1.3","1.1.2",UPDATE_AVAILABLE);
    parse("v1.1.2","1.1.1",UPDATE_AVAILABLE);
    parse("v1.1.1","1.1.0",UPDATE_AVAILABLE);
    parse("v1.1.0","1.0.3",UPDATE_AVAILABLE);
    parse("v.1.0.3","1.0.3",UPDATE_CURRENT);
    parse("v.1.0.4","1.0.3",UPDATE_AVAILABLE);
    parse("v1.0.10","1.0.9",UPDATE_AVAILABLE);
    parse("v2.0.0","1.99.99",UPDATE_AVAILABLE);
    parse("v1.0.2","1.0.3",UPDATE_AHEAD);
    parse("1.10.0","1.2.99",UPDATE_AVAILABLE);
    AmUpdate out;
    assert(am_update_parse(release,strlen(release),"1.0.1",&out)==0 && out.state==UPDATE_AVAILABLE);
    assert(!strcmp(out.notes,"Notes\n- Fixed copy"));++cases;
    const char *invalid[]={"", "{}", "[]", "{\"tag_name\":\"v1.0.2\"}",
      "{\"tag_name\":\"v1.0.2\",\"draft\":true,\"prerelease\":false}",
      "{\"tag_name\":\"v1.0.2\",\"draft\":false,\"prerelease\":true}",
      "{\"tag_name\":\"v1.0.2\",\"draft\":false,\"prerelease\":false,}",
      "{\"tag_name\":\"v1.0.2\",\"tag_name\":\"v2.0.0\",\"draft\":false,\"prerelease\":false}",
      "{\"tag_name\":\"v1.0.2-beta\",\"draft\":false,\"prerelease\":false}",
      "{\"tag_name\":\"v1.0.2\",\"draft\":false,\"prerelease\":false}garbage",
      "{\"tag_name\":\"v1.0.2\",\"draft\":false,\"prerelease\":false,\"body\":\"bad\\q\"}",
      "{\"tag_name\":\"v1.0.2\",\"draft\":false,\"prerelease\":false,\"body\":\"unterminated}",
      "{\"tag_name\":\"v4294967296.0.0\",\"draft\":false,\"prerelease\":false}",
      "{\"tag_name\":\"v01.0.2\",\"draft\":false,\"prerelease\":false}",
      "{\"tag_name\":\"v1.0.2\",\"draft\":false,\"prerelease\":false,\"unknown\":[1,]}",
      "{\"tag_name\":\"v1.0.2\",\"draft\":false,\"prerelease\":false,\"unknown\":01}"};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        assert(am_update_parse(invalid[i],strlen(invalid[i]),"1.0.2",&out)<0);++cases;
    }
    for (size_t n=0;n<strlen(release);++n) assert(am_update_parse(release,n,"1.0.2",&out)<0);
    ++cases;
    AmUpdateTransport transport={NULL,wifi,get};
    memset(&mock,0,sizeof(mock));mock.status=200;mock.json=release;
    am_update_check_with_transport(&out,"1.0.3",&transport);
    assert(out.state==UPDATE_OFFLINE && !mock.calls && !out.error && !out.version[0] && !out.message[0]);++cases;
    mock.connected=-1;am_update_check_with_transport(&out,"1.0.3",&transport);
    assert(out.state==UPDATE_OFFLINE && !mock.calls && !out.message[0]);++cases;
    mock.connected=1;am_update_check_with_transport(&out,"1.0.3",&transport);
    assert(out.state==UPDATE_AHEAD && mock.calls==1);++cases;
    mock.fail=1;am_update_check_with_transport(&out,"1.0.3",&transport);
    assert(out.state==UPDATE_ERROR && out.error==-55 && !out.version[0]);++cases;
    mock.fail=0;mock.status=403;am_update_check_with_transport(&out,"1.0.3",&transport);
    assert(out.state==UPDATE_ERROR && strstr(out.message,"limit"));++cases;
    mock.status=404;am_update_check_with_transport(&out,"1.0.3",&transport);
    assert(out.state==UPDATE_ERROR && strstr(out.message,"No public"));++cases;
    mock.status=200;mock.too_large=1;am_update_check_with_transport(&out,"1.0.3",&transport);
    assert(out.state==UPDATE_ERROR);++cases;
    mock.too_large=0;mock.json="not json";am_update_check_with_transport(&out,"1.0.3",&transport);
    assert(out.state==UPDATE_ERROR);++cases;
    mock.json=release;mock.fail=1;mock.status=0;
    am_update_check_with_transport(&out,"1.0.3",&transport);
    char diagnostic[320];am_update_diagnostic(&out,diagnostic,sizeof(diagnostic));
    assert(strstr(diagnostic,"HTTPS request") && strstr(diagnostic,"FFFFFFC9") && strstr(diagnostic,"HTTP: 0"));++cases;
    out.tls_verify=0x20;strcpy(out.reason,"Certificate verification failed.");
    am_update_diagnostic(&out,diagnostic,sizeof(diagnostic));
    assert(strstr(diagnostic,"Certificate verification failed") && strstr(diagnostic,"TLS verify: 00000020"));++cases;
    mock.fail=0;mock.status=200;mock.json="not JSON";
    am_update_check_with_transport(&out,"1.0.3",&transport);
    assert(out.state==UPDATE_ERROR && !strcmp(out.stage,"Release JSON / version"));++cases;
    char large[6000];memset(large,'A',sizeof(large));
    int n=snprintf(large,sizeof(large),"{\"tag_name\":\"v1.0.2\",\"draft\":false,\"prerelease\":false,\"body\":\"");
    memset(large+n,'B',5000);strcpy(large+n+5000,"\"}");
    assert(am_update_parse(large,strlen(large),"1.0.2",&out)==0 && strlen(out.notes)==sizeof(out.notes)-1);++cases;
    const char *asset="{\"tag_name\":\"v1.2.0\",\"draft\":false,\"prerelease\":false,\"assets\":[{\"name\":\"PSVSD_AM_1.2.0.vpk\",\"browser_download_url\":\"https://github.com/RT-Online/PSVSD-AM/releases/download/v1.2.0/PSVSD_AM_1.2.0.vpk\",\"digest\":\"sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}]}";
    assert(am_update_parse(asset,strlen(asset),"1.1.0",&out)==0 && out.state==UPDATE_AVAILABLE && out.asset_url[0] && strlen(out.sha256)==64);++cases;
    char unsafe[1024];snprintf(unsafe,sizeof(unsafe),"%s",asset);
    char *owner=strstr(unsafe,"RT-Online");assert(owner);memcpy(owner,"XX-Online",9);
    assert(am_update_parse(unsafe,strlen(unsafe),"1.1.0",&out)==0 && !out.asset_url[0]);++cases;
    snprintf(unsafe,sizeof(unsafe),"%s",asset);char *sha=strstr(unsafe,"sha256:");assert(sha);sha[7]='z';
    assert(am_update_parse(unsafe,strlen(unsafe),"1.1.0",&out)==0 && !out.asset_url[0]);++cases;
    parse("v1.1.4","1.1.4",UPDATE_CURRENT);parse("v1.1.4","1.1.3",UPDATE_AVAILABLE);
    if (argc>1) {
        FILE *file=fopen(argv[1],"rb");assert(file);char *json=malloc(AM_RELEASE_MAX_BYTES+1);assert(json);
        size_t used=fread(json,1,AM_RELEASE_MAX_BYTES,file);assert(!ferror(file));fclose(file);
        assert(am_update_parse(json,used,"1.0.3",&out)==0);printf("Live release fixture: %s\n",out.version);free(json);++cases;
    }
    printf("updates: %d parser/version/offline/error cases passed\n",cases);return 0;
}
