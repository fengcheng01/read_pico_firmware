# 小纸 Pico 阅读固件（开发中）

**语言:** [English](./README.md) | [简体中文](./README.zh-CN.md) | [日本語](./README.ja-JP.md)

[贡献指南](CONTRIBUTING.md) · [支持](SUPPORT.md) · [安全政策](SECURITY.md) · [行为准则](CODE_OF_CONDUCT.md)

[![License](https://img.shields.io/github/license/MindReset/read_pico_firmware?style=for-the-badge&logo=apache&logoColor=white)](LICENSE)
[![Build](https://img.shields.io/github/actions/workflow/status/MindReset/read_pico_firmware/build.yml?branch=main&style=for-the-badge&logo=githubactions&logoColor=white)](https://github.com/MindReset/read_pico_firmware/actions)
![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v6.1-E7352C?style=for-the-badge&logo=espressif&logoColor=white)
![Target](https://img.shields.io/badge/target-ESP32--S3-E7352C?style=for-the-badge&logo=espressif&logoColor=white)

小纸 Pico 是深圳思维重置科技有限公司旗下小纸 Read 系列的开发板，面向墨水屏开源固件开发者，
搭载 ESP32-S3 和 4.7 寸单色墨水屏。本分支基于官方出厂演示固件进行阅读优先产品化开发。

固件为显示、触摸、加速度计、电源、按键、TF 卡、字体、睡眠与唤醒提供独立的演示页和诊断页，
用于逐项确认硬件状态，并为自有固件提供刷新策略、功耗处理和交互方式的参考实现。

板级支持、PMU 协议主机端和芯片驱动均为可独立复用的组件。目前已重构四根产品导航、书封优先首页与三行书架、正文工具和分组设置，使用真实阅读记录与单次续读入口。时钟/待办、全部设置下级 UI 和系统级恢复仍待实现，并非完整 OS 发布。

普通开机进入“正在读”，日常入口为“正在读 / 书架 / 今日 / 设置”，诊断保留在“设置 → 进阶与诊断”。首页继续阅读不重复询问；底栏书架直接进入，菜单书架仍询问。详见 [产品界面、维护边界与验证](docs/READING_HOME.md)。第二台设备暂未提供开发资料，尚无驱动或可刷写镜像。

面向 AI agent 的目录职责、`app_desc_t` 契约、术语表和注释规范见 [AGENTS.md](AGENTS.md)。

优化后工具条提供目录/书签、添加书签、字号子面板、更多设置、清除残影和书架；更多设置分为文字排版、翻页与显示两组。书签删除须确认。同步在后台运行，下载位置确认后才应用，同步时暂停上传/删除，离页取消；首页不常驻字体提示，正文缺字时提供完整字体入口。


TF 卡还支持 **USB 连接电脑**：设置 → 存储与设备 → USB 连接电脑，确认后重启进入磁盘模式。电脑复制完先安全弹出，再在设备退出重启；此时暂停阅读/传书，退出后恢复刷机串口。见 [USB 卡盘说明](docs/USB_SD.md)。

## 官方文档与更多设备

- [小纸 Pico 官方文档](https://dot.mindreset.tech/docs/read_0)
- [Dot Open Platform](https://github.com/MindReset/dot_open_platform)：探索更多可以动手玩的 Dot 设备与项目，包括 Quote/0 硬件资源和 Rand/0 本地显示集成，以及固件示例、引脚表和外壳文件。

## 硬件

| 项目 | 规格 |
| --- | --- |
| 主控 | ESP32-S3，16 MB flash，8 MB Octal PSRAM，二者均运行于 120 MHz |
| 屏幕 | 4.7 寸单色墨水屏，1216 × 684，16 级灰阶，16 bit 并口经 LCD 外设驱动 |
| 屏电源 | SY7636A，PGOOD 经 IO 扩展读回 |
| 电源管理 | CW32L010，自定义 I2C 协议：电池、充放电、指示灯、RTC、闹钟、开关机 |
| 触摸 | CST836U，两点触摸、中断与深睡唤醒 |
| 加速度计 | SC7A20H，敲击、朝向、自由落体、FIFO |
| IO 扩展 | FCA9555，屏控制脚与卡检测 |
| 存储 | TF 卡（1 bit SDMMC），字体从卡上加载 |
| 其它 | 蜂鸣器、三个电容按键区 |

## 编译与烧写

需要 ESP-IDF v6.1。`components/read_pico/read_pico_flash_hpm.c` 依赖 v6 才提供的
`esp_flash_chips/spi_flash_override.h`。

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```

`sdkconfig.defaults` 中的 120 MHz flash / PSRAM 时序依赖板上实际的 flash 型号。
CI 使用 `sdkconfig.ci` 换回默认时序，仅验证能否编译通过，见
[.github/workflows/build.yml](.github/workflows/build.yml)。

如果在固件开发或烧录时，设备从睡眠状态唤醒后无法被识别，请依次尝试：

1. 更换 USB Type-A（标准 USB）数据线。
2. 对设备重新执行一次睡眠和唤醒操作。
3. 重启开发板后重试。

面板公共电压（VCOM）在出厂时标定并写入 PMU。固件开机读取一次用于配置驱动，
不在本地保存，也不提供修改入口。

## 电脑端界面预览

需要 Python 3.9+ 和 C 编译器（macOS 可用 Xcode 或 Command Line Tools），运行：

```sh
python3 tools/preview/serve.py
```

打开 <http://127.0.0.1:8765>，默认展示新版产品首页，可点书封进入正文、翻页、调整字号、打开工具，再返回书架或设置；另有空态/异常和原测试页。不需要 ESP-IDF 或设备。产品页使用原生 C 绘图，书籍状态由宿主适配、四本展示书共用内置样文与真实分页；不访问用户书库，不验证文件/进度持久化。不是完整 ESP32-S3 模拟器，外设、物理刷新和功耗仍需真机验证。
详见 [覆盖范围、限制与检查](tools/preview/README.md)。

## 页面

唯一页面目录在 [main/app/app_registry.c](main/app/app_registry.c)。全局菜单只列四根入口，以下原功能经设置分组访问。

| 页面 | 内容 |
| --- | --- |
| 正在读 | 普通开机首页，真实已保存进度、最近三本、续读与书架/导入入口 |
| 今日 | 大字时钟与日期（PMU RTC + 时区，未校时显式提示）、今日/近 7 日阅读分钟、当前书进度；待办仍为明确空态 |
| 时间与时区 | 设置子页；时钟状态展示，15 分钟步进时区调整，传书连 WiFi 自动校时 |
| 设置 | 阅读、传书、时间、睡眠、存储与独立诊断入口 |
| 阅读设置 | 默认字号（36–72 步进 4）、正文字体入口与晃动实验开关 |
| 睡眠设置 | 锁屏后模式：浅睡/深睡/关机；拿起唤醒；锁屏样式（静态/时钟/日历/黄历）与 4 位锁屏密码 |
| 存储状态 | TF 卡与内置书库容量，只读检测与手动重检；异常重启记录摘要（内置 `crash.log`，无反栈） |
| 概览 | 开机 I2C 在线检测、识别码、电池与充电状态、构建时间 |
| 墨水屏刷新 | 整屏 GC16、局部 DU、16 灰阶与快速 8 灰阶梯图，各自附实测耗时 |
| 阅读测试 | 内置正文，DU / GL16 / GC16 翻页，页眉调字号 |
| 触摸 | 两点连续 DU 跟手，抬手整页定稿；深睡与自动唤醒 |
| 加速度计 | 实时三轴与倾角、敲击计数、朝向判定 |
| 加速度计诊断 | 采样参数与自测 |
| 电源与电池 | 电池电压、电量与充电状态；屏电源轨、温度与故障；只读的 SY7636A 配置 |
| 电源管理协议 | 协议状态、事件、配置与命令 |
| 电源按键 | `key_raw_events`、按下与抬起电平、DOWN/UP/SHORT/LONG 事件 |
| 睡眠与唤醒（诊断） | 实际执行浅睡/深睡/关机的原测试页 |
| TF 卡与蜂鸣器（诊断） | 卡容量与挂载状态、重挂、格式化、蜂鸣 |
| 字体 | 从阅读设置进入；每页六行内置/TF 卡 TTF，当前标记与正文预览 |
| 扩展口 | Port-0 电平与中断，底栏脉冲触摸复位 |
| 设备功能自检 | 探活与命令 ACK；需要断电的项目只在后台记录结果 |
| 阅读排版 | 工具条“更多设置”菜单：字体、行距、页边距、首行缩进、段落间距、行辅助线（每行实线/虚线）、对齐（左/居中/两端）、点击翻页、自动翻页、清残影周期（关/3/5/10/14/20/30）；夜间模式反色正文 |
| 书签与跳转 | 目录页三页签：章节直达、书签（每书至多 16 条，点按跳转/确认删除，随删书清理）与 10%–100% 百分比跳转 |
| 读完推荐 | 末页继续向前弹出面板，推荐至多三本同来源图书（按最近阅读优先） |
| 阅读状态栏 | 页脚时钟/电量百分比/阅读百分比可配置，不显示刻度线；今日页常显电量 |
| 空闲锁屏 | 关/5/10/30 分钟无操作自动锁屏；自动翻页期间不锁 |
| 图书 | TF 卡或内置存储的 UTF-8 / GBK TXT 与 EPUB，目录、字号与逐书进度；滑动翻页、长按正文进目录、长按书架条目查看详情、清进度或确认删除；书架支持来源筛选、名称/最近阅读排序及拼音/首字母/英文搜索；单本用弹窗管理，管理页支持批量选择、清进度/删除与重扫；实验晃动翻页默认关。EPUB 支持 NCX 与 nav 目录。 |
| 阅读同步 | 传书页内：kosync 协议（兼容 KOReader），手动/自动上传、手动下载最后一本书的进度 |
| 传书 | 设备热点或已有 WiFi，浏览器上传 TXT/EPUB，TF 卡支持上传完整 TTF 字体；热点可扫码连接；已有 WiFi 可在触屏扫描选网并输入密码，也保留网页配网。优先 TF 卡，内置存储单文件 ≤ 1 MB；网页可列出和搜索当前上传目标中的图书，确认替换/删除，取消上传和重试；设备可确认遗忘已保存网络。离页断网。 |

图书阅读三键 KEY1 / KEY2 / KEY3 对应上一页 / 工具条 / 下一页；工具条提供强刷。长按中键约500 ms可打开演示菜单。其他页面仍为 KEY2 整屏 GC16、KEY3 菜单。菜单项抬起提交，滑出可取消。

## 目录

```text
main/
  app_main.c        开机装配，随后交给 app_loop
  app/              app 接口（app.h）、注册表、事件循环
  apps/             每个演示页一个文件，只导出 app_desc_t
  ui/               ui_kit 绘制原语与布局常量、ui_menu 两层菜单
  font/             stb_truetype 字形缓存
  factory/          设备功能自检与出厂 VCOM 标定
components/
  read_pico/        板级 BSP：I2C、EPD 板定义与扫描时序、TF 卡、蜂鸣器、flash HPM
  read_pico_pmu/    CW32L010 协议主机端
  epdiy/            墨水屏渲染，裁剪至 LCD 外设路径
  continuous_du/    连续 DU：跨多轮累积相位，用于跟手
  cst836u/ sc7a20h/ fca9555/ sy7636a/    芯片驱动
  e0470_epaper_waveform/                 面板波形表与裁剪函数
  pwm_audio/        LEDC PWM 音频，蜂鸣器底层之一
assets/             图片素材（main/assets/*.bin 的来源）
tools/              字体与图片转换脚本
```

新增演示页：在 `main/apps/` 新建文件，实现 `app_desc_t` 中需要的回调，
再加入 `main/app/app_registry.c` 的菜单表。主循环不需要改动。

## 引脚

| 功能 | GPIO |
| --- | --- |
| I2C SCL / SDA | 40 / 39（400 kHz） |
| EPD 数据 D0–D15 | 4–18, 45 |
| EPD XLE / XSTL / XCL / SPV / CKV | 3 / 46 / 21 / 47 / 48 |
| FCA9555 INT# | 41（同时作为浅睡唤醒源） |
| CST836U INT# | 43 |
| SC7A20H INT1 | 1 |
| TF 卡 CLK / CMD / D0 | 38 / 42 / 44 |
| 蜂鸣器 | 2 |

屏电源开关、XOE、MODE、VCOM_EN、触摸复位和卡检测位于 FCA9555 的 Port-0，
见 [main/apps/app_ioe.c](main/apps/app_ioe.c) 中的引脚表。

日常阅读和设置使用完整 48 相 GL16 灰阶直刷，保留抗锯齿；正文与页脚一次刷新，设置切页和字体切换不再强制黑闪。清残影周期统一计数，默认每 14 次普通刷新一次 GC16；设为“关”可持续直刷，累积残影可用“清除残影”处理。主页和导航字号已放大，字体列表每页显示六条。实际残影与速度需真机确认。

## 传书与使用限制

AP 与已有 WiFi 均提供传书网页二维码；热点页可切换连接 WiFi 与打开网页二维码。设备显示的构建时间统一标注 UTC。

传书服务仅在传书页运行，离页停止。它使用局域网 HTTP，没有独立登录或 TLS；同网设备可管理当前上传存储中的图书，请使用可信网络。保存的 WiFi 凭据位于设备 NVS，公开接口和日志不返回密码。本版未启用 NVS/flash 加密，不以此提供物理访问防护。

停止传书后返回进入前的页面或菜单位置。检测到已挂载TF卡失效时，设备停止相关阅读/传书并回退字体；插回后需在TF页显式重新挂载。写入时拔卡可能损坏文件系统。EPUB 开章时自动加载本地 JPEG/PNG，与正文一起分页；当前章节最多自动尝试 64 幅图，总灰度位图预算 768 KiB，按图片数等比缩小。加载失败或超出范围时保留可点击占位，单独预览后返回原位置；翻页和绘制不重复解码；ZIP条目、资源清单项和章节各最多32768个，OPF/导航元数据解压后最多4 MiB，正文/图片读取最多2 MiB，ZIP中央目录最多8 MiB；标题总预算1 MiB，超出后用编号标题；即使未使用的ZIP条目也不能超过4 MiB。以上内存与资源限制独立于章节数，并非章数合规就一定能打开；不支持ZIP64或超过32768项/章的文件；外部换书或换卡产生同路径、同大小文件时，旧阅读进度可能仍被匹配。保存失败的待重试状态不能保证在断电后保留。

进入书架时询问是否继续上次阅读，选择“留在书架”不会打开图书；确认“继续阅读”才加载（KEY1取消、KEY3继续）。先解析目录与当前章节，再完成起始两页或续读位置；其余分页每次补两页，其他章节按需加载。分页未完成时页数显示“…”；向前跨章到上一章末页仍需完成该章分页。字宽测量使用驻留字体表，不提前生成整章字形位图。打开图书和加载章节时保留等待提示；失败显示可见原因（内存不足、超出限制、格式不支持或文件异常）。

滑动翻页在抬手时提交，距离阈值由120像素缩短到64像素；超过24像素轻点容差但不足64像素的短拖仍取消。在图片占位上滑动仍翻页，不触发图片加载。

插图范围：基线 JPEG 优先在解码时缩小，允许最多放大两倍到显示尺寸以减少计算，原图最多16M像素、单边8192；PNG及渐进 JPEG 原图最多1M像素，解码堆上限4 MiB。输出不超过648×1000灰度像素，透明 PNG 合成白底；当前章的内嵌图片随章释放；独立放大预览另外缓存最近一幅图片，同图关闭后再次打开可复用；超限、缺失、损坏或不支持的图片在点击后说明原因，不中断正文阅读。同一归一化资源路径再次出现时显示“重复图片”和本次打开图书后已读章节中的最早节号（EPUB目录顺序包含封面、前言，可能不同于正文章号）。章首连续图片后紧接标题时，这些图片及已识别的同资源引用单独显示“标题图”，不重复显示首次位置；标题后的其他插图仍保留已读最早提示；不扫描未读章节，不声称是全书首次位置，关闭图书后记录清空；不同路径下的相同内容不作匹配；支持 SVG 包装中的 JPEG/PNG 引用，不绘制纯 SVG 矢量、CSS背景或外链图片。解码器来源与许可见 [第三方说明](main/book/vendor/README.md)。

内置字体包含 GB2312 的 6763 个常用汉字、标点和界面字符，无卡也可显示常用中文书名与正文。生僻字、更多繁体字或其他语言请将完整中文 TTF 放到 TF 卡的 `fonts/` 或 `assets/fonts/`，再在“字体”页选用。默认字体路径为 `fonts/ChillDuanSansVF.ttf`；缺字时应检查卡上字体文件和所选字体的字形覆盖。仓库提供 [TF 部署字体与版权说明](sdcard/README.md)。也可通过 WiFi 网页的“上传字体”区上传到 `/sdcard/fonts`：单文件上限 32 MiB，仅支持带 TrueType 轮廓的 TTF（不支持 OTF/CFF、TTC、WOFF）。上传前检查容量，同名替换需确认，校验失败或中断保留旧字体；停止传书后在设备“字体”页选用。传书期间暂用内置字体，防止替换正在读取的字库；不会改变已保存的字体选择。

功能变化见 [版本变更](docs/CHANGELOG.md)。离线拼音字表来自 pypinyin（MIT），见 [组件许可与再生成说明](components/read_pico_search/README.md)。

## 致谢与许可

- 固件本体：Apache-2.0，见 [LICENSE](LICENSE)。
- [epdiy](https://github.com/vroland/epdiy)：墨水屏时序与渲染。本仓库为按本板
  LCD 路径裁剪的 fork，LGPL-3.0-or-later，改动清单见
  [components/epdiy/LICENSE](components/epdiy/LICENSE)。
- [stb_truetype](https://github.com/nothings/stb)：字形光栅化，公共领域。
- [pwm_audio](https://github.com/espressif/esp-iot-solution/tree/master/components/audio/pwm_audio)：
  Espressif LEDC PWM 音频。本仓库为裁剪副本，Apache-2.0，见
  [components/pwm_audio/LICENSE](components/pwm_audio/LICENSE)。
- 面板波形表随本板附带，按现状提供，Apache-2.0，见
  [components/e0470_epaper_waveform/LICENSE](components/e0470_epaper_waveform/LICENSE)。
- 内置字体 `main/assets/builtin.ttf` 由 `tools/gen_builtin_font.py` 从
  [ChillDuanSans](https://github.com/Warren2060/ChillDuanSans)（寒蝉端黑体，
  Warren2060，SIL OFL-1.1）可变字体子集化生成。完整原版字体和许可证随 `sdcard/fonts/` 提供；内置修改版名为 Read Pico UI。两者均适用 OFL-1.1，详见 [部署说明](sdcard/README.md)。

感谢各位开发者的耐心与支持。

深圳思维重置科技有限公司
