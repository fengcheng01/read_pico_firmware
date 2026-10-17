# 版本变更 / Changelog

按日期和作者简述对用户可见的功能变化；详细实现历史见 Git。使用方法见 [README](../README.zh-CN.md)。
User-visible changes by date and author; Git retains implementation history. See [README](../README.md) for usage.

## 0.6.17 · 2026-10-17

- 修复 TXT 章节解析两类漏检（“明朝那些事儿”“橙红年代”实测）：章号前带部名的行（“洪武大帝 第一章 童年”）现在按“干净前缀+空白+第N章”识别；17k 等站点导出的“N-M 标题　[本章字数：…]”长行不再因超长被否决，标题截到标记前。两本书分别从 Part 兜底恢复为 159/860 章。
  Fix two TXT chapter-detection misses (verified against “明朝那些事儿” and “橙红年代”): lines carrying a part name before the chapter number (“洪武大帝 第一章 童年”) now match as a clean prefix plus whitespace plus 第N章, and long site-export lines (“N-M title　[本章字数：…]”, as exported by 17k and friends) are no longer rejected for overlength, with titles cut before the marker. The two books restore to 159/860 chapters from the Part fallback.
- TXT 解析合并参考仓库 RickyOS 的增强：中文数字补 〇/亿/廿（“第一百〇八章”不再漏）；“第 1 章”式空格容忍；英文题头支持 book/part 前缀与罗马数字（Chapter IV）；Markdown # 标题识别（代码围栏内跳过）；编码改为严格 UTF-8→GBK 双验证，UTF-8 文件单字节损坏不再整本误判 GBK 出乱码。TXT 目录缓存版本升级，旧缓存自动重建。
  TXT parsing merges enhancements from the reference repository RickyOS: Chinese numerals gain 〇/亿/廿 (“第一百〇八章” indexes again), whitespace around “第 1 章” is tolerated, English headings accept book/part prefixes and Roman numerals (Chapter IV), Markdown # headings are recognized (skipping code fences), and encoding detection switches to strict UTF-8-then-GBK validation so a single stray byte no longer flips a UTF-8 book into GBK mojibake. The TXT index cache version bumps, rebuilding old caches automatically.

- 封面提取补齐最后一级启发式（参考 KOReader）：EPUB3 cover-image 属性 → EPUB2 meta → 常见文件名之后，再扫 spine 首个文档内的首张图片；封面缩放改用与解码同规则的平滑重采样（面积平均/双线性），缩略图缓存版本升级自动重生成。
  Covers gain the last-resort heuristic (after the KOReader review): beyond the EPUB3 cover-image property, EPUB2 meta and common names, the first image inside the first spine document is tried; thumbnail scaling now shares the decoder's smooth resampling (box/bilinear) with a cache-version bump for regeneration.
- 阅读页脚新增剩余阅读时间与章节刻度：按本书累计每页耗时（单页 3 秒起计、120 秒截断，随进度 v3 持久化）估算剩余分钟；百分比开关打开时页脚绘制进度轨道并标出各章节起点，估算跨会话保留。设置页新增“剩余时间”开关。
  The reader footer gains remaining-time and chapter ticks: per-book average dwell (counting from 3 s, clamped at 120 s, persisted with progress v3) estimates minutes left; with the percentage toggle on the footer draws a progress track with chapter-start marks, and settings adds a "time left" switch.
- 中文排版细节升级：中西文相邻自动加 1/4 em 间隙；两端对齐只拉伸合规字隙（开括号后、右标点前不再拉宽）；悬挂行先把空格收半宽再压字距。标题（h1–h3）以覆盖率提升实现假粗体，不改字宽与分页。
  CJK typography details: a quarter-em gap appears between CJK and Latin neighbours; justification only widens legal gaps (never right after openers or before closers); hung lines halve spaces before squeezing gaps. Headings (h1-h3) gain a faux bold via coverage boost without touching advances or pagination.
- 英文阅读不再拦腰断词：整词下移换行，放不下的长词按 TeX/Liang en-US 模式断词并补画连字符，书内软连字符（U+00AD）与显式连字符同样生效。
  English reading no longer breaks mid-word: whole words wrap, oversized words hyphenate via the TeX/Liang en-US patterns with a painted hyphen, and in-book soft hyphens (U+00AD) and explicit hyphens work too.
- 修复查看图片返回列表后的残影：照片以 GC16 整屏显示，而返回重绘走保白的 GL16 清不掉灰阶；现在离开查看页（含书内大图返回正文）后的第一帧统一 GC16 定稿。
  Fix ghosting after leaving the image viewer: photos display via full GC16 while the return repaint used white-preserving GL16 that cannot clear photo grays; the first frame after leaving a viewer (including the in-book image returning to text) now settles with one GC16 pass.

## 0.6.16 · 2026-10-17

- 图片画质升级（参考 KOReader 方案）：PNG 与 JPEG 缩小改为面积平均、放大改双线性，替换原先丢行出摩尔纹的抽稀/最近邻；小图 JPEG 不再先丢一半分辨率，大图解码平面按成本封顶。
  Sharper images (after a KOReader review): shrinking now area-averages and growing interpolates bilinearly, replacing decimation/nearest that dropped rows into moire; small JPEGs keep full decode resolution while large ones cap the decode plane by cost.
- 书内插图、书内大图查看与书架封面接入 16 级有序抖动（4x4 升 8x8 Bayer、6 位余数），灰阶渐变更平滑；TF 查看器与锁屏壁纸同步升级。
  Inline illustrations, the in-book image viewer and shelf covers now share the 16-level ordered dither (4x4 to 8x8 Bayer with a 6-bit remainder) for smoother gradients; the TF viewer and lock wallpaper upgrade with them.
- 中文排版新增标点挤压：行尾放不下的句读/引号先各收半个字宽留在行内，不再整行压字距或提前折行；禁则与悬挂规则不变。
  CJK typesetting gains punctuation compression: overflowing trailing marks first give up half an em to stay on the line instead of squeezing every gap or wrapping early; kinsoku and hanging rules are unchanged.
- 阅读页新增页脚定位条：按下页脚带出现刻度尺，拖动时 DU 跟手显示全书百分比，松开跳转，点按或抬指取消；进入位置有起始标记。
  The reader gains a footer scrub: pressing the footer band opens a ruler that tracks the finger via DU with a live whole-book percent, jumps on release, and cancels on tap or multitouch; the entry position carries a notch.
- 摘录锁屏来源由仅书名改为“出自-书名·章节名”：保存摘录时快照章节标题（记录升级 v2，旧记录自动回退仅书名）；手帐金句列表与睡眠页摘录选择器同步显示章节名；竖排来源列加宽为两列。
  Excerpt lock sources now read “出自-书名·章节名”: chapter titles snapshot at save time (records upgrade to v2 with legacy ones falling back to book-only), the journal quote list and the sleep picker show the chapter too, and the vertical source column widens to two.
- 修复手帐金句页底部说明与“每日目标”行重叠：说明移入卡片列表与翻页行之间的空带并收为单行，避开共用按钮与目标行。
  Fix the journal quotes-tab note overlapping the daily-goal row: the note moves into the band between the card list and the nav row as a single line, clear of the shared button and goal row.

## 0.6.15 · 2026-10-16

- 修复阅读中途进入蓝牙页提示“内存不足”：启动前先释放插图成品、封面与字形等可重建缓存再判断，内部内存门槛按 EXTALLOC 需求放宽，拒绝时日志记录实测值供真机诊断。
  Fix "insufficient memory" when entering the BLE page mid-reading: release rebuildable decoded-image, cover and glyph caches before the check, relax internal floors to EXTALLOC needs, and log measured values on refusal for device diagnosis.
- 修复学习按键“突然未映射”：翻页器地址变化后重新搜索连接时保留已学习映射（仅遗忘或恢复标准按键时清除），绑定按报告帧内容匹配，不再依赖重连后的特征顺序。
  Fix learned keys suddenly unmapped: rescanning after address rotation keeps learned mappings (cleared only by Forget/Restore), and bindings match report frame content rather than post-reconnect characteristic order.
- 修复翻页不灵敏：连接或发现失败后不再停留在错误状态，每 10 秒自动重连；章节排版/刷屏期间排队的按键有效期从 0.8 秒放宽到 1.5 秒。
  Fix unresponsive turning: connection or discovery failures now retry automatically every ten seconds instead of parking in the error state; presses queued during chapter layout or refresh stay valid for 1.5 s instead of 0.8 s.
- 修复大于 2 MB 的渐进式 JPEG 壁纸“图片超过 50 MB 或解码失败”：基线解码器拒收后文件指针停在流中段，帧头嗅探未回到文件开头，渐进回退从未生效。嗅探先回绕再扫描，窗口按 EXIF 64 KB 规范收小，渐进路径更快。
  Fix "over 50 MB or decode failed" on progressive JPEG wallpapers past 2 MB: after the baseline decoder rejects the file the pointer sits mid-stream, so the header sniff never rewound to the start and the progressive fallback never ran. The sniff rewinds first, and its window shrinks to the 64 KB EXIF bound for a faster progressive path.

## 0.6.14 · 2026-10-16

- 修复字形光栅化分配失败仍发布空白/不完整缓存的问题，失败后释放候选、回收可重建缓存/映射并有界重试；部分填充的工作窗口先失效，避免跨页面持续缺字。
  Reject blank/incomplete glyphs after raster allocation failures; release failed candidates, reclaim optional caches/maps and retry once. Invalidate mutable glyph-window identity before writes, preventing persistent missing text across pages.
- 蓝牙进出页回收可重建字体资源；下一页缓冲延迟到实际预绘时分配，阅读返回开书失败回收后只重试一次。返回按钮沿用实际来源，不再把阅读入口标为“返回设置”。
  Reclaim font resources at BLE page boundaries and defer next-page buffers until actual preparation. Retry failed reader restoration once after reclaiming. Return follows its actual origin without calling reader entry Settings.
- 密码数字与清空/退格增加无字库、无分配应急图形，字形不可用仍能输入；密码校验和按键区域不变。
  Add font-independent, allocation-free PIN digits and clear/backspace graphics when glyphs are unavailable, preserving verification and hit regions.

## 0.6.13 · 2026-10-16

- 修复TF字体进入连接与传输后全页无字：先释放可重建缓存并完成内置解包，再提交字体切换；分配、解包或建表失败保留/恢复原字体，拒绝启动传输。提前停止蓝牙，停服返回仍同步恢复所选字体。
  Fix missing labels after entering transfer with an SD font: reclaim optional caches and finish built-in inflation before switching; allocation/inflation/table failures retain or restore the original font and gate transfer. Stop BLE first and restore the saved font before painting the return page.
- 按用户要求暂时关闭GitHub自动固件构建，仅保留手动入口；官网部署与本地构建保持。修正容器属主差异下的提交树预检，仅对当前仓库临时授权读取。
  Temporarily make GitHub firmware builds manual-only as requested, retaining website deployment and local builds. Scope committed-tree checks to the current repository when container ownership differs.

## 0.6.12 · 2026-10-16

- 修复干净构建中的过期波形依赖、SNTP条件编译和USB磁盘配置缺失；CI先检查提交树依赖与条件编译配对。官网发布版本独立维护，网站更新不再因版本文案触发固件构建。
  Fix stale waveform dependencies, an unterminated SNTP conditional and missing USB MSC configuration in clean builds. CI checks committed dependencies and conditional pairing first. Maintain the website release version separately so site version copy does not trigger firmware builds.
- 已连接翻页器跨页面保留；阅读工具新增蓝牙入口，返回原书原位置。WiFi、锁屏和关闭蓝牙仍停止无线，非正文不派发动作。
  Retain connected remotes across navigation, add a reader-tools Bluetooth entry and restore the book/position on return. WiFi/lock/off still stop radio and non-body input stays gated.
- 图片解码按运行时间让出CPU，替代固定行/图块次数休眠；PNG滤波按行分支、列坐标复用，像素保持一致。TF卡最多八个完整性校验预览槽，重开复用，源变化/损坏失效，缓存写入失败仍可查看。
  Yield image decoding by elapsed runtime instead of fixed row/block sleeps. Branch PNG filters per row and reuse source columns with identical pixels. Keep up to eight validated SD previews; reopen reuses them, source changes/corruption invalidate them, and failed cache writes never block viewing.
- 官网发布包含统一BIN入口、当前界面与烧录硬件复位修复，公开页面继续隐藏本项目仓库入口。
  Website release includes unified BIN import, current screens and the hardware-reset fix, retaining hidden project repository links.

## 0.6.11 · 2026-10-16

- 官网烧录结束后恢复必发RTS硬件复位，不再将命令应答视为设备重启；复位未确认时保留手动重启提示。
  Restore the mandatory RTS hardware reset after flashing; a protocol ACK no longer skips reset or establishes boot. Keep manual restart guidance when reset is unconfirmed.
- 修复蓝牙主动取消扫描后等待不存在的完成事件，立即发起连接；等待旧连接断开也有超时和失败反馈。
  Connect immediately after active scan cancellation instead of waiting for an event the SDK does not send; bound old-connection waits and report errors.
- 图片查看增加三张内存缓存、相邻预加载和异步加载提示，低内存减少预加载；缩放避免重复逐像素计算，保持原灰阶及GC16刷新。退出/媒体丢失汇合任务，睡眠保留当前图片以供唤醒重绘。
  Add three RAM image slots, adjacent preloading and asynchronous loading feedback, reducing preloads under memory pressure. Avoid repeated per-pixel scaling math while retaining grayscale and GC16. Join tasks on exit/media loss; retain the displayed image for wake redraw.

## 0.6.10 · 2026-10-16

- 修复封面复用将提取失败当作无封面并缓存几何占位图的问题。区分原图、确认缺失和可重试失败；后台最多重试三次，首页/书架失败槽三秒后可再取，不把失败永久记为完成。
  Fix cover reuse treating extraction failures as absent artwork and caching geometric placeholders. Distinguish originals, confirmed absence and retryable failure; retry three times per job and let failed home/shelf slots retry after three seconds instead of sticking permanently.
- 封面定位兼容XML命名空间、属性等号空白、百分号编码和相对路径点段；解码输出预算缩至128KiB，减少临时内存。原书和阅读进度不变。
  Resolve cover XML namespace prefixes, attribute whitespace, percent escapes and relative dot segments. Bound decoded output to128KiB to reduce transient memory; retain books and reading progress.

## 0.6.9 · 2026-10-16

- 官网暂时移除本项目的GitHub、源码与发行页入口，保留本地烧录和第三方许可声明。
  Temporarily remove this project's GitHub/source/release links from the public website, retaining local flashing and third-party license notices.
- 图书、图片和字体上传前统一检查目标目录、容量与小文件写入；分开报告创建、写入、保存或替换失败及错误码，避免把所有读写问题统称为空间不足。浏览器遇到应答中断时尝试查询本次设备错误，不自动重发。
  Preflight each upload kind with destination, capacity and a small staging write. Report open/write/close/commit failures and errno separately instead of a generic storage-full message. On a lost response, query the matching device failure without automatic retransmission.
- 上传使用无stdio缓冲的4KiB接收/写入块，减少联网后的临时内存需求；启动清理覆盖图片暂存与备份。失败仍保留原文件，实机TF卡与WiFi稳定性待验证。
  Use unbuffered4KiB receive/write chunks to reduce transient memory demand while networking; include image staging/backup files in startup cleanup. Failures preserve originals; actual TF/WiFi stability still requires hardware testing.

## 0.6.8 · 2026-10-16

- 官网按PageRest现有功能更新首页、原生界面图库、双语说明与手机布局；烧录A/B合并单文件入口，自动识别full/app并展示地址和数据影响，增量禁止全擦，完整包仍重置设置与阅读记录。
  Refresh the PageRest homepage, native gallery, bilingual copy and responsive layout. Merge A/B flashing into one content-detected full/app input with address/data impact; apps forbid full erase and full images still reset settings/reading records.

- 图书、图片、字体各模块直接显示上传进度、文件结果及取消入口；派发后保留本次名称和状态，不退回“尚未选择”。
  Show progress, per-file results and cancellation within each upload module, retaining dispatched names/status instead of an empty selection message.
- 速度灰阶正文减小字体曲线展平误差，缓存区分绘制精度，保留字重、分页与21相波形，不增加补刷；画质与系统字体绘制不变，光学改善待实机对照。
  Reduce curve-flattening error in gray-speed text with distinct cache keys, retaining stroke, pagination and21-phase waveforms without added updates. Quality/UI rendering stays unchanged; optical improvement awaits hardware comparison.
- 蓝牙新增独立锁屏按键学习，拒绝占用标准翻页键或其他已学习按键；仅正文请求原有锁屏事务，反转不改变锁屏动作。兼容旧双键设置，锁屏仍关闭BLE且不支持遥控唤醒。
  Learn an independent Bluetooth lock button, rejecting standard turn-key and learned-action conflicts. Only body input requests the existing lock transaction; direction reversal excludes lock. Migrate old two-button preferences; BLE still stops on lock without remote wake.

## 0.6.7 · 2026-10-16

- 按用户选择保留速度与翻页观感，撤回实机退化的灰阶速度覆盖率加深；21相灰缘、黑白快刷和画质档不变，不增加补刷或驱动剂量。
  Preserve speed/turn appearance as selected by the user, withdrawing the rejected gray-speed coverage boost. Keep21-phase gray edges, binary fast and quality without added scans/doses.
