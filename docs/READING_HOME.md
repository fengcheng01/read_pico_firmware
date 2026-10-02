# 阅读产品界面 / Reading product UI

本分支在原驱动与阅读引擎上重构产品界面，不是完整 OS 发布。普通冷启动进入“正在读”，工厂 VCOM 标定与设备自检续跑仍优先。全局菜单与产品页底栏只有“正在读 / 书架 / 今日 / 设置”四个根入口；原硬件概览及诊断经“设置 → 进阶与诊断”访问。

This branch rebuilds the product UI atop the existing drivers and reading engine, not a complete OS release. Normal cold boot enters Now reading, retaining factory VCOM/self-test priority. Global navigation and product footers expose only Now reading / Library / Today / Settings. Original hardware overview and tests are reached through Settings → Diagnostics.

## 使用 / Use

- 首页显示上次可用图书、已保存进度与最多三本其他最近阅读。EPUB 封面按 container→OPF→manifest 定位（cover-image 属性、meta cover、常见文件名回退），书架逐 tick 有界提取并缩放为卡片位图；TXT/无封面回退文件名排版，失败不阻塞书架。没有有效 `last` 时回退最近有效进度；没有阅读记录时提供书架/导入入口，不展示示例书。
  Home shows the last available book, saved progress and up to three other recent books. EPUB covers are located via container→OPF→manifest (cover-image property, meta cover, common-name fallback) and extracted one bounded per tick into the card bitmap; TXT/coverless books keep the typographic fallback and failures never block the shelf. Missing `last` falls back to the most recent valid progress; an unread library offers shelf/import actions without demo books.
- 点“继续阅读”或最近图书，是明确的单次开书请求；先等待存储探测与书架验证，然后按原阅读器恢复章节、文本偏移和字号。路径不存在或解析失败留在可用书架，显示原错误，不打开演示正文。
  Continue or a recent-book tap explicitly requests one open. After storage probing and shelf validation, the existing reader restores chapter, text offset and size. A missing or invalid book leaves an accessible shelf with an error, never the embedded reading demo.
- 首页“书架”只打开书架，不再询问续读；直接从全局菜单进入原图书页仍保留续读询问。首页 KEY1 续读（没有记录则打开书架），KEY2 强刷，KEY3 与右下把手开菜单；阅读器三键仍为上页/工具条/下页，长按中键保留菜单出口。
  Home Library opens the shelf without a resume prompt; direct global-menu entry retains the original confirmation. On home, KEY1 resumes (or opens an unread shelf), KEY2 cleans the screen and KEY3/the handle opens the menu. Reader keys remain previous/tools/next with middle-key hold for the menu.
- 导入复用原 AP/STA 传书页，离页停服务；回首页重新读取记录。无卡和缺完整字库仍受原内置存储 ≤1 MiB/书、内置 GB2312 字库之外的生僻字覆盖限制。
  Import reuses the AP/STA transfer page and stops its service on exit; returning home reloads records. Without TF or a complete font, the original internal ≤1 MiB/book and UI-subset font limitations apply.
- 普通书架每页三本，使用文件名书封、多行标题、来源与保存进度；翻页/批量管理在导航上方。搜索、来源筛选、排序与长按单本管理保留；批量页仍七行，切换时按首项位置换算页码，不清除勾选。
  Ordinary shelves show three typographic-cover cards per page, multiline titles, source and saved progress; paging/management sit above root navigation. Search, filtering, sorting and long-press management remain; batches keep seven rows and convert page indices by first item without clearing selection.
