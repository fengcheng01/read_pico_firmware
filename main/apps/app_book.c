/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 冻结修订：用户确认三档字缘观感相近，移除字缘取码选择，正文始终原始取码；不读取旧偏好。
 * 保留灰阶/黑白与快速/细腻，灰阶开关全宽；旧命中和按下态同步删除，不留隐藏入口。
 * Frozen revision: Similar device results retire edge-code choices; body text always uses original codes,
 * ignoring legacy preferences. Retain gray/binary and Fast/Fine, use a full-width gray toggle and remove old hits/press states.
 * 冻结修订：用户要求0.7.0只保留当前两种效果，UI命名快速/细腻灰阶，不显示相数或灰阶刷新实验。
 * 旧实验2..5回细腻档，原波形/字形与其它偏好不改；render不写设置。
 * Frozen revision: User-approved0.7.0 retains the two current effects as Fast/Fine Gray, without phase counts
 * or gray-refresh experiments in UI. Withdrawn2..5 fall back to Fine; retain waveforms/raster/other choices and pure render.
 * 冻结修订：用户授权独立单次28相灰阶均衡；仅日间速度灰阶纯文字正文，原21相及其它档不变。
 * 两档均使用原始灰码，已撤回21相专用校正/空间实验；render不写设置。
 * Frozen revision: The user authorizes separate single-scan28-phase balance for daytime gray-speed text only,
 * retaining21 phases and other paths. Both modes use original gray codes with21-phase correction/spatial
 * experiments withdrawn; render never writes settings.
 * 冻结：用户批准蓝牙翻页器；仅正文消费外部翻页，覆盖层与菜单输入丢弃；导航保留已连链路，网络/锁屏仍停止。
 * Frozen: User-approved Bluetooth turning consumes remote input only in body text, discards overlay/menu input, and retains established links across navigation; network/lock still stop the radio.
 *
 * 冻结修订：下一页缓冲只在实际预绘时按需分配，不在书架/返回开书解析前抢占无线与字体内存。
 * Frozen revision: Allocate next-page buffers only for actual preparation, never before shelf entry/book reopening consumes radio/font headroom.
 * 图书书架、阅读、目录与页内进度；文件解析和排版由book模块负责。
 * Book shelf, reader, TOC and progress; book modules own parsing and pagination.
 *
 * 冻结：统一手势入口并接管三键为上页/工具条/下页；工具条保留强刷，长按中键或把手打开产品导航，诊断经设置访问。屏幕翻页在抬起提交，不画按下态。
 * 晃动实验默认关，只翻下一页；离页关闭AOI2并休眠。render只绘图。
 * 预渲染回调返回前收齐，避免菜单/锁屏绕过页内TTF锁。
 * 用户修订：正文与页脚单遍更新；日间和夜间正文均按用户选择的gc_every定期清理；日间标准定稿黑/灰，夜间和直刷保持未变像素。
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
 * 实机修订：0.5.29黑基准清理后第一张仍有残影并亮闪，细边无可见改善；撤回两项入口与绘制分支，恢复原直刷及当前方案完整清白恢复。
 * Device revision: On 0.5.29 ghosts remain on the first page after black-baseline cleaning with a flash, and fine edges show no visible benefit; withdraw both controls and draw branches, restoring original direct text and current-profile physical white recovery.
 * 冻结修订：用户要求继续改进，新增独立清白直绘对照；真实清白后从白参考绘制，避免再用GC反转，普通夜间按原周期。
 * Frozen revision: The user requests continued improvement; add a distinct white-clear repaint comparison using the real white baseline without another GC reversal, retaining the saved night interval.
 * 冻结修订：字缘可选真实灰阶，完整厂家灰迁移与黑白DU共扫；原黑白直刷保持可选，灰缘较慢且可能局部闪动，仅纯文字生效。
 * Frozen revision: Edges may retain actual gray via complete vendor gray transitions alongside BW DU; original BW direct remains selectable, while gray edges are slower and may flicker locally, for text only.
 * 实机修订：灰缘抗锯齿获用户确认，但翻页先二值细字再闪成灰缘；灰边迁移左对齐相0与端点同步起扫（见波形组件），本页路由不变。
 * Device revision: The user confirms gray-edge anti-aliasing but reports binary-first-then-gray turns; gray transitions now left-align to phase 0 beside the endpoints (see the waveform component), with this page's routing unchanged.
 * 实机修订：左对齐后仍见先锯齿后抖动；灰边改按跟手DU公式单调同向逼近（见波形组件），与黑白直刷同速，本页路由不变。
 * Device revision: After left-alignment the jagged-then-shake look remains; gray edges now approach monotonically per the follow-DU formula (see the waveform component), as fast as binary direct, with this page's routing unchanged.
 * 冻结修订：用户继续要求改善夜间黑底残影，新增可选白预驱实验（默认关）；15→0先白后黑仅在普通夜间翻页生效，周期清理与日间路径不变，除残影效果须同板实测。
 * Frozen revision: The user keeps asking for night black-background relief, adding the optional white pre-drive experiment (off by default); 15→0 white-then-black applies only to ordinary night turns, leaving intervals, cleaning and day paths unchanged, with ghost relief awaiting same-board proof.
 * 实机修订：残影仍在；白预驱升级为厂家对角线满剂量并叠加保持黑底微洗（见波形组件），开关与生效范围不变。
 * Device revision: Ghosts remain; the white pre-drive upgrades to the full vendor diagonal dose plus a held-black mini-wash (see the waveform component), with the switch and its scope unchanged.
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
 * 用户修订：阅读设置里的枚举值统一由底部选择面板点选；字体、点击分区和实验项仍进入专用页面。书架当前页稳定后预取相邻页，翻页首帧从复用缓存恢复，不先画空封面。
 * 实机修订：阅读内启动进度同步前释放正文预绘任务栈，联网结束后只解除暂停、下一次真实翻页再懒创建，避免刚释放的连续内存再次碎片化并阻断随后蓝牙启动。
 * Frozen: Use shared gestures and own previous/tools/next keys; retain toolbar full refresh and middle-key hold/handle product navigation, with diagnostics under Settings. Screen turns commit on release without pressed decoration.
 * Shake is experimental, off by default, forward only; exit disables AOI2 and sleeps it. Render only paints.
 * Join preparation before returning callbacks so menus/lock cannot race the page-local TTF lock.
 * User revision: present body/footer in one pass; day and night body turns clean at the user's gc_every interval. Standard day settles black/gray and night/direct hold unchanged pixels.
 * Middle-key press toggles tools; holding for 500ms opens product navigation, as approved in the four-root redesign.
 * User-authorized management shows full details before separate clear/delete confirmations; retain failed saves for retry without pruning other books.
 * User revision: single-book actions use a shelf dialog; full management is for batches. Paging/sorting preserve selection; filtering/applied search and rescanning clear it.
 * Invalidate failed progress only for changed paths; retain deletion cleanup retries across page exits for this boot.
 * Lost media saves progress and closes reader resources before global font fallback; never auto-resume an invalid mount.
 * User revision: offer resume on shelf entry; open the previous book only after confirmation to keep the shelf available.
 * User-approved OS: explicit home open requests open after file/mount validation before the first shelf frame; explicit shelf requests skip resume, menu shelf selections also enter the shelf directly.
 * User-requested product redesign: ordinary shelves use three typographic-cover rows and four-root navigation; batches keep seven rows and all confirmation/save semantics.
 * User-requested responsive tabs: poll a bounded background cover worker; root exits never join it, file mutation/open and sleep do. TXT/coverless books retain typographic covers.
 * User-ordered P1 revision: TXT and coverless books now draw deterministic generated covers (path-seeded, three geometric layouts, no text) through the same card slots; typographic covers remain only when generation fails.
 * Hardware feedback changed images to bounded chapter-time decoding and inline pagination; failed/over-budget images retain retry placeholders; render never decodes.
 * User-approved completion: the lock/sleep prepare hook saves reader progress first; reading time accumulates only on valid local dates, cuts off after 5 idle minutes and skips uncalibrated periods.
 * User-approved completion: the toolbar gains a typography menu (font/leading/margins/first-line indent/paragraph gap/per-line guide rule/tap zones/shake/night/auto turn/screen cleaning); leading, margin, indent and gap changes repaginate at once while keeping the anchor; guide rules run under every text line, solid or dashed; alignment adds left/center/justified and the footer status bar gains clock/battery switches; the menu stays open after changes and exits via Return or the middle key.
 * User revision: enumerated reader settings use the bottom choice sheet; font,
 * tap zones and experiments retain dedicated pages. After the current shelf page
 * settles, adjacent pages prefetch and page changes restore cached covers before paint.
 * Device revision: release the body-prefetch task stack before progress-sync networking, then only lift suspension and recreate it lazily on the next real turn, so the reclaimed contiguous block is not fragmented again before a following BLE start.
 * 用户实机反馈：阅读设置增加按需自动联网同步，下载先确认；书架目录与封面跨根页缓存，来源变化或移除时失效。
 * Hardware feedback: reader settings add on-demand WiFi sync with confirmed pulls; cache shelf catalogs/covers across roots until source changes or loss.
 * 用户修订：阅读内进度同步隐藏服务器与用户名，只保留操作与反馈；账号仍在系统设置配置。
 * User revision: reader progress sync hides the server and username, retaining actions and feedback; account setup remains in system settings.
 * 冻结：真实Tab/当前正文布局入口一次厂家GC16；普通导航用厂家GL16，正文翻页用日夜标准或厂家黑白DU，夜间另按用户周期清理；已移除的Crossmux对照入口见下方撤回记录。
 * 修订原因：0.5.20入口残影及夜间白字累积均可由手动清理恢复，用户批准入口清理与夜间周期清理，不再使用单相入口推白。
 * 冻结：用户要求正文长按选择所在句子，确认保存真实摘录并保留章节字节锚点；目录由工具栏访问。
 * Frozen: User-requested body holds select sentences for confirmed excerpts with chapter byte anchors; the toolbar retains TOC access.
 * Frozen: Real Tab/current-body layout entries use one vendor GC16; ordinary navigation uses vendor GL16 and body turns use day/night standard or vendor black/white DU, with separate user-selected night cleanup; the removed Crossmux comparison entry is recorded below as withdrawn.
 * Revision: Manual cleaning restores 0.5.20 entry ghosts and accumulated white night glyphs; the user authorizes entry and periodic night cleanup instead of a single white entry action.
 * 用户批准设计稿落地：工具拆为目录/书签、添加书签、字号子面板、更多设置、清残影和书架；更多设置两组各六行，书签删除先确认，降低误触与信息密度。
 * User-approved design implementation: tools expose TOC/bookmarks, add mark, size subpanel, grouped settings, cleaning and shelf; two six-row groups and confirmed bookmark deletion reduce density and accidental loss.
 * 用户批准六项功能：目录页三页签（章节/书签/百分比跳转，书签管理或长按先确认删除）；读完面板推荐同源下一本；清残影周期改档位设置（gc_every）；异常复位日志走 os_crash；黄历锁屏在 sleep/设置侧。
 * Frozen: The TOC gains three tabs (chapters/bookmarks/percent jump, management or long-press confirms bookmark deletion); an end-of-book panel suggests same-source next books; the ghost-cleanup period becomes the gc_every setting; abnormal resets log via os_crash; the almanac lock face lives in sleep/settings.
 * 实机修订：0.5.25定向补黑和后置局部擦写未改善夜间残影，后者增加旧字闪动；撤回两实验，保留普通翻页与已有周期/手动完整清理。
 * Device revision: 0.5.25 black reinforcement and post-DU local cleaning failed to improve night ghosts, with local cleaning flashing old glyphs; withdraw both experiments and retain ordinary turns with existing interval/manual full cleaning.
 * 冻结：用户批准独立Crossmux Pico刷新对照，默认仍当前方案；对照仅夜间纯文字固定黑白，入口GL、翻页DU、周期/手动单次GC，图片/控件/日间保留原路径；不是整套Crossmux移植。
 * Frozen: The user authorizes an independent Crossmux Pico refresh comparison, retaining the current default; comparison night text is binary with entry GL, turn DU and one periodic/manual GC, while images/controls/day keep their paths; this is not a whole Crossmux transplant.
 * 实机决策：用户要求移除Crossmux夜间对照；方案循环回到当前/清白直绘两态，旧存值1回当前，直刷不再由对照隐式开启。
 * Device decision: The user orders the Crossmux night comparison removed; the profile cycle returns to Current/White-repaint, stored value 1 falls back to Current, and direct no longer turns on implicitly with the comparison.
 * 实机决策（暂时）：多轮实机调夜间残影未果，用户要求暂时下线夜间模式——夜间开关、方案与白预驱入口删除，存量夜间设置不再生效恒为日间；
 * 显示层夜间机制与存储保留待面板级方案，恢复只需还原入口与设置读取。
 * Device decision (temporary): after repeated failed night-ghost tuning the user orders night mode withdrawn — the night toggle,
 * profile and pre-drive entries are deleted and stored night settings no longer apply, always rendering day;
 * the display-layer machinery and storage stay for a panel-level fix, with restoration needing only the entries and the settings read.
 * 冻结修订：用户批准摘录自适应确认卡；实机修订只在确认或临时操作显示三键语义，正文/工具/设置不常驻；保留选句、保存重试与长按导航。
 * 决策变更（2026-10-17）：用户参考 KOReader 调研批准页脚定位条——按下页脚带进入、拖动 DU 跟手、
 * 松开按全书字节跳转、点按/多点取消；仍是临时浮层，正文区域不加常驻进度尺的原则不变。
 * Frozen revision: User-approved adaptive excerpts show key meanings only for confirmation/transient actions after hardware feedback; body/tools/settings stay uncluttered; preserve holds, save retry and navigation.
 * Decision change (2026-10-17): following the KOReader review the user approved a footer scrub — pressing
 * the footer band enters, drags track via DU, release jumps by whole-book bytes, tap/multitouch cancels;
 * it remains a transient overlay and the body stays free of any persistent progress ruler.
 * 冻结修订：用户实机要求整理书架；六封面改为三列两排大封面，列表/封面双图标并入统一工具栏。
 * Frozen revision: Hardware feedback reorganizes the shelf into three columns/two rows of larger covers, with paired layout icons in one toolbar.
 * 三行列表仍默认，布局切换保留首个可见书的位置，批量管理继续用列表。
 * Three rows remain default, layout changes keep the leading book, and bulk management remains a list.
 * 冻结修订：用户批准第二批 UI；字号以进入面板时的同一原文锚点预览，预览只绘制、不另建页表；设置按排版/操作/显示/同步分组，实验单列，字体返回恢复来源面板。
 * Frozen revision: User-approved batch two previews sizes at one source anchor without a second page table; settings group text/input/display/sync with separate experiments, and font returns restore the source panel.
 * 冻结修订：用户批准四项阅读优化；预绘只在推屏期间后台运行、逐行停止，空闲分片续绘；布局/字体/覆盖层变更作废旧结果。
 * Frozen revision: User-approved reading optimization runs background prefetch during output only, stopping at row boundaries and resuming in idle slices; layout/font/overlay changes discard stale work.
 * 冻结修订：第二批省电仅在正文排版和预绘就绪后请求50ms输入可打断心跳；剩余时间用会话内有效向前阅读样本，长书显示小时/天而非999分封顶。
 * Frozen revision: Batch-two power saving requests a 50ms input-interruptible heartbeat only after body layout/prefetch settles; ETA uses effective forward session samples and hours/days instead of a 999-minute cap.
 * 字重仅调整正文覆盖率并保留字宽；封面有界复用与成组补刷，不更改波形或硬件时序。
 * Stroke tuning affects body coverage only, retaining advances; bounded cover reuse and grouped updates never alter waveforms or hardware timing.
 * 实机修订：0.6.6灰阶速度覆盖率加深退化，撤回补偿，所有档位继续使用用户原字重。
 * Device revision: Hardware rejects the0.6.6 gray-speed coverage boost; withdraw it and retain user-selected stroke in every mode.
 * 用户修订：速度灰阶仅提高曲线展平精度，不增加刷屏相数、补刷或覆盖率剂量；质量档及UI保留原光栅。
 * User revision: Gray-speed only refines curve flattening, without added phases/scans/coverage doses; quality and UI keep original rasterization.
 * 决策变更（2026-10-17）：用户要求无闪与翻页速度不变的前提下继续改善灰缘抗锯齿；速度灰缘绘制取码
 * 可按落点表反演（仅目标灰码选择，不动波形/相数/剂量），实验开关 bk_edgeopt 默认关，落点数值须同板校准。
 * Decision change (2026-10-17): the user asks for further gray-edge anti-aliasing with the flash-free
 * turn speed unchanged; gray-edge code selection may invert a landing table (target codes only, without
 * waveform/phase/dose changes) behind the experimental bk_edgeopt switch, off by default, landing values
 * to be calibrated on the same board.
 * 冻结修订：0.7.0用户确认三档近似，撤回上述取码对照；不读旧bk_edgeopt，正文固定原始取码。
 * Frozen revision: Similar results across all three modes retire the above comparisons in0.7.0; never read old bk_edgeopt and always use original body codes.
 */