- 上传允许短暂接收超时，连续30秒无数据才终止；取消/断线仍及时失败清理暂存。仅传书STA关闭无线省电，保留有界内存配置。
  Retry transient upload receive timeouts within a30-second idle deadline; stop/disconnect still clean temporary files. Disable WiFi power saving only for transfer STA while retaining bounded memory.
- 网页选择区列出待上传文件名、大小、类型并可移除；各按钮只发送本类文件，忙碌时禁止修改队列。中断提示显示已发送字节和结果未确认状态。
  List selected names/sizes/types beside pickers with removal; each button sends only its kind, locking changes while busy. Interrupted uploads report bytes sent and unconfirmed results.
- 停止传书后，同任务在下一页首帧前恢复所选字体，避免设置列表先显示内置字体再刷新；文件丢失仍回退内置且保留选择。
  Restore the selected font on the owner task before painting the next page after transfer ownership ends, avoiding a built-in-first settings frame; missing files fall back without erasing the preference.

## 0.6.6 · 2026-10-16

- 速度灰阶档正文覆盖率轻微增加1/16像素，保留抗锯齿、字宽、原翻页波形及已保存字缘选择；画质档不变，实屏对比仍需验证。
  Add 1/16-pixel body coverage in gray-speed mode, retaining antialiasing, advances, turn waveforms and saved edge choices; quality mode is unchanged and optical comparison remains pending.
- 个人摘录正文再放大一档并保留自适应居中；锁屏按分钟更新底部时钟，深睡选择实际浅睡且不在30分钟后冻结；显式关机保留关机前时刻。
  Enlarge adaptive centered personal quotes; update the clock every minute in light sleep, including a Deep selection, without freezing after30minutes; explicit Off retains the pre-off time.
- WiFi扫描、联网及传书使用有界接收/发送缓冲，初始化内存不足时只回退重试一次最小配置；持续失败正常释放并返回错误。
  Bound WiFi scan/network/transfer buffers, retry initialization once with a minimal profile on NO_MEM and release/report persistent failures.
- 书架每本书下方只保留阅读状态，去来源文字；打卡月历说明框不再误开书。手帐三页均可设置每日阅读目标（0关闭，5分钟步进，最多360分钟），失败保留旧值。
  Remove per-book source labels; make the calendar legend inert. Set daily goals on any Journal tab (0off,5-minute steps up to360); failed saves retain the old value.
- 网站刷机器按选择串口、握手、写入阶段报告原始错误；取消不重复弹窗，写入后复位失败明确提示手动重启；软复位成功不再操作可能已重枚举的USB信号。
  Report web flasher failure stages and original errors, avoid reopening cancelled choosers and stale USB signaling after soft reset, and distinguish completed writes needing manual restart.

## 0.6.5 · 2026-10-16

- 书架使用统一工具栏双图标切换列表/封面，六封面改为三列两排，成品缩略图增至180×232；保留浏览位置、两行标题、来源进度与长按详情。
  Integrate list/cover icons into one shelf toolbar and enlarge native thumbnails to180×232 in three columns/two rows, retaining position, two title lines, progress and hold actions.
- 短摘录放大，正文与来源整体居中；长摘录保持字号自适应及来源上方回退。
  Enlarge short excerpts and center quote/source as a group, keeping adaptive sizing and above-quote sources for long passages.
- 使用用户提供并经去水印/灰阶整理的插画替换开机图，首帧完整刷新、末帧GL16短淡入，不添加固定等待；资源失败回退固件名与版本。
  Replace the original splash with cleaned user-provided art, using an initial full refresh and one GL16 fade frame without fixed waits; asset failure retains name/version fallback.
- 网站烧录器明确文件读取失败，校验应用与合并镜像；增量误选完整重装时在串口连接前拒绝，升级模式接受两种BIN并只写应用。
  Explain local file-read errors, validate app/merged images and reject app-only files in full reinstall before serial access; Upgrade accepts both and writes only the app.

## 0.6.4 · 2026-10-16

- 修复点击分区恢复按钮的长文案越界；默认分区说明独立显示。正文、工具和设置移除常驻三键提示，摘录等确认操作保留必要提示。
  Fix overflowing tap-zone reset copy with a separate default-layout note. Remove permanent body/tools/settings key hints while retaining confirmation guidance.
- 摘录锁屏移除无含义的“页”印章，均衡竖排列长并优先按句读分列，来源靠近正文；保留所选原文、真实时间/统计和英文横排回退。
  Remove the decorative stamp, balance vertical quote columns around punctuation and attach the source, retaining selected text, real time/stats and Latin fallback.
- 日间标准、黑白快刷、灰缘和揭页共用成功翻页周期，到期一次厂家GC16清理目标页；失败不累计、控件/页脚不消耗周期、手动清理重置计数。保留原波形及下电时序，实屏残影改善待验证。
  Share a successful-turn cleanup interval across day reader profiles, using one vendor GC16 target when due. Failures/controls/footer updates do not count, manual cleanup resets the interval. Waveforms/power timing stay intact; optical improvement awaits hardware validation.

- 普通构建自动同步产出 full 完整镜像与 incremental 仅应用镜像，附实际烧录地址和 SHA-256；校验版本、分区容量及合并内容，失败不发布新包。
  Normal builds produce paired full/app-only images with actual flash offsets and SHA-256, validating versions, partition capacity and merged contents before publication.

- 下一页预绘在刷屏结束时按行停止，空闲继续；连续翻到未完成目标时接着绘制，布局改变丢弃旧结果，低内存回退正常绘图。
  Stop next-page prefetch at row boundaries after display output, resume during idle and complete partial targets on rapid turns; discard stale layouts and draw synchronously under memory pressure.
- 长章节行缓冲按实际行长增长；封面有界复用最近成品，同页合并补刷，冷缓存等待不超过合并窗口再分批显示。
  Grow line scratch by actual row length, reuse a bounded set of prepared covers and coalesce shelf updates, revealing cold batches after the collection window.
- 阅读→更多设置→文字排版新增字重减/加及实时示例，点击数值恢复标准；调整独立保存，不改变字宽和当前页位置，保存失败保留旧值。
  Add stroke minus/plus with a live sample under reader typography; tap the value to reset. Persist independently without changing advances/current position; failed saves retain the previous value.

## 0.6.3 · 2026-10-15

- 增加 BLE HID 翻页器的搜索、设备确认、保存回连、按键测试、方向反转与两键学习，默认关闭，遗忘需确认，保存失败保留旧设置。
  Add BLE HID discovery, peer confirmation, remembered reconnect, key test, direction reversal and two-button learning; default off, confirm Forget and retain old settings after save failures.
- 外部输入仅在正文翻页；覆盖层/菜单丢弃输入，不合成按住连翻。离页、睡眠和 WiFi 前释放蓝牙，停止失败拦住后续操作；不支持蓝牙唤醒，具体翻页器兼容性待真机验证。
  External input turns body text only, dropping overlay/menu input without generated held repeats. Release BLE before exit/sleep/WiFi and reject the latter operations after stop failure; Bluetooth wake is unsupported and device compatibility awaits hardware validation.

## 0.6.2 · 2026-10-15

- 个人摘录锁屏增加分页选择、预览与确认；独立保存所选副本，新增和16条轮换不改变选择，保存失败可重试，旧未选状态明确回退。
  Personal locks add paged selection, preview and confirmation with an independent pinned snapshot surviving new records/rolling eviction, retry on failure and explicit unselected fallback.
- 按参考图改为竖排正文与来源、小印章及底部大号锁屏时刻、真实农历与阅读记录；英文正文横排回退，时间为静态快照，未校时不造数。
  The reference layout adds vertical text/source, a small seal, large static lock time and actual lunar/reading data. Latin body text falls back horizontally and uncalibrated time never invents values.

## 0.6.1 · 2026-10-15

- 摘录确认卡随句子长度调整，显示来源书名、取消与重试保存；阅读、工具及摘录状态显示对应三键语义，保留中键长按导航，不增加进度尺。
  Excerpt confirmation adapts to sentence length with source title, Cancel and retry. Reader/tools/excerpt states show key meanings while retaining middle-key navigation holds, without a progress ruler.
- 睡眠设置新增“个人摘录”：静态展示最新保存的原文与来源，无记录回退内置图片；不依赖校时，不按分钟重绘，沿用所选浅睡/深睡/关机策略。旧壁纸设置兼容保留。
  Sleep adds Personal Excerpt: a static latest saved quote/source, falling back to the embedded image when empty. It needs no calibrated clock or minute repaint and keeps the chosen light/deep/off mode and legacy wallpaper compatibility.

## 0.6.0 · 2026-10-12

- 增加可保存的六封面书架；默认三行保留，布局往返保持浏览位置，点击/长按与批量管理统一分页，管理选择显式勾选。
  Add persisted six-cover shelves while keeping the three-row default, browsing position across layout toggles, unified tap/hold/bulk paging and explicit checkboxes.
- 手帐图表标出日期、真实分钟及最高值，区分未校时/加载/零；月历不造日期，最近书目不冒充今日足迹，移除固定目标。
  Journal labels actual dates/minutes/maximum and separates unsynced/loading/zero; Calendar invents no date, recent titles claim no daily footprint and fixed goals are removed.
- 睡眠设置增加锁屏缩略示意与图片预览，明确实际浅睡/深睡/关机及对应唤醒，未校时与低内存有回退说明；关机时钟/黄历提示改为长按开机。
  Sleep adds miniature style/image previews, explains effective light/deep/off wake behavior with unsynced/OOM fallback, and fixes clock/almanac Off hints to hold-to-boot.

## 0.5.62-20261012.1 · 2026-10-12

- 字号面板预览当前正文文字，连续调整保持同一原文锚点；工具/字号/设置统一底栏“返回阅读”，字体页返回原面板与位置。
  Sizes preview current body text at one source anchor through repeated changes. Tools/sizes/settings share bottom Return to Reading; Fonts restores the source panel and position.
- 阅读设置分为文字、操作、显示、同步，晃动/动画单列实验页；显示模式明确画质/速度选择及字缘启用条件，清残影用次数说明。
  Reader settings separate text/input/display/sync and isolate shake/animation under Experiments. Display explicitly selects quality/speed, explains edge prerequisites and counts cleanup updates.
- 传书明确连接网络、打开上传网页两步及等待/完成/失败反馈；完成后可停止网络并去书架，接收中拒绝，停止返回保留来源。
  Transfer explains network joining and opening the upload page, with ready/completion/error feedback. Completed uploads can stop networking and open Library; active reception rejects it and Stop/Return preserves origin.

## 0.5.61-20261012.1 · 2026-10-12

- 首页主按钮改为独立宽按钮，修复无续读记录时与统计栏重叠；统计收敛为一行，最近阅读每本仅显示一次百分比。上传进度仍保留原大小与边缘容差，加载中可直接打开书架。
  Home uses a separate wide primary action, fixing its overlap with stats when no resume record exists. Stats share one row and each recent book shows its percentage once. Upload keeps its size and touch tolerance; Library remains accessible during loading.
- 固件命名为 PageRest，当前在设置页的版本行显示；设备品牌名称独立保留。
  The firmware is named PageRest, shown beside the version in Settings for now; hardware branding remains separate.

## 0.5.60-20261011.8 · 2026-10-11

