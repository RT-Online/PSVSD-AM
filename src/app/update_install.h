/* Explicit package staging and helper handoff. GPL-3.0-or-later. */
#ifndef AM_UPDATE_INSTALL_H
#define AM_UPDATE_INSTALL_H
#include "updates.h"
#define AM_UPDATE_ROOT "ux0:data/PSVSD_Accessory/update"
#define AM_UPDATE_VPK AM_UPDATE_ROOT "/release.vpk"
#define AM_UPDATE_STAGE AM_UPDATE_ROOT "/package"
#define AM_UPDATE_HELPER AM_UPDATE_ROOT "/helper"
int am_update_prepare(const char *version,char *reason,unsigned cap,AmUpdateProgressCallback progress,void *context);
int am_update_launch_installer(void);
int am_update_launch_app(void);
int am_package_remove_helper(void);
int am_package_promote(const char *path);
int am_package_validate(const char *path,const char *expected);
#endif
