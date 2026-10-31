#include "ui.h"
void am_ui_navigate(AmUi *m,int view) {
    if (view<0 || view>=AM_NAV_COUNT) return;
    if (m->nav<VIEW_REBOOT) m->return_nav=m->nav;
    m->nav=view;m->editing=0;m->field=0;m->clear_confirm=0;m->update_confirm=0;m->message[0]=0;
}
static void back(AmUi *m) {
    if (m->nav==VIEW_REBOOT || m->nav==VIEW_EXIT) {
        m->nav=m->return_nav<VIEW_REBOOT?m->return_nav:VIEW_OVERVIEW;m->editing=0;m->field=0;
    } else if (m->editing) m->editing=0;
    else m->nav=VIEW_OVERVIEW;
}
int am_ui_input(AmUi *m,unsigned keys) {
    if (m->busy) return ACTION_NONE;
    if (m->update_confirm) {
        if (keys&AM_CIRCLE) { m->update_confirm=0;return ACTION_NONE; }
        if (keys&(AM_LEFT|AM_RIGHT|AM_UP|AM_DOWN)) { m->update_choice^=1;return ACTION_NONE; }
        if (keys&AM_CROSS) { m->update_confirm=0;return m->update_choice==0?ACTION_INSTALL_UPDATE:ACTION_NONE; }
        return ACTION_NONE;
    }
    if (m->clear_confirm) {
        if (keys&AM_CIRCLE) { m->clear_confirm=0;return ACTION_NONE; }
        if (keys&(AM_LEFT|AM_RIGHT|AM_UP|AM_DOWN)) { m->clear_choice^=1;return ACTION_NONE; }
        if (keys&AM_CROSS) {
            m->clear_confirm=0;
            return m->clear_choice==0?ACTION_CLEAR_LOG:ACTION_NONE;
        }
        return ACTION_NONE;
    }
    if (keys&AM_CIRCLE) { back(m);return ACTION_NONE; }
    if ((keys&AM_TRIANGLE) && m->nav==VIEW_LOG) {
        m->clear_confirm=1;m->clear_choice=1;m->message[0]=0;return ACTION_NONE;
    }
    if (keys&AM_SQUARE) return ACTION_EXPORT;
    if (m->editing && (m->nav==VIEW_REBOOT || m->nav==VIEW_EXIT)) {
        if (keys&(AM_UP|AM_DOWN)) { m->field^=1;return ACTION_NONE; }
        if (keys&AM_CROSS) {
            if (m->nav==VIEW_REBOOT) return m->field==0?ACTION_SAVE_REBOOT:ACTION_REBOOT;
            return m->field==0?ACTION_SAVE_EXIT:ACTION_EXIT;
        }
        return ACTION_NONE;
    }
    if (m->editing) {
        if (keys&(AM_UP|AM_DOWN)) { m->field^=1;return ACTION_NONE; }
        if (keys&(AM_LEFT|AM_RIGHT|AM_CROSS)) {
            if (m->field==0) m->pending.enabled^=1;else m->pending.target^=1;
            m->dirty=m->pending.enabled!=m->saved.enabled || m->pending.target!=m->saved.target;
            m->message[0]=0;
        }
        return ACTION_NONE;
    }
    if (keys&AM_UP) { am_ui_navigate(m,(m->nav+AM_NAV_COUNT-1)%AM_NAV_COUNT);return ACTION_NONE; }
    if (keys&AM_DOWN) { am_ui_navigate(m,(m->nav+1)%AM_NAV_COUNT);return ACTION_NONE; }
    if (m->nav==VIEW_LOG) {
        if ((keys&AM_LEFT) && m->log_page) --m->log_page;
        if ((keys&AM_RIGHT) && m->log_page+1<am_ui_log_pages(m->log)) ++m->log_page;
    }
    if (m->nav==VIEW_UPDATES) {
        if ((keys&AM_LEFT) && m->update_page) --m->update_page;
        if ((keys&AM_RIGHT) && m->update_page+1<am_ui_update_pages(m->update.notes)) ++m->update_page;
    }
    if (!(keys&AM_CROSS)) return ACTION_NONE;
    switch (m->nav) {
        case VIEW_SETTINGS:case VIEW_REBOOT:case VIEW_EXIT:m->editing=1;m->field=0;break;
        case VIEW_TEST:return ACTION_TEST;
        case VIEW_UPDATES:
            if (m->update.state==UPDATE_AVAILABLE && m->update.asset_url[0]) { m->update_confirm=1;m->update_choice=1;return ACTION_NONE; }
            return ACTION_CHECK_UPDATES;
        case VIEW_LOG:return ACTION_LOG;
    }
    return ACTION_NONE;
}
