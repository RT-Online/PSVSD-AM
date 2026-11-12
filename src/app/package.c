/* Bounded package metadata and system promotion; VitaShell API sequence.
 * TheFloW / RefurbedTech, GPL-3.0-or-later. */
#include "update_install.h"
#include <psp2/sysmodule.h>
#include <psp2/promoterutil.h>
#include <stdint.h>
static int package_operation(const char *path,int remove) {
    uint32_t args[]={0x180000,(uint32_t)-1,(uint32_t)-1,1,(uint32_t)-1,(uint32_t)-1};
    int result=-1,ret;SceSysmoduleOpt option={16,&result,{-1,-1}};
    int paf=0,module=0,initialized=0;
    if (sceSysmoduleIsLoadedInternal(SCE_SYSMODULE_INTERNAL_PAF)!=0) {
        ret=sceSysmoduleLoadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF,sizeof(args),args,&option);
        if (ret<0) return ret;
        paf=1;
    }
    if (sceSysmoduleIsLoadedInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL)!=0) {
        ret=sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);if (ret<0) goto done;module=1;
    }
    ret=scePromoterUtilityInit();if (ret<0) goto done;initialized=1;
    ret=remove?scePromoterUtilityDeletePkg(path):scePromoterUtilityPromotePkgWithRif(path,1);
    if (ret>=0 && !remove) { int promoted=0;ret=scePromoterUtilityGetResult(&promoted);if (ret>=0) ret=promoted; }
done:
    if (initialized) scePromoterUtilityExit();
    if (module) sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    if (paf) { SceSysmoduleOpt out={0,&result,{0,0}};sceSysmoduleUnloadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF,0,NULL,&out); }
    return ret;
}
int am_package_promote(const char *path) { return package_operation(path,0); }
int am_package_remove_helper(void) { return package_operation(AM_INSTALLER_TITLE_ID,1); }
