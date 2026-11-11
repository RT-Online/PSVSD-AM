/* Vita Wi-Fi and verified GitHub HTTPS. GPL-3.0-or-later. */
#include "updates.h"
#include "../common/paths.h"
#include <psp2/kernel/processmgr.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <curl/curl.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ssl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TLS_DATE_FLAGS (MBEDTLS_X509_BADCERT_EXPIRED | MBEDTLS_X509_BADCERT_FUTURE)
#define ASSET_CDN "https://release-assets.githubusercontent.com/"

typedef struct {
    void *net_memory;
    int net_owned,ctl_owned,module_owned;
    const char *stage;
    CURLcode error;
    long status;
    uint32_t certificate_flags;
    char reason[CURL_ERROR_SIZE];
} Network;

typedef struct {
    char *body;
    size_t cap,used;
} Response;

typedef struct {
    FILE *file;
    size_t used;
    AmUpdateProgressCallback progress;
    void *context;
} Download;

typedef struct {
    CURL *handle;
    Response *response;
    Download *download;
} Transfer;

static void network_close(Network *n) {
    if (n->ctl_owned) sceNetCtlTerm();
    if (n->net_owned) sceNetTerm();
    free(n->net_memory);
    if (n->module_owned) sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
}

static int wifi(void *context) {
    Network *n=context;
    if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NET)!=0) {
        if (sceSysmoduleLoadModule(SCE_SYSMODULE_NET)<0) return 0;
        n->module_owned=1;
    }
    if (sceNetShowNetstat()<0) {
        if (n->net_memory) return 0;
        n->net_memory=malloc(256*1024);
        if (!n->net_memory) return 0;
        SceNetInitParam param={n->net_memory,256*1024,0};
        if (sceNetInit(&param)<0) return 0;
        n->net_owned=1;
    }
    int state=0;
    if (sceNetCtlInetGetState(&state)<0) {
        if (sceNetCtlInit()>=0) n->ctl_owned=1;
        if (sceNetCtlInetGetState(&state)<0) return 0;
    }
    if (state!=SCE_NETCTL_STATE_CONNECTED) return 0;
    SceNetCtlInfo info={0};
    /* Require a Wi-Fi SSID before contacting the release server. */
    return sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_SSID,&info)>=0 && info.ssid[0];
}

static int verify_certificate(void *context,mbedtls_x509_crt *cert,int depth,uint32_t *flags) {
    (void)cert;
    (void)depth;
    Network *n=context;
    /* Record the flags while leaving certificate validation in effect. */
    n->certificate_flags|=*flags;
    return 0;
}

static CURLcode configure_tls(CURL *handle,void *ssl_config,void *context) {
    (void)handle;
    mbedtls_ssl_conf_verify(ssl_config,verify_certificate,context);
    return CURLE_OK;
}

static void note_date_error(const Network *n,AmUpdate *out) {
    if (n->error!=CURLE_PEER_FAILED_VERIFICATION || !n->certificate_flags ||
        (n->certificate_flags&~TLS_DATE_FLAGS)) return;
    out->state=UPDATE_DATE_ERROR;
    snprintf(out->message,sizeof(out->message),"Check the Vita's date and time.");
}

static size_t receive(char *data,size_t size,size_t count,void *context) {
    Transfer *transfer=context;
    if (size && count>SIZE_MAX/size) return 0;
    size_t bytes=size*count;
    long status=0;
    curl_easy_getinfo(transfer->handle,CURLINFO_RESPONSE_CODE,&status);
    if (status!=200) return bytes;
    if (transfer->download) {
        Download *d=transfer->download;
        if (bytes>AM_DOWNLOAD_MAX_BYTES-d->used) return 0;
        size_t written=fwrite(data,1,bytes,d->file);
        d->used+=written;
        return written;
    }
    Response *r=transfer->response;
    if (bytes>r->cap-r->used) return 0;
    memcpy(r->body+r->used,data,bytes);
    r->used+=bytes;
    return bytes;
}

static int download_progress(void *context,curl_off_t total,curl_off_t done,curl_off_t up_total,curl_off_t up_done) {
    (void)up_total;
    (void)up_done;
    Download *d=context;
    d->progress(d->context,AM_UPDATE_DOWNLOAD,
        done>0 && done<=AM_DOWNLOAD_MAX_BYTES?(unsigned)done:0,
        total>0 && total<=AM_DOWNLOAD_MAX_BYTES?(unsigned)total:0);
    return 0;
}