- 首页“正在读”下方的三格统计条（今日阅读 / 连续打卡 / 在本书架）不再可点：此前它与“继续阅读”按钮共用同一条命中带，有书时轻点任意方框都会直接开书。现在有书时只有 Hero 卡片开书；没有书时该位置仍是“打开书架 / 导入图书”按钮。
  The three stat boxes under Home (today's reading / streak / shelf count) are no longer tappable: they shared the resume button's hit band, so any box tap opened the book. With a book only the hero card opens it now; in the empty state that slot stays the "open shelf / import" button.

- 修复“已有 WiFi”连接失败（ESP_ERR_HTTPD_TASK）：传书网页服务器此前在 WiFi 缓冲分配完之后才创建，内存紧张时 12KB 的服务任务栈拿不出来。现在网页服务器先于 WiFi 启动，且栈高 12KB 失败时自动降 8KB 重试（设备端记录每次的实际栈高与剩余内存）。
  Fix the saved-WiFi connect failure (ESP_ERR_HTTPD_TASK): the web server task was created only after the WiFi buffers claimed their memory, so its 12KB stack could no longer allocate. The server now starts before WiFi, and a failed 12KB stack automatically retries at 8KB (each attempt's stack size and free heap land in the device log).
- 传书网页（热点模式）的“配置已有 WiFi”表单接通真实接口：保存走 POST /wifi、遗忘走 DELETE /wifi，结果就地显示；此前表单没有任何脚本绑定，点“保存网络”会触发浏览器原生提交——密码进网址并中断进行中的上传队列。
  The hotspot-mode web page's "configure existing WiFi" form now reaches the real endpoints: saving POSTs /wifi and forgetting DELETEs /wifi with inline results; the form previously had no script wiring, so "保存网络" triggered a bare browser submit that leaked the password into the URL and killed any running upload queue.
- 其余打磨：图片查看器中段轻点不再误触退出（返回列表只走按钮），锁屏图片页进页清空上一轮消息；传输页编辑同步服务器/用户名/密码时标题随字段变化，不再挂着“WiFi 密码”和无关网络名；启动失败 3 秒后的自动重试在状态卡预告；设置→进阶与诊断页头与入口同名；阅读点击分区的“恢复默认分区”文案改为如实描述整列布局；手帐“今日手记”更名“今日概览”；睡眠页“图片”芯片只负责选中样式，进入挑选走标题行新出现的“去挑选 ›”；字体页补 TF 卡失效回调（列表即时重扫）。
  Further polish: the image viewer's middle third no longer exits by accident (leaving uses the button) and the lock-image page clears stale messages on entry; the transfer password editor retitles itself for sync fields instead of showing "WiFi 密码" with an unrelated SSID; the three-second automatic retry is announced in the status card; the diagnostics page header matches its Settings entry; the tap-zones "restore defaults" label now describes the real column layout; the Journal "今日手记" tab renames to "今日概览"; the Sleep "图片" chip only selects the style while a new "去挑选 ›" header link opens the picker; the font page gains a media-loss callback that rescans the list.

## 0.5.59-20261011.7 · 2026-10-11

- 多档设置改为底部选择面板，一次点定：阅读“更多设置”里的行距、页边距、行辅助线、对齐方式、自动翻页、清残影周期（7 档），以及设置→睡眠的“空闲锁屏”（关/5/10/30 分钟），轻点行即弹出面板、点选项即刻生效，不再逐档连点连闪；两档项（缩进、段距、晃动、翻页动画等）保持就地开关。可选项行值尾新增 “›” 提示。
  Multi-tier settings now use a bottom choice sheet for a one-tap pick: leading, margins, guide rules, alignment, auto page-turn and the 7-tier ghost-cleanup period in the reader's grouped settings, plus Settings→Sleep's idle lock (off/5/10/30 min). Tapping the row opens the sheet and tapping an option applies at once — no more cycling through tiers with a flash per step; two-state items stay in-place toggles. Sheet rows mark their value with a trailing “›”.
- 危险动作统一为底部确认面板：USB 连接电脑不再把“重新检测/USB”两键原地变身为“取消/确认连接”，而是弹固定面板（标题+风险说明+左取消/右实底确认）；“遗忘网络”从整页确认迁到同款面板；锁屏密码的“清除密码”补上确认（此前一点即清除）。面板上方轻点等同取消。
  Dangerous actions share one bottom confirm sheet: USB connection no longer relabels the Refresh/USB buttons into Cancel/Confirm — it opens the fixed sheet (title + risk line + Cancel left / solid Confirm right); “遗忘网络” migrates from its full page onto the same sheet; the lock-PIN “清除密码” gains its missing confirmation (previously one tap cleared). A tap above the sheet cancels.
- 返回语义统一：锁屏图片页的“返回”（右上与底栏）改为回到进入来源而不是固定跳睡眠设置页；传书“选择 WiFi”的“返回传书”只切回传书页、不再悄悄重启网络会话（要点哪个模式回首页点即可）；传书“进度同步”底栏补上“返回”键，不再只有右上入口；锁屏密码页补画菜单把手，全机逃生口一致。
  Back semantics unify: the lock-image page's Return (header and bar) goes back to the entry origin instead of a fixed jump to sleep settings; the transfer WiFi list's “返回传书” only switches back to the transfer page and no longer silently restarts the networking session (tap a mode on Home to restart); the transfer sync view gains a bottom-bar Return instead of a header-only exit; the PIN page draws the menu handle like every other page.
- 手帐页不再每分钟整页闪：改为只在跨日（统计与打卡月历真正变化的时刻）重绘；同步页各控件补按下灰底反馈。
  The Journal page no longer flashes fully every minute: it repaints only when the day flips (when stats and the calendar truly change); the transfer sync view's controls gain the gray press feedback.

## 0.5.58-20261011.6 · 2026-10-11

- 触控按下即时反馈推广到全部产品页：设置（含阅读/时间/睡眠/存储/诊断子页）、首页、手帐、字体列表与锁屏图片页的按钮和行控件按下立即显示灰色按压底（局部快速刷新，不整页闪），滑出取消、抬起命中同一目标才生效——与锁屏密码键盘同规则。此前这些页面点击后毫无反应，要等整页刷新完成才知道结果，也容易诱发重复点击。
  Instant press feedback now covers every product page: buttons and rows on Settings (with the reading/time/sleep/storage/diagnostics children), Home, Journal, the font list and the lock-image page light up with a gray press bed on touch (a local fast refresh, no page flash), sliding off cancels and only a release over the same target commits — the same rule as the lock keypad. These pages previously stayed silent until the full-page refresh finished, inviting double taps.
- 首页点击热区修正：最近阅读列表第一行右侧的进度胶囊此前被“全部图书”隐形热区覆盖，点它想打开这本书却会跳去书架；行下方还有一段不看横坐标的兜底热区。现在“全部图书 ›”只保留标题行一处，最近阅读整行（含进度胶囊）都打开对应的书；“上传进度/停止”按钮从 156px 加宽到 176px 并保留边缘容差，减少误触旁边的“继续阅读”。
  Home hit zones fixed: the progress pill on the first recent-reading row was covered by an invisible “全部图书” zone and jumped to the shelf instead of opening that book, plus an x-less fallback band sat below the rows. “全部图书 ›” now keeps a single zone on its own header row, the whole recent row (pill included) opens its book, and the upload/stop button widens from 156px to 176px with edge tolerance to miss “继续阅读” less.
- 拒绝不再静默：传书页上传/删除进行中切换“设备热点/已有 WiFi”时，状态卡就地提示“正在上传或删除，完成后再切换”（数秒后自动撤下）；时间页时区到边界时提示“时区已到上限/下限”；阅读设置字号到 36/80 边界时提示到界。设置里无传书能力的“连接与传书”行改浅色禁用样式，不再与可点行同观感。
  Rejections stop being silent: switching transfer modes while an upload or delete runs explains “正在上传或删除，完成后再切换” in the status card (withdrawn after a few seconds); the time page reports the timezone bound; the reading-settings size steppers report the 36–80 limits. The Settings transfer row on devices without that capability gains a light disabled look instead of matching actionable rows.
- 时间页“时间未校时 · 点击联网对时”这行提示文字现在真的可以点（并入联网对时触区）；手帐打卡月历底部说明卡的隐形开书热区按下时显示按压底，可被发现，命中判定也统一为标准“起落同矩形”规则。
  The time page's “时间未校时 · 点击联网对时” line is now actually tappable (folded into the sync target), and the Journal month tab's hidden open-book card reveals itself with a press bed; its hit test joins the standard same-rect down/up rule.

## 0.5.57-20261011.5 · 2026-10-11

- EPUB 开书提速：解析结果（章节表、累计偏移、标题）落盘缓存到 TF 卡，以 ZIP 中央目录指纹钉住文件身份——换书/改书自动失效。再次开书（含深睡唤醒的自动续读）跳过 container/OPF/NCX/NAV 全套 XML 解析，直接从缓存恢复章节表；无目录书的正文补目结果也会回写，首次补目后不再重复扫描。实测 749 章的书解析与恢复耗时约 17ms 对 1ms（设备上对应零点几到两秒对一次小文件读取）。章节表超过 512 KB 的超大型书自动跳过缓存，照常解析不受影响。
  Faster EPUB opening: the parsed structure (chapter table, cumulative offsets, titles) persists to the TF card keyed by the ZIP central-directory fingerprint, so replaced or edited books miss automatically. Reopening (including deep-sleep auto-resume) skips the whole container/OPF/NCX/NAV XML parse and restores the chapter table from cache; body-heading TOC fallback results are written back too, so the scan never repeats after its first run. Measured 17ms parse versus 1ms restore on a 749-chapter book (sub-second to two seconds versus one small file read on device). Books whose table exceeds 512 KB skip the cache and parse as before.

## 0.5.56-20261011.4 · 2026-10-11

- 修复渐进式 JPEG 封面整类不显示（如《重生之大涅槃》）：此类封面需要约 14.7MB 的整幅解码缓冲，超出内存上限被直接拒绝。引入参考固件同款 JPEGDEC 解码器，只读 DC 扫描输出 1/8 分辨率灰度（清晰度略降但完全可辨），内存与文件两路都支持——渐进封面的书正常显示封面，查看器里大于 2MB 的渐进 JPG 也能打开（此前不支持）。
  Fix progressive-JPEG covers vanishing wholesale (e.g. "重生之大涅槃"): such covers needed a ~14.7MB whole-image decode buffer and were rejected outright. The JPEGDEC decoder shared by the reference firmwares now reads the DC scan only, producing a 1/8-resolution grayscale (slightly soft but fully legible) on both the memory and file paths — books with progressive covers show their covers again, and progressive JPGs over 2 MB open in the viewer (previously unsupported).
- 深睡唤醒/重启不再等书架扫描才回正文：断电唤醒的自动续读只等 TF 卡探测完成（几百毫秒）即直接开书回到进度，书架扫描转入后台补齐；退出阅读回书架时目录已就绪，后台扫描结算也不会再打断正文。书架大、书多的设备开机进书明显变快。
  Deep-sleep wakes and reboots no longer wait for the shelf scan before the text: the auto-resume opens the book straight to its position once the TF probe finishes (a few hundred milliseconds) while the shelf scan fills in afterwards; the catalog is ready when leaving the reader, and a background scan settling never interrupts the text again. Devices with large shelves enter the book noticeably faster at boot.

## 0.5.55-20261011.3 · 2026-10-11

- 解除图片 2MB 上限：解码改为直接从 TF 卡文件流式读取（PNG 固定窗口逐段喂入解压器、JPEG 读回调指向文件，压缩数据不再整块驻留内存，与参考固件同架构）。网页上传图片单文件上限提到 20 MB，设备端查看/锁屏壁纸支持卡上 ≤50 MB 的 JPG/PNG（含读卡器直拷的大照片）；渐进式 JPEG 超过 2 MB 仍不支持（小的照常解码）。
  The 2MB image cap is lifted: decoding now streams straight from the TF-card file (PNG feeds the inflater through a fixed window, the JPEG read callback points at the file — compressed data no longer resides whole in RAM, matching the reference firmwares). Web uploads accept up to 20 MB per image, and the viewer/wallpaper handles on-card JPG/PNG up to 50 MB (including photos copied via a card reader); progressive JPEGs over 2 MB remain unsupported (small ones still decode).

## 0.5.54-20261011.2 · 2026-10-11

- 修复 KOReader 进度同步后阅读字号被重置变小：拉取应用只采用远端位置，字号保留这本书本机记录的档位，不再恢复推送时档位或全局默认。同时把进度记录与设置层的字号上限从 72 对齐到阅读器的 80（此前 76/80 两档下进度无法保存，同步后必回落 48）。
  Pulling KOReader progress no longer resets the reading font size: applying keeps this book's local tier instead of the push-time or global default. Progress records and settings now accept the reader's full 36–80 range (the 76/80 tiers previously failed to save and always collapsed to 48 after a sync).
- 图片查看页新增“上一张 / 下一张”按钮：主按钮行（设为锁屏/清除壁纸/返回列表）上方加一整行两枚宽键，查看中直接翻图，无需返回列表重选；第一张/最后一张时对应键置灰，左右滑动手势仍然可用，翻页仍为整页 GC16 全灰度刷新。
  The image viewer gains previous/next buttons: a full-width pair sits above the main bars so browsing stays in the viewer without a list detour; the pair greys out at either end, swipes keep working and turns still present with full GC16.

## 0.5.53-20261011.1 · 2026-10-11

- 修复大图直开复发“图片超过 2MB 或解码失败”：设备上解码需要“文件数据 + 整条压缩流副本”两块 1.4MB 连续内存同时存在，运行一段时间后堆碎片化导致副本分配失败。现在压缩流不再复制——IDAT 数据段直接逐段喂入解压器，唯一的大块内存只剩读取的文件本身；同时解码链各失败阶段（文件头/段表/内存分配/行校验）都会留串口日志，复发时可精确定位。
  Fix the recurring "over 2MB or decode failed" on large images: decoding needed the file data plus a full copy of the compressed stream — two concurrent 1.4MB blocks — and heap fragmentation after long uptime killed the copy. The stream is no longer copied: IDAT segments feed the decompressor directly, leaving the loaded file as the only large block; every decode failure stage (header/segments/allocations/row checks) also logs over serial for precise diagnosis of any recurrence.
- WiFi“已保存，连接失败”两处强化：失败提示现在带具体错误码（如 ESP_ERR_XXX，与扫描失败同款），下次复发时把括号里的内容反馈即可定位；启动失败 3 秒后自动重试一次（覆盖锁屏解锁后的自动重启与瞬时故障），用户点按接入方式会重新武装重试。
  Two WiFi hardenings for "saved, connect failed": the failure hint now carries the specific error code (like scan failures) — report the parenthesized text to pinpoint a recurrence; a failed start auto-retries once after three seconds (covering unlock-restarts and transient faults), re-armed by an explicit mode tap.

## 0.5.52-20261010.14 · 2026-10-10

- 修复上一版“密码页按电源键回锁屏”引入的回归：按电源键唤醒后密码页一闪而过、立刻又锁屏，无法输入。原因是电源键“按下”和“松开”各排一条事件——唤醒消费了按下事件，松开事件的残余被密码页入口误读成重锁请求。现在密码页开始前先等唤醒键松开并抽干残余事件，再进入输入循环；输入途中按电源键回锁屏、开机密码页按电源键转软睡的行为保持不变。
  Fix the regression from the previous power-key-relocks-entry change: waking with the power key flashed the PIN page and instantly relocked, blocking input. The power key queues one event on press and another on release — the wake consumed the press while the leftover release event was mistaken for a relock request at entry. The PIN page now waits for the wake key's release and drains the leftover before entering the input loop; mid-entry relocking and the boot-page soft sleep behave as before.

## 0.5.51-20261010.13 · 2026-10-10

- 密码输入页按电源键回到锁屏：此前输入锁屏密码途中按电源键无效（事件被丢弃）。现在立即放弃输入、重画锁屏面再睡一轮；开机密码页按电源键则直接转软睡（不想解锁就再睡）。输对密码解锁的路径不变。
  Power key on the PIN entry page returns to the lock face: pressing it mid-entry used to do nothing (events were drained). Entry now aborts at once, repaints the lock face and sleeps another round; on the boot PIN page the key drops into soft sleep instead. Correct-entry unlocking is unchanged.
- 修复锁屏壁纸又回退默认图：阅读中锁屏时解码壁纸仍可能撞上内存压力（文件数据+压缩流+解码峰值），失败即回退内置图。现在“设为锁屏”时把已解码的灰度快照存到 TF 卡（`/sdcard/.wallpaper`，带源文件身份校验），锁屏直接读快照（约 600KB，不再解码）；快照缺失或身份不符（图片被替换）时回退实时解码并在成功后自动补写快照。
  Fix the wallpaper reverting to the default again: locking while reading could still hit memory pressure during wallpaper decode (file data + packed stream + decode peak) and fall back to the built-in image. Setting a wallpaper now persists the decoded grayscale snapshot to the TF card (`/sdcard/.wallpaper`, identity-checked against the source file); the lock reads the snapshot directly (~600KB, no decoding), falling back to a live decode when the snapshot is missing or stale (replaced image) and rewriting it on success.
- 行辅助线贴近文字：网格模式下辅助线固定在行槽底部，与本行文字之间悬空整整一段行距，看起来脱节。现在统一画在文字底部下方四分之一行距处（网格与图文章节同式），依然不会进入任何页面的文字带。
  Per-line guide rules hug their text: grid-mode rules were pinned to slot bottoms, floating a full leading away from their line. Both grid and illustration modes now place rules a quarter of the leading below the text, still outside every page's text band.

## 0.5.50-20261010.12 · 2026-10-10

- 耗电审查修复两处：① 修复在传书页按电源键锁屏后设备永不入睡——WiFi 运行会让浅睡被拒，锁屏循环退化成 20ms 空转、CPU 和射频满功率跑到没电。现在锁屏准备会先停止传书会话（与离开传书页同语义），解锁回到传书页后自动重启会话恢复原状态；深睡/关机路径不受影响。② 触摸采样降频：此前无论是否触摸都以 125Hz 盲轮询触摸芯片（持续的 I2C 事务与 CPU 唤醒），现在手指按住时保持全速、松开后降为 50Hz 兜底轮询，并接上此前未使用的 CST836U 中断线——按下瞬间由中断立即唤回，触摸起始无额外延迟。阅读静止时的平均电流应明显下降。
  Two fixes from the power review: ① locking from the transfer page never slept — a running radio rejects light sleep and the lock degraded into a 20ms busy loop with CPU and radio at full power until the battery died. Lock preparation now stops the transfer session (same semantics as leaving the page) and unlock auto-restarts it; deep-sleep and power-off paths are unaffected. ② Touch sampling slows down: the touch chip used to be blindly polled at 125Hz whether touched or not (constant I2C traffic and CPU wakeups); full speed now applies only while the finger is down, idle drops to a 50Hz fallback, and the previously unused CST836U interrupt line wakes the sampler instantly on touch-down with no added latency. Idle reading current should drop noticeably.

## 0.5.49-20261010.11 · 2026-10-10

- 深睡/关机/久置降级唤醒后回到锁屏那一刻的页面：断电级睡眠前把当时的页面记入 NVS，下次开机跳过首页直达该页；锁屏时正在读书则自动重新打开那本书并跳回阅读进度（等书架扫描落定后不再弹"继续上次阅读？"，直接进正文）。书被删、开书中断保护或卡未就绪时安静落在书架。此前的唤醒固定回首页、需要手动点"继续阅读"等待重新加载。
  After deep-sleep, power-off or the idle descent, boot returns to the page showing when the lock happened: the page is recorded into NVS before power cuts and the next boot goes straight to it; a reader holding live text reopens that book at the saved position once the shelf scan lands (no resume prompt, straight into the text). Deleted books, interrupted-open protection or missing media land quietly on the shelf. Previously every wake landed on home and required tapping through resume.

## 0.5.48-20261010.10 · 2026-10-10

- 优化误触电源键锁屏后的耗电：此前浅睡锁屏（含设置了深睡但锁屏面为时钟/日历/黄历的情况）会每分钟唤醒刷新画面，无人操作也整夜运行。现在锁屏后连续 30 分钟只有分钟唤醒、没有任何按键或拿起时，自动转入深睡（软睡）——面板保留当前锁屏画面，短按电源键重新开机；30 分钟内任何按键或拿起仍照常立即唤醒返回原页。设置页“浅睡”的说明同步更新。
  Improve battery after an accidental power-key lock: light locks (including deep-sleep users whose lock face is clock/calendar/almanac) used to wake every minute and refresh forever untouched. After thirty minutes of nothing but minute wakes, the lock now descends to soft sleep — the panel keeps the face and a short press re-powers — while any key or pickup within the window still wakes to the previous page instantly. The settings page's light-sleep description is updated to match.

## 0.5.47-20261010.9 · 2026-10-10

- 修复约 1 兆像素 RGBA 壁纸在设备上仍提示“图片超过 2MB 或解码失败”：上一版放宽了像素上限，但真机上的实际瓶颈是解码内存——文件数据（1.4MB）+压缩流副本（1.4MB）+整幅原始像素缓冲（RGBA 4.2MB）并发约 7MB，8MB PSRAM 必然失败。PNG 解码改为 ROM miniz 环形流式还原：32KB 解压环喂给滚动两行缓冲，边解压边去滤边降采样，不再分配整幅原始缓冲；峰值内存降到约 2MB（文件数据+压缩流+输出）。864×1216 RGBA 已用主机 ASAN 验证与独立参照实现逐字节一致。附带收益：压缩率高的更大 PNG（如截图类）现在也能解码，仅受 4096 像素边长与 2MB 文件上限约束。
  Fix ~1-megapixel RGBA wallpapers still failing on-device with "over 2MB or decode failed": the previous round relaxed the pixel cap, but the real bottleneck on hardware was decode memory — file data (1.4MB) plus the packed-stream copy (1.4MB) plus the full raw pixel buffer (4.2MB RGBA) ran to ~7MB concurrent, inevitably failing on 8MB PSRAM. PNG decoding now streams through ROM miniz's ring mode: a 32KB inflate ring feeds a rolling two-row buffer that unfilters and downsamples on the fly, never allocating the full raw image; peak memory drops to roughly 2MB (file data + packed stream + output). The 864x1216 RGBA file is verified byte-exact against an independent reference under host ASAN. Bonus: larger but well-compressed PNGs (screenshot-type content) now decode too, bounded only by the 4096-pixel side limit and the 2MB file cap.

## 0.5.46-20261010.8 · 2026-10-10

- 修复约 1 兆像素的 PNG 壁纸无法打开（提示“图片超过 2MB 或解码失败”）：解码器对输入图片有一道“最多 100 万像素”的平面上限，864×1216（=1,050,624 像素）只超了 2048 像素就被拒。该上限本意是控制内存，现在改为按颜色通道数精确计算解码峰值内存再判定：RGBA 壁纸截图按新规则可解（实测 864×1216 解为 648×912），解码总内存上界从 4 MB 提到 6 MB，输出尺寸与单文件 2 MB 上限不变，超界图片仍会被拒绝。灰度 PNG 的可接受面积也随之扩大一倍。
  Fix ~1-megapixel PNG wallpapers failing to open ("over 2MB or decode failed"): the decoder enforced a flat 1-megapixel input cap and 864x1216 (=1,050,624 pixels) missed it by just 2048 pixels. The cap guarded memory, so acceptance now derives from the exact per-channel peak decode heap: RGBA wallpaper shots decode under the new rule (864x1216 verified to 648x912), the total decode memory bound rises 4→6 MB, output-size and 2 MB file caps are unchanged, and oversize images still reject. Grayscale PNGs gain twice the acceptable area.

## 0.5.45-20261010.7 · 2026-10-10

- 修复在线传书时长时间不操作被自动锁屏：锁屏浅睡会断开网页与设备的连接，上传中途或等待电脑连接到点就锁。现在传书会话在线（含启动中与自动同步）期间不触发空闲自动锁屏，电源键手动锁屏不受影响；点“停止”或离开传书页后恢复正常锁屏。USB 挂载模式本来就是专用启动路径（不跑事件循环、不休眠），不受空闲锁屏影响。
  Fix idle auto-lock interrupting online transfers: lock light-sleep drops the webpage link, so waiting for or mid-way through an upload the device would lock on schedule. A live transfer session (starting or auto-syncing included) now suppresses idle locking, while manual power-key locking still works; pressing stop or leaving the page restores normal locking. USB disk mode already runs its dedicated boot path (no event loop, no sleep) and is unaffected by idle locking.

## 0.5.44-20261010.6 · 2026-10-10

- 传书网页“卡上内容”改为三个管理页签：图书管理 / 字体管理 / 图片管理（原“目录浏览”页签移除）。字体与图片各自列出 TF 卡对应目录的全部文件并支持网页内删除，与图书删除同一套确认与准入；删除壁纸后锁屏自动回退默认图，删除在用字体会回退内置字体。上传进度与结果也移出“上传图书”卡片，独立成“上传任务”卡——此前不管传的是字体还是图片，进度条和结果都显示在图书卡片里，看起来像“字体跑进了图书”。
  The transfer page's on-card section becomes three management tabs: books / fonts / images (the old directory-browsing tab is gone). Fonts and images each list their whole TF directory and can be deleted right in the webpage under the same confirmation and admission as book deletion; deleting the wallpaper falls the lock back to the default image and deleting the active font falls back to the built-in one. Upload progress and results also move out of the book card into a dedicated task card — previously every upload, fonts and images included, showed its progress inside the book card and read as "fonts landing among the books".

## 0.5.43-20261010.5 · 2026-10-10

- 修复 WiFi 卡在“正在连接已保存网络”：连接看门狗在“连上→掉线”循环里会被反复重置（每次掉线重连都清零 15 秒期限与尝试次数），路由器信号不稳时永不报错。现在累计掉线 4 次即判失败提示重试；单次掉线重连行为不变。
  Fix WiFi stuck at "connecting to the saved network": the connection watchdog reset on every connect-drop cycle (each reconnect cleared the 15-second deadline and attempt count), so unstable routers never surfaced an error. Four accumulated drops now fail the round with the retry hint; a single drop still reconnects as before.
- 修复锁屏壁纸偶发回退内置图：每次锁屏都重新整幅解码，阅读后的内存压力下偶发分配失败就回退默认图（下次成功是因为内存状态变了）。壁纸解码结果现按路径+大小+修改时间缓存，首次成功后每次锁屏直接绘制不再依赖临时内存；文件缺失或解码失败会留串口日志便于定位。
  Fix the wallpaper lock occasionally reverting to the built-in image: every lock re-decoded the full image, and post-reading memory pressure could fail the allocation once (the next lock succeeded only because memory state changed). The decoded wallpaper is now cached by path+size+mtime — after the first success every lock paints straight from the cache without transient allocations; missing files and decode failures leave serial logs for diagnosis.

## 0.5.42-20261010.4 · 2026-10-10

- 在线传书页面全面整改图片上传：图片卡片上移到图书之后不再折叠在页面底部；“上传图书”拖入区自动分流——JPG/PNG 拖进来直接排进图片队列（不再混入图书结果列表的观感），未知扩展名当场提示；同一文件重复拖入只保留一份（此前重复排队造成多条“网络中断”记录）；目录浏览新增“图片”芯片（root=images 只读浏览 TF images 目录），上传结果一目了然。设备书架本身只收录 TXT/EPUB，图片不会进书架。
  The transfer page reworks image uploads end to end: the image card moves up right after books instead of hiding below the fold; the book dropzone auto-routes — dropped JPG/PNG files join the image queue directly (no longer reading as books mixed into the shared results list) and unknown extensions prompt on the spot; dropping the same file repeatedly keeps a single copy (previously each repeat queued again, leaving stray "network interrupted" rows); directory browsing gains an image chip (root=images reads the TF images directory), making uploads easy to verify. The device shelf itself only indexes TXT/EPUB, so images never appear as books.
- 修复 0.5.41 灰缘减法拆对导致的笔画变淡：上一版把旧灰并列档的剂量“减并列深度”，重叠区药量被削薄、部分字变淡。降灰行回退纯差值，并只在并列组的最深档**追加**迟到脉冲（加法拆对，只加深不变浅），升向行回退纯差值——任何档位的落点都不会比 0.5.40 更浅，白底阶梯与厂家端点不变，翻页仍 21 相单向无闪。
  Fix 0.5.41's faint strokes from subtractive tie-splitting: the previous version subtracted tie depth from old-gray doses, thinning text-overlap regions. Descent rows return to pure differences with one ADDITIVE late pulse on each tie group's darkest member (splitting darker only, never lighter) and climbing reverts to pure differences — no target lands lighter than 0.5.40, with the white ladder and vendor endpoints unchanged; turns stay 21 one-way flash-free phases.

## 0.5.41-20261010.3 · 2026-10-10

- 灰缘直刷锯齿再进一步：0.5.40 解决了偶发偏淡，残余锯齿来自文字重叠区——旧灰行按撤销计数差值给药，而计数表里 2&3、4&5&6、7&8、9&10 同值，这些档位落点仍成对塌陷。半档拆对推广到全部旧灰行（降灰=差值减并列深度+相14迟到脉冲；升向=差值加并列深度减一+迟到脉冲），方向均为更浅档落更浅；翻页仍 21 相单向无闪，白底阶梯与厂家端点不变。
  Gray-edge jaggies improve once more: 0.5.40 resolved the faint strokes, and the remainder lives in text-overlap regions — old-gray rows dose by undo-count differences, where 2&3, 4&5&6, 7&8 and 9&10 tie and still collapse in pairs. The half-step split now covers every old-gray row (descent = difference minus tie depth plus the phase-14 late pulse; climbing = difference plus tie depth minus one plus the late pulse), always landing lighter targets lighter; turns stay 21 one-way flash-free phases with the white ladder and vendor endpoints unchanged.
- 修复图片锁屏残影：壁纸照片是大面积混合灰阶，单次差分 GC16 擦不净上一页画面。锁屏入口改为先物理清白再按厂家 GC16 重画（与手动清残影同路径）；时钟/日历/黄历与内置图锁屏保持原单次 GC16 不变。
  Fix wallpaper lock ghosting: photo wallpapers carry full-screen mixed grays that a single differential GC16 cannot fully clear, so the lock entry now physically clears white and repaints with vendor GC16 (the same path as manual cleaning); clock/calendar/almanac and built-in-image locks keep their single GC16 pass.
- 修复图片目录问题拖垮传书连接：TF 卡 images 目录创建或清理失败此前会使整个传书会话启动失败，界面呈现“已保存，连接失败，请重试”。现在降级为图片上传不可用（网页隐藏图片卡片），图书/字体传书不受影响。若换网后仍连接失败，请提供串口 `transfer` 日志（`start busy …` 与 `wifi heap …` 行）。
  Fix image-directory failures sinking transfer connections: a TF images directory that failed to create or clean used to fail the whole session, surfacing as "saved, connect failed, retry". It now degrades to image uploads unavailable (the page hides the image card) without touching book or font transfers. If changing networks still fails to connect, provide the serial `transfer` lines (`start busy …` and `wifi heap …`).

## 0.5.40-20261010.2 · 2026-10-10

- 在线传书新增图片上传：传书网页新增“上传图片”卡片，JPG/PNG 经 `/images` 接口保存到 TF 卡 images 目录（单文件 2 MB，与查看端解码上限一致），提交前按 JPEG/PNG 魔数校验，改名文件直接拒绝；无 TF 卡时给出提示。上传后在设备“设置→睡眠与锁屏→图片”里查看并设为锁屏。
  Online transfer gains image uploads: the transfer page adds an image card routing JPG/PNG through `/images` into the TF images directory (2 MB per file, matching the viewer's decode cap), magic-checked before commit so renamed non-images reject outright, with a hint when no TF card is present. View and set as lock on the device under Settings→Sleep & lock→Image.
- 图片显示增强：解码后先做 0.5%/99.5% 百分位自动色阶（窄对比照片在 16 级灰上明显改善；满幅与近单色图不动），再居中裁切与 16 级抖动。参考 RickyOS 对同款面板的做法（保留全部 16 级灰）保留灰阶层次，叠加自动色阶提升对比度。
  Image display improves: decoded photos take a 0.5%/99.5% percentile auto-level pass (narrow-contrast photos read far better across 16 grays; full-range and near-mono images stay untouched) before the cover-crop and 16-level dither. Following RickyOS's same-panel approach of keeping all 16 grays, the gray ramp stays while auto-leveling lifts contrast.
- 灰缘直刷拆开相邻落点：0.5.39 后横竖笔画欠直——相邻灰档共用整数剂量（每脉冲约两级），落点成对塌陷，边缘梯度两级一档。奇数档 3/5/7/9/11 改为短一档主段加相 14 迟到脉冲（间隔后单脉冲剂量较弱，落点介于两档之间），有效剂量呈半档步进；厂家端点行本就是“前导段+相17迟到脉冲”结构。笔画偶发偏淡与档位成对相关，同轮拆开；仍 21 相单向无闪，若迟到脉冲等效满脉冲则落点退回上一版不会更差。
  Gray-edge direct splits adjacent landings: after 0.5.39 H/V strokes still read crooked — adjacent gray targets share integer doses (about two levels per pulse), landing in pairs and stepping the edge gradient two levels at a time. Odd targets 3/5/7/9/11 now run one dose shorter plus a late pulse at phase 14 (a pulse after a rest doses less, landing between the two integer doses) for half-step effective spacing; vendor endpoint rows already use leading-run-plus-late-tick form. The occasional faint strokes track the same pairs and split together; still 21 one-way flash-free phases, and if the late pulse reads as full, landings fall back to the previous version, never worse.
- 锁屏样式去掉“静态图”，统一为图片：样式改为 图片/时钟/日历/黄历，默认图片——有壁纸画壁纸，无壁纸或读取失败回退原内置静态图；“图片”芯片选中样式并进入锁屏图片页，清除壁纸即回内置图。旧固件的壁纸样式值自动并入图片样式。
  The static lock option is removed, unifying on image: styles become image/clock/calendar/almanac with image default — wallpaper when present, the former built-in static image otherwise; the image pill selects the style and opens the picker, and clearing the wallpaper returns to the built-in image. Legacy wallpaper style values merge into image at load.

## 0.5.39-20261010.1 · 2026-10-10

- 灰缘直刷浅端剂量微调：实机反馈观感再进一步、字芯不发白，仅余轻微锯齿。按落点模型最近落点修正两个偏浅目标——15→12 由 5 个黑脉冲改为 4 个（原落在约 10，偏浅近两级）、15→13 由 4 改为 3，浅端外圈梯度恢复连续；深端含 15→1 的厂家成形位不变，仍 21 相单向无闪。
  Gray-edge direct fine-tunes the light end: device feedback reads one step better with sound cores and only mild jaggies. Two light-biased targets take nearest-landing corrections — 15→12 drops from five black pulses to four (it landed near 10, almost two levels light) and 15→13 from four to three, restoring a continuous outer gradient; the dark end including 15→1's vendor formation level stays, still 21 one-way flash-free phases.
- 修复重启后从阅读直达书架不加载：开机直开续读时书源目录从未扫描，阅读工具条“书架”切到书架后为空，须底部页签重进才触发扫描。现在从阅读/设置进入书架的三个入口都会在目录无效时补上进入扫描（含存储探测与提示文案）。
  Fix the shelf staying empty when entered from reading after a reboot: booting straight into a resumed book never scans the catalog, so the reader toolbar's shelf switch showed nothing until a bottom-tab re-entry. All three shelf entries from reading/settings now kick the entry scan when the catalog is invalid (with storage probing and the scanning notice).
- 新增锁屏图片：TF 卡 `pictures`/`images` 目录的 JPG/PNG 可在锁屏图片页整页灰阶查看（左右滑动或点按两侧切换），“设为锁屏”把壁纸写入设置并把锁屏样式切到图片，“清除壁纸”恢复默认静态图；壁纸按原比例居中裁切整屏绘制并做 16 级有序抖动，缺图/超 2 MB/解码失败自动回退默认锁屏。入口在设置→睡眠与锁屏的“图片”芯片；壁纸不随分钟变化，深睡策略与静态图一致。单文件上限 2 MB、目录列表至多 96 张，照片过大请先在电脑缩小。
  Lock images arrive: JPG/PNG under TF `pictures`/`images` view full-screen in grayscale on the lock-image page (swipe or tap the sides to switch); "set as lock" stores the wallpaper and switches the lock style to image, and "clear wallpaper" restores the default static face. Wallpapers cover-crop at the original aspect with 16-level ordered dithering, falling back to the default lock when missing, over 2 MB or undecodable. The entry is the image pill in Settings→Sleep & lock; wallpapers never change per minute and share the static deep-sleep policy. Files cap at 2 MB with at most 96 listed entries — shrink oversized photos on a desktop first.

## 0.5.38-20261009.8 · 2026-10-09

- 翻页动画开关并入设置行区：去掉“（日间标准）”括号文案，改为“翻页显示”组内标准行（自动翻页下方、清残影周期之上），不再与底部“清残影/返回阅读”按钮重叠；同轮修复该组“清残影周期”档位自 0.5.35 起不显示当前值的问题。
  The page-turn animation switch joins the settings rows: the parenthetical label text is dropped in favor of a standard row inside the page-display group (under auto turn, above the cleanup tier), no longer overlapping the bottom clean/return buttons; the same pass fixes the cleanup tier showing no current value since 0.5.35.
- 灰缘直刷重校白底降灰剂量：厂家灰8表对每个灰目标都是 B10 成形加 W 回拉，说明白→黑趋进约 10 个黑脉冲即近饱和；0.5.36 按 DU 撤销行差值算出的 13–16 个黑脉冲把灰缘整体压到近黑、抗锯齿梯度塌缩。白底降灰现按厂家锚点陡降曲线给药（4 脉冲未出浅区、10 脉冲即成形位、18 满黑，to=1 保持厂家成形深度），旧灰差值与升向白侧不变；仍 21 相单向无闪、端点厂家 DU，灰阶落点请实机再验。
  Gray-edge direct recalibrates white-background descent doses: the vendor gray8 table forms every gray target with B10 plus a W pull-back, proving the white-to-black approach saturates near 10 black pulses; 0.5.36's undo-row difference doses of 13–16 pulses pushed fringes toward black and collapsed the anti-aliasing gradient. White-background descent now doses along the vendor-anchored plunge curve (4 pulses still light, 10 at the formation level, 18 full black, with to=1 keeping the vendor formation depth) while old-gray differences and the white side stay; still 21 one-way flash-free phases with vendor DU endpoints — gray landings need device revalidation.
- 无目录 EPUB 正文编号题头补目：首次打开目录时扫描 h1–h3 与独立段落中的“第＋数字＋章/节/卷/回/部/集”与“Chapter＋数字/罗马数字”，须其后出现普通正文才收录（连续编号目录页互不作证），前置件按文件名与标题跳过；只填补未命名章节，不改写正式标题，超过 256 章且已有部分正式标题的大书跳过。此前这类书目录全部显示“第 N 节”。
  Navigation-less EPUBs gain a body numbered-heading TOC: the first TOC open scans h1–h3 and standalone paragraphs for 第 + digits + 章/节/卷/回/部/集 and Chapter + digits/Roman numerals, applying a heading only when ordinary prose follows (consecutive numbered TOC labels cannot validate each other) with front matter skipped by filename and title; only untitled chapters fill in, authored titles never rewrite, and books beyond 256 chapters with some authored titles skip the scan. Such books previously listed every chapter as a numbered placeholder.
- TXT/无封面书确定性生成封面：按书路径种子在三种几何版式（深底藏书票、近白留白、上下分割）中选定一种绘制灰阶封面，与 EPUB 封面同槽同节奏上卡；不画文字（书名排版仍在卡侧），重绘成本低不做 TF 缓存，生成失败回退原文件名排版。
  TXT and coverless books get deterministic generated covers: a path seed picks one of three geometric layouts (dark bookplate, near-white whitespace, split panel) drawn in grayscale and delivered through the same card slots as EPUB covers; no text is drawn (the title still typesets beside the card), redrawing is cheap enough to skip an SD cache, and generation failure falls back to the typographic card.

## 0.5.37-20261009.7 · 2026-10-09

- 正文断行加入中文禁则：句末标点（。，、！？：；）』」》等右标点不再单独落到下一行行首，优先悬挂进右缘留白并压缩字隙（每隙≤3px），放不下时连同前一个正文字一起下移；左括号引号（（「『《等）不再停留在行尾，成串左标点整串下移；两端对齐/居中/命中共用同一套悬挂位置。窄到只剩标点的极端行允许小幅成组越界，保证永不空行、永不循环。全部断行用例见 book_layout host 测试。
  Body wrapping gains CJK kinsoku: closing punctuation (。，、！？：；』」》…) never starts a line alone — it hangs into the right margin with per-gap squeezing (≤3px) first, else moves down together with the preceding body glyph; opening brackets/quotes (（「『《…) never end a line, with whole runs moving down; justification, centering and glyph hits share the same hung positions. Pathologically narrow mark-only lines overhang as a group so wrapping never empties a line or loops. All break cases live in the book_layout host tests.
- 开书改为持久化事务，中断不再自动重提：开书解析前先把路径写入启动状态（NVS），成功才清除；断电或崩溃打断后，下次开机日志标明中断书目，续读询问自动跳过这本书一次，书架手动打开不受影响、打开成功即解除。开机半途中断同样留痕（首帧上屏才标记启动完成）。
  Book opening becomes a persisted transaction: the path lands in startup state (NVS) before parsing and clears only on success; after a power cut or crash the next boot logs the interrupted title, the resume prompt skips that book once, manual opening from the shelf stays allowed and a successful open lifts the block. Half-finished startups leave the same trace (startup counts as complete only after the first full frame reaches the panel).
- 新增错相揭页动画（实验，默认关）：翻页显示组新增“翻页动画”开关。开启后日间标准文字翻页改用错相波前——整页只算一次差分，16 条带一拍一带错开启动，每像素仍走完整厂家 GL16 相位序列，方向跟随翻页（前进自右向左）。直刷/灰缘/夜间与白 repaint 路径不受影响；扫描失败自动按未知基准整屏恢复重放。视觉节奏与残影表现请实机验收。
  Add a staggered page-turn animation (experimental, off by default) via a new switch in the page-display group. Enabled, day standard text turns take a staggered wavefront — one whole-page difference with 16 bands launched one per tick, every pixel keeping its full vendor GL16 phase sequence, heading following the turn (forward sweeps right-to-left). Direct, gray-edge, night and white-repaint paths are untouched; scan failures recover by replaying the turn whole-screen from an unknown baseline. Please validate pacing and ghosting on device.

## 0.5.36-20261009.6 · 2026-10-09

- 灰缘直刷消除翻页闪动：0.5.35同刻收尾后，厂家灰8"压黑再拉回"的过冲结构整页同步上演，读作明显闪动。灰行改按厂家DU自身中灰源行的差值剂量单向直驱——降到灰g需 DU(0,15)计数−DU(0,g)计数 个黑脉冲、升向同理，灰→灰取差值——从相0同起点单调逼近，无过冲无回拉；端点仍厂家DU，表长回到21相与黑白直刷同速。剂量来自厂家实测表而非线性假设，灰阶落点请实机再验。
  Gray-edge direct removes the turn flash: after 0.5.35's simultaneous finishing, the vendor gray8 overshoot-then-pull-back structure played synchronized across the page and read as a distinct flash. Gray rows now drive one way with marginal doses taken from the vendor DU's own mid-gray source rows — descending to gray g takes DU(0,15)−DU(0,g) black pulses, climbing symmetric, gray-to-gray marginal — approaching monotonically from a shared phase-0 start without overshoot or pull-back; endpoints stay vendor DU and the table returns to 21 phases, matching binary-direct speed. Doses come from vendor measurements rather than a linear assumption; gray landing points need device revalidation.

## 0.5.35-20261009.5 · 2026-10-09

- 夜间模式暂时下线：黑底残影多轮实机调优未果，阅读设置的夜间开关、“夜间方案”与“白预驱”入口移除，存量夜间设置不再生效恒为日间；翻页设置的清残影周期仍作用于通用页面。显示层夜间机制与存储保留，恢复只需还原入口与设置读取。
  Night mode is temporarily withdrawn: after repeated failed on-device ghost tuning, the night toggle, profile and pre-drive entries are removed and stored night settings no longer apply, always rendering day; the cleanup tier keeps serving generic pages. The display-layer machinery and storage stay, with restoration needing only the entries and the settings read.
- 灰缘直刷消除两段观感：厂家灰8灰边行全部在相27收尾，而厂家DU端点动作在相0-17先完成，实机呈现“先细锯齿后抗锯齿”。端点动作现整体右移至相10-27与灰行同刻收尾，终态逐字节不变。
  Gray-edge direct removes the two-stage look: vendor gray8 fringe rows all finish at phase 27 while vendor DU endpoint actions finished first at phases 0-17, showing thin-jagged then antialiased text. Endpoint actions now shift as a block into phases 10-27 to finish together, with byte-identical final states.
- 传书网页目录浏览新增“书库/字体”切换：`/fs` 支持 `root=fonts` 只读浏览字体目录（同一隐藏与截断规则、同样的越界拒绝），网页以切换芯片选择，无字体库时隐藏字体项。
  Transfer-page browsing gains Book/Font switches: `/fs` accepts `root=fonts` to browse the font directory read-only under the same hiding, truncation and traversal rules, with the page choosing via chips and hiding Font without a font library.

## 0.5.34-20261009.4 · 2026-10-09

- 在线传书网页全新设计并新增目录浏览与TXT预览：整页改为卡片布局（拖拽/点击上传、图书管理与目录浏览双标签、配网表单），新 `/fs` 接口只读浏览书库目录树（面包屑导航、目录优先排序、隐藏上传临时/备份与点文件、超400条截断提示），TXT 可弹层预览开头 4 KB（网页自动识别 UTF-8/GB18030 编码），EPUB 提示为打包格式不支持预览；浏览只读，上传删除仍在图书管理。
  Redesign the online transfer page with directory browsing and TXT preview: card layout (drag-or-click upload, Books/Browser tabs, provisioning form), a new read-only `/fs` endpoint walks the book-root tree (breadcrumbs, folders first, upload temps/backups and dot files hidden, 400-entry truncation note), TXT files preview their first 4 KB in a bottom sheet with page-side UTF-8/GB18030 detection while EPUB shows a packed-format hint; browsing stays read-only, with upload/delete still in Books.
- 修复夜间白预驱从未生效：选择性编码把一切未变像素发EE保持码，渲染器按码字节查14→14行，此前写在0→0行的背景微洗从未被驱动（开关两态无差的根因）。微洗现位于14→14行，并新增夜间保持掩码：目标非纯黑的未变像素走15→15保持行，黑底独享W2+B18重饱和、正文灰缘不受洗。请再开白预驱实机对比残影。
  Fix the night white pre-drive never firing: the selective encoder emits the EE hold code for every unchanged pixel and the renderer indexes the 14→14 row by that byte, so the wash previously written on 0→0 was never driven — the root cause of no switch difference. The wash now lives on 14→14 with a night hold mask: unchanged non-pure-black targets take the 15→15 hold row, so only the background gets the W2+B18 re-saturation while text fringes stay out. Please A/B ghost relief on device again.
- 灰缘直刷恢复厂家灰8标定迁移：0.5.33的线性脉冲公式在面板近黑压缩区欠驱，实机锯齿更重且笔画内发白（细字白芯）。灰行现整行回抄厂家压缩灰8表（白化前导+B10成形+W尾，30相），端点仍为厂家DU；边纹剂量回到厂家标定，观感请实机再验。
  Restore vendor-calibrated gray rows in gray-edge direct: 0.5.33's linear pulse formula under-dosed the panel's compressive near-black region into heavier jaggies and whitish stroke cores. Gray rows now copy the vendor compact gray8 table verbatim (whiten lead + B10 formation + W tail, 30 phases) with vendor DU endpoints, returning edge dosing to factory calibration; please revalidate on device.

## 0.5.33-20261009.3 · 2026-10-09

- 灰缘直刷根除两段观感：厂家灰序列"压过头再拉回"的过冲结构使扫描中边缘必然先变全黑再修回。灰边改按跟手DU剂量公式从背景色单调同向逼近（k=ceil(灰差·18/15)个同向脉冲），与字芯同向同动，无过冲无回修；表长28→21相，与黑白直刷同速。灰阶绝对精度换观感，边纹质量请实机再验。
  Gray-edge direct eliminates the two-stage look at its root: vendor gray sequences overshoot then trim, so edges must pass fully black mid-scan. Gray edges now approach monotonically per the follow-DU dose formula (k=ceil(delta*18/15) same-polarity pulses) from the background color, moving with the glyph cores without overshoot or trim; the table shrinks 28 to 21 phases, as fast as binary direct. Absolute gray accuracy trades for look; please revalidate fringe quality on device.
- 白预驱升级并新增背景微洗：15→0升级为厂家0→0对角线等剂量（直刷族同样W18先白后黑）；保持黑底0→0每翻一页走一次W2+B18微洗，模仿日间对角线重饱和。微洗应有暗灰一瞬，是否减轻残影与观感请开关对比后反馈；周期清理与日间路径不变。
  The white pre-drive upgrades and gains a background wash: 15→0 now takes the full vendor 0→0 diagonal dose (W18 then B18 on the direct family too), and held black 0→0 takes a W2+B18 mini-wash each turn, mimicking daytime diagonal re-saturation. Expect a brief dark-gray blink from the wash; please A/B ghost relief and visibility with the switch. Interval cleaning and day paths stay unchanged.
- 按用户要求移除Crossmux夜间对照：方案循环回到"当前/清白直绘"两态，已保存的Crossmux值1回当前，对照波形表、对照出口与裁剪器一并删除。
  Remove the Crossmux night comparison per user request: the profile cycle returns to Current/White-repaint, the saved Crossmux value 1 falls back to Current, and the comparison waveform tables, exit and trimmer are deleted together.
- 修复换网后WiFi"已保存，尚未连接"：0.5.31的"释放失败保留资源"会在断开失败后闩锁，后续连接在入口被静默拒绝，重新输密码只保存不连接。现在已保存网络的启动会先重试一次停止再连接；IDF对已删除对象返回的INVALID_STATE按已释放处理不再闩锁；锁屏维护遇到残留会自愈重试停止；全量会话缓冲不足时回退小缓冲保证配网能连；状态文案区分"尚未连接"与"连接失败"。
  Fix saved-but-never-connected WiFi after changing networks: 0.5.31's retain-on-failed-teardown latched after a failed disconnect so later connects were silently rejected at the gate and re-entering the password only saved. Starts on saved networks now retry the stop once before connecting; IDF INVALID_STATE for already-deleted objects counts as released instead of latching; lock maintenance self-heals residue by retrying the stop; full sessions fall back to smaller buffers when allocation fails so provisioning still connects; and the status line now distinguishes not-connected from connect-failed.

## 0.5.32-20261009.2 · 2026-10-09

- 灰缘直刷消除翻页两段观感：实测黑白端点动作在相0-17、灰边首动作迟至相16且全部拖到相27，先显二值细字再闪成灰缘。灰边序列现去前导空等左对齐相0与端点同步起扫，脉冲逐字保留，总相31→28稍快；边缘光学质量与是否仍局部闪动须同字体同字号实机确认。
  Gray-edge direct removes the two-stage page-turn look: measured endpoint actions in phases 0-17 while gray edges start as late as 16 and all end at 27, showing binary thin text first. Gray sequences now drop the leading idle, aligning to phase 0 beside the endpoints with byte-identical pulses, 31 to 28 phases; edge quality and residual flicker still need same-font same-size device checks.
- 新增可选实验“白预驱”（默认关，翻页设置内，方案行右侧）：普通夜间翻页旧白字回黑底（15→0）先用白脉冲再走厂家黑迁移。标准路径逐相拷贝厂家GL48的0→0对角线（先白18后黑18）；直刷族用W6前导加完整DU。白像素加白光学不可见，机制模仿日间对角线重饱和，是否减轻黑底残影未经同板验证；周期清理、入口和日间路径不变。
  Add the optional White Pre-drive experiment (off by default, right of the profile row in page-turn settings): ordinary night turns drive 15→0 white first before vendor black transitions. The standard path copies the vendor GL48 0→0 diagonal phase by phase (18 white then 18 black); the direct family uses a W6 lead plus full DU. White-on-white is optically invisible, mimicking daytime diagonal re-saturation; black-background ghost relief is not same-board verified, while interval cleaning, entries and day paths stay unchanged.

## 0.5.31-20261009.1 · 2026-10-09

- 针对锁屏后WiFi扫描ESP_ERR_NO_MEM，扫描和校时专用连接使用较小静态缓冲，完整传书配置保持；释放失败保留真实资源供重试，并记录内存及失败阶段。
  Address post-lock WiFi scan ESP_ERR_NO_MEM with smaller static buffers for scanning/clock-only sessions, retaining full transfer configuration; preserve resources after failed teardown for retry and log heap/failure stages.
- 新增独立可选“夜间方案：清白直绘”：实际物理清白后从白参考用厂家GL或DU绘制目标，省掉清白后的GC反转；保留成功翻页周期及原两方案，清理仍会闪，光学残影改善待真机。
  Add optional White-Clear Repaint: physically clear to white then repaint the real target with vendor GL/DU from that baseline, omitting a subsequent GC reversal. Retain successful-turn intervals and older profiles; cleaning still flashes and optical improvement awaits device tests.
- 直刷新增真实灰阶字缘，完整厂家灰迁移与原DU黑白端点共扫；原黑白无闪速度保留可选，灰缘较慢且可能局部闪动；旧失败的细边键和值2不复用，已保存走时模型保持。
  Add actual gray direct edges by scanning complete vendor gray transitions alongside original DU endpoints. Original binary no-flash speed remains selectable; gray edges take longer and may flicker locally. Retired fine-edge keys/profile 2 are not reused, and saved clock models remain.

## 0.5.30-20261009.1 · 2026-10-09

- 实机确认黑基准清理后第一张仍有残影并亮闪、细边没有可见抗锯齿改善，撤回两项入口与分支；旧黑基准选择回当前方案，细边键忽略不擦。恢复原直刷及当前方案物理清白加完整GC，保留WiFi扫描、时钟学习与已有设置；不宣称夜间残影或灰阶直刷已解决。
  Withdraw Black Baseline and Fine Edges after device feedback shows ghosts on the first cleaned page with flashing and no visible edge improvement. Legacy Black Baseline falls back to Current and fine-edge keys remain untouched. Restore original direct rendering and Current physical white/full-GC cleaning, retaining WiFi scans, clock learning and settings without claiming a night-ghost or gray-direct fix.

## 0.5.29-20261009.1 · 2026-10-09 · 显示试验已撤回 / Display experiments withdrawn

- 时间页区分首次联网样本、锁屏累计与自动校时失败；静态浅睡锁屏也按维护期限唤醒，不重画静态画面，静态深睡保持断电。未联网的准确起点不能替代可信样本，不按11小时快钟估算硬编码补偿。
  Distinguish the first trusted sync, accumulated lock sleep and maintenance failures. Static light-sleep locks service deadlines without repainting; static deep sleep still powers off. An apparently accurate offline starting time cannot replace a trusted sample or justify a fixed correction.
- WiFi扫描失败显示实际错误码，补完整主动驻留及成功空列表的一次被动兜底，保持国家策略。Windows热点的实际失败原因仍需设备返回码确认。
  Show actual WiFi scan errors, retain the full active dwell and retry successful empty lists once passively without changing country policy. The Windows hotspot failure still requires the device's returned error to identify its cause.
- 阅读新增默认关闭的“字缘：细边”，仅有效黑白直刷正文使用原始覆盖率取墨，字号、字重、分页、标准灰阶及DU刷新保持；不是灰阶抗锯齿。
  Add off-by-default Fine Edges for eligible binary direct body text using raw glyph coverage, retaining size, weight, pagination, standard gray and DU scans; this is not gray antialiasing.
- 新增可选“夜间方案：黑基准”：周期、手动及布局入口先真实整屏黑，再完整厂家GC画回保留目标，两段成功才重置周期。普通翻页及两旧方案保持，仍会亮闪，残影改善须真机比较。
  Add optional Black Baseline night cleanup: interval/manual/layout entry first physically refreshes to black, then uses full vendor GC to restore the retained target, resetting counts only after both succeed. Ordinary turns and older profiles remain; it flashes and ghost reduction requires device comparison.

## 0.5.28-20261008.1 · 2026-10-08

- TF卡新增跨开机封面缩略图及TXT目录缓存，完整源身份和缓存校验通过才复用；损坏、满卡或断卡回退原路径，不改变阅读进度。TXT命中仍顺序校验原始内容，省去目录解析。
  Add persistent SD cover thumbnails and TXT indexes, reused only after source identity and cache validation. Corruption, full cards or media loss fall back without changing progress. TXT hits still verify all source bytes while skipping index parsing.
- 字体缺少⋯时借用同字体…，原字优先，测量、预热和绘制一致；原文及摘录保持。
  When a font lacks ⋯, reuse its … glyph, retaining native glyph priority and consistent metrics, prewarm and painting without changing source text or excerpts.
- 新增扫描相位队列残留统计及限频异常日志，不改变正常波形与队列。用户确认Crossmux夜间对照仍随翻页积累黑底残影；本版不宣称修复光学残影。
  Add phase-queue residue counters and bounded anomaly logs without changing normal waveforms or queues. Device feedback confirms accumulated dark-background ghosts in the Crossmux night comparison; this version does not claim an optical fix.
- 睡眠补偿改为带时钟模型身份的原子记录；升级不再应用旧的无标识比例，需重新取得两次可信校时学习，同模型后续升级保留。修复未校准RTC零值锁住锚点、失败或短TIME_GET响应被旧快照误判成功的问题。
  Store sleep correction atomically with a clock-model identity. Upgrades stop applying untagged legacy rates and require two trusted samples to relearn; later upgrades retain matching models. Fix unset RTC values latching an anchor and stale snapshots falsely validating failed or short TIME_GET responses.

## 0.5.27-20261008.1 · 2026-10-08

- 新增“更多设置→翻页显示→夜间方案：Crossmux”可选Pico对照，默认仍为当前方案并保留已有周期/翻页选择。纯文字夜间固定黑白DU20，已知参考入口GL37、周期及手动单次GC36；未知参考保留原完整恢复，日间/插图/控件不参加。仅对照波形和策略，保留本地扫描时序；宿主UI验证通过，残影、白度与闪烁仍待真机比较。
  Add an optional Pico comparison under More Settings → Turns/Display → Night Profile: Crossmux, retaining Current by default and saved intervals/turn choices. Night text uses binary DU20, known-reference entry GL37 and single periodic/manual GC36; unknown references retain full recovery, with day/illustrations/controls excluded. Compare waveforms and policy while retaining local scan timing; host UI checks pass, with ghosts, whiteness and flicker awaiting device comparison.

## 0.5.26 · 2026-10-08

- 根据真机反馈撤回无效的定向补黑及后置局部擦写实验，移除入口与额外扫描，消除该实验引入的旧字闪动；升级不再读取已保存实验档位，保留夜间、直刷、周期及时钟修复。夜间历史残影仍未解决，手动及周期完整清理保持原方案，不承诺普通无闪翻页无残影。
  Withdraw ineffective targeted black reinforcement and post-DU local cleaning after device feedback, removing their controls and extra scans to eliminate experimental old-glyph flashes. Upgrades ignore saved experiment choices while retaining night, direct, interval and clock fixes. Historical night ghosts remain unresolved; manual/interval full cleaning retains its prior behavior without claiming ghost-free ordinary turns.

## 0.5.25 · 2026-10-08 · 已撤回 / Withdrawn

- 新增可切换的夜间残影实验：关闭、定向补黑、局部擦写，本测试版缺省定向补黑；仅夜间纯文字黑白直刷，在正常DU成功后处理旧亮字已变黑的位置，保护新白字和其余黑底。局部擦写会轻闪并增加翻页耗时；到期整屏清理仍沿用原设置，标准灰阶、日间及插图不参加实验。实际残影改善需真机对照。
  Add switchable night ghosting experiments: off, targeted black reinforcement and local erase/rewrite, defaulting to reinforcement in this test build. Only text-only binary night direct turns post-scan prior light glyphs now black after successful DU, protecting new white glyphs and the remaining black background. Local cleaning flashes briefly and slows turns; due full cleaning retains its saved interval, with standard gray, day and illustrations outside the experiment. Ghost improvement requires device comparison.

## 0.5.24 · 2026-10-08

- 修复异常复位日志在非零启动时刻把时间锚定到零、导致后续显示额外加上启动耗时的问题；日志使用实际单调时间，写盘重试保留已有可信锚点与走时学习窗口。离线长期快慢仍取决于睡眠时钟、已保存补偿和重启时的PMU起点，需要真机对照。
  Fix abnormal-reset logging anchoring time at zero after boot has progressed, which added boot elapsed time to later displays. Logs use the actual monotonic time and preserve trusted anchors and learning windows through write retries. Long-term offline drift still depends on the sleep clock, stored compensation and the PMU restart baseline, requiring device comparison.

## 0.5.23 · 2026-10-08

- 夜间手动清残影、实际返回正文布局及周期到期时，先物理清白再用厂家GC16呈现保留的正文，针对设置轮廓残留；一次操作可能多次闪动且更慢，实际残影仍需真机复测。普通夜间翻页、日间、字体网格和触摸队列保持。
  Night manual cleaning, actual layout returns and due intervals physically clear white before vendor GC16 presents the retained body, addressing setting outlines. One operation can flash several times and take longer; actual ghosts still need device comparison. Ordinary night turns, day behavior, font grids and the touch queue remain.
- 书架搜索与键盘取消按下刷屏，完整轻点仅直刷输入栏；连续输入不穿插定稿，呈现结束后停输两秒局部恢复灰阶，失败两秒后重试。取消、应用、64字符限制及匹配算法保持。
  Library Search and its keyboard omit pressed scans; complete taps directly update only the input. Continuous typing avoids settling scans, with local gray restoration after two idle seconds following presentation and a two-second retry on failure. Cancel, Apply, the 64-character limit and matching remain.

## 0.5.22 · 2026-10-08

- 夜间阅读统一暗色正文、边距、页脚、工具条与阅读覆盖层/图片预览；目录、阅读设置等其它页面保留原配色。分页和辅助线网格不变，标准抗锯齿与直刷实际黑白目标保留。
  Night reading darkens the body, margins, footer, tools and reading overlays/image previews together; TOC, reader settings and other pages retain their palette. Pagination/guide grids, standard antialiasing and actual binary direct targets remain.
- 按用户选择，夜间两模式共用清残影周期：0关闭，第N次成功翻页用一次厂家GC16，失败不计数，成功布局/手动/故障GC重置；日间不周期。普通夜间GL49/DU21不改波形、不加推动，累积残影仍可手动清理，效果待真机对照。
  Per user preference, both night profiles share the cleanup interval: 0 disables it, the Nth successful turn uses one vendor GC16, failures do not count, and successful layout/manual/recovery GC resets it. Day omits this schedule. Ordinary night GL49/DU21 keep their waveforms without added drive; accumulated ghosts remain manually cleanable and require device comparison.

## 0.5.21 · 2026-10-07

- 标准日间恢复厂家完整GL49黑/灰定稿及真实整页差分，未变字芯不再直接跳过；白底保持。夜间仍选择性保持，避免黑底整屏亮闪。已获真机认可的黑白DU21与辅助线网格保持，标准字芯光学效果待复测。
  Restore complete vendor GL49 black/gray settling with actual full-page differences for standard day turns instead of skipping unchanged stroke cores; white holds. Night retains selective holds to avoid flashing the black background light. Keep device-accepted binary DU21 and guide grids; standard stroke results await device comparison.
- 按用户授权，仅实际应用/菜单/阅读视图布局切换及已显示覆盖层退出正文时清理一次厂家GC16，成功消费、失败重试；普通翻页、按钮和加载tick不重复。旧入口白推动退出产品路径，不承诺消除物理残影或提高白度。
  Per user authorization, apply one vendor GC16 only at actual app/menu/reader layout changes and return from displayed overlays to body, consuming success and retrying failure. Ordinary turns, controls and loading ticks do not repeat it. Retire the old white-tick entry from product use without claiming eliminated physical ghosts or increased whiteness.

## 0.5.20 · 2026-10-07

- 撤回0.5.19估算灰阶、统一擦白及加强入口推白，恢复厂家灰阶路径和原入口补偿；该实验被实机反馈确认加重灰底与残影。
  Withdraw 0.5.19 estimated grays, uniform whitening and increased entry drive after device feedback confirmed worse gray backgrounds and ghosts; restore vendor gray paths and prior entry compensation.
- 按用户明确取舍，无闪直刷使用厂家完整黑白DU，保留实际旧灰参考，标准及插图章节仍有灰阶。直刷从首帧使用真实黑白目标，辅助线保持可见。
  Per the user's explicit preference, direct uses complete vendor black/white DU with actual prior-gray references; standard and illustrated chapters retain gray. Direct uses real binary targets from entry and keeps guides visible.
- 纯文字开启辅助线时统一标题与正文网格，旧线与新页文字保持错开；开关网格重排并保留文字锚点，段距改用整行空槽。关闭辅助线及图文布局保持。
  Text-only guides align headings and body to one grid so old rules stay clear of new text; grid toggles repaginate at the text anchor and paragraph gaps use whole slots. Unguided and illustrated layout remains unchanged.

## 0.5.19 · 2026-10-07 · 已撤回 / Withdrawn

- 无闪直刷改为21相单向实验灰阶，八档文字目标同步起步，去掉中间灰先擦后写；保留抗锯齿、真实参考帧和插图章节标准灰阶。实际灰度与闪烁观感待真机验证。
  Direct turns use an experimental 21-phase monotonic gray model, starting eight text targets together without reverse erase/write paths; retain antialiasing, real baselines and standard illustrated chapters. Physical grays and perceived flicker require device testing.
- 两模式及导航加强真实变白的单次擦除，布局入口白底补偿由一相增为三相，仍合并既有扫描；连续正文白白保持，不恢复历史字形补擦或周期黑闪。辅助线排版检查通过，转场旧线与新字重叠、空白灰底和残影仍需同板复测。
  Strengthen one-time erasure for real to-white changes in both modes and navigation; entry compensation grows from one to three white actions within existing scans. Consecutive turns hold white without historical glyph cleanup or scheduled flashes. Guide geometry passes; transient old-line overlap, gray paper and ghosts need same-board testing.

## 0.5.18 · 2026-10-07

- Tab/菜单布局切换及正文入口使用单次均匀白底补偿，合并现有GL16扫描，不整屏压黑；普通重绘/按钮/连续翻页不重复。真实残影与纸底亮度待实测。
  Tab/menu layout changes and body entries merge one uniform white compensation into existing GL16 scans without whole-screen black clearing; ordinary redraws/controls/turns never repeat it. Physical ghosts and paper brightness await device comparison.
- 正在读按钮扩大并改名“上传进度”；阅读同步隐藏服务器与用户名，传书账号配置保留。EPUB同步按真实文本节点和Unicode偏移还原，修复长段落退回段首；复杂结构保留明确近似回退及下载确认。
  Enlarge and rename Home Upload Progress; hide server/user in reader sync while retaining transfer account settings. EPUB sync restores real text-node and Unicode offsets rather than returning long paragraphs to their start; complex structures retain explicit approximate fallback and pull confirmation.

## 0.5.17 · 2026-10-07

- 用户拒绝黑闪体验，撤销0.5.16的GC16进页/正文边界清理。Tab、菜单及正文入口用完整厂家GL16保留黑/灰定稿与白白保持，不使用正文选择码跳过定稿，也不累计触发周期GC16。正常翻页/局推保持，未增加擦白相或扫描；物理残影待实测。
  Withdraw 0.5.16 flashing entry/body-boundary GC16 per user feedback. Tabs, menus and body entries use full vendor GL16 with black/gray settling and held white, without body selectors skipping settling or scheduled GC16. Normal turns/local updates remain; no extra white phases or scans. Physical ghost reduction awaits measurement.

## 0.5.16 · 2026-10-07

- 针对主Tab残影延续到正文，四根页进页、书架开书、目录/阅读设置返回正文及正文回书架用一次完整GC16清理，布局切换会短暂黑闪；成功后才记录正文视图基准，失败继续重试。普通控件及连续正文翻页保持原更新与无周期黑闪策略。实际残影效果待真机复测。
  Address root-Tab ghosts carrying into reading with one complete GC16 on root entry, shelf opening, TOC/settings-to-body and body-to-shelf transitions, with a brief flash. Record the displayed body view only on success so failed entries retry. Ordinary controls and consecutive body turns retain their profiles without scheduled flashes. Physical ghost reduction awaits device tests.

## 0.5.15 · 2026-10-07

- 修复长时间锁屏仅一次校时无法完成走时学习：默认低频连接已保存WiFi，约一小时补齐学习，完成后每六小时维护；时间页可关闭并显示补偿ppm/保存状态。短样本保留起点，零补偿保存，PMU暂时失败可重试；每会话最多30秒，失败一小时后重试，解锁取消。长期精度与功耗待真机测试。
  Complete sleep-rate learning after a single initial sync through enabled-by-default saved-WiFi lock maintenance, after about an hour then every six hours. Time provides a switch and measured ppm/save status. Preserve short-sample anchors, persist zero rates and retry transient PMU failures; bound sessions to 30 seconds, failures to hourly retries and cancel on unlock. Long-run accuracy and power await hardware tests.
- EPUB进度同步优先和KOReader交换实际章节/段落路径，段落级定位并明确提示；TXT及不支持结构保留rp1，解析失败保留百分比回退，下载仍须确认。路径按需独立读取，不干扰当前书源。跨客户端实机同步待验证。
  Prefer actual EPUB chapter/paragraph paths for KOReader sync with an explicit paragraph-resolution notice, retaining rp1 for TXT/unsupported structures and percentage fallback for unresolved pulls. Confirmation remains required; independent on-demand mapping preserves the active reader. Cross-client device sync awaits verification.

## 0.5.14 · 2026-10-05

- 根据连续翻页出现白色旧字叠加的反馈，撤销历史白白补擦及真实变白后的额外三相推动；两模式与Tab/菜单保持未变像素，变化仍走原有厂家灰阶/完整DU路径，保留抗锯齿、真实参考和正常正文无黑闪。物理效果待真机对照。
  Responds to accumulating white old glyphs by removing historical held-white erasure and the three extra to-white drives. Both turn modes and tabs/menus hold unchanged pixels, retaining vendor gray/complete DU transitions, antialiasing, real baselines and flash-free normal body turns. Physical results require device comparison.
- 标准49相、直刷31相，每页比上一版少三相扫描，并移除311,904字节历史存储和每页历史遍历；不删厂家有效迁移相。扫描预算减少约34ms，实际整次翻页耗时须真机测量。
  Standard 49/direct 31 save three scan phases per page and remove 311,904 bytes of history storage plus per-page history traversal, without removing effective vendor transition phases. Scan budget falls by about 34ms; total turn latency requires device measurement.

## 0.5.13 · 2026-10-05

- 翻页保持未变化的章节名、状态栏和正文像素，消除它们每页被完整波形重新驱动的现象；当前变化仍保留灰阶迁移和抗锯齿。
  Hold unchanged chapter/footer/body pixels through turns instead of driving them with a full waveform on every page; changes retain grayscale paths and antialiasing.
- 空白旧墨迹定向补擦合并进一次主扫描：标准52相、直刷34相，替代49+6/31+6；白白旧痕迹擦白增强，保留中性收尾、真实参考和正常正文无黑闪。实际残影与总耗时仍需真机对比。
  Merge selected blank-history cleanup into one scan: standard 52/direct 34 replace 49+6/31+6, strengthen held-white cleanup and retain neutral settling, real baselines and flash-free body turns. Physical ghosts and total latency need device comparison.
- 手势页增加独立触摸队列，刷新时保留完整轻点的按下和抬起；按采样时刻识别，切页/睡眠/读错/溢出安全取消旧输入，减少需要再点一次才能翻页的问题。
  Add independent queued touch sampling on gesture pages to retain complete taps during scans, recognize capture times and safely cancel old input at transitions/sleep/errors/overflow, addressing missed turns.

## 0.5.12 · 2026-10-05

- 两种翻页压缩墨迹历史遍历，去掉重复目标帧观察及直刷的大范围差分检查；白目标选择图在上电前计算，预算下电后维护，减少带电等待CPU的时间。保留抗锯齿、原厂驱动相数与无黑闪正文；实际速度及淡残影仍需真机对照。
  Both turn profiles pack ink-history traversal and omit duplicate target observation and direct-page bounds checks. White selectors are prepared before HV and budgets committed after power-off, reducing powered CPU waits while preserving antialiasing, vendor drive lengths and no-flash body turns. Physical speed and faint ghosts still require device comparison.
- 阅读底部标题仅显示章节名，保留页码、进度、时间、电量和保存失败提示。
  Reader footer titles show only the chapter, retaining pages, progress, clock, battery and save-failure notices.
- 针对锁屏走慢增加网络实测睡眠比例：同次开机两次联网校时相隔至少一小时、主要处于浅睡时学习，持久保存并仅补偿睡眠累计；短样本、过大偏差和失败校时不学习，醒着的计时保持原速。升级后先校时，锁屏数小时后再校时完成首次学习；温度变化下长期误差仍待真机验收。
  Sleep-rate learning addresses slow lock clocks using two network syncs within one boot, at least an hour apart and dominated by light sleep. Persisted rates compensate sleep only; short samples, excessive offsets and failed syncs do not train, while awake time retains its rate. Sync after upgrading and again following hours of lock sleep for first learning; temperature-dependent long-run accuracy remains unverified.

## 0.5.11 · 2026-10-05

- 根据直刷锯齿反馈，正文直刷从21相硬黑白改为31相八档灰阶，原厂短灰阶保留字形边缘、完整DU保持黑白端点驱动，插图章节仍用标准灰阶；正常正文无周期GC16。
  Responds to jagged direct text by replacing 21-phase binary turns with 31-phase eight-gray turns, using vendor short-gray edges and complete DU endpoints, with illustrated chapters still standard gray and no scheduled body GC16.
- 两种正文及Tab/菜单改为最近墨迹定向补擦：仅当前白目标及近期文字邻域选择6相擦白/中性收尾，保留黑字/灰边，复用供电及真实参考帧；撤销全白底固定轻擦，局部按钮/分钟不附加，历史有界且失败保留。额外扫描时间与物理残影待真机对照，不以每按钮全刷解决。
  Both body profiles and tabs/menus use recent-ink selected-white cleanup: six erase/neutral phases protect current black/gray edges, reuse rails and actual baselines, replace fixed whole-white pulses, omit local/minute updates and retain bounded history on failures. Extra time and physical ghosts need device comparison without per-button full cleanup.

## 0.5.10 · 2026-10-05

- 修正IDF6.1睡前10周期慢钟短采样覆盖开机校准精度的问题：当前慢钟的这类调用使用本板3000周期配置，其余时钟源/采样保持原样；不按单次反馈硬编码减时。升级后重新联网校时，长期睡眠漂移仍需实机对照。
  Prevents IDF6.1's ten-cycle pre-sleep slow-clock sample from replacing boot precision: those current-slow-clock calls use the board's 3000-cycle configuration, retaining other sources/sample sizes without a guessed time offset. Resync after upgrading; long sleep drift needs device comparison.
- 阅读“更多设置→翻页显示”新增可选无闪直刷，保留默认标准灰阶。纯文字章节使用21相原厂DU与真实黑白参考帧，白天/夜间细灰线转前景，含图章节保持GL16；两种正文效果都不触发周期GC16，实际速度与残影待实机验证。
  Adds optional direct turns under Reader More Settings → Turns/Display, retaining standard gray by default. Text-only chapters use 21-phase vendor DU and real binary reference frames; day/night light guides become foreground, while image chapters retain GL16. Neither body profile triggers scheduled GC16; actual speed/ghosting require device tests.
- 针对文字与页脚之间及Tab空白处淡残影，整页GL16/直刷在末次擦白相给未变化白底一次轻擦，非白灰阶迁移与三相中性收尾不变；局部控件/分钟刷新不加入该推动，不增加整屏压黑。
  For faint ghosts in body/footer gaps and tab whitespace, page GL16/direct adds one held-white erase pulse at the last vendor erase phase, preserving nonwhite transitions and three neutral tail phases. Local controls/minute updates omit it without added whole-screen black drive.

## 0.5.9 · 2026-10-05

- 阅读工具条/设置里的清残影先退出覆盖层，再对正文执行一次完整清理，避免清理菜单后返回正文又留下菜单轮廓；普通调整仍保留设置页。
  Reader toolbar/settings cleanup closes overlays and cleans the body once, preventing cleanup of the menu followed by a ghosted return; ordinary adjustments still keep settings open.
- 正文、菜单、Tab及局推共用保留原厂48相的GL16，尾部补1相中性保持，总49相，不增加压黑或擦白驱动；正常正文翻页仍不触发周期GC16。中性扫描多约11ms，物理残影改善待真机验证。
  Body/menu/tabs/local GL16 retain all 48 vendor phases plus one neutral tail, 49 total, without extra black/white drive; normal body turns still never trigger scheduled GC16. The neutral scan adds about 11ms; physical ghost reduction needs device verification.

## 0.5.8 · 2026-10-05

- 用户明确要求正常阅读翻页不黑闪：正文始终单遍 GL16，不消耗或触发周期 GC16，即使已有清残影周期设为1也不会在翻页中周期清屏；手动清残影保留，通用页面周期不变。保留0.5.7完整厂家波形和上下电修正。
  User requires no black flashes during normal reading turns: body turns always use one GL16 pass, neither consuming nor triggering scheduled GC16 even with a saved interval of one. Manual cleanup remains, with generic-page intervals unchanged. Retains 0.5.7 complete vendor waveforms and power-sequence corrections.

## 0.5.7 · 2026-10-05

- 针对阅读与首页持续灰底/残影，灰阶恢复完整厂家48相，撤销擦除/饱和裁剪、相位重排、额外白底推动和稳定黑字跳过；正文、Tab、菜单、局部控件和分钟字带使用一致厂家迁移。普通按钮仍局推，不增加每次整屏黑闪；已有周期和手动 GC16 保留。相较37相增加约0.12秒扫描，物理效果待真机验证。
  Persistent reader/Home gray backgrounds and ghosts prompted complete vendor 48-phase gray tables, removing erase/saturation truncation, reordering, extra white drive and held black. Body/tabs/menus/local controls/minute bands share original transitions. Controls remain local without per-action full black flashes; saved/manual GC16 remain. Scanning adds about 0.12 seconds over 37 phases; physical results require board verification.
- 修正扫描结束 MODE/XOE 与板级下电顺序互相干扰：上下电独占控制线，取消高压开启期间多余MODE低脉冲，保留原厂电源延时和厂家VCOM。上电或输出使能失败不扫描、不提交虚假参考帧，成功重试恢复未知基准。保留已修复的重启位图旋转。
  Fixes interference between scan-end MODE/XOE and board shutdown: power sequencing owns control lines, removing an extra MODE low pulse while high voltage is on, with original power delays/factory VCOM retained. Failed power/output enable never scans or commits a false baseline; successful retry recovers unknown state. Retains the fixed boot bitmap rotation.

## 0.5.6 · 2026-10-04

- 修复重启时短暂竖条/散点花屏：启动与静态锁屏位图先有界解包到临时 PSRAM，再按屏幕方向绘制，避免把竖屏资源直接写入横屏扫描缓冲；失败安全回退白页，绘制后释放临时内存。
  Fixes transient boot stripes/dots: inflate boot/static-lock bitmaps into temporary PSRAM then draw with display rotation, rather than putting portrait assets into landscape scan memory. Fail safely to white and release temporary storage after painting.
- 审查 CrossMux main `ffd62f3d` / Pico SDK `96de1be6` 后修正文字保持策略：37相单遍 GL16 驱动白底18相擦白并重新定稿灰字，稳定黑字保持；正文、书架、Tab 与菜单使用一致迁移，保留真实旧帧。普通按钮仍局推，GC16 仍按已有周期、手动或故障恢复，普通更新后立即下电。
  Reviewing CrossMux main `ffd62f3d` / Pico SDK `96de1be6` led to corrected text holding: one 37-phase GL16 pass erases white for 18 phases and resettles grays while holding stable black. Body/shelf/tabs/menus share transitions and retain real old frames. Controls stay local; GC16 retains saved intervals, manual/fault recovery and immediate power-off after ordinary updates.

## 0.5.5 · 2026-10-04

- 用户修订刷新原则：普通按钮优先局推，不消耗整屏清理周期；正文和根 Tab 恢复单遍 GL16，GC16 仅按周期、手动清理或故障恢复。保留普通刷新后立即下电。
  User-revised refresh policy: prefer local controls outside whole-screen cleanup counting; body/root pages use one GL16 pass, reserving GC16 for scheduled/manual cleaning or fault recovery. Ordinary updates still power off immediately.
- 修复正文三横杠导航选书架误回正文：菜单为明确书架入口，同一个页面也先保存离开再进入书架，不弹续读。
  Fixes reader navigation Library returning to the body: explicit menu entry saves/exits and reenters the shelf even with a shared page descriptor, without a resume prompt.
- 核对 CrossMux main `eaa49ff8` 与其 Pico SDK `96de1be6`：采用真实旧帧差分与文字保持策略，纯黑跳过前导擦白，16阶抗锯齿目标仍保留完整灰阶迁移；移植行队列指针表修复（S3 两表少申请38,400字节）、失败帧不回写参考帧和未知基准清白重试。保留本机波形源表、厂家VCOM和120MHz产品时序。
  Reviewed CrossMux main `eaa49ff8` and its Pico SDK `96de1be6`: actual-frame differences/held text, no leading erase for pure black, complete transitions for 16-level antialiased grays; port pointer-sized queue tables (38,400 fewer requested bytes on S3), success-only baseline commits and clean retry after unknown baselines. Retain vendor source tables, factory VCOM and 120MHz product timing.

## 0.5.4 · 2026-10-04

- 锁屏浅睡采用分频快 RC 并保留时间锚点，唤醒不再引入 PMU 偏差；更新后应联网校时，长期走时需实机对照。
  Light sleep uses divided fast RC and retains its time anchor without importing PMU drift on wake; sync once after upgrading and compare long-term device time.
- 普通刷新立即下电；正文翻页、根 Tab 与手帐子页每次 GC16 清理，接受黑闪以处理实机持续残影。封面缓存保护当前页，包括无封面失败结果，避免重复加载闪烁。
  Ordinary refreshes power down immediately; reader turns, root tabs and Journal tabs use GC16 cleanup, accepting its flash for persistent hardware ghosting. Visible covers and failed cover results stay pinned against reload flicker.
- 最近阅读增加分页与总数，可访问所有有效阅读记录；首页按钮明确标为“上传”，下载仍在进度同步设置中确认。
  Recent reading gains pagination and totals for all valid records. Home labels its action Upload; confirmed pulls remain in sync settings.
- 正文长按选中所在句子并确认保存摘录；金句便签显示真实摘录、支持分页与原文位置跳转，旧书签保留在目录。最多保留 16 条最新摘录。
  Hold a body sentence and confirm its excerpt. Journal Notes show real excerpts with paging and source-position jumps; existing bookmarks stay in TOC. Keep the latest 16 excerpts.
- 修复阅读统计 NVS 分区/命名空间查询与重复迭代，近七天累计包含已保存的今日分钟，无需清除数据。
  Corrects the NVS partition/namespace lookup and duplicate stats iteration so seven-day totals include persisted daily minutes without clearing data.

## 0.5.3 · 2026-10-04

- 首页同步按钮独立命中，上传已保存进度并显示结果或配置/网络提示；继续阅读不受影响，停止、离页与锁屏会取消同步。
  Home Sync has its own hit target, uploads saved progress and shows results/configuration/network hints; Continue remains separate, and Stop/exit/lock cancel sync.
- 四个根页使用37相导航 GL16：保留上一帧及灰阶迁移，把白底擦白补足到18相，单次推屏不增加相数；周期 GC16 与手动清残影保持。
  Four roots use a 37-phase navigation GL16: retain the prior frame and gray transitions, with 18 white-background erase phases in one presentation; periodic GC16/manual cleaning remain.
- 手帐保留卡片布局，增大统计、子标签、日期与便签字号，长文本有界换行。阅读底栏首行书名/章节，次行时间电量与页码百分比；分钟变化仅局推底栏，翻页校正预渲染中的旧时间。
  Journal retains its card layout with larger stats/tabs/dates/notes and bounded wrapping. Reader footer puts book/chapter above clock/battery and page/percent; minute changes push only the footer and page turns correct stale prepared clocks.

## 0.5.2 · 2026-10-04

- 根导航在触摸首帧派发按下事件，第二帧只稳定轻点锚点；共享导航切页后重新判触摸沿，避免刷屏期间漏掉抬起造成下一次点击失效。底栏图标缓存覆盖率，减少重复绘制开销。
  Root tabs receive presses on the first touch sample, with the second only stabilizing tap anchors; shared navigation rearms edges after switches so releases missed during refresh cannot swallow the next tap. Cached icon coverage reduces repeated drawing work.
- 修正锁屏时钟刷新区域截断数字底部及字体 120px 上限。动态锁屏用 ESP 分钟定时浅睡（深睡设置下也采用浅睡），关机保持断电；清除旧 PMU 循环闹钟。中断线常低、浅睡被拒绝时带延时轮询并检查分钟期限；PIN 读错/多点取消输入。
  Fixes the clipped lock-clock digit bottoms and the 120px font cap. Dynamic faces use ESP minute-timed light sleep even under the deep-sleep setting; power-off still powers down and clears legacy PMU repeat alarms. Stuck interrupts and rejected sleep use delayed polling with a minute deadline; PIN read errors/multitouch cancel input.

## 0.5.1 · 2026-10-03

- 修复 tab“点两下才切换”：触摸首帧坐标常带落笔抖动，识别器把整次轻点误判为拖动并在抬起时静默取消；PRESS 现在延迟一帧、以稳定坐标重锚（参考固件高频采样后才动作的做法），抖动不再吞掉首次点按。书架 tab 在上一本书加载中也不再静默拒绝切页。
  Fixes tabs needing two taps: the first touch frame often carries pen-down jitter, which the recognizer misread as a drag and silently cancelled the whole tap on release; PRESS now waits one frame and re-anchors on the settled coordinates (the reference firmware acts only after many samples), so jitter stops eating the first tap. The shelf tab also stops silently refusing while a previous book is loading.
- 修复锁屏时钟不走时：浅睡分钟唤醒从 ESP 定时器改为 PMU 循环闹钟，经 CW_INT→IOE 中断→GPIO 唤醒——与按键唤醒同一条已验证路径；每分钟到点必醒、重锚 RTC 后重画。
  Fixes the frozen lock clock: light-sleep minute wakes now ride the PMU repeat alarm through CW_INT→IOE interrupt→GPIO — the same proven path as key wake; every minute reliably wakes, re-anchors to the RTC and repaints.
- 修复锁屏时钟字残缺（照片里笔画缺损的"3"）：分钟重画从 8 帧跟手 DU 换成修剪 GL16 表的静默局部全像素推送——大号细笔画完整上墨，仍无黑闪，且不计入清残影档位（锁屏永不周期性黑闪）；首页状态栏分钟走时同步换用。
  Fixes eroded clock digits (the stroke-chipped "3" in the photo): minute repaints switch from the 8-frame follow DU to a quiet full-pixel area push on the trimmed GL16 table — large thin glyphs get full ink, still no black flash, and it stays outside the ghost-cleanup tier (the lock face never flashes periodically); the home status-row clock follows suit.
- 底栏 tab 改为按下即切换（跨面板参考固件的列表按压行为）：手指按下的瞬间完成切页，不再依赖抬起时机，推屏期间的点按不再丢失后续操作。
  Bottom-bar tabs switch the instant they are pressed (the cross-panel reference list-press behavior): the page flips on touch-down instead of depending on the release, so taps spanning refreshes stop being lost.
- 锁屏时钟分钟刷新改为局部推送：每分钟只推时间字带（无黑闪、不闪整屏），跨天或累计 15 分钟才整页 GL16 定稿；闹钟开机重画同样改无闪 GL16。
  Lock-clock minute refreshes go partial: each minute pushes only the time band (no flash, no whole-screen blink); a date change or 15 accumulated minutes takes one whole-page GL16 settle, and the alarm-boot repaint uses the flash-free GL16 too.
- 正在读首页右上角时间开始分钟级走时：跳分时整页重画进缓冲、只把顶部状态条推上屏，无闪烁也不计入清残影档位。
  The Now-Reading home clock now ticks by the minute: the page repaints into the buffer but only the top status row goes to the panel — no flicker and no ghost-cleanup tier impact.
- 修复“立即对时”永远提示先配置 WiFi：判定调用把 NULL 当 SSID 缓冲传给已保存凭据接口，永远返回参数错误；现已用真实缓冲，已保存的 WiFi 直接连接对时。
  Fixes Sync Now always claiming WiFi was unset: the check passed NULL as the SSID buffer to the saved-credentials API, which fails with an invalid-argument error every time; a real buffer now connects and syncs over the saved network.

- 锁屏时钟开始走时：浅睡模式下每分钟定时唤醒重画锁屏；深睡（断电）模式下 PMU 循环闹钟到点自动开机、只重画锁屏后继续断电。静态图锁屏样式不启用；正常开机会自动清掉残留闹钟。
  The lock-screen clock now ticks: light sleep wakes on the minute boundary to repaint the face, and deep sleep (powered off) uses a PMU repeat alarm that auto-powers the host on just to repaint, then powers off again. The static lock face opts out; a normal boot clears any leftover alarm.
- 时间走时改为参考固件架构：醒着的时段信 ESP 晶体钟（单调插值），只在开机、唤醒和 SNTP 校准时从 PMU RTC 重锚，15 秒轮询不再追针——PMU RTC 自身变快不会再被放大到显示，偏差只记日志。
  Timekeeping moves to the reference architecture: the ESP crystal clock owns awake time (monotonic interpolation), re-anchoring from the PMU RTC only at boot, wake and SNTP calibration; the 15-second polls no longer step the display, so a fast-drifting RTC stops amplifying into the clock, with deltas logged only.
- 页面刷新后电源轨 3 秒短窗保活：连续 tab 切换与翻页不再每次冷启动高压轨（每次省约 200ms），空闲到点仍自动断电；进睡前强制放轨。
  Page refreshes hold the HV rails for a 3-second window: consecutive tab switches and page turns skip the rail ramp (~200 ms each) while idle expiry still powers down; sleep paths release the rails explicitly.
- 设置页版本行去掉误导性的“UTC 编译时间”标注，只保留版本号。
  The Settings version line drops the misleading UTC build-time label and keeps the version only.

- tab 导航永不阻塞：书架进入请求在进页时即时完成（不再等待 SD 探测与目录扫描），排队中的请求可被新请求覆盖——连点 tab 不再出现"图标闪一下但切不过去"。
  Tab navigation never blocks: a shelf entry completes the moment the page is entered (no more waiting on the SD probe or directory scan) and a queued request is overwritten by a newer one — rapid tab taps no longer bounce with a bar flash and no switch.
- 深睡（断电）后冷启动加做一轮物理清屏：针对深睡冷启动花屏的保险（浅睡实测不花屏，已撤销唤醒时的额外清屏，唤醒恢复原速）。
  Cold boots after deep sleep (power loss) run one extra physical panel clear, targeting the deep-sleep boot garble (light sleep is clean in testing; the extra wake-time clear is withdrawn, restoring wake speed).
- 密码管理页选项排版修正：设置新密码/清除密码保持两大按钮，"解锁验证"改为整行按钮，长文案不再溢出按钮框。
  PIN management layout fixed: Set/Clear keep the two large buttons and unlock-verification becomes a full-width row, so long labels no longer overflow their frames.

- 锁屏唤醒花屏根治性处理：浅睡唤醒后、首刷之前先物理清屏并把软件双缓冲铺白（参考固件唤醒重建基线的同型做法），面板与差分基准强制一致，不再依赖任何推断；唤醒会多一次约一秒的清屏。
  Wake-garble addressed decisively: after light-sleep wake and before the first push, the panel is physically cleared and both software buffers white-seeded (the reference firmware's wake-baseline-rebuild recipe), forcing panel/baseline consistency instead of inference; wakes gain roughly one second of clearing.
- 唤醒/退出阅读后首页与手帐不再全量重扫：进度保存只原地刷新百分比与顺序，仅换书（最近阅读路径变化）才重扫目录；tab 二次进入零加载。
  Home and Journal no longer rescan after wake or leaving the reader: progress saves refresh percentages and order in place, and only a book switch (last-read path change) rescans; second tab entries load nothing.
- 设置里的锁屏密码页重做：与锁屏挑战同款的按压即时高亮 + 跟手 DU 反馈（不再整页闪烁），头部改标准页头消除标题与返回键遮挡；解锁验证频率（每次/仅开机）也在该页切换。
  The Settings lock-PIN page is rebuilt with the challenge's instant press highlight and follow-DU feedback (no more full-page flashing), a standard header removing the title/back overlap; the unlock-verification cadence (every unlock / boot-only) also switches there.
- 正文字号上限放宽到 80px（原 72px）。
  The body font ceiling rises to 80px (from 72px).

- 正文翻页移植同面板参考固件（crossmux Read Pico）的文字转页表：裁剪 GL16 左对齐（各方向同起拍、擦白与推黑重叠），无白场交叉淡化、未变化像素不驱动，且黑字落墨不再先闪白；清残影档位到期时仍自动升 GC16 深清。
  Reading turns port the text-turn table from the same-panel reference firmware (crossmux Read Pico): the trimmed GL16 left-aligned so every direction starts on one beat and the white erase overlaps the black write — a cross-fade with no white field, unchanged pixels undriven, and black glyphs no longer flash white on arrival; the ghost-cleanup tier still promotes to GC16.
- 普通页面 GL16 改走 37 相修剪表（参考固件的 Half 档），比 48 相完整表快约四分之一；tab 切换与菜单更跟手，推屏窗口变短也减少了窗口内快速点按的丢失。
  Ordinary GL16 pages move to the 37-phase trimmed table (the reference Half tier), about a quarter faster than the 48-phase full table; tab switches and menus feel snappier and the shorter push window drops fewer quick taps.
- 退出阅读回书架不再全量重扫：原地刷新进度与排序，目录和封面缓存保持有效，秒回书架。
  Leaving the reader for the shelf no longer rescans: progress and order refresh in place while catalog and cover caches stay valid.
- 残影治理回到参考固件路线：灰底只能靠周期 GC16 全清（档位默认 5 次一清，可调 0/3/5/10/14/20/30），实验性的 GL16 白行克隆已撤销；开机图保持 GC16 绝对刷。
  Ghost management returns to the reference recipe: the gray floor only clears through periodic GC16 full cleans (tier default 5, adjustable 0/3/5/10/14/20/30) and the experimental GL16 white-row clone is withdrawn; the boot splash stays an absolute GC16 pass.
- 锁屏密码新增“解锁验证”开关：每次解锁验证（默认）或仅开机验证——后者浅睡唤醒免输密码，开机仍需验证；清除密码入口保持在密码管理页。
  The lock PIN gains an unlock-verification switch: every unlock (default) or boot-only — the latter skips the challenge on light-sleep wake while boot stays gated; clearing the PIN remains in the PIN management page.
- 时间服务重做：RTC/SNTP 只做锚点，运行时间由单调钟插值推进，分钟显示不再随 15 秒轮询跳变；RTC 读数与插值偏差超 10 秒才重锚，截断抖动不追针；TIME_SYNC 写入后回读校验（≤2 秒），被忽略的写入不再误标已校准。
  Time service rebuilt: RTC/SNTP anchor only while runtime time interpolates on the monotonic clock, so the minute display no longer steps with the 15-second polls; RTC reads re-anchor only beyond a 10-second tolerance, ignoring truncation jitter; TIME_SYNC writes are read-back verified (≤2 s) so ignored writes no longer count as calibrated.
- 修复进书架即异常重启：封面后台提取任务按主任务同标准配 16KB 栈，消除 PNG 封面解码链的栈溢出。
  Fixes the shelf-entry abnormal reboot: the background cover extractor gets the same 16KB stack as the main task, removing the PNG-cover decode stack overflow.
- 锁屏密码输入即时反馈：按下立即以跟手 DU 高亮键位，抬起后圆点与键位并集一次推送，累计后 GL16 定稿清残影。
  Instant lock-PIN feedback: presses highlight the key at once with follow DU, releases push the dots/key union in one update, and GL16 settles the buildup.

## 2026-10-03 · Codex · v0.5.0 手帐改版、残影消除与唤醒竖纹修复 / Journal UI, anti-ghosting and wake artifact fix

- 「今日」根页升级为完整「阅读手帐」：底部导航标签改为“手帐”，图标换为精美双页笔记本线条图标；支持复古印章日期头（集成 PMU RTC、`os_lunar` 农历干支与 `READING STREAK` 连续打卡印章）、三项子视图自由切换（今日手记 / 打卡月历 / 金句便签）；「今日手记」呈现 4 格数据大盘、近 7 日时长走势直方图（今日实心高亮）与阅读足迹卡片；「打卡月历」以实心黑块直观反映本月阅读达标天数与连续打卡记录；「金句便签」深度打通 `rp_marks` 本地书签与精选文学便签，点击便签可一键开书续读；全页彻底去除多余背景杂点，保持纯净高对比度纸面。
  The Today root page upgrades to complete Reading Journal: bottom navigation label becomes "手帐" (Journal) with an open notebook line icon; features a vintage date stamp header (integrating PMU RTC, `os_lunar` ganzhi/zodiac and `READING STREAK` stamp), 3 switchable sub-modes (Today's Notes / Monthly Calendar / Quotes & Notes); "Today's Notes" renders a 4-grid stats dashboard, a 7-day trend bar chart (today solid-black) and reading footprint timeline card; "Monthly Calendar" displays stamped days with solid-black cells and streak tracking; "Quotes & Notes" integrates `rp_marks` local bookmarks and classic excerpts with one-tap book resumption; eliminates all background dot noise for a pristine paper-white ground.
- 「正在读」首页全新重构：按新版 UI 设计稿重构，顶层集成 Hero 聚焦主卡片（真机封面/排版封面、正在读黑底徽章、今日阅读分钟、大字标题、进度条与内置继续阅读按钮）、3 格阅读数据胶囊（今日阅读、连续打卡天数、在本书架总数），以及最近阅读精简列表（迷你封面、书名、格式与进度元数据、独立进度徽章胶囊，以及“全部图书 ›”快捷进入书架入口）。
  Now Reading home page complete overhaul: restructured per the new UI design drafts, featuring a top Hero focus card (bitmap/typographic cover, black "正在读" badge, today's reading minutes, prominent title, progress bar and embedded resume button), 3-chip stats strip (today's reading, continuous streak, total books on shelf), and a streamlined recent reading list (mini-covers, book title, format/progress meta, progress pill badges, and a "全部图书 ›" shelf jump link).
- 「设置」根页卡片化美化：设置列表项采用独立圆角卡片结构，视觉层次分明，与整套手帐纸质设计语言保持高度统一，同时严格保持原有交互与触区坐标兼容。
  Settings page card styling: settings list items use individual rounded card containers, creating clear visual hierarchy consistent with the paper journal design language while keeping full touch coordinate compatibility.
- 残影问题根治：彻底移除 `display.c` 中强制将 back 缓冲区归白的有缺陷实验逻辑，恢复真实的双缓冲差分（`front_fb` 与实际 `back_fb` 差分），使每次 GL16 页面切换与正文翻页都能准确对上一帧黑色像素计算并释放擦除脉冲，彻底消除切页和阅读过程中的隐约残影；累计刷新计数达到周期时，恢复为完整的 `MODE_GC16` 全像素黑白深度清除，不再伪白刷新。
  Root-cause fix for ghosting: completely removes the flawed white-baseline override in `display.c` and restores real dual-buffer differencing between `front_fb` and actual `back_fb`, ensuring every GL16 page turn and tab switch applies accurate erase pulses to prior dark pixels and eliminates faint ghosts; periodic refresh cleanup restores full-pixel `MODE_GC16` deep cleaning instead of fake white refresh.
- 锁屏唤醒密码页竖纹撕裂修复：唤醒后进入锁屏密码挑战（`app_lock_pin_challenge`）的首帧必须采用整屏 `update_display_full(hl)`（MODE_GC16）将旧锁屏/开机图完全擦净，消除两帧未擦除墨水在差分扫描中碰撞产生的中央纵向黑纹与脏污断线；密码输入过程后续按键保持流畅 GL16 局部更新。
  Fixes wake-to-PIN vertical tearing artifact: the first frame of `app_lock_pin_challenge` upon wake uses `update_display_full(hl)` (MODE_GC16) to cleanly erase the old lock face / boot splash, eliminating the center vertical black stripe and scan line noise; subsequent PIN digit entries retain smooth GL16 updates.
- 静置屏幕变脏问题解决：修复分钟级时钟轮询在破损差分下反复叠印导致的灰底老化与噪点累积；配合正确的物理差分擦除与 GC16 周期清屏，屏幕长时间静置保持纸白清爽。
  Eliminates idle screen soiling: fixes repeated ghost accumulation from minute clock polls under broken differencing; with proper physical differential erase and periodic GC16 clears, the display stays crisp and paper-white over long idle periods.
- 九等分九宫格点击分区重构：将原有的固定 4 种死板布局彻底重构为自由交互的 3×3 九宫格。屏幕等分为左上/中上/右上/左中/正中/右中/左下/中下/右下 9 个独立区域，每个区域均可在“无、上一页、下一页、菜单”四种动作中独立配置；设置页呈现直观交互的 3×3 宫格矩阵，点击任意格就地循环并持久化至 NVS，阅读时严格按触摸点所属宫格分发操作。
  Custom 9-grid tap zones: replaces the 4 rigid presets with a fully customizable 3×3 grid. The screen is evenly divided into 9 zones (top-left, top-mid, top-right, mid-left, center, mid-right, bottom-left, bottom-mid, bottom-right), each individually configurable to None, Previous Page, Next Page, or Menu. The settings page presents an interactive 3×3 matrix where tapping any cell cycles actions in place and persists to NVS; reading dispatches actions strictly by the tapped grid cell.
- 密码输入防闪烁与全刷真机恢复：密码输入 1~3 位数字时将整屏刷新替换为针对 4 个圆点区域的 8 帧高速 DU 局部推屏（`dots_area`），彻底消除输入单次密码导致全屏闪烁的问题；锁屏界面进入时走 `update_display_full(hl)`（GC16）将休眠前的前台内容彻底擦净；恢复 `APP_PAGE_FORCE_FULL = 1`，保证任何全刷请求（唤醒恢复、强刷按键 KEY2、阅读工具“清除残影”）均能执行真正的硬件级 GC16 全像素重置。
  PIN entry flicker-free & full refresh restoration: entering 1-3 PIN digits uses an 8-frame fast DU area update over the 4-dot region (`dots_area`), eliminating screen flashing on each digit tap; entering lock screen uses `update_display_full(hl)` (GC16) to cleanly erase previous app residues before sleep; restores `APP_PAGE_FORCE_FULL = 1` to ensure every full redraw request (wake restore, KEY2 forced clean, reader clean ghosting) executes a true hardware-level GC16 reset.

## 2026-10-03 · ZCode · v0.4.2 残影、EPUB 提速与点击分区 / Ghosting, faster EPUB and tap zones

- 整页切换为单遍“白基准 GL16”：back 缓冲归白后一次扫描绘整页，背景像素带厂家 (15,0) 克隆来的整段白推相（18 相），每页自带完整擦白——无黑闪、无累积残影；局部更新到期时同样整页白基准清理；阅读翻页（整屏面积 GL16）走同一路径。冷启动用物理清屏铺白。唤醒后先校验锁屏密码再回原页；开机图无条件展示，锁屏挑战前即可核对版本。
  Full-page transitions use a single-pass white-baseline GL16: reset the back buffer to white, repaint in one scan while every background pixel carries the vendor whitening sequence cloned from (15,0) (18 phases) — no black flash, no accumulating ghost; accumulated partial updates clean the whole panel the same way. Reading turns (full-screen GL16 areas) share the path. Cold boot whitens physically. Wake challenges the lock PIN before restoring the page; the splash always shows so the version is checkable before the lock challenge.
- EPUB 打开提速：离页保留最近一本书的目录与当前章（重进前校验 ZIP 中央目录），同一章二次打开即时；ZIP 目录常驻 PSRAM、CRC 查表加速。大书首次解析才显示加载提示，切章不再刷中间页。
  Faster EPUB opening: the last book's metadata and current chapter survive leaving (re-validated against the ZIP central directory), so re-opening the same chapter is instant; ZIP directories stay resident in PSRAM with table-driven CRC. Only first-time parsing of large books shows a loading note; chapter switches drop the intermediate refresh.
- 固件版本集中到 version.txt（本版 0.4.2-20261003.1，改版本号会自动触发重配置），开机图底部与设置页显示固件版本和构建时间，刷机是否成功一眼可查。
  The firmware version lives in version.txt (0.4.2-20261003.1 here; edits reconfigure the build automatically); the splash footer and Settings show the version and build time so a successful flash is obvious.
- 同步传输加硬：超时 15 秒、TLS 走 PSRAM、PMU 时钟回种系统时间；失败时提示分阶段错误码（连接/写入/响应/正文），替代笼统的“无法访问服务器”。
  Sync transport hardening: 15 s timeout, TLS allocations in PSRAM, PMU clock seeds system time; failures report staged error codes (connect/write/response/body) instead of a generic server message.
- 阅读新增“点击分区”设置页：左右翻页、右手、左手、上下四种布局，点正文翻页、中间呼出工具条；底栏四根换为书/书架/日历/设置抗锯齿线条图标，标签恒黑、选中下划线。
  Reading adds a tap-zone settings page with four layouts (sides, right hand, left hand, vertical): tap the body to turn, the center for tools; root navigation switches to anti-aliased line icons with always-black labels and an active underline.
- 网页刷机自动重启：elink.aittyy.com 页面补丁先走 ROM 软复位命令（与烧录同一命令通道）再补复位脉冲，烧录完成后设备应自行重启，不必长按电源键。
  Web flasher auto restart: the patched elink.aittyy.com page sends the ROM soft-reset command (the same command channel as flashing) followed by a reset pulse, so the device restarts itself after writing — no long power-key press.

## 2026-10-02 · Codex · 锁屏、残影与同步 / Lock, ghosting and sync

- 清理使用完整厂家 GC16，局部刷新到期后清理整屏；默认周期 5 次，已有选择保留。产品扫描采用 12 MHz 和更高预填，封面灰阶刷新前先绘制缓冲；锁屏唤醒去掉空白清屏过渡，密码四点居中，0/清空/退格回到键盘第四行。
  Cleaning uses complete vendor GC16 and covers the whole panel after accumulated partial updates. The default interval is 5, retaining saved choices. Product scan uses 12 MHz and more prefill; paint covers before grayscale presentation. Wake removes blank clear transitions, centers PIN dots and restores 0/Clear/Backspace to the fourth keypad row.
- 已保存 WiFi 默认重连，选中同网络免重复输密码；同步自动按需联网，阅读「更多设置 → 进度同步」可测试/上传/下载。下载仍须确认；跨固件位置按百分比近似并持久保存，打开当前正文后应用，避免旧进度覆盖结果。
  Saved WiFi reconnects by default without repeat password entry. Sync connects on demand, also via reader More Settings → Progress Sync. Pulls require confirmation; foreign positions persist as approximate percentages and are applied to the open reader without old progress overwriting them.
- 正在读/今日摘要与书架目录、封面跨根页面复用，来源/进度变化时失效；首页点击书籍直接打开并显示真实 EPUB 封面，无封面保留排版回退。设置小方框增加明确勾选。
  Home/Today summaries and shelf catalogs/covers survive root changes until their sources/progress change. Home opens books directly and shows real EPUB covers with typographic fallback; small Settings boxes show explicit checkmarks.

## 2026-10-02 · Codex · 字体、图文与灰阶阅读 / Fonts, inline images and grayscale reading

- 内置字库补齐 GB2312 的 6763 个汉字，压缩字库与静态图片保持原分区；字体列表每页六条，保留当前标记和正文示例，四根页面放大字号与调整排版。
  Built-in fonts cover all 6763 GB2312 Chinese characters; packed fonts/static images retain the partition layout. The picker shows six rows with selection and text preview, and root typography/layout is enlarged.
- EPUB 插图自动有界加载并与正文同页分页；失败与超限仍提供占位重试。去掉页脚进度刻度与隐藏跳转区，保留页码、可选百分比和目录跳转。
  EPUB images load within bounds for inline pagination, retaining retry placeholders on failures/limits. Removes the footer ruler and hidden jump area, keeping pages, optional percentage and TOC jumping.
- 阅读正文与页脚一次完整 GL16 灰阶更新，设置与字体切换不再强制黑闪；清残影周期由显示出口统一计数，默认 14 次，可关闭或手动清理。物理残影和速度待实机确认。
  Reader body/footer use one complete GL16 grayscale update; Settings/font changes no longer force a black flash. Display owns one cleanup counter, default 14, with Off/manual cleaning; physical ghosting and speed await device confirmation.

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