- 正文不显示四根导航；本章缺字时显示一行完整字体入口。中央点按/中键显示六项工具：目录/书签、添加书签、字号、更多设置、清除残影、书架；字号进入加减子面板，更多设置分成两组六行，共十二项：正文字体、行距（标准/舒展/宽松）、页边距（标准/宽/更宽）、首行缩进（两字符/关）、段落间距（标准/加大）、行辅助线（关/实线/虚线，画在每行文字下方）、对齐方式（左/居中/两端对齐，段末行保持左对齐，只影响绘制不重排）、点击翻页开关（左/右 30% 分区）、晃动实验、夜间模式、自动翻页（关/20/40/90 秒，从上次翻页计时，工具条或插图打开时暂停并阻止空闲锁屏）、清残影周期（关/3/5/10/14/20/30 次差分刷后升一次全像素 GC16，同时作用于正文翻页与通用页面），底部为清残影与返回阅读。菜单在调整后保持打开、值就地刷新，经底部“返回阅读”或中键退出；行距/边距/缩进/段距变更即时重排并保持文本锚点，辅助线与夜间只影响绘制。夜间模式只反色正文（白字黑底、图片灰度翻转），页脚工具条不变。
  Reading hides root navigation; the builtin-font notice stays a tappable single line. Center tap/middle key reveals TOC, smaller/larger text, Typography, Night and shelf actions. Typography lists twelve rows: alignment (left/center/justified with left last lines, draw-only), font, leading (standard/relaxed/loose), side margins, first-line indent (two ems/off), paragraph gap (standard/relaxed), the per-line guide rule (off/solid/dashed, drawn under every text line), the tap-zone switch (left/right 30%), the shake experiment, night mode, auto page turn (off/20/40/90 s timed from the last turn, paused while the toolbar or an image is open) and the ghost-cleanup period (off/3/5/10/14/20/30 soft updates before one full-pixel GC16, applying to reader turns and general pages alike), with screen cleaning and return at the bottom. The menu stays open with values refreshing in place, leaving via Return or the middle key; leading/margins/indent/gap repaginate at once while keeping the anchor, and the guide rule and night are draw-only. Night inverts only the body (white on black, flipped image grays) while chrome stays normal.
- 目录页改为三页签：目录（章节直达）、书签（首行“＋ 为本页加书签”，最多 16 条按位置排序、满员淘汰最旧，点按跳转、长按先确认删除，随文件删除一并遗忘；“清进度”保留书签）、跳转（10%–100% 按全书字节比例估算，使用全书字节比例定位）。
  The TOC becomes three tabs: chapters (direct jumps), bookmarks (a leading add-row, at most 16 position-sorted entries evicting the oldest, tap to jump, long-press to confirm deletion, forgotten with file deletion while progress clears keep them) and percent jump (10%–100% estimated by whole-book bytes, sharing the track-tap locator).
- 读完面板：在末章末页继续向前时弹出“已读完本书”，推荐最多三本同来源图书（按最近阅读优先）；点候选直接打开，“继续停留”关闭，任意翻页或三键先收面板。
  End-of-book panel: paging forward past the last page shows "finished" with up to three same-source suggestions (recency first); a candidate opens it directly, Stay closes the panel, and any turn or key dismisses it first.
- 设置子项指向产品页：阅读（默认字号 36–72 步进 4、正文字体入口、阅读状态栏时钟/电量/阅读百分比开关）、传书（复用原 AP/STA 页）、时间与时区、睡眠（浅睡/深睡/关机与浅睡拿起唤醒，写入既有 NVS 设置）、存储（TF 与内置书库只读容量检测）与诊断目录。原睡眠/TF 演示页移入诊断，仍可实际执行与格式化；字体选择器从阅读设置进入。
  Settings children point at product pages: Reading (default size 36–72 in steps of 4, body-font entry, footer clock/battery/bar switches), Transfer (the existing AP/STA page), Time & timezone, Sleep (light/deep/off plus light-sleep pickup wake, writing the existing NVS settings), Storage (read-only TF/internal capacity probing) and the diagnostics catalog. The sleep/TF demos moved under diagnostics and still execute/format; the font picker is reached from Reading.
- 空闲自动锁屏：睡眠设置选择 关/5/10/30 分钟，任何触摸刷新计时，自动翻页会阻止空闲锁屏；到点走与电源键相同的锁屏/保存路径。
  Idle auto-lock: Sleep settings pick off/5/10/30 minutes, any touch refreshes the timer, auto page turns block it; expiry takes the same lock/save path as the power key.
