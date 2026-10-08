# e0470_epaper_waveform

**E0470A01** 4.7 寸单色墨水屏（684 × 1216，40 pin）的 epdiy 波形表，以及供诊断实验使用的裁剪器。

epdiy waveform tables for the **E0470A01** 4.7" monochrome e-paper panel (684 × 1216, 40-pin), plus a trimmer for diagnostic experiments.

组件名 / Registry name: `mindreset/e0470_epaper_waveform`

## 波形表 / Tables

| 符号 / Symbol | 内容 / Content | 用途 / Use |
| --- | --- | --- |
| `E0470_WAVEFORM` | GC16原厂48相；GL16原厂48相加1相中性；附阈值DU，目标0–7取黑、8–15取白 / Vendor GC16 48 phases; vendor GL16 48 plus one neutral; threshold DU maps targets 0–7 to black and 8–15 to white | 普通灰阶页面、局推和周期清理 / Ordinary gray pages, local updates and scheduled cleaning |
| `E0470_FULL_WAVEFORM` | 完整厂家GC16 / GL16各48相、DU20相，单一0–50 °C温度档 / Complete vendor GC16 / GL16 48 phases each, DU 20 phases, one 0–50 °C range | GC16清理与厂家对照 / GC16 cleaning and vendor reference |
| `E0470_GRAY8_WAVEFORM` | 厂家8灰阶表，GC16 / GL16各30相 / Vendor eight-gray tables, GC16 / GL16 30 phases each | 可选灰阶诊断 / Optional gray diagnostics |
| `E0470_FOLLOW_WAVEFORM` | 开机生成8帧短DU，黑最多7相、白最多8相，配合FAST扫描档 / Boot-built eight-frame short DU, up to seven black and eight white actions, with FAST scans | 触摸笔迹与实时读数 / Touch ink and live digits |
| `E0470_TEXTTURN_WAVEFORM` | 完整厂家GL48加1相中性，保留全部变化与黑/灰对角线，白白保持 / Full vendor GL48 plus one neutral, retaining all changes and black/gray diagonals while holding white | 标准日间正文真实整页差分 / Actual full-page differences for standard day turns |
| `E0470_TEXTTURN_NIGHT_WAVEFORM` | 厂家变化GL48加1相中性，全部对角线保持 / Vendor changed GL48 paths plus one neutral, with all diagonals held | 标准夜间选择性翻页，黑底不参与0→0定稿 / Selective standard night turns, excluding black background from 0→0 settling |
| `E0470_DIRECT_WAVEFORM` | 厂家DU20相加1相中性，真实旧灰阶到黑白目标；中间目标全部保持 / Vendor DU 20 phases plus one neutral, actual old grays to binary targets; all intermediate targets hold | 速度优先黑白正文与搜索输入栏局推；图文章节正文不用 / Speed-first binary body turns and local search input; excluded for illustrated body turns |
| `E0470_NIGHT_BLACK_BOOST_WAVEFORM` | 5相，仅00使用厂家DU末尾两个黑动作，其余保持 / Five phases; only 00 uses the final two vendor DU black actions, all others hold | 夜间直刷成功后定向补黑实验 / Targeted reinforcement after successful night direct DU |
| `E0470_NIGHT_LOCAL_CLEAN_WAVEFORM` | 48相，仅00完整复制厂家GC16黑→黑，其余保持 / 48 phases; only 00 copies complete vendor GC16 black→black, all others hold | 旧字局部擦写实验，可能局部亮闪 / Local old-glyph erase/rewrite, possibly flashing locally |
| `E0470_NAVIGATION_WAVEFORM` | 厂家GL16原48相加1相中性，保留黑/灰对角线，白白保持 / Vendor GL16 48 phases plus one neutral, preserving black/gray diagonals and holding white | 普通导航重绘 / Ordinary navigation redraws |
| `E0470_NAVIGATION_ENTRY_WAVEFORM` | 仅真实15→15在第45相增加一次白动作，其余与NAV一致 / One added white action for actual 15→15 at phase 45 only; otherwise identical to NAV | 诊断对照，产品布局入口不调用 / Diagnostic reference, outside product layout entries |

