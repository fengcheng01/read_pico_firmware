/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 图书书架、阅读、目录与页内进度；文件解析和排版由book模块负责。
 * Book shelf, reader, TOC and progress; book modules own parsing and pagination.
 *
 * 冻结：统一手势入口并接管三键为上页/工具条/下页；工具条保留强刷，长按中键或把手打开产品导航，诊断经设置访问。屏幕翻页在抬起提交，不画按下态。
 * 晃动实验默认关，只翻下一页；离页关闭AOI2并休眠。render只绘图。
 * 预渲染回调返回前收齐，避免菜单/锁屏绕过页内TTF锁。
 * 用户修订：正文与页脚单遍更新；日间不触发周期GC16，夜间按用户选择的gc_every定期清理；日间标准定稿黑/灰，夜间和直刷保持未变像素。
 * 冻结：实机反馈原夜间只有正文黑块、周边仍白，修订为READING及其图片/工具/确认/读完覆盖层整屏黑底；目录、阅读设置和其他页面仍用日间。
 * Frozen: Hardware feedback showed a body-only black block with white surroundings; READING and its image/tool/confirmation/end overlays now invert the whole screen, while TOC, reader settings and other pages stay day-themed.
 * 冻结：先组合完整日间帧再反色一次，预绘缓存保持日间；局部编辑按真实front内存主题恢复日间再转回，不把已反色目标重复反色。
 * Frozen: Compose a complete day frame before one inversion and keep preparation caches day-themed; local edits return the actual front memory to day before restoring its target theme, without reinverting an already-inverted target.
 * 冻结：用户反馈夜间清理后仍留设置轮廓，当前夜间方案手动、布局和到期清理先物理清白再呈现真实正文；下述Crossmux对照经用户批准使用独立策略。
 * Frozen: Settings outlines remain after night cleanup on hardware; the current night profile physically clears white before manual, layout and due body cleaning; the user-approved Crossmux comparison below has an independent policy.
 * 冻结：用户反馈搜索点击迟滞，搜索入口和键盘不刷按下态；完整轻点后仅输入栏黑白直刷，停输后局部灰阶定稿，匹配仍只在应用执行。
 * Frozen: Search taps lag on hardware; search entry and keyboard omit pressed scans, complete taps update only the input in binary direct, idle settles local gray, and matching still runs only on Apply.
 * 冻结：阅读设置/工具条的清残影先退出覆盖层，再清当前正文，避免清的是菜单而非阅读页。
 * Frozen: Cleaning from reader settings/tools first closes overlays, then cleans the current body instead of the menu.
 * 冻结：0.5.19实机退化后用户明确优先无闪速度，纯文字直刷改厂家黑白DU；当前标准及所有含图章节保留灰阶，夜间周期清理由显示层按用户档位执行。
 * Frozen: After 0.5.19 regressed on-device, the user prioritizes flash-free speed; text direct uses vendor black/white DU and current standard/all illustrated chapters retain gray, with display-owned night cleanup at the user's interval.
 * 冻结：0.5.20仍有布局残影，用户批准不同视图切换及已呈现覆盖层退出正文时请求一次入口清理；普通翻页、控件和后台重绘不追加入口请求。
 * Frozen: Layout ghosts remain on 0.5.20; the user authorizes one entry cleanup for distinct views and exits from presented overlays to the body, without additional entry requests on ordinary turns, controls or background redraws.
 * 实机残影修订：撤回未经校准的单向灰阶、统一擦白及三相入口推白；日间标准恢复厂家黑/灰定稿，夜间及直刷保持未变像素。
 * Device-ghost revision: Withdraw uncalibrated monotonic gray, uniform whitening and three entry white actions; standard day restores vendor black/gray settling, while night and direct hold unchanged pixels.
 * 冻结：用户确认辅助线只在转场穿字，纯文字开启时统一正文/标题行网格与线位；标准段距不空槽，加大/标题段末空一槽，开关网格重排并保留文本锚点，关闭及图文排版不变。
 * Frozen: The user confirms guides cross text only during turns; text-only guides align body/headings and rules to one grid, with no extra standard gap and one empty slot after relaxed paragraphs/headings; grid toggles repaginate at the text anchor, leaving unguided and illustrated layout unchanged.
 * 中键按下切工具条，持续按住500ms返回产品导航（用户批准四根入口重构）。
 * 用户授权基础管理：长按书架先看完整详情，清进度与删文件分别确认；失败保留待重试记录，不自动回收其他书进度。
 * 用户修订：单本管理为书架弹窗；管理页用于批量操作。分页和排序保留勾选，筛选/应用搜索及重扫清除勾选。
 * 失败进度仅按变更路径失效；删除后的清理重试保留到本次开机结束，不随切页释放。
 * 卡失效时先保存进度并关闭阅读资源，再由主循环回退字体；禁止自动续读失效挂载。
 * 用户修订：进入书架只询问续读，确认前不打开上次图书，避免大书阻塞书架。
 * 用户批准 OS：首页明确开书请求验证文件与挂载后直接打开，首帧不显示书架；明确书架请求不弹续读，菜单书架入口也明确直达，不误回正文。
 * 用户要求产品重构：普通书架采用三行文件名书封与四根导航；批量管理仍七行，保留所有确认/保存语义。
 * 用户本轮要求封面在后台提取，tick 仅领取结果；根页面切换不等待解码。
 * 用户批准：书架 EPUB 封面按页逐张提取（TXT/无封面回退文件名排版），提取在 tick 内有界进行。
 * 用户实机反馈后图片在章节加载时有界解码并参与正文分页；坏图或超预算仍可点占位重试，render 不解码。
 * 用户修订：普通按钮局推，整页快速刷新；不得为按钮反馈扩大为整屏清理。
 * 用户批准补全产品功能：锁屏/睡眠统一保存钩子先落正文进度；阅读时长只在有效本地日期累计，空闲 5 分钟截断，未校时不归日。
 * 用户批准补全产品功能：工具条新增排版菜单（字体/行距/边距/首行缩进/段落间距/行辅助线/点击分区/晃动/夜间/自动翻页/清残影）；行距边距缩进段距变更即时重排并保持文本锚点；行辅助线为每行文字下方的实线/虚线，对齐含两端对齐，页脚可开时钟/电量；菜单调整后保持打开，返回阅读或中键退出。
 * Frozen: Use shared gestures and own previous/tools/next keys; retain toolbar full refresh and middle-key hold/handle product navigation, with diagnostics under Settings. Screen turns commit on release without pressed decoration.
 * Shake is experimental, off by default, forward only; exit disables AOI2 and sleeps it. Render only paints.
 * Join preparation before returning callbacks so menus/lock cannot race the page-local TTF lock.
 * User revision: present body/footer in one pass; day has no scheduled GC16, while night cleans at the user's gc_every interval. Standard day settles black/gray and night/direct hold unchanged pixels.
 * Middle-key press toggles tools; holding for 500ms opens product navigation, as approved in the four-root redesign.
 * User-authorized management shows full details before separate clear/delete confirmations; retain failed saves for retry without pruning other books.
 * User revision: single-book actions use a shelf dialog; full management is for batches. Paging/sorting preserve selection; filtering/applied search and rescanning clear it.
 * Invalidate failed progress only for changed paths; retain deletion cleanup retries across page exits for this boot.
 * Lost media saves progress and closes reader resources before global font fallback; never auto-resume an invalid mount.
 * User revision: offer resume on shelf entry; open the previous book only after confirmation to keep the shelf available.
 * User-approved OS: explicit home open requests open after file/mount validation before the first shelf frame; explicit shelf requests skip resume, menu shelf selections also enter the shelf directly.
 * User-requested product redesign: ordinary shelves use three typographic-cover rows and four-root navigation; batches keep seven rows and all confirmation/save semantics.
 * User-requested responsive tabs: poll a bounded background cover worker; root exits never join it, file mutation/open and sleep do. TXT/coverless books retain typographic covers.
 * Hardware feedback changed images to bounded chapter-time decoding and inline pagination; failed/over-budget images retain retry placeholders; render never decodes.
 * User-approved completion: the lock/sleep prepare hook saves reader progress first; reading time accumulates only on valid local dates, cuts off after 5 idle minutes and skips uncalibrated periods.
 * User-approved completion: the toolbar gains a typography menu (font/leading/margins/first-line indent/paragraph gap/per-line guide rule/tap zones/shake/night/auto turn/screen cleaning); leading, margin, indent and gap changes repaginate at once while keeping the anchor; guide rules run under every text line, solid or dashed; alignment adds left/center/justified and the footer status bar gains clock/battery switches; the menu stays open after changes and exits via Return or the middle key.
 * 用户实机反馈：阅读设置增加按需自动联网同步，下载先确认；书架目录与封面跨根页缓存，来源变化或移除时失效。
 * Hardware feedback: reader settings add on-demand WiFi sync with confirmed pulls; cache shelf catalogs/covers across roots until source changes or loss.
 * 用户修订：阅读内进度同步隐藏服务器与用户名，只保留操作与反馈；账号仍在系统设置配置。
 * User revision: reader progress sync hides the server and username, retaining actions and feedback; account setup remains in system settings.
 * 冻结：真实Tab/当前正文布局入口一次厂家GC16；普通导航用厂家GL16，正文翻页用日夜标准或厂家黑白DU，夜间另按用户周期清理；Crossmux对照入口见下述授权策略。
 * 修订原因：0.5.20入口残影及夜间白字累积均可由手动清理恢复，用户批准入口清理与夜间周期清理，不再使用单相入口推白。
 * 冻结：用户要求正文长按选择所在句子，确认保存真实摘录并保留章节字节锚点；目录由工具栏访问。
 * Frozen: User-requested body holds select sentences for confirmed excerpts with chapter byte anchors; the toolbar retains TOC access.
 * Frozen: Real Tab/current-body layout entries use one vendor GC16; ordinary navigation uses vendor GL16 and body turns use day/night standard or vendor black/white DU, with separate user-selected night cleanup; comparison entries follow the authorized policy below.
 * Revision: Manual cleaning restores 0.5.20 entry ghosts and accumulated white night glyphs; the user authorizes entry and periodic night cleanup instead of a single white entry action.
 * 用户批准设计稿落地：工具拆为目录/书签、添加书签、字号子面板、更多设置、清残影和书架；更多设置两组各六行，书签删除先确认，降低误触与信息密度。
 * User-approved design implementation: tools expose TOC/bookmarks, add mark, size subpanel, grouped settings, cleaning and shelf; two six-row groups and confirmed bookmark deletion reduce density and accidental loss.
 * 用户批准六项功能：目录页三页签（章节/书签/百分比跳转，书签管理或长按先确认删除）；读完面板推荐同源下一本；清残影周期改档位设置（gc_every）；异常复位日志走 os_crash；黄历锁屏在 sleep/设置侧。
 * Frozen: The TOC gains three tabs (chapters/bookmarks/percent jump, management or long-press confirms bookmark deletion); an end-of-book panel suggests same-source next books; the ghost-cleanup period becomes the gc_every setting; abnormal resets log via os_crash; the almanac lock face lives in sleep/settings.
 * 实机修订：0.5.25定向补黑和后置局部擦写未改善夜间残影，后者增加旧字闪动；撤回两实验，保留普通翻页与已有周期/手动完整清理。
 * Device revision: 0.5.25 black reinforcement and post-DU local cleaning failed to improve night ghosts, with local cleaning flashing old glyphs; withdraw both experiments and retain ordinary turns with existing interval/manual full cleaning.
 * 冻结：用户批准独立Crossmux Pico刷新对照，默认仍当前方案；对照仅夜间纯文字固定黑白，入口GL、翻页DU、周期/手动单次GC，图片/控件/日间保留原路径；不是整套Crossmux移植。
 * Frozen: The user authorizes an independent Crossmux Pico refresh comparison, retaining the current default; comparison night text is binary with entry GL, turn DU and one periodic/manual GC, while images/controls/day keep their paths; this is not a whole Crossmux transplant.
 */
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#include "app.h"
#include "app_registry.h"
#include "app_sleep_hooks.h"
#include "book_layout.h"
#include "book_cover.h"
#include "book_entry.h"
#include "book_quotes.h"
#include "book_marks.h"
#include "book_policy.h"
#include "book_progress.h"
#include "book_stats.h"
#include "book_source.h"
#include "book_store.h"
#include "display.h"
#include "display_pixels.h"
#include "e0470_epaper_waveform.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "os_time.h"
#include "os_sync.h"
#include "read_pico_init.h"
#include "read_pico_sd.h"
#include "read_pico_search.h"
#include "settings.h"
#include "ttf_font.h"
#include "ui_kit.h"
#include "ui_gesture.h"
#include "ui_menu.h"
#include "ui_product.h"

#define BOOK_ROWS UI_PRODUCT_SHELF_ROWS
#define BULK_ROWS 7
#define BOOK_TOC_ROWS 10
#define BOOK_PCT_ROWS 10
#define BOOK_PX_MIN 36
#define BOOK_PX_MAX 80
#define BOOK_PX_STEP 4
#define BOOK_SHAKE_THS_MG 192
#define BOOK_SHAKE_DUR 2
#define BOOK_SIZE_SETTLE_MS 400
#define BOOK_TOOL_COUNT 6
#define BOOK_TOOL_ROWS 2

typedef enum { SHELF, READING, TOC, LAYOUT, TAP_ZONES, MANAGE, BULK, SEARCH } book_view_t;
typedef struct {
    char name[256];
    char path[BOOK_STORE_PATH_MAX];
    uint32_t size;
    bool is_flash;
    bool has_progress;
    uint8_t pct;
    uint32_t recent;
    bool selected, removed, search_match;
} shelf_entry_t;

static const char* TAG = "book";
static book_view_t s_view;
// 仅成功推屏更新视图基准，失败后的正文入口仍需重试清理。
// Commit the view baseline only after a successful present so failed body entries retry cleanup.
static book_view_t s_presented_view;
static bool s_presented_valid;
static bool s_presented_reading_overlay;
static shelf_entry_t* s_shelf;
static size_t s_shelf_capacity;
static int s_count, s_visible_count, s_filter;
static bool s_recent_sort;
static shelf_entry_t s_managed;
static bool s_delete_confirm, s_file_removed;
static char s_shelf_warning[128], s_manage_message[128];
static char s_query[65], s_search_draft[65], s_batch_message[128];
static book_view_t s_search_parent;
static bool s_batch_confirm, s_batch_delete;
typedef struct pending_progress {
    char path[BOOK_STORE_PATH_MAX];
    book_progress_t value;
    bool dirty, progress_saved;
    book_progress_watch_t* watch;
    struct pending_progress* next;
} pending_progress_t;
static pending_progress_t* s_pending;
typedef struct delete_retry {
    shelf_entry_t entry;
    struct delete_retry* next;
} delete_retry_t;
static delete_retry_t* s_delete_retries;
static char s_latest_path[BOOK_STORE_PATH_MAX];
static book_entry_request_t s_entry;
static bool s_entry_active;
static bool s_quote_selecting;
static book_quote_t s_quote;
static char s_quote_notice[96];
static bool s_save_failed;
static bool s_pending_invalidated;
static int64_t s_save_retry_ms;
static unsigned s_store_revision;
static bool s_scan_pending, s_resume_pending, s_toolbar, s_clear_confirm;
static char s_resume_path[BOOK_STORE_PATH_MAX], s_resume_name[256];
static char s_message[128], s_storage[128], s_path[BOOK_STORE_PATH_MAX], s_title[128];
static char s_book_title[256];
static char s_font_path[192];
static bool s_font_notice;
// front内存主题独立于光学提交状态；预绘始终日间，不改变这两项。
// Front memory theme is independent of optical commit status; day preparation never changes these flags.
static bool s_reader_target_night, s_reader_target_binary;
static char* s_text;
static blk_t* s_blocks;
static size_t s_block_count;
static size_t s_text_len, s_chapter, s_page;
static bool s_image_open;
static uint8_t* s_image_pixels;
static uint16_t s_image_width, s_image_height;
static size_t s_image_block = SIZE_MAX;
static const char* s_image_error;
static char s_image_origin[224];
static uint32_t s_file_size;
static int s_px, s_unsaved;
static int64_t s_poll_ms, s_last_turn_ms, s_size_settle_ms, s_sensor_ms;
static bool s_shake_enabled, s_sensor_on;
static bool s_sensor_saved;
static sc7a20h_sensor_config_t s_sensor_config;
static book_shake_gate_t s_shake;
static EpdRect s_area;
static enum EpdDrawMode s_mode = MODE_GL16;
// 正文翻页请求文字转页表；封面行等局部区域推送用普通差分。
// Body turns request the text-turn table; partial pushes such as cover rows use the ordinary diff.
static bool s_text_turn;
static int s_pressed_control = -1;
static int64_t s_du_ms;
static unsigned s_du_count;
static EpdRect s_du_area;
static bool s_search_dirty, s_search_fast;
static int64_t s_search_edit_ms;
static SemaphoreHandle_t s_draw_lock, s_prep_done;
static TaskHandle_t s_prep_task;
static uint8_t* s_next_fb;
static int s_next_page = -1, s_prep_page = -1;
// 目录页三页签：0 章节 1 书签 2 百分比跳转。/ TOC tabs: 0 chapters, 1 bookmarks, 2 percent jump.
static uint8_t s_toc_tab;
static int s_mark_delete = -1;
static bool s_mark_manage, s_toolbar_sizes;
static uint8_t s_layout_group;
static char s_sync_note[128];
static bool s_catalog_valid;
static unsigned s_catalog_revision;
static int s_shelf_leaf;
static EpdRect mark_manage_rect(void) { return ui_product_back_rect(); }
static EpdRect mark_confirm_rect(int i) { return ui_row_rect(i, 2, 620, UI_BTN_H); }
static book_mark_t s_marks[BOOK_MARKS_MAX];
static size_t s_mark_count;
// 读完面板与同源下一本候选。/ End-of-book panel and same-source next candidates.
static bool s_ended;
typedef struct {
    char name[128];
    char path[BOOK_STORE_PATH_MAX];
} next_book_t;
static next_book_t s_next_books[3];
static int s_next_count;

