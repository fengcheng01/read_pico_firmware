# 版本变更 / Changelog

按日期和作者简述对用户可见的功能变化；详细实现历史见 Git。使用方法见 [README](../README.zh-CN.md)。
User-visible changes by date and author; Git retains implementation history. See [README](../README.md) for usage.

## 2026-10-02 · Codex · USB 连接电脑 / USB card disk

- 设置中的存储页新增 USB 连接电脑入口，确认后重启让电脑独占 TF 卡；电脑可读写图书/字体，安全弹出后退出重启恢复阅读与刷机串口。暂停阅读、传书和自动睡眠，不导出内置存储、不自动格式化；拔卡停止当前会话。
  Storage adds USB computer connection: confirm to reboot and grant the host TF ownership for books/fonts; safely eject, exit/reboot to restore reading and flashing serial. Reading, transfer and idle sleep pause; no internal-storage export or auto-format. Removal stops the session.

## 2026-10-01 · Codex · 阅读界面优化 / Reader UI improvements

- 阅读工具新增直接加书签与字号子面板；“更多设置”分为文字排版、翻页与显示两组，保留清残影和书架出口。书签管理和长按删除均须确认，取消保留原记录。
  Reader tools add direct bookmarks and a size subpanel; More Settings groups typography and turns/display, retaining cleaning and shelf exits. Bookmark management and long-press deletion require confirmation; cancellation preserves the record.
- 首页不再常驻字体技术告知；正文仅在当前章节确实存在缺字时提供完整字体入口。今日页以续读/书架按钮替代未实现待办区；存储记录区分检测中、无记录和不可读。
  Home drops the permanent font notice; the reader offers the full-font entry only when the current chapter has missing glyphs. Today replaces the unfinished todo area with Continue/Library, and storage records distinguish probing, absence and read failure.
- 同步使用单个后台请求，下载先展示本地/远端位置并确认，离页取消收齐；同步与上传/删除互斥。修复 EPUB 封面 XML 终止符、HTTP 短写/分段/超长响应及 JSON 边界处理。
  Sync uses one worker, stages local/remote positions for pull confirmation and cancels/joins on exit, excluding uploads/deletions during sync. Fixes cover XML termination, HTTP short writes/fragments/oversized responses and JSON bounds.

## 2026-10-01 · ZCode

- 书签与跳转：目录页改为“目录 / 书签 / 跳转”三页签。书签每书最多 16 条（按位置排序、满员淘汰最旧），首行一键加当前页，点按跳转、长按删除，删除图书时随进度一并清理；跳转页签提供 10%–100% 百分比直达，与进度条点按共用定位路径。
  Bookmarks and jumping: the TOC gains chapters/bookmarks/percent tabs. Each book holds up to 16 position-sorted bookmarks (oldest evicted), a leading row adds the current page, taps jump and long-presses delete, and deleting a book cleans its bookmarks with the progress; the percent tab offers 10%–100% jumps sharing the track-tap locator.
- 读完推荐：末页继续向前弹出“已读完”面板，推荐最多三本同来源图书（按最近阅读优先），点选即开、任意翻页或按键先收面板。
  End-of-book suggestions: paging past the last page shows a finished panel with up to three same-source books (recency first); picking one opens it, and any turn or key dismisses the panel first.
- 清残影周期可调：排版菜单新增“清残影周期”（关/3/5/10/14/20/30），控制差分刷多少次后自动升一次全像素 GC16，同时作用于正文翻页与通用页面；默认保持原 14 次。
  Adjustable ghost cleanup: Typography gains the ghost-cleanup period (off/3/5/10/14/20/30) controlling how many soft updates precede one full-pixel GC16, for reader turns and general pages alike; the default keeps the original 14.
- 黄历锁屏：锁屏样式新增“黄历”（农历日大字、干支年/生肖/农历月与公历日期），未校时仍回退静态图；“宜读书”为固定文案。
  Almanac lock face: lock styles add Almanac (the lunar day large with the ganzhi year, zodiac, lunar month and solar date), still falling back to static while uncalibrated; 宜读书 is fixed copy.
- 异常重启记录：panic/看门狗/掉电类重启会在内置存储 `crash.log` 追加一行有界记录（无反栈，IDF v6 未提供 panic 钩子），存储页显示条数与最近原因。
  Abnormal-reset log: panic/watchdog/brownout resets append one bounded line to `crash.log` on internal storage (no backtrace; IDF v6 exposes no panic hook), with the count and latest reason shown on the Storage page.

## 2026-09-30 · Codex