- 页脚状态栏与时钟电量：正文页脚左侧可显示时钟与电量百分比（阅读设置开关），今日页日期行右侧常显电量；电量来自 PMU 缓存快照，render 保持纯绘图。
  Footer status bar: the reader footer left shows the clock and battery percent (Reading settings switches) and Today always shows battery by the date; battery reads the cached PMU snapshot so render stays pure.
- 时间页提供“联网对时”入口直达传书页；STA 上行自动 SNTP 校准后，时间页与同步视图显示已校准。
  The time page links "network sync" straight to Transfer; after a STA uplink calibrates via SNTP, the time page and sync view show the synced state.
- 今日页显示大字时钟与日期，分钟变化只对时钟带做 DU 刷新；下方为真实阅读摘要：今日/近 7 日阅读分钟、当前书进度与书架数量。时间来自 PMU RTC 与用户时区；RTC 未校准时显示“时间未校时”，不伪造时间。
  Today shows a large clock and date, DU-refreshing only the clock band on minute changes; below is a real reading summary: today/7-day minutes, current-book progress and shelf count. Time comes from the PMU RTC plus the user timezone; an uncalibrated RTC shows the explicit notice, never an invented time.
- 校时：已有 WiFi（STA）传书会话取得网络地址后自动 SNTP 校时一次，写入 PMU TIME_SYNC 随即释放；热点模式与停止态不联网。CW32 断电/复位后时间丢失属预期，需要重新校时。
  Calibration: a STA transfer session with an uplink runs one SNTP sync, writes PMU TIME_SYNC and releases immediately; AP mode and stopped states stay offline. CW32 power-cuts reset the clock by design and need recalibration.
- 阅读统计：正文视图内累计阅读时长，空闲超过 5 分钟截断，按本地日期归日、保留最近 30 天，未校时期间不累计。统计是尽力而为的数据，不用于任何精确计费。
  Reading stats: reader-view time accumulates with a 5-minute idle cutoff, attributed to local dates keeping the newest 30 days, and skipped while uncalibrated. Stats are best-effort and never precision accounting.
- 电源键锁屏前会先运行统一保存钩子：正文进度立即落盘，阅读统计落检查点；失败不阻塞睡眠。普通翻页满 8 次的节流保存仍保留。
  Before the power-key lock runs a unified prepare hook: reader progress saves at once and stats flush a checkpoint; failures never block sleep. The 8-turn throttled save remains.
