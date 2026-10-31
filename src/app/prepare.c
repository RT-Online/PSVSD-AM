/* Streamed, verified app-folder preparation. GPL-3.0-or-later. */
#include "platform.h"
#include "manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define COPY_ENOENT ((int)0x80010002)
#define COPY_EXISTS ((int)0x80010011)
#define COPY_ERROR ((int)0x80010005)
#define COPY_CHUNK 32768u
#define COPY_MAX_ITEMS 8192u
#define COPY_MAX_DEPTH 24u
#define COPY_PATH 512u
typedef struct {
    char *path;
    uint64_t size;
    int dir;
} CopyItem;
typedef struct {
    CopyItem *items;
    size_t count,capacity;
    unsigned published,old_moved;
    char staging[COPY_PATH],failure[COPY_PATH];
    unsigned char *first,*second;
    AmCopyProgress state;
    AmCopyCallback progress;
    void *ctx;
} Copy;
static const char *const copy_apps[]={"RTPSV0001","VITASHELL"};
static int copy_fail(Copy *c,const char *path,int ret) {
    snprintf(c->failure,sizeof(c->failure),"%s",path);return ret<0?ret:COPY_ERROR;
}
static int path_join(char *out,size_t cap,const char *root,const char *rel) {
    int n=snprintf(out,cap,"%s/%s",root,rel);return n<0 || (size_t)n>=cap?COPY_ERROR:0;
}
static int storage_ready(int restoring) {
    AmStatus s;am_status(&s);
    return s.verified_mount && (restoring?(s.actual_target==AM_UX0 && s.original_ready):s.actual_target==AM_UMA0);
}
static int folder(const char *path) {
    SceIoStat st;int r=sceIoGetstat(path,&st);
    if (r>=0) return SCE_S_ISDIR(st.st_mode)?0:COPY_ERROR;
    if (r!=COPY_ENOENT) return r;
    return sceIoMkdir(path,0777);
}
static int parents(const char *path) {
    char p[COPY_PATH];if (strlen(path)>=sizeof(p)) return COPY_ERROR;
    snprintf(p,sizeof(p),"%s",path);
    for (char *s=strchr(p,'/');s;s=strchr(s+1,'/')) {
        *s=0;int ret=folder(p);*s='/';if (ret<0) return ret;
    }
    return 0;
}
static void publish_progress(Copy *c,const char *phase,const char *path,unsigned base,unsigned span) {
    snprintf(c->state.phase,sizeof(c->state.phase),"%s",phase);
    snprintf(c->state.path,sizeof(c->state.path),"%s",path);
    uint64_t n=c->state.total_bytes?c->state.bytes_done:c->state.files_done;
    uint64_t total=c->state.total_bytes?c->state.total_bytes:c->state.total_files;
    unsigned fraction=total?(unsigned)(span*n/total):0;
    if (fraction>span) fraction=span;
    c->state.percent=base+fraction;
    if (c->progress) c->progress(&c->state,c->ctx);
}
static int remember(Copy *c,const char *path,int dir,uint64_t size) {
    if (c->count==COPY_MAX_ITEMS || size>UINT64_MAX/100 || UINT64_MAX/100-c->state.total_bytes<size) return COPY_ERROR;
    if (c->count==c->capacity) {
        size_t next=c->capacity?c->capacity*2:64;
        CopyItem *p=realloc(c->items,next*sizeof(*p));if (!p) return COPY_ERROR;
        c->items=p;c->capacity=next;
    }
    char *p=malloc(strlen(path)+1);if (!p) return COPY_ERROR;
    strcpy(p,path);c->items[c->count++]=(CopyItem){p,size,dir};
    if (!dir) { c->state.total_bytes+=size;++c->state.total_files; }
    return 0;
}
static int valid_name(const char *p,size_t cap) {
    if (!memchr(p,0,cap) || !*p) return 0;
    return !strchr(p,'/') && !strchr(p,'\\') && !strchr(p,':');
}
static int scan(Copy *c,const char *rel,unsigned depth) {
    char path[COPY_PATH];SceIoStat st;
    if (depth>COPY_MAX_DEPTH || path_join(path,sizeof(path),"ux0:app",rel)<0) return copy_fail(c,rel,COPY_ERROR);
    int r=sceIoGetstat(path,&st);if (r<0) return copy_fail(c,path,r);
    if (!SCE_S_ISREG(st.st_mode) && !SCE_S_ISDIR(st.st_mode)) return copy_fail(c,path,COPY_ERROR);
    int dir=SCE_S_ISDIR(st.st_mode);
    if ((r=remember(c,rel,dir,dir?0:(uint64_t)st.st_size))<0) return copy_fail(c,path,r);
    publish_progress(c,"Scanning app folders",rel,0,0);
    if (!dir) return 0;
    int fd=sceIoDopen(path);if (fd<0) return copy_fail(c,path,fd);
    SceIoDirent e;memset(&e,0,sizeof(e));
    while ((r=sceIoDread(fd,&e))>0) {
        if (!valid_name(e.d_name,sizeof(e.d_name))) { r=copy_fail(c,path,COPY_ERROR);break; }
        if (strcmp(e.d_name,".") && strcmp(e.d_name,"..")) {
            char child[COPY_PATH];
            if (path_join(child,sizeof(child),rel,e.d_name)<0) { r=copy_fail(c,rel,COPY_ERROR);break; }
            r=scan(c,child,depth+1);if (r<0) break;
        }
        memset(&e,0,sizeof(e));
    }
    int closed=sceIoDclose(fd);
    if (r<0) return r;
    return closed<0?copy_fail(c,path,closed):0;
}
static int read_exact(int fd,unsigned char *data,size_t n) {
    while (n) { int r=sceIoRead(fd,data,n);if (r<=0) return r<0?r:COPY_ERROR;data+=r;n-=(size_t)r; }
    return 0;
}
static int write_exact(int fd,const unsigned char *data,size_t n) {
    while (n) { int r=sceIoWrite(fd,data,n);if (r<=0) return r<0?r:COPY_ERROR;data+=r;n-=(size_t)r; }
    return 0;
}
static int eof(int fd) { unsigned char b;int r=sceIoRead(fd,&b,1);return r<0?r:r==0?0:COPY_ERROR; }
static int source_matches_size(const char *path,uint64_t size) {
    SceIoStat st;int r=sceIoGetstat(path,&st);
    return r<0?r:!SCE_S_ISREG(st.st_mode) || (uint64_t)st.st_size!=size?COPY_ERROR:0;
}
static int stream_file(Copy *c,const CopyItem *item,const char *target) {
    char source[COPY_PATH];int ret=path_join(source,sizeof(source),"ux0:app",item->path);
    if (ret<0 || (ret=source_matches_size(source,item->size))<0) return copy_fail(c,item->path,ret);
    int in=sceIoOpen(source,SCE_O_RDONLY,0);if (in<0) return copy_fail(c,source,in);
    ret=parents(target);int out=ret>=0?sceIoOpen(target,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_EXCL,0666):ret;
    if (out<0) { sceIoClose(in);return copy_fail(c,target,out); }
    publish_progress(c,"Copying app files",item->path,0,45);
    for (uint64_t off=0;off<item->size;) {
        size_t n=item->size-off>COPY_CHUNK?COPY_CHUNK:(size_t)(item->size-off);
        ret=read_exact(in,c->first,n);if (ret>=0) ret=write_exact(out,c->first,n);
        if (ret<0) break;
        off+=n;c->state.bytes_done+=n;publish_progress(c,"Copying app files",item->path,0,45);
    }
    if (ret>=0) ret=eof(in);
    int a=sceIoClose(in),b=sceIoClose(out);
    if (ret>=0) ret=a<0?a:b;
    if (ret<0) return copy_fail(c,item->path,ret);
    ++c->state.files_done;publish_progress(c,"Copying app files",item->path,0,45);return 0;
}
static int verify_file(Copy *c,const CopyItem *item,const char *target,const char *phase,unsigned base,unsigned span) {
    char source[COPY_PATH];int ret=path_join(source,sizeof(source),"ux0:app",item->path);
    if (ret<0 || (ret=source_matches_size(source,item->size))<0 || (ret=source_matches_size(target,item->size))<0)
        return copy_fail(c,item->path,ret);
    int in=sceIoOpen(source,SCE_O_RDONLY,0);if (in<0) return copy_fail(c,source,in);
    int out=sceIoOpen(target,SCE_O_RDONLY,0);if (out<0) { sceIoClose(in);return copy_fail(c,target,out); }
    publish_progress(c,phase,item->path,base,span);
    for (uint64_t off=0;off<item->size;) {
        size_t n=item->size-off>COPY_CHUNK?COPY_CHUNK:(size_t)(item->size-off);
        ret=read_exact(in,c->first,n);if (ret>=0) ret=read_exact(out,c->second,n);
        if (ret>=0 && memcmp(c->first,c->second,n)) ret=COPY_ERROR;
        if (ret<0) break;
        off+=n;c->state.bytes_done+=n;publish_progress(c,phase,item->path,base,span);
    }
    if (ret>=0) ret=eof(in);
    if (ret>=0) ret=eof(out);
    int a=sceIoClose(in),b=sceIoClose(out);if (ret>=0) ret=a<0?a:b;
    if (ret<0) return copy_fail(c,item->path,ret);
    ++c->state.files_done;publish_progress(c,phase,item->path,base,span);return 0;
}
/* Verify the complete directory topology against the scanned manifest, including
 * empty directories and hidden files. Extra or newly added files fail the copy. */