- 阅读增强：排版菜单新增对齐方式（左/居中/两端对齐）；阅读状态栏可配置时钟/电量百分比/进度条（页脚左侧）；今日页常显电量；睡眠设置新增空闲自动锁屏（关/5/10/30 分钟，自动翻页时不锁屏）；时间页新增联网对时入口（直达传书页，STA 上行自动 SNTP）。
  Reading upgrades: alignment (left/center/justified) joins Typography; a configurable footer status bar (clock/battery percent/progress bar); Today always shows battery; idle auto-lock (off/5/10/30 min, paused by auto turns) under Sleep settings; the time page links network calibration straight to Transfer.
- 预览一致性架构：预览与设备共享同一份产品源码（真实阅读器/设置/进度/统计/同步/搜索），仅仿真 ESP-IDF 边界（内存 NVS、pthread FreeRTOS、内存堆）；新增 parity 检查强制两侧源码清单一致，防止功能改了固件忘改模拟或反之。
  Preview parity architecture: the preview and device share the same product sources (real reader/settings/progress/stats/sync/search) behind an ESP-IDF boundary emulation (in-memory NVS, pthread FreeRTOS, memory heap); a new parity check keeps both source lists identical so a firmware change can no longer miss the preview or vice versa.
- EPUB 封面：书架对 EPUB 按“container→OPF→manifest（cover-image 属性/meta cover/常见文件名回退）”定位封面，逐 tick 有界提取并缩放为卡片位图；TXT/无封面回退文件名排版。
  EPUB covers: the shelf locates covers via container→OPF→manifest (cover-image property/meta cover/common-name fallback), extracting one bounded cover per tick scaled into the card bitmap; TXT/coverless books keep the typographic fallback.
- 审查修复：SNTP 每会话只校准一次（不再反复初始化/重写 PMU）；断开 WiFi 后自动上传标志复位；同步页“测试/注册”按钮离线置灰；超长自建服务器地址不再被键盘悄悄截断；锁屏画改用 force_poll 保留校准状态；崩溃日志 sscanf 溢出修复（并行新增的书签/农历/崩溃模块经宿主回归接入）。
  Review fixes: SNTP calibrates once per session (no repeated PMU writes); auto-push flag resets on disconnect; sync Test/Register buttons disable offline; long self-hosted URLs no longer truncated by the keyboard; lock faces use force_poll keeping sync state; a crash-log sscanf overflow fixed (parallel bookmarks/lunar/crash modules absorbed with host regressions).
- 首次固件构建通过：ESP-IDF v6.1 + esp32s3，app 分区 21% 余量；IDF v6 适配（NVS 迭代器 API、SNTP 手工配置、ROM MD5、uint32_t 格式串、sdkconfig 增加 SNTP 双服务器与 MBEDTLS_MD5_C）。
  First firmware build passes: ESP-IDF v6.1 + esp32s3 with 21% app headroom; IDF v6 adaptations (NVS iterator API, manual SNTP config, ROM MD5, uint32_t format strings, sdkconfig adds dual SNTP servers and MBEDTLS_MD5_C).
- 电脑预览支持滑动与返回：网页端拖拽 ≥64px 合成滑动（正文左右滑翻页）、“停止并返回”按设备语义回到进入来源；点击分区（左/右 30% 翻页）在预览中同样可验。
  Desktop preview gains swipes and returns: browser drags over 64px synthesize swipes (body page turns), Stop-and-return restores the entry origin per device semantics, and the left/right 30% tap zones are verifiable too.
- 传书页进入电脑预览：真实页面代码 + 夹具网络状态，首页/进度同步/键盘可在浏览器点验（不联网）。
  The transfer page joins the desktop preview: real page code over fixture network state with home/sync/keyboard explorable in the browser (offline).
- 阅读排版菜单：工具条新增“排版”与“夜间”。排版页十项：字体、行距（标准/舒展/宽松）、页边距、首行缩进（两字符）、段落间距、行辅助线（每行文字下方实线/虚线）、点击翻页开关、晃动实验、夜间模式、自动翻页（关/20/40/90 秒），底部清残影。菜单调整后保持打开、值就地刷新（返回阅读或中键退出）；行距/边距/缩进/段距变更即时重排并保持当前阅读位置。夜间模式反色正文（白字黑底、图片灰度翻转）。
  Reader typography menu: the toolbar gains Typography and Night. Typography lists font, leading tiers, side margins, first-line indent, paragraph gap, the per-line guide rule (solid/dashed under every text line), the tap-zone switch, the shake experiment, night mode and auto page turn (off/20/40/90 s), plus screen cleaning. Leading/margins/indent/gap changes repaginate at once while keeping the reading position. Night inverts the body with flipped image grays.
- 锁屏增强：睡眠设置内可选静态图/时钟/日历锁屏面（未校时自动回退静态图）；新增 4 位锁屏密码，开机在主循环之前阻塞校验，设置内可修改或清除。
  Lock-screen upgrades: Sleep settings pick a static/clock/calendar lock face (auto-falling back to static while uncalibrated); a 4-digit lock PIN blocks at boot before the main loop and can be changed or cleared in Settings.
