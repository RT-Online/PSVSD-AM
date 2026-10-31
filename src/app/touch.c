/* Front-panel taps, tap release gating and log swipes. GPL-3.0-or-later. */
#include "touch.h"
#include <string.h>
#include <stdlib.h>

static int inside(int x,int y,int left,int top,int width,int height) {
    return x>=left && x<left+width && y>=top && y<top+height;
}
void am_touch_block(AmTouchState *s) { memset(s,0,sizeof(*s));s->waiting=1; }
int am_touch_point(const AmTouchBounds *b,int rx,int ry,int *x,int *y) {
    if (b->max_x<=b->min_x || b->max_y<=b->min_y ||
        rx<b->min_x || rx>b->max_x || ry<b->min_y || ry>b->max_y) return 0;
    *x=(int)((int64_t)(rx-b->min_x)*959/(b->max_x-b->min_x));
    *y=(int)((int64_t)(ry-b->min_y)*543/(b->max_y-b->min_y));
    return 1;
}
int am_ui_tap(AmUi *m,int x,int y) {
    if (m->busy) return ACTION_NONE;
    if (m->clear_confirm) {
        if (inside(x,y,AM_BACK_X,AM_FOOTER_Y,AM_BACK_W,AM_FOOTER_H)) return am_ui_input(m,AM_CIRCLE);
        if (inside(x,y,AM_CONFIRM_X,AM_CONFIRM_Y,AM_CONFIRM_W,AM_CONFIRM_H)) {
            m->clear_choice=0;return am_ui_input(m,AM_CROSS);
        }
        if (inside(x,y,AM_CANCEL_X,AM_CONFIRM_Y,AM_CONFIRM_W,AM_CONFIRM_H)) {
            m->clear_choice=1;return am_ui_input(m,AM_CROSS);
        }
        return ACTION_NONE;
    }
    for (int i=0;i<AM_NAV_COUNT;++i) {
        int top=AM_NAV_Y(i)-2;
        if (inside(x,y,24,top,218,i<VIEW_REBOOT?40:42)) {
            am_ui_navigate(m,i);return ACTION_NONE;
        }
    }
    if (inside(x,y,AM_BACK_X,AM_FOOTER_Y,AM_BACK_W,AM_FOOTER_H)) return am_ui_input(m,AM_CIRCLE);
    if (inside(x,y,AM_REFRESH_X,AM_FOOTER_Y,AM_REFRESH_W,AM_FOOTER_H)) return ACTION_EXPORT;
    if (m->nav==VIEW_LOG || m->nav==VIEW_UPDATES) {
        if (inside(x,y,AM_PREV_X,AM_FOOTER_Y,AM_PAGE_W,AM_FOOTER_H)) return am_ui_input(m,AM_LEFT);
        if (inside(x,y,AM_NEXT_X,AM_FOOTER_Y,AM_PAGE_W,AM_FOOTER_H)) return am_ui_input(m,AM_RIGHT);
        if (m->nav==VIEW_LOG && inside(x,y,AM_CLEAR_X,AM_FOOTER_Y,AM_CLEAR_W,AM_FOOTER_H)) return am_ui_input(m,AM_TRIANGLE);
    }
    if (m->nav==VIEW_UPDATES && inside(x,y,AM_UPDATE_X,AM_UPDATE_Y,AM_UPDATE_W,AM_UPDATE_H)) return ACTION_CHECK_UPDATES;
    if (m->nav==VIEW_SETTINGS && inside(x,y,276,214,628,72)) {
        m->editing=1;m->field=0;return am_ui_input(m,AM_CROSS);
    }
    if (m->nav==VIEW_SETTINGS && inside(x,y,276,300,628,72)) {
        m->editing=1;m->field=1;return am_ui_input(m,AM_CROSS);
    }
    if (m->nav==VIEW_TEST && inside(x,y,AM_TEST_X,AM_TEST_Y,AM_TEST_W,AM_TEST_H)) return ACTION_TEST;
    if (m->nav==VIEW_REBOOT || m->nav==VIEW_EXIT) {
        for (int i=0;i<AM_CHOICE_COUNT;++i) if (inside(x,y,AM_CHOICE_X,AM_CHOICE_Y+i*AM_CHOICE_STEP,AM_CHOICE_W,AM_CHOICE_H)) {
            m->editing=1;m->field=i;return am_ui_input(m,AM_CROSS);
        }
    }
    return ACTION_NONE;
}
int am_touch_sample(AmTouchState *s,AmUi *m,int contacts,int id,int x,int y) {
    if (m->busy || contacts>1 || contacts<0) { am_touch_block(s);return ACTION_NONE; }
    if (s->waiting) { if (!contacts) s->waiting=0;return ACTION_NONE; }
    if (contacts==1) {
        if (!s->active) {
            s->active=1;s->id=id;s->start_x=s->last_x=x;s->start_y=s->last_y=y;
            s->samples=1;s->moved=0;
        } else if (s->id!=id) am_touch_block(s);
        else {
            s->last_x=x;s->last_y=y;++s->samples;
            if (abs(x-s->start_x)>12 || abs(y-s->start_y)>12) s->moved=1;
        }
        return ACTION_NONE;
    }
    if (!s->active) return ACTION_NONE;
    s->active=0;
    if (s->samples>50) return ACTION_NONE;
    int dx=s->last_x-s->start_x,dy=s->last_y-s->start_y;
    if (s->moved) {
        if (m->clear_confirm) return ACTION_NONE;
        if ((m->nav!=VIEW_LOG && m->nav!=VIEW_UPDATES) || s->start_x<258 || s->start_y<200 || s->start_y>=AM_CONTENT_BOTTOM) return ACTION_NONE;
        if (abs(dx)>=60 && abs(dx)>abs(dy)) return am_ui_input(m,dx<0?AM_RIGHT:AM_LEFT);
        if (abs(dy)>=60 && abs(dy)>abs(dx)) return am_ui_input(m,dy<0?AM_RIGHT:AM_LEFT);
        return ACTION_NONE;
    }
    return am_ui_tap(m,s->last_x,s->last_y);
}