static int verify_tree(Copy *c,const char *root,const char *rel,unsigned depth,size_t *seen) {
    char path[COPY_PATH];
    if (depth>COPY_MAX_DEPTH || path_join(path,sizeof(path),root,rel)<0)
        return copy_fail(c,rel,COPY_ERROR);
    const CopyItem *item=NULL;
    for (size_t i=0;i<c->count;++i) if (!strcmp(c->items[i].path,rel)) { item=&c->items[i];break; }
    if (!item) return copy_fail(c,path,COPY_ERROR);
    SceIoStat st;int ret=sceIoGetstat(path,&st);
    if (ret<0) return copy_fail(c,path,ret);
    if (item->dir ? !SCE_S_ISDIR(st.st_mode) :
        !SCE_S_ISREG(st.st_mode) || (uint64_t)st.st_size!=item->size)
        return copy_fail(c,path,COPY_ERROR);
    ++*seen;
    if (!item->dir) return 0;
    int fd=sceIoDopen(path);if (fd<0) return copy_fail(c,path,fd);
    SceIoDirent entry;memset(&entry,0,sizeof(entry));
    while ((ret=sceIoDread(fd,&entry))>0) {
        if (!valid_name(entry.d_name,sizeof(entry.d_name))) { ret=COPY_ERROR;break; }
        if (strcmp(entry.d_name,".") && strcmp(entry.d_name,"..")) {
            char child[COPY_PATH];
            ret=path_join(child,sizeof(child),rel,entry.d_name);
            if (ret>=0) ret=verify_tree(c,root,child,depth+1,seen);
            if (ret<0) break;
        }
        memset(&entry,0,sizeof(entry));
    }
    int closed=sceIoDclose(fd);
    return ret<0?copy_fail(c,path,ret):closed<0?copy_fail(c,path,closed):0;
}
static int verify_manifest(Copy *c,const char *root) {
    size_t seen=0;
    for (unsigned i=0;i<2;++i) {
        if (!storage_ready(c->state.restoring)) return copy_fail(c,"Storage mapping changed",COPY_ERROR);
        int ret=verify_tree(c,root,copy_apps[i],0,&seen);
        if (ret<0) return ret;
    }
    return seen==c->count?0:copy_fail(c,root,COPY_ERROR);
}
static int verify_all(Copy *c,int installed) {
    char checked_root[COPY_PATH];
    if (installed) snprintf(checked_root,sizeof(checked_root),"uma0:app");
    else if (path_join(checked_root,sizeof(checked_root),c->staging,"new")<0) return COPY_ERROR;
    int checked=verify_manifest(c,"ux0:app");
    if (checked>=0) checked=verify_manifest(c,checked_root);
    if (checked<0) return checked;
    c->state.bytes_done=0;c->state.files_done=0;
    for (size_t i=0;i<c->count;++i) if (!c->items[i].dir) {
        char path[COPY_PATH],root[COPY_PATH];
        if (!storage_ready(c->state.restoring)) return copy_fail(c,"Storage mapping changed",COPY_ERROR);
        if (installed) snprintf(root,sizeof(root),"uma0:app");
        else if (path_join(root,sizeof(root),c->staging,"new")<0) return COPY_ERROR;
        if (path_join(path,sizeof(path),root,c->items[i].path)<0) return COPY_ERROR;
        int r=verify_file(c,&c->items[i],path,installed?"Verifying installed files":"Verifying copied files",installed?75:45,installed?24:25);
        if (r<0) return r;
    }
    return 0;
}
static int sync_usb(Copy *c,const char *phase,unsigned percent) {
    if (!storage_ready(c->state.restoring)) return copy_fail(c,"Storage mapping changed before flush",COPY_ERROR);
    publish_progress(c,phase,"uma0:",percent,0);
    int r=sceIoSync("uma0:",0);return r<0?copy_fail(c,phase,r):0;
}
/* Promote whole verified folders. The previous trees (including stale files)
 * remain untouched in staging/old until both replacements pass verification. */
