# 电脑端界面预览 / Desktop UI preview

在电脑上运行真实固件页面与阅读引擎，不需要 ESP-IDF 或设备。浏览器只展示原生 C framebuffer，不另画一套产品 UI。
Run real firmware pages and the reader engine on the host without ESP-IDF or a board. The browser displays the native C framebuffer rather than a separate product UI.

## 启动 / Run

需要 Python 3.9+、GNU C11 编译器及 zlib。macOS 使用 Xcode/Command Line Tools 的编译器与 SDK，可用 `CC` 和 `PREVIEW_MACOS_SDK` 指定路径。
Requires Python 3.9+, a GNU C11 compiler and zlib. On macOS, use the Xcode/Command Line Tools compiler and SDK; override paths with `CC` and `PREVIEW_MACOS_SDK`.

```sh
python3 tools/preview/serve.py
```

打开 <http://127.0.0.1:8765>；服务只监听本机，Ctrl+C 停止。端口冲突可用 `--port 8766`。改 C 后重启构建，改 HTML 后刷新。多个页签共享一个会话。
Open <http://127.0.0.1:8765>; the server listens on loopback only, and Ctrl+C stops it. Use `--port 8766` if occupied. Restart to rebuild C changes and reload for HTML changes. Tabs share a session.

## 一致性与覆盖 / Parity and coverage

- 页面、真实 app_book、TXT/EPUB 引擎、封面、搜索、设置、进度、书签、统计与同步协议核心共用设备源码。`parity.py` 对比设备 CMake 和预览，设备边界与替身必须显式登记。
  Pages, real app_book, TXT/EPUB engines, covers, search, settings, progress, bookmarks, statistics and sync protocol share device sources. `parity.py` compares device CMake and preview sources; device boundaries and stand-ins are explicitly registered.
- `esp_host.c` 仿真 NVS、堆及 FreeRTOS 边界；`miniz_host.c` 用 zlib 适配 ZIP。`device_port.c` 提供隔离书根；build.py 生成真实 TXT/EPUB 夹具，扫描与续读走真实代码。不访问用户书库或设备 NVS。
  `esp_host.c` emulates NVS, heap and FreeRTOS boundaries; `miniz_host.c` adapts ZIP through zlib. `device_port.c` supplies isolated roots; build.py generates real TXT/EPUB fixtures and runs real scanning/resume code. User libraries and device NVS are not accessed.
- 覆盖四根导航、设置子页、书架/封面、正文/工具/排版、目录/书签/跳转、读完面板和传书/同步布局。网络、PMU、时钟和存储状态由边界夹具提供；不能据此验收真实 WiFi、服务器或 TF 卡。
  Covers four-root navigation, Settings children, shelf/covers, reader/tools/typography, TOC/bookmarks/jump, completion and transfer/sync layouts. Network, PMU, clock and storage states come from boundary fixtures and cannot validate real WiFi, servers or TF cards.

点击传递 tap，按住拖拽至少 64px 松开生成滑动；三键或键盘 `1/2/3` 对应设备三键，`M/Esc` 打开导航。原生测试命令支持 `hold x y`；浏览器单击不能验收设备长按。`tick` 推进虚拟时间，异步状态可能需要多次 tick。
Clicks send taps; dragging at least 64px and releasing generates a swipe. Buttons or `1/2/3` map to device keys, and `M/Esc` opens navigation. Native tests support `hold x y`; browser clicks do not validate device holds. `tick` advances virtual time; asynchronous states may need several ticks.

## 检查与导出 / Checks and export

```sh
python3 tools/preview/verify.py
python3 tools/preview/verify.py --sanitize
python3 tools/preview/serve.py --export build-host/desktop-preview/screenshots
```

verify 构建前执行 parity；检查导航、真实阅读流程、书签、跳转、排版、灰阶与 PNG/资源映射。`--sanitize` 使用 ASAN/UBSAN；已有构建可用 `--no-build`。程序、生成头、夹具、缓存和截图在被忽略的 `build-host/` 内，不能提交。
verify runs parity before building and checks navigation, real reading flows, bookmarks, jumps, typography, gray levels and PNG/assets. `--sanitize` uses ASAN/UBSAN; reuse a build with `--no-build`. Binaries, generated headers, fixtures, caches and snapshots live in ignored `build-host/` and must not be committed.

## 限制与许可 / Limits and licenses

显示为 684×1216、16 级灰度；缩放仅影响浏览器显示。刷新请求不执行物理波形，不模拟残影、温度、功耗、睡眠或板级实时性。宿主堆与线程仿真不等于 8 MB PSRAM；不能验收并发、耗时、掉电持久化或硬件 IO。设置仅保存在当前宿主进程中。UI/字体、状态和目录的宿主检查不能替代真机验收。
Display is 684×1216 with sixteen gray levels; browser scaling changes presentation only. Refresh requests do not execute physical waveforms or model ghosting, temperature, power, sleep or board timing. Host heap/thread emulation does not match 8 MB PSRAM and cannot validate concurrency, latency, power-loss persistence or hardware IO. Settings last within the host process. Host UI/font/state/TOC checks do not replace hardware acceptance.

适配器使用 Apache-2.0。构建时抽取的 epdiy CPU 绘图库仍遵循 LGPL-3.0-or-later，字体与资源保留原许可。分发编译程序需遵守依赖许可，见 [epdiy LICENSE](../../components/epdiy/LICENSE)。
Adapters use Apache-2.0. Extracted epdiy CPU graphics remain LGPL-3.0-or-later, and fonts/assets retain their licenses. Binary distribution must comply with dependency licenses; see [epdiy LICENSE](../../components/epdiy/LICENSE).
