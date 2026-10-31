#ifndef PSVSD_AM_UI_H
#define PSVSD_AM_UI_H
#include "manager.h"
enum { VIEW_OVERVIEW,VIEW_SETTINGS,VIEW_TEST,VIEW_LOG,VIEW_CREDITS,VIEW_REBOOT,VIEW_EXIT,AM_NAV_COUNT };
typedef struct {
    int nav,field,editing,dirty,reboot_pending,busy,touch_available,return_nav;
    int clear_confirm,clear_choice;
    int copy_active;
    AmCopyProgress copy;
    unsigned log_page,progress;
    AmSettings pending,saved;
    AmStatus status;
    AmTestResult test;
    int test_attempted,test_passed;
    char message[192],stage[80],log[16384];
} AmUi;
void am_ui_render(uint32_t *pixels,const AmUi *model);
unsigned am_ui_log_pages(const char *text);
enum { AM_UP=1,AM_DOWN=2,AM_LEFT=4,AM_RIGHT=8,AM_CROSS=16,AM_CIRCLE=32,AM_SQUARE=64,AM_TRIANGLE=128 };
enum { ACTION_NONE,ACTION_TEST,ACTION_EXPORT,ACTION_REBOOT,ACTION_EXIT,ACTION_LOG,ACTION_SAVE_REBOOT,ACTION_SAVE_EXIT,ACTION_CLEAR_LOG };
int am_ui_input(AmUi *model,unsigned keys);
void am_ui_navigate(AmUi *model,int view);
#endif