static int asset_redirect(const char *url) {
    return !strncmp(url,AM_RELEASE_DOWNLOAD_PREFIX,sizeof(AM_RELEASE_DOWNLOAD_PREFIX)-1) ||
        !strncmp(url,ASSET_CDN,sizeof(ASSET_CDN)-1);
}

static int request(Network *n,const char *url,Response *response,Download *download) {
    CURL *handle=NULL;
    struct curl_slist *headers=NULL;
    int initialized=0;
    const int api=!strcmp(url,AM_RELEASE_API);
    const uint64_t timeout_ms=download?180000:20000;
    uint64_t started=sceKernelGetProcessTimeWide();
    n->error=CURLE_OK;
    n->status=0;
    n->certificate_flags=0;
    n->reason[0]=0;
    n->stage="Wi-Fi recheck";
    if (!wifi(n)) return -1;

    n->stage="Initialize update connection";
    CURLcode code=curl_global_init(CURL_GLOBAL_DEFAULT);
    if (code!=CURLE_OK) goto done;
    initialized=1;
    const curl_version_info_data *version=curl_version_info(CURLVERSION_NOW);
    if (!version->ssl_version || strncmp(version->ssl_version,"mbedTLS/",8)) {
        code=CURLE_NOT_BUILT_IN;
        goto done;
    }
    handle=curl_easy_init();
    if (!handle) { code=CURLE_OUT_OF_MEMORY;goto done; }
    if (api) {
        const char *values[]={"Accept: application/vnd.github+json","X-GitHub-Api-Version: 2026-03-10"};
        for (unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i) {
            struct curl_slist *next=curl_slist_append(headers,values[i]);
            if (!next) { code=CURLE_OUT_OF_MEMORY;goto done; }
            headers=next;
        }
    }

    Transfer transfer={handle,response,download};
    n->stage="Configure update request";
#define SET(option,value) do { code=curl_easy_setopt(handle,option,value);if (code!=CURLE_OK) goto done; } while (0)
    SET(CURLOPT_URL,url);
    SET(CURLOPT_PROTOCOLS_STR,"https");
    SET(CURLOPT_USERAGENT,"PSVSD-AM/" ACCESSORY_VERSION);
    SET(CURLOPT_HTTPHEADER,headers);
    SET(CURLOPT_CONNECTTIMEOUT,8L);
    SET(CURLOPT_NOSIGNAL,1L);
    SET(CURLOPT_HTTP_VERSION,(long)CURL_HTTP_VERSION_1_1);
    SET(CURLOPT_SSLVERSION,(long)CURL_SSLVERSION_TLSv1_2);
    SET(CURLOPT_CAINFO,"app0:update-ca-bundle.pem");
    SET(CURLOPT_SSL_VERIFYPEER,1L);
    SET(CURLOPT_SSL_VERIFYHOST,2L);
    SET(CURLOPT_SSL_SESSIONID_CACHE,0L);
    SET(CURLOPT_SSL_CTX_DATA,n);
    SET(CURLOPT_SSL_CTX_FUNCTION,configure_tls);
    SET(CURLOPT_ERRORBUFFER,n->reason);
    SET(CURLOPT_WRITEFUNCTION,receive);
    SET(CURLOPT_WRITEDATA,&transfer);
    if (download && download->progress) {
        SET(CURLOPT_NOPROGRESS,0L);
        SET(CURLOPT_XFERINFOFUNCTION,download_progress);
        SET(CURLOPT_XFERINFODATA,download);
    }

    n->stage="HTTPS request";
    for (unsigned redirects=0;;++redirects) {
        uint64_t elapsed_ms=(sceKernelGetProcessTimeWide()-started)/1000;
        if (elapsed_ms>=timeout_ms) { code=CURLE_OPERATION_TIMEDOUT;break; }
        SET(CURLOPT_TIMEOUT_MS,(long)(timeout_ms-elapsed_ms));
        n->certificate_flags=0;
        n->status=0;
        code=curl_easy_perform(handle);
        curl_easy_getinfo(handle,CURLINFO_RESPONSE_CODE,&n->status);
        if (code!=CURLE_OK || n->status<300 || n->status>=400 || api) break;

        char *next=NULL;
        curl_easy_getinfo(handle,CURLINFO_REDIRECT_URL,&next);
        if (redirects==4 || !next || !asset_redirect(next)) {
            n->stage="Validate GitHub asset redirect";
            code=CURLE_TOO_MANY_REDIRECTS;
            break;
        }
        SET(CURLOPT_URL,next);
    }
#undef SET
done:
    n->error=code;
    if (code!=CURLE_OK && !n->reason[0])
        snprintf(n->reason,sizeof(n->reason),"%s",curl_easy_strerror(code));
    if (handle) curl_easy_cleanup(handle);
    curl_slist_free_all(headers);
    if (initialized) curl_global_cleanup();
    return code==CURLE_OK?0:-(int)code;
}