- 锁屏样式可选静态图/时钟/日历/黄历（睡眠设置内），时钟/日历/黄历取 PMU RTC 本地时间，未校时自动回退静态图；黄历面显示农历日大字、干支年/生肖/农历月与公历日期（`os_lunar` 纯换算，1900–2100 公开年表，锚点对齐春节/端午/中秋；“宜读书”为固定文案，不是逐日宜忌数据）。锁屏密码为 4 位数字，开机在主循环之前阻塞校验（菜单不可绕过），设置页可修改/清除，浅睡唤醒暂不校验。
  Lock styles offer static/clock/calendar/almanac (in Sleep settings); clock/calendar/almanac use PMU RTC local time and fall back to the static image while uncalibrated. The almanac face shows the lunar day large plus the ganzhi year, zodiac, lunar month and solar date (`os_lunar` pure conversion over the public 1900–2100 table with Spring-Festival/Dragon-Boat/Mid-Autumn anchors; 宜读书 is fixed copy, never per-day do/don't data). The lock PIN is 4 digits and blocks at boot before the loop (no menu bypass); Settings can change or clear it, while light-sleep wakes stay ungated for now.
- 异常重启记录：开机读复位原因，panic/看门狗/掉电类复位在内置 FAT 的 `crash.log` 追加一行（时间戳可能为 0=未校时），有界 4 KiB 裁尾；存储页显示条数与最近原因。IDF v6 未公开 panic 钩子，记录不含反栈；日志只写内置存储，不写 TF 卡。
  Abnormal-reset log: boot reads the reset reason and appends one line for panic/watchdog/brownout resets to `crash.log` on the internal FAT (timestamps may read 0 while uncalibrated), bounded to 4 KiB with line-boundary trimming; the Storage page shows the count and latest reason. IDF v6 exposes no panic hook, so no backtrace is captured; the log never touches the TF card.
- 传书页主视图换为产品页眉与连接/状态两张卡片；配网、二维码与停止语义不变。新增“进度同步”入口（kosync 协议，兼容 KOReader 与各 kosync 服务器）：服务器/用户名/密码用传书页键盘输入，密码仅以 MD5 保存与传输；手动“上传进度/下载进度”作用于最后一本读过的书，或开启每次已有 WiFi 会话自动上传一次。文档标识为 KOReader 部分 MD5（12 个偏移各 1024 字节），跨设备同文件精确恢复（rp1 进度串，文件大小须一致）；与其它客户端同步时按百分比近似并明确提示。上传/下载只在传书 STA 上行期间联网，https 校验证书不自跳过；自建服务器可用 http。
  The transfer home uses the product header with connection/status cards; provisioning, QR and stop semantics are unchanged. A new progress-sync entry (kosync protocol, compatible with KOReader and kosync servers) edits server/username/password through the transfer keyboard, with the password stored and sent only as MD5; manual push/pull act on the last-read book, or auto-push once per STA session. Document identity is KOReader's partial MD5 (1024 bytes at 12 offsets); the same file restores exactly across devices via the rp1 string (file sizes must match), while other clients sync by percentage with an explicit notice. Push/pull network only during STA uplinks, https verifies certificates without skipping, and self-hosted servers may use http.

首页新增只读枚举不会格式化或创建目录。**原书架和上传仍沿用旧的内置分区“挂载失败可格式化”初始化策略**，本次没有将其改成安全恢复/用户确认流程；存储异常时先备份，不把首页保护误认为全机保数据改造已完成。

New home enumeration neither formats storage nor creates directories. **The existing shelf/upload paths still retain their older format-on-mount-failure initialization policy.** Safe recovery and explicit initialization confirmation are not implemented by this change; back up abnormal storage rather than treating home protection as system-wide data safety.

## 维护契约 / Maintenance contract

- 唯一页面目录：`main/app/app_registry.c`。`os_app_id_t` 是稳定语义 ID，不持久化菜单序号/指针；首页、底部动作和字体跳转使用同一目录。`OS_APP_NONE` 不能被解析为产品页。
  One catalog in `main/app/app_registry.c`; `os_app_id_t` identifies stable intent, never persisted menu indices/pointers. Home, footer actions and the reader font route share it. `OS_APP_NONE` is not a product destination.
- 注册表头四项必须保持产品根顺序；全局菜单返回它们的真实索引，维持主循环按下态契约。设置快捷项随后（传书/字体/时间/阅读/睡眠/存储/诊断目录），诊断子目录从同一张表读取；`OS_APP_SLEEP`/`OS_APP_STORAGE` 指向产品设置页，原演示页以 `OS_APP_NONE` 挂入诊断。变更分组时同步目录边界与菜单回归，不往事件循环加入页面业务。
  The first four registry entries must retain root order. Global menus return their real indices, preserving loop pressed-state contracts. Settings shortcuts follow (transfer/fonts/time/reading/sleep/storage/tools); the diagnostic subcatalog reads the same table, with `OS_APP_SLEEP`/`OS_APP_STORAGE` now product settings and the demos carried as `OS_APP_NONE`. Update group boundaries and menu regressions together; add no page logic to the loop.
- `ui/product/ui_product` 统一标题、四根导航、书封卡与阅读正文/工具/页脚几何；设备 `app_book` 与宿主绘图适配共用它。绘制不得读存储或修改设置；绘图与命中使用同一矩形函数。
  `ui/product/ui_product` shares headers, roots, cover cards and reader body/tool/footer geometry between device `app_book` and the host drawing adapter. Drawing performs no storage reads/settings writes; draw and hit paths share rectangle functions.
- `book_entry` 是 UI 线程单槽请求：忙时拒绝覆盖、复制路径、进入页消费一次；扫描后回报 opened/shelf-ready/not-found/failed，离页或丢卡取消未完成请求。只有直接书根内 TXT/EPUB 路径被接受。不借用页面静态状态、不靠标题匹配或伪造触摸跳转。
  `book_entry` is a UI-thread single slot: reject busy replacement, copy the path, consume once on entry, report opened/shelf-ready/not-found/failed after scanning, and cancel unfinished work on exit/media loss. Accept only direct TXT/EPUB paths in book roots; no private-state writes, title matching or fabricated touches.
- `book_home` 固定保存当前书、三条最近摘要及四条排序候选，一个目录句柄；每次 `step()` 最多检查 16 个目录项，完成/取消后关闭目录。不存在与大小不匹配的旧进度不算有效记录。持久 `last_open_s` 是序号，不是时间；百分比只是已保存值，不表示“用户已读完”。
  `book_home` stores one current and three recent summaries plus four sorting candidates and one directory handle. Each step checks at most 16 directory entries, closing on completion/cancellation. Missing files and size-mismatched progress are not valid history. Persistent `last_open_s` is a sequence, not time; a saved percentage is not a user-defined finished status.
- `render()` 仅消费快照绘图；存储 IO 在进入/tick 回调进行。每批项数有界，但 FAT 挂载、文件 stat/NVS 读取的单次延迟仍须真机测量；不是硬实时响应保证。
  Render only draws a snapshot; storage IO runs in enter/tick callbacks. Batches are bounded by entry count, but individual FAT mount/stat/NVS latency needs hardware measurement, not a hard-real-time guarantee.
- `os_device.h` 的能力区分未知/没有/具备；实时探测状态独立。Pico 桥接在 `os_device_pico.c`，不把 GPIO、PMU 或触摸芯片带到首页。能力层目前仅覆盖首页所用的机型名、画布和存储探测/导入可用性，不是完整显示/输入/电源 HAL。
  Device capability distinguishes unknown/absent/present separately from runtime probe status. `os_device_pico.c` keeps GPIO/PMU/touch chips out of home. This boundary covers only home model/canvas and storage-probe/import availability, not a complete display/input/power HAL.
- `os_time` 分两层：`os_time.c` 是纯换算/格式化/日期键（宿主测试直接编译）；`os_time_pico.c` 在 UI 任务上以 ≥15 秒节流调用 PMU refresh 刷新缓存、以 TIME_SYNC 作为对 PMU 的唯一写命令，并在传书 STA 上行期间驱动 SNTP。时区存 NVS（±47 个一刻钟，默认 UTC+8）。`book_stats` 按 `book_stats_store` 接口持久化（设备 NVS `rp_stats` 命名空间，键 `st_YYYYMMDD` 分钟）；空闲 5 分钟截断、单拍上限 1 分钟、保留 30 天。
  `os_time` splits in two: `os_time.c` is pure conversion/formatting/date keys (compiled directly by host tests); `os_time_pico.c` refreshes the cache via throttled PMU refresh on the UI task, treats TIME_SYNC as the only PMU write, and drives SNTP during STA transfer uplinks. The timezone persists in NVS (±47 quarter-hours, default UTC+8). `book_stats` persists through `book_stats_store` (device NVS namespace `rp_stats`, keys `st_YYYYMMDD` minutes); 5-minute idle cutoff, 1-minute per-sample cap, newest 30 days kept.
- 睡眠统一保存在 `app/app_sleep_hooks` 的固定 4 槽注册表里：`enter_lock_and_sleep()` 画锁屏前先跑全部钩子；页面进页注册、离页注销，钩子在 UI 任务同步执行、失败不阻塞。`app_book` 注册的钩子保存当前正文并冲刷统计。
  Unified pre-sleep saving lives in the fixed 4-slot registry `app/app_sleep_hooks`: `enter_lock_and_sleep()` runs every hook before painting the lock face; pages register on enter and unregister on exit, hooks run synchronously on the UI task and never block on failure. `app_book`'s hook saves the open reader and flushes stats.
- `book_marks` 按书籍路径存 NVS（`rp_marks` 命名空间，版本化小端记录 + 路径碰撞校验，满 16 条淘汰最旧，序号先提交再写记录）。写操作仅 UI 线程；“清进度”保留书签，删除文件时与进度一起遗忘。`os_lunar` 是 1900–2100 纯换算（表外年份失败，不外推）。`os_crash` 纯文本部分可宿主测试，设备桥 `os_crash_pico.c` 只经 `book_store_read_roots` 的免格式化挂载写内置 FAT。
  `book_marks` stores per-path NVS bookmarks (namespace `rp_marks`, versioned little-endian records with path-collision checks, oldest-evicted at 16, sequence committed before the record). Writes stay on the UI thread; progress clears keep bookmarks while file deletion forgets them with progress. `os_lunar` is a pure 1900–2100 conversion failing outside the table without extrapolation. `os_crash`'s text half is host-testable while the device bridge `os_crash_pico.c` writes the internal FAT only through the format-free mount of `book_store_read_roots`.

第二台 Metalio E-ink 4 Plus 尚无开发资料，未提供它的可刷写固件或驱动。后续按机型分别构建镜像，共用页面与业务；不承诺同一个二进制支持 ESP32-S3 与 ESP32-S31。

The Metalio E-ink 4 Plus has no developer sources available here yet; no flashable image or driver is supplied for it. Future targets build separate images while sharing pages/business logic, not one binary for both ESP32-S3 and ESP32-S31.

## 验证 / Verify

从仓库根目录运行 / Run from the repository root:

```sh
python3 tools/run_os_home_tests.py
python3 tools/book_ui_host_test.py
python3 tools/preview/verify.py --sanitize
bash tools/run_app_loop_host_tests.sh
python3 tools/test_sd_media_guard.py
```

电脑预览与设备**共享同一份产品源码**（含完整阅读器 `app_book`、阅读引擎、设置/进度/统计、同步协议与搜索），仅由 `esp_host.c` 仿真 NVS/FreeRTOS/内存堆等 ESP-IDF 边界；`parity.py` 强制两侧源码清单一致，设备专属桥与显式替身必须登记。夹具书为真实文件，扫描/打开/进度/删除路径全走真实代码。不挂载卡、不读 NVS，不据此验收 TXT/EPUB 文件生命周期、持久进度、删除或上传。这些测试不等于 ESP-IDF 构建、PSRAM/并发、波形、休眠或真机验收。旧 macOS 测试 SDK 不匹配时仅为当次命令选择当前 Xcode SDK。

The desktop preview and the device **share the same product sources** (including the full reader `app_book`, the layout engine, settings/progress/stats, the sync protocol and search), with `esp_host.c` emulating only the ESP-IDF boundary (NVS/FreeRTOS/heap); `parity.py` keeps both source lists identical and requires registering device-only bridges or explicit stand-ins. Fixture books are real files, so scanning, opening, progress and deletion run through real code. No card/NVS access or file lifecycle/persistence/delete/upload acceptance is implied. This does not replace ESP-IDF builds or hardware PSRAM/concurrency/waveform/sleep tests. For older macOS SDK mismatches, select the current Xcode SDK for that command only.

未实现：本地待办及其输入通道、自定义图片锁屏（需通用图片解码进锁屏路径）、英文连字符断词（需词典/模式库，CJK 主场景优先）、真实封面缓存、冷启动恢复、保数据初始化迁移及第二机型驱动。SNTP 校时与 PMU 走时精度、锁屏保存耗时、阅读中周期性 PMU refresh 的延迟影响都未在真机测量。

Not implemented: local todos and their input channel (web or device IME), restyling every Settings child, real cover caches, cold-boot recovery, data-preserving initialization or the second target. SNTP calibration and RTC drift, lock-save latency, and the latency of periodic PMU refresh while reading are all unmeasured on hardware.

## 阅读优化后的交互与同步契约 / Refined reader and sync contracts

工具条为目录/书签、添加书签、字号、更多设置、清除残影、书架。字号进入保留当前位置的加减子面板；更多设置分成两组六行，所有既有排版选项保留。书签“管理书签”可点选删除，长按也只打开确认框；取消不改变记录或阅读位置。首页不常驻字体技术说明；正文按当前章节驻留 cmap 检查是否缺字，只有缺字时显示完整字体入口。今日只展示真实时间/统计并提供续读或书架入口。
Tools expose TOC/bookmarks, Add Bookmark, Size, More Settings, Clean and Library. Size opens a +/- subpanel retaining the text anchor; More Settings uses two six-row groups and retains all existing options. Manage Bookmarks selects entries for deletion; holds also only open confirmation, and cancellation preserves records and position. Home avoids a permanent font notice; the reader checks the chapter against resident cmap and offers a full-font entry only for missing glyphs. Today shows real time/statistics with Continue or Library.

同步由 UI 准备配置/文件标识/进度快照，单个 8 KiB 栈后台任务只做 HTTP 与协议解析，不读写 NVS。每个网络调用的等待为 1 秒，流处理总期限 10 秒，短写循环、分段收齐，响应超长/失败/未完成则拒绝。JSON 用 cJSON，解析前限制嵌套八层，输出不静默截断，百分比必须有限且位于 0..1。https 保持证书校验。
The UI prepares config/file-identity/progress snapshots, and one worker with an 8 KiB stack handles HTTP/protocol parsing without NVS access. Network calls wait up to one second, with a ten-second stream deadline; writes loop and fragmented responses assemble, rejecting oversized, failed or incomplete bodies. cJSON parses at most eight nesting levels after a precheck; outputs never silently truncate, and percentages must be finite within 0..1. HTTPS certificate validation remains enabled.

`read_pico_transfer_claim_sync` 在现有临界区中与上传/删除互斥：已有文件操作时启动同步立即拒绝；同步期间网页文件变更返回冲突，完成后可重试。下载结果不立即写进度，页面显示本地/远端百分比和精确/近似位置；确认时重新核对文件大小、部分 MD5 与本地进度序号，发生变化则要求重新下载。取消保留本地。离页/媒体丢失/断网先取消并收齐后台请求，再解除互斥并处理 HTTP 生命周期。睡眠钩子只置原子取消位，不等待网络；醒后 UI 收齐失败结果。HTTP 的 DNS/TLS 连接阶段也受平台超时实现约束，以上期限不是硬实时保证。
`read_pico_transfer_claim_sync` uses the existing critical section to exclude upload/deletion: active mutations reject sync admission immediately; mutations during sync return conflict and can retry afterward. Pulls stage results rather than saving immediately; UI shows local/remote percentages and exact/approximate location. Confirmation rechecks size, partial MD5 and the local progress sequence, requiring a fresh pull if changed; cancellation keeps local progress. Exit/media loss/disconnection cancels and joins the worker before releasing exclusion and handling HTTP lifecycle. The sleep hook only sets an atomic cancel bit, never waits for the network; UI collects failure on wake. DNS/TLS connection waits additionally depend on platform timeout behavior, so these deadlines are not hard-real-time guarantees.

预览使用离线设备边界替身，不验证真实网络时序；`tools/run_os_home_tests.py` 额外编译真实设备同步桥，以 pthread/HTTP 替身验证网络线程与 UI NVS 分离、取消、确认及冲突，验证 HTTP 流边界和无 NUL XML。宿主构建需先有 IDF 锁定的 `managed_components/espressif__cjson`（通过 `idf.py reconfigure` 获取），不另复制 JSON 解析器。
Preview uses offline device boundaries rather than validating network timing. `tools/run_os_home_tests.py` also compiles the real sync bridge with pthread/HTTP fakes to check worker/UI-NVS separation, cancellation, confirmation and conflicts, plus HTTP stream bounds and raw XML without NUL. Host builds require the IDF-locked `managed_components/espressif__cjson` populated by `idf.py reconfigure`; no duplicate JSON parser is maintained.


USB 卡盘经设置 → 存储确认重启进入，完整操作、独占/引导/刷机串口契约见 [USB_SD.md](USB_SD.md)。/ USB disk entry, exclusive ownership, boot and serial recovery are documented in USB_SD.md.

## 字体、图文和刷新 / Fonts, illustrations and refresh

内置 Regular 字库覆盖 GB2312 全部 6763 个汉字、标点与界面文字；更多繁体字、生僻字或其他语言可用 TF 卡 TTF。字体选择每页六行，包含内置项、当前标记和正文示例；切换成功才保存，切换不执行测速或强制黑闪。主页标题、书名、元数据和四根导航加大字号，长标题仍换行或截断。
The built-in Regular font covers all 6763 GB2312 Chinese characters, punctuation and UI text; TF TTF fonts extend traditional, rare or other-language coverage. The picker shows six rows including Built-in, a selected mark and a text sample; only successful selection persists, with no benchmark or forced black flash. Root headings, titles, metadata and navigation use larger type with bounded wrapping/truncation.

EPUB 开章时串行尝试最多 64 幅本地 JPEG/PNG，章节灰度位图共用 768 KiB 预算并按数量缩小，加载后参加正文分页。失败、内存不足或超限保留可点击占位；放大预览仍有独立一幅缓存，关闭保持正文位置。旧章释放所有内嵌位图。翻页和 render 不解码图片；资源与解码器上限见 README。
EPUB chapter loading serially attempts up to 64 local JPEG/PNG images, sharing a 768 KiB grayscale budget and reducing dimensions by image count before inline pagination. Failure, memory pressure or limits leave tappable placeholders. Enlarged preview keeps a separate single-image cache and returns to the same text position. Inline images are freed with the old chapter; turns and render do not decode. Resource and decoder limits are in README.

正文去掉页脚进度横线及章刻度，也移除其隐藏的点击跳转区；目录的跳转页签保留。状态栏开关控制百分比文字。普通翻页的正文、图片和页脚一次 GL16 更新，保留 16 灰阶抗锯齿；日常 GL16 使用未裁剪的 48 相波形，增加擦除余量，速度和残影仍需实机测量。设置、书架与字体进页不再强制 GC16，诊断和明确清残影仍可强刷。全机只由 display 统计普通刷新次数，默认第 14 次升 GC16；关闭周期后由用户手动清残影。
The reader removes the footer rule, chapter ticks and their hidden tap-to-jump area; the TOC jump tab remains. The footer switch controls percentage text. Ordinary turns update body, images and footer in one GL16 presentation, retaining 16-gray antialiasing. Daily GL16 uses the untrimmed 48-phase waveform for more erase margin; speed and physical ghosting require measurement. Settings, shelf and fonts no longer force GC16 on entry, while diagnostics and explicit cleaning retain full refresh. Only display counts ordinary updates, defaulting to GC16 on the 14th; disabling the period leaves manual cleaning.

字库与两张静态位图用 `RPFT + LE32 原长度 + zlib` 打包，由 `tools/gen_builtin_font.py` 生成 `.pack`。解包校验长度、流结束和校验和；位图直接还原到已有 framebuffer，字库还原到 PSRAM，驻留字库不再复制整张 glyf 表。原始 TTF、位图和许可证保留，不改变分区和 TF 部署路径。
The font and two static bitmaps use `RPFT + LE32 raw length + zlib` packs generated by `tools/gen_builtin_font.py`. Unpacking checks length, stream completion and checksum. Bitmaps inflate into the existing framebuffer; the font resides in PSRAM without a duplicate whole glyf table. Original TTF/bitmaps/licenses remain; partitions and TF paths are unchanged.
