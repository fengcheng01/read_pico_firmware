# 蓝牙翻页器 / Bluetooth page-turner

PageRest 0.6.15 在「设置 → 蓝牙翻页器」提供 BLE HID 主机功能，初始关闭。一次连接一个翻页器，保存最近成功连接的一台及上一页、下一页、锁屏三项自定义按键。
PageRest 0.6.15 adds a BLE HID host under Settings → Bluetooth Page-turner, initially off. One remote connects at a time; the last usable peer and Previous/Next/Lock custom buttons are remembered.

阅读中按中键打开工具，点「蓝牙翻页器」即可搜索、连接或测试；点返回或左键回到原书原位置。已就绪链路经过设置、主页和书架导航仍保留，非正文输入丢弃；未完成的搜索/连接离页仍收尾。锁屏/WiFi主动停止，续读按已保存设备回连。
While reading, open Tools with the middle key and choose Bluetooth Page-turner to search/connect/test. Back or the left key restores the same book/position. Ready links survive Settings/Home/Library navigation without dispatching non-body input; unfinished scans/connections still stop on exit. Lock/WiFi stop radio, and reading reconnects the saved peer.

## 使用 / Use

1. 开启蓝牙，让翻页器进入配对模式；如果它仍连着手机，先断开手机连接。
   Enable Bluetooth and put the remote in pairing mode. Disconnect it from a phone first if necessary.
2. 点「搜索翻页器」，等待约 12 秒；每页四台，选择名称后核对设备，再点「连接此设备」。未广播 HID 的有名设备也可显示，但连接时仍须通过 HID 服务验证。
   Search for about twelve seconds, browse four peers per page, select a name and confirm the device. Named peers without an advertised HID UUID remain visible but must pass HID service discovery.
3. 「链路已连接」仍在准备输入；看到「按键就绪」再按键测试。成功后保存所选设备，进入阅读正文时会回连。
   “Link connected” still means input discovery is underway. Wait for “Keys ready,” then test a button. The usable peer is remembered for reconnecting in the reader.
4. 方向不合习惯时打开「方向反转」。未映射的按钮可点「学习上一页／下一页」，松开所有键后按一次目标键；30 秒无输入会结束学习。同一个按钮不能同时绑定两个方向。「恢复标准按键」清除自定义映射；「遗忘此设备」须再次确认。
   Reverse directions if desired. For an unmapped button, choose Learn Previous/Next, release every key, then press the target once. Learning ends after thirty seconds without input. A button cannot serve both directions. Restore Standard Keys removes custom mappings; Forget requires confirmation.

5. 需要遥控锁屏时点「学习锁屏」，松开所有键，再按独立按钮。标准翻页键及已用于其他学习动作的键会被拒绝，避免误锁屏；恢复标准按键也清除锁屏映射。锁屏仅在正文生效，设置页的按键测试不会锁屏。旧版本配对及双键映射会保留，锁屏没有默认分配。
   To lock remotely, select Learn Lock, release every button, then press an independent button. Standard turn keys and keys assigned to another learned action are rejected. Restore Standard Keys also clears lock. Lock works only in body text, never the settings test. Existing pairing/two-key mappings migrate without assigning a lock default.

翰林阅Free3须实际发出独立BLE HID报告才可学习；长按若仍发送相同翻页报告，不能当作独立锁屏键。尚未实机核对该型号按键报告。锁屏复用设备原有的进度保存、停止蓝牙及睡眠流程；电源键或已配置的拿起方式唤醒后恢复阅读，翻页器无法唤醒。
Hanlin Free3 needs a distinct BLE HID report for learning. A hold that sends the same page report cannot become a separate lock button. This model's reports have not been verified on hardware. Lock reuses progress-saving, BLE-stop and sleep handling; wake with the power key or configured lift gesture, not the remote.

| 标准键 / Standard key | 默认动作 / Default |
| --- | --- |
| Left / Up / PageUp | 上一页 / Previous |
| Right / Down / PageDown / Space | 下一页 / Next |
| Volume Up / Previous Track / Consumer Page Back | 上一页 / Previous |
| Volume Down / Next Track / Consumer Page Forward | 下一页 / Next |