static int publish_all(Copy *c) {
    c->state.files_done=0;c->state.bytes_done=0;
    for (unsigned i=0;i<2;++i) {
        char target[COPY_PATH],fresh[COPY_PATH],old[COPY_PATH],root[COPY_PATH];
        if (!storage_ready(c->state.restoring)) return copy_fail(c,"Storage mapping changed",COPY_ERROR);
        if (path_join(target,sizeof(target),"uma0:app",copy_apps[i])<0 ||
            path_join(root,sizeof(root),c->staging,"new")<0 || path_join(fresh,sizeof(fresh),root,copy_apps[i])<0 ||
            path_join(root,sizeof(root),c->staging,"old")<0 || path_join(old,sizeof(old),root,copy_apps[i])<0) return COPY_ERROR;
        int ret=parents(target);if (ret<0) return copy_fail(c,target,ret);
        SceIoStat st;ret=sceIoGetstat(target,&st);
        if (ret>=0) {
            if (!SCE_S_ISDIR(st.st_mode) && !SCE_S_ISREG(st.st_mode)) return copy_fail(c,target,COPY_ERROR);
            ret=parents(old);
            if (ret>=0 && !storage_ready(c->state.restoring)) ret=COPY_ERROR;
            if (ret>=0) ret=sceIoRename(target,old);
            if (ret<0) return copy_fail(c,target,ret);
            c->old_moved|=1u<<i;
        } else if (ret!=COPY_ENOENT) return copy_fail(c,target,ret);
        if (!storage_ready(c->state.restoring)) return copy_fail(c,"Storage mapping changed",COPY_ERROR);
        ret=sceIoRename(fresh,target);if (ret<0) return copy_fail(c,target,ret);
        c->published|=1u<<i;
        size_t prefix=strlen(copy_apps[i]);
        for (size_t j=0;j<c->count;++j) if (!c->items[j].dir &&
            !strncmp(c->items[j].path,copy_apps[i],prefix) && c->items[j].path[prefix]=='/') {
            ++c->state.files_done;c->state.bytes_done+=c->items[j].size;
        }
        publish_progress(c,"Installing verified folders",copy_apps[i],70,5);
    }
    return 0;
}
static int rollback(Copy *c) {
    int failed=0;
    for (unsigned n=2;n>0;--n) {
        unsigned i=n-1,bit=1u<<i;
        if (!((c->published|c->old_moved)&bit)) continue;
        char target[COPY_PATH],fresh[COPY_PATH],old[COPY_PATH],root[COPY_PATH];
        if (!storage_ready(c->state.restoring)) return COPY_ERROR;
        if (path_join(target,sizeof(target),"uma0:app",copy_apps[i])<0 ||
            path_join(root,sizeof(root),c->staging,"new")<0 || path_join(fresh,sizeof(fresh),root,copy_apps[i])<0 ||
            path_join(root,sizeof(root),c->staging,"old")<0 || path_join(old,sizeof(old),root,copy_apps[i])<0) { failed=COPY_ERROR;continue; }
        /* Move the new tree aside rather than deleting files during recovery. */
        int ret=(c->published&bit)?sceIoRename(target,fresh):0;
        if (ret>=0 && (c->old_moved&bit)) {
            ret=storage_ready(c->state.restoring)?sceIoRename(old,target):COPY_ERROR;
        }
        if (ret<0) { failed=ret;am_log("USB folder rollback failed %s: %08X\n",target,(unsigned)ret); }
    }
    return failed;
}
static int remove_tree(const char *path,unsigned depth) {
    if (depth>COPY_MAX_DEPTH+4) return COPY_ERROR;
    int fd=sceIoDopen(path);if (fd<0) return fd;
    SceIoDirent e;int ret=0,r;memset(&e,0,sizeof(e));
    while ((r=sceIoDread(fd,&e))>0) {
        if (!valid_name(e.d_name,sizeof(e.d_name))) { ret=COPY_ERROR;break; }
        if (strcmp(e.d_name,".") && strcmp(e.d_name,"..")) {
            char child[COPY_PATH];int x=path_join(child,sizeof(child),path,e.d_name);
            if (x>=0) x=SCE_S_ISDIR(e.d_stat.st_mode)?remove_tree(child,depth+1):sceIoRemove(child);
            if (x<0) { ret=x;break; }
        }
        memset(&e,0,sizeof(e));
    }
    int closed=sceIoDclose(fd);if (ret>=0 && r<0) ret=r;if (ret>=0 && closed<0) ret=closed;
    return ret<0?ret:sceIoRmdir(path);
}
static int prepare_apps(int restoring,AmCopyCallback progress,void *ctx,char *message,size_t cap) {
    if (!cap) return COPY_ERROR;
    message[0]=0;
    if (!storage_ready(restoring)) { snprintf(message,cap,"Destination must be verified at uma0 before copying.");return COPY_ERROR; }
    Copy *c=calloc(1,sizeof(*c));if (!c) return COPY_ERROR;
    c->state.restoring=restoring;
    c->progress=progress;c->ctx=ctx;c->first=malloc(COPY_CHUNK);c->second=malloc(COPY_CHUNK);
    int ret=0;
    if (!c->first || !c->second) { ret=COPY_ERROR;goto done; }
    am_log("Preparing app folders: ux0:app to verified uma0:app; complete folders replaced; destination-only files removed.\n");
    if ((ret=scan(c,"RTPSV0001",0))<0 || (ret=scan(c,"VITASHELL",0))<0) goto done;
    const char *required[]={"RTPSV0001/eboot.bin","VITASHELL/eboot.bin"};
    for (unsigned n=0;n<2;++n) {
        int found=0;for (size_t i=0;i<c->count;++i) if (!c->items[i].dir && !strcmp(c->items[i].path,required[n])) found=1;
        if (!found) { ret=copy_fail(c,required[n],COPY_ENOENT);goto done; }
    }
    if (!storage_ready(c->state.restoring)) { ret=copy_fail(c,"Storage mapping changed before copy",COPY_ERROR);goto done; }
    const char *dirs[]={"uma0:data","uma0:data/PSVSD_AM"};
    for (unsigned i=0;i<2;++i) if ((ret=folder(dirs[i]))<0) { copy_fail(c,dirs[i],ret);goto done; }
    uint32_t token=sceKernelGetProcessTimeLow();
    for (unsigned i=0;i<16;++i) {
        snprintf(c->staging,sizeof(c->staging),"uma0:data/PSVSD_AM/prepare-%08X",token+i);
        ret=sceIoMkdir(c->staging,0777);if (ret!=COPY_EXISTS) break;
    }
    if (ret<0) { copy_fail(c,c->staging,ret);c->staging[0]=0;goto done; }
    c->state.files_done=0;c->state.bytes_done=0;
    for (size_t i=0;i<c->count;++i) {
        char path[COPY_PATH],root[COPY_PATH];
        if (!storage_ready(c->state.restoring)) { ret=copy_fail(c,"Storage mapping changed",COPY_ERROR);goto done; }
        if (path_join(root,sizeof(root),c->staging,"new")<0 || path_join(path,sizeof(path),root,c->items[i].path)<0) { ret=COPY_ERROR;goto done; }
        if (c->items[i].dir) { ret=parents(path);if (ret>=0) ret=folder(path); }
        else ret=stream_file(c,&c->items[i],path);
        if (ret<0) { if (!c->failure[0]) copy_fail(c,path,ret);goto done; }
    }
    if ((ret=sync_usb(c,"Flushing copied files",45))<0 || (ret=verify_all(c,0))<0 || (ret=publish_all(c))<0 ||
        (ret=sync_usb(c,"Flushing installed files",75))<0 || (ret=verify_all(c,1))<0) goto done;
    publish_progress(c,"App folders verified", "Continuing Save",100,0);
    am_log("Destination app folders verified: %u files, %llu bytes.\n",c->state.total_files,(unsigned long long)c->state.total_bytes);
done:
    if (ret<0) {
        /* Do not roll back or clean up through an unverified mount point. */
        int restored=c->staging[0] && !storage_ready(c->state.restoring)?COPY_ERROR:rollback(c);
        snprintf(message,cap,restored<0?"App copy failed; recovery files retained. See Log.":"App copy failed (%08X). See Log.",(unsigned)ret);
        am_log("App copy failed at %s: %08X; rollback=%08X\n",c->failure[0]?c->failure:c->state.path,(unsigned)ret,(unsigned)restored);
        if (restored<0) {
            am_log("USB app recovery files retained: %s\n",c->staging);c->staging[0]=0;
        }
    }
    if (c->staging[0]) {
        if (!storage_ready(c->state.restoring)) am_log("Storage mapping no longer verified; staging retained at %s\n",c->staging);
        else {
            int clean=remove_tree(c->staging,0);
            if (clean<0) am_log("USB copy staging cleanup %s: %08X (files retained).\n",c->staging,(unsigned)clean);
        }
    }
    for (size_t i=0;i<c->count;++i) free(c->items[i].path);
    free(c->items);free(c->first);free(c->second);free(c);return ret;
}

int am_prepare_usb_apps(AmCopyCallback progress,void *ctx,char *message,size_t cap) { return prepare_apps(0,progress,ctx,message,cap); }
int am_prepare_original_apps(AmCopyCallback progress,void *ctx,char *message,size_t cap) { return prepare_apps(1,progress,ctx,message,cap); }
