# 版本变更 / Changelog

按日期和作者简述对用户可见的功能变化；详细实现历史见 Git。使用方法见 [README](../README.zh-CN.md)。
User-visible changes by date and author; Git retains implementation history. See [README](../README.md) for usage.

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