方向反转作用于上一页/下一页，锁屏动作不受反转影响。测试区显示反转后的实际方向。没有释放报告的非标准遥控器采用保守间隔识别，不自动合成长按连翻；相同标准 HID 按住报告只算一次。
Reversal affects standard and learned page directions only, and the test area shows the resulting direction. Nonstandard remotes without release reports use a conservative report gap; no held repeats are synthesized. Identical held standard HID reports count once.

## 行为与兼容边界 / Behavior and limits

- 输入只控制正文前后页及已学习锁屏。工具、目录、摘录确认、插图、读完面板、全局菜单和触摸交互会丢弃输入，回正文不补翻。短时密集输入合并为最多一页，避免积压；排队超过 1.5 秒的按键过期丢弃，覆盖章节边界的排版与刷屏耗时。
  Input controls body page turns and learned lock only. Tools, TOC, excerpt confirmation, images, end panels, global menus and touch discard it without replay. Short bursts produce at most one turn rather than a backlog; presses queued longer than 1.5 s expire, covering chapter-boundary layout and refresh time.
- 导航离页保留已建立链路；锁屏或启动 WiFi 扫描/同步/传书前停止 BLE 并释放协议栈；失败时不继续启动 WiFi 或睡眠。阅读恢复后重新连接所选设备；连接或发现失败不再停滞，间隔 10 秒自动重试。蓝牙本身不延长空闲锁屏时间，蓝牙按键不能唤醒锁屏。
  Navigation retains an established link; locking or starting WiFi scanning/sync/transfer stops BLE and releases its stack. Failed stop prevents WiFi/sleep. Resumed reading reconnects the selected peer; connection or discovery failures no longer stall, retrying every ten seconds. Merely enabling BLE does not prevent idle lock, and Bluetooth keys cannot wake the locked device.
- ESP32-S3 只支持 BLE，传统 Bluetooth Classic 翻页器、需要独立夹屏接收器的产品不属于直接配对范围。需要输入口令或数字比对的键盘本版明确拒绝；鼠标/触摸手势和厂商私有 GATT 协议不提供自动适配。
  ESP32-S3 supports BLE only. Classic-only remotes and products requiring a separate screen-clipping receiver are outside direct pairing. This version rejects passkey/numeric-comparison keyboards; mouse/touch gestures and vendor-specific GATT protocols have no automatic adaptation.
- HID 布局按报告描述符、报告号及输入类型解析。未知布局仅提供整帧学习；带动态计数、加密或不断变化字节的私有报告不保证可学习。学习按键按帧内容匹配，不依赖重连后的特征枚举顺序。未交换身份密钥、使用轮换地址的设备重新搜索连接后，学习按键保留；只有遗忘或恢复标准按键才清除。
  HID parsing follows descriptors, report IDs and input types. Unknown layouts offer whole-frame learning only; private reports with counters, encryption or changing bytes may not learn reliably. Learned keys match frame content rather than post-reconnect characteristic ordering. Rotating-address peers keep learned keys after rescanning and reconnecting; only Forget or Restore clears them.
- 保存失败保留旧偏好并提示重试；无线初始化失败间隔 30 秒重试，连续三次中断后需关闭再开启。启动内存不足先释放插图成品、封面与字形等可重建缓存再判断，仍不足才拒绝；拒绝时日志记录内部 RAM、最大连续块与 PSRAM 实测值。内存门槛只用于提前拒绝明显不足，不能代替真实设备峰值、功耗与长时间测试。
  Save failure retains previous preferences and offers retry. Failed startup retries after thirty seconds; three interrupted attempts require toggling off/on. Short start headroom first releases rebuildable decoded-image, cover and glyph caches and rechecks; refusal logs the measured internal RAM, largest block and PSRAM. Memory floors reject obvious shortages, not replace device peak-memory, power or endurance tests.

0.6.11修复选择设备后一直建立连接：IDF6.1的主动扫描取消同步返回，不发送DISC_COMPLETE；取消成功或已结束后直接连接，不等待额外事件。连接请求先设置等待超时，旧连接断开失败、协议栈未就绪和连接超时均进入明确错误；失败后可重新搜索/连接。链路建立后仍需完成HID发现和订阅才算按键就绪，不据此宣称Free3实机兼容已验收。
Version0.6.11 fixes a connection stall: IDF6.1 active scan cancellation returns synchronously without DISC_COMPLETE. Connect directly after successful/already-ended cancellation. Arm the wait timeout before transitions and report old-link termination, host-readiness and connection errors. Search/connect can be retried. HID discovery/subscription must still finish before Keys Ready; this does not establish Free3 hardware compatibility.