开机在第一次刷新前调用一次 `e0470_waveform_init()`，生成阈值DU、跟随表及产品表。默认GL、导航与日间TEXT复用完整厂家48相加1相中性的同一LUT；夜间TEXT使用原有page LUT，仅将所有对角线清为保持，变化迁移不改。两者均49相，46–48相全中性，日夜分流不增加LUT内存。GC16及完整对照GL16直接引用只读厂家表，不替换真实变白路径，不添加内容呈现后的白推动。

Call `e0470_waveform_init()` once before the first refresh to build threshold DU, follow and product tables. Default GL, navigation and day TEXT share the same complete vendor 48-phase LUT plus one neutral phase. Night TEXT uses the existing page LUT, clearing only every diagonal to hold while preserving changed transitions. Both have 49 phases with neutral phases 46–48; the day/night split allocates no additional LUT memory. GC16 and full reference GL16 use read-only vendor data directly, without replacing actual to-white paths or adding post-content white drive.

## 黑白直刷 / Black-and-white direct

用户明确优先无闪速度，直刷选择厂家黑白DU，不保留八档抗锯齿灰边。调用前实际正文目标帧的每个4-bit像素按小于8取0、否则取15；不作空间抖动，日夜共用阈值。真实旧帧不量化，可为0–15任何灰码。直刷表对这16个真实起点到0/15逐相复制厂家DU的全部20相，再补1相中性；不压缩、连续重排或估算推动预算。源表最后两相中性，因此18–20相均保持。所有对角线及目标1–14全部保持，防止选择码被解释成灰阶写入。

The user prioritizes direct speed without a flash, choosing vendor black/white DU without eight-level antialiased edges. Before presentation, each actual 4-bit target becomes 0 below 8, otherwise 15, without spatial dithering; day and night share the threshold. Actual prior frames remain unquantized and may contain any code 0–15. For these 16 real sources to targets 0/15, direct copies all 20 vendor DU phases in order and appends one neutral phase, without compression, action rearrangement or estimated budgets. The source's last two phases are neutral, so phases 18–20 hold. Every diagonal and all targets 1–14 hold, preventing selector codes from becoming gray writes.