static void render(app_ctx_t* ctx, uint8_t* fb);
static void draw_layout_menu(uint8_t* fb);
static void draw_tap_zones(uint8_t* fb);
static app_redraw_t jump_to_bytes(app_ctx_t* ctx, uint32_t target);
static void scan_shelf(app_ctx_t* ctx);
static void free_book(void);
static void save_progress(void);
static void apply_entry(app_ctx_t* ctx);
static void invalidate_prep(void);
static void sort_shelf(app_ctx_t* ctx);
static void draw_control(uint8_t* fb, EpdRect rect, const char* label, int id) {
    if (id == 112 && s_query[0]) {
        ui_fill_round_rect(fb, rect, UI_BTN_RADIUS, UI_GRAY_BLACK);
        ui_text_vc(fb, rect.x + rect.width / 2, rect.y + rect.height / 2,
                   UI_PX_BTN, label, EPD_DRAW_ALIGN_CENTER, true);
        return;
    }
    if (s_pressed_control == id) ui_draw_pressed_round_rect(fb, rect, UI_BTN_RADIUS);
    ui_draw_button(fb, rect, label, false);
}
static void lock_draw(void) { if (s_draw_lock) xSemaphoreTake(s_draw_lock, portMAX_DELAY); }
static void unlock_draw(void) { if (s_draw_lock) xSemaphoreGive(s_draw_lock); }
static size_t fb_bytes(void) { return (size_t)epd_width() * epd_height() / 2; }
typedef struct { bool night, binary; } reader_patch_t;
static reader_patch_t reader_patch_begin(app_ctx_t* ctx) {
    reader_patch_t patch = {s_reader_target_night, s_reader_target_binary};
    if (patch.night) display_invert_frame(ctx->fb, epd_width(), epd_height());
    return patch;
}
static void reader_patch_end(app_ctx_t* ctx, reader_patch_t patch) {
    if (patch.night) display_invert_frame(ctx->fb, epd_width(), epd_height());
    if (patch.binary) display_prepare_direct_frame(ctx->fb, epd_width(), epd_height(), patch.night);
}
static void finish_reader_frame(app_ctx_t* ctx, uint8_t* fb) {
    bool night = app_settings_book_night() && (s_image_open || (s_view == READING && s_text));
    if (night) display_invert_frame(fb, epd_width(), epd_height());
    if (fb == ctx->fb) { s_reader_target_night = night; s_reader_target_binary = false; }
}
// 含已解码插图的章节保留灰阶，直刷仅处理纯文字实际黑白目标。
// Chapters with decoded illustrations retain gray; direct uses actual black/white targets for text only.
static bool reader_crossmux_enabled(void) {
    if (!app_settings_book_night() || app_settings_book_night_profile() != BOOK_NIGHT_PROFILE_CROSSMUX) return false;
    for (size_t i = 0; i < s_block_count; ++i) if (s_blocks[i].image) return false;
    return true;
}
static bool reader_direct_enabled(void) {
    if (!app_settings_book_direct() && !reader_crossmux_enabled()) return false;
    for (size_t i = 0; i < s_block_count; ++i) if (s_blocks[i].image) return false;
    return true;
}
static EpdRect body_rect(void) {
    // 边距档每级 16px，只收窄正文，不动页脚与工具条。/ Margin tiers inset 16 px each, narrowing only the body.
    EpdRect body = ui_product_reader_body(s_font_notice);
    int inset = (int)app_settings_book_margin() * 16;
    body.x += inset; body.width -= 2 * inset;
    return body;
}
// 在build/draw前同步排版；辅助线开关需重分页，颜色随实际直刷模式保持可见。
// Sync typography before builds/draws; guide activation repaginates and its color stays visible in the effective direct profile.
static void apply_typography(void) {
    book_layout_set_leading(app_settings_book_leading());
    // 日间规范帧包含正文、图片和线；完整组合完成后才统一转换主题。
    // The day canonical frame includes text, images and rules; convert theme only after full composition.
    book_layout_set_night(false);
    book_layout_set_indent(app_settings_book_indent());
    book_layout_set_paragraph(app_settings_book_para());
    book_layout_set_guide(app_settings_book_guide());
    book_layout_set_guide_origin(ui_product_reader_body(false).y);
    book_layout_set_guide_contrast(reader_direct_enabled());
    book_layout_set_align(app_settings_book_align());
}
// 页脚状态串：时钟/电量按设置拼装；数据来自缓存快照，render 保持纯绘图。
// Footer status cluster: clock/battery per settings from cached snapshots; render stays pure.
static void footer_status(char* out, size_t cap) {
    if (!out || !cap) return;
    out[0] = 0;
    char clock[16];
    if (app_settings_footer_clock()) {
        os_time_format_clock(clock, sizeof(clock));
        snprintf(out, cap, "%s", clock);
    }
    if (app_settings_footer_battery()) {
        int permille = os_time_battery_permille();
        if (permille >= 0) {
            size_t used = strlen(out);
            snprintf(out + used, cap - used, "%s%d%%", used ? " · " : "", permille / 10);
        }
    }
}
// 目录页顶部三页签占 184..264，列表从 264 起。/ The three TOC tabs own 184..264; rows start at 264.
static EpdRect toc_tab_rect(int i) { return ui_row_rect(i, 3, 184, 80); }
static EpdRect row_rect(int row, bool toc) {
    if (!toc && s_view != BULK) return ui_product_shelf_rect(row);
    int h = toc ? (UI_CONTENT_BOTTOM - 264) / BOOK_TOC_ROWS : UI_BTN_H + UI_GAP;
    return (EpdRect){UI_MARGIN, (toc ? 264 : 308) + row * h, ui_content_width(), toc ? h - 8 : UI_BTN_H};
}
// 页签下当前列表的行 id：章节行沿用行号，书签/百分比行用独立段。/ Row ids per tab: chapters keep row ids; bookmarks/percent use their own ranges.
static int toc_row_id(uint8_t tab, int row) {
    if (tab == 1) return row ? 930 + row : 910;
    if (tab == 2) return 940 + row;
    return row;
}
static EpdRect tool_rect(int i) {
    return ui_product_tool_rect(i);
}
static int view_rows(void) { return s_view == TOC ? BOOK_TOC_ROWS : s_view == BULK ? BULK_ROWS : BOOK_ROWS; }
static EpdRect nav_rect(int index) { return s_view == SHELF || s_view == MANAGE ? ui_product_shelf_nav_rect(index) : ui_bar_rect(index, 3); }
// 页签下当前列表行数：书签首行为“加书签”。/ Rows per tab; the bookmark tab leads with the add row.
static int toc_row_count(void) {
    if (s_view != TOC) return s_visible_count;
    if (s_toc_tab == 1) return (int)s_mark_count + 1;
    if (s_toc_tab == 2) return BOOK_PCT_ROWS;
    return (int)book_chapter_count();
}
static int leaves(void) {
    int count = s_view == TOC ? toc_row_count() : s_visible_count;
    int rows = view_rows();
    return count ? 1 + (count - 1) / rows : 1;
}

// 截断必须停在UTF8字符边界。/ Truncation must stop at a UTF8 character boundary.
static void copy_text(char* dst, size_t cap, const char* src) {
    if (!cap) return;
    size_t n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
        while (n && ((unsigned char)src[n] & 0xc0) == 0x80) --n;
    }
    memcpy(dst, src, n);
    dst[n] = 0;
}
static void fit_text(char* text, int px, int width) {
    while (*text && ttf_text_width_px(px, text) > width) {
        size_t n = strlen(text) - 1;
        while (n && ((unsigned char)text[n] & 0xc0) == 0x80) --n;
        text[n] = 0;
    }
}
static uint32_t chapter_end(void) {
    return s_chapter + 1 < book_chapter_count()
        ? book_chapter_byte_offset(s_chapter + 1) : book_total_bytes();
}
static unsigned percent(size_t page) {
    uint32_t total = book_total_bytes();
    if (!total) return 0;
    if (book_layout_complete() && s_chapter + 1 == book_chapter_count() && page + 1 == book_layout_page_count()) return 100;
    uint32_t off = book_position_bytes(book_chapter_byte_offset(s_chapter), chapter_end(),
                                       book_layout_page_start_offset(page), s_text_len);
    return (unsigned)((uint64_t)off * 100 / total);
}
// 重载当前书的书签；无书或无记录时清零。/ Reload the current book's bookmarks; zero without a book or record.
static void marks_reload(void) {
    s_mark_count = 0;
    if (s_path[0]) (void)book_marks_list(s_path, s_marks, BOOK_MARKS_MAX, &s_mark_count);
}
static pending_progress_t* pending_find(const char* path) {
    for (pending_progress_t* p = s_pending; p; p = p->next) if (!strcmp(p->path, path)) return p;
    return NULL;
}
static int layout_name(uint8_t* fb, const char* name, int y, bool draw);
static EpdRect manage_panel(void) {
    int height = layout_name(NULL, s_managed.name, 0, false) + 420;
    return (EpdRect){UI_MARGIN - 16, 190 + (876 - height) / 2, ui_content_width() + 32, height};
}
static EpdRect manage_rect(int index, int count) {
    EpdRect panel = manage_panel();
    return ui_row_rect(index, count, panel.y + panel.height - 94, 76);
}
static EpdRect batch_rect(int id) {
    if (id < 3) return ui_row_rect(id, 3, 978, 48);
    return ui_row_rect(id - 3, 2, 1038, 48);
}
static EpdRect search_rect(int id) {
    if (id < 40) {
        int width = (ui_content_width() - 54) / 10;
        return (EpdRect){UI_MARGIN + id % 10 * (width + 6), 388 + id / 10 * 100, width, 88};
    }
    if (id < 43) return ui_row_rect(id - 40, 3, 808, 80);
    return ui_bar_rect(id - 43, 2);
}
static EpdRect search_input_rect(void) { return (EpdRect){UI_MARGIN, 190, ui_content_width(), 168}; }
static void draw_search_input(uint8_t* fb);
static size_t selected_count(void) {
    size_t selected = 0;
    for (int i = 0; i < s_count; ++i) if (s_shelf[i].selected) ++selected;
    return selected;
}
static void clear_selection(void) {
    for (int i = 0; i < s_count; ++i) s_shelf[i].selected = false;
}
static void toggle_selection(int index) {
    if (index >= 0 && index < s_visible_count) s_shelf[index].selected = !s_shelf[index].selected;
}
static void select_page(int page) {
    for (int i = page * BULK_ROWS; i < s_visible_count && i < (page + 1) * BULK_ROWS; ++i) s_shelf[i].selected = true;
}
static const char* search_keys(void) {
    return "1234567890" "qwertyuiop" "asdfghjkl-" "zxcvbnm._'";
}
static void search_begin(void) {
    s_search_parent = s_view;
    memcpy(s_search_draft, s_query, sizeof(s_query));
    s_view = SEARCH;
    s_search_dirty = s_search_fast = false;
}
static void refresh_search_matches(void) {
    for (int i = 0; i < s_count; ++i)
        s_shelf[i].search_match = read_pico_search_match(s_shelf[i].name, s_query);
}
static void search_finish(app_ctx_t* ctx, bool apply) {
    s_search_dirty = s_search_fast = false;
    s_view = s_search_parent;
    if (apply) {
        memcpy(s_query, s_search_draft, sizeof(s_query));
        refresh_search_matches();
        clear_selection();
        sort_shelf(ctx);
        s_batch_message[0] = 0;
    }
    memset(s_search_draft, 0, sizeof(s_search_draft));
}
static app_redraw_t search_action(app_ctx_t* ctx, int id) {
    size_t len = strlen(s_search_draft);
    if ((id >= 0 && id < 40) || id == 40) {
        if (len < sizeof(s_search_draft) - 1) {
            s_search_draft[len] = id == 40 ? ' ' : search_keys()[id];
            s_search_draft[len + 1] = 0;
        }
    } else if (id == 41 && len) s_search_draft[len - 1] = 0;
    else if (id == 42) s_search_draft[0] = 0;
    else if (id == 43 || id == 44) { search_finish(ctx, id == 44); return APP_REDRAW_PAGE; }
    return len == strlen(s_search_draft) ? APP_REDRAW_NONE : APP_REDRAW_AREA;
}
static bool pending_reserve(const char* path) {
    if (pending_find(path)) return true;
    pending_progress_t* p = heap_caps_malloc(sizeof(*p), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(sizeof(*p));
    if (!p) return false;
    memset(p, 0, sizeof(*p));
    copy_text(p->path, sizeof(p->path), path);
    // 阅读页内注册；传书 HTTP 尚未启动，退出后也不注销失败项。
    // Register in reading before transfer HTTP starts; failed records survive page exit.
    p->watch = book_progress_watch_create(path);
    if (!p->watch) { free(p); return false; }
    pending_progress_t** tail = &s_pending;
    while (*tail) tail = &(*tail)->next;
    *tail = p;
    return true;
}
static bool pending_restore(const char* path, uint32_t size, book_progress_t* out) {
    pending_progress_t* pending = pending_find(path);
    if (!pending || !pending->dirty || pending->value.file_size != size) return false;
    *out = pending->value;
    return true;
}
static void pending_discard(const char* path) {
    pending_progress_t** p = &s_pending;
    while (*p) {
        if (!strcmp((*p)->path, path)) {
            pending_progress_t* old = *p;
            *p = old->next;
            book_progress_watch_destroy(old->watch);
            free(old);
            break;
        }
        p = &(*p)->next;
    }
    s_save_failed = false;
    for (pending_progress_t* item = s_pending; item; item = item->next) if (item->dirty) s_save_failed = true;
}
static void pending_mark_latest(const char* path) {
    pending_progress_t** item = &s_pending;
    while (*item && strcmp((*item)->path, path)) item = &(*item)->next;
    if (*item) {
        pending_progress_t* current = *item;
        *item = current->next;
        current->next = NULL;
        pending_progress_t** tail = &s_pending;
        while (*tail) tail = &(*tail)->next;
        *tail = current;
    }
    copy_text(s_latest_path, sizeof(s_latest_path), path);
}
static void pending_drop_invalidated(void) {
    pending_progress_t* p = s_pending;
    while (p) {
        pending_progress_t* next = p->next;
        if (book_progress_watch_invalidated(p->watch)) {
            if (p->dirty) s_pending_invalidated = true;
            pending_discard(p->path);
        }
        p = next;
    }
}
static delete_retry_t* delete_retry_find(const char* path) {
    for (delete_retry_t* p = s_delete_retries; p; p = p->next)
        if (!strcmp(p->entry.path, path)) return p;
    return NULL;
}
static bool pending_flush(pending_progress_t* p) {
    if (!p->dirty) return true;
    if (!p->progress_saved) {
        if (book_progress_save(p->path, &p->value) != ESP_OK) return false;
        p->progress_saved = true;
    }
    if (!strcmp(p->path, s_latest_path) && book_progress_set_last_path(p->path) != ESP_OK) return false;
    p->dirty = false;
    return true;
}
static void retry_progress(void) {
    s_save_failed = false;
    bool had_dirty = false;
    pending_progress_t* p = s_pending;
    while (p) {
        pending_progress_t* next = p->next;
        bool dirty = p->dirty;
        had_dirty |= dirty;
        if (s_text && !strcmp(p->path, s_path)) { p = next; continue; }
        if (!pending_flush(p)) s_save_failed = true;
        else if (strcmp(p->path, s_path)) pending_discard(p->path);
        else if (dirty) s_unsaved = 0;
        p = next;
    }
    if (s_text && (s_unsaved || had_dirty)) save_progress();
    s_save_failed = false;
    for (pending_progress_t* item = s_pending; item; item = item->next) if (item->dirty) s_save_failed = true;
}

/* ---- 管理详情 / Management details ---- */
static int layout_name(uint8_t* fb, const char* name, int y, bool draw) {
    const char* at = name;
    while (*at) {
        char line[256];
        size_t used = 0;
        while (at[used]) {
            unsigned char first = (unsigned char)at[used];
            size_t n = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
            size_t remain = strlen(at + used);
            if (n > remain) n = 1;
            if (used + n >= sizeof(line)) break;
            memcpy(line + used, at + used, n);
            line[used + n] = 0;
            if (ttf_text_width_px(UI_PX_CAPTION, line) > ui_content_width() && used) break;
            used += n;
        }
        if (!used) break;
        line[used] = 0;
        if (draw) ui_text(fb, UI_MARGIN, y, UI_PX_CAPTION, line, EPD_DRAW_ALIGN_LEFT, false);
        y += 36;
        at += used;
    }
    return y;
}
static int draw_wrapped_name(uint8_t* fb, const char* name, int y) {
    return layout_name(fb, name, y, true);
}
static void draw_manage(uint8_t* fb) {
    EpdRect panel = manage_panel();
    ui_fill_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_WHITE);
    ui_draw_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN, panel.y + 18, UI_PX_BODY,
            s_clear_confirm ? (s_delete_confirm ? "确认删除文件？" : "确认清除进度？") : "图书详情", EPD_DRAW_ALIGN_LEFT, false);
    int y = draw_wrapped_name(fb, s_managed.name, panel.y + 74) + 12;
    char info[96];
    snprintf(info, sizeof(info), "%s · %s · %.2f MB", s_managed.is_flash ? "内置存储" : "TF 卡",
             strrchr(s_managed.name, '.') ? strrchr(s_managed.name, '.') + 1 : "", s_managed.size / 1048576.0);
    ui_text(fb, UI_MARGIN, y, UI_PX_CAPTION, info, EPD_DRAW_ALIGN_LEFT, false);
    snprintf(info, sizeof(info), s_managed.has_progress ? "阅读进度 %u%%" : "尚无阅读进度", s_managed.pct);
    ui_text(fb, UI_MARGIN, y + 44, UI_PX_CAPTION, info, EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, UI_MARGIN, y + 88, UI_PX_CAPTION, s_managed.is_flash ? "/flash/books" : "/sdcard/books", EPD_DRAW_ALIGN_LEFT, false);
    if (s_manage_message[0]) ui_text(fb, UI_MARGIN, panel.y + panel.height - 198, UI_PX_CAPTION, s_manage_message, EPD_DRAW_ALIGN_LEFT, false);
    if (s_clear_confirm) {
        ui_text(fb, UI_MARGIN, panel.y + panel.height - 142, UI_PX_CAPTION, s_delete_confirm ? "删除后文件无法恢复" : "仅清阅读进度，保留图书文件", EPD_DRAW_ALIGN_LEFT, false);
        draw_control(fb, manage_rect(0, 2), "取消", 300);
        draw_control(fb, manage_rect(1, 2), s_delete_confirm ? "确认删除" : "确认清除", 301);
    } else if (s_file_removed) {
        draw_control(fb, manage_rect(0, 2), "关闭", 400);
        draw_control(fb, manage_rect(1, 2), "重试清理", 403);
    } else {
        draw_control(fb, manage_rect(0, 3), "关闭", 400);
        draw_control(fb, manage_rect(1, 3), "清进度", 401);
        draw_control(fb, manage_rect(2, 3), "删除文件", 402);
    }
}
static void draw_search_input(uint8_t* fb) {
    ui_clear_rect_fast(fb, search_input_rect());
    EpdRect field = {UI_MARGIN, 200, ui_content_width(), 88};
    ui_draw_round_rect(fb, field, UI_BTN_RADIUS, UI_GRAY_BLACK);
    const char* tail = s_search_draft;
    while (*tail && ttf_text_width_px(UI_PX_BODY, tail) > field.width - 2 * UI_PAD) ++tail;
    ui_text_vc(fb, field.x + UI_PAD, field.y + field.height / 2, UI_PX_BODY, tail, EPD_DRAW_ALIGN_LEFT, false);
    char count[48];
    snprintf(count, sizeof(count), "%u/64 · 清空后应用可显示全部", (unsigned)strlen(s_search_draft));
    ui_text(fb, UI_MARGIN, 310, UI_PX_CAPTION, count, EPD_DRAW_ALIGN_LEFT, false);
}
static void draw_search(uint8_t* fb) {
    ui_clear_page(fb);
    ui_draw_header(fb, "搜索图书", "输入拼音首字母、完整拼音或英文");
    draw_search_input(fb);
    const char* keys = search_keys();
    for (int i = 0; i < 40; ++i) {
        char label[2] = {keys[i], 0};
        draw_control(fb, search_rect(i), label, 500 + i);
    }
    draw_control(fb, search_rect(40), "空格", 540);
    draw_control(fb, search_rect(41), "退格", 541);
    draw_control(fb, search_rect(42), "清空", 542);
    ui_text(fb, UI_MARGIN, 938, UI_PX_CAPTION, "应用清除勾选 · KEY1 取消", EPD_DRAW_ALIGN_LEFT, false);
    draw_control(fb, search_rect(43), "取消", 543);
    draw_control(fb, search_rect(44), "应用", 544);
    ui_draw_menu_handle(fb, false);
}
static void draw_batch_confirmation(uint8_t* fb) {
    if (!s_batch_confirm) return;
    EpdRect panel = {UI_MARGIN - 12, 410, ui_content_width() + 24, 330};
    ui_fill_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_WHITE);
    ui_draw_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_BLACK);
    char title[96];
    snprintf(title, sizeof(title), "%s %u 本图书？", s_batch_delete ? "删除" : "清除进度：", (unsigned)selected_count());
    ui_text(fb, UI_MARGIN, 438, UI_PX_BODY, title, EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, UI_MARGIN, 508, UI_PX_CAPTION, s_batch_delete ? "删除文件不可撤销，失败项可重试" : "仅清阅读进度，所有文件保留", EPD_DRAW_ALIGN_LEFT, false);
    draw_control(fb, ui_row_rect(0, 2, 620, UI_BTN_H), "取消", 600);
    draw_control(fb, ui_row_rect(1, 2, 620, UI_BTN_H), "确认", 601);
}
static void save_progress(void) {
    if (!s_text || !s_path[0] || !book_layout_page_count()) return;
    bool was_failed = s_save_failed;
    pending_progress_t* pending = pending_find(s_path);
    if (!pending) { s_save_failed = true; return; }
    book_progress_t p = {
        .file_size = s_file_size, .chapter = (uint16_t)s_chapter,
        .byte_off = (uint32_t)book_layout_page_start_offset(s_page),
        .px = (uint8_t)s_px, .pct = (uint8_t)percent(s_page),
        .last_open_s = 0,
    };
    if (!pending->dirty || pending->value.file_size != p.file_size || pending->value.chapter != p.chapter ||
        pending->value.byte_off != p.byte_off || pending->value.px != p.px || pending->value.pct != p.pct) {
        pending->value = p;
        pending->progress_saved = false;
    }
    pending->dirty = true;
    if (pending_flush(pending)) {
        s_unsaved = 0;
        s_save_failed = false;
        for (pending_progress_t* item = s_pending; item; item = item->next) if (item->dirty) s_save_failed = true;
    }
    else { if (!s_unsaved) s_unsaved = 1; s_save_failed = true; }
    if (was_failed != s_save_failed) invalidate_prep();
}
static void invalidate_prep(void) { s_next_page = s_prep_page = -1; }
static void free_image(void) {
    free(s_image_pixels); s_image_pixels = NULL;
    s_image_width = s_image_height = 0;
    s_image_block = SIZE_MAX; s_image_open = false;
    s_image_error = NULL; s_image_origin[0] = 0;
}
static app_redraw_t close_image(void) {
    s_image_open = false;
    invalidate_prep();
    return APP_REDRAW_PAGE;
}