static int get(void *context,char *body,size_t cap,size_t *used,int *status) {
    Network *n=context;
    Response response={body,cap,0};
    int ret=request(n,AM_RELEASE_API,&response,NULL);
    *used=response.used;
    *status=(int)n->status;
    return ret;
}

void am_update_check(AmUpdate *out) {
    Network network={0};
    AmUpdateTransport transport={&network,wifi,get};
    am_update_check_with_transport(out,ACCESSORY_VERSION,&transport);
    if (out->state==UPDATE_ERROR && network.error!=CURLE_OK) {
        snprintf(out->stage,sizeof(out->stage),"%s",network.stage);
        out->error=(int)network.error;
        out->tls_verify=(int)network.certificate_flags;
        snprintf(out->reason,sizeof(out->reason),"%.191s",network.reason);
        note_date_error(&network,out);
    }
    network_close(&network);
}

static int verify_download(const Download *d,const char *path,const char *expected) {
    FILE *file=fopen(path,"rb");
    if (!file) return -1;
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    int status=mbedtls_sha256_starts(&sha,0);
    unsigned char buffer[16384],digest[32];
    size_t bytes,verified=0;
    if (d->progress) d->progress(d->context,AM_UPDATE_VERIFY,0,(unsigned)d->used);
    while (status==0 && (bytes=fread(buffer,1,sizeof(buffer),file))>0) {
        if (bytes>d->used-verified) { status=-1;break; }
        status=mbedtls_sha256_update(&sha,buffer,bytes);
        verified+=bytes;
        if (d->progress) d->progress(d->context,AM_UPDATE_VERIFY,(unsigned)verified,(unsigned)d->used);
    }
    if (ferror(file) || verified!=d->used) status=-1;
    fclose(file);
    if (status==0) status=mbedtls_sha256_finish(&sha,digest);
    mbedtls_sha256_free(&sha);
    if (status!=0) return -1;
    char hex[65];
    for (unsigned i=0;i<32;++i) snprintf(hex+i*2,3,"%02x",digest[i]);
    return strcmp(hex,expected)==0?0:-1;
}

int am_update_download(const AmUpdate *release,const char *path,AmUpdate *out,AmUpdateProgressCallback progress,void *context) {
    Network network={0};
    Download d={.progress=progress,.context=context};
    int ret=-1;
    memset(out,0,sizeof(*out));
    out->state=UPDATE_ERROR;
    snprintf(out->stage,sizeof(out->stage),"Download release VPK");
    if (progress) progress(context,AM_UPDATE_DOWNLOAD,0,0);
    if (release->state!=UPDATE_AVAILABLE ||
        strncmp(release->asset_url,AM_RELEASE_DOWNLOAD_PREFIX,sizeof(AM_RELEASE_DOWNLOAD_PREFIX)-1) ||
        strlen(release->sha256)!=64) {
        snprintf(out->reason,sizeof(out->reason),"Verified release asset unavailable.");
        goto done;
    }

    d.file=fopen(path,"wb");
    if (!d.file) { snprintf(out->reason,sizeof(out->reason),"Cannot create update file.");goto done; }
    int transfer=request(&network,release->asset_url,NULL,&d);
    if (fclose(d.file)!=0) {
        snprintf(out->reason,sizeof(out->reason),"Could not finish writing update.");
        goto done;
    }
    if (transfer<0 || network.status!=200 || !d.used) {
        snprintf(out->reason,sizeof(out->reason),"%.191s",
            network.reason[0]?network.reason:"Could not download the release VPK.");
        goto done;
    }

    snprintf(out->stage,sizeof(out->stage),"Verify release SHA-256");
    if (verify_download(&d,path,release->sha256)<0) {
        snprintf(out->reason,sizeof(out->reason),"Release digest mismatch. Installation stopped.");
        goto done;
    }
    ret=0;
done:
    network_close(&network);
    out->tls_verify=(int)network.certificate_flags;
    out->http_status=(int)network.status;
    out->received=d.used;
    out->error=ret<0?(network.error?(int)network.error:-1):0;
    if (ret<0) {
        remove(path);
        snprintf(out->message,sizeof(out->message),"Could not download the update. See Log.");
        note_date_error(&network,out);
    }
    return ret;
}
