/* Explicit package staging and helper handoff. GPL-3.0-or-later. */
#ifndef AM_UPDATE_INSTALL_H
#define AM_UPDATE_INSTALL_H
#include "updates.h"
#include "../common/paths.h"
#define AM_APPMGR_NOT_RUNNING 0x80802012u
int am_update_prepare(const char *version,char *reason,unsigned cap,AmUpdateProgressCallback progress,void *context);
int am_update_launch_installer(void);
int am_update_launch_app(void);
#define AM_RELAUNCH_WAIT_US 300000000u
int am_update_relaunch_start(void);
int am_update_relaunch_confirm(void);
void am_update_relaunch_cancel(void);
int am_package_remove_helper(void);
int am_update_cleanup_installer(void);
enum { AM_INSTALLER_REMOVED=1,AM_INSTALLER_CLEANUP_DEFERRED };
int am_update_cleanup_installer_on_return(void);
int am_package_promote(const char *path);
int am_package_release(const char *path,const char *semantic,char package[6]);
int am_package_helper_release(const char *path,const char *semantic,const char *package);
#endif
