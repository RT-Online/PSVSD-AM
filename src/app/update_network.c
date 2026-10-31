/* Vita Wi-Fi gate and app-bundled libcurl/Mbed TLS transport. GPL-3.0-or-later. */
#include "updates.h"
#include "../common/paths.h"
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <curl/curl.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    void *net_memory;
    int net_owned,ctl_owned;
    unsigned modules;
    const char *stage;
    int curl_error,failed;
    long verify_result;
    char reason[CURL_ERROR_SIZE];
    char *body;
    size_t cap,used;
    int oversized;
} Network;
static const SceSysmoduleModuleId modules[]={SCE_SYSMODULE_NET};
static int load_module(Network *n,unsigned i) {
    if (sceSysmoduleIsLoaded(modules[i])==0) return 0;
    int ret=sceSysmoduleLoadModule(modules[i]);
    if (ret>=0) n->modules|=1u<<i;
    return ret;
}
static int wifi(void *context) {
    Network *n=context;int state=0;
    if (load_module(n,0)<0) return 0;
    if (sceNetShowNetstat()<0) {
        n->net_memory=malloc(256*1024);if (!n->net_memory) return 0;
        SceNetInitParam param={n->net_memory,256*1024,0};
        if (sceNetInit(&param)<0) return 0;
        n->net_owned=1;
    }
    if (sceNetCtlInetGetState(&state)<0) {
        if (sceNetCtlInit()>=0) n->ctl_owned=1;
        if (sceNetCtlInetGetState(&state)<0) return 0;
    }
    if (state!=SCE_NETCTL_STATE_CONNECTED) return 0;
    SceNetCtlInfo info;memset(&info,0,sizeof(info));
    /* Requiring an SSID excludes a mobile-only connection. Nothing attempts
     * to connect, prompt, or fetch HTTP data while Wi-Fi is unavailable. */
    if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_SSID,&info)<0 || !info.ssid[0]) return 0;
    return 1;
}
/* Enforce the response limit before copying any received bytes. */
static size_t receive(void *data,size_t size,size_t count,void *context) {
    Network *n=context;
    if (size && count>SIZE_MAX/size) return 0;
    size_t bytes=size*count;
    if (bytes>n->cap-n->used) { n->oversized=1;return 0; }
    memcpy(n->body+n->used,data,bytes);n->used+=bytes;
    return bytes;
}
static int get(void *context,char *body,size_t cap,size_t *used,int *status) {
    Network *n=context;CURL *handle=NULL;struct curl_slist *headers=NULL;
    CURLcode code;int initialized=0,ret=-1;long http_status=0;
    n->stage="Wi-Fi recheck";n->failed=1;
    if (!wifi(n)) return -1;
    n->stage="Initialize bundled TLS";
    code=curl_global_init(CURL_GLOBAL_DEFAULT);if (code!=CURLE_OK) goto done;
    initialized=1;handle=curl_easy_init();
    if (!handle) { code=CURLE_OUT_OF_MEMORY;goto done; }
    const char *values[]={"Accept: application/vnd.github+json","X-GitHub-Api-Version: 2026-03-10","Accept-Encoding: identity"};
    for (unsigned i=0;i<3;++i) {
        struct curl_slist *next=curl_slist_append(headers,values[i]);
        if (!next) { code=CURLE_OUT_OF_MEMORY;goto done; }
        headers=next;
    }
    n->body=body;n->cap=cap;n->used=0;
#define OPTION(label,option,value) do { n->stage=label;code=curl_easy_setopt(handle,option,value);if (code!=CURLE_OK) goto done; } while (0)
    OPTION("Set GitHub endpoint",CURLOPT_URL,AM_RELEASE_API);
    OPTION("Restrict protocol",CURLOPT_PROTOCOLS_STR,"https");
    OPTION("Disable redirects",CURLOPT_FOLLOWLOCATION,0L);
    OPTION("Set User-Agent",CURLOPT_USERAGENT,"PSVSD-AM/" ACCESSORY_VERSION);
    OPTION("Set request headers",CURLOPT_HTTPHEADER,headers);
    OPTION("Set request timeout",CURLOPT_TIMEOUT,20L);
    OPTION("Set connect timeout",CURLOPT_CONNECTTIMEOUT,8L);
    OPTION("Disable signals",CURLOPT_NOSIGNAL,1L);
    OPTION("Set HTTP version",CURLOPT_HTTP_VERSION,(long)CURL_HTTP_VERSION_1_1);
    OPTION("Require TLS 1.2 or newer",CURLOPT_SSLVERSION,(long)CURL_SSLVERSION_TLSv1_2);
    OPTION("Set bundled trust roots",CURLOPT_CAINFO,"app0:update-ca-bundle.pem");
    OPTION("Verify server certificate",CURLOPT_SSL_VERIFYPEER,1L);
    OPTION("Verify server hostname",CURLOPT_SSL_VERIFYHOST,2L);
    OPTION("Set error buffer",CURLOPT_ERRORBUFFER,n->reason);
    OPTION("Set response callback",CURLOPT_WRITEFUNCTION,receive);
    OPTION("Set response context",CURLOPT_WRITEDATA,n);
#undef OPTION
    n->stage="Bundled TLS HTTPS request";
    code=curl_easy_perform(handle);
    curl_easy_getinfo(handle,CURLINFO_RESPONSE_CODE,&http_status);
    curl_easy_getinfo(handle,CURLINFO_SSL_VERIFYRESULT,&n->verify_result);
    *status=(int)http_status;*used=n->used;
    if (n->oversized) n->stage="Response size limit";
    if (code==CURLE_OK) { body[n->used]=0;ret=0; }
done:
    n->curl_error=(int)code;n->failed=ret<0;
    if (code!=CURLE_OK && !n->reason[0]) snprintf(n->reason,sizeof(n->reason),"%s",curl_easy_strerror(code));
    if (handle) curl_easy_cleanup(handle);
    curl_slist_free_all(headers);
    if (initialized) curl_global_cleanup();
    return ret<0?(code==CURLE_OK?-1:-(int)code):0;
}
void am_update_check(AmUpdate *out) {
    Network network={0};AmUpdateTransport transport={&network,wifi,get};
    am_update_check_with_transport(out,ACCESSORY_VERSION,&transport);
    if (out->state==UPDATE_ERROR && network.failed) {
        if (network.stage) snprintf(out->stage,sizeof(out->stage),"%s",network.stage);
        out->error=network.curl_error?network.curl_error:out->error;
        out->ssl_error=(int)network.verify_result;out->ssl_flags=(unsigned)network.verify_result;
        snprintf(out->reason,sizeof(out->reason),"%.191s",network.reason);
    }
    if (network.ctl_owned) sceNetCtlTerm();
    if (network.net_owned) sceNetTerm();
    free(network.net_memory);
    if (network.modules&1u) sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
}

