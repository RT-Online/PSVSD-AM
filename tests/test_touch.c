#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/app/touch.h"
static int cases;
static void reset(AmUi *m,AmTouchState *s) { memset(m,0,sizeof(*m));memset(s,0,sizeof(*s));++cases; }
static int tap(AmUi *m,AmTouchState *s,int x,int y) {
    assert(am_touch_sample(s,m,1,3,x,y)==ACTION_NONE);
    return am_touch_sample(s,m,0,0,0,0);
}
int main(void) {
    AmUi m;AmTouchState s;int x,y;
    reset(&m,&s);AmTouchBounds bounds={0,0,1919,1087};
    assert(am_touch_point(&bounds,0,0,&x,&y) && x==0 && y==0);
    assert(am_touch_point(&bounds,1919,1087,&x,&y) && x==959 && y==543);
    assert(!am_touch_point(&bounds,-1,20,&x,&y));
    bounds=(AmTouchBounds){10,20,1929,1107};assert(am_touch_point(&bounds,10,20,&x,&y) && !x && !y);
    bounds.max_x=bounds.min_x;assert(!am_touch_point(&bounds,10,20,&x,&y));
    reset(&m,&s);tap(&m,&s,100,180);assert(m.nav==VIEW_SETTINGS && !m.pending.enabled);
    tap(&m,&s,600,250);assert(m.pending.enabled && m.dirty);
    tap(&m,&s,600,330);assert(m.pending.target==AM_UX0);
    assert(tap(&m,&s,850,520)==ACTION_NONE && !m.editing);
    reset(&m,&s);tap(&m,&s,100,230);assert(m.nav==VIEW_TEST);
    assert(tap(&m,&s,AM_TEST_X+AM_TEST_W/2,AM_TEST_Y+AM_TEST_H/2)==ACTION_TEST);
    assert(tap(&m,&s,AM_SPEED_TEST_X+AM_TEST_W/2,AM_TEST_Y+AM_TEST_H/2)==ACTION_SPEED_TEST);
    m.busy=1;
    assert(tap(&m,&s,AM_SPEED_TEST_X+AM_TEST_W/2,AM_TEST_Y+AM_TEST_H/2)==ACTION_NONE);
    reset(&m,&s);tap(&m,&s,100,405);assert(m.nav==VIEW_REBOOT);
    assert(tap(&m,&s,600,240)==ACTION_SAVE_REBOOT);
    assert(tap(&m,&s,600,315)==ACTION_REBOOT);
    reset(&m,&s);tap(&m,&s,100,450);assert(m.nav==VIEW_EXIT);
    assert(tap(&m,&s,600,240)==ACTION_SAVE_EXIT);
    assert(tap(&m,&s,600,315)==ACTION_EXIT);
    reset(&m,&s);tap(&m,&s,100,450);assert(tap(&m,&s,600,395)==ACTION_NONE && m.nav==VIEW_EXIT);
    assert(tap(&m,&s,850,485)==ACTION_NONE && m.nav==VIEW_OVERVIEW);
    reset(&m,&s);m.dirty=1;assert(tap(&m,&s,850,520)==ACTION_NONE && m.dirty);
    reset(&m,&s);assert(tap(&m,&s,700,485)==ACTION_EXPORT);
    reset(&m,&s);m.busy=1;assert(tap(&m,&s,800,400)==ACTION_NONE && !m.nav);
    reset(&m,&s);am_touch_block(&s);assert(tap(&m,&s,100,450)==ACTION_NONE && !m.nav);
    tap(&m,&s,100,450);assert(m.nav==VIEW_EXIT);
    reset(&m,&s);m.nav=VIEW_SETTINGS;
    for (int i=0;i<10;++i) assert(am_touch_sample(&s,&m,1,1,600,250)==ACTION_NONE);
    assert(!m.pending.enabled);am_touch_sample(&s,&m,0,0,0,0);assert(m.pending.enabled);
    reset(&m,&s);m.nav=VIEW_SETTINGS;am_touch_sample(&s,&m,1,1,600,250);
    am_touch_sample(&s,&m,2,1,600,250);am_touch_sample(&s,&m,0,0,0,0);assert(!m.pending.enabled);
    reset(&m,&s);m.nav=VIEW_SETTINGS;am_touch_sample(&s,&m,1,1,600,250);
    am_touch_sample(&s,&m,1,2,600,250);am_touch_sample(&s,&m,0,0,0,0);assert(!m.pending.enabled);
    reset(&m,&s);m.nav=VIEW_SETTINGS;
    for (int i=0;i<52;++i) am_touch_sample(&s,&m,1,1,600,250);
    am_touch_sample(&s,&m,0,0,0,0);assert(!m.pending.enabled);
    reset(&m,&s);m.nav=VIEW_SETTINGS;am_touch_sample(&s,&m,1,1,600,250);
    am_touch_sample(&s,&m,1,1,650,250);am_touch_sample(&s,&m,0,0,0,0);assert(!m.pending.enabled);
    reset(&m,&s);m.nav=VIEW_LOG;
    for (unsigned i=0;i<5000;++i) m.log[i]=(i%50==49)?'\n':'A';
    m.log[5000]=0;
    am_touch_sample(&s,&m,1,1,700,300);am_touch_sample(&s,&m,1,1,600,300);am_touch_sample(&s,&m,0,0,0,0);
    assert(m.log_page==1);
    tap(&m,&s,320,520);assert(m.log_page==0);tap(&m,&s,420,520);assert(m.log_page==1);
    reset(&m,&s);am_touch_sample(&s,&m,1,1,100,450);am_touch_sample(&s,&m,-1,0,0,0);
    am_touch_sample(&s,&m,0,0,0,0);assert(m.nav==VIEW_OVERVIEW);
    reset(&m,&s);m.nav=VIEW_LOG;
    assert(tap(&m,&s,540,520)==ACTION_NONE && m.clear_confirm && m.clear_choice==1);
    assert(tap(&m,&s,100,450)==ACTION_NONE && m.nav==VIEW_LOG && m.clear_confirm);
    assert(tap(&m,&s,700,520)==ACTION_NONE && m.clear_confirm);
    assert(tap(&m,&s,750,395)==ACTION_NONE && !m.clear_confirm);
    reset(&m,&s);m.nav=VIEW_LOG;tap(&m,&s,540,520);
    assert(tap(&m,&s,430,395)==ACTION_CLEAR_LOG && !m.clear_confirm);
    reset(&m,&s);m.nav=VIEW_LOG;tap(&m,&s,540,520);
    assert(tap(&m,&s,850,520)==ACTION_NONE && !m.clear_confirm && m.nav==VIEW_LOG);
    reset(&m,&s);assert(tap(&m,&s,540,520)==ACTION_NONE && !m.clear_confirm);
    reset(&m,&s);tap(&m,&s,100,300);assert(m.nav==VIEW_UPDATES);
    assert(tap(&m,&s,800,145)==ACTION_CHECK_UPDATES);
    m.busy=1;assert(tap(&m,&s,800,145)==ACTION_NONE);
    reset(&m,&s);m.nav=VIEW_UPDATES;m.update_date_notice=1;
    int ok_x=AM_DATE_OK_X+AM_DATE_OK_W/2,ok_y=AM_CONFIRM_Y+AM_CONFIRM_H/2;
    am_touch_block(&s);
    assert(tap(&m,&s,ok_x,ok_y)==ACTION_NONE && m.update_date_notice);
    assert(tap(&m,&s,800,145)==ACTION_NONE && m.update_date_notice);
    assert(tap(&m,&s,100,450)==ACTION_NONE && m.nav==VIEW_UPDATES);
    assert(tap(&m,&s,ok_x,ok_y)==ACTION_NONE && !m.update_date_notice);
    assert(tap(&m,&s,800,145)==ACTION_CHECK_UPDATES);
    printf("touch: %d tap, gesture, coordinate and cancellation cases passed\n",cases);return 0;
}
