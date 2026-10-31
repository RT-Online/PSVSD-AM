/* Vita Wi-Fi gate and verified HTTPS transport. GPL-3.0-or-later. */
#include "updates.h"
#include "../common/paths.h"
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/net/http.h>
#include <psp2/libssl.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    void *net_memory;
    int net_owned,ctl_owned,ssl_owned,http_owned;
    unsigned modules;
} Network;
static const SceSysmoduleModuleId modules[]={SCE_SYSMODULE_NET,SCE_SYSMODULE_SSL,SCE_SYSMODULE_HTTP};
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
static int get(void *context,char *body,size_t cap,size_t *used,int *status) {
    Network *n=context;int tmpl=0,conn=0,req=0,ret=-1;
    unsigned long long started=sceKernelGetProcessTimeWide();
    if (!wifi(n)) return -1; /* Connection may have changed since gating. */
    if ((ret=load_module(n,1))<0 || (ret=load_module(n,2))<0) goto done;
    ret=sceSslInit(256*1024);
    if (ret>=0) n->ssl_owned=1;
    else if (ret!=(int)SCE_SSL_ERROR_ALREADY_INITED) goto done;
    ret=sceHttpInit(256*1024);if (ret<0) goto done;
    n->http_owned=1;
    ret=sceHttpsEnableOption(SCE_HTTPS_FLAG_SERVER_VERIFY|SCE_HTTPS_FLAG_CN_CHECK|
        SCE_HTTPS_FLAG_NOT_AFTER_CHECK|SCE_HTTPS_FLAG_NOT_BEFORE_CHECK|SCE_HTTPS_FLAG_KNOWN_CA_CHECK);
    if (ret<0) goto done;
    tmpl=sceHttpCreateTemplate("PSVSD-AM/" ACCESSORY_VERSION,SCE_HTTP_VERSION_1_1,1);
    if (tmpl<=0) { ret=tmpl<0?tmpl:-1;goto done; }
    if ((ret=sceHttpSetResolveTimeOut(tmpl,3000000))<0 ||
        (ret=sceHttpSetResolveRetry(tmpl,0))<0 ||
        (ret=sceHttpSetConnectTimeOut(tmpl,5000000))<0 ||
        (ret=sceHttpSetSendTimeOut(tmpl,5000000))<0 ||
        (ret=sceHttpSetRecvTimeOut(tmpl,5000000))<0 ||
        (ret=sceHttpSetAutoRedirect(tmpl,SCE_HTTP_DISABLE))<0) goto done;
    conn=sceHttpCreateConnectionWithURL(tmpl,AM_RELEASE_API,0);
    if (conn<=0) { ret=conn<0?conn:-1;goto done; }
    req=sceHttpCreateRequestWithURL(conn,SCE_HTTP_METHOD_GET,AM_RELEASE_API,0);
    if (req<=0) { ret=req<0?req:-1;goto done; }
    if ((ret=sceHttpAddRequestHeader(req,"Accept","application/vnd.github+json",SCE_HTTP_HEADER_OVERWRITE))<0 ||
        (ret=sceHttpAddRequestHeader(req,"X-GitHub-Api-Version","2026-03-10",SCE_HTTP_HEADER_OVERWRITE))<0 ||
        (ret=sceHttpAddRequestHeader(req,"Accept-Encoding","identity",SCE_HTTP_HEADER_OVERWRITE))<0 ||
        (ret=sceHttpSendRequest(req,NULL,0))<0 || (ret=sceHttpGetStatusCode(req,status))<0) goto done;
    if (*status!=200) { ret=0;goto done; }
    *used=0;
    for (;;) {
        if (sceKernelGetProcessTimeWide()-started>20000000ull) { ret=-1;break; }
        char extra;size_t remaining=cap-*used;
        unsigned amount=remaining>2048?2048:(unsigned)remaining;
        ret=sceHttpReadData(req,remaining?body+*used:&extra,remaining?amount:1);
        if (ret<=0) break;
        if (!remaining || (unsigned)ret>amount) { ret=-1;break; }
        *used+=(size_t)ret;
    }
    if (ret>=0) body[*used]=0;
done:
    if (req>0) sceHttpDeleteRequest(req);
    if (conn>0) sceHttpDeleteConnection(conn);
    if (tmpl>0) sceHttpDeleteTemplate(tmpl);
    return ret;
}
void am_update_check(AmUpdate *out) {
    Network network={0};AmUpdateTransport transport={&network,wifi,get};
    am_update_check_with_transport(out,ACCESSORY_VERSION,&transport);
    if (network.http_owned) sceHttpTerm();
    if (network.ssl_owned) sceSslTerm();
    if (network.ctl_owned) sceNetCtlTerm();
    if (network.net_owned) sceNetTerm();
    free(network.net_memory);
    for (unsigned n=3;n>0;--n) if (network.modules&(1u<<(n-1))) sceSysmoduleUnloadModule(modules[n-1]);
}