- 阅读进度同步：传书页新增“进度同步”（kosync 协议，兼容 KOReader），服务器/账号设备上配置（密码仅存 MD5），手动或自动上传、手动下载最后一本书的进度；同文件跨设备精确恢复，其它客户端按百分比近似。
  Reading-progress sync: the transfer page gains Progress sync (kosync protocol, KOReader-compatible) with on-device server/account setup (password stored as MD5 only), manual/auto push and manual pull of the last book's progress; exact restore for the same file across devices, percentage approximation otherwise.
- 传书页主视图产品化：产品页眉与连接/状态卡片；网络配网、二维码与停止返回语义不变。
  Transfer home restyled with the product header and connection/status cards; provisioning, QR and stop/return semantics unchanged.

- 设置产品化：设置子项改为真正的产品页——阅读（默认字号、正文字体入口、晃动实验）、时间与时区、睡眠（浅睡/深睡/关机 + 拿起唤醒）、存储（TF 与内置容量只读检测）；原睡眠/TF 演示页移入诊断目录，字体选择器从阅读设置进入。
  Product Settings: children become real product pages — Reading (default size, body-font entry, shake experiment), Time & timezone, Sleep (light/deep/off plus pickup wake) and Storage (read-only TF/internal capacities); the sleep/TF demos move under diagnostics and the font picker is reached from Reading.
- 今日与时钟：今日页显示大字时钟、日期与真实阅读摘要（今日/近 7 日分钟、当前书进度、书架数量），分钟变化仅 DU 刷新时钟带；RTC 未校准时显式提示，不伪造时间。新增“设置 → 时间与时区”子页，15 分钟步进调整时区并即时生效。
  Today & clock: the Today page shows a large clock, date and a real reading summary (today/7-day minutes, current progress, shelf count), DU-refreshing only the clock band on minute changes; an uncalibrated RTC shows an explicit notice, never invented time. A new Settings → Time & timezone subpage adjusts the timezone in 15-minute steps with immediate effect.
- 自动校时：已有 WiFi 传书会话取得网络地址后自动 SNTP 校时一次并写入电源管理芯片，随即释放网络定时查询；热点模式不联网。CW32 断电后时间丢失属硬件特性，需重新校时。
  Auto time sync: STA transfer sessions run one SNTP calibration that writes the PMU and releases immediately; AP mode stays offline. CW32 power-cuts lose the clock by hardware design and need recalibration.
- 阅读统计：正文视图累计阅读时长，空闲 5 分钟截断、按本地日期归日、保留最近 30 天；未校时期间不累计，不把开机时长当阅读时长。
  Reading stats: reader-view time accumulates with a 5-minute idle cutoff, attributed to local dates keeping the newest 30 days; uncalibrated periods are skipped and uptime never impersonates reading time.
- 锁屏统一保存：电源键锁屏/睡眠前先落盘当前正文进度并冲刷统计检查点，失败不阻塞睡眠；普通翻页满 8 次的节流保存保留。
  Unified pre-lock saving: the power-key lock saves current reader progress and flushes stat checkpoints first; failures never block sleep. The 8-turn throttled save remains.
- 产品 UI：统一“正在读 / 书架 / 今日 / 设置”四根导航，诊断收进设置；三行书封书架、精简正文告知和六项阅读工具，保留搜索/管理/保存语义。今日明确未启用，设置下级页复用已有功能，尚非完整 OS。
  Product UI: four-root navigation with diagnostics under Settings, three cover cards per shelf page, compact reader notices and six reading tools; retain search/management/save behavior. Today explicitly remains unavailable; Settings children reuse existing pages, not a complete OS.
- 新版电脑预览：展示首页→书架→真实样文分页/字号/工具→返回；共用设备产品绘图，书籍状态用隔离的宿主适配，不访问用户文件或持久进度。新增产品流程回归与设备书页宿主语法检查，不等于固件构建或真机验收。
  New desktop preview: Home→Shelf→real sample pagination/size/tools→return using shared device product drawing and isolated host book state, never user files/persistent progress. Add product-flow regressions and host syntax checks for the device reader; not firmware/hardware acceptance.

- 阅读首页：普通开机进入“正在读”，分批读取真实已保存进度，文件名封面与最近三本；首页明确续读直接打开原阅读器，书架入口不再重复询问，原菜单续读询问与诊断保留。新增稳定产品 ID、Pico 能力桥接和只读首页存储枚举（不格式化/建目录）；时钟、待办与第二机型尚未实现。
  Reading home: normal boot opens Now reading with incremental real saved-history summaries, typographic covers and three recent books. Explicit home resume opens the existing reader without another prompt; home shelf entry skips resume while direct menu confirmation and diagnostics remain. Add stable product IDs, a Pico capability bridge and non-formatting/non-creating home root enumeration; clock, todos and the second target remain unimplemented.
