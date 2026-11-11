#ifndef PSVSD_AM_TOUCH_H
#define PSVSD_AM_TOUCH_H
#include "ui.h"
typedef struct { int min_x,min_y,max_x,max_y; } AmTouchBounds;
typedef struct {
    int waiting,active,id,start_x,start_y,last_x,last_y,moved;
    unsigned samples;
} AmTouchState;
void am_touch_block(AmTouchState *state);
int am_touch_point(const AmTouchBounds *bounds,int raw_x,int raw_y,int *x,int *y);
int am_touch_sample(AmTouchState *state,AmUi *ui,int contacts,int id,int x,int y);
int am_ui_tap(AmUi *ui,int x,int y);
#define AM_NAV_Y(i) ((i)<VIEW_REBOOT ? 124+(i)*40 : 388+((i)-VIEW_REBOOT)*48)
#define AM_UPDATE_X 702
#define AM_UPDATE_Y 126
#define AM_UPDATE_W 202
#define AM_UPDATE_H 42
#define AM_UPDATE_ROWS 5
#define AM_FOOTER_Y 480
#define AM_FOOTER_H 64
#define AM_FOOTER_BUTTON_Y 492
#define AM_FOOTER_BUTTON_H 42
#define AM_CONTENT_BOTTOM 472
#define AM_LOG_ROWS 10
#define AM_REFRESH_X 632
#define AM_REFRESH_W 140
#define AM_BACK_X 790
#define AM_BACK_W 134
#define AM_PREV_X 278
#define AM_NEXT_X 378
#define AM_PAGE_W 88
#define AM_CLEAR_X 478
#define AM_CLEAR_W 140
#define AM_CONFIRM_X 282
#define AM_CANCEL_X 600
#define AM_CONFIRM_Y 366
#define AM_CONFIRM_W 300
#define AM_CONFIRM_H 62
#define AM_DATE_OK_X 501
#define AM_DATE_OK_W 180
#define AM_TEST_X 726
#define AM_TEST_Y 362
#define AM_TEST_W 178
#define AM_TEST_H 43
#define AM_CHOICE_X 276
#define AM_CHOICE_Y 214
#define AM_CHOICE_W 628
#define AM_CHOICE_H 72
#define AM_CHOICE_STEP 88
#define AM_CHOICE_COUNT 2
#endif
