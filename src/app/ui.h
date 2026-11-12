#ifndef PSVSD_AM_UI_H
#define PSVSD_AM_UI_H
#include "manager.h"
#include "updates.h"
enum { VIEW_OVERVIEW,VIEW_SETTINGS,VIEW_TEST,VIEW_LOG,VIEW_UPDATES,VIEW_CREDITS,VIEW_REBOOT,VIEW_EXIT,AM_NAV_COUNT };
typedef struct {
    int nav,field,editing,dirty,reboot_pending,busy,touch_available,return_nav;
    int clear_confirm,clear_choice;
    int update_confirm,update_choice,update_date_notice,auto_update;
    int copy_active;
    AmCopyProgress copy;
    unsigned log_page,progress;
    AmSettings pending,saved;
    AmStatus status;
    AmUpdate update;
    unsigned update_scroll,update_animation;
    AmUpdateProgress update_progress;
    AmTestResult test;
    int test_attempted,test_passed;
    AmSpeedResult speed;
    unsigned speed_progress;
    int speed_active,speed_attempted;
    char message[192],stage[80],log[16384];
} AmUi;
void am_ui_render(uint32_t *pixels,const AmUi *model);
unsigned am_ui_log_pages(const char *text);
enum { AM_UPDATE_NOTE_LINE=24 };
int am_ui_scroll_updates(AmUi *model,int pixels);
int am_ui_auto_update_ready(const AmUi *model);
enum { AM_UP=1,AM_DOWN=2,AM_LEFT=4,AM_RIGHT=8,AM_CROSS=16,AM_CIRCLE=32,AM_SQUARE=64,AM_TRIANGLE=128 };
enum { ACTION_NONE,ACTION_TEST,ACTION_EXPORT,ACTION_REBOOT,ACTION_EXIT,ACTION_LOG,ACTION_SAVE_REBOOT,ACTION_SAVE_EXIT,ACTION_CLEAR_LOG,ACTION_CHECK_UPDATES,ACTION_INSTALL_UPDATE,ACTION_SPEED_TEST,ACTION_TOGGLE_AUTO_UPDATE };
int am_ui_input(AmUi *model,unsigned keys);
void am_ui_navigate(AmUi *model,int view);
#endif
