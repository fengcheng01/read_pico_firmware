/* SPDX-License-Identifier: Apache-2.0
 * 中文：日/周阅读时长分页；读取真实统计，入口和返回属于手帐。
 * English: Paginated daily/weekly reading totals from real stats, entered and returned through Journal.
 * 冻结：render不落盘；缺卡时明确旧记录不完整，不把开机时长当阅读。
 * Frozen: Rendering never persists; missing-card history is explicitly incomplete, and uptime never counts as reading.
 */
#include "app.h"
#include "app_registry.h"
#include "book_stats.h"
#include "book_stats_archive.h"
#include "os_time.h"
#include "ui_product.h"
#include "ui_menu.h"
#include "ui_gesture.h"
#include "ui_press.h"
#include <stdio.h>
static bool s_week;
static unsigned s_page;
static uint32_t s_today,s_dates[7],s_end[7],s_minutes[7];
static int64_t s_poll_ms;
static unsigned s_media;
static ui_press_t s_press;
static EpdRect control(int id) {
    if(id==4)return ui_product_back_rect();
    return ui_row_rect(id%2,2,id<2?210:990,id<2?68:80);
}
static void reload(void) {
    s_today=0;os_time_date_key(&s_today);
    uint32_t anchor=s_today;
    if(s_week&&s_today)anchor=os_time_date_shift(s_today,-((os_time_info()->weekday+6)%7));
    for(unsigned i=0;i<7;++i){
        int offset=-(int)(s_page*7+i)*(s_week?7:1);
        s_dates[i]=anchor?os_time_date_shift(anchor,offset):0;
        s_end[i]=s_week?os_time_date_shift(s_dates[i],6):s_dates[i];
        if(s_end[i]>s_today)s_end[i]=s_today;
        if(s_dates[i]<20200101&&s_end[i]>=20200101)s_dates[i]=20200101;
        s_minutes[i]=s_today?book_stats_total_minutes(s_dates[i],s_end[i]):0;
    }
}
static void render(app_ctx_t* ctx,uint8_t* fb) {
    (void)ctx;ui_clear_page(fb);ui_product_header(fb,"阅读记录","按日 · 按周 · 长期归档");
    ui_draw_button(fb,control(4),"返回手帐",false);
    ui_draw_button(fb,control(0),"每日",!s_week);ui_draw_button(fb,control(1),"每周",s_week);
    uint32_t max=1;for(unsigned i=0;i<7;++i)if(s_minutes[i]>max)max=s_minutes[i];
    char text[96];
    for(unsigned i=0;i<7;++i){
        int y=326+(int)i*86;
        if(!s_today){if(!i)ui_text(fb,UI_MARGIN,y,28,"请先校准时间",EPD_DRAW_ALIGN_LEFT,false);break;}
        if(s_dates[i]<20200101)break;
        snprintf(text,sizeof(text),"%04u.%02u.%02u%s",(unsigned)(s_dates[i]/10000),(unsigned)(s_dates[i]/100%100),(unsigned)(s_dates[i]%100),s_week?" 起":"");
        ui_text(fb,UI_MARGIN,y,26,text,EPD_DRAW_ALIGN_LEFT,false);
        bool incomplete=!book_stats_archive_ready()||book_stats_archive_error();
        if(incomplete&&!s_minutes[i])snprintf(text,sizeof(text),"--");
        else if(s_minutes[i]>=60)snprintf(text,sizeof(text),"%s%lu小时%lu分",incomplete?"≥":"",(unsigned long)(s_minutes[i]/60),(unsigned long)(s_minutes[i]%60));
        else snprintf(text,sizeof(text),"%s%lu分钟",incomplete?"≥":"",(unsigned long)s_minutes[i]);
        ui_text(fb,UI_LOCK_WIDTH-UI_MARGIN,y,26,text,EPD_DRAW_ALIGN_RIGHT,false);
        int width=(int)((uint64_t)s_minutes[i]*ui_content_width()/max);
        if(width)epd_fill_rect((EpdRect){UI_MARGIN,y+42,width,6},UI_GRAY_BLACK,fb);
    }
    const char* note=!book_stats_archive_ready()?"无TF卡，仅显示本机保留记录；旧记录可能缺失":
        book_stats_archive_error()?"TF归档读取失败，当前统计可能不完整":"TF长期归档 · 本机保留最近30个日期记录";
    ui_product_title(fb,(EpdRect){UI_MARGIN,912,ui_content_width(),68},note,24,2);
    ui_draw_button(fb,control(2),"更早",false);ui_draw_button(fb,control(3),"较新",false);
    ui_draw_menu_handle(fb,false);
}
static int hit(uint16_t x,uint16_t y) {for(int i=0;i<5;i++)if(ui_rect_hit(control(i),x,y))return i;return -1;}
static app_redraw_t gesture(app_ctx_t* ctx,const ui_gesture_event_t* ev) {
    int id=hit(ev->x0,ev->y0);
    if(ev->type==UI_GESTURE_PRESS)return id>=0?ui_press_begin(ctx,&s_press,id,control(id),render):APP_REDRAW_NONE;
    if(ev->type!=UI_GESTURE_TAP)return ui_press_restore(ctx,&s_press,render);
    ui_press_forget(&s_press);if(id<0||id!=hit(ev->x,ev->y))return APP_REDRAW_NONE;
    if(id==4){ctx->request_app=app_by_id(OS_APP_TODAY);return APP_REDRAW_NONE;}
    if(id<2){s_week=id==1;s_page=0;}
    else if(id==2&&s_today&&s_dates[6]>20200107)++s_page;
    else if(id==3&&s_page)--s_page;
    reload();return APP_REDRAW_PAGE;
}
static void enter(app_ctx_t* ctx) {s_poll_ms=ctx->now_ms;s_media=book_stats_archive_generation();s_page=0;ui_press_reset(&s_press);reload();}
static app_redraw_t tick(app_ctx_t* ctx) {
    ui_press_settle(ctx,&s_press,ctx->now_ms);
    if(ctx->consumed)return APP_REDRAW_NONE;
    uint32_t date=0;os_time_date_key(&date);
    if(date!=s_today){s_page=0;reload();return APP_REDRAW_PAGE;}
    if(s_media!=book_stats_archive_generation()||ctx->now_ms-s_poll_ms>=60000){
        s_media=book_stats_archive_generation();s_poll_ms=ctx->now_ms;reload();return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}
const app_desc_t app_os_history={.title="阅读记录",.detail="每日 · 每周 · 长期归档",.clean_page=true,
    .on_enter=enter,.render=render,.on_gesture=gesture,.on_tick=tick};