/* Release downloads are explicit, bounded and digest-checked before staging. */
#include <mbedtls/sha256.h>
typedef struct { FILE *file;size_t used; } Download;
static size_t download_bytes(void *data,size_t size,size_t count,void *context) {
    Download *d=context;
    if (size && count>SIZE_MAX/size) return 0;
    size_t bytes=size*count;
    if (bytes>64u*1024u*1024u-d->used) return 0;
    size_t n=fwrite(data,1,bytes,d->file);d->used+=n;return n;
}
int am_update_download(const AmUpdate *release,const char *path,AmUpdate *out) {
    Network n={0};Download d={0};CURL *h=NULL;int initialized=0,ret=-1;CURLcode code=CURLE_OK;long status=0;
    memset(out,0,sizeof(*out));out->state=UPDATE_ERROR;
    snprintf(out->stage,sizeof(out->stage),"Download release VPK");
    if (strncmp(release->asset_url,"https://github.com/RT-Online/PSVSD-AM/releases/download/",56) || strlen(release->sha256)!=64 || !wifi(&n)) { snprintf(out->reason,sizeof(out->reason),"Wi-Fi or verified release asset unavailable.");goto done; }
    d.file=fopen(path,"wb");if (!d.file) { snprintf(out->reason,sizeof(out->reason),"Cannot create update file.");goto done; }
    code=curl_global_init(CURL_GLOBAL_DEFAULT);if (code!=CURLE_OK) goto done;
    initialized=1;h=curl_easy_init();if (!h) { code=CURLE_OUT_OF_MEMORY;goto done; }
#define OPT(option,value) do { code=curl_easy_setopt(h,option,value);if (code!=CURLE_OK) goto done; } while(0)
    OPT(CURLOPT_URL,release->asset_url);OPT(CURLOPT_PROTOCOLS_STR,"https");OPT(CURLOPT_REDIR_PROTOCOLS_STR,"https");
    OPT(CURLOPT_FOLLOWLOCATION,1L);OPT(CURLOPT_MAXREDIRS,4L);OPT(CURLOPT_FAILONERROR,1L);
    OPT(CURLOPT_USERAGENT,"PSVSD-AM/" ACCESSORY_VERSION);OPT(CURLOPT_CONNECTTIMEOUT,8L);OPT(CURLOPT_TIMEOUT,180L);
    OPT(CURLOPT_NOSIGNAL,1L);OPT(CURLOPT_SSLVERSION,(long)CURL_SSLVERSION_TLSv1_2);
    OPT(CURLOPT_CAINFO,"app0:update-ca-bundle.pem");OPT(CURLOPT_SSL_VERIFYPEER,1L);OPT(CURLOPT_SSL_VERIFYHOST,2L);
    OPT(CURLOPT_ERRORBUFFER,n.reason);OPT(CURLOPT_WRITEFUNCTION,download_bytes);OPT(CURLOPT_WRITEDATA,&d);
#undef OPT
    code=curl_easy_perform(h);curl_easy_getinfo(h,CURLINFO_RESPONSE_CODE,&status);
    if (fclose(d.file)!=0) { d.file=NULL;snprintf(out->reason,sizeof(out->reason),"Could not finish writing update.");goto done; }d.file=NULL;
    if (code!=CURLE_OK || status!=200 || !d.used) goto done;
    snprintf(out->stage,sizeof(out->stage),"Verify release SHA-256");
    FILE *file=fopen(path,"rb");if (!file) goto done;
    mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);int hash=mbedtls_sha256_starts(&sha,0);
    unsigned char buffer[16384],digest[32]={0};size_t bytes;
    while (hash==0 && (bytes=fread(buffer,1,sizeof(buffer),file))>0) hash=mbedtls_sha256_update(&sha,buffer,bytes);
    if (ferror(file)) hash=-1;
    fclose(file);if (hash==0) hash=mbedtls_sha256_finish(&sha,digest);mbedtls_sha256_free(&sha);
    char hex[65];for (unsigned i=0;i<32;++i) snprintf(hex+i*2,3,"%02x",digest[i]);
    if (hash || strcmp(hex,release->sha256)) { snprintf(out->reason,sizeof(out->reason),"Release digest mismatch; installation stopped.");goto done; }
    ret=0;
done:
    if (d.file) fclose(d.file);
    if (h) curl_easy_cleanup(h);
    if (initialized) curl_global_cleanup();
    if (n.ctl_owned) sceNetCtlTerm();
    if (n.net_owned) sceNetTerm();
    free(n.net_memory);
    if (n.modules&1u) sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    out->http_status=(int)status;out->received=d.used;out->error=code?(int)code:-1;
    if (ret<0) { remove(path);if (!out->reason[0]) snprintf(out->reason,sizeof(out->reason),"%.191s",n.reason[0]?n.reason:curl_easy_strerror(code)); }
    return ret;
}