/* ---- 绘制与预渲染 / Drawing and preparation ---- */
static char s_footer_status[48];
static void draw_reader_footer(uint8_t* fb, size_t page) {
    char status[48];
    footer_status(status, sizeof(status));
    ui_product_reader_chrome(fb, s_save_failed ? "进度未保存，稍后重试" : s_title,
                             (unsigned)page + 1, book_layout_complete() ? (unsigned)book_layout_page_count() : 0,
                             percent(page), s_font_notice, status, app_settings_footer_bar());
}
static void draw_reader(uint8_t* fb, size_t page) {
    ui_clear_page(fb);
    apply_typography();
    book_layout_draw_page(fb, page, body_rect(), s_px);
    draw_reader_footer(fb, page);
    if (s_toolbar) {
        if (s_toolbar_sizes) ui_product_reader_sizes(fb, s_title, s_px, s_pressed_control);
        else ui_product_reader_tools(fb, s_title, s_px, app_settings_book_night(), s_pressed_control);
        if (s_message[0]) {
            EpdRect notice = {UI_MARGIN, 864, ui_content_width(), 32};
            ui_clear_rect_fast(fb, notice);
            ui_product_title(fb, notice, s_message, 24, 1);
        }
    }
    ui_draw_menu_handle(fb, false);
}
static void prep_task(void* arg) {
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        lock_draw();
        if (s_prep_page >= 0 && s_text && s_next_fb) {
            draw_reader(s_next_fb, (size_t)s_prep_page);
            s_next_page = s_prep_page;
        }
        unlock_draw();
        xSemaphoreGive(s_prep_done);
    }
}
static void ensure_prep(void) {
    if (!s_draw_lock) s_draw_lock = xSemaphoreCreateMutex();
    if (!s_prep_done) s_prep_done = xSemaphoreCreateBinary();
    if (!s_next_fb) s_next_fb = heap_caps_aligned_alloc(16, fb_bytes(), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_prep_task && s_draw_lock && s_prep_done && s_next_fb) {
        if (xTaskCreatePinnedToCore(prep_task, "book_prep", 12 * 1024, NULL, 3, &s_prep_task, 1) != pdPASS)
            s_prep_task = NULL;
    }
}
static bool kick_prep(void) {
    if (!s_prep_task || s_view != READING || s_image_open || s_toolbar || s_clear_confirm ||
        s_page + 1 >= book_layout_page_count() || s_next_page == (int)s_page + 1) return false;
    s_prep_page = (int)s_page + 1;
    xSemaphoreTake(s_prep_done, 0);
    xTaskNotifyGive(s_prep_task);
    return true;
}
static void draw_image(uint8_t* fb) {
    ui_clear_page(fb);
    bool title_image = s_image_block < s_block_count && s_blocks[s_image_block].image_title;
    ui_draw_header(fb, s_image_error ? "图片未加载" : title_image ? "标题图预览" : "图片预览", "点击图片或返回按钮回到正文");
    int top = UI_CONTENT_TOP;
    if (s_image_origin[0]) {
        char line[sizeof(s_image_origin)]; copy_text(line, sizeof(line), s_image_origin);
        fit_text(line, UI_PX_CAPTION, ui_content_width());
        ui_text(fb, UI_MARGIN, top, UI_PX_CAPTION, line, EPD_DRAW_ALIGN_LEFT, false);
        top += UI_PX_CAPTION + UI_GAP;
    }
    EpdRect area = {UI_MARGIN, top, ui_content_width(), UI_CONTENT_BOTTOM - top};
    if (s_image_pixels && s_image_width && s_image_height) {
        int width = s_image_width, height = s_image_height;
        if (width > area.width) { height = height * area.width / width; width = area.width; }
        if (height > area.height) { width = width * area.height / height; height = area.height; }
        if (width < 1) width = 1;
        if (height < 1) height = 1;
        int left = area.x + (area.width - width) / 2, y0 = area.y + (area.height - height) / 2;
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            size_t src = (size_t)((int64_t)y * s_image_height / height) * s_image_width +
                         (size_t)((int64_t)x * s_image_width / width);
            epd_draw_pixel(left + x, y0 + y, s_image_pixels[src], fb);
        }
    } else ui_text(fb, area.x, area.y + area.height / 2, UI_PX_CAPTION,
                   s_image_error ? s_image_error : "图片不可用", EPD_DRAW_ALIGN_LEFT, false);
    ui_draw_button(fb, ui_bar_rect(0, 1), "返回正文", false);
    ui_draw_menu_handle(fb, false);
}
/* ---- 书架封面缓存 / Shelf cover cache ---- */
// 最多 4 槽（当前页 3 卡 + 余量）；重扫/离页释放，逐 tick 提取一张，失败也记槽避免反复重试。
// At most four slots (three visible cards plus headroom); freed on rescan/exit,
// one extraction per tick, failures recorded too so they never retry in a loop.
typedef struct {
    char path[BOOK_STORE_PATH_MAX];
    uint8_t* gray;
    bool failed;
} cover_slot_t;
static cover_slot_t s_cover_slots[4];
static void covers_reset(void) {
    for (unsigned i = 0; i < sizeof(s_cover_slots) / sizeof(s_cover_slots[0]); ++i) {
        free(s_cover_slots[i].gray);
        s_cover_slots[i].gray = NULL;
        s_cover_slots[i].path[0] = 0;
        s_cover_slots[i].failed = false;
    }
}
static const uint8_t* cover_for(const char* path) {
    for (unsigned i = 0; i < sizeof(s_cover_slots) / sizeof(s_cover_slots[0]); ++i)
        if (!strcmp(s_cover_slots[i].path, path)) return s_cover_slots[i].gray;
    return NULL;
}
static bool cover_tried(const char* path) {
    for (unsigned i = 0; i < sizeof(s_cover_slots) / sizeof(s_cover_slots[0]); ++i)
        if (!strcmp(s_cover_slots[i].path, path)) return true;
    return false;
}
static bool cover_visible(const char* path, int leaf) {
    for (int row = 0; row < BOOK_ROWS; ++row) {
        int i = leaf * BOOK_ROWS + row;
        if (i < s_visible_count && !strcmp(s_shelf[i].path, path)) return true;
    }
    return false;
}
static void cover_store(const char* path, uint8_t* gray, int leaf) {
    cover_slot_t* slot = NULL;
    for (unsigned i = 0; i < sizeof(s_cover_slots) / sizeof(s_cover_slots[0]); ++i) {
        if (!s_cover_slots[i].path[0]) { slot = &s_cover_slots[i]; break; }
        if (!strcmp(s_cover_slots[i].path, path)) { slot = &s_cover_slots[i]; break; }
        if (!cover_visible(s_cover_slots[i].path, leaf)) slot = &s_cover_slots[i];
    }
    if (!slot) { free(gray); return; }
    free(slot->gray);
    snprintf(slot->path, sizeof(slot->path), "%s", path);
    slot->gray = gray;
    slot->failed = gray == NULL;
}
static bool is_epub_entry(const shelf_entry_t* entry) {
    const char* dot = strrchr(entry->name, '.');
    return dot && !strcasecmp(dot, ".epub");
}

static void draw_product_shelf(app_ctx_t* ctx, uint8_t* fb) {
    ui_clear_page(fb);
    char subtitle[96];
    snprintf(subtitle, sizeof(subtitle), "%d 本图书 · %d / %d 页", s_visible_count, ctx->leaf + 1, leaves());
    ui_product_header(fb, "书架", subtitle);
    draw_control(fb, (EpdRect){470, 68, 174, 68}, "导入", 114);
    draw_control(fb, ui_row_rect(0, 3, 176, 72), s_filter == 0 ? "全部来源" : s_filter == 1 ? "TF 卡" : "内置", 110);
    draw_control(fb, ui_row_rect(1, 3, 176, 72), s_recent_sort ? "按最近" : "按名称", 111);
    draw_control(fb, ui_row_rect(2, 3, 176, 72), s_query[0] ? "搜索中" : "搜索", 112);
    const char* notice = s_save_failed ? "进度未保存：点此重试" : s_message[0] ? s_message : s_shelf_warning;
    ui_product_title(fb, (EpdRect){UI_MARGIN, 264, ui_content_width(), 34}, notice, 26, 1);
    for (int row = 0; row < BOOK_ROWS; ++row) {
        int i = ctx->leaf * BOOK_ROWS + row;
        if (i >= s_visible_count) break;
        char title[256], meta[96]; copy_text(title, sizeof(title), s_shelf[i].name);
        char* dot = strrchr(title, '.'); if (dot) *dot = 0;
        const char* source = s_shelf[i].is_flash ? "内置" : "TF 卡";
        if (s_shelf[i].removed) snprintf(meta, sizeof(meta), "已删除 · 进度待清理");
        else if (s_shelf[i].has_progress) snprintf(meta, sizeof(meta), "%s · 已读 %u%%", source, s_shelf[i].pct);
        else snprintf(meta, sizeof(meta), "%s · 未开始", source);
        ui_product_shelf_card(fb, ui_product_shelf_rect(row), title, meta, s_shelf[i].pct,
                              s_shelf[i].has_progress, s_pressed_control == row,
                              s_shelf[i].removed ? NULL : cover_for(s_shelf[i].path));
    }
    if (!s_visible_count) {
        ui_text(fb, UI_MARGIN, 394, 40, s_count ? "当前筛选没有图书" : "书架还是空的", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, UI_MARGIN, 476, 28, "从右上角导入 TXT / EPUB", EPD_DRAW_ALIGN_LEFT, false);
    }
    draw_control(fb, nav_rect(0), "上一页", 100);
    draw_control(fb, nav_rect(1), "批量管理", 101);
    draw_control(fb, nav_rect(2), "下一页", 102);
    ui_product_root_bar(fb, OS_APP_LIBRARY);
}
// 读完面板盖在正文上，不遮挡页脚。/ The end-of-book panel overlays the body above the footer.
static void draw_ended(uint8_t* fb) {
    EpdRect panel = {UI_MARGIN - 12, 360, ui_content_width() + 24, 640};
    ui_fill_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_WHITE);
    ui_draw_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN, 396, UI_PX_SECTION, "已读完本书", EPD_DRAW_ALIGN_LEFT, false);
    char name[128];
    copy_text(name, sizeof(name), s_title);
    fit_text(name, UI_PX_CAPTION, ui_content_width());
    ui_text(fb, UI_MARGIN, 452, UI_PX_CAPTION, name, EPD_DRAW_ALIGN_LEFT, false);
    for (int i = 0; i < s_next_count; ++i) {
        char label[160];
        copy_text(label, sizeof(label), s_next_books[i].name);
        char* dot = strrchr(label, '.');
        if (dot) *dot = 0;
        fit_text(label, UI_PX_BTN, ui_content_width() - 2 * UI_PAD);
        draw_control(fb, (EpdRect){UI_MARGIN, 520 + i * 108, ui_content_width(), 96}, label, 950 + i);
    }
    if (!s_next_count)
        ui_text(fb, UI_MARGIN, 560, UI_PX_CAPTION, "书架上暂无其他图书", EPD_DRAW_ALIGN_LEFT, false);
    draw_control(fb, ui_row_rect(0, 2, 880, UI_BTN_H), "继续停留", 960);
    draw_control(fb, ui_row_rect(1, 2, 880, UI_BTN_H), "返回书架", 961);
}
// 目录页：三页签 + 对应列表。/ The TOC page: three tabs plus their lists.
static void draw_toc(app_ctx_t* ctx, uint8_t* fb) {
    ui_clear_page(fb);
    char sub[128];
    snprintf(sub, sizeof(sub), "%s · 第 %u 节 · %u%%", s_title, (unsigned)s_chapter + 1, percent(s_page));
    ui_product_header(fb, "目录与书签", sub);
    char mark_label[16];
    snprintf(mark_label, sizeof(mark_label), "书签 %u", (unsigned)s_mark_count);
    for (int i = 0; i < 3; ++i) {
        EpdRect r = toc_tab_rect(i);
        bool active = s_toc_tab == (uint8_t)i;
        if (s_pressed_control == 900 + i) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
        else if (active) ui_draw_selected_round_rect(fb, r, UI_BTN_RADIUS);
        else ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
        ui_text_vc(fb, r.x + r.width / 2, r.y + r.height / 2, 28,
                   i == 1 ? mark_label : i == 2 ? "跳转" : "目录", EPD_DRAW_ALIGN_CENTER, false);
    }
    if (s_toc_tab == 1) ui_draw_button(fb, mark_manage_rect(), s_mark_manage ? "完成" : "管理书签", false);
    int rows = view_rows();
    int count = toc_row_count();
    for (int row = 0; row < rows; ++row) {
        int index = ctx->leaf * rows + row;
        if (index >= count) break;
        EpdRect r = row_rect(row, true);
        char name[160], mark[40] = "";
        if (s_toc_tab == 0) {
            if (book_chapter_title(index, name, sizeof(name)) != ESP_OK)
                snprintf(name, sizeof(name), "第 %d 节", index + 1);
            if (index == (int)s_chapter) copy_text(mark, sizeof(mark), "本节");
        } else if (s_toc_tab == 1) {
            if (!index) {
                copy_text(name, sizeof(name), "＋ 为本页加书签");
            } else {
                const book_mark_t* m = &s_marks[index - 1];
                char title[128];
                if (book_chapter_title(m->chapter, title, sizeof(title)) != ESP_OK)
                    snprintf(title, sizeof(title), "第 %u 节", (unsigned)m->chapter + 1);
                snprintf(name, sizeof(name), "%s", title);
                snprintf(mark, sizeof(mark), "%u%% · 第 %u 节", (unsigned)m->pct, (unsigned)m->chapter + 1);
            }
        } else {
            snprintf(name, sizeof(name), "跳到 %d%%", (index + 1) * 10);
            if (percent(s_page) / 10 == (unsigned)index) copy_text(mark, sizeof(mark), "当前");
        }
        int id = toc_row_id(s_toc_tab, row);
        fit_text(name, UI_PX_BTN, r.width - 2 * UI_PAD - ttf_text_width_px(UI_PX_CAPTION, mark) - UI_GAP);
        if (s_pressed_control == id) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
        ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
        if (s_toc_tab == 0 && index == (int)s_chapter) ui_draw_selected_round_rect(fb, r, UI_BTN_RADIUS);
        ui_text_vc(fb, r.x + UI_PAD, r.y + r.height / 2, UI_PX_BTN, name, EPD_DRAW_ALIGN_LEFT, false);
        ui_text_vc(fb, r.x + r.width - UI_PAD, r.y + r.height / 2, UI_PX_CAPTION, mark, EPD_DRAW_ALIGN_RIGHT, false);
    }
    const char* hint = s_message[0] ? s_message : s_toc_tab == 1 ? s_mark_manage ? "点击书签选择删除，确认后生效" : "点按跳转 · 管理书签可删除"
                                 : s_toc_tab == 2 ? "按全书字节位置估算跳转" : "点击章节直达";
    ui_text(fb, UI_MARGIN, UI_CONTENT_BOTTOM - 8, UI_PX_CAPTION, hint, EPD_DRAW_ALIGN_LEFT, false);
    draw_control(fb, ui_bar_rect(0, 3), "上一页", 100);
    draw_control(fb, ui_bar_rect(1, 3), "返回阅读", 101);
    draw_control(fb, ui_bar_rect(2, 3), "下一页", 102);
    ui_draw_menu_handle(fb, false);
    if (s_mark_delete >= 0 && (size_t)s_mark_delete < s_mark_count) {
        EpdRect panel = {UI_MARGIN - 16, 410, ui_content_width() + 32, 326};
        ui_clear_rect_fast(fb, panel);
        ui_draw_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_BLACK);
        ui_text(fb, UI_MARGIN, 444, 36, "删除这条书签？", EPD_DRAW_ALIGN_LEFT, false);
        char detail[96];
        snprintf(detail, sizeof(detail), "第 %u 节 · %u%% · 不改变阅读位置",
                 (unsigned)s_marks[s_mark_delete].chapter + 1, (unsigned)s_marks[s_mark_delete].pct);
        ui_product_title(fb, (EpdRect){UI_MARGIN, 494, ui_content_width(), 44}, s_title, 28, 1);
        ui_product_title(fb, (EpdRect){UI_MARGIN, 548, ui_content_width(), 64}, detail, 24, 2);
        ui_draw_button(fb, mark_confirm_rect(0), "保留书签", true);
        ui_draw_button(fb, mark_confirm_rect(1), "确认删除", true);
    }
}
/* ---- 正文摘录 / Body excerpts ---- */
static EpdRect quote_button(int i) { return ui_row_rect(i, 2, 944, 70); }
static bool choose_quote(uint16_t x, uint16_t y) {
    lock_draw();
    size_t hit = book_layout_text_at(s_page, body_rect(), x, y), start, end;
    bool ok = hit != SIZE_MAX && book_quote_sentence(s_text, s_text_len, hit, &start, &end);
    unlock_draw();
    if (!ok) return false;
    s_quote = (book_quote_t){.chapter = s_chapter, .byte_off = start, .end_off = end, .pct = percent(s_page)};
    copy_text(s_quote.path, sizeof(s_quote.path), s_path);
    memcpy(s_quote.text, s_text + start, end - start); s_quote.text[end - start] = 0;
    s_quote_selecting = true; s_quote_notice[0] = 0; invalidate_prep(); return true;
}
static app_redraw_t quote_action(bool save) {
    if (!save) { s_quote_selecting = false; return APP_REDRAW_PAGE; }
    if (book_quotes_add(&s_quote) == ESP_OK) {
        s_quote_selecting = false;
        copy_text(s_message, sizeof(s_message), "已保存到手帐的金句便签");
    } else copy_text(s_quote_notice, sizeof(s_quote_notice), "摘录未保存，请重试或取消");
    return APP_REDRAW_PAGE;
}
static void draw_quote(uint8_t* fb) {
    EpdRect panel = {UI_MARGIN - 12, 250, ui_content_width() + 24, 790};
    ui_fill_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_WHITE);
    ui_draw_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN + 8, 280, 34, "选中句子", EPD_DRAW_ALIGN_LEFT, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN + 8, 342, ui_content_width() - 16, 440}, s_quote.text, 30, 10);
    ui_product_title(fb, (EpdRect){UI_MARGIN + 8, 808, ui_content_width() - 16, 70},
                     s_quote_notice[0] ? s_quote_notice : "保存后可从金句便签回到本句；最多保留 16 条最新摘录。", 24, 2);
    ui_draw_button(fb, quote_button(0), "取消", false);
    ui_draw_button(fb, quote_button(1), "保存摘录", true);
}
static void render(app_ctx_t* ctx, uint8_t* fb) {
    lock_draw();
    if (s_image_open) { draw_image(fb); goto finished; }
    if (s_view == SEARCH) { draw_search(fb); goto finished; }
    if (s_view == LAYOUT) { draw_layout_menu(fb); goto finished; }
    if (s_view == TAP_ZONES) { draw_tap_zones(fb); goto finished; }
    if (s_view == TOC && s_text) { draw_toc(ctx, fb); goto finished; }
    if (s_view == READING && s_text) {
        draw_reader(fb, s_page);
        if (s_ended) draw_ended(fb);
        if (s_quote_selecting) draw_quote(fb);
    } else {
        if (s_view == SHELF || s_view == MANAGE) draw_product_shelf(ctx, fb);
        else {
            ui_clear_page(fb);
            ui_product_header(fb, "批量管理", s_storage);
            draw_control(fb, ui_row_rect(0, 3, 176, 72), s_filter == 0 ? "全部来源" : s_filter == 1 ? "TF 卡" : "内置", 110);
            draw_control(fb, ui_row_rect(1, 3, 176, 72), s_recent_sort ? "按最近" : "按名称", 111);
            draw_control(fb, ui_row_rect(2, 3, 176, 72), s_query[0] ? "搜索中" : "搜索", 112);
            char shelf_hint[128];
            snprintf(shelf_hint, sizeof(shelf_hint), "已选 %u 本 · 筛选/重扫清勾选", (unsigned)selected_count());
            ui_text(fb, UI_MARGIN, 264, UI_PX_CAPTION, s_view == BULK && s_batch_message[0] ? s_batch_message : s_message[0] ? s_message : s_view == BULK ? shelf_hint : s_shelf_warning, EPD_DRAW_ALIGN_LEFT, false);
            if (!s_visible_count && s_count)
                ui_text(fb, UI_MARGIN, 308, UI_PX_CAPTION, "当前筛选没有图书", EPD_DRAW_ALIGN_LEFT, false);
            if (!s_message[0] || s_visible_count) {
                int rows = view_rows();
                int count = s_visible_count;
                for (int row = 0; row < rows; ++row) {
                    int i = ctx->leaf * rows + row;
                    if (i >= count) break;
                    EpdRect r = row_rect(row, s_view == BULK);
                    char name[128], mark[32] = "";
                    copy_text(name, sizeof(name), s_shelf[i].name);
                    if (s_shelf[i].has_progress) snprintf(mark, sizeof(mark), "%s %u%%", s_shelf[i].is_flash ? "内置" : "", s_shelf[i].pct);
                    else if (s_shelf[i].is_flash) copy_text(mark, sizeof(mark), "内置");
                    if (s_view == BULK) snprintf(mark, sizeof(mark), "%s", s_shelf[i].selected ? "已选" : "未选");
                    if (s_shelf[i].removed) snprintf(mark, sizeof(mark), "%s待清理", s_view == BULK && s_shelf[i].selected ? "已选·" : "已删·");
                    fit_text(name, UI_PX_BTN, r.width - 2 * UI_PAD - ttf_text_width_px(UI_PX_CAPTION, mark) - UI_GAP);
                    if (s_pressed_control == row) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
                    ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
                    ui_text_vc(fb, r.x + UI_PAD, r.y + r.height / 2, UI_PX_BTN, name, EPD_DRAW_ALIGN_LEFT, false);
                    ui_text_vc(fb, r.x + r.width - UI_PAD, r.y + r.height / 2, UI_PX_CAPTION, mark, EPD_DRAW_ALIGN_RIGHT, false);
                }
            }
            draw_control(fb, ui_bar_rect(0, 3), "上一页", 100);
            draw_control(fb, ui_bar_rect(1, 3), s_view == BULK ? "返回书架" : "管理", 101);
            draw_control(fb, ui_bar_rect(2, 3), "下一页", 102);
            if (s_view == BULK) {
                const char* labels[] = {"本页全选", "清除勾选", "重新扫描", "删除所选", "清进度"};
                for (int i = 0; i < 5; ++i) draw_control(fb, batch_rect(i), labels[i], 610 + i);
                draw_batch_confirmation(fb);
            }
            ui_draw_menu_handle(fb, false);
        }
        if (s_view == MANAGE) draw_manage(fb);
        if (s_resume_path[0]) {
            EpdRect panel = {UI_MARGIN - 12, 380, ui_content_width() + 24, 350};
            ui_fill_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_WHITE);
            ui_draw_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_BLACK);
            ui_text(fb, UI_MARGIN, 410, UI_PX_BODY, "继续上次阅读？", EPD_DRAW_ALIGN_LEFT, false);
            char name[256];
            copy_text(name, sizeof(name), s_resume_name);
            fit_text(name, UI_PX_CAPTION, ui_content_width());
            ui_text(fb, UI_MARGIN, 478, UI_PX_CAPTION, name, EPD_DRAW_ALIGN_LEFT, false);
            ui_text(fb, UI_MARGIN, 530, UI_PX_CAPTION, "确认后才加载图书", EPD_DRAW_ALIGN_LEFT, false);
            draw_control(fb, ui_row_rect(0, 2, 620, UI_BTN_H), "留在书架", 700);
            draw_control(fb, ui_row_rect(1, 2, 620, UI_BTN_H), "继续阅读", 701);
        }
    }