## 维护与验证 / Maintenance and validation

`main/os/ble_hid.c` 是纯解析器；`os_ble_radio_pico.c` 独占 NimBLE central、串行服务/特征/描述符发现及订阅。描述符范围止于下一特征之前，Report Reference 的输出报告不订阅为输入；mbuf 链使用有界复制，不直接假设连续内存。`os_page_turner.c` 是 UI 任务上的产品状态机，使用独立 `rp_turner` NVS，IDF 自行持久化配对密钥。新增机型应替换无线边界并明确能力，不能仅改设备名称宣称兼容。
`main/os/ble_hid.c` is a pure decoder. `os_ble_radio_pico.c` owns the NimBLE central, serial discovery/subscription, descriptor ranges ending before the next characteristic and Report Reference input/output filtering. Bounded copies traverse mbuf chains rather than assuming contiguous data. `os_page_turner.c` runs product policy on the UI task with independent `rp_turner` NVS; IDF persists bonding keys. A new target requires its own radio boundary and explicit capabilities, not just a renamed device.

可复跑宿主检查 / Reproducible host checks:

```sh
python3 tools/run_ble_tests.py
python3 tools/preview/verify.py --sanitize
python3 tools/run_os_home_tests.py
python3 tools/wifi_scan_lifecycle_host_test.py
```

宿主预览使用明确标为模拟的设备，不访问电脑蓝牙。宿主验证不等于具体翻页器配对、射频、显示光学、睡眠功耗或断电恢复的真机验收。
The preview uses peers explicitly labeled simulated and never accesses the computer's Bluetooth. Host checks do not establish pairing, RF, optical, sleep-power or power-cycle acceptance for a physical remote.

设计参考：[RickyOS 的蓝牙设置与映射流程](https://github.com/chinoryunqin/RickyOS/blob/ecfc977e6dd2fa93ef3a1f237ea0c0e48ad292ae/src/activities/settings/BluetoothSettingsActivity.cpp)、[KiikoRead 的 BLE 翻页器实现](https://github.com/wegooo-cell/read-pico-reader/blob/4bd2b9e808c214c7606cb4c31e606af146d48bce/components/ble_page_turner/src/ble_page_turner.c)。此处是针对当前 ESP-IDF 页面契约独立实现，并未直接复制其无线状态机。
Design references: [RickyOS Bluetooth settings/mapping flow](https://github.com/chinoryunqin/RickyOS/blob/ecfc977e6dd2fa93ef3a1f237ea0c0e48ad292ae/src/activities/settings/BluetoothSettingsActivity.cpp) and [KiikoRead's BLE page-turner](https://github.com/wegooo-cell/read-pico-reader/blob/4bd2b9e808c214c7606cb4c31e606af146d48bce/components/ble_page_turner/src/ble_page_turner.c). This implementation independently targets the current ESP-IDF page contracts without copying their radio state machines.

蓝牙页进入和离开时回收可重建字体缓存，保留字体选择；返回沿用进入来源，阅读工具进入后恢复原书位置。
BLE page entry/exit reclaims rebuildable font caches while retaining the selection; Back follows the entry origin and reader-tool entry restores the original book position.

0.6.15修复真机反馈的三个稳定性问题：阅读中途进蓝牙页报“内存不足”，先释放可重建的插图/封面/字形缓存再判断，内部 RAM 门槛按 EXTALLOC 实际需求放宽并记录实测值；学习按键“突然未映射”，替换保存设备不再清空学习映射、绑定改按帧内容匹配；连接/发现失败后不再永久停在错误相，间隔自动重连，排队按键的过期上限从 0.8 秒放宽到 1.5 秒。锁屏浅睡保持断链不变，蓝牙仍不能唤醒本机。
Version 0.6.15 fixes three hardware-reported stability issues: entering the BLE page mid-reading no longer fails headroom outright (rebuildable image/cover/glyph caches are released first, internal-RAM floors relax to EXTALLOC needs with measured values logged); learned keys no longer vanish (replacement saves keep mappings and bindings match frame content); connection or discovery failures retry automatically instead of parking in the error phase, and queued presses now live 1.5 s instead of 0.8 s. Lock-screen light sleep still disconnects; Bluetooth still cannot wake the device.