- 电脑预览：直接编译原菜单、内置阅读测试、刷新与字体页面，提供本机浏览器点击交互、原始灰度 PNG 导出和集成检查；无需 ESP-IDF，不模拟外设、物理波形或完整书架。
  Desktop preview: compile the original menu, built-in reading, refresh and font pages for loopback browser interaction, native grayscale PNG export and integration checks; no ESP-IDF required, with no peripheral, physical waveform or full bookshelf emulation.

## 2026-09-28 · UNSaWEN

- 阅读：进入书架先确认是否续读；ZIP目录连续读取、字宽测量不生成位图，当前章先排起始/续读页，其余分页分批补齐。
  Confirm resume before opening the last book; read ZIP directories sequentially, measure advances without rasterizing, and paginate the initial/resume pages before completing the chapter incrementally.
- EPUB插图：默认占位，点击后单独预览本地JPEG/PNG，返回保持正文位置；标题前的章首图片及同资源单独标为标题图，其他重复图片提示本次已读章节中最早出现的位置，开章不再反复解码重复图片。
  Show image placeholders and load local JPEG/PNG previews only on tap, returning to the same text position; label opening title images and matching resources separately, retaining earliest-visited hints for other repeated illustrations and avoid decoding images on chapter opening.
- 手势：滑动翻页阈值由120缩短到64像素，保留抬手提交、轻点容差和短拖取消。
  Reduce swipe distance from 120 to 64 pixels while retaining release-to-commit, tap tolerance and short-drag cancellation.
- EPUB：ZIP 条目、资源项及章节上限提高到 32768，使用固定窗口扫描ZIP目录和紧凑索引查找，标题按实际长度保存，OPF/导航上限4 MiB；打开和排版显示等待提示，失败显示具体原因。
  Raise EPUB ZIP, manifest and spine limits to 32768, scan ZIP directories through a fixed window, use compact indexed lookup and actual-length titles, and allow 4 MiB OPF/navigation metadata; show parsing/layout wait hints and failure reasons.
- WiFi：TF 卡支持上传完整 TTF 字体，32 MiB 上限、同名确认、提交前结构校验及中断恢复；传书期间暂停卡上字体读取，离页恢复。
  Upload complete TTF fonts to TF over WiFi with a 32 MiB limit, confirmed replacement, pre-commit structural checks and interruption recovery; suspend card font reads during transfer and resume on exit.
- 部署：随仓库提供未修改的 ChillDuanSans VF v1.30、原 OFL-1.1 许可和来源校验；内置子集命名为 Read Pico UI，保留原版权与许可。
  Bundle unmodified ChillDuanSans VF v1.30, original OFL-1.1 terms and source verification; name the embedded subset Read Pico UI and retain attribution and licensing.

## 2026-09-27 · UNSaWEN

- EPUB 图片位置增加“[图片]”占位，避免纯图片章节显示为空白；暂不解码图片。
  Show “[图片]” placeholders at EPUB image positions so image-only chapters are visible; image decoding is not yet supported.

- 修复长篇 EPUB 因条目或章节超过 512 而无法打开的问题；上限提高到 2048，单资源解压上限仍为 2 MiB。
  Fix long EPUBs failing to open above 512 entries or chapters; raise the limit to 2048 while retaining the 2 MiB uncompressed resource limit.

- 图书阅读：支持 TF 卡或内置存储的 UTF-8/GBK TXT 与纯文本 EPUB，目录、逐书续读、字号、手势翻页及默认关闭的实验晃动翻页。
  Read UTF-8/GBK TXT and text-only EPUB from TF or internal storage, with a TOC, per-book resume, font sizes, gestures and optional experimental shake-to-turn.
- 图书管理：增加拼音/首字母/英文搜索、来源筛选、名称/最近阅读排序、详情，以及分别确认的单本/批量删除和清进度。
  Add pinyin/initials/English search, source filters, name/recent sorting, details and separately confirmed single/batch deletion or progress reset.
- WiFi 传书：支持设备热点和已有网络、触屏/网页配网、连接与网址二维码；网页管理当前存储，支持确认替换、取消和重试；停止后返回进入前的位置。
  Transfer over the device hotspot or an existing network, with touchscreen/web provisioning and connection/URL QR codes. Manage current storage in the browser, confirm replacements, cancel or retry uploads, and return to the entry location on stop.
- 可靠性：处理保存失败与部分成功重试、中文文件名和覆盖中断恢复；修复显示欠载后相位队列残留导致的死锁；检测 TF 失效后停止相关消费者并回退字体，插回后需显式重挂。
  Handle save failures, partial-operation retries, Chinese filenames and interrupted replacements. Fix a display queue deadlock after underrun; stop affected consumers and fall back fonts on TF loss, requiring explicit remount after reinsertion.