finished:
    finish_reader_frame(ctx, fb);
    unlock_draw();
}
static bool present(app_ctx_t* ctx, app_redraw_t redraw) {
    if (redraw == APP_REDRAW_NONE || redraw == APP_REDRAW_DONE) return true;
    int64_t start = esp_timer_get_time();
    const bool search_edit = s_view == SEARCH && s_search_fast;
    if (redraw == APP_REDRAW_PAGE || redraw == APP_REDRAW_FULL) render(ctx, ctx->fb);
    // 初次进正文也提交真实黑白帧，避免入口灰阶与后续DU目标不同。
    // Body entries also commit actual black/white frames so entry gray never differs from subsequent DU targets.
    if (s_view == READING && s_text && !s_image_open && !s_toolbar && !s_clear_confirm && !s_quote_selecting && !s_ended && reader_direct_enabled()) {
        display_prepare_direct_frame(ctx->fb, epd_width(), epd_height(), app_settings_book_night());
        s_reader_target_binary = true;
    }
    bool prep = kick_prep();
    int64_t drawn = esp_timer_get_time();
    enum EpdDrawError err;
    // 仅真实布局边界请求清理，已呈现覆盖层退出正文也算边界。
    // Request cleanup only at real layout boundaries, including presented overlays returning to the body.
    bool body_entry = s_view == READING && s_text && (!s_presented_valid || s_presented_view != READING);
    bool view_entry = s_presented_valid && s_presented_view != s_view;
    bool reading_overlay = s_view == READING && s_text && (s_image_open || s_toolbar || s_clear_confirm || s_quote_selecting || s_ended);
    bool overlay_exit = s_view == READING && s_text && s_presented_valid && s_presented_view == READING &&
        s_presented_reading_overlay && !reading_overlay;
    const bool crossmux_body = s_view == READING && s_text && !reading_overlay && reader_crossmux_enabled();
    if (redraw == APP_REDRAW_FULL) {
        s_text_turn = false;
        err = crossmux_body ? update_display_night_crossmux(ctx->hl, DISPLAY_CROSSMUX_CLEAN) :
            s_reader_target_night ? update_display_clean(ctx->hl) : update_display_full(ctx->hl);
    }
    else if ((body_entry || view_entry || overlay_exit) && APP_PAGE_REFRESH_MODE == MODE_GL16) {
        s_text_turn = false;
        display_request_navigation_settle();
        err = crossmux_body ? update_display_night_crossmux(ctx->hl, DISPLAY_CROSSMUX_ENTRY) :
            update_display_with(ctx->hl, s_reader_target_night ? &E0470_TEXTTURN_NIGHT_WAVEFORM : &E0470_NAVIGATION_WAVEFORM, MODE_GL16);
    }
    else if (redraw == APP_REDRAW_AREA) {
        // 正文单遍快速翻页，控件仅局推。/ Present body turns once with the fast profile; controls stay local.
        if (s_text_turn) {
            s_text_turn = false;
            bool direct = reader_direct_enabled();
            if (direct) s_reader_target_binary = true;
            err = crossmux_body ? update_display_night_crossmux(ctx->hl, DISPLAY_CROSSMUX_TURN) :
                direct ? update_display_text_direct(ctx->hl, app_settings_book_night()) : update_display_text_turn(ctx->hl, app_settings_book_night());
        } else {
            // 阅读控件使用原灰阶表但不进入通用整页周期，字号重绘也不算翻页。
            // Reader controls retain their gray table outside generic page counting; size redraws never count as turns.
            const EpdWaveform* waveform = &E0470_WAVEFORM;
            if (s_view == SEARCH && s_search_fast) waveform = &E0470_DIRECT_WAVEFORM;
            else if ((s_mode & 0xF) == MODE_GL16 && s_view == READING && s_text)
                waveform = s_reader_target_night ? &E0470_TEXTTURN_NIGHT_WAVEFORM : &E0470_TEXTTURN_WAVEFORM;
            err = update_display_area_with(ctx->hl, waveform, s_mode, s_area);
            s_search_fast = false;
        }
    }
    else if (crossmux_body)
        err = update_display_night_crossmux(ctx->hl, DISPLAY_CROSSMUX_REDRAW);
    else if (APP_PAGE_REFRESH_MODE == MODE_GL16)
        err = update_display_with(ctx->hl, s_reader_target_night ? &E0470_TEXTTURN_NIGHT_WAVEFORM : &E0470_NAVIGATION_WAVEFORM, MODE_GL16);
    else err = update_display_mode(ctx->hl, APP_PAGE_REFRESH_MODE);
    int64_t displayed = esp_timer_get_time();
    if (prep) xSemaphoreTake(s_prep_done, portMAX_DELAY);
    if (s_view == READING && !s_image_open) footer_status(s_footer_status, sizeof(s_footer_status));
    if (redraw == APP_REDRAW_AREA && s_mode == MODE_DU) {
        s_du_area = s_du_count ? ui_rect_union(s_du_area, s_area) : s_area;
        ++s_du_count;
        s_du_ms = esp_timer_get_time() / 1000;
    } else s_du_count = 0;
    ESP_LOGI(TAG, "present draw=%lld display=%lld join=%lld ms", (drawn - start) / 1000,
             (displayed - drawn) / 1000, (esp_timer_get_time() - displayed) / 1000);
    guard_draw_result(ctx->hl, err);
    // 队列采样时间可能陈旧；从实际呈现结束计时，失败定稿也等待下一次闲置再重试。
    // Queued sample times can be stale; time from the actual present, and retry failed settling only after another idle interval.
    if (s_view == SEARCH && s_search_dirty && redraw == APP_REDRAW_AREA)
        s_search_edit_ms = esp_timer_get_time() / 1000;
    if (err == EPD_DRAW_SUCCESS) {
        s_presented_view = s_view;
        s_presented_reading_overlay = reading_overlay;
        s_presented_valid = true;
        if (s_view == SEARCH && (redraw == APP_REDRAW_PAGE || redraw == APP_REDRAW_FULL ||
            (redraw == APP_REDRAW_AREA && !search_edit)))
            s_search_dirty = s_search_fast = false;
    }
    s_mode = MODE_GL16;
    return true;
}
static app_redraw_t paint_reading(app_ctx_t* ctx, enum EpdDrawMode mode, bool body_turn) {
    int64_t started = esp_timer_get_time();
    lock_draw();
    bool cached = !s_toolbar && !s_clear_confirm && s_next_fb && s_next_page == (int)s_page;
    if (cached)
        memcpy(ctx->fb, s_next_fb, fb_bytes());
    else draw_reader(ctx->fb, s_page);
    // 预渲染的底栏可能跨分钟；翻页呈现前重画当前状态。/ A prepared footer may cross a minute; repaint current status before the turn.
    if (cached) {
        ui_clear_rect_fast(ctx->fb, ui_bar_rect(0, 1));
        draw_reader_footer(ctx->fb, s_page);
    }
    finish_reader_frame(ctx, ctx->fb);
    unlock_draw();
    ESP_LOGI(TAG, "paint cached=%d ms=%lld", cached, (esp_timer_get_time() - started) / 1000);
    // 正文、图片和页码共同推送；只有实际翻页进入正文计数，字号或延迟定稿只走控件出口。
    // Present body, illustrations and page numbers together; only actual turns count as body turns, while size changes and delayed settles use the control path.
    s_area = (EpdRect){0, 0, UI_LOCK_WIDTH, UI_LOCK_HEIGHT};
    s_mode = mode;
    s_text_turn = body_turn;
    return APP_REDRAW_AREA;
}
static void loading_detail(app_ctx_t* ctx, const char* text, const char* detail) {
    lock_draw();
    reader_patch_t patch = reader_patch_begin(ctx);
    EpdRect r = {UI_MARGIN, UI_BAR_TOP + 8, ui_bar_rect(0, 1).width, 80};
    ui_clear_rect_fast(ctx->fb, r);
    ui_text(ctx->fb, r.x, r.y, UI_PX_CAPTION, text, EPD_DRAW_ALIGN_LEFT, false);
    if (detail) ui_text(ctx->fb, r.x, r.y + 36, UI_PX_CAPTION, detail, EPD_DRAW_ALIGN_LEFT, false);
    reader_patch_end(ctx, patch);
    unlock_draw();
    guard_draw_result(ctx->hl, update_display_area_with(
        ctx->hl, patch.night ? &E0470_TEXTTURN_NIGHT_WAVEFORM : &E0470_WAVEFORM,
        patch.night ? MODE_GL16 : MODE_DU, r));
}
static void loading(app_ctx_t* ctx, const char* text) { loading_detail(ctx, text, NULL); }
static app_redraw_t open_image(app_ctx_t* ctx, size_t block_index) {
    if (block_index >= s_block_count || !s_blocks[block_index].image_src) return APP_REDRAW_NONE;
    blk_t* block = &s_blocks[block_index];
    if (s_image_block != block_index || !s_image_pixels) {
        free_image();
        invalidate_prep();
        loading_detail(ctx, "正在加载图片…", "加载后可返回原阅读位置");
        lock_draw();
        free(s_next_fb); s_next_fb = NULL;
        esp_err_t err = book_chapter_load_image(s_chapter, block->image_src, &s_image_pixels, &s_image_width, &s_image_height);
        unlock_draw();
        ensure_prep();
        s_image_block = block_index;
        if (err != ESP_OK) {
            s_image_error = err == ESP_ERR_NO_MEM ? "内存不足，请返回后重试" :
                            err == ESP_ERR_NOT_FOUND ? "图片文件缺失" :
                            err == ESP_ERR_INVALID_SIZE ? "图片超出大小限制或文件损坏" : "图片损坏或格式暂不支持";
        }
        if (block->image_repeated && !block->image_title) {
            char title[160] = {0};
            (void)book_chapter_title(block->image_first_chapter, title, sizeof(title));
            snprintf(s_image_origin, sizeof(s_image_origin), "已读最早：第 %u 节 · %s", (unsigned)block->image_first_chapter + 1, title);
        }
    }
    s_image_open = true;
    s_size_settle_ms = 0;
    invalidate_prep();
    return APP_REDRAW_PAGE;
}

static const char* book_error_message(esp_err_t err) {
    if (err == ESP_ERR_NO_MEM) return "解析失败：内存不足，请稍后重试";
    if (err == ESP_ERR_INVALID_SIZE) return "超出上限：32768 项或资源过大";
    if (err == ESP_ERR_NOT_SUPPORTED) return "文件格式、压缩或加密暂不支持";
    if (err == ESP_ERR_NOT_FOUND) return "书籍或章节缺失，请检查文件和卡";
    return "解析失败：文件异常，请重新传入";
}

/* ---- 文件与进度 / Files and progress ---- */
static void free_book(void) {
    s_quote_selecting = false;
    pending_progress_t* pending = pending_find(s_path);
    if (pending && !pending->dirty) pending_discard(s_path);
    invalidate_prep();
    free_image();
    book_layout_free();
    free(s_text);
    html_blocks_free(s_blocks, s_block_count);
    s_blocks = NULL;
    s_block_count = 0;
    s_text = NULL;
    s_text_len = 0;
    book_close();
    s_path[0] = 0;
    s_mark_count = 0;
    s_ended = false;
}
static bool load_chapter(app_ctx_t* ctx, size_t chapter, size_t offset, bool last_page) {
    // 不为加载单独刷第二个中间页。/ Avoid a second intermediate loading refresh.
    html_text_t loaded = {0};
    esp_err_t err = book_chapter_load_blocks(chapter, &loaded);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "chapter load failed: %s", esp_err_to_name(err));
        copy_text(s_message, sizeof(s_message), book_error_message(err));
        return false;
    }
    book_chapter_load_inline_images(chapter, &loaded);
    lock_draw();
    invalidate_prep();
    apply_typography();
    bool old_notice = s_font_notice;
    s_font_notice = !ttf_font_supports_text(loaded.utf8, loaded.len);
    bool ok = book_layout_begin_blocks(loaded.utf8, loaded.len, loaded.blocks, loaded.count, body_rect(), s_px);
    // 先完成续读页；其余页由 tick 分批排版，上一章末页仍须定位到末尾。
    // Finish the resume page first; ticks paginate the rest, while previous-chapter navigation needs its end.
    while (ok && !book_layout_complete() &&
           (last_page || book_layout_page_start_offset(book_layout_page_count()) <= offset))
        ok = book_layout_extend(2);
    if (!ok) {
        html_text_free(&loaded);
        s_font_notice = old_notice;
        bool restored = s_text && book_layout_build_blocks(s_text, s_text_len, s_blocks, s_block_count, body_rect(), s_px);
        unlock_draw();
        if (!restored) {
            free_book();
            s_view = SHELF;
            ctx->leaf = 0;
        }
        copy_text(s_message, sizeof(s_message), "排版失败：内存不足或章节过长");
        return false;
    }
    free(s_text);
    html_blocks_free(s_blocks, s_block_count);
    free_image();
    s_text = loaded.utf8;
    s_text_len = loaded.len;
    s_blocks = loaded.blocks;
    s_block_count = loaded.count;
    s_chapter = chapter;
    s_page = last_page ? book_layout_page_count() - 1 : book_layout_page_for_offset(offset);
    if (book_chapter_title(chapter, s_title, sizeof(s_title)) != ESP_OK)
        snprintf(s_title, sizeof(s_title), "第 %u 节", (unsigned)chapter + 1);
    copy_text(s_font_path, sizeof(s_font_path), ttf_font_path());
    s_message[0] = 0;
    unlock_draw();
    return true;
}
static bool open_book(app_ctx_t* ctx, const char* path) {
    book_cover_join();
    if (!strncmp(path, "/sdcard/", 8)) {
        read_pico_sd_info_t sd = {0};
        read_pico_sd_get_info(&sd);
        if (!sd.present || !sd.mounted) {
            copy_text(s_message, sizeof(s_message), "TF 卡不可用，请重新挂载后打开");
            return false;
        }
    }
    // 自动续读也不能绕过同路径旧文件的清理重试。
    // Automatic resume must also finish cleanup of the old file at this path.
    if (delete_retry_find(path)) {
        copy_text(s_message, sizeof(s_message), "文件已删除，进度清理失败，请重试");
        return false;
    }
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 || (uint64_t)st.st_size > UINT32_MAX) {
        copy_text(s_message, sizeof(s_message), "无法读取文件，请检查存储卡");
        return false;
    }
    if (strncmp(path, "/flash/", 7) == 0 && st.st_size > BOOK_STORE_FLASH_FILE_MAX) {
        copy_text(s_message, sizeof(s_message), "内置存储单本限 1 MB");
        return false;
    }
    if (!pending_reserve(path)) {
        copy_text(s_message, sizeof(s_message), "内存不足，无法保留待保存进度");
        return false;
    }
    save_progress();
    free_book();
    if (!pending_reserve(path)) {
        copy_text(s_message, sizeof(s_message), "内存不足，无法打开图书");
        return false;
    }
    if (!book_cached(path) && st.st_size > 1024 * 1024)
        loading_detail(ctx, "正在解析图书…", "大书需要更多时间，请稍候");
    // 持绘制锁释放下一页缓存，降低解析峰值。/ Release the next-page buffer under the drawing lock to reduce parsing peaks.
    lock_draw(); free(s_next_fb); s_next_fb = NULL; unlock_draw();
    int64_t open_started = esp_timer_get_time();
    esp_err_t err = book_open(path);
    int64_t source_ms = (esp_timer_get_time() - open_started) / 1000;
    ensure_prep();
    if (err != ESP_OK) {
        pending_progress_t* pending = pending_find(path);
        if (pending && !pending->dirty) pending_discard(path);
        copy_text(s_message, sizeof(s_message), book_error_message(err));
        ESP_LOGW(TAG, "open failed: %s", esp_err_to_name(err));
        return false;
    }
    copy_text(s_path, sizeof(s_path), path);
    const char* base = strrchr(path, '/');
    copy_text(s_book_title, sizeof(s_book_title), base ? base + 1 : path);
    char* ext = strrchr(s_book_title, '.');
    if (ext && ext != s_book_title) *ext = 0;
    s_file_size = (uint32_t)st.st_size;
    book_progress_t p = {0};
    bool resume = pending_restore(path, s_file_size, &p) || book_progress_load(path, s_file_size, &p);
    s_px = resume ? p.px : app_settings_book_px();
    if (s_px < BOOK_PX_MIN || s_px > BOOK_PX_MAX) s_px = 48;
    bool positioned = s_entry_active && s_entry.has_position && !strcmp(path, s_entry.path);
    if (positioned && s_entry.chapter >= book_chapter_count()) {
        copy_text(s_message, sizeof(s_message), "摘录所在章节已失效"); free_book(); return false;
    }
    size_t chapter = positioned ? s_entry.chapter : resume && p.chapter < book_chapter_count() ? p.chapter : 0;
    uint32_t target = 0;
    if (!positioned && resume && p.approximate) {
        target = (uint64_t)book_total_bytes() * p.pct / 100;
        if (target && target >= book_total_bytes()) --target;
        chapter = 0;
        while (chapter + 1 < book_chapter_count() && book_chapter_byte_offset(chapter + 1) <= target) ++chapter;
    }
    if (!load_chapter(ctx, chapter, positioned ? s_entry.byte_off : resume && !p.approximate && p.chapter == chapter ? p.byte_off : 0, false)) {
        free_book();
        return false;
    }
    if (!positioned && resume && p.approximate) {
        uint32_t start = book_chapter_byte_offset(chapter), end = chapter_end();
        size_t off = end > start ? (uint64_t)(target - start) * s_text_len / (end - start) : 0;
        s_page = book_layout_page_for_offset(off);
    }
    s_view = READING;
    s_mark_delete = -1; s_mark_manage = s_toolbar_sizes = false; s_layout_group = 0;
    s_toolbar = s_clear_confirm = s_batch_confirm = false;
    s_unsaved = 0;
    s_size_settle_ms = 0;
    s_ended = false;
    pending_mark_latest(s_path);
    marks_reload();
    save_progress();
    ESP_LOGI(TAG, "opened kind=%d chapters=%u pages=%u px=%d source=%lld chapter=%lld ms", book_kind(), (unsigned)book_chapter_count(), (unsigned)book_layout_page_count(), s_px,
             (long long)source_ms, (long long)((esp_timer_get_time() - open_started) / 1000 - source_ms));
    return true;
}
static app_redraw_t resume_choice(app_ctx_t* ctx, bool confirmed) {
    if (!s_resume_path[0]) return APP_REDRAW_NONE;
    char path[BOOK_STORE_PATH_MAX];
    copy_text(path, sizeof(path), s_resume_path);
    s_resume_path[0] = 0;
    if (confirmed) open_book(ctx, path);
    return APP_REDRAW_PAGE;
}
/* ---- 书架数据 / Shelf data ---- */
static bool shelf_matches(const shelf_entry_t* item) {
    return item->search_match && (s_filter == 0 || (s_filter == 1 && !item->is_flash) || (s_filter == 2 && item->is_flash));
}
static int compare_books(const void* a, const void* b) {
    const shelf_entry_t* x = a;
    const shelf_entry_t* y = b;
    if (shelf_matches(x) != shelf_matches(y)) return shelf_matches(x) ? -1 : 1;
    if (s_recent_sort && x->recent != y->recent) return x->recent > y->recent ? -1 : 1;
    int name = strcasecmp(x->name, y->name);
    return name ? name : strcmp(x->path, y->path);
}
static void sort_shelf(app_ctx_t* ctx) {
    if (s_count > 1) qsort(s_shelf, s_count, sizeof(*s_shelf), compare_books);
    s_visible_count = 0;
    while (s_visible_count < s_count && shelf_matches(&s_shelf[s_visible_count])) ++s_visible_count;
    ctx->leaf = 0;
}
static bool shelf_reserve(void) {
    if ((size_t)s_count < s_shelf_capacity) return true;
    if (s_count == INT_MAX) return false;
    size_t cap = s_shelf_capacity ? s_shelf_capacity * 2 : 32;
    if (cap > INT_MAX || cap > SIZE_MAX / sizeof(*s_shelf)) return false;
    shelf_entry_t* entries = heap_caps_realloc(s_shelf, cap * sizeof(*entries), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!entries) return false;
    s_shelf = entries;
    s_shelf_capacity = cap;
    return true;
}