#include <dirent.h>
#include <stdatomic.h>
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
#include "boot_state.h"
#include "book_layout.h"
#include "book_eta.h"
#include "book_cover.h"
#include "book_cover_auto.h"
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
#include "os_page_turner.h"
#include "read_pico_init.h"
#include "read_pico_sd.h"
#include "read_pico_search.h"
#include "settings.h"
#include "ttf_font.h"
#include "ui_kit.h"
#include "ui_gesture.h"
#include "ui_dither.h"
#include "ui_menu.h"
#include "ui_product.h"
#include "ui_sheet.h"

#define BOOK_ROWS UI_PRODUCT_SHELF_ROWS
#define BULK_ROWS 7
#define BOOK_TOC_ROWS 10
#define BOOK_PCT_ROWS 10
// 字号上下界沿用进度记录的共用档位；步进是阅读器自己的缩放粒度。
// Tier bounds come from the shared progress range; the step is the reader's own zoom granularity.
#define BOOK_PX_MIN BOOK_PROGRESS_PX_MIN
#define BOOK_PX_MAX BOOK_PROGRESS_PX_MAX
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
static bool s_recent_sort, s_shelf_grid;
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
static bool s_animation_save_failed;
static bool s_gray_balance_save_failed;
static bool s_pending_invalidated;
static int64_t s_save_retry_ms;
static unsigned s_store_revision;
static bool s_scan_pending, s_resume_pending, s_toolbar, s_clear_confirm;
// 断电唤醒的自动续读：不等书架扫描，TF 探测完成即直接开书回进度；扫描转后台补齐。
// Auto-resume after a power-cut wake: no shelf-scan wait — reopen the book at its
// saved position as soon as the TF probe finishes, with the scan filling in afterwards.
static bool s_boot_resume;
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
// 本书累计有效阅读毫秒/翻页数（进度 v3 持久）；s_time_left 是页脚缓存快照，绘制只读。
// Cumulative effective reading milliseconds/page turns for this book (progress v3);
// s_time_left is the footer's cached snapshot that drawing only reads.
static uint32_t s_read_ms, s_pages;
static char s_time_left[40];
static book_eta_t s_eta;
static int64_t s_eta_since;
static int64_t s_eta_last_tick;
static uint16_t s_chapter_ticks[128];
static int s_chapter_tick_count;
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
static bool s_soft_turn_eligible;
// 最近一次翻页方向：揭页动画的波前走向。/ Direction of the last turn: the page-turn wavefront heading.
static int s_turn_dir = 1;
static int s_pressed_control = -1;
static int64_t s_du_ms;
static unsigned s_du_count;
static EpdRect s_du_area;
static bool s_search_dirty, s_search_fast;
static int64_t s_search_edit_ms;
static SemaphoreHandle_t s_draw_lock, s_prep_done;
static TaskHandle_t s_prep_task;
static atomic_bool s_prep_stop;
static bool s_prep_sync_suspended;
static book_layout_cursor_t s_prep_cursor;
static uint8_t* s_next_fb;
static int s_next_page = -1, s_prep_page = -1;
// 目录页三页签：0 章节 1 书签 2 百分比跳转。/ TOC tabs: 0 chapters, 1 bookmarks, 2 percent jump.
static uint8_t s_toc_tab;
static int s_mark_delete = -1;
static bool s_mark_manage, s_toolbar_sizes;
// 面板会话固定原文位置，连续调整不能沿重分页页首漂移。
// Keep one source position per panel session so repeated reflows cannot drift through page starts.
static size_t s_panel_anchor;
typedef struct {
    bool active, sizes;
    char path[BOOK_STORE_PATH_MAX];
    uint16_t chapter;
    uint32_t offset;
    book_view_t view;
    uint8_t group;
} font_return_t;
static font_return_t s_font_return;
static uint8_t s_layout_group;
// 更多设置的选择面板：打开的行号，-1 关闭。/ The grouped-settings choice sheet: open row, -1 closed.
static int s_choice_row = -1;
static bool s_stroke_failed;
static char s_sync_note[128];
static bool s_catalog_valid;
static unsigned s_catalog_revision;
static int s_shelf_leaf;
static int s_layout_leaf = -1, s_layout_anchor;
static bool s_layout_grid;
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
static void cover_offset(size_t offset);
static void sort_shelf(app_ctx_t* ctx);
static void draw_control(uint8_t* fb, EpdRect rect, const char* label, int id) {
    bool shelf_control = s_view == SHELF && id >= 110 && id <= 112;
    if (id == 112 && s_query[0]) {
        ui_fill_round_rect(fb, rect, UI_BTN_RADIUS, UI_GRAY_BLACK);
        ui_text_vc(fb, rect.x + rect.width / 2, rect.y + rect.height / 2,
                   shelf_control ? 26 : UI_PX_BTN, label, EPD_DRAW_ALIGN_CENTER, true);
        return;
    }
    if (s_pressed_control == id) ui_draw_pressed_round_rect(fb, rect, UI_BTN_RADIUS);
    if (shelf_control) {
        ui_draw_round_rect(fb, rect, UI_BTN_RADIUS, UI_GRAY_BLACK);
        ui_text_vc(fb, rect.x + rect.width / 2, rect.y + rect.height / 2, 26, label, EPD_DRAW_ALIGN_CENTER, false);
    } else ui_draw_button(fb, rect, label, false);
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
static bool reader_direct_enabled(void) {
    if (!app_settings_book_direct()) return false;
    for (size_t i = 0; i < s_block_count; ++i) if (s_blocks[i].image) return false;
    return true;
}
static bool reader_gray_direct_enabled(void) {
    return reader_direct_enabled() && app_settings_book_edge_gray();
}
static bool reader_gray_balanced_enabled(void) {
    return reader_gray_direct_enabled() && !app_settings_book_night() && app_settings_book_gray_balanced();
}
static void reader_body_font(bool active) {
    ttf_set_stroke(active ? app_settings_book_stroke() : 0);
    ttf_set_precise_curves(active && reader_gray_direct_enabled());
    ttf_set_edge_style(0);
}
static bool reader_white_repaint_enabled(void) {
    if (!app_settings_book_night() || app_settings_book_night_profile() != BOOK_NIGHT_PROFILE_WHITE_REPAINT) return false;
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
    book_layout_set_guide_contrast(reader_direct_enabled() && !reader_gray_direct_enabled());
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
static int shelf_rows(void) { return s_shelf_grid ? 6 : BOOK_ROWS; }
// 同一浏览位置切换布局保留原首本书，真正翻页或重排后重新取锚点。
// Keep the original leading book while toggling one browsing position; navigation/reordering resets the anchor.
static void change_shelf_layout(app_ctx_t* ctx) {
    if (s_layout_leaf != ctx->leaf || s_layout_grid != s_shelf_grid) s_layout_anchor = ctx->leaf * shelf_rows();
    s_shelf_grid = !s_shelf_grid;
    app_settings_set_shelf_grid(s_shelf_grid);
    ctx->leaf = s_layout_anchor / shelf_rows();
    s_shelf_leaf = s_layout_leaf = ctx->leaf;
    s_layout_grid = s_shelf_grid;
}
static EpdRect shelf_layout_rect(void) { return ui_product_shelf_control_rect(3); }
static EpdRect shelf_filter_rect(int index) {
    return s_view == SHELF ? ui_product_shelf_control_rect(index) : ui_row_rect(index, 3, 176, 72);
}
static EpdRect row_rect(int row, bool toc) {
    if (!toc && s_view != BULK) return s_shelf_grid ? ui_product_shelf_grid_rect(row) : ui_product_shelf_rect(row);
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
static int view_rows(void) { return s_view == TOC ? BOOK_TOC_ROWS : s_view == BULK ? BULK_ROWS : shelf_rows(); }
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
static unsigned percent_basis_points(size_t page) {
    uint32_t total = book_total_bytes();
    if (!total) return 0;
    if (book_layout_complete() && s_chapter + 1 == book_chapter_count() && page + 1 == book_layout_page_count()) return 10000;
    uint32_t off = book_position_bytes(book_chapter_byte_offset(s_chapter), chapter_end(),
                                       book_layout_page_start_offset(page), s_text_len);
    return (unsigned)((uint64_t)off * 10000 / total);
}
static unsigned percent(size_t page) {
    return percent_basis_points(page) / 100;
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
    unsigned basis_points = percent_basis_points(s_page);
    book_progress_t p = {
        .file_size = s_file_size, .chapter = (uint16_t)s_chapter,
        .byte_off = (uint32_t)book_layout_page_start_offset(s_page),
        .px = (uint8_t)s_px, .pct = (uint8_t)(basis_points / 100),
        .pct_frac = (uint8_t)(basis_points % 100),
        .last_open_s = 0, .read_ms = s_read_ms, .pages = s_pages,
    };
    if (!pending->dirty || pending->value.file_size != p.file_size || pending->value.chapter != p.chapter ||
        pending->value.byte_off != p.byte_off || pending->value.px != p.px || pending->value.pct != p.pct ||
        pending->value.pct_frac != p.pct_frac ||
        pending->value.read_ms != p.read_ms || pending->value.pages != p.pages) {
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
static void invalidate_prep(void) {
    s_next_page = s_prep_page = -1;
    memset(&s_prep_cursor, 0, sizeof(s_prep_cursor));
}
static void free_image(void) {
    free(s_image_pixels); s_image_pixels = NULL;
    s_image_width = s_image_height = 0;
    s_image_block = SIZE_MAX; s_image_open = false;
    s_image_error = NULL; s_image_origin[0] = 0;
}
// 大图以灰阶整屏呈现；返回正文需一次全刷清残影，GL16 保白清不干净照片灰阶。
// 标记在 present 消费，随后的正文呈现恢复正常档位。
// The full-page image arrives in grays; returning to text needs one full refresh
// since GL16 cannot clear photo grays. present() consumes the flag and later body
// presentations return to their normal profile.
static bool s_full_settle;
static app_redraw_t close_image(void) {
    s_image_open = false;
    invalidate_prep();
    s_full_settle = true;
    return APP_REDRAW_PAGE;
}

/* ---- 绘制与预渲染 / Drawing and preparation ---- */
static char s_footer_status[48];
// 章节起点刻度：千分比位置，超过 128 章按步长抽稀。
// Chapter-start ticks at permille positions, subsampled by step beyond 128 chapters.
static void chapter_ticks_rebuild(void) {
    s_chapter_tick_count = 0;
    uint32_t total = book_total_bytes();
    size_t chapters = book_chapter_count();
    if (!total || chapters <= 1) return;
    unsigned step = chapters > 128 ? (unsigned)((chapters + 127) / 128) : 1;
    for (size_t i = step; i < chapters && s_chapter_tick_count < 128; i += step) {
        uint32_t at = book_chapter_byte_offset(i);
        if (!at || at >= total) continue;
        s_chapter_ticks[s_chapter_tick_count++] = (uint16_t)((uint64_t)at * 1000 / total);
    }
}
// 剩余时间使用本次会话向前读过的字节速度，避免短章节页密度与历史翻页计数放大误差。
// ETA uses forward byte throughput in this session, avoiding short-chapter density and historical turn-count bias.
static void refresh_time_left(void) {
    s_time_left[0] = 0;
    if (!app_settings_footer_time()) return;
    uint32_t total = book_total_bytes();
    if (!total || !s_text_len) return;
    uint32_t at = book_position_bytes(book_chapter_byte_offset(s_chapter), chapter_end(),
                                     book_layout_page_start_offset(s_page), s_text_len);
    book_eta_format(&s_eta, percent_basis_points(s_page) == 10000 ? 0 : total > at ? total - at : 0,
                    s_time_left, sizeof(s_time_left));
}
static void draw_reader_footer(uint8_t* fb, size_t page) {
    char status[96];
    footer_status(status, sizeof(status) - sizeof(s_time_left));
    if (s_time_left[0]) strcat(status, s_time_left);
    ui_product_reader_chrome(fb, s_save_failed ? "进度未保存，稍后重试" : s_title,
                             (unsigned)page + 1, book_layout_complete() ? (unsigned)book_layout_page_count() : 0,
                             percent(page), s_font_notice, status, app_settings_footer_bar(),
                             s_chapter_ticks, s_chapter_tick_count);
}
static void draw_reader(uint8_t* fb, size_t page) {
    ui_clear_page(fb);
    apply_typography();
    reader_body_font(true);
    book_layout_draw_page(fb, page, body_rect(), s_px);
    reader_body_font(false);
    draw_reader_footer(fb, page);
    if (s_toolbar) {
        if (s_toolbar_sizes) {
            ui_clear_rect_fast(fb, (EpdRect){UI_MARGIN - 16, 24, ui_content_width() + 32, 758});
            ui_text(fb, UI_MARGIN, 52, 24, "当前正文文字预览", EPD_DRAW_ALIGN_LEFT, false);
            EpdRect preview = ui_product_reader_preview_rect();
            EpdRect body = body_rect();
            preview.x = body.x; preview.width = body.width;
            reader_body_font(true);
            bool preview_drawn = book_layout_draw_preview(fb, s_panel_anchor, preview, s_px);
            reader_body_font(false);
            if (!preview_drawn)
                ui_product_title(fb, preview, "此处暂无可预览文字，返回阅读查看图文", 28, 2);
            ui_product_reader_sizes(fb, s_title, s_px, s_pressed_control);
        }
        else ui_product_reader_tools(fb, s_title, s_px, app_settings_book_night(), s_pressed_control);
        if (s_message[0]) {
            EpdRect notice = {UI_MARGIN, 864, ui_content_width(), 32};
            ui_clear_rect_fast(fb, notice);
            ui_product_title(fb, notice, s_message, 24, 1);
        }
    }
    ui_draw_menu_handle(fb, false);
}
// 只保留正文预绘；页脚在采用缓存时重画，避免跨分钟或字重调整留下旧状态。
// Prefetch only the body; repaint chrome on adoption to avoid stale time or stroke state.
static int prep_step(void) {
    if (!s_next_fb || !s_text || s_prep_page < 0) return -1;
    if (!s_prep_cursor.generation) { ui_clear_page(s_next_fb); apply_typography(); }
    reader_body_font(true);
    int result = book_layout_draw_slice(s_next_fb, (size_t)s_prep_page, body_rect(), s_px, &s_prep_cursor);
    reader_body_font(false);
    if (result > 0) s_next_page = s_prep_page;
    else if (result < 0) invalidate_prep();
    return result;
}
static void prep_task(void* arg) {
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        lock_draw();
        while (!atomic_load_explicit(&s_prep_stop, memory_order_acquire))
            if (prep_step() != 0) break;
        unlock_draw();
        xSemaphoreGive(s_prep_done);
    }
}
static void ensure_prep(void) {
    if (!s_draw_lock) s_draw_lock = xSemaphoreCreateMutex();
    if (!s_prep_done) s_prep_done = xSemaphoreCreateBinary();
    // 给章节、插图和无线任务留余量；加速缓冲分配失败时正常同步绘图。
    // Reserve chapter, image and radio headroom; synchronous drawing survives allocation failure.
    size_t need = fb_bytes();
    if (!s_next_fb && heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) > need + 1024u * 1024u)
        s_next_fb = heap_caps_aligned_alloc(16, need, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_prep_task && s_draw_lock && s_prep_done && s_next_fb) {
        if (xTaskCreatePinnedToCore(prep_task, "book_prep", 12 * 1024, NULL, 3, &s_prep_task, 1) != pdPASS)
            s_prep_task = NULL;
    }
}
// 页面回调返回前都会收齐预绘，设置页触发同步时任务正阻塞；直接销毁可立即归还内部栈。
// Page callbacks always join prefetch before returning, so the task is blocked when settings starts sync; deleting it immediately returns its internal stack.
static void prep_suspend_for_sync(void) {
    if (s_prep_sync_suspended) return;
    invalidate_prep();
    if (s_prep_task) { vTaskDelete(s_prep_task); s_prep_task = NULL; }
    s_prep_sync_suspended = true;
}
static void prep_resume_after_sync(void) {
    if (!s_prep_sync_suspended) return;
    s_prep_sync_suspended = false;
    // 保留刚归还的连续内部块；空闲分片仍可同步预绘，后台任务等下次真实推屏再按需创建。
    // Keep the newly returned contiguous internal block; idle slices still prefetch synchronously, and the worker is recreated on the next real presentation.
}
static bool prep_allowed(void) {
    return s_view == READING && s_text && !s_image_open && !s_toolbar && !s_clear_confirm &&
        !s_quote_selecting && !s_ended && s_page + 1 < book_layout_page_count() &&
        !book_layout_page_has_images(s_page) && !book_layout_page_has_images(s_page + 1);
}
static bool prep_target(void) {
    if (!prep_allowed()) return false;
    int target = (int)s_page + 1;
    if (s_prep_page != target || s_prep_cursor.generation != book_layout_generation()) {
        invalidate_prep(); s_prep_page = target;
    }
    return s_next_page != target;
}
static bool kick_prep(void) {
    if (!prep_target()) return false;
    if (!s_prep_task) ensure_prep();
    if (!s_prep_task) return false;
    xSemaphoreTake(s_prep_done, 0);
    atomic_store_explicit(&s_prep_stop, false, memory_order_release);
    xTaskNotifyGive(s_prep_task);
    return true;
}
static void prep_end(bool started) {
    if (!started) return;
    atomic_store_explicit(&s_prep_stop, true, memory_order_release);
    // 返回主循环前交还字体和章节；只等当前一行，不等整页。
    // Return fonts and chapters before the event loop resumes; wait for one row, never a whole page.
    xSemaphoreTake(s_prep_done, portMAX_DELAY);
}
static bool prep_resume(size_t page) {
    if (!s_next_fb || s_prep_page != (int)page || !s_prep_cursor.started ||
        s_prep_cursor.generation != book_layout_generation()) return false;
    while (prep_step() == 0) {}
    return s_next_page == (int)page;
}
static void prep_idle(app_ctx_t* ctx) {
    if (ctx->consumed || ctx->request_app || ctx->request_menu ||
        (ctx->touch && ctx->touch->touched) || !prep_target()) return;
    if (!s_next_fb) ensure_prep();
    if (!s_next_fb) return;
    int64_t deadline = esp_timer_get_time() + 3000;
    lock_draw();
    for (unsigned i = 0; i < 4; ++i)
        if (prep_step() != 0 || esp_timer_get_time() >= deadline) break;
    unlock_draw();
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
        // 大图查看与书内插图、TF 查看器同走 16 级有序抖动。/ The full-page viewer shares
        // the 16-level ordered dither with inline illustrations and the TF viewer.
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            size_t src = (size_t)((int64_t)y * s_image_height / height) * s_image_width +
                         (size_t)((int64_t)x * s_image_width / width);
            epd_draw_pixel(left + x, y0 + y, ui_image_dither(s_image_pixels[src], left + x, y0 + y), fb);
        }
    } else ui_text(fb, area.x, area.y + area.height / 2, UI_PX_CAPTION,
                   s_image_error ? s_image_error : "图片不可用", EPD_DRAW_ALIGN_LEFT, false);
    ui_draw_button(fb, ui_bar_rect(0, 1), "返回正文", false);
    ui_draw_menu_handle(fb, false);
}
/* ---- 书架封面缓存 / Shelf cover cache ---- */
// 最多六张原生尺寸封面；失败槽三秒后重试，不把临时失败固定为无封面。
// Keep six native covers; retry failed slots after three seconds instead of freezing transient errors as absence.
typedef struct {
    char path[BOOK_STORE_PATH_MAX];
    uint8_t* gray;
    bool failed;
    int64_t retry_ms;
} cover_slot_t;
static cover_slot_t s_cover_slots[6];
static bool s_covers_dirty, s_cover_batch_grid;
static int s_cover_batch_leaf = -1;
static int64_t s_cover_batch_due;
static int s_cover_prefetch_leaf = -1, s_cover_prefetch_pos;
// 进页后仅记录一次空闲门控，诊断不触发重绘。/ Log idle gating once after entry without requesting a redraw.
static bool s_cover_diag_pending;
static void covers_reset(void) {
    s_covers_dirty = false; s_cover_batch_leaf = -1;
    s_cover_prefetch_leaf = -1; s_cover_prefetch_pos = 0;
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
        if (!strcmp(s_cover_slots[i].path, path)) return !s_cover_slots[i].failed || esp_timer_get_time() / 1000 < s_cover_slots[i].retry_ms;
    return false;
}
static bool cover_visible(const char* path, int leaf) {
    for (int row = 0; row < shelf_rows(); ++row) {
        int i = leaf * shelf_rows() + row;
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
    slot->retry_ms = esp_timer_get_time() / 1000 + 3000;
}
static bool is_epub_entry(const shelf_entry_t* entry) {
    const char* dot = strrchr(entry->name, '.');
    return dot && !strcasecmp(dot, ".epub");
}

// 翻页首帧先从复用层恢复成品；未完成的后台项留给tick继续，不阻塞触摸或按键。
// Restore prepared covers before the first frame after shelf paging. A pending
// background item stays with the tick path and never blocks touch or keys.
static void shelf_cover_restore(app_ctx_t* ctx) {
    s_covers_dirty = false;
    s_cover_batch_leaf = ctx->leaf;
    s_cover_batch_grid = s_shelf_grid;
    s_cover_prefetch_leaf = ctx->leaf;
    s_cover_prefetch_pos = 0;
    for (int row = 0; row < shelf_rows(); ++row) {
        int i = ctx->leaf * shelf_rows() + row;
        if (i >= s_visible_count) break;
        if (s_shelf[i].removed || cover_tried(s_shelf[i].path)) continue;
        uint8_t* gray = NULL;
        if (!book_cover_poll(s_shelf[i].path, &gray)) break;
        cover_store(s_shelf[i].path, gray, ctx->leaf);
    }
}

// 当前页稳定后逐张预取下一页、再预取上一页；18槽恰好覆盖网格模式三页。
// Once the current page settles, prefetch one item at a time from the next page,
// then the previous page. Eighteen slots cover all three grid pages exactly.
static void shelf_cover_prefetch(app_ctx_t* ctx) {
    if (s_cover_prefetch_leaf != ctx->leaf) {
        s_cover_prefetch_leaf = ctx->leaf;
        s_cover_prefetch_pos = 0;
    }
    int rows = shelf_rows();
    while (s_cover_prefetch_pos < rows * 2) {
        int pos = s_cover_prefetch_pos;
        int leaf = pos < rows ? ctx->leaf + 1 : ctx->leaf - 1;
        int row = pos % rows;
        if (leaf < 0 || leaf >= leaves()) { ++s_cover_prefetch_pos; continue; }
        int i = leaf * rows + row;
        if (i >= s_visible_count || s_shelf[i].removed) { ++s_cover_prefetch_pos; continue; }
        uint8_t* gray = NULL;
        if (!book_cover_poll(s_shelf[i].path, &gray)) return;
        free(gray);
        ++s_cover_prefetch_pos;
        return;
    }
}

static void draw_product_shelf(app_ctx_t* ctx, uint8_t* fb) {
    ui_clear_page(fb);
    char subtitle[96];
    snprintf(subtitle, sizeof(subtitle), "%d 本图书 · %d / %d 页", s_visible_count, ctx->leaf + 1, leaves());
    ui_product_header(fb, "书架", subtitle);
    draw_control(fb, (EpdRect){470, 68, 174, 68}, "导入", 114);
    draw_control(fb, shelf_filter_rect(0), s_filter == 0 ? "全部来源" : s_filter == 1 ? "TF 卡" : "内置", 110);
    draw_control(fb, shelf_filter_rect(1), s_recent_sort ? "按最近" : "按名称", 111);
    draw_control(fb, shelf_filter_rect(2), s_query[0] ? "搜索中" : "搜索", 112);
    const char* notice = s_save_failed ? "进度未保存：点此重试" : s_message[0] ? s_message : s_shelf_warning;
    ui_product_shelf_layout(fb, s_shelf_grid);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 250, ui_content_width(), 34}, notice, 24, 1);
    for (int row = 0; row < shelf_rows(); ++row) {
        int i = ctx->leaf * shelf_rows() + row;
        if (i >= s_visible_count) break;
        char title[256], meta[96]; copy_text(title, sizeof(title), s_shelf[i].name);
        char* dot = strrchr(title, '.'); if (dot) *dot = 0;
        if (s_shelf[i].removed) snprintf(meta, sizeof(meta), "已删除 · 进度待清理");
        else if (s_shelf[i].has_progress) snprintf(meta, sizeof(meta), "已读 %u%%", s_shelf[i].pct);
        else snprintf(meta, sizeof(meta), "未开始");
        if (s_shelf_grid) ui_product_shelf_grid_card(fb, row_rect(row, false), title, meta,
                          is_epub_entry(&s_shelf[i]) ? "EPUB" : "TXT", s_shelf[i].pct,
                          s_shelf[i].has_progress, s_pressed_control == row,
                          s_shelf[i].removed ? NULL : cover_for(s_shelf[i].path));
        else ui_product_shelf_card(fb, ui_product_shelf_rect(row), title, meta, s_shelf[i].pct,
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
static EpdRect quote_button(int i) { return ui_product_excerpt_button(i); }
static bool choose_quote(uint16_t x, uint16_t y) {
    lock_draw();
    size_t hit = book_layout_text_at(s_page, body_rect(), x, y), start, end;
    bool ok = hit != SIZE_MAX && book_quote_sentence(s_text, s_text_len, hit, &start, &end);
    unlock_draw();
    if (!ok) return false;
    s_quote = (book_quote_t){.chapter = s_chapter, .byte_off = start, .end_off = end, .pct = percent(s_page)};
    copy_text(s_quote.path, sizeof(s_quote.path), s_path);
    // 锁屏来源要显示章节名：保存时快照当前章节标题，取不到则留空回退仅书名。
    // Lock sources show the chapter: snapshot the current title here, leaving it empty to fall back to book-only.
    if (book_chapter_title(s_chapter, s_quote.chapter_title, sizeof(s_quote.chapter_title)) != ESP_OK)
        s_quote.chapter_title[0] = 0;
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
    ui_product_excerpt_confirm(fb, s_quote.text, s_book_title, s_quote_notice);
}
static void draw_key_hints(uint8_t* fb) {
    if (s_quote_selecting) ui_product_key_hints(fb, "取消", "取消 / 长按导航", "保存");
    else if (s_image_open || (s_view == READING && s_ended)) ui_product_key_hints(fb, "关闭", "关闭 / 长按导航", "关闭");
    else if (s_resume_path[0]) ui_product_key_hints(fb, "留在书架", "导航", "继续阅读");
    else if (s_mark_delete >= 0) ui_product_key_hints(fb, "取消", "取消 / 长按导航", "—");
    else if (s_clear_confirm && s_text) ui_product_key_hints(fb, "—", "长按导航", "—");

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
                    fit_text(name, UI_PX_BTN, r.width - 2 * UI_PAD - 64 - ttf_text_width_px(UI_PX_CAPTION, mark) - UI_GAP);
                    if (s_pressed_control == row) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
                    ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
                    ui_draw_choice_round_rect(fb, (EpdRect){r.x + UI_PAD, r.y + (r.height - 42) / 2, 42, 42}, 6, s_shelf[i].selected);
                    ui_text_vc(fb, r.x + UI_PAD + 64, r.y + r.height / 2, UI_PX_BTN, name, EPD_DRAW_ALIGN_LEFT, false);
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
    draw_key_hints(fb);
    finish_reader_frame(ctx, fb);
    unlock_draw();
}
static bool present(app_ctx_t* ctx, app_redraw_t redraw) {
    if (redraw == APP_REDRAW_NONE || redraw == APP_REDRAW_DONE) return true;
    int64_t start = esp_timer_get_time();
    const bool search_edit = s_view == SEARCH && s_search_fast;
    if (redraw == APP_REDRAW_PAGE || redraw == APP_REDRAW_FULL) render(ctx, ctx->fb);
    if (s_full_settle && (redraw == APP_REDRAW_PAGE || redraw == APP_REDRAW_FULL)) {
        s_full_settle = false;
        s_presented_view = s_view;
        s_presented_reading_overlay = false;
        s_presented_valid = true;
        guard_draw_result(ctx->hl, update_display_full(ctx->hl));
        return true;
    }
    // 初次进正文也提交真实黑白帧，避免入口灰阶与后续DU目标不同。
    // Body entries also commit actual black/white frames so entry gray never differs from subsequent DU targets.
    if (s_view == READING && s_text && !s_image_open && !s_toolbar && !s_clear_confirm && !s_quote_selecting && !s_ended && reader_direct_enabled() && !reader_gray_direct_enabled()) {
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
    const bool white_repaint_body = s_view == READING && s_text && !reading_overlay && reader_white_repaint_enabled();
    const bool gray_direct = reader_gray_direct_enabled();
    if (redraw == APP_REDRAW_FULL) {
        s_text_turn = false;
        err = white_repaint_body ? update_display_night_white_repaint(ctx->hl, DISPLAY_NIGHT_CLEAN, reader_direct_enabled(), gray_direct) :
            s_reader_target_night ? update_display_clean(ctx->hl) : update_display_full(ctx->hl);
    }
    else if ((body_entry || view_entry || overlay_exit) && APP_PAGE_REFRESH_MODE == MODE_GL16) {
        s_text_turn = false;
        display_request_navigation_settle();
        err = white_repaint_body ? update_display_night_white_repaint(ctx->hl, DISPLAY_NIGHT_ENTRY, reader_direct_enabled(), gray_direct) :
            update_display_with(ctx->hl, s_reader_target_night ? &E0470_TEXTTURN_NIGHT_WAVEFORM : &E0470_NAVIGATION_WAVEFORM, MODE_GL16);
    }
    else if (redraw == APP_REDRAW_AREA) {
        // 正文单遍快速翻页，控件仅局推。/ Present body turns once with the fast profile; controls stay local.
        if (s_text_turn) {
            s_text_turn = false;
            bool direct = reader_direct_enabled();
            if (direct) s_reader_target_binary = !gray_direct;
            // 产品日间正文统一累计清理周期；临时夜间机制保留原入口。
            // Product day body turns share cleanup counting; dormant night paths retain their original entries.
            if (!s_reader_target_night) {
                display_reader_mode_t profile = gray_direct ? (reader_gray_balanced_enabled() ?
                    DISPLAY_READER_GRAY_BALANCED : DISPLAY_READER_GRAY) : direct ? DISPLAY_READER_BINARY :
                    app_settings_book_turn_anim() && !s_image_open && !s_search_fast &&
                    (app_settings_book_turn_style() != 2 || s_soft_turn_eligible) ? DISPLAY_READER_ANIMATED : DISPLAY_READER_STANDARD;
                err = update_display_reader_turn(ctx->hl, profile, s_area, s_turn_dir > 0);
            } else err = white_repaint_body ? update_display_night_white_repaint(ctx->hl, DISPLAY_NIGHT_TURN, direct, gray_direct) :
                gray_direct ? update_display_text_gray_direct(ctx->hl, app_settings_book_night()) :
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
    else if (white_repaint_body)
        err = update_display_night_white_repaint(ctx->hl, DISPLAY_NIGHT_REDRAW, reader_direct_enabled(), gray_direct);
    else if (APP_PAGE_REFRESH_MODE == MODE_GL16)
        err = update_display_with(ctx->hl, s_reader_target_night ? &E0470_TEXTTURN_NIGHT_WAVEFORM : &E0470_NAVIGATION_WAVEFORM, MODE_GL16);
    else err = update_display_mode(ctx->hl, APP_PAGE_REFRESH_MODE);
    int64_t displayed = esp_timer_get_time();
    prep_end(prep);
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
    bool cached = !s_toolbar && !s_clear_confirm && !s_quote_selecting && s_next_fb &&
        ((s_next_page == (int)s_page && s_prep_cursor.generation == book_layout_generation()) || prep_resume(s_page));
    if (cached)
        memcpy(ctx->fb, s_next_fb, fb_bytes());
    else draw_reader(ctx->fb, s_page);
    // 预渲染的底栏可能跨分钟；翻页呈现前重画当前状态。/ A prepared footer may cross a minute; repaint current status before the turn.
    if (cached) {
        ui_clear_rect_fast(ctx->fb, ui_bar_rect(0, 1));
        draw_reader_footer(ctx->fb, s_page);
        ui_draw_menu_handle(ctx->fb, false);
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

/* ---- 页脚定位条 / Footer scrub ---- */
// 进入位置换算全书千分比：章内字节按章长比例折回全书字节轴，与 jump_to_bytes 同一映射。
// Convert the current position to a book-wide permille: chapter bytes fold back onto the
// whole-book byte axis at the chapter's ratio — the same mapping jump_to_bytes applies.
static unsigned scrub_entry_frac(void) {
    uint32_t total = book_total_bytes();
    if (!total) return 0;
    uint32_t start = book_chapter_byte_offset(s_chapter), end = chapter_end();
    uint64_t within = end > start && s_text_len
        ? (uint64_t)book_layout_page_start_offset(s_page) * (end - start) / s_text_len : 0;
    uint64_t at = (uint64_t)start + within;
    return (unsigned)(at * 1000u / total);
}
static bool scrub_ready(void) {
    return s_view == READING && s_text && !s_ended && !s_toolbar && !s_clear_confirm &&
           !s_image_open && !s_quote_selecting && !s_scan_pending && !s_resume_path[0] &&
           book_total_bytes() > 0;
}
// 轨道横跨页脚内容区两端各留 16px；手柄中心即目标位置。/ The track spans the footer
// content with 16px margins; the handle center marks the target.
static int scrub_track_x0(void) { return ui_bar_rect(0, 1).x + 16; }
static int scrub_track_x1(void) { EpdRect bar = ui_bar_rect(0, 1); return bar.x + bar.width - 16; }
static unsigned scrub_frac_at(int x) {
    if (x < scrub_track_x0()) return 0;
    if (x > scrub_track_x1()) return 1000;
    return (unsigned)((int64_t)(x - scrub_track_x0()) * 1000 / (scrub_track_x1() - scrub_track_x0()));
}
static bool s_scrub;
static unsigned s_scrub_frac, s_scrub_entry;
static int s_scrub_paint_x = -1;
// 定位条只用黑白：DU 推送灰阶不可靠，文字与轨道都保持二值。
// The scrub stays pure black and white: DU pushes carry unreliable grays,
// so text and track remain binary.
static void scrub_paint(app_ctx_t* ctx) {
    EpdRect bar = ui_bar_rect(0, 1);
    lock_draw();
    reader_patch_t patch = reader_patch_begin(ctx);
    ui_clear_rect_fast(ctx->fb, bar);
    int x0 = scrub_track_x0(), x1 = scrub_track_x1();
    epd_draw_rect((EpdRect){x0, bar.y + 64, x1 - x0, 8}, UI_GRAY_BLACK, ctx->fb);
    int ex = x0 + (int)((int64_t)(x1 - x0) * s_scrub_entry / 1000);
    epd_fill_rect((EpdRect){ex - 2, bar.y + 58, 4, 20}, UI_GRAY_BLACK, ctx->fb);
    int hx = x0 + (int)((int64_t)(x1 - x0) * s_scrub_frac / 1000);
    if (hx - 14 < x0) hx = x0 + 14;
    if (hx + 14 > x1) hx = x1 - 14;
    epd_fill_rect((EpdRect){hx - 14, bar.y + 52, 28, 32}, UI_GRAY_BLACK, ctx->fb);
    char label[24];
    snprintf(label, sizeof(label), "%u%%", s_scrub_frac / 10);
    ui_text(ctx->fb, bar.x + bar.width / 2, bar.y + 6, 30, label, EPD_DRAW_ALIGN_CENTER, false);
    ui_text(ctx->fb, bar.x + bar.width / 2, bar.y + 40, 20, "拖动定位 · 松开跳转 · 点按取消", EPD_DRAW_ALIGN_CENTER, false);
    reader_patch_end(ctx, patch);
    unlock_draw();
    s_scrub_paint_x = hx;
    guard_draw_result(ctx->hl, update_display_area_with(ctx->hl, &E0470_WAVEFORM, MODE_DU, bar));
}
static void scrub_begin(app_ctx_t* ctx, int x) {
    s_scrub = true;
    s_scrub_entry = scrub_entry_frac();
    s_scrub_frac = scrub_frac_at(x);
    s_scrub_paint_x = -1;
    scrub_paint(ctx);
}
// 拖动跟手：手柄位移不足 4px 不重刷，DU 每步只推页脚带。
// Finger tracking: handle moves under 4px skip repaints; each DU step pushes the footer band only.
static void scrub_move(app_ctx_t* ctx, int x) {
    s_scrub_frac = scrub_frac_at(x);
    int x0 = scrub_track_x0(), x1 = scrub_track_x1();
    int hx = x0 + (int)((int64_t)(x1 - x0) * s_scrub_frac / 1000);
    if (s_scrub_paint_x >= 0 && hx > s_scrub_paint_x - 4 && hx < s_scrub_paint_x + 4) return;
    scrub_paint(ctx);
}
// 松开或滑动收尾：按全书字节跳转，复用目录百分比跳页的落点规则。
// Release or swipe finishes: jump by whole-book bytes, reusing the TOC percent-jump landing rule.
static app_redraw_t scrub_commit(app_ctx_t* ctx, int x) {
    s_scrub = false;
    uint32_t total = book_total_bytes();
    if (!total) return APP_REDRAW_PAGE;
    uint32_t target = (uint64_t)total * scrub_frac_at(x) / 1000;
    return jump_to_bytes(ctx, target);
}
// 取消恢复页脚；与分钟更新同路：补丁后清带重画，GL16 静默推屏。
// Cancel restores the footer; the minute-update route: patch, clear, redraw,
// then a quiet GL16 push.
static void scrub_restore(app_ctx_t* ctx) {
    s_scrub = false;
    lock_draw();
    reader_patch_t patch = reader_patch_begin(ctx);
    EpdRect bar = ui_bar_rect(0, 1);
    ui_clear_rect_fast(ctx->fb, bar);
    draw_reader_footer(ctx->fb, s_page);
    reader_patch_end(ctx, patch);
    unlock_draw();
    guard_draw_result(ctx->hl, update_display_area_quiet(ctx->hl, bar));
}
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
    s_eta_since = 0;
    // 跳转/章节切换同样计读并重置翻页时钟；紧接着的 turn_page 二次调用只见到零间隔。
    // Jumps and chapter switches account dwell too and reset the turn clock; the
    // immediately following turn_page call only sees a zero gap.
    if (s_last_turn_ms > 0 && ctx->now_ms > s_last_turn_ms) {
        int64_t dwell = ctx->now_ms - s_last_turn_ms;
        if (dwell >= 3000) {
            if (dwell > 120000) dwell = 120000;
            s_read_ms += (uint32_t)dwell;
            ++s_pages;
        }
    }
    s_last_turn_ms = ctx->now_ms;

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
    chapter_ticks_rebuild();
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
    // 大书无论命中与否都提示：命中路径仍要整本读指纹，静默等待会被当成点按失效。
    // Always notice big books: the cache-hit path still fingerprints the whole file,
    // and a silent wait reads as an ignored tap.
    if (!book_cached(path) && st.st_size > 1024 * 1024)
        loading_detail(ctx, "正在解析图书…", "大书需要更多时间，请稍候");
    // 持绘制锁释放下一页缓存，降低解析峰值。/ Release the next-page buffer under the drawing lock to reduce parsing peaks.
    lock_draw(); free(s_next_fb); s_next_fb = NULL; unlock_draw();
    // 开书是事务：先落启动状态再解析，断电后下次开机不再自动重提这本书。
    // Opening is a transaction: persist startup state before parsing; a power cut stops automatic re-offers next boot.
    if (pico_boot_book_begin(path) != ESP_OK) {
        copy_text(s_message, sizeof(s_message), "无法记录启动状态，请重试");
        return false;
    }
    int64_t open_started = esp_timer_get_time();
    esp_err_t err = book_open(path);
    pico_boot_book_end(err == ESP_OK);
    int64_t source_ms = (esp_timer_get_time() - open_started) / 1000;
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
    // v3 阅读统计随进度恢复；旧记录从零累计，首翻页不计时钟。
    // v3 reading stats restore with progress; older records accumulate from zero and the first turn starts the clock.
    s_read_ms = p.read_ms;
    s_pages = p.pages;
    memset(&s_eta, 0, sizeof(s_eta)); s_eta_since = 0;
    s_time_left[0] = 0;
    s_last_turn_ms = 0;
    if (s_px < BOOK_PX_MIN || s_px > BOOK_PX_MAX) s_px = 48;
    bool positioned = s_entry_active && s_entry.has_position && !strcmp(path, s_entry.path);
    if (positioned && s_entry.chapter >= book_chapter_count()) {
        copy_text(s_message, sizeof(s_message), "摘录所在章节已失效"); free_book(); return false;
    }
    size_t chapter = positioned ? s_entry.chapter : resume && p.chapter < book_chapter_count() ? p.chapter : 0;
    uint32_t target = 0;
    if (!positioned && resume && p.approximate) {
        target = (uint64_t)book_total_bytes() * ((uint32_t)p.pct * 100 + p.pct_frac) / 10000;
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
        cover_offset(off);
        s_page = book_layout_page_for_offset(off);
    }
    s_view = READING;
    s_mark_delete = -1; s_mark_manage = s_toolbar_sizes = false; s_layout_group = 0;
    s_choice_row = -1;
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
    s_layout_leaf = -1;
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
// 从阅读直达书架：目录无效时（如开机直开续读从未进入过书架）补上进入扫描，
// 不依赖底部页签重进本页。
// Enter the shelf straight from reading: kick the entry scan when the catalog is
// invalid (e.g. boot resumed into a book without any shelf visit) instead of
// requiring a tab re-entry.
static void shelf_enter_catalog(app_ctx_t* ctx) {
    if (s_catalog_valid && s_catalog_revision == book_store_revision()) return;
    covers_reset();
    s_count = s_visible_count = 0;
    ctx->leaf = 0;
    copy_text(s_storage, sizeof(s_storage), "正在检测存储…");
    copy_text(s_message, sizeof(s_message), "正在扫描图书…");
    read_pico_sd_start_probe();
    s_scan_pending = true;
    s_store_revision = book_store_revision();
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
    // 覆盖层护栏：定位条、摘录确认与读完面板期间的翻页请求一律拒绝，先收覆盖层。
    // Overlay guard: turn requests during scrub, excerpt confirmation or the end panel
    // are rejected outright; overlays close first.
    if (!s_text || s_clear_confirm || s_scrub || s_quote_selecting || s_ended) return APP_REDRAW_NONE;
    bool source_text_only = !book_layout_page_has_images(s_page);
    s_soft_turn_eligible = false;
    s_ended = false;
    if (dir > 0 && s_page + 1 >= book_layout_page_count() && !book_layout_complete()) {
        lock_draw();
        // 追加页表不改变已完成页面，保留其尚未完成的预绘。
        // Appending pagination never changes completed pages; retain their partial prefetch.
        bool ok = book_layout_extend(2);
        unlock_draw();
        if (!ok) {
            free_book(); s_view = SHELF; ctx->leaf = 0;
            copy_text(s_message, sizeof(s_message), "排版失败：内存不足或章节过长");
            return APP_REDRAW_PAGE;
        }
    }
    int64_t eta_dwell = s_eta_since > 0 ? ctx->now_ms - s_eta_since : 0;
    uint32_t eta_before = book_position_bytes(book_chapter_byte_offset(s_chapter), chapter_end(),
                                            book_layout_page_start_offset(s_page), s_text_len);
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
    if (!changed) {
        s_view = s_text ? TOC : SHELF;
        if (!s_text) shelf_enter_catalog(ctx);
        ctx->leaf = s_text && s_toc_tab == 0 ? (int)(s_chapter / BOOK_TOC_ROWS) : 0;
        return APP_REDRAW_PAGE;
    }
    s_toolbar = false;
    // 翻页计读：单页停留 3s 起计、120s 截断，防秒翻与挂机污染均值。
    // Page-dwell accounting: counts from 3s and clamps at 120s so quick flips and idle hangs cannot pollute the average.
    if (s_last_turn_ms > 0 && ctx->now_ms > s_last_turn_ms) {
        int64_t dwell = ctx->now_ms - s_last_turn_ms;
        if (dwell >= 3000) {
            if (dwell > 120000) dwell = 120000;
            s_read_ms += (uint32_t)dwell;
            ++s_pages;
        }
    }
    s_last_turn_ms = ctx->now_ms;
    if (dir > 0) {
        uint32_t after = book_position_bytes(book_chapter_byte_offset(s_chapter), chapter_end(),
                                            book_layout_page_start_offset(s_page), s_text_len);
        book_eta_sample(&s_eta, eta_dwell, eta_before, after);
    }
    s_eta_since = ctx->now_ms;
    refresh_time_left();
    s_turn_dir = dir < 0 ? -1 : 1;
    s_soft_turn_eligible = source_text_only && !book_layout_page_has_images(s_page);
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
    size_t off = s_toolbar_sizes ? s_panel_anchor : book_layout_page_start_offset(s_page);
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
    if (app) {
        if (s_text && s_path[0]) {
            s_font_return = (font_return_t){.active = true, .sizes = s_toolbar_sizes, .chapter = (uint16_t)s_chapter,
                .offset = (uint32_t)((s_toolbar_sizes || s_view == LAYOUT) ? s_panel_anchor : book_layout_page_start_offset(s_page)),
                .view = s_view, .group = s_layout_group};
            copy_text(s_font_return.path, sizeof(s_font_return.path), s_path);
        }
        save_progress(); ctx->request_app = app;
    }
}

/* ---- 排版菜单 / Typography menu ---- */
#define BOOK_LAYOUT_ROWS 14
enum { SETTINGS_TEXT, SETTINGS_INPUT, SETTINGS_DISPLAY, SETTINGS_SYNC, SETTINGS_EXPERIMENTS };
// 行标识稳定，分组与位置独立于已保存设置值。/ Stable row IDs; groups/positions are independent of saved values.
static const uint8_t k_layout_groups[BOOK_LAYOUT_ROWS] = {0, 0, 0, 0, 0, 0, 0, 1, 4, 1, 4, 2, 1, 0};
static bool layout_visible(int i) { return k_layout_groups[i] == s_layout_group; }
static EpdRect layout_tab_rect(int i) { return ui_row_rect(i, 4, 184, 80); }
// 阅读内只显示同步操作，账号详情留在系统设置；绘制与手势共用边界。
// Reader settings show sync actions only; account details stay in system settings, with shared paint and gesture bounds.
static EpdRect sync_control_rect(int i) { return (EpdRect){UI_MARGIN, 292 + i * 112, ui_content_width(), 96}; }
static EpdRect direct_rect(void) { return ui_row_rect(0, 2, 292, 96); }
static EpdRect speed_rect(void) { return ui_row_rect(1, 2, 292, 96); }
static EpdRect gray_balance_rect(void) { return (EpdRect){UI_MARGIN, 474, ui_content_width(), 58}; }
static EpdRect edge_gray_rect(void) {
    return (EpdRect){UI_MARGIN, 548, ui_content_width(), 84};
}
static EpdRect layout_row_rect(int i) {
    if (s_layout_group == SETTINGS_DISPLAY) return (EpdRect){UI_MARGIN, 756, ui_content_width(), 96};
    int row = 0;
    for (int j = 0; j < i; ++j) if (layout_visible(j)) ++row;
    int step = s_layout_group == SETTINGS_TEXT ? 88 : 112;
    return (EpdRect){UI_MARGIN, 292 + row * step, ui_content_width(), s_layout_group == SETTINGS_TEXT ? 76 : 96};
}
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
        case 8: return s_shake_enabled ? "开" : "关";
        case 9: return app_settings_book_auto() == 0 ? "关" : app_settings_book_auto() == 1 ? "20 秒" :
                       app_settings_book_auto() == 2 ? "40 秒" : "90 秒";
        // 揭页动画开时仅日间标准文字翻页走错相波前；其余翻页路径不变。
        // Enabled, only day standard text turns take the staggered wavefront; other turn paths unchanged.
        case 10: return app_settings_book_turn_style() == 2 ? "柔化对照" : app_settings_book_turn_anim() ? "原有动画" : "关";
        case 12: return "晃动 / 动画 ›";
        default:
            // 第 11 行的值由 draw 处按档位现拼。/ Row 11's value is composed by the draw path.
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
    ui_draw_button(fb, (EpdRect){UI_MARGIN, 710, ui_content_width(), 76}, "恢复默认分区", false);
    ui_draw_button(fb, (EpdRect){UI_MARGIN, 796, ui_content_width(), 76}, app_settings_book_tap() ? "点击翻页：开启" : "点击翻页：关闭", app_settings_book_tap());
    ui_product_title(fb, (EpdRect){UI_MARGIN, 894, ui_content_width(), 42},
                     "默认：左列上页 · 中列菜单 · 右列下页", 22, 1);
    ui_draw_button(fb, ui_bar_rect(0, 2), "返回设置", false);
    ui_draw_button(fb, ui_bar_rect(1, 2), "返回阅读", false);
}
// 可枚举设置统一走选择面板，一次点定；字体、点击分区与实验入口仍进入各自的专用页面。
// Enumerated settings share the one-tap choice sheet; font, tap-zone and experiment
// entries continue into their dedicated pages.
static bool layout_choice_row(int row) {
    return (row >= 1 && row <= 6) || row == 8 || row == 9 || row == 10 || row == 11;
}
static const char* const k_leading_labels[] = {"标准", "舒展", "宽松"};
static const char* const k_margin_labels[] = {"标准", "宽", "更宽"};
static const char* const k_indent_labels[] = {"关", "两字符"};
static const char* const k_para_labels[] = {"标准", "加大"};
static const char* const k_guide_labels[] = {"关", "实线", "虚线"};
static const char* const k_align_labels[] = {"左", "居中", "两端"};
static const char* const k_switch_labels[] = {"关", "开"};
static const char* const k_animation_labels[] = {"关闭", "原有动画", "柔化对照"};
static const char* const k_auto_labels[] = {"关", "20 秒", "40 秒", "90 秒"};
static const char* const k_gc_labels[] = {"关", "3 次", "5 次", "10 次", "14 次", "20 次", "30 次"};
static const uint8_t k_leading_values[] = {0, 15, 30};
static int layout_choice_count(int row) {
    if (row == 3 || row == 4 || row == 8) return 2;
    return row == 9 ? 4 : row == 11 ? (int)(sizeof(k_gc_steps) / sizeof(k_gc_steps[0])) : 3;
}
static const char* const* layout_choice_labels(int row) {
    switch (row) {
        case 1: return k_leading_labels;
        case 2: return k_margin_labels;
        case 3: return k_indent_labels;
        case 4: return k_para_labels;
        case 5: return k_guide_labels;
        case 6: return k_align_labels;
        case 8: return k_switch_labels;
        case 10: return k_animation_labels;
        case 9: return k_auto_labels;
        default: return k_gc_labels;
    }
}
static int layout_choice_current(int row) {
    switch (row) {
        case 1: { int v = app_settings_book_leading(); return v >= 30 ? 2 : v >= 15 ? 1 : 0; }
        case 2: return (int)app_settings_book_margin();
        case 3: return app_settings_book_indent() ? 1 : 0;
        case 4: return (int)app_settings_book_para();
        case 5: return (int)app_settings_book_guide();
        case 6: return (int)app_settings_book_align();
        case 8: return s_shake_enabled ? 1 : 0;
        case 9: return (int)app_settings_book_auto();
        case 10: return app_settings_book_turn_style();
        default: {
            uint8_t every = app_settings_gc_every();
            for (unsigned i = 0; i < sizeof(k_gc_steps) / sizeof(k_gc_steps[0]); ++i)
                if (k_gc_steps[i] == every) return (int)i;
            return 0;
        }
    }
}
static void draw_layout_menu(uint8_t* fb) {
    ui_clear_page(fb);
    ui_product_header(fb, s_layout_group == SETTINGS_EXPERIMENTS ? "实验功能" : "更多设置",
                      "调整即时保存，返回仍在当前阅读位置");
    const char* tabs[] = {"文字排版", "翻页操作", "显示", "进度同步"};
    for (int i = 0; i < 4; ++i) {
        EpdRect r = layout_tab_rect(i);
        bool selected = i == (s_layout_group == SETTINGS_EXPERIMENTS ? SETTINGS_INPUT : s_layout_group);
        if (s_pressed_control == 820 + i) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
        else if (selected) ui_draw_selected_round_rect(fb, r, UI_BTN_RADIUS);
        else ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
        ui_text_vc(fb, r.x + r.width / 2, r.y + r.height / 2, 24, tabs[i], EPD_DRAW_ALIGN_CENTER, false);
    }
    if (s_layout_group == SETTINGS_SYNC) {
        const char* actions[] = {"测试连接", os_sync_pull_pending() ? "保留本地" : "上传当前进度",
                                os_sync_pull_pending() ? "确认应用远端" : "下载远端进度", "停止同步"};
        for (int i = 0; i < 4; ++i) draw_control(fb, sync_control_rect(i), actions[i], 840 + i);
        ui_product_title(fb, (EpdRect){UI_MARGIN, 778, ui_content_width(), 246},
                         s_sync_note[0] ? s_sync_note : "操作时自动连接已保存 WiFi；下载后先确认，本地位置不会自动覆盖。", 28, 3);
    }
    static const char* labels[] = {"正文字体", "行距", "页边距", "首行缩进", "段落间距",
                                   "行辅助线", "对齐方式", "点击分区", "晃动翻页", "自动翻页", "翻页动画", "清残影周期", "实验功能", "字重"};
    char gc_value[16];
    for (int i = 0; i < BOOK_LAYOUT_ROWS; ++i) {
        if (!layout_visible(i)) continue;
        EpdRect r = layout_row_rect(i);
        if (s_pressed_control == 1200 + i) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
        if (i == 13) {
            ui_text_vc(fb, r.x, r.y + r.height / 2, 28, "字重", EPD_DRAW_ALIGN_LEFT, false);
            reader_body_font(true);
            ui_text_vc(fb, r.x + 108, r.y + r.height / 2, 28, "书页 Aa", EPD_DRAW_ALIGN_LEFT, false);
            reader_body_font(false);
            char value[16]; int steps = app_settings_book_stroke();
            snprintf(value, sizeof(value), steps ? "%+d" : "标准", steps);
            if (s_stroke_failed) copy_text(value, sizeof(value), "未保存");
            EpdRect minus = {r.x + r.width - 276, r.y + 4, 80, r.height - 8};
            EpdRect plus = {r.x + r.width - 80, r.y + 4, 80, r.height - 8};
            ui_draw_button(fb, minus, "−", false); ui_draw_button(fb, plus, "+", false);
            ui_text_vc(fb, r.x + r.width - 138, r.y + r.height / 2, 24, value, EPD_DRAW_ALIGN_CENTER, false);
            ui_hairline(fb, r.y + r.height, r.x, r.width, UI_GRAY_LIGHT);
            continue;
        }
        const char* value = layout_value(i);
        if (i == 11) {
            unsigned every = app_settings_gc_every();
            snprintf(gc_value, sizeof(gc_value), every ? "%u 次" : "关", every);
            value = gc_value;
        }
        ui_text_vc(fb, r.x, r.y + r.height / 2, 28, labels[i], EPD_DRAW_ALIGN_LEFT, false);
        char with_hint[40];
        if (layout_choice_row(i)) snprintf(with_hint, sizeof(with_hint), "%s ›", value);
        else copy_text(with_hint, sizeof(with_hint), value);
        ui_text_vc(fb, r.x + r.width, r.y + r.height / 2, 24, with_hint, EPD_DRAW_ALIGN_RIGHT, false);
        ui_hairline(fb, r.y + r.height, r.x, r.width, UI_GRAY_LIGHT);
    }
    if (s_layout_group == SETTINGS_DISPLAY) {
        bool speed = app_settings_book_direct();
        for (int i = 0; i < 2; ++i) {
            EpdRect r = i ? speed_rect() : direct_rect();
            if (s_pressed_control == 850 + i) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
            if (speed == (i == 1)) ui_draw_selected_round_rect(fb, r, UI_BTN_RADIUS);
            else ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
            ui_text_vc(fb, r.x + r.width / 2, r.y + r.height / 2, 34,
                       i ? "速度优先" : "画质优先", EPD_DRAW_ALIGN_CENTER, false);
        }
        ui_product_title(fb, (EpdRect){UI_MARGIN, 406, ui_content_width(), 60},
                         speed ? "纯文字使用直刷，灰阶档保留柔和字缘；插图保留原灰阶。"
                               : "正文与插图使用灰阶。选择速度优先可使用纯文字直刷。", 24, 2);
        const bool balanced = app_settings_book_gray_balanced();
        if (speed && app_settings_book_edge_gray()) {
            const char* label = balanced ? "灰阶刷新：细腻灰阶" : "灰阶刷新：快速灰阶";
            draw_control(fb, gray_balance_rect(), label, 855);
        }
        else {
            ui_draw_round_rect(fb, gray_balance_rect(), UI_BTN_RADIUS, UI_GRAY_LIGHT);
            ui_text_vc(fb, gray_balance_rect().x + gray_balance_rect().width / 2, 503, 26,
                       "灰阶刷新：开启速度档灰阶后可选", EPD_DRAW_ALIGN_CENTER, false);
        }
        if (speed) {
            draw_control(fb, edge_gray_rect(), app_settings_book_edge_gray() ? "速度档字缘：灰阶" : "速度档字缘：黑白", 853);
        } else {
            ui_draw_round_rect(fb, edge_gray_rect(), UI_BTN_RADIUS, UI_GRAY_LIGHT);
            ui_text_vc(fb, edge_gray_rect().x + edge_gray_rect().width / 2, 590, 28, "速度档字缘", EPD_DRAW_ALIGN_CENTER, false);
        }
        ui_product_title(fb, (EpdRect){UI_MARGIN, 656, ui_content_width(), 72},
                         speed ? (s_gray_balance_save_failed ? "灰阶刷新保存失败，请重试；原档位保持不变。" :
                                  balanced && app_settings_book_edge_gray() ?
                                    "细腻灰阶让字缘更平滑，但翻页闪动可能更明显；快速灰阶响应更快。" :
                                  "快速灰阶响应更快；细腻灰阶字缘更平滑。两档均使用原始字缘。") :
                                 "选择速度优先后，可调整字缘。", 24, 2);
        ui_product_title(fb, (EpdRect){UI_MARGIN, 884, ui_content_width(), 130},
                         "正文按成功翻页次数执行清理；设为0关闭，也可随时手动清理。清理时可能闪动。", 26, 3);
    } else if (s_layout_group == SETTINGS_INPUT || s_layout_group == SETTINGS_EXPERIMENTS) {
        ui_product_title(fb, (EpdRect){UI_MARGIN, 680, ui_content_width(), 256},
                         s_layout_group == SETTINGS_EXPERIMENTS
                             ? (s_animation_save_failed ? "动画设置未保存，请重新选择重试；原档位保持不变。" :
                                "柔化对照仅用于日间纯文字动画，观感待实机比较；可随时切回原有动画或关闭。")
                             : "点击分区决定轻点的位置与动作；中键打开工具，中键长按进入导航。", 28, 4);
    }
    ui_product_title(fb, (EpdRect){UI_MARGIN, 1054, ui_content_width(), 38},
                     s_layout_group == SETTINGS_SYNC ? s_title : "调整保持阅读位置 · 中键也可返回阅读", 22, 1);
    draw_control(fb, ui_product_reader_return_rect(0), "清残影", 810);
    draw_control(fb, ui_product_reader_return_rect(1), "返回阅读", 811);
    if (s_choice_row >= 0) {
        int count = layout_choice_count(s_choice_row);
        int pressed = s_pressed_control >= 870 && s_pressed_control < 870 + count ? s_pressed_control - 870 : -1;
        ui_draw_choice_sheet(fb, labels[s_choice_row], layout_choice_labels(s_choice_row), count,
                             layout_choice_current(s_choice_row), pressed);
    }
    ui_draw_menu_handle(fb, false);
}
// 行距/边距/缩进/段距变更后的整章重排：保持文本锚点，菜单留在原地，失败回书架。
// Repaginate after leading/margin/indent/gap changes, keeping the anchor while the menu stays put; failures return to the shelf.
static app_redraw_t relayout_menu(app_ctx_t* ctx) {
    if (!s_text) return APP_REDRAW_PAGE;
    size_t off = s_panel_anchor;
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
    s_choice_row = -1;
    s_toolbar = s_toolbar_sizes = false;
    s_pressed_control = -1;
    s_text_turn = false;
    s_du_count = 0;
    s_size_settle_ms = 0;
    invalidate_prep();
    return APP_REDRAW_FULL;
}

// 面板选项即刻生效并收起；排版项保持锚点重排。/ A sheet option applies at once and closes; typography keeps the anchor repagination.
static app_redraw_t apply_layout_choice(app_ctx_t* ctx, int row, int index) {
    s_choice_row = -1;
    switch (row) {
        case 1:
            app_settings_set_book_leading(k_leading_values[index]);
            return relayout_menu(ctx);
        case 2:
            app_settings_set_book_margin((uint8_t)index);
            return relayout_menu(ctx);
        case 3:
            app_settings_set_book_indent(index != 0);
            return relayout_menu(ctx);
        case 4:
            app_settings_set_book_para((uint8_t)index);
            return relayout_menu(ctx);
        case 5: {
            uint8_t old = app_settings_book_guide();
            app_settings_set_book_guide((uint8_t)index);
            if (!old || !index) return relayout_menu(ctx);
            break;
        }
        case 6:
            // 对齐不改变断行，仅绘制变化。/ Alignment never rewraps; only drawing changes.
            app_settings_set_book_align((uint8_t)index);
            break;
        case 8:
            s_shake_enabled = index != 0;
            app_settings_set_book_shake(s_shake_enabled);
            sensor_set(ctx, s_shake_enabled);
            break;
        case 9:
            app_settings_set_book_auto((uint8_t)index);
            break;
        case 10:
            s_animation_save_failed = !app_settings_set_book_turn_style((uint8_t)index);
            break;
        default:
            app_settings_set_gc_every(k_gc_steps[index]);
            break;
    }
    return APP_REDRAW_PAGE;
}
// 阅读工具直达翻页器，复用一次性位置票据，返回时恢复正文而非落到书架。
// Open the remote from reading tools with a one-shot position ticket, returning to body text instead of the shelf.
static void goto_bluetooth(app_ctx_t* ctx) {
    const app_desc_t* app = app_by_id(OS_APP_BLUETOOTH);
    if (!app || !s_text || !s_path[0]) return;
    s_font_return = (font_return_t){.active = true, .chapter = (uint16_t)s_chapter,
        .offset = (uint32_t)book_layout_page_start_offset(s_page), .view = READING};
    copy_text(s_font_return.path, sizeof(s_font_return.path), s_path);
    save_progress(); ctx->request_app = app;
}
static app_redraw_t layout_action(app_ctx_t* ctx, uint16_t x, uint16_t y) {
    if (s_choice_row >= 0) {
        int count = layout_choice_count(s_choice_row);
        int hit = ui_choice_sheet_hit(x, y, count);
        if (hit >= 0) return apply_layout_choice(ctx, s_choice_row, hit);
        if (hit == UI_SHEET_HIT_OUTSIDE) { s_choice_row = -1; return APP_REDRAW_PAGE; }
        return APP_REDRAW_NONE;
    }
    if (s_layout_group == SETTINGS_DISPLAY && app_settings_book_direct() && ui_rect_hit(edge_gray_rect(), x, y)) {
        app_settings_set_book_edge_gray(!app_settings_book_edge_gray());
        invalidate_prep();
        return APP_REDRAW_PAGE;
    }
    if (s_layout_group == SETTINGS_DISPLAY && app_settings_book_direct() && app_settings_book_edge_gray() &&
        ui_rect_hit(gray_balance_rect(), x, y)) {
        uint8_t current = app_settings_book_gray_profile();
        s_gray_balance_save_failed = !app_settings_set_book_gray_profile(current == 0 ? 1 : 0);
        invalidate_prep();
        return APP_REDRAW_PAGE;
    }
    if (s_layout_group == SETTINGS_DISPLAY && (ui_rect_hit(direct_rect(), x, y) || ui_rect_hit(speed_rect(), x, y))) {
        app_settings_set_book_direct(ui_rect_hit(speed_rect(), x, y));
        invalidate_prep();
        return APP_REDRAW_PAGE;
    }
    for (int i = 0; i < 4; ++i) if (ui_rect_hit(layout_tab_rect(i), x, y)) {
        s_layout_group = i; s_choice_row = -1; return APP_REDRAW_PAGE;
    }
    if (s_layout_group == SETTINGS_SYNC) {
        for (int i = 0; i < 4; ++i) if (ui_rect_hit(sync_control_rect(i), x, y)) {
            if (i == 3) {
                os_sync_job_cancel();
                prep_resume_after_sync();
                copy_text(s_sync_note, sizeof(s_sync_note), "已停止同步");
            }
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
                book_cover_join();
                prep_suspend_for_sync();
                if (!os_sync_job_start(i == 0 ? OS_SYNC_JOB_AUTH : i == 1 ? OS_SYNC_JOB_PUSH : OS_SYNC_JOB_PULL,
                                       s_sync_note, sizeof(s_sync_note)))
                    prep_resume_after_sync();
            }
            return APP_REDRAW_PAGE;
        }
    }
    for (int i = 0; i < BOOK_LAYOUT_ROWS; ++i) if (layout_visible(i) && ui_rect_hit(layout_row_rect(i), x, y)) {
        invalidate_prep();
        // 菜单保持打开，值就地刷新；“返回阅读”或中键退出后才看到正文效果。
        // The menu stays open with values refreshing in place; body effects show after Return or the middle key.
        if (layout_choice_row(i)) {
            // 多档行弹选择面板，不再逐档循环。/ Multi-tier rows open the sheet instead of cycling.
            s_choice_row = i;
            return APP_REDRAW_PAGE;
        }
        switch (i) {
            case 0: goto_font(ctx); return APP_REDRAW_NONE;
            case 13: {
                EpdRect r = layout_row_rect(13);
                int value = app_settings_book_stroke();
                if (x >= r.x + r.width - 80) ++value;
                else if (x >= r.x + r.width - 196) value = 0;
                else if (x >= r.x + r.width - 276) --value;
                else return APP_REDRAW_NONE;
                if (value < -16 || value > 16) return APP_REDRAW_NONE;
                s_stroke_failed = !app_settings_set_book_stroke(value);
                if (s_stroke_failed) {
                    copy_text(s_message, sizeof(s_message), "字重未保存，请重试");
                    return APP_REDRAW_PAGE;
                }
                s_message[0] = 0;
                return APP_REDRAW_PAGE;
            }
            case 7:
                s_choice_row = -1;
                s_view = TAP_ZONES;
                break;
            case 12:
                s_layout_group = SETTINGS_EXPERIMENTS;
                break;
        }
        return APP_REDRAW_PAGE;
    }
    if (ui_rect_hit(ui_bar_rect(0, 2), x, y)) return clean_reading();
    if (ui_rect_hit(ui_bar_rect(1, 2), x, y)) { os_sync_job_cancel(); s_choice_row = -1; s_view = READING; return APP_REDRAW_PAGE; }
    return APP_REDRAW_NONE;
}
static app_redraw_t manage_action(app_ctx_t* ctx, uint16_t x, uint16_t y) {
    int count = s_clear_confirm || s_file_removed ? 2 : 3;
    bool hit = false;
    for (int i = 0; i < count; ++i) if (ui_rect_hit(manage_rect(i, count), x, y)) {
        if (s_clear_confirm) { if (!i) s_clear_confirm = false; else manage_apply(ctx); }
        else if (!i) s_view = SHELF;
        else if (s_file_removed) manage_apply(ctx);
        else { s_clear_confirm = true; s_delete_confirm = i == 2; s_manage_message[0] = 0; }
        hit = true;
        break;
    }
    // 确认态点空隙是无操作：不整页闪刷。/ Confirm-state gaps are no-ops without a page flash.
    return hit ? APP_REDRAW_PAGE : APP_REDRAW_NONE;
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
        // 未选中时点删除/清进度是无操作：不整页闪刷，等待有效选择。
        // Delete/clear-progress without a selection is a no-op: skip the page flash
        // and wait for a valid selection.
        if (i >= 3 && !selected_count()) return APP_REDRAW_NONE;
        if (!i) select_page(ctx->leaf);
        else if (i == 1) clear_selection();
        else if (i == 2) { clear_selection(); s_batch_message[0] = 0; scan_shelf(ctx); }
        else { s_batch_delete = i == 3; s_batch_confirm = true; }
        return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}
// 工具条轨道与百分比跳转共用：按全书字节定位章与页，并回到阅读视图。
// Shared by the track tap and percent jump: locate the chapter and page by
// whole-book bytes, then return to the reader view.
// 增量分页只覆盖到入口页附近；跳转目标可能远在其后，先补齐到目标偏移再取页。
// 失败时保留已完成页，页码取回落到已知末页，不破坏章节状态。
// Incremental pagination only covers the entry pages; jump targets may sit far beyond,
// so extend coverage to the target offset before resolving the page. On failure keep the
// completed pages and let the lookup fall back to the last known page.
static void cover_offset(size_t offset) {
    lock_draw();
    while (!book_layout_complete() && book_layout_page_start_offset(book_layout_page_count()) <= offset)
        if (!book_layout_extend(8)) {
            ESP_LOGW(TAG, "jump pagination incomplete at %u", (unsigned)offset);
            break;
        }
    unlock_draw();
}
static app_redraw_t jump_to_bytes(app_ctx_t* ctx, uint32_t target) {
    save_progress();
    size_t ch = 0;
    while (ch + 1 < book_chapter_count() && book_chapter_byte_offset(ch + 1) <= target) ++ch;
    if (!load_chapter(ctx, ch, 0, false)) return APP_REDRAW_PAGE;
    uint32_t start = book_chapter_byte_offset(ch), end = chapter_end();
    size_t off = end > start ? (uint64_t)(target - start) * s_text_len / (end - start) : 0;
    cover_offset(off);
    s_page = book_layout_page_for_offset(off);
    s_view = READING;
    s_toolbar = false;
    refresh_time_left();
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
            if (!s_toolbar_sizes && ui_rect_hit(ui_product_reader_return_rect(0), x, y)) {
                goto_bluetooth(ctx); return APP_REDRAW_NONE;
            }
            if (ui_rect_hit(ui_product_reader_return_rect(1), x, y)) {
                s_toolbar = s_toolbar_sizes = false;
                invalidate_prep();
                return APP_REDRAW_PAGE;
            }
            for (int i = 0; i < BOOK_TOOL_COUNT; ++i) if (ui_rect_hit(tool_rect(i), x, y)) {
                invalidate_prep();
                if (s_toolbar_sizes) {
                    if (i == 0) s_toolbar_sizes = false;
                    else if (i == 1 || i == 2) return resize_text(ctx, i == 1 ? -1 : 1);
                    else if (i == 3) { goto_font(ctx); return APP_REDRAW_NONE; }
                    else if (i == 4) { save_progress(); s_toolbar = false; s_layout_group = SETTINGS_TEXT; s_view = LAYOUT; }
                    else { save_progress(); free_book(); s_view = SHELF; shelf_enter_catalog(ctx); shelf_refresh_progress(ctx); }
                    return APP_REDRAW_PAGE;
                }
                if (i == 0) { save_progress(); book_prepare_toc(); s_view = TOC; ctx->leaf = s_toc_tab == 0 ? s_chapter / BOOK_TOC_ROWS : 0; marks_reload(); }
                else if (i == 1) {
                    esp_err_t err = book_marks_add(s_path, (uint16_t)s_chapter,
                        (uint32_t)book_layout_page_start_offset(s_page), (uint8_t)percent(s_page));
                    copy_text(s_message, sizeof(s_message), err == ESP_OK ? "已添加本页书签" : "书签保存失败，请重试");
                    marks_reload();
                }
                else if (i == 2) { s_panel_anchor = book_layout_page_start_offset(s_page); s_toolbar_sizes = true; s_message[0] = 0; }
                else if (i == 3) { s_panel_anchor = book_layout_page_start_offset(s_page); save_progress(); s_toolbar = false; s_layout_group = SETTINGS_TEXT; s_view = LAYOUT; }
                else if (i == 4) return clean_reading();
                else { save_progress(); free_book(); s_view = SHELF; shelf_enter_catalog(ctx); shelf_refresh_progress(ctx); }
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
        if (s_view == SHELF && ui_rect_hit(shelf_layout_rect(), x, y)) {
            EpdRect r = shelf_layout_rect();
            bool grid = x >= r.x + r.width / 2;
            if (grid == s_shelf_grid) return APP_REDRAW_NONE;
            change_shelf_layout(ctx);
            return APP_REDRAW_PAGE;
        }
        if (ui_rect_hit(shelf_filter_rect(0), x, y)) {
            s_filter = (s_filter + 1) % 3; clear_selection(); s_batch_message[0] = 0;
            sort_shelf(ctx);
            return APP_REDRAW_PAGE;
        }
        if (ui_rect_hit(shelf_filter_rect(1), x, y)) {
            s_recent_sort = !s_recent_sort;
            sort_shelf(ctx);
            return APP_REDRAW_PAGE;
        }
        if (ui_rect_hit(shelf_filter_rect(2), x, y)) { search_begin(); return APP_REDRAW_PAGE; }
        if (s_view == SHELF && s_save_failed && y >= 250 && y < 284) {
            retry_progress();
            return APP_REDRAW_PAGE;
        }
    }
    int nav = -1;
    for (int i = 0; i < 3; ++i) if (ui_rect_hit(nav_rect(i), x, y)) nav = i;
    if (nav == 0 || nav == 2) {
        int next = ctx->leaf + (nav == 0 ? -1 : 1);
        if (next >= 0 && next < leaves()) {
            ctx->leaf = next; s_layout_leaf = -1;
            if (s_view == SHELF) shelf_cover_restore(ctx);
        }
        return APP_REDRAW_PAGE;
    }
    if (nav == 1) {
        s_message[0] = 0;
        if (s_view == TOC && s_text) { s_view = READING; s_toolbar = false; }
        else {
            s_layout_leaf = -1;
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
    s_eta_since = 0;
    os_sync_job_request_cancel();
    if (s_text) save_progress();
    book_stats_flush();
    // 锁屏时正文在手则置续读标记：断电唤醒后开机直达并自动开书；否则清掉防旧值。
    // Arm the reading marker when live text is held at lock so a power-cut wake
    // reopens straight into the book; clear it otherwise to avoid stale values.
    app_settings_set_lock_reading(s_text && s_view == READING && s_path[0]);
    return !s_save_failed;
}
static void on_enter(app_ctx_t* ctx) {
    os_page_turner_enter();
    s_gray_balance_save_failed = false;
    s_prep_sync_suspended = false;
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
    s_view = SHELF;
    ctx->leaf = cached ? s_shelf_leaf : 0;
    if (!cached) s_count = s_visible_count = 0;
    s_px = app_settings_book_px();
    s_scan_pending = !cached;
    s_layout_leaf = -1;
    s_shelf_grid = app_settings_shelf_grid();
    s_entry_active = book_entry_take(&s_entry);
    font_return_t font_origin = s_font_return;
    bool font_return = false;
    if (!s_entry_active && font_origin.active &&
        book_entry_request_position(font_origin.path, font_origin.chapter, font_origin.offset)) {
        s_entry_active = book_entry_take(&s_entry);
        font_return = s_entry_active;
    }
    // 明确的新导航优先；字体/蓝牙返回票据仅消费一次，不持久化到启动状态。
    // Explicit new navigation wins; consume font/BLE-return tickets once without persisting boot intent.
    s_font_return.active = false;
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
    // 断电唤醒的续读标记只在没有明确请求时生效（导航请求优先），取走即清。
    // The power-cut reading marker applies only without an explicit request
    // (navigation wins) and clears as it is taken.
    s_boot_resume = !entry_taken && app_settings_take_lock_reading();
    // 传书页已停止并 join HTTP，安全注销且只丢弃对应路径的旧进度。
    // Transfer has stopped and joined HTTP; safely unregister only invalidated paths.
    pending_drop_invalidated();
    s_store_revision = book_store_revision();
    s_mark_delete = -1; s_mark_manage = s_toolbar_sizes = false; s_layout_group = 0;
    s_toolbar = s_clear_confirm = s_batch_confirm = false;
    s_pressed_control = -1;
    s_scrub = false; s_scrub_paint_x = -1;
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
        if (!opened && font_return) {
            // 返回正文只重试一次，先回收字体缓存给重开事务留余量。
            // Retry body restoration once after reclaiming font caches for the reopening transaction.
            ttf_font_reclaim();
            opened = open_book(ctx, s_entry.path);
        }
        if (opened && font_return) {
            s_panel_anchor = font_origin.offset;
            s_layout_group = font_origin.group;
            s_view = font_origin.view == LAYOUT ? LAYOUT : READING;
            s_toolbar = s_view == READING && font_origin.sizes;
            s_toolbar_sizes = s_toolbar;
        }
        book_entry_finish(opened ? BOOK_ENTRY_OPENED : BOOK_ENTRY_FAILED);
        s_entry_active = false;
        if (!opened && !cached) { s_scan_pending = true; read_pico_sd_start_probe(); }
    } else if (cached && s_entry_active) apply_entry(ctx);
    if (cached && s_resume_pending) {
        s_resume_pending = false;
        char path[BOOK_STORE_PATH_MAX];
        if (book_progress_last_path(path, sizeof(path))) for (int i = 0; i < s_count; ++i)
            if (!s_shelf[i].removed && !strcmp(s_shelf[i].path, path)) {
                // 上次开书中断的书不再自动重提；书架里手动打开不受影响。
                // A book interrupted mid-open is not re-offered automatically; manual opening from the shelf is unaffected.
                if (!pico_boot_asset_allowed(path)) break;
                copy_text(s_resume_path, sizeof(s_resume_path), path);
                copy_text(s_resume_name, sizeof(s_resume_name), s_shelf[i].name); break;
            }
    }
    s_shake_enabled = app_settings_book_shake();
    if (s_shake_enabled) sensor_set(ctx, true);
    s_cover_diag_pending = true;
    ESP_LOGI(TAG, "shelf entry view=%u books=%d visible=%d scan=%u active=%u resume=%u",
             (unsigned)s_view, s_count, s_visible_count, (unsigned)s_scan_pending,
             (unsigned)s_scan_active, (unsigned)(s_resume_path[0] != 0));
}
// 全局菜单书架明确请求不触发续读询问。/ Global-menu shelf entry explicitly skips the resume prompt.
static void book_menu_select(app_ctx_t* ctx) {
    (void)ctx;
    (void)book_entry_request(BOOK_ENTRY_SHELF, NULL);
}
static void book_on_exit(app_ctx_t* ctx) {
    s_eta_since = 0;
    os_page_turner_leave();
    s_scrub = false; s_scrub_paint_x = -1;
    scan_close(); s_scan_active = false;
    os_sync_job_cancel();
    s_prep_sync_suspended = false;
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
    s_scrub = false; s_scrub_paint_x = -1;
    scan_close(); s_scan_active = false;
    os_sync_job_cancel();
    s_prep_sync_suspended = false;
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
        if (s_toolbar) {
            if (!s_toolbar_sizes) {
                *rect = ui_product_reader_return_rect(0);
                if (ui_rect_hit(*rect, x, y)) return 207;
            }
            *rect = ui_product_reader_return_rect(1);
            if (ui_rect_hit(*rect, x, y)) return 206;
            for (int i = 0; i < BOOK_TOOL_COUNT; ++i) {
                *rect = tool_rect(i);
                if (ui_rect_hit(*rect, x, y)) return 200 + i;
            }
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
    if (s_view == SHELF) { *rect = shelf_layout_rect(); if (ui_rect_hit(*rect, x, y)) return 115; }
    if (s_view == SHELF || s_view == BULK) {
        for (int i = 0; i < 3; ++i) {
            *rect = shelf_filter_rect(i);
            if (ui_rect_hit(*rect, x, y)) return 110 + i;
        }
        *rect = (EpdRect){UI_MARGIN, 250, ui_content_width(), 34};
        if (s_view == SHELF && s_save_failed && ui_rect_hit(*rect, x, y)) return 113;
    }
    if (s_view == LAYOUT) {
        // 面板打开时仅选项可点；面板上方不铺按下态。/ While the sheet is open only options hit; no press bed above it.
        if (s_choice_row >= 0) {
            int count = layout_choice_count(s_choice_row);
            int hit = ui_choice_sheet_hit(x, y, count);
            if (hit >= 0) { *rect = ui_sheet_choice_chip(hit, count); return 870 + hit; }
            return -1;
        }
        *rect = edge_gray_rect();
        if (s_layout_group == SETTINGS_DISPLAY && app_settings_book_direct() && ui_rect_hit(*rect, x, y)) return 853;
        *rect = gray_balance_rect();
        if (s_layout_group == SETTINGS_DISPLAY && app_settings_book_direct() && app_settings_book_edge_gray() &&
            ui_rect_hit(*rect, x, y)) return 855;
        *rect = direct_rect();
        if (s_layout_group == SETTINGS_DISPLAY && ui_rect_hit(*rect, x, y)) return 850;
        *rect = speed_rect();
        if (s_layout_group == SETTINGS_DISPLAY && ui_rect_hit(*rect, x, y)) return 851;
        if (s_layout_group == SETTINGS_SYNC) for (int i = 0; i < 4; ++i) {
            *rect = sync_control_rect(i);
            if (ui_rect_hit(*rect, x, y)) return 840 + i;
        }
        for (int i = 0; i < 4; ++i) { *rect = layout_tab_rect(i); if (ui_rect_hit(*rect, x, y)) return 820 + i; }
        for (int i = 0; i < BOOK_LAYOUT_ROWS; ++i) {
            if (!layout_visible(i)) continue;
            *rect = layout_row_rect(i);
            if (ui_rect_hit(*rect, x, y)) return 1200 + i;
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
    if (ev->type == UI_GESTURE_CANCEL) s_eta_since = 0;
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
    // 定位条会话内：DRAG 跟手；松手（SWIPE）跳转，未移动的轻点与多点取消恢复页脚。
    // Inside a scrub session: DRAG tracks; release (SWIPE) jumps, an unmoved tap or
    // multitouch cancels and restores the footer.
    if (s_scrub) {
        if (ev->type == UI_GESTURE_DRAG) { scrub_move(ctx, ev->x); return APP_REDRAW_DONE; }
        if (ev->type == UI_GESTURE_CANCEL) { scrub_restore(ctx); return APP_REDRAW_DONE; }
        if (ev->type == UI_GESTURE_SWIPE_L || ev->type == UI_GESTURE_SWIPE_R ||
            ev->type == UI_GESTURE_SWIPE_U || ev->type == UI_GESTURE_SWIPE_D)
            return scrub_commit(ctx, ev->x);
        if (ev->type == UI_GESTURE_TAP) { scrub_restore(ctx); return APP_REDRAW_DONE; }
        return APP_REDRAW_NONE;
    }
    EpdRect start_rect = {0}, end_rect = {0};
    int start = control_at(ctx, ev->x0, ev->y0, &start_rect);
    int end = control_at(ctx, ev->x, ev->y, &end_rect);
    if (ev->type == UI_GESTURE_PRESS) {
        if (scrub_ready() && ui_rect_hit(ui_bar_rect(0, 1), ev->x0, ev->y0)) {
            s_pressed_control = -1;
            scrub_begin(ctx, ev->x0);
            return APP_REDRAW_DONE;
        }
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
        if (s_view == SHELF && start >= 0 && start < shelf_rows() && !s_clear_confirm) {
            s_managed = s_shelf[ctx->leaf * shelf_rows() + start];
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
    if (ev->type == UI_GESTURE_TAP && !(s_scan_pending && (s_view == SHELF || s_view == BULK))) {
        if (start >= 0 && start == end) {
            app_redraw_t result = action_at(ctx, ev->x0, ev->y0);
            if (s_view == SEARCH) return result;
            return result != APP_REDRAW_NONE ? result : paint_control(ctx, start_rect);
        }
        if (start < 0 && end < 0 && s_view == READING && !s_clear_confirm)
            return action_at(ctx, ev->x0, ev->y0);
        if (start < 0 && end < 0 && s_view == LAYOUT && s_choice_row >= 0)
            return action_at(ctx, ev->x0, ev->y0);
    }
    if (!s_clear_confirm && !(s_scan_pending && (s_view == SHELF || s_view == BULK)) && !s_resume_path[0]) {
        if (s_view == READING && !s_ended && !s_toolbar && ui_rect_hit(body_rect(), ev->x0, ev->y0)) {
            if (ev->type == UI_GESTURE_SWIPE_L) return turn_page(ctx, 1);
            if (ev->type == UI_GESTURE_SWIPE_R) return turn_page(ctx, -1);
        } else if ((s_view == SHELF || s_view == TOC || (s_view == BULK && !s_batch_confirm)) &&
                   (ev->type == UI_GESTURE_SWIPE_U || ev->type == UI_GESTURE_SWIPE_D)) {
            int next = ctx->leaf + (ev->type == UI_GESTURE_SWIPE_U ? 1 : -1);
            if (next >= 0 && next < leaves()) {
                ctx->leaf = next;
                if (s_view == SHELF) shelf_cover_restore(ctx);
                return APP_REDRAW_PAGE;
            }
        }
    }
    return decorated ? paint_control(ctx, start_rect) : APP_REDRAW_NONE;
}
static app_redraw_t on_key(app_ctx_t* ctx, int key) {
    s_pressed_control = -1;
    // 按键打断定位条并整页恢复页脚。/ A key interrupts the scrub and restores the footer with a full repaint.
    if (s_scrub) {
        s_scrub = false;
        return APP_REDRAW_PAGE;
    }
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
    // 扫描封锁只作用于书架数组重建的视图；阅读输入不受后台扫描影响。
    // The scan freeze only guards views rebuilding the shelf array; reading input is unaffected by the background scan.
    if ((s_scan_pending && (s_view == SHELF || s_view == BULK)) || s_clear_confirm) return APP_REDRAW_NONE;
    if (s_view == MANAGE) {
        if (key == UI_KEY_1) { s_view = SHELF; return APP_REDRAW_PAGE; }
        return APP_REDRAW_NONE;
    }
    if (s_view == SEARCH) { if (key == UI_KEY_1) { search_finish(ctx, false); return APP_REDRAW_PAGE; } return APP_REDRAW_NONE; }
    if (s_batch_confirm) { if (key == UI_KEY_1) { s_batch_confirm = false; return APP_REDRAW_PAGE; } return APP_REDRAW_NONE; }
    if (key == UI_KEY_2) {
        if (s_view == BULK) { int first = ctx->leaf * BULK_ROWS; s_view = SHELF; ctx->leaf = first / shelf_rows(); }
        else if (s_view == READING) {
            s_toolbar_sizes = false; s_toolbar = !s_toolbar;
            invalidate_prep();
        } else if ((s_view == TOC || s_view == LAYOUT || s_view == TAP_ZONES) && s_text) {
            os_sync_job_cancel();
            s_choice_row = -1;
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
    // 设置面板与点击分区视图的 leaf 属于书架：在此翻动是看不见的隐形页，不响应。
    // The layout and tap-zone views share the shelf's leaf count: paging here would
    // move an invisible page, so the keys do nothing instead.
    if (s_view == LAYOUT || s_view == TAP_ZONES) return APP_REDRAW_NONE;
    int next = ctx->leaf + dir;
    if (next < 0 || next >= leaves()) return APP_REDRAW_NONE;
    ctx->leaf = next;
    if (s_view == SHELF) shelf_cover_restore(ctx);
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
static app_redraw_t shelf_covers_tick(app_ctx_t* ctx) {
    // 同页结果合并补刷；冷图超过250ms分批显示，切页只保留文件缓存，不推旧卡片。
    // Coalesce visible results; reveal cold batches after 250ms and never present cards from a previous page.
    if (s_cover_batch_leaf != ctx->leaf || s_cover_batch_grid != s_shelf_grid) {
        s_covers_dirty = false; s_cover_batch_leaf = ctx->leaf; s_cover_batch_grid = s_shelf_grid;
    }
    bool waiting = false;
    for (int row = 0; row < shelf_rows(); ++row) {
        int i = ctx->leaf * shelf_rows() + row;
        if (i >= s_visible_count) break;
        if (s_shelf[i].removed || cover_tried(s_shelf[i].path)) continue;
        uint8_t* gray = NULL;
        if (!book_cover_poll(s_shelf[i].path, &gray)) { waiting = true; break; }
        cover_store(s_shelf[i].path, gray, ctx->leaf);
        if (!gray) continue;
        if (!s_covers_dirty) s_cover_batch_due = ctx->now_ms + 250;
        s_covers_dirty = true;
    }
    if (s_covers_dirty && (!waiting || ctx->now_ms >= s_cover_batch_due)) {
        s_covers_dirty = false;
        ESP_LOGI(TAG, "shelf covers present leaf=%d grid=%u", ctx->leaf, (unsigned)s_shelf_grid);
        s_area = row_rect(0, false);
        for (int row = 1; row < shelf_rows(); ++row) s_area = ui_rect_union(s_area, row_rect(row, false));
        render(ctx, ctx->fb); s_mode = MODE_GL16;
        return APP_REDRAW_AREA;
    }
    if (!waiting && !s_covers_dirty) shelf_cover_prefetch(ctx);
    return APP_REDRAW_NONE;
}

static app_redraw_t on_tick(app_ctx_t* ctx) {
    // 菜单/阻塞操作期间不调页面心跳，恢复后重新计当前页，避免把停留算作阅读。
    // Page ticks pause during menus/blocking work; restart the current dwell after a gap instead of treating it as reading.
    if (s_eta_last_tick && (ctx->now_ms < s_eta_last_tick || ctx->now_ms - s_eta_last_tick > 1000)) s_eta_since = 0;
    s_eta_last_tick = ctx->now_ms;
    bool eta_reading = s_text && s_view == READING && !s_toolbar && !s_image_open &&
                       !s_quote_selecting && !s_scrub && !s_ended && !s_clear_confirm;
    if (!eta_reading) s_eta_since = 0;
    else if (!s_eta_since) s_eta_since = ctx->now_ms;
    if (s_cover_diag_pending) {
        s_cover_diag_pending = false;
        unsigned ready = 0;
        for (int row = 0; row < shelf_rows(); ++row) {
            int i = ctx->leaf * shelf_rows() + row;
            if (i >= 0 && i < s_visible_count && cover_for(s_shelf[i].path)) ++ready;
        }
        ESP_LOGI(TAG, "shelf cover gate view=%u consumed=%u scan=%u active=%u resume=%u ready=%u",
                 (unsigned)s_view, (unsigned)ctx->consumed, (unsigned)s_scan_pending,
                 (unsigned)s_scan_active, (unsigned)(s_resume_path[0] != 0), ready);
    }
    // 无线事件只在 UI 任务解释；覆盖层和触摸期间消费后丢弃，绝不回放。
    // Interpret radio input on the UI task only; consume and discard during overlays/touch, never replay it.
    bool remote_body = s_text && s_view == READING && !s_toolbar && !s_image_open &&
        !s_quote_selecting && !s_ended && !s_clear_confirm && !s_scrub && !s_resume_path[0] && !ctx->consumed;
    int remote = os_page_turner_poll(ctx->now_ms, remote_body, s_text && !os_sync_job_busy());
    if (remote == OS_PAGE_TURNER_LOCK) { app_loop_request_lock(); return APP_REDRAW_NONE; }
    if (remote) { app_loop_stay_awake(); return turn_page(ctx, remote); }

    if (os_sync_job_busy()) app_loop_stay_awake();
    if (os_sync_job_poll(s_sync_note, sizeof(s_sync_note))) {
        prep_resume_after_sync();
        return APP_REDRAW_PAGE;
    }
    if (ctx->consumed) return APP_REDRAW_NONE;
    // 自动翻页：从上次翻页起计时，仅在正文且无工具条/插图时触发；任何翻页都会重置锚点。
    // Auto page turn: timed from the last turn, only in the reader without toolbar/image; every turn resets the anchor.
    if (app_settings_book_auto() && s_text && s_view == READING && !s_toolbar && !s_image_open && !s_quote_selecting && !s_scrub) {
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
        app_redraw_t covers = shelf_covers_tick(ctx);
        if (covers != APP_REDRAW_NONE) return covers;
    }

    // 断电唤醒直达续读：只等 TF 探测完成即开书，不等书架扫描（开书自身校验文件与挂载，
    // 失败自然落回书架）；扫描随后在后台继续，退出阅读时目录已在。
    // A power-cut wake reopens the book as soon as the TF probe finishes — the shelf
    // scan no longer gates it (opening validates the file and mount itself and falls
    // back to the shelf on failure); the scan keeps filling in afterwards so the
    // catalog is ready when the reader exits.
    if (s_boot_resume) {
        if (ctx->now_ms - s_poll_ms < 100) return APP_REDRAW_NONE;
        s_poll_ms = ctx->now_ms;
        read_pico_sd_info_t sd = {0};
        if (read_pico_sd_get_info(&sd) == ESP_ERR_NOT_FINISHED) return APP_REDRAW_NONE;
        s_boot_resume = false;
        char path[BOOK_STORE_PATH_MAX];
        if (book_progress_last_path(path, sizeof(path)) && pico_boot_asset_allowed(path))
            open_book(ctx, path);
        return APP_REDRAW_PAGE;
    }
    if (s_scan_pending) {
        bool stepped = true;
        if (!s_scan_active) {
            stepped = false;
            if (ctx->now_ms - s_poll_ms >= 500) {
                s_poll_ms = ctx->now_ms;
                read_pico_sd_info_t info = {0};
                if (read_pico_sd_get_info(&info) != ESP_ERR_NOT_FINISHED) {
                    scan_shelf(ctx);
                    stepped = true;
                }
            }
        }
        if (stepped && scan_shelf_step(ctx)) {
            apply_entry(ctx);
            if (s_resume_pending) {
                s_resume_pending = false;
                char path[BOOK_STORE_PATH_MAX];
                if (book_progress_last_path(path, sizeof(path))) {
                    // 与进页同一约束：上次开书中断的书不再自动重提。/ Same rule as page entry: never re-offer a book interrupted mid-open.
                    if (pico_boot_asset_allowed(path)) {
                        for (int i = 0; i < s_count; ++i) if (!s_shelf[i].removed && !strcmp(s_shelf[i].path, path)) {
                            copy_text(s_resume_path, sizeof(s_resume_path), path);
                            copy_text(s_resume_name, sizeof(s_resume_name), s_shelf[i].name);
                            break;
                        }
                    }
                }
            }
        } else stepped = false;
        // 扫描期间书架视图独占 tick 并整页重绘；其它视图不再提前返回，正文维护（分钟页脚、
        // 定稿、补页、预绘）在后台扫描下照常进行。
        // During the scan the shelf keeps its exclusive ticks and settle repaints; other
        // views no longer return early, so reader maintenance (minute footer, settling,
        // remaining pages, prefetch) keeps running behind the background scan.
        if (s_view == SHELF || s_view == BULK) return stepped ? APP_REDRAW_PAGE : APP_REDRAW_NONE;
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
                              s_quote_selecting || s_ended || s_scrub ||
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
    prep_idle(ctx);
    return APP_REDRAW_NONE;
}
static EpdRect area_hint(app_ctx_t* ctx) { (void)ctx; return s_area; }

// 正文静止后才放宽心跳；排版、定稿、晃动与覆盖层保留快速调度。
// Relax the heartbeat only for settled body text; pagination, settling, shake and overlays keep fast scheduling.
static uint32_t idle_wait_ms(app_ctx_t* ctx) {
    if (ctx->consumed || os_sync_job_busy() || s_view != READING || !s_text || !book_layout_complete() || s_toolbar ||
        s_image_open || s_clear_confirm || s_scrub || s_quote_selecting || s_ended ||
        s_shake_enabled || s_du_count || s_size_settle_ms) return 0;
    if (s_next_fb && s_page + 1 < book_layout_page_count() && s_next_page != (int)s_page + 1) return 0;
    return 50;
}

const app_desc_t app_book = {
    .title = "书架", .detail = "图书 · 阅读 · 管理", .enter_full = false, .clean_page = true, .owns_keys = true,
    .render = render, .present = present, .on_enter = on_enter, .on_exit = book_on_exit, .on_menu_select = book_menu_select,
    .on_media_lost = book_on_media_lost,
    .on_gesture = gesture_event, .on_key = on_key, .on_key_long = on_key_long,
    .on_tick = on_tick, .area_hint = area_hint, .idle_wait_ms = idle_wait_ms,
};
