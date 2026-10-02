# USB 连接电脑 / USB SD-card disk

小纸 Pico 的 TF 卡可通过 USB MSC 作为电脑磁盘使用。入口：**设置 → 存储与设备 → USB 连接电脑 → 确认连接**。电脑可复制、读取和删除卡上的文件；本功能不导出内置书库。

Read Pico exposes the TF card as a USB MSC disk. Open **Settings → Storage → USB computer connection → Confirm**. Computers can copy/read/delete card files; internal storage is never exposed.

1. 插入 TF 卡，等存储检测完成；用 USB 数据线连接电脑，在设备上确认进入。设备会重启，电脑将重新识别为 `Read Pico SD` 磁盘；Windows 文件资源管理器、macOS Finder、Linux 文件管理器可访问，盘符/卷名由卡自身及电脑决定。
   Insert TF, wait for probing and connect a USB data cable. Confirm on the device; it reboots and enumerates as `Read Pico SD`. The host/card determines drive letters and volume names.
2. 图书放进卡根目录 `books/`（TXT/EPUB）；完整 TTF 字体放进 `assets/fonts/` 或 `fonts/`。没有文件夹时可在电脑创建。卡上的文件系统须能被设备 FatFs 读取；USB 模式不替用户格式化。电脑提示初始化/格式化时先取消并检查卡，尤其已有数据时。
   Put TXT/EPUB in `books/`, TTF in `assets/fonts/` or `fonts/`; create directories on the computer if needed. Use a filesystem supported by device FatFs. USB mode never auto-formats; cancel host initialization/format prompts and inspect existing media first.
3. 写入完成后先在电脑“安全弹出”，再点设备“结束连接 → 退出并重启”。如果电脑只卸载文件系统而未发送介质弹出，先安全卸载，再拔 USB 数据线后退出。重新开机后阅读、字体与刷机串口恢复。
   Safely eject on the computer, then select End connection → Exit and restart. If the host only unmounts without a media-eject command, safely unmount, disconnect the cable, then exit. Reading, fonts and the flashing serial port return on boot.

磁盘模式中只运行专用 USB/触屏流程，暂停阅读、WiFi 传书、自动锁屏与睡眠；电源长按强制断电仍由硬件控制，不应在写入时操作。USB MSC 与原 USB Serial/JTAG 共用内部 PHY，磁盘模式没有原刷机串口；退出后再在线刷固件。拔卡会停止本次连接，重新插卡不会恢复旧会话，须退出并再次进入。TF 卡不能被电脑和设备文件系统同时访问。

Disk mode runs only the dedicated USB/touch flow; reading, WiFi transfer, idle lock and sleep pause. Hardware forced power-off still applies; avoid it during writes. MSC shares the internal PHY with USB Serial/JTAG, so exit before flashing. Card removal stops the session; reinsertion requires exiting and re-entering. The host and device filesystem never access TF concurrently.

## 维护契约 / Maintenance contract

- 正常 UI 关闭卡字体句柄并卸载 SD 文件系统，NVS 保存一次性 USB 请求后重启；启动先消费并提交清除请求，再初始化板级硬件并跳过 SD 文件系统探测。请求清除失败不进入 USB；工厂 VCOM 与 PIN 门禁仍优先，自检续跑时回退普通事件循环。
  The UI closes card-font handles/unmounts SD and persists a one-shot request. Boot consumes and commits its removal before bring-up, skipping SD filesystem probing. Clearing failure refuses USB; factory VCOM/PIN gates retain priority, and self-test resume returns to the ordinary loop.
- `read_pico_sd_open_raw` 只接受全新未探卡的启动，以原 CLK/CMD/D0 的 1-bit SDMMC、20 MHz 初始化；裸卡独占后拒绝探测、重挂载和格式化。MSC 初始化 owner=USB；电脑弹出/断开后，官方组件可在 `/usbcard` 内部挂 FAT，但不启普通消费者、不自动格式化。
  Raw open requires a fresh non-probing boot, uses existing 1-bit pins at 20 MHz, and refuses local probes/remount/format for the rest of the boot. MSC initially belongs to USB; the official component may mount FAT at `/usbcard` after eject/disconnect, with no ordinary consumers or automatic formatting.
- TinyUSB 读/写/弹出回调留在 USB 任务；退出的介质释放同样排入该任务，在延迟写收齐后卸载 USB、关闭 SDMMC。退出和普通启动显式归还 PHY 给 Serial/JTAG；一次性请求使异常复位不循环进入磁盘模式。依赖锁定 `esp_tinyusb 2.2.0` 和 lock 中的 TinyUSB；释放排队使用该版本的 `device/usbd_pvt.h`，升级组件需重验队列与 MSC 生命周期。
  USB callbacks and queued medium deletion run on the USB task; after deferred writes drain, uninstall USB and close SDMMC. Exit/normal boot restore the serial PHY. The one-shot request prevents USB-mode reboot loops. Dependencies are pinned; the deletion queue uses this TinyUSB version's private header, so upgrades require lifecycle review.

可复跑：`python3 tools/run_os_home_tests.py`（USB 生命周期/请求，含 ASAN/UBSAN）、`python3 tools/test_sd_media_guard.py`（裸卡独占、失败清理、无格式化）、`python3 tools/preview/verify.py --sanitize`（入口、无卡、确认取消和请求失败）。这些边界替身不验证 USB 枚举、真实 SD 读写速度、操作系统安全弹出或物理拔卡；交付固件须另做真机连接、往返文件哈希及退出后阅读/串口恢复验收。

Host substitutes verify contracts, not enumeration or actual SD/OS behavior. Hardware acceptance requires a file round-trip hash, safe eject and post-exit reading/serial recovery. Implementation follows [Espressif USB device documentation](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html).