// 在删除文件前预留记录，避免删除成功后才遇到内存不足。
// Reserve before unlink so allocation failure never loses a completed deletion's retry.
static delete_retry_t* delete_retry_reserve(const shelf_entry_t* entry) {
    delete_retry_t* existing = delete_retry_find(entry->path);
    if (existing) return existing;
    delete_retry_t* p = heap_caps_malloc(sizeof(*p), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(sizeof(*p));
    if (!p) return NULL;
    p->entry = *entry;
    p->next = s_delete_retries;
    s_delete_retries = p;
    return p;
}
static void delete_retry_discard(const char* path) {
    delete_retry_t** p = &s_delete_retries;
    while (*p) {
        if (!strcmp((*p)->entry.path, path)) {
            delete_retry_t* old = *p;
            *p = old->next;
            free(old);
            return;
        }
        p = &(*p)->next;
    }
}
static book_store_root_t s_scan_roots[2];
static int s_scan_roots_count, s_scan_root;
static DIR* s_scan_dir;
static bool s_scan_active, s_scan_truncated, s_scan_unreadable, s_scan_skipped;
static unsigned s_catalog_progress_revision;

static void scan_close(void) {
    if (s_scan_dir) closedir(s_scan_dir);
    s_scan_dir = NULL;
}
static void scan_shelf(app_ctx_t* ctx) {
    (void)ctx;
    scan_close();
    s_count = s_visible_count = 0;
    s_catalog_valid = false;
    s_message[0] = s_shelf_warning[0] = s_storage[0] = 0;
    s_scan_root = s_scan_roots_count = 0;
    esp_err_t root_err = book_store_roots(s_scan_roots, &s_scan_roots_count);
    for (int i = 0; i < s_scan_roots_count; ++i) {
        const char* label = s_scan_roots[i].is_flash ? "内置" : "TF";
        if (i) strncat(s_storage, " · ", sizeof(s_storage) - strlen(s_storage) - 1);
        strncat(s_storage, label, sizeof(s_storage) - strlen(s_storage) - 1);
    }
    s_scan_unreadable = root_err != ESP_OK || book_store_roots_degraded();
    s_scan_truncated = s_scan_skipped = false;
    s_scan_active = true;
    s_scan_pending = true;
}
static bool scan_shelf_step(app_ctx_t* ctx) {
    // 每轮至多四项，目录句柄随离页关闭；首屏和导航无需等全目录。
    // Limit each tick to four entries; close on exit so first paint/navigation never waits for the catalog.
    for (unsigned budget = 0; budget < 4 && s_scan_root < s_scan_roots_count; ++budget) {
        if (!s_scan_dir) {
            s_scan_dir = opendir(s_scan_roots[s_scan_root].path);
            if (!s_scan_dir) { s_scan_unreadable = true; ++s_scan_root; continue; }
        }
        errno = 0;
        struct dirent* ent = readdir(s_scan_dir);
        if (!ent) {
            if (errno) s_scan_unreadable = true;
            scan_close(); ++s_scan_root; continue;
        }
        const char* ext = strrchr(ent->d_name, '.');
        if (!ext || (strcasecmp(ext, ".txt") && strcasecmp(ext, ".epub"))) continue;
        shelf_entry_t candidate = {0};
        shelf_entry_t* item = &candidate;
        int len = snprintf(item->path, sizeof(item->path), "%s/%s", s_scan_roots[s_scan_root].path, ent->d_name);
        if (len < 0 || (size_t)len >= sizeof(item->path) || strlen(ent->d_name) >= sizeof(item->name)) { s_scan_skipped = true; continue; }
        struct stat st;
        if (stat(item->path, &st) != 0) { s_scan_unreadable = true; continue; }
        if (!S_ISREG(st.st_mode)) continue;
        if (st.st_size < 0 || (uint64_t)st.st_size > UINT32_MAX) { s_scan_skipped = true; continue; }
        if (!shelf_reserve()) { s_scan_truncated = true; scan_close(); s_scan_root = s_scan_roots_count; break; }
        copy_text(item->name, sizeof(item->name), ent->d_name);
        item->search_match = read_pico_search_match(item->name, s_query);
        item->size = st.st_size;
        item->is_flash = s_scan_roots[s_scan_root].is_flash;
        book_progress_t p;
        item->has_progress = book_progress_load(item->path, item->size, &p);
        item->pct = item->has_progress ? p.pct : 0;
        item->recent = item->has_progress ? p.last_open_s : 0;
        s_shelf[s_count++] = candidate;
    }
    if (s_scan_root < s_scan_roots_count) return false;
    int n = s_scan_roots_count;
    bool truncated = s_scan_truncated, unreadable = s_scan_unreadable, skipped = s_scan_skipped;
    // 同路径重新出现也先完成旧清理，避免新阅读进度被后续重试擦掉。
    // Finish old cleanup even if the path reappears, before new reading can create progress.
    for (delete_retry_t* p = s_delete_retries; p; p = p->next) {
        int i = 0;
        while (i < s_count && strcmp(s_shelf[i].path, p->entry.path)) ++i;
        if (i == s_count) {
            if (!shelf_reserve()) { truncated = true; continue; }
            ++s_count;
        }
        s_shelf[i] = p->entry;
        s_shelf[i].selected = false;
        s_shelf[i].search_match = read_pico_search_match(p->entry.name, s_query);
    }
    sort_shelf(ctx);
    if (!n) copy_text(s_storage, sizeof(s_storage), "存储不可用，请检查 TF 卡");
    if (truncated) snprintf(s_shelf_warning, sizeof(s_shelf_warning), "内存不足，仅列 %d 本；释放后重扫", s_count);
    else if (unreadable) copy_text(s_shelf_warning, sizeof(s_shelf_warning), "部分目录或文件不可读，请检查后重扫");
    else if (skipped) copy_text(s_shelf_warning, sizeof(s_shelf_warning), "部分文件过大或名称过长，未列入");
    else if (s_pending_invalidated) copy_text(s_shelf_warning, sizeof(s_shelf_warning), "图书已更新，旧待保存进度已作废");
    if (!s_count && !unreadable && !truncated) copy_text(s_message, sizeof(s_message), "暂无图书，请传入 TXT 或 EPUB");
    s_catalog_valid = !unreadable && !truncated;
    s_catalog_revision = book_store_revision();
    s_catalog_progress_revision = book_progress_revision();
    s_scan_active = s_scan_pending = false;
    ESP_LOGI(TAG, "shelf books=%d roots=%d", s_count, n);
    return true;
}

// 退出阅读回书架：原地刷新进度与排序，目录和封面缓存保持有效，不重扫。
// Exit reading back to the shelf: refresh progress and order in place; the
// catalog and cover caches stay valid without a rescan.
static void shelf_refresh_progress(app_ctx_t* ctx) {
    for (int i = 0; i < s_count; ++i) {
        book_progress_t p = {0};
        s_shelf[i].has_progress = book_progress_load(s_shelf[i].path, s_shelf[i].size, &p);
        s_shelf[i].pct = p.pct;
        s_shelf[i].recent = p.last_open_s;
    }
    s_catalog_progress_revision = book_progress_revision();
    sort_shelf(ctx);
}

static void refresh_capacity(void) {
    book_store_root_t roots[2];
    int count = 0;
    s_storage[0] = 0;
    book_store_roots(roots, &count);
    for (int i = 0; i < count; ++i) {
        char line[64];
        snprintf(line, sizeof(line), "%s%s %.1f MB", i ? " · " : "", roots[i].is_flash ? "内置余" : "TF余", book_store_free_bytes(&roots[i]) / 1048576.0);
        strncat(s_storage, line, sizeof(s_storage) - strlen(s_storage) - 1);
    }
}
static void manage_apply(app_ctx_t* ctx) {
    book_cover_join();
    if (!strcmp(s_path, s_managed.path)) free_book();
    esp_err_t err;
    if (s_delete_confirm && !s_file_removed) {
        delete_retry_t* retry = delete_retry_reserve(&s_managed);
        if (!retry) {
            copy_text(s_manage_message, sizeof(s_manage_message), "内存不足，无法保留删除重试记录");
            s_clear_confirm = false;
            return;
        }
        bool removed = false;
        err = book_store_delete(s_managed.path, &removed);
        if (removed) {
            retry->entry.removed = true;
            s_file_removed = true;
            pending_discard(s_managed.path);
            // 文件已删，书签一并遗忘；失败不阻塞，重试路径会再走这里。
            // The file is gone; forget its bookmarks too. Failures never block — retries pass here again.
            (void)book_marks_forget(s_managed.path);
            book_store_notify_changed();
            s_store_revision = book_store_revision();
            for (int i = 0; i < s_count; ++i) if (!strcmp(s_shelf[i].path, s_managed.path)) s_shelf[i].removed = true;
            refresh_capacity();
        } else delete_retry_discard(s_managed.path);
    } else {
        pending_discard(s_managed.path);
        err = book_progress_forget(s_managed.path);
    }
    s_clear_confirm = false;
    if (err != ESP_OK) {
        copy_text(s_manage_message, sizeof(s_manage_message), s_file_removed ?
                  "文件已删除，进度清理失败，请重试" : s_delete_confirm ?
                  "文件删除失败，请检查存储后重试" : "进度清理失败，请重新尝试");
        return;
    }
    delete_retry_discard(s_managed.path);
    s_view = SHELF;
    int write = 0, leaf = ctx->leaf;
    for (int i = 0; i < s_count; ++i) {
        if (!strcmp(s_shelf[i].path, s_managed.path)) {
            if (s_file_removed) continue;
            s_shelf[i].has_progress = false; s_shelf[i].pct = 0; s_shelf[i].recent = 0;
        }
        s_shelf[write++] = s_shelf[i];
    }
    s_count = write;
    sort_shelf(ctx);
    ctx->leaf = leaf < leaves() ? leaf : leaves() - 1;
}

/* ---- 输入与生命周期 / Input and lifecycle ---- */
static void sensor_set(app_ctx_t* ctx, bool on) {
    memset(&s_shake, 0, sizeof(s_shake));
    if (!ctx->sensor_ready) return;
    if (on) {
        if (!s_sensor_saved) {
            s_sensor_config = *sc7a20h_get_config(ctx->acc);
            s_sensor_saved = true;
        }
        sc7a20h_sensor_config_t config = s_sensor_config;
        config.odr = SC7A20H_ODR_100;
        config.fs = SC7A20H_FS_2G;
        s_sensor_on = sc7a20h_apply_config(ctx->acc, &config) == ESP_OK &&
            sc7a20h_activity_config(ctx->acc, BOOK_SHAKE_THS_MG, BOOK_SHAKE_DUR) == ESP_OK;
        if (!s_sensor_on) {
            sc7a20h_aoi_cfg_t off = {0};
            sc7a20h_aoi_config(ctx->acc, SC7A20H_AOI2, &off);
            sc7a20h_apply_config(ctx->acc, &s_sensor_config);
            s_sensor_saved = false;
            read_pico_sensor_sleep(ctx->acc);
        }
    } else {
        sc7a20h_aoi_cfg_t off = {0};
        sc7a20h_aoi_config(ctx->acc, SC7A20H_AOI2, &off);
        if (s_sensor_saved) sc7a20h_apply_config(ctx->acc, &s_sensor_config);
        s_sensor_saved = false;
        read_pico_sensor_sleep(ctx->acc);
        s_sensor_on = false;
    }
}
// 读完面板候选：先同来源，再跨来源；每次挑最近阅读优先、名称次之的一本。
// End-of-book candidates: same source first, then cross-source; each pick
// prefers recency with the name as the tiebreaker.
static void build_next_books(void) {
    s_next_count = 0;
    if (!s_shelf || !s_path[0]) return;
    bool flash = strncmp(s_path, "/flash/", 7) == 0;
    for (int pass = 0; pass < 2 && s_next_count < 3; ++pass) {
        for (;;) {
            int best = -1;
            for (int i = 0; i < s_count; ++i) {
                shelf_entry_t* b = &s_shelf[i];
                if (b->removed || !strcmp(b->path, s_path)) continue;
                if ((pass == 0) != (b->is_flash == flash)) continue;
                bool taken = false;
                for (int j = 0; j < s_next_count; ++j) if (!strcmp(s_next_books[j].path, b->path)) taken = true;
                if (taken) continue;
                if (best < 0 || b->recent > s_shelf[best].recent ||
                    (b->recent == s_shelf[best].recent && strcasecmp(b->name, s_shelf[best].name) < 0)) best = i;
            }
            if (best < 0 || s_next_count >= 3) break;
            copy_text(s_next_books[s_next_count].name, sizeof(s_next_books[0].name), s_shelf[best].name);
            copy_text(s_next_books[s_next_count].path, sizeof(s_next_books[0].path), s_shelf[best].path);
            ++s_next_count;
        }
    }
}
static app_redraw_t turn_page(app_ctx_t* ctx, int dir) {
    if (!s_text || s_clear_confirm) return APP_REDRAW_NONE;
    s_ended = false;
    if (dir > 0 && s_page + 1 >= book_layout_page_count() && !book_layout_complete()) {
        lock_draw();
        invalidate_prep();
        bool ok = book_layout_extend(2);
        unlock_draw();
        if (!ok) {
            free_book(); s_view = SHELF; ctx->leaf = 0;
            copy_text(s_message, sizeof(s_message), "排版失败：内存不足或章节过长");
            return APP_REDRAW_PAGE;
        }
    }
    bool changed = true;
    if (dir > 0 && s_page + 1 < book_layout_page_count()) ++s_page;
    else if (dir < 0 && s_page) --s_page;
    else if (dir > 0 && s_chapter + 1 < book_chapter_count()) { save_progress(); changed = load_chapter(ctx, s_chapter + 1, 0, false); }
    else if (dir < 0 && s_chapter) { save_progress(); changed = load_chapter(ctx, s_chapter - 1, 0, true); }
    else if (dir > 0 && book_layout_complete()) {
        // 全书末页再向前：弹读完面板，推荐同源下一本。/ Past the last page: show the end panel with same-source suggestions.
        build_next_books();
        s_ended = true;
        return APP_REDRAW_PAGE;
    }
    else return APP_REDRAW_NONE;
    if (!changed) { s_view = s_text ? TOC : SHELF; ctx->leaf = s_text && s_toc_tab == 0 ? (int)(s_chapter / BOOK_TOC_ROWS) : 0; return APP_REDRAW_PAGE; }
    s_toolbar = false;
    s_last_turn_ms = ctx->now_ms;
    if (s_unsaved < 8) ++s_unsaved;
    if (s_unsaved >= 8 && !s_save_failed) save_progress();
    ESP_LOGI(TAG, "turn chapter=%u page=%u/%u pct=%u", (unsigned)s_chapter, (unsigned)s_page + 1, (unsigned)book_layout_page_count(), percent(s_page));
    // 正文与页脚单次呈现；日间保持，夜间由显示层按用户周期清理。
    // Present body/footer once; day retains its profile and display owns the user-selected night cleanup interval.
    return paint_reading(ctx, MODE_GL16, true);
}
static app_redraw_t resize_text(app_ctx_t* ctx, int dir) {
    int next = s_px + dir * BOOK_PX_STEP;
    if (next < BOOK_PX_MIN || next > BOOK_PX_MAX) return APP_REDRAW_NONE;
    size_t off = book_layout_page_start_offset(s_page);
    save_progress();
    lock_draw();
    invalidate_prep();
    int old = s_px;
    apply_typography();
    if (!book_layout_build_blocks(s_text, s_text_len, s_blocks, s_block_count, body_rect(), next)) {
        bool restored = book_layout_build_blocks(s_text, s_text_len, s_blocks, s_block_count, body_rect(), old);
        unlock_draw();
        if (!restored) {
            free_book();
            s_view = SHELF;
            copy_text(s_message, sizeof(s_message), "排版失败，请重新扫描并打开图书");
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    s_px = next;
    s_page = book_layout_page_for_offset(off);
    unlock_draw();
    app_settings_set_book_px(s_px);
    save_progress();
    s_size_settle_ms = ctx->now_ms + BOOK_SIZE_SETTLE_MS;
    return paint_reading(ctx, MODE_GL16, false);
}
static void goto_font(app_ctx_t* ctx) {
    const app_desc_t* app = app_by_id(OS_APP_FONTS);
    if (app) { save_progress(); ctx->request_app = app; }
}

/* ---- 排版菜单 / Typography menu ---- */
#define BOOK_LAYOUT_ROWS 12
// 排版/翻页各六行，第三组同步独立按钮。
// Typography/turns have six rows each; the third group has dedicated sync buttons.
static bool layout_visible(int i) { return i / 6 == s_layout_group; }
static EpdRect layout_tab_rect(int i) { return ui_row_rect(i, 3, 184, 80); }
// 阅读内只显示同步操作，账号详情留在系统设置；绘制与手势共用边界。
// Reader settings show sync actions only; account details stay in system settings, with shared paint and gesture bounds.
static EpdRect sync_control_rect(int i) { return (EpdRect){UI_MARGIN, 292 + i * 112, ui_content_width(), 96}; }
static EpdRect direct_rect(void) { return (EpdRect){UI_MARGIN, 970, ui_content_width(), 72}; }
static EpdRect night_profile_rect(void) { return (EpdRect){UI_MARGIN, 1048, ui_content_width(), 48}; }
static EpdRect layout_row_rect(int i) { return (EpdRect){UI_MARGIN, 292 + (i % 6) * 112, ui_content_width(), 96}; }
static const char* layout_value(int row) {
    switch (row) {
        case 0: return ttf_font_is_builtin() ? "内置子集 ›" : "TF 字体 ›";
        case 1: return app_settings_book_leading() == 0 ? "标准" : app_settings_book_leading() <= 15 ? "舒展" : "宽松";
        case 2: return app_settings_book_margin() == 0 ? "标准" : app_settings_book_margin() == 1 ? "宽" : "更宽";
        case 3: return app_settings_book_indent() ? "两字符" : "关";
        case 4: return app_settings_book_para() ? "加大" : "标准";
        case 5: return app_settings_book_guide() == 0 ? "关" : app_settings_book_guide() == 1 ? "实线" : "虚线";
        case 6: return app_settings_book_align() == 0 ? "左" : app_settings_book_align() == 1 ? "居中" : "两端";
        case 7: return app_settings_book_tap() ? "九宫格设置 ›" : "关 ›";
        case 8: return s_shake_enabled ? "开*" : "关*";
        case 9: return app_settings_book_night() ? "开" : "关";
        case 10: return app_settings_book_auto() == 0 ? "关" : app_settings_book_auto() == 1 ? "20 秒" :
                       app_settings_book_auto() == 2 ? "40 秒" : "90 秒";
        default:
            // 第 12 行的值由 draw 处按档位现拼。/ Row 12's value is composed by the draw path.
            return "";
    }
}
// 档位循环：0/3/5/10/14/20/30。/ The gc tier cycle.
static const uint8_t k_gc_steps[] = {0, 3, 5, 10, 14, 20, 30};
static EpdRect tap_grid_cell_rect(int i) {
    int col = i % 3;
    int row = i / 3;
    int gap = 10;
    int w = (ui_content_width() - 2 * gap) / 3;
    int h = 156;
    int x = UI_MARGIN + col * (w + gap);
    int y = 196 + row * (h + gap);
    return (EpdRect){x, y, w, h};
}

static const char* k_grid_zone_names[9] = {
    "左上", "中上", "右上",
    "左中", "正中", "右中",
    "左下", "中下", "右下"
};

static const char* k_grid_action_names[4] = {"无", "上一页", "下一页", "菜单"};

static void draw_tap_zones(uint8_t* fb) {
    ui_clear_page(fb);
    ui_product_header(fb, "点击分区设置", "屏幕九等分 · 点击各宫格切换操作");
    for (int i = 0; i < 9; ++i) {
        EpdRect r = tap_grid_cell_rect(i);
        uint8_t act = app_settings_book_tap_zone(i);
        if (act == BOOK_TAP_ACTION_MENU) {
            epd_fill_rect(r, 0xE0, fb);
            epd_draw_rect(r, UI_GRAY_BLACK, fb);
        } else if (act == BOOK_TAP_ACTION_PREV || act == BOOK_TAP_ACTION_NEXT) {
            ui_draw_round_rect(fb, r, UI_CHIP_RADIUS, UI_GRAY_BLACK);
        } else {
            epd_fill_rect(r, UI_GRAY_WHITE, fb);
            ui_draw_round_rect(fb, r, UI_CHIP_RADIUS, UI_GRAY_LIGHT);
        }
        ui_text_vc(fb, r.x + r.width / 2, r.y + 42, 24, k_grid_zone_names[i], EPD_DRAW_ALIGN_CENTER, false);
        ui_text_vc(fb, r.x + r.width / 2, r.y + 104, 34, k_grid_action_names[act], EPD_DRAW_ALIGN_CENTER, false);
    }
    ui_draw_button(fb, (EpdRect){UI_MARGIN, 710, ui_content_width(), 76}, "恢复默认分区 (左上页/中菜单/右下页)", false);
    ui_draw_button(fb, (EpdRect){UI_MARGIN, 796, ui_content_width(), 76}, app_settings_book_tap() ? "点击翻页：开启" : "点击翻页：关闭", app_settings_book_tap());
    ui_draw_button(fb, ui_bar_rect(0, 2), "返回设置", false);
    ui_draw_button(fb, ui_bar_rect(1, 2), "返回阅读", false);
}
static void draw_layout_menu(uint8_t* fb) {
    ui_clear_page(fb);
    ui_product_header(fb, "更多设置", s_layout_group == 1 ? "Crossmux夜间用黑白；周期单次GC" : "排版与翻页分组，调整保留阅读位置");
    for (int i = 0; i < 3; ++i) {
        EpdRect r = layout_tab_rect(i);
        if (i == s_layout_group) ui_draw_selected_round_rect(fb, r, UI_BTN_RADIUS);
        else ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
        ui_text_vc(fb, r.x + r.width / 2, r.y + r.height / 2, 26, i == 2 ? "进度同步" : i ? "翻页显示" : "文字排版", EPD_DRAW_ALIGN_CENTER, false);
    }
    if (s_layout_group == 2) {
        const char* actions[] = {"测试连接", os_sync_pull_pending() ? "保留本地" : "上传当前进度",
                                os_sync_pull_pending() ? "确认应用远端" : "下载远端进度", "停止同步"};
        for (int i = 0; i < 4; ++i) draw_control(fb, sync_control_rect(i), actions[i], 840 + i);
        ui_product_title(fb, (EpdRect){UI_MARGIN, 778, ui_content_width(), 246},
                         s_sync_note[0] ? s_sync_note : "操作时自动连接已保存 WiFi；下载后先确认，本地位置不会自动覆盖。", 28, 3);
    }
    static const char* labels[] = {"正文字体", "行距", "页边距", "首行缩进", "段落间距",
                                   "行辅助线", "对齐方式", "点击分区", "晃动翻页*", "夜间模式", "自动翻页", "清残影周期"};
    char gc_value[16] = {0};
    for (int i = 0; i < BOOK_LAYOUT_ROWS; ++i) {
        if (!layout_visible(i)) continue;
        EpdRect r = layout_row_rect(i);
        if (s_pressed_control == 800 + i) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
        const char* value = layout_value(i);
        if (i == 11) {
            unsigned every = app_settings_gc_every();
            snprintf(gc_value, sizeof(gc_value), every ? "%u 页" : "关", every);
            value = gc_value;
        }
        ui_text_vc(fb, r.x, r.y + r.height / 2, 28, labels[i], EPD_DRAW_ALIGN_LEFT, false);
        ui_text_vc(fb, r.x + r.width, r.y + r.height / 2, 24, value, EPD_DRAW_ALIGN_RIGHT, false);
        ui_hairline(fb, r.y + r.height, r.x, r.width, UI_GRAY_LIGHT);
    }
    if (s_layout_group == 1) {
        draw_control(fb, direct_rect(), app_settings_book_direct() ? "翻页效果：无闪直刷" : "翻页效果：标准灰阶", 850);
        draw_control(fb, night_profile_rect(), app_settings_book_night_profile() == BOOK_NIGHT_PROFILE_CROSSMUX ? "夜间方案：Crossmux" : "夜间方案：当前", 852);
    } else ui_product_title(fb, (EpdRect){UI_MARGIN, 1054, ui_content_width(), 38},
                            s_layout_group == 2 ? s_title : "调整即时保存并保持位置，返回阅读生效；* 为实验功能", 22, 1);
    draw_control(fb, ui_bar_rect(0, 2), "清残影", 810);
    draw_control(fb, ui_bar_rect(1, 2), "返回阅读", 811);
    ui_draw_menu_handle(fb, false);
}
// 行距/边距/缩进/段距变更后的整章重排：保持文本锚点，菜单留在原地，失败回书架。
// Repaginate after leading/margin/indent/gap changes, keeping the anchor while the menu stays put; failures return to the shelf.
static app_redraw_t relayout_menu(app_ctx_t* ctx) {
    if (!s_text) return APP_REDRAW_PAGE;
    size_t off = book_layout_page_start_offset(s_page);
    save_progress();
    lock_draw();
    invalidate_prep();
    apply_typography();
    if (!book_layout_build_blocks(s_text, s_text_len, s_blocks, s_block_count, body_rect(), s_px)) {
        unlock_draw();
        free_book();
        s_view = SHELF;
        copy_text(s_message, sizeof(s_message), "排版失败，请重新扫描并打开图书");
        return APP_REDRAW_PAGE;
    }
    s_page = book_layout_page_for_offset(off);
    unlock_draw();
    save_progress();
    s_size_settle_ms = ctx->now_ms + BOOK_SIZE_SETTLE_MS;
    return APP_REDRAW_PAGE;
}
// 手动清理直接呈现正文，取消覆盖层与过期预渲染状态。
// Manual cleanup presents the body directly, cancelling overlays and stale preparation state.
static app_redraw_t clean_reading(void) {
    os_sync_job_cancel();
    s_view = READING;
    s_toolbar = s_toolbar_sizes = false;
    s_pressed_control = -1;
    s_text_turn = false;
    s_du_count = 0;
    s_size_settle_ms = 0;
    invalidate_prep();
    return APP_REDRAW_FULL;
}

static app_redraw_t layout_action(app_ctx_t* ctx, uint16_t x, uint16_t y) {
    if (s_layout_group == 1 && ui_rect_hit(night_profile_rect(), x, y)) {
        app_settings_set_book_night_profile(app_settings_book_night_profile() == BOOK_NIGHT_PROFILE_CURRENT ?
                                          BOOK_NIGHT_PROFILE_CROSSMUX : BOOK_NIGHT_PROFILE_CURRENT);
        invalidate_prep();
        return APP_REDRAW_PAGE;
    }
    if (s_layout_group == 1 && ui_rect_hit(direct_rect(), x, y)) {
        app_settings_set_book_direct(!app_settings_book_direct());
        invalidate_prep();
        return APP_REDRAW_PAGE;
    }
    for (int i = 0; i < 3; ++i) if (ui_rect_hit(layout_tab_rect(i), x, y)) {
        s_layout_group = i; return APP_REDRAW_PAGE;
    }
    if (s_layout_group == 2) {
        for (int i = 0; i < 4; ++i) if (ui_rect_hit(sync_control_rect(i), x, y)) {
            if (i == 3) { os_sync_job_cancel(); copy_text(s_sync_note, sizeof(s_sync_note), "已停止同步"); }
            else if (os_sync_pull_pending() && (i == 1 || i == 2)) {
                char path[BOOK_STORE_PATH_MAX]; copy_text(path, sizeof(path), s_path);
                bool applied = os_sync_pull_confirm(i == 2, s_sync_note, sizeof(s_sync_note));
                if (applied && i == 2) {
                    // 应用远端后关闭旧正文，避免开书前的保存覆盖下载结果。
                    // Close the old reader after applying remote progress so pre-open saving cannot overwrite the pull.
                    free_book();
                    open_book(ctx, path);
                    return APP_REDRAW_PAGE;
                }
            } else if (!os_sync_job_busy() && !os_sync_pull_pending()) {
                save_progress(); invalidate_prep();
                os_sync_job_start(i == 0 ? OS_SYNC_JOB_AUTH : i == 1 ? OS_SYNC_JOB_PUSH : OS_SYNC_JOB_PULL,
                                  s_sync_note, sizeof(s_sync_note));
            }
            return APP_REDRAW_PAGE;
        }
    }
    for (int i = 0; i < BOOK_LAYOUT_ROWS; ++i) if (layout_visible(i) && ui_rect_hit(layout_row_rect(i), x, y)) {
        invalidate_prep();
        // 菜单保持打开，值就地刷新；“返回阅读”或中键退出后才看到正文效果。
        // The menu stays open with values refreshing in place; body effects show after Return or the middle key.
        switch (i) {
            case 0: goto_font(ctx); return APP_REDRAW_NONE;
            case 1:
                app_settings_set_book_leading(app_settings_book_leading() >= 30 ? 0 : app_settings_book_leading() + 15);
                return relayout_menu(ctx);
            case 2:
                app_settings_set_book_margin((app_settings_book_margin() + 1) % 3);
                return relayout_menu(ctx);
            case 3:
                app_settings_set_book_indent(!app_settings_book_indent());
                return relayout_menu(ctx);
            case 4:
                app_settings_set_book_para(!app_settings_book_para());
                return relayout_menu(ctx);
            case 5: {
                uint8_t old = app_settings_book_guide();
                uint8_t next = (old + 1) % 3;
                app_settings_set_book_guide(next);
                if (!old || !next) return relayout_menu(ctx);
                break;
            }
            case 6:
                // 对齐不改变断行，仅绘制变化。/ Alignment never rewraps; only drawing changes.
                app_settings_set_book_align((app_settings_book_align() + 1) % 3);
                break;
            case 7:
                s_view = TAP_ZONES;
                break;
            case 8:
                s_shake_enabled = !s_shake_enabled;
                app_settings_set_book_shake(s_shake_enabled);
                sensor_set(ctx, s_shake_enabled);
                break;
            case 9:
                app_settings_set_book_night(!app_settings_book_night());
                break;
            case 10:
                app_settings_set_book_auto((app_settings_book_auto() + 1) % 4);
                break;
            default: {
                // 清残影周期档位循环。/ Cycle the ghost-cleanup tier.
                uint8_t current = app_settings_gc_every();
                unsigned at = 0;
                for (unsigned j = 0; j < sizeof(k_gc_steps); ++j) if (k_gc_steps[j] == current) at = j;
                app_settings_set_gc_every(k_gc_steps[(at + 1) % (sizeof(k_gc_steps) / sizeof(k_gc_steps[0]))]);
                break;
            }
        }
        return APP_REDRAW_PAGE;
    }
    if (ui_rect_hit(ui_bar_rect(0, 2), x, y)) return clean_reading();
    if (ui_rect_hit(ui_bar_rect(1, 2), x, y)) { os_sync_job_cancel(); s_view = READING; return APP_REDRAW_PAGE; }
    return APP_REDRAW_NONE;
}
static app_redraw_t manage_action(app_ctx_t* ctx, uint16_t x, uint16_t y) {
    int count = s_clear_confirm || s_file_removed ? 2 : 3;
    for (int i = 0; i < count; ++i) if (ui_rect_hit(manage_rect(i, count), x, y)) {
        if (s_clear_confirm) { if (!i) s_clear_confirm = false; else manage_apply(ctx); }
        else if (!i) s_view = SHELF;
        else if (s_file_removed) manage_apply(ctx);
        else { s_clear_confirm = true; s_delete_confirm = i == 2; s_manage_message[0] = 0; }
        break;
    }
    return APP_REDRAW_PAGE;
}
// 成功项退出选择，已删文件的失败项仅重试元数据。/ Deselect successes; removed-file failures retry metadata only.
static void batch_apply(app_ctx_t* ctx) {
    book_cover_join();
    unsigned done = 0, failed = 0;
    int write = 0;
    for (int i = 0; i < s_count; ++i) {
        shelf_entry_t item = s_shelf[i];
        bool discard = false;
        if (item.selected) {
            if (!strcmp(s_path, item.path)) free_book();
            esp_err_t err;
            if (s_batch_delete && !item.removed) {
                delete_retry_t* retry = delete_retry_reserve(&item);
                if (!retry) { ++failed; s_shelf[write++] = item; continue; }
                bool removed = false;
                err = book_store_delete(item.path, &removed);
                if (removed) {
                    retry->entry.removed = true;
                    item.removed = true; pending_discard(item.path);
                    (void)book_marks_forget(item.path);  // 文件已删，书签同删。/ File gone; bookmarks go with it.
                    book_store_notify_changed(); s_store_revision = book_store_revision();
                } else delete_retry_discard(item.path);
            } else { pending_discard(item.path); err = book_progress_forget(item.path); }
            if (err == ESP_OK) {
                delete_retry_discard(item.path);
                ++done; item.selected = false; item.has_progress = false; item.pct = 0; item.recent = 0;
                discard = item.removed;
            } else ++failed;
        }
        if (!discard) s_shelf[write++] = item;
    }
    s_count = write;
    refresh_capacity();
    int leaf = ctx->leaf;
    sort_shelf(ctx);
    ctx->leaf = leaf < leaves() ? leaf : leaves() - 1;
    s_batch_confirm = false;
    snprintf(s_batch_message, sizeof(s_batch_message), "成功 %u 本，失败 %u 本%s", done, failed, failed ? "；所选可重试" : "");
}
static app_redraw_t batch_action(app_ctx_t* ctx, uint16_t x, uint16_t y) {
    if (s_batch_confirm) {
        if (ui_rect_hit(ui_row_rect(0, 2, 620, UI_BTN_H), x, y)) s_batch_confirm = false;
        else if (ui_rect_hit(ui_row_rect(1, 2, 620, UI_BTN_H), x, y)) batch_apply(ctx);
        return APP_REDRAW_PAGE;
    }
    for (int i = 0; i < 5; ++i) if (ui_rect_hit(batch_rect(i), x, y)) {
        if (!i) select_page(ctx->leaf);
        else if (i == 1) clear_selection();
        else if (i == 2) { clear_selection(); s_batch_message[0] = 0; scan_shelf(ctx); }
        else if (selected_count()) { s_batch_delete = i == 3; s_batch_confirm = true; }
        return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}
// 工具条轨道与百分比跳转共用：按全书字节定位章与页，并回到阅读视图。
// Shared by the track tap and percent jump: locate the chapter and page by
// whole-book bytes, then return to the reader view.
static app_redraw_t jump_to_bytes(app_ctx_t* ctx, uint32_t target) {
    save_progress();
    size_t ch = 0;
    while (ch + 1 < book_chapter_count() && book_chapter_byte_offset(ch + 1) <= target) ++ch;
    if (!load_chapter(ctx, ch, 0, false)) return APP_REDRAW_PAGE;
    uint32_t start = book_chapter_byte_offset(ch), end = chapter_end();
    size_t off = end > start ? (uint64_t)(target - start) * s_text_len / (end - start) : 0;
    s_page = book_layout_page_for_offset(off);
    s_view = READING;
    s_toolbar = false;
    save_progress();
    return APP_REDRAW_PAGE;
}
// 读完面板：下一本候选优先，其次停留或回书架。/ End-of-book panel: next-book candidates first, then stay or shelf.
static app_redraw_t ended_action(app_ctx_t* ctx, uint16_t x, uint16_t y) {
    for (int i = 0; i < s_next_count; ++i)
        if (ui_rect_hit((EpdRect){UI_MARGIN, 520 + i * 108, ui_content_width(), 96}, x, y)) {
            s_ended = false;
            s_view = SHELF;
            open_book(ctx, s_next_books[i].path);
            return APP_REDRAW_PAGE;
        }
    if (ui_rect_hit(ui_row_rect(0, 2, 880, UI_BTN_H), x, y)) { s_ended = false; return APP_REDRAW_PAGE; }
    if (ui_rect_hit(ui_row_rect(1, 2, 880, UI_BTN_H), x, y)) {
        s_ended = false;
        save_progress();
        free_book();
        s_view = SHELF;
        scan_shelf(ctx);
        return APP_REDRAW_PAGE;
    }
    s_ended = false;  // 面板外点按关闭。/ Taps outside dismiss the panel.
    return APP_REDRAW_PAGE;
}
// 目录页动作：页签 + 当前列表。/ TOC actions: tabs plus the active list.
static app_redraw_t toc_action(app_ctx_t* ctx, uint16_t x, uint16_t y) {
    if (s_mark_delete >= 0) {
        for (int i = 0; i < 2; ++i) if (ui_rect_hit(mark_confirm_rect(i), x, y)) {
            if (i == 1) {
                esp_err_t err = book_marks_remove_at(s_path, (size_t)s_mark_delete);
                copy_text(s_message, sizeof(s_message), err == ESP_OK ? "已删除书签" : "删除失败，请重试");
                marks_reload();
            }
            s_mark_delete = -1;
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_toc_tab == 1 && ui_rect_hit(mark_manage_rect(), x, y)) {
        s_mark_manage = !s_mark_manage; return APP_REDRAW_PAGE;
    }
    for (int i = 0; i < 3; ++i) if (ui_rect_hit(toc_tab_rect(i), x, y)) {
        if (s_toc_tab != (uint8_t)i) { s_toc_tab = (uint8_t)i; ctx->leaf = 0; }
        if (i == 1) marks_reload();
        return APP_REDRAW_PAGE;
    }
    int rows = view_rows();
    int count = toc_row_count();
    for (int row = 0; row < rows; ++row) {
        if (!ui_rect_hit(row_rect(row, true), x, y)) continue;
        int index = ctx->leaf * rows + row;
        if (index >= count) break;
        if (s_toc_tab == 0) {
            save_progress();
            if (load_chapter(ctx, (size_t)index, 0, false)) { s_view = READING; s_toolbar = false; save_progress(); }
            return APP_REDRAW_PAGE;
        }
        if (s_toc_tab == 1) {
            if (!index) {
                esp_err_t err = book_marks_add(s_path, (uint16_t)s_chapter,
                               (uint32_t)book_layout_page_start_offset(s_page), (uint8_t)percent(s_page));
                copy_text(s_message, sizeof(s_message), err == ESP_OK ? "已添加本页书签" : "书签保存失败，请重试");
                marks_reload();
                return APP_REDRAW_PAGE;
            }
            if ((size_t)index <= s_mark_count) {
                if (s_mark_manage) { s_mark_delete = index - 1; return APP_REDRAW_PAGE; }
                book_mark_t* m = &s_marks[index - 1];
                save_progress();
                if (m->chapter < book_chapter_count() && load_chapter(ctx, m->chapter, m->byte_off, false)) {
                    s_view = READING;
                    s_toolbar = false;
                    save_progress();
                }
                return APP_REDRAW_PAGE;
            }
            return APP_REDRAW_NONE;
        }
        // 百分比跳转按全书字节估算。/ Percent jumps estimate by whole-book bytes.
        uint32_t total = book_total_bytes();
        if (!total) return APP_REDRAW_NONE;
        uint32_t target = (uint64_t)total * (uint32_t)((index + 1) * 10) / 100;
        return jump_to_bytes(ctx, target ? target - 1 : 0);
    }
    return APP_REDRAW_NONE;
}
static app_redraw_t action_at(app_ctx_t* ctx, uint16_t x, uint16_t y) {
    if (s_view == TOC && s_mark_delete >= 0) return toc_action(ctx, x, y);
    if (s_resume_path[0]) {
        for (int i = 0; i < 2; ++i) if (ui_rect_hit(ui_row_rect(i, 2, 620, UI_BTN_H), x, y)) {
            return resume_choice(ctx, i != 0);
        }
        return APP_REDRAW_NONE;
    }
    if (s_view == LAYOUT) return layout_action(ctx, x, y);
    if (s_view == MANAGE) return manage_action(ctx, x, y);
    if (s_view == TOC && s_text) { app_redraw_t result = toc_action(ctx, x, y); if (result != APP_REDRAW_NONE) return result; }
    if (s_view == TAP_ZONES) {
        for (int i = 0; i < 9; ++i) {
            if (ui_rect_hit(tap_grid_cell_rect(i), x, y)) {
                uint8_t next_act = (app_settings_book_tap_zone(i) + 1) % 4;
                app_settings_set_book_tap_zone(i, next_act);
                return APP_REDRAW_PAGE;
            }
        }
        if (ui_rect_hit((EpdRect){UI_MARGIN, 710, ui_content_width(), 76}, x, y)) {
            app_settings_reset_book_tap_zones_default();
            return APP_REDRAW_PAGE;
        }
        if (ui_rect_hit((EpdRect){UI_MARGIN, 796, ui_content_width(), 76}, x, y)) {
            app_settings_set_book_tap(!app_settings_book_tap());
            return APP_REDRAW_PAGE;
        }
        if (ui_rect_hit(ui_bar_rect(0, 2), x, y)) s_view = LAYOUT;
        else if (ui_rect_hit(ui_bar_rect(1, 2), x, y)) s_view = READING;
        return APP_REDRAW_PAGE;
    }
    if (s_view == SEARCH) {
        for (int i = 0; i < 45; ++i) if (ui_rect_hit(search_rect(i), x, y)) {
            app_redraw_t result = search_action(ctx, i);
            if (result == APP_REDRAW_AREA) {
                lock_draw();
                draw_search_input(ctx->fb);
                s_area = search_input_rect();
                EpdRect physical = ui_rotate_rect_to_fb(s_area);
                display_prepare_direct_area(ctx->fb, epd_width(), epd_height(), physical.x, physical.y, physical.width, physical.height);
                unlock_draw();
                s_mode = MODE_GL16;
                s_search_dirty = s_search_fast = true;
            }
            return result;
        }
        return APP_REDRAW_NONE;
    }
    if (s_view == BULK) { app_redraw_t result = batch_action(ctx, x, y); if (result != APP_REDRAW_NONE) return result; }
    if (s_view == READING) {
        if (s_image_open) return close_image();
        if (s_ended) return ended_action(ctx, x, y);
        if (s_font_notice && y < 88) { goto_font(ctx); return APP_REDRAW_NONE; }
        if (s_toolbar) {
            for (int i = 0; i < BOOK_TOOL_COUNT; ++i) if (ui_rect_hit(tool_rect(i), x, y)) {
                invalidate_prep();
                if (s_toolbar_sizes) {
                    if (i == 0) s_toolbar_sizes = false;
                    else if (i == 1 || i == 2) return resize_text(ctx, i == 1 ? -1 : 1);
                    else if (i == 3) { goto_font(ctx); return APP_REDRAW_NONE; }
                    else if (i == 4) { save_progress(); s_toolbar = false; s_layout_group = false; s_view = LAYOUT; }
                    else { save_progress(); free_book(); s_view = SHELF; shelf_refresh_progress(ctx); }
                    return APP_REDRAW_PAGE;
                }
                if (i == 0) { save_progress(); s_view = TOC; ctx->leaf = s_toc_tab == 0 ? s_chapter / BOOK_TOC_ROWS : 0; marks_reload(); }
                else if (i == 1) {
                    esp_err_t err = book_marks_add(s_path, (uint16_t)s_chapter,
                        (uint32_t)book_layout_page_start_offset(s_page), (uint8_t)percent(s_page));
                    copy_text(s_message, sizeof(s_message), err == ESP_OK ? "已添加本页书签" : "书签保存失败，请重试");
                    marks_reload();
                }
                else if (i == 2) { s_toolbar_sizes = true; s_message[0] = 0; }
                else if (i == 3) { save_progress(); s_toolbar = false; s_layout_group = false; s_view = LAYOUT; }
                else if (i == 4) return clean_reading();
                else { save_progress(); free_book(); s_view = SHELF; shelf_refresh_progress(ctx); }
                return APP_REDRAW_PAGE;
            }
        }
        if (!s_toolbar) {
            lock_draw();
            size_t block = book_layout_image_at(s_page, body_rect(), x, y, NULL);
            unlock_draw();
            if (block != SIZE_MAX) return open_image(ctx, block);
        }
        if (!s_toolbar && app_settings_book_tap()) {
            int zone = book_tap_grid_zone(x, y, UI_LOCK_WIDTH, UI_BAR_TOP);
            int action = (zone >= 0) ? book_tap_zone_action(app_settings_book_tap_zone((uint8_t)zone)) : -2;
            if (action == -1 || action == 1) return turn_page(ctx, action);
            if (action == -2) return APP_REDRAW_NONE; // 无操作
        }
        s_toolbar_sizes = false; s_toolbar = !s_toolbar;
        invalidate_prep();
        return APP_REDRAW_PAGE;
    }
    if (s_view == SHELF || s_view == BULK) {
        if (s_view == SHELF) {
            os_app_id_t root = ui_product_root_hit(x, y);
            if (root != OS_APP_NONE) {
                if (root != OS_APP_LIBRARY) ui_product_navigate(ctx, root);
                return APP_REDRAW_NONE;
            }
            if (ui_rect_hit((EpdRect){470, 68, 174, 68}, x, y)) {
                ctx->request_app = app_by_id(OS_APP_TRANSFER); return APP_REDRAW_NONE;
            }
        }
        if (ui_rect_hit(ui_row_rect(0, 3, 176, 72), x, y)) {
            s_filter = (s_filter + 1) % 3; clear_selection(); s_batch_message[0] = 0;
            sort_shelf(ctx);
            return APP_REDRAW_PAGE;
        }
        if (ui_rect_hit(ui_row_rect(1, 3, 176, 72), x, y)) {
            s_recent_sort = !s_recent_sort;
            sort_shelf(ctx);
            return APP_REDRAW_PAGE;
        }
        if (ui_rect_hit(ui_row_rect(2, 3, 176, 72), x, y)) { search_begin(); return APP_REDRAW_PAGE; }
        if (s_view == SHELF && s_save_failed && y >= 264 && y < 298) {
            retry_progress();
            return APP_REDRAW_PAGE;
        }
    }
    int nav = -1;
    for (int i = 0; i < 3; ++i) if (ui_rect_hit(nav_rect(i), x, y)) nav = i;
    if (nav == 0 || nav == 2) {
        int next = ctx->leaf + (nav == 0 ? -1 : 1);
        if (next >= 0 && next < leaves()) ctx->leaf = next;
        return APP_REDRAW_PAGE;
    }
    if (nav == 1) {
        s_message[0] = 0;
        if (s_view == TOC && s_text) { s_view = READING; s_toolbar = false; }
        else {
            int first = ctx->leaf * view_rows();
            s_view = s_view == BULK ? SHELF : BULK; s_batch_confirm = false;
            ctx->leaf = first / view_rows();
        }
        return APP_REDRAW_PAGE;
    }
    if (s_message[0] && !((s_view == SHELF || s_view == BULK) && s_visible_count)) return APP_REDRAW_NONE;
    if (s_view != SHELF && s_view != BULK) return APP_REDRAW_NONE;
    int rows = view_rows();
    for (int row = 0; row < rows; ++row) if (ui_rect_hit(row_rect(row, s_view == BULK), x, y)) {
        int i = ctx->leaf * rows + row;
        if (s_view == BULK && i < s_visible_count) toggle_selection(i);
        else if (s_view == SHELF && i < s_visible_count) {
            if (s_shelf[i].removed) {
                s_managed = s_shelf[i]; s_view = MANAGE; s_file_removed = s_delete_confirm = true; s_clear_confirm = false;
                copy_text(s_manage_message, sizeof(s_manage_message), "文件已删除，进度清理失败，请重试");
            } else open_book(ctx, s_shelf[i].path);
        }
        return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}
// 睡眠准备：正文打开时统一保存并落统计；失败不阻塞睡眠，交给既有重试机制。
// Sleep prepare: save the open reader and flush stats; failures never block sleep and fall back to existing retries.
static bool book_sleep_prepare(void) {
    os_sync_job_request_cancel();
    if (s_text) save_progress();
    book_stats_flush();
    return !s_save_failed;
}
static void on_enter(app_ctx_t* ctx) {
    // 外部页/全屏导航带来日间帧；后续完整绘制才发布阅读目标主题。
    // External pages/full-screen navigation supply a day frame; the next complete draw publishes the reader target theme.
    s_reader_target_night = s_reader_target_binary = false;
    s_presented_valid = false;
    s_presented_reading_overlay = false;
    s_text_turn = false;
    bool cached = s_catalog_valid && s_catalog_revision == book_store_revision();
    if (!cached) covers_reset();
    app_sleep_prepare_unregister(book_sleep_prepare);
    app_sleep_prepare_register(book_sleep_prepare);
    s_resume_path[0] = 0;
    ensure_prep();
    s_view = SHELF;
    ctx->leaf = cached ? s_shelf_leaf : 0;
    if (!cached) s_count = s_visible_count = 0;
    s_px = app_settings_book_px();
    s_scan_pending = !cached;
    s_entry_active = book_entry_take(&s_entry);
    const bool entry_taken = s_entry_active;
    // 书架进入不依赖扫描结果：立即完成请求，导航槽位绝不滞留（扫描照常后台进行）。
    // A shelf entry never depends on the scan: finish the request at once so the
    // navigation slot never strands (the scan keeps running in the background).
    if (s_entry_active && s_entry.kind == BOOK_ENTRY_SHELF) {
        book_entry_finish(BOOK_ENTRY_SHELF_READY);
        s_entry_active = false;
    }
    // 续读询问只在非请求进入（普通菜单）时出现；明确请求一律直达。
    // The resume prompt appears only for ordinary menu entry; explicit requests go straight in.
    s_resume_pending = !entry_taken;
    // 传书页已停止并 join HTTP，安全注销且只丢弃对应路径的旧进度。
    // Transfer has stopped and joined HTTP; safely unregister only invalidated paths.
    pending_drop_invalidated();
    s_store_revision = book_store_revision();
    s_mark_delete = -1; s_mark_manage = s_toolbar_sizes = false; s_layout_group = 0;
    s_toolbar = s_clear_confirm = s_batch_confirm = false;
    s_pressed_control = -1;
    s_du_count = 0;
    s_size_settle_ms = 0;
    s_poll_ms = 0;
    s_search_dirty = s_search_fast = false;
    s_sync_note[0] = 0;
    if (!cached) {
        copy_text(s_storage, sizeof(s_storage), "正在检测存储…");
        copy_text(s_message, sizeof(s_message), "正在扫描图书…");
        read_pico_sd_start_probe();
    } else if (s_catalog_progress_revision != book_progress_revision()) {
        for (int i = 0; i < s_count; ++i) {
            book_progress_t p = {0};
            s_shelf[i].has_progress = book_progress_load(s_shelf[i].path, s_shelf[i].size, &p);
            s_shelf[i].pct = p.pct; s_shelf[i].recent = p.last_open_s;
        }
        s_catalog_progress_revision = book_progress_revision();
    }
    if (s_entry_active && s_entry.kind == BOOK_ENTRY_OPEN) {
        s_scan_pending = s_resume_pending = false;
        bool opened = open_book(ctx, s_entry.path);
        book_entry_finish(opened ? BOOK_ENTRY_OPENED : BOOK_ENTRY_FAILED);
        s_entry_active = false;
        if (!opened && !cached) { s_scan_pending = true; read_pico_sd_start_probe(); }
    } else if (cached && s_entry_active) apply_entry(ctx);
    if (cached && s_resume_pending) {
        s_resume_pending = false;
        char path[BOOK_STORE_PATH_MAX];
        if (book_progress_last_path(path, sizeof(path))) for (int i = 0; i < s_count; ++i)
            if (!s_shelf[i].removed && !strcmp(s_shelf[i].path, path)) {
                copy_text(s_resume_path, sizeof(s_resume_path), path);
                copy_text(s_resume_name, sizeof(s_resume_name), s_shelf[i].name); break;
            }
    }
    s_shake_enabled = app_settings_book_shake();
    if (s_shake_enabled) sensor_set(ctx, true);
}
// 全局菜单书架明确请求不触发续读询问。/ Global-menu shelf entry explicitly skips the resume prompt.
static void book_menu_select(app_ctx_t* ctx) {
    (void)ctx;
    (void)book_entry_request(BOOK_ENTRY_SHELF, NULL);
}
static void book_on_exit(app_ctx_t* ctx) {
    scan_close(); s_scan_active = false;
    os_sync_job_cancel();
    if (s_view == SHELF) s_shelf_leaf = ctx->leaf;
    app_sleep_prepare_unregister(book_sleep_prepare);
    if (s_entry_active) book_entry_finish(BOOK_ENTRY_CANCELLED);
    s_entry_active = false;
    s_resume_path[0] = 0;
    s_pressed_control = -1;
    save_progress();
    book_stats_flush();
    sensor_set(ctx, false);
    free_book();
    if (s_prep_task) { vTaskDelete(s_prep_task); s_prep_task = NULL; }
    free(s_next_fb);
    s_next_fb = NULL;
    if (s_prep_done) { vSemaphoreDelete(s_prep_done); s_prep_done = NULL; }
    if (s_draw_lock) { vSemaphoreDelete(s_draw_lock); s_draw_lock = NULL; }
}
// 先停止使用旧卡句柄与字体预渲染，主循环随后切换内置字体。
// Stop old-card handles and font preparation before the loop switches to the builtin font.
static void book_on_media_lost(app_ctx_t* ctx) {
    scan_close(); s_scan_active = false;
    os_sync_job_cancel();
    s_catalog_valid = false;
    covers_reset();
    if (s_entry_active) book_entry_finish(BOOK_ENTRY_CANCELLED);
    s_entry_active = false;
    s_resume_path[0] = 0;
    lock_draw();
    save_progress();
    free_book();
    unlock_draw();
    s_view = SHELF;
    s_count = s_visible_count = 0;
    ctx->leaf = 0;
    s_mark_delete = -1; s_mark_manage = s_toolbar_sizes = false; s_layout_group = 0;
    s_toolbar = s_clear_confirm = s_batch_confirm = false;
    s_pressed_control = -1;
    s_scan_pending = true;
    s_resume_pending = false;
    s_du_count = 0;
    s_size_settle_ms = 0;
    copy_text(s_storage, sizeof(s_storage), "TF 卡已移除");
    copy_text(s_message, sizeof(s_message), "已停止阅读，正在检查内置图书");
}
// 控件编号仅用于保持按下与抬起命中同一个目标。/ IDs pair a press with release on the same control.
static int control_at(app_ctx_t* ctx, uint16_t x, uint16_t y, EpdRect* rect) {
    if (s_resume_path[0]) {
        for (int i = 0; i < 2; ++i) {
            *rect = ui_row_rect(i, 2, 620, UI_BTN_H);
            if (ui_rect_hit(*rect, x, y)) return 700 + i;
        }
        return -1;
    }
    if (s_view == TAP_ZONES) {
        for (int i = 0; i < 9; ++i) { *rect = tap_grid_cell_rect(i); if (ui_rect_hit(*rect, x, y)) return 850 + i; }
        *rect = (EpdRect){UI_MARGIN, 710, ui_content_width(), 76};
        if (ui_rect_hit(*rect, x, y)) return 860;
        *rect = (EpdRect){UI_MARGIN, 796, ui_content_width(), 76};
        if (ui_rect_hit(*rect, x, y)) return 861;
        for (int i = 0; i < 2; ++i) { *rect = ui_bar_rect(i, 2); if (ui_rect_hit(*rect, x, y)) return 862 + i; }
        return -1;
    }
    if (s_view == SEARCH) {
        for (int i = 0; i < 45; ++i) { *rect = search_rect(i); if (ui_rect_hit(*rect, x, y)) return 500 + i; }
        return -1;
    }
    if (s_view == BULK && s_batch_confirm) {
        for (int i = 0; i < 2; ++i) { *rect = ui_row_rect(i, 2, 620, UI_BTN_H); if (ui_rect_hit(*rect, x, y)) return 600 + i; }
        return -1;
    }
    if (s_view == BULK) for (int i = 0; i < 5; ++i) { *rect = batch_rect(i); if (ui_rect_hit(*rect, x, y)) return 610 + i; }
    if (s_view == MANAGE) {
        int count = s_clear_confirm || s_file_removed ? 2 : 3;
        for (int i = 0; i < count; ++i) {
            *rect = manage_rect(i, count);
            if (ui_rect_hit(*rect, x, y)) return s_clear_confirm ? 300 + i : s_file_removed && i == 1 ? 403 : 400 + i;
        }
        return -1;
    }
    if (s_clear_confirm) {
        for (int i = 0; i < 2; ++i) {
            *rect = ui_row_rect(i, 2, 610, UI_BTN_H);
            if (ui_rect_hit(*rect, x, y)) return 300 + i;
        }
        return -1;
    }
    if (s_view == READING) {
        if (s_ended) {
            for (int i = 0; i < s_next_count; ++i) {
                *rect = (EpdRect){UI_MARGIN, 520 + i * 108, ui_content_width(), 96};
                if (ui_rect_hit(*rect, x, y)) return 950 + i;
            }
            for (int i = 0; i < 2; ++i) {
                *rect = ui_row_rect(i, 2, 880, UI_BTN_H);
                if (ui_rect_hit(*rect, x, y)) return 960 + i;
            }
            return -1;
        }
        if (s_toolbar) for (int i = 0; i < BOOK_TOOL_COUNT; ++i) {
            *rect = tool_rect(i);
            if (ui_rect_hit(*rect, x, y)) return 200 + i;
        }
        if (!s_toolbar) {
            lock_draw();
            size_t block = book_layout_image_at(s_page, body_rect(), x, y, rect);
            unlock_draw();
            if (block != SIZE_MAX) return 1000 + (int)block;
        }
        return -1;
    }
    if (s_view == SHELF) {
        for (int i = 0; i < 4; ++i) {
            *rect = ui_bar_rect(i, 4);
            if (ui_rect_hit(*rect, x, y)) return 120 + i;
        }
        *rect = (EpdRect){470, 68, 174, 68};
        if (ui_rect_hit(*rect, x, y)) return 114;
    }
    for (int i = 0; i < 3; ++i) {
        *rect = nav_rect(i);
        if (ui_rect_hit(*rect, x, y)) return 100 + i;
    }
    if (s_view == TOC) {
        if (s_mark_delete >= 0) {
            for (int i = 0; i < 2; ++i) {
                *rect = mark_confirm_rect(i);
                if (ui_rect_hit(*rect, x, y)) return 970 + i;
            }
            return -1;
        }
        if (s_toc_tab == 1) { *rect = mark_manage_rect(); if (ui_rect_hit(*rect, x, y)) return 972; }
        for (int i = 0; i < 3; ++i) {
            *rect = toc_tab_rect(i);
            if (ui_rect_hit(*rect, x, y)) return 900 + i;
        }
        int rows = view_rows();
        int count = toc_row_count();
        for (int row = 0; row < rows && ctx->leaf * rows + row < count; ++row) {
            *rect = row_rect(row, true);
            if (ui_rect_hit(*rect, x, y)) return toc_row_id(s_toc_tab, row);
        }
        return -1;
    }
    if (s_view == SHELF || s_view == BULK) {
        for (int i = 0; i < 3; ++i) {
            *rect = ui_row_rect(i, 3, 176, 72);
            if (ui_rect_hit(*rect, x, y)) return 110 + i;
        }
        *rect = (EpdRect){UI_MARGIN, 264, ui_content_width(), 34};
        if (s_view == SHELF && s_save_failed && ui_rect_hit(*rect, x, y)) return 113;
    }
    if (s_view == LAYOUT) {
        *rect = night_profile_rect();
        if (s_layout_group == 1 && ui_rect_hit(*rect, x, y)) return 852;
        *rect = direct_rect();
        if (s_layout_group == 1 && ui_rect_hit(*rect, x, y)) return 850;
        if (s_layout_group == 2) for (int i = 0; i < 4; ++i) {
            *rect = sync_control_rect(i);
            if (ui_rect_hit(*rect, x, y)) return 840 + i;
        }
        for (int i = 0; i < 3; ++i) { *rect = layout_tab_rect(i); if (ui_rect_hit(*rect, x, y)) return 820 + i; }
        for (int i = 0; i < BOOK_LAYOUT_ROWS; ++i) {
            if (!layout_visible(i)) continue;
            *rect = layout_row_rect(i);
            if (ui_rect_hit(*rect, x, y)) return 800 + i;
        }
        *rect = ui_bar_rect(0, 2);
        if (ui_rect_hit(*rect, x, y)) return 810;
        *rect = ui_bar_rect(1, 2);
        if (ui_rect_hit(*rect, x, y)) return 811;
        return -1;
    }
    if (s_message[0] && !((s_view == SHELF || s_view == BULK) && s_visible_count)) return -1;
    int rows = view_rows();
    for (int row = 0; row < rows && ctx->leaf * rows + row < s_visible_count; ++row) {
        *rect = row_rect(row, false);
        if (ui_rect_hit(*rect, x, y)) return row;
    }
    return -1;
}
static app_redraw_t paint_control(app_ctx_t* ctx, EpdRect rect) {
    render(ctx, ctx->fb);
    s_area = rect;
    s_mode = MODE_GL16;
    return APP_REDRAW_AREA;
}
static app_redraw_t gesture_event(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    // 根导航独立于书架扫描与封面任务，加载期间也接受完整点击。
    // Root navigation is independent of shelf/cover loading and accepts taps while they run.
    if (s_view == SHELF && ev->type == UI_GESTURE_TAP) {
        os_app_id_t root = ui_product_root_hit(ev->x0, ev->y0);
        if (root != OS_APP_NONE && root == ui_product_root_hit(ev->x, ev->y)) {
            if (root != OS_APP_LIBRARY) ui_product_navigate(ctx, root);
            return APP_REDRAW_NONE;
        }
    }
    // 书架页底栏同样按下即切（书架自身除外）。/ The shelf bottom bar also switches on
    // press, except the Library tab itself.
    if (s_view == SHELF && ev->type == UI_GESTURE_PRESS) {
        os_app_id_t root = ui_product_root_hit(ev->x, ev->y);
        if (root != OS_APP_NONE && root != OS_APP_LIBRARY && ui_product_root_press(ctx, ev))
            return APP_REDRAW_NONE;
    }
    if (s_quote_selecting) {
        if (ev->type == UI_GESTURE_TAP) for (int i=0;i<2;++i)
            if (ui_rect_hit(quote_button(i),ev->x0,ev->y0) && ui_rect_hit(quote_button(i),ev->x,ev->y)) return quote_action(i==1);
        return APP_REDRAW_NONE;
    }
    if (s_image_open) {
        s_pressed_control = -1;
        return ev->type == UI_GESTURE_TAP ? close_image() : APP_REDRAW_NONE;
    }
    EpdRect start_rect = {0}, end_rect = {0};
    int start = control_at(ctx, ev->x0, ev->y0, &start_rect);
    int end = control_at(ctx, ev->x, ev->y, &end_rect);
    if (ev->type == UI_GESTURE_PRESS) {
        if (start >= 1000 || s_view == SEARCH ||
            ((s_view == SHELF || s_view == BULK) && start == 112)) {
            s_pressed_control = -1;
            return APP_REDRAW_NONE;
        }
        s_pressed_control = start;
        return start >= 0 ? paint_control(ctx, start_rect) : APP_REDRAW_NONE;
    }
    bool decorated = s_pressed_control >= 0;
    s_pressed_control = -1;
    if (ev->type == UI_GESTURE_LONG_PRESS && start == end) {
        if (s_view == SHELF && start >= 0 && start < BOOK_ROWS && !s_clear_confirm) {
            s_managed = s_shelf[ctx->leaf * BOOK_ROWS + start];
            s_view = MANAGE;
            s_clear_confirm = false;
            s_delete_confirm = s_file_removed = s_managed.removed;
            copy_text(s_manage_message, sizeof(s_manage_message), s_file_removed ? "文件已删除，进度清理失败，请重试" : "");
            return APP_REDRAW_PAGE;
        }
        // 书签行长按删除；id 段 931..939 对应当前页书签行。/ Long-press deletes a bookmark; ids 931..939 are on-page bookmark rows.
        if (s_view == TOC && s_toc_tab == 1 && s_text && start >= 931 && start <= 939) {
            int index = ctx->leaf * BOOK_TOC_ROWS + (start - 930);
            if (index >= 1 && (size_t)index <= s_mark_count) {
                s_mark_delete = index - 1;
                return APP_REDRAW_PAGE;
            }
        }
        if (s_view == READING && !s_ended && !s_toolbar && !s_clear_confirm && ui_rect_hit(body_rect(), ev->x0, ev->y0)) {
            return choose_quote(ev->x0, ev->y0) ? APP_REDRAW_PAGE : APP_REDRAW_NONE;
        }
    }
    if (ev->type == UI_GESTURE_TAP && !s_scan_pending) {
        if (start >= 0 && start == end) {
            app_redraw_t result = action_at(ctx, ev->x0, ev->y0);
            if (s_view == SEARCH) return result;
            return result != APP_REDRAW_NONE ? result : paint_control(ctx, start_rect);
        }
        if (start < 0 && end < 0 && s_view == READING && !s_clear_confirm)
            return action_at(ctx, ev->x0, ev->y0);
    }
    if (!s_clear_confirm && !s_scan_pending && !s_resume_path[0]) {
        if (s_view == READING && !s_ended && !s_toolbar && ui_rect_hit(body_rect(), ev->x0, ev->y0)) {
            if (ev->type == UI_GESTURE_SWIPE_L) return turn_page(ctx, 1);
            if (ev->type == UI_GESTURE_SWIPE_R) return turn_page(ctx, -1);
        } else if ((s_view == SHELF || s_view == TOC || (s_view == BULK && !s_batch_confirm)) &&
                   (ev->type == UI_GESTURE_SWIPE_U || ev->type == UI_GESTURE_SWIPE_D)) {
            int next = ctx->leaf + (ev->type == UI_GESTURE_SWIPE_U ? 1 : -1);
            if (next >= 0 && next < leaves()) { ctx->leaf = next; return APP_REDRAW_PAGE; }
        }
    }
    return decorated ? paint_control(ctx, start_rect) : APP_REDRAW_NONE;
}
static app_redraw_t on_key(app_ctx_t* ctx, int key) {
    s_pressed_control = -1;
    if (s_quote_selecting) return key == UI_KEY_3 ? quote_action(true) : quote_action(false);
    if (s_image_open && (key == UI_KEY_1 || key == UI_KEY_2 || key == UI_KEY_3)) return close_image();
    if (s_resume_path[0]) {
        if (key == UI_KEY_1 || key == UI_KEY_3) {
            return resume_choice(ctx, key == UI_KEY_3);
        }
        if (key == UI_KEY_2) { s_resume_path[0] = 0; ctx->request_menu = true; }
        return APP_REDRAW_PAGE;
    }
    if (s_mark_delete >= 0) {
        if (key == UI_KEY_1 || key == UI_KEY_2) { s_mark_delete = -1; return APP_REDRAW_PAGE; }
        return APP_REDRAW_NONE;
    }
    if (s_scan_pending || s_clear_confirm) return APP_REDRAW_NONE;
    if (s_view == MANAGE) {
        if (key == UI_KEY_1) { s_view = SHELF; return APP_REDRAW_PAGE; }
        return APP_REDRAW_NONE;
    }
    if (s_view == SEARCH) { if (key == UI_KEY_1) { search_finish(ctx, false); return APP_REDRAW_PAGE; } return APP_REDRAW_NONE; }
    if (s_batch_confirm) { if (key == UI_KEY_1) { s_batch_confirm = false; return APP_REDRAW_PAGE; } return APP_REDRAW_NONE; }
    if (key == UI_KEY_2) {
        if (s_view == BULK) { int first = ctx->leaf * BULK_ROWS; s_view = SHELF; ctx->leaf = first / BOOK_ROWS; }
        else if (s_view == READING) {
            s_toolbar_sizes = false; s_toolbar = !s_toolbar;
            invalidate_prep();
        } else if ((s_view == TOC || s_view == LAYOUT || s_view == TAP_ZONES) && s_text) {
            os_sync_job_cancel();
            s_view = READING;
            s_toolbar = false;
        } else {
            ctx->request_menu = true;
            return APP_REDRAW_NONE;
        }
        return APP_REDRAW_PAGE;
    }
    if (key != UI_KEY_1 && key != UI_KEY_3) return APP_REDRAW_NONE;
    // 读完面板下三键先收面板，不翻页。/ Under the end panel the keys first dismiss it instead of turning.
    if (s_view == READING && s_ended) {
        s_ended = false;
        return APP_REDRAW_PAGE;
    }
    int dir = key == UI_KEY_1 ? -1 : 1;
    if (s_view == READING) return turn_page(ctx, dir);
    int next = ctx->leaf + dir;
    if (next < 0 || next >= leaves()) return APP_REDRAW_NONE;
    ctx->leaf = next;
    return APP_REDRAW_PAGE;
}
static app_redraw_t on_key_long(app_ctx_t* ctx, int key) {
    if (key != UI_KEY_2) return APP_REDRAW_NONE;
    save_progress();
    ctx->request_menu = true;
    return APP_REDRAW_NONE;
}
// 扫描后才消费打开意图；直接菜单进入仍使用原询问流程。
// Resolve open intent only after scanning; direct menu entry keeps the original confirmation flow.
static void apply_entry(app_ctx_t* ctx) {
    if (!s_entry_active) return;
    if (s_entry.kind == BOOK_ENTRY_SHELF) {
        book_entry_finish(BOOK_ENTRY_SHELF_READY);
        s_entry_active = false;
        return;
    }
    for (int i = 0; i < s_count; ++i) {
        if (!s_shelf[i].removed && !strcmp(s_shelf[i].path, s_entry.path)) {
            book_entry_finish(open_book(ctx, s_entry.path) ? BOOK_ENTRY_OPENED : BOOK_ENTRY_FAILED);
            s_entry_active = false;
            return;
        }
    }
    copy_text(s_message, sizeof(s_message), "图书已移除或不可读，请在书架重新选择");
    book_entry_finish(BOOK_ENTRY_NOT_FOUND);
    s_entry_active = false;
}
static app_redraw_t on_tick(app_ctx_t* ctx) {
    if (os_sync_job_busy()) app_loop_stay_awake();
    if (os_sync_job_poll(s_sync_note, sizeof(s_sync_note))) return APP_REDRAW_PAGE;
    if (ctx->consumed) return APP_REDRAW_NONE;
    // 自动翻页：从上次翻页起计时，仅在正文且无工具条/插图时触发；任何翻页都会重置锚点。
    // Auto page turn: timed from the last turn, only in the reader without toolbar/image; every turn resets the anchor.
    if (app_settings_book_auto() && s_text && s_view == READING && !s_toolbar && !s_image_open && !s_quote_selecting) {
        static const int auto_ms[] = {0, 20000, 40000, 90000};
        int interval = auto_ms[app_settings_book_auto()];
        if (interval && ctx->now_ms - s_last_turn_ms >= interval) {
            app_redraw_t turned = turn_page(ctx, 1);
            if (turned != APP_REDRAW_NONE) app_loop_stay_awake();
            return turned;
        }
    }
    // 阅读时长：只统计正文视图，空闲截断与归日由统计模块负责。
    // Reading time: only the reader view counts; idle cutoff and dating belong to the stats module.
    os_time_poll(ctx->now_ms);
    uint32_t stats_date = 0;
    bool stats_valid = os_time_date_key(&stats_date);
    book_stats_observe(ctx->now_ms, s_text && s_view == READING, stats_valid, stats_date);
    if (s_save_failed && ctx->now_ms - s_save_retry_ms >= 15000) {
        s_save_retry_ms = ctx->now_ms;
        bool failed = s_save_failed;
        retry_progress();
        if (failed != s_save_failed) { invalidate_prep(); return APP_REDRAW_PAGE; }
    }
    if (s_store_revision != book_store_revision()) {
        scan_close(); s_scan_active = false;
        covers_reset(); s_catalog_valid = false;
        s_store_revision = book_store_revision();
        if (s_view == SHELF) s_scan_pending = true;
    }
    if (s_view == SHELF && !s_scan_pending && !s_resume_path[0] && !s_clear_confirm) {
        // 逐 tick 提取封面后先画目标缓冲，再灰阶推该卡片。
        // After per-tick cover extraction, paint the target buffer before grayscale presentation.
        for (int row = 0; row < BOOK_ROWS; ++row) {
            int i = ctx->leaf * BOOK_ROWS + row;
            if (i >= s_visible_count) break;
            if (s_shelf[i].removed || !is_epub_entry(&s_shelf[i]) || cover_tried(s_shelf[i].path)) continue;
            uint8_t* gray = NULL;
            if (!book_cover_poll(s_shelf[i].path, &gray)) return APP_REDRAW_NONE;
            cover_store(s_shelf[i].path, gray, ctx->leaf);
            if (!gray) continue;
            s_area = ui_product_shelf_rect(row);
            render(ctx, ctx->fb);
            s_mode = MODE_GL16;
            return APP_REDRAW_AREA;
        }
    }
    if (s_scan_pending) {
        if (!s_scan_active) {
            if (ctx->now_ms - s_poll_ms < 500) return APP_REDRAW_NONE;
            s_poll_ms = ctx->now_ms;
            read_pico_sd_info_t info = {0};
            if (read_pico_sd_get_info(&info) == ESP_ERR_NOT_FINISHED) return APP_REDRAW_NONE;
            scan_shelf(ctx);
        }
        if (!scan_shelf_step(ctx)) return APP_REDRAW_NONE;
        apply_entry(ctx);
        if (s_resume_pending) {
            s_resume_pending = false;
            char path[BOOK_STORE_PATH_MAX];
            if (book_progress_last_path(path, sizeof(path))) {
                for (int i = 0; i < s_count; ++i) if (!s_shelf[i].removed && !strcmp(s_shelf[i].path, path)) {
                    copy_text(s_resume_path, sizeof(s_resume_path), path);
                    copy_text(s_resume_name, sizeof(s_resume_name), s_shelf[i].name);
                    break;
                }
            }
        }
        return APP_REDRAW_PAGE;
    }
    if (s_view == SEARCH && s_search_dirty && ctx->now_ms - s_search_edit_ms >= UI_SETTLE_IDLE_MS) {
        lock_draw();
        draw_search_input(ctx->fb);
        unlock_draw();
        s_search_fast = false;
        s_area = search_input_rect();
        s_mode = MODE_GL16;
        return APP_REDRAW_AREA;
    }
    if (s_du_count && (s_du_count >= UI_SETTLE_DU_MAX ||
        ctx->now_ms - s_du_ms >= UI_SETTLE_IDLE_MS)) {
        s_area = s_du_area;
        s_mode = MODE_GL16;
        ESP_LOGI(TAG, "settle du=%u", s_du_count);
        return APP_REDRAW_AREA;
    }
    if (s_text && strcmp(s_font_path, ttf_font_path())) {
        s_font_notice = !ttf_font_supports_text(s_text, s_text_len);
        size_t off = book_layout_page_start_offset(s_page);
        save_progress();
        lock_draw();
        invalidate_prep();
        apply_typography();
        bool ok = book_layout_build_blocks(s_text, s_text_len, s_blocks, s_block_count, body_rect(), s_px);
        if (ok) s_page = book_layout_page_for_offset(off);
        copy_text(s_font_path, sizeof(s_font_path), ttf_font_path());
        unlock_draw();
        if (!ok) { free_book(); s_view = SHELF; copy_text(s_message, sizeof(s_message), "字体重排失败，请重新打开图书"); }
        return APP_REDRAW_PAGE;
    }
    if (s_size_settle_ms && ctx->now_ms >= s_size_settle_ms) {
        s_size_settle_ms = 0;
        if (s_view == READING && !s_image_open) return paint_reading(ctx, MODE_GL16, false);
    }
    if (s_view == READING && !s_image_open && s_text && !book_layout_complete() &&
        !s_toolbar && !(ctx->touch && ctx->touch->touched)) {
        lock_draw();
        invalidate_prep();
        bool ok = book_layout_extend(2);
        unlock_draw();
        if (!ok) {
            free_book(); s_view = SHELF; ctx->leaf = 0;
            copy_text(s_message, sizeof(s_message), "排版失败：内存不足或章节过长");
            return APP_REDRAW_PAGE;
        }
        // 页数完成后随下次交互更新，避免阅读中重复刷屏。
        // Display the final page count on the next interaction, avoiding unsolicited refreshes.
    }
    if (s_shake_enabled && s_sensor_on && ctx->now_ms - s_sensor_ms >= 40) {
        s_sensor_ms = ctx->now_ms;
        if (!sc7a20h_powered(ctx->acc)) sensor_set(ctx, true);
        sc7a20h_events_t ev;
        if (sc7a20h_read_events(ctx->acc, &ev) == ESP_OK) {
            bool suppressed = s_view != READING || s_image_open || s_toolbar || s_clear_confirm ||
                              (ctx->touch && ctx->touch->touched) || ctx->now_ms - s_last_turn_ms < 800;
            if (book_shake_feed(&s_shake, ev.aoi2_src & 0x40, suppressed, ctx->now_ms)) return turn_page(ctx, 1);
        }
    }
    if (s_view == READING && s_text && !s_image_open && !s_toolbar && !s_clear_confirm &&
        !s_ended && !s_quote_selecting && !(ctx->touch && ctx->touch->touched)) {
        char status[48];
        footer_status(status, sizeof(status));
        if (strcmp(status, s_footer_status)) {
            lock_draw();
            reader_patch_t patch = reader_patch_begin(ctx);
            invalidate_prep();
            EpdRect footer = ui_bar_rect(0, 1);
            ui_clear_rect_fast(ctx->fb, footer);
            draw_reader_footer(ctx->fb, s_page);
            reader_patch_end(ctx, patch);
            unlock_draw();
            // 夜间只驱动变化，避免黑页脚未变像素经历日间定稿；此局推不计正文周期。
            // Night drives changes only instead of day settling unchanged black footer pixels; this local push does not count as a body turn.
            guard_draw_result(ctx->hl, patch.night
                ? update_display_area_with(ctx->hl, &E0470_TEXTTURN_NIGHT_WAVEFORM, MODE_GL16, footer)
                : update_display_area_quiet(ctx->hl, footer));
            copy_text(s_footer_status, sizeof(s_footer_status), status);
            return APP_REDRAW_DONE;
        }
    }
    return APP_REDRAW_NONE;
}
static EpdRect area_hint(app_ctx_t* ctx) { (void)ctx; return s_area; }

const app_desc_t app_book = {
    .title = "书架", .detail = "图书 · 阅读 · 管理", .enter_full = false, .clean_page = true, .owns_keys = true,
    .render = render, .present = present, .on_enter = on_enter, .on_exit = book_on_exit, .on_menu_select = book_menu_select,
    .on_media_lost = book_on_media_lost,
    .on_gesture = gesture_event, .on_key = on_key, .on_key_long = on_key_long,
    .on_tick = on_tick, .area_hint = area_hint,
};