直刷表以 `MODE_GL16` 描述符接入现有选择性整页API，实际动作来自DU，并不提供灰阶迁移。厂家DU对目标1–14没有驱动，不能据此承诺八档真实灰阶；标准GL16及8灰阶源表则含反向擦除与写入。本板SDK同样从真实旧灰码选择厂家DU端点路径，并对目标使用黑白阈值，见[固定SDK源码](https://github.com/0x1abin/freeink-sdk/blob/96de1be6ce08eb732909e6e8149af8f892b9a2c5/libs/display/EpdiyLcd/src/e0470/e0470_epaper_waveform.c)。本实现保留DU端点路径，不移植其他面板电压或时序。

Direct uses a `MODE_GL16` descriptor to integrate with the existing selective page API, but its actions come from DU and provide no gray transitions. Vendor DU drives none of targets 1–14 and cannot establish eight actual shades; vendor GL16 and eight-gray tables contain reverse erasure and writing. The board SDK likewise selects vendor DU endpoint paths from actual old gray codes and thresholds targets to black/white, as shown in the [pinned SDK source](https://github.com/0x1abin/freeink-sdk/blob/96de1be6ce08eb732909e6e8149af8f892b9a2c5/libs/display/EpdiyLcd/src/e0470/e0470_epaper_waveform.c). This implementation retains those endpoint paths without importing other-panel voltage or timing settings.

标准正文及含已解码插图章节保留灰阶。无遮挡直刷正文首帧仍提交实际黑白目标；真正布局切换清理一次，夜间目标先物理清白再GC16，后续普通翻页使用DU，夜间设置周期到期也先清白再GC16，工具条、图片、清理/摘录确认及读完覆盖层保留灰阶，局部/分钟更新保留各自灰阶路径。0.5.20的直刷速度和辅助线位置已获用户真机确认，21相比标准49相少，但相数不能代替整次绘制、供电及调度的实测速度，也不证明零残影。

Standard body turns and chapters with decoded illustrations retain gray. Initial unobstructed direct body frames still commit actual binary targets; real layout changes clean once, with a physical white clear before night GC16; subsequent ordinary turns use DU, and due night turns also clear before GC16. Toolbar/image/cleanup-confirmation/excerpt-confirmation/read-complete overlays retain gray, as do the respective local/minute paths. The user confirmed 0.5.20 direct speed and guide placement on hardware. Its 21 phases are fewer than standard's 49, but phase count does not measure total rendering, power and scheduling latency or establish zero ghosts.

搜索输入栏复用DIRECT DU21而不新增波形。完整轻点只绘制604×168输入栏，物理坐标区域先用 `display_prepare_direct_area()` 量化为真实0/15目标并保留奇边区外像素；其它控件保留灰阶。局部API仍使用 `MODE_GL16` 类型描述符，厂家动作仍是DU；所有中间目标及对角线保持，差分扫描对齐扩展中的未改灰阶不参与二值推动。连续输入不穿插GL，最后呈现结束两秒后重画输入栏灰阶并局推，成功后消费待定稿状态、失败两秒后重试，不计正文翻页周期。
Search input reuses DIRECT DU21 without a new waveform. Complete taps redraw only the 604×168 input. `display_prepare_direct_area()` converts its physical region to actual 0/15 targets while preserving outside edge pixels; other controls retain gray. The local API still uses a `MODE_GL16` type descriptor, but vendor actions remain DU. Held intermediate targets and diagonals prevent unchanged grays in aligned scan extensions from receiving binary drive. Continuous input never interleaves GL; two idle seconds after the last presentation, the input is redrawn in gray and locally presented, consuming settling work on success or retrying after two seconds on failure, outside body-turn counts.

## 参考帧与入口 / Baselines and entry

日间标准使用 `epd_hl_update_screen_full()` 的实际差分码，保留包括0→0在内的厂家对角线，使逻辑未变黑芯也按原厂序列重新定稿；15→15白底保持。其14→14不是中性，因此不能调用选择性EE出口。夜间标准及直刷配 `epd_hl_update_screen_selective(..., NULL)`，全部未变像素编码EE并保持。夜间若使用日间0→0的18白/18黑定稿，会令黑底整屏亮闪，因此保留原选择性策略；厂家15→15也保持，夜间未变白芯无法同样重新定稿。

Standard day uses actual difference codes from `epd_hl_update_screen_full()`, retaining vendor diagonals including 0→0 so logically equal black cores undergo the original reconditioning sequence. Actual 15→15 white holds. Its 14→14 is active, so selective EE presentation is incompatible. Standard night and direct pair with `epd_hl_update_screen_selective(..., NULL)`, encoding every unchanged pixel as held EE. Applying day's 18-white/18-black 0→0 sequence at night would flash the whole black background light, so night retains its previous selective strategy. Vendor 15→15 also holds; unchanged white night cores cannot receive the same reconditioning.

导航普通重绘保留厂家黑/灰对角线定稿与白白保持。产品的实际应用/菜单/阅读视图布局边界由上层事件标记，日间单次完整厂家GC16、夜间物理清白后一次GC16目标提交成功才消费标记，失败保留并重试；普通翻页、按钮、加载tick不重复边界清理，夜间到期翻页独立遵循下方周期，详细事件范围见[阅读主页契约](../../docs/READING_HOME.md)。前缓冲保留实际目标；普通更新沿用真实旧灰，夜间清理只有实际物理清白后才建立白参考，失败不提交目标。不伪造未经清理的白基准，不按历史字形推白。手动清理、启动和故障恢复继续保留。

Ordinary navigation retains vendor black/gray diagonal settling and held white. Upper-level events mark actual app/menu/reader-view layout boundaries; one vendor GC16 for day, or one GC16 target commit after night physical clearing, consumes the marker only on success; failed boundaries remain eligible for retry. Ordinary turns, controls and loading ticks never repeat boundary cleaning; due night turns independently follow the interval below. The [reader contract](../../docs/READING_HOME.md) defines the exact events. Front buffers retain actual targets. Ordinary updates keep actual prior grays; night cleanup establishes white only after physically clearing and never commits failed targets. White references are never fabricated on uncleared panels, and no historical glyph whitening is applied. Manual cleanup, boot and fault recovery remain.

夜间阅读整屏暗色在应用目标帧中完成，正文、边距、页脚、阅读工具与从阅读打开的预览统一反色；目录、阅读设置等非READING视图保持原配色。标准灰阶抗锯齿与直刷最终黑白目标保持各自契约，暗色切换不引入面板反相波形或电压变化。
Whole-frame dark reading is composed in the application target: body, margins, footer, reading tools and previews opened from reading invert together. Non-READING views such as TOC and reader settings keep their palette. Standard antialiasing and direct final binary targets retain their respective contracts, without panel inversion waveforms or voltage changes.

夜间页脚、加载反馈及其它普通阅读GL更新复用现有NIGHT GL49，保持0→0黑背景，标准与直刷均适用；日间加载反馈保留厂家DU。局推及覆盖层重绘不计正文周期，搜索输入使用局部DU，其它非阅读页面保留原厂家GL路径。
Night footer, loading feedback and other ordinary reader GL updates reuse existing NIGHT GL49 to hold the 0→0 black background in both profiles; day loading feedback retains vendor DU. Local updates and overlay redraws do not count toward the body interval, and search input uses local DU, while other non-reader pages retain their original vendor GL paths.

夜间普通标准GL49与直刷DU21保持原波形，清理周期在显示出口完成，不派生INV波形或增加LUT推动。用户选择夜间优先无闪并按 `gc_every` 清理：0关闭，关闭后的成功夜间翻页清除未完成周期；N为两模式共用成功夜间正文翻页数，第N次先 `epd_clear()` 物理清白，建立真实白参考后一次完整厂家GC16提交保留目标，成功后归零。夜间手动清理和实际返回暗色正文布局也使用该路径。失败、普通按钮/覆盖层/分钟/加载tick不计数；任何成功整屏GC16（含布局、手动及故障恢复）及开机物理清白都归零，恢复GC不额外计一次翻页。日间正文不计此周期，所有参考仍成功才提交；`epd_clear()` 原有三轮清白保留，一次清理操作可能多次整屏亮暗闪动且更慢，不能承诺零物理残影。
Ordinary night standard GL49 and direct DU21 keep their waveforms; the display layer handles the interval without deriving INV waveforms or adding LUT drive. The user chooses no flashing between `gc_every` cleanups: 0 disables it, and a successful night turn while disabled clears any unfinished interval. N counts successful night body turns shared by both modes. The Nth physically clears white with `epd_clear()`, establishes a truthful white reference, then commits the retained target once with complete vendor GC16 and resets on success. Night manual cleaning and actual returns to the dark body layout use the same path. Failures and controls/overlays/minute/loading ticks do not count; any successful full-screen GC16, including layout, manual and fault recovery, or the physical boot white clear resets it. Recovery GC never adds another turn. Day body turns omit this interval and all baselines still commit only on success. `epd_clear()` retains its original three clearing rounds, so one cleanup operation can flash light/dark across the panel several times and take longer; it does not establish zero physical ghosts.

0.5.19的任意单向灰阶估算、统一18相白推动及三相入口补偿已被真机确认退化，0.5.20撤回。0.5.21仅为标准日间恢复厂家非白对角线，夜间保持原策略；按用户授权将布局边界清理改为一次原厂GC16，不再接旧入口单白相表。端点预算差不是灰阶校准，未经本板实测不得再作为修复交付。0.5.23在夜间手动/布局返回/周期清理前增加真实物理清白，普通夜间和日间波形不变。源表、电压和扫描频率不变，夜间残影与所选清理周期效果仍待真机验证，不承诺改善光学白度。

Device feedback confirmed regression from 0.5.19's estimated monotonic gray, uniform 18-phase white drive and three-phase entry compensation, withdrawn in 0.5.20. Version 0.5.21 restores vendor nonwhite diagonals for standard day only, retaining night's strategy. The user-authorized layout cleanup uses one original vendor GC16 instead of the old white-tick entry table. Endpoint-budget differences do not calibrate gray and must not be delivered as fixes without board measurements. Version 0.5.23 adds actual physical white clearing before night manual/layout-return/interval cleanups, retaining ordinary night and day waveforms. Source tables, voltages and scan frequencies remain unchanged; night ghosts and the selected cleanup interval still need device validation, without a promise of improved optical whiteness.

`E0470_NAVIGATION_ENTRY_WAVEFORM`、`E0470_WHITE_CLEANUP_WAVEFORM`及历史像素工具仅保留诊断对照，产品不调用、不分配历史或选择图。诊断入口表仅真实15→15的第45相白动作与NAV不同，其他255条迁移一致，46–48相中性。非NULL选择图只适用于支持FF擦白的实验波形；夜间选择性正文及直刷的FF/EE均保持，日间和导航EE则是实际14→14定稿。

`E0470_NAVIGATION_ENTRY_WAVEFORM`, `E0470_WHITE_CLEANUP_WAVEFORM` and history helpers remain diagnostic references outside product updates, with no product history/selector allocations. Diagnostic entry differs from NAV only in actual 15→15's phase-45 white action; the other 255 transitions match and phases 46–48 stay neutral. Non-NULL selectors require an experimental waveform supporting FF erasure. Night selective body and direct FF/EE both hold, while day and navigation EE represent actual 14→14 settling.

## 验证 / Verification

`bash tools/run_display_host_test.sh`运行真实显示出口和波形源码，穷举日间256×49全部迁移/对角线与厂家相位一致、15白底保持，夜间全部变化一致且对角线保持；核对日间复用默认LUT不增加内存。DU真实旧灰到二端点逐相原表一致、无反向、三相中性尾不变，并验证packed像素阈值、日夜/幂等、真实成功参考、模式切换、一次边界GC16目标提交、夜间物理清白时保留目标及真实白参考、夜间两模式共用周期/关闭/失败不计数/成功GC重置及日间不周期。诊断入口表单相差异仍独立核对，但产品刷新不调用。

`bash tools/run_display_host_test.sh` runs production display and waveform code, exhaustively checking every day 256×49 vendor transition/diagonal, held white 15, exact night changed paths with held diagonals, and day reuse of the default LUT without more memory. DU still matches every vendor phase from actual old grays to both endpoints, with no reverse drive and three neutral tails. Tests cover packed thresholds, day/night/idempotence, truthful success baselines, mode switches, one boundary GC16 target commit, retained targets and truthful white references through night physical clears, shared night intervals/off/failure counts/GC resets and unscheduled day turns. The diagnostic single-phase entry difference is checked independently and excluded from product updates.

`bash tools/run_display_pixels_area_host_test.sh`在ASAN/UBSAN下检查区域量化全部256种打包灰值、奇边半字节、负/极端裁剪、无效输入及护栏；`python3 tools/preview/verify.py --sanitize`检查真实搜索输入的刷新选择/区域/灰阶恢复，以及夜间手动、返回正文与周期清白后的目标保持。旋转由调用方处理，物理区域转换器不依赖面板或GUI。
`bash tools/run_display_pixels_area_host_test.sh` checks all 256 packed gray values, odd edge nibbles, negative/extreme clipping, invalid input and guards under ASAN/UBSAN. `python3 tools/preview/verify.py --sanitize` checks real search refresh selection/bounds/gray restoration and retained targets after night manual/body-return/interval physical clears. Callers own rotation; the physical-area converter has no panel or GUI dependency.

`python3 tools/book_guide_host_test.py`以真实布局和字体轮廓核对纯文字辅助线网格，检查任意前后页组合的旧线×新字形零交叉；开启/关闭按文本锚点重新分页，实虚线切换仅重绘。完整几何契约见[阅读主页文档](../../docs/READING_HOME.md)。测试不模拟墨水粒子、光学灰度、闪烁或残影累积。应在同板清理一次后比较两模式连续20–30页及Tab往返的静止旧字、旧线和白底。

`python3 tools/book_guide_host_test.py` checks the text-only guide grid with production layout and font outlines, ensuring zero intersections between old rules and new glyph ink for arbitrary page pairs. Guide on/off repaginates at the text anchor; solid/dashed switching only redraws. The [reader document](../../docs/READING_HOME.md) defines the full geometry contract. Tests do not model ink particles, optical gray, flicker or accumulated ghosts. On the same board, clean once before comparing settled old glyphs/rules and backgrounds after 20–30 turns in both modes and Tab round trips.

## 裁剪器 / Trimmer

`e0470_waveform_trim()` 在不改变灰阶梯子的前提下缩短完整的 GC16 / GL16 表。完整表中每对 (from, to) 的序列共用同一个骨架：保持补齐、往反向轨道擦除、在目标轨道饱和、灰阶尾、结尾保持。灰阶档位全部位于尾段；裁剪器保留尾段不动，只裁擦除上限和饱和开头，再把所有序列右对齐。代价是擦除余量变小，旧内容的残影会先出现，因此参数需要在真机上对着残影调整。

`e0470_waveform_trim()` shortens a full GC16 / GL16 table without changing its gray ladder. Every (from, to) sequence in the full table shares one skeleton: hold pad, erase toward the opposite rail, saturate on the target rail, gray tail, trailing hold. The gray levels live in the tail; the trimmer keeps the tail intact, cuts the erase maximum and the saturation head, then right-aligns every sequence. The cost is less erase margin, so ghosting from old content appears first. Parameters should be tuned on hardware against that ghosting.

历史诊断裁剪参数（不用于产品刷新）：`erase_max = 11`，`sat_cut = 5`，`white_sat_cut = 0`，`hold = 3`。

Historical diagnostic trim (outside product presentation): `erase_max = 11`, `sat_cut = 5`, `white_sat_cut = 0`, `hold = 3`.

## 辅助函数 / Helpers

- `e0470_waveform_phases()`：取某条波形中 `MODE_GC16` / `MODE_GL16` / `MODE_DU` 的相位表。/ The phase table for `MODE_GC16` / `MODE_GL16` / `MODE_DU` in a given waveform.
- `e0470_phase_action()`：查 (from → to) 在指定相位的 2 bit 动作（保持 / 压黑 / 擦白）。/ The 2-bit action (hold / darken / erase) for (from → to) at a given phase.
- `e0470_follow_lut_build()`：生成任意帧数的跟随表。/ Build a follow table of any frame count.

## 说明 / Notes

自行调整屏幕波形会使设备失去保修。`waveforms/*.h` 是数据表，不应手工编辑。

Changing panel waveforms voids the device warranty. The `waveforms/*.h` files are data tables and are not meant to be edited by hand.

帧周期固定为 11 090 µs（约 90 Hz），取自面板算法文档的设计点。扫描时序由 `read_pico`（`read_pico_epd_timing.h`）负责，不在本组件内。

The frame period is fixed at 11 090 µs (about 90 Hz), the design point in the panel's algorithm document. Scan timing is owned by `read_pico` (`read_pico_epd_timing.h`), not by this component.

## 依赖 / Dependencies

`epdiy`（提供 `epd_waveform.h`）。

`epdiy` (for `epd_waveform.h`).

## 许可 / License

Apache-2.0，见 [LICENSE](LICENSE)。波形表随本板附带，按现状提供。

Apache-2.0, see [LICENSE](LICENSE). The waveform tables ship with the board and are provided as-is.

## 夜间实验 / Night experiment

0.5.25按用户授权提供可关闭的两档实验，仅在纯文字夜间直刷的普通DU成功后调用。00表示实际目标已黑且旧帧曾亮的位置，EE及其它码保持；不伪造旧白差分。定向补黑使用厂家白→黑DU的末尾两个黑动作加三相中性；局部擦写完整复制厂家GC16真实黑→黑48相，不压缩或重排。实际帧不改，两扫全成功才计一次翻页，失败完整恢复；周期/入口/手动清理替代实验。标准、日间及图文章节不选这两张表。
Version 0.5.25 offers two optional user-authorized experiments only after successful ordinary DU in text-only binary night turns. Selector 00 denotes an actual black target that was light in the prior frame; EE and all other codes hold without fabricated white differences. Reinforcement uses the final two black actions from vendor white→black DU plus three neutral phases. Local cleaning copies every phase of complete vendor GC16 black→black without compression or reordering. Actual frames stay unchanged; only both successful scans count, with full recovery after failure. Due/entry/manual cleanup replaces the experiment. Standard, day and illustrated chapters never select these tables.

`bash tools/run_display_host_test.sh`逐相检查实际实验表的256选择码与厂家相序，以及原厂家/产品表不变。附加推动未光学校准，局部擦写可能轻闪、变慢；软件测试不能证明残影改善或长期效果，需同板对照。完整运行及失败契约见[阅读刷新说明](../../docs/READING_HOME.md#夜间旧字实验--night-old-glyph-experiment)。
The display host runner checks all 256 selectors against actual experimental/vendor phase order and unchanged original profiles. Added drive is not optically calibrated; local cleaning can flash and slow down. Software tests establish neither ghost reduction nor long-term efficacy; compare on the same panel. The linked reading specification defines operation and failures.
