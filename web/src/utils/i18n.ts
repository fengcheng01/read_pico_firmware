export type Lang = 'zh' | 'en';

export interface Translation {
  brand: string;
  subBrand: string;
  tagline: string;
  description: string;
  nav: {
    features: string;
    gallery: string;
    flasher: string;
    specs: string;
    github: string;
  };
  features: {
    tag: string;
    title: string;
    subtitle: string;
    reading: {
      title: string;
      desc: string;
      bullet1: string;
      bullet2: string;
      bullet3: string;
    };
    shelf: {
      title: string;
      desc: string;
      bullet1: string;
      bullet2: string;
      bullet3: string;
    };
    journal: {
      title: string;
      desc: string;
      bullet1: string;
      bullet2: string;
      bullet3: string;
    };
    transfer: {
      title: string;
      desc: string;
      bullet1: string;
      bullet2: string;
      bullet3: string;
    };
  };
  gallery: {
    tag: string;
    title: string;
    subtitle: string;
    tabs: {
      reading: string;
      shelf: string;
      journal: string;
      calendar: string;
      quotes: string;
      home: string;
      transfer: string;
    };
  };
  flasher: {
    tag: string;
    title: string;
    subtitle: string;
    tabModeA: string;
    tabModeB: string;
    modeADesc: string;
    modeBDesc: string;
    modeAWarning: string;
    modeBWarning: string;
    modeBSelectType: string;
    modeBMerged: string;
    modeBTrio: string;
    dropText: string;
    browseText: string;
    fileSelected: string;
    selectHint: string;
    startBtn: string;
    flashingBtn: string;
    baudRate: string;
    eraseCheckbox: string;
    eraseHint: string;
    bootloaderLabel: string;
    partitionLabel: string;
    appLabel: string;
    mergedLabel: string;
    readyToFlash: string;
    confirmTitle: string;
    confirmModeAContent: string;
    confirmModeBContent: string;
    confirmBtn: string;
    cancelBtn: string;
    noticeHeader: string;
    notice1: string;
    notice2: string;
    notice3: string;
    unsupportedBrowser: string;
    consoleLogs: string;
  };
  specs: {
    tag: string;
    title: string;
    mcu: string;
    mcuVal: string;
    display: string;
    displayVal: string;
    pmu: string;
    pmuVal: string;
    storage: string;
    storageVal: string;
    battery: string;
    batteryVal: string;
    connectivity: string;
    connectivityVal: string;
  };
  footer: {
    license: string;
    disclaimer: string;
  };
}

export const translations: Record<Lang, Translation> = {
  zh: {
    brand: '小纸 Pico',
    subBrand: 'Read Pico',
    tagline: '极简 · 纯粹 · 属于手掌的 4.7" 墨水屏阅读器',
    description:
      '搭载 4.7 英寸 16 级灰阶电子墨水屏、ESP32-S3 双核主控与独立低功耗 PMU。为纯粹阅读与每日阅读手帐而生。',
    nav: {
      features: '核心功能',
      gallery: '界面欣赏',
      flasher: '在线烧录',
      specs: '硬件规格',
      github: 'GitHub 仓库',
    },
    features: {
      tag: 'CRAFTED FOR READING',
      title: '专注纯粹阅读与每日记录',
      subtitle: '剔除繁冗干扰，回归指尖墨香与专注排版',
      reading: {
        title: '沉浸正文阅读',
        desc: '支持 EPUB 图文排版与 TXT 原生解析。',
        bullet1: '16 级灰阶抗锯齿字形与物理平滑排版',
        bullet2: '极速断点续读，精准字符级进度记忆',
        bullet3: '行距、边距、字号微调与轻量章节书签',
      },
      shelf: {
        title: '双存储卡片书架',
        desc: '内置高速 Flash 与 TF 卡扩展双源书库管理。',
        bullet1: '离线拼音、声母与英文拼写即时书名检索',
        bullet2: 'EPUB 封面自动有界解码与排版回退卡片',
        bullet3: '批量管理与清晰的已读百分比标记',
      },
      journal: {
        title: '今日阅读手帐',
        desc: '阅读足迹与习惯沉淀，专属个人精神岛屿。',
        bullet1: '精准统计今日与近 7 日真实专注时长',
        bullet2: '打卡月历与阅读热度直观呈现',
        bullet3: '长按摘录佳句，生成便签与足迹卡片',
      },
      transfer: {
        title: '无线传书与 USB 电脑挂载',
        desc: '支持无线 WiFi 传书与标准 USB 电脑连接挂载 TF 卡，灵活自由。',
        bullet1: '设备自建 AP 热点扫码直传 / 局域网 STA 浏览器拖拽极速导入',
        bullet2: '设置中一键开启 USB 电脑连接模式，免拔卡直接挂载为电脑 U 盘',
        bullet3: '兼容 KOReader 的 Kosync 云端进度双向同步与 SNTP 自动校时',
      },
    },
    gallery: {
      tag: 'PIXEL PERFECT',
      title: '真机 1:1 像素级界面渲染',
      subtitle: '684 × 1216 分辨率原生渲染画面，精工细作的每一个交互页面',
      tabs: {
        reading: '正文阅读',
        shelf: '书架 (带真实书封)',
        home: '首页正在读',
        journal: '手帐 · 今日足迹',
        calendar: '手帐 · 打卡月历',
        quotes: '手帐 · 金句便签',
        transfer: '极速传书',
      },
    },
    flasher: {
      tag: 'WEB SERIAL TOOL',
      title: '固件在线升级与烧录',
      subtitle: '使用现代浏览器 Web Serial 技术，直接连线升级小纸 Pico',
      tabModeA: '模式 A：保留数据平滑升级 (官方系统更新)',
      tabModeB: '模式 B：完整重装 / 跨固件换系统 (新装/救砖推荐)',
      modeADesc: '仅刷写应用程序到 0x10000 分区。完整保留机器内的本地图书、进度记录及系统配置。',
      modeBDesc: '写入完整引导、分区表及核心系统。解决因曾刷过第三方固件导致的分区或引导时序不兼容。',
      modeAWarning: '【重要提醒】模式 A 仅适用于当前机器已处于“官方固件”的日常升级！若您的设备此前刷入过第三方固件（如 Crossmux、Freeink 或非官方分支），由于引导程序(120MHz时序)与分区表不匹配，使用模式 A 刷入将导致无法开机！跨固件更换系统请务必使用【模式 B】。',
      modeBWarning: '【跨固件安全换装】模式 B 会重写 Bootloader (0x0)、分区表 (0x8000) 与应用。可选勾选“擦除整片 Flash”以彻底清除旧固件残留。',
      modeBSelectType: '固件形式：',
      modeBMerged: '单文件全量合并包 (merged-firmware.bin @ 0x0)',
      modeBTrio: '官方三件套 (Bootloader + Partitions + App)',
      dropText: '将固件文件拖拽至此处，或',
      browseText: '点击选择文件',
      fileSelected: '已选固件',
      selectHint: '仅支持 .bin 格式固件文件',
      startBtn: '连接设备并开始烧录',
      flashingBtn: '正在烧录中…',
      baudRate: '波特率',
      eraseCheckbox: '完全抹除 Flash（清除所有历史分区与配置，跨系统推荐）',
      eraseHint: '将擦除所有数据，彻底还原至出厂纯净态',
      bootloaderLabel: '1. Bootloader 引导程序 (0x0000)',
      partitionLabel: '2. Partition Table 分区表 (0x8000)',
      appLabel: '3. Application 应用程序 (0x10000)',
      mergedLabel: '全量固件包 (0x0000)',
      readyToFlash: '分区已就绪，可开始烧录',
      confirmTitle: '烧录安全确认',
      confirmModeAContent: '您正在执行【模式 A：平滑升级】。请再次确认：您的机器目前运行的是小纸官方固件吗？如果机器正在运行第三方系统，请取消并切换到【模式 B】，否则机器将无法开机！',
      confirmModeBContent: '您正在执行【模式 B：完整重装】。此操作将写入完整的 Bootloader 与分区表。如果勾选了全片擦除，所有存储内容将被清空。是否继续？',
      confirmBtn: '我已核对，确认烧录',
      cancelBtn: '返回检查',
      noticeHeader: '烧录前注意事项：',
      notice1: '若设备此前处于“电脑 U 盘（USB Disk）模式”，必须先在电脑端安全弹出并在机器上退出该模式，释放刷机串口。',
      notice2: '建议使用 Chrome / Edge 浏览器，并通过可靠的数据线连接电脑（部分仅充电线无法传输数据）。',
      notice3: '烧录完成后设备将自动发送退出指令并复位。若屏幕未更新，请长按机身电源键手动开机。',
      unsupportedBrowser: '您的浏览器不支持 Web Serial API。请在桌面端 Chrome 或 Edge 浏览器中打开本站以使用烧录功能。',
      consoleLogs: '串口终端日志',
    },
    specs: {
      tag: 'SPECIFICATIONS',
      title: '硬件参数与架构',
      mcu: '主控芯片',
      mcuVal: 'ESP32-S3 双核 LX7 240MHz, 16MB Flash, 8MB Octal PSRAM @ 120MHz',
      display: '电子墨水屏',
      displayVal: '4.7 英寸 E-Paper 电子墨水屏 (684 × 1216, 16 级灰阶, LCD 16-bit 并口总线)',
      pmu: '电源管理协处理器',
      pmuVal: 'CW32L010 独立低功耗单片机, 专有 I2C 协议控制电源、RTC、按键与唤醒',
      storage: '存储扩展',
      storageVal: '内置 SPI Flash 存储分区 + MicroSD (TF) 卡插槽扩展',
      battery: '按键与交互',
      batteryVal: 'CST836U 触控芯片 + 机身物理三功能键 + SC7A20H 加速度计',
      connectivity: '无线通信',
      connectivityVal: 'Wi-Fi 802.11 b/g/n (AP / STA 双模传书与云端对时)',
    },
    footer: {
      license: '小纸 Pico (Read Pico) 采用 Apache-2.0 开源协议',
      disclaimer: '基于 ESP-IDF 与定制墨水屏波形驱动构建。专注提供最佳纯粹掌上阅读体验。',
    },
  },
  en: {
    brand: 'Read Pico',
    subBrand: '小纸 Pico',
    tagline: 'Minimal · Pure · 4.7" Handheld E-Paper Reader',
    description:
      'Engineered with a 4.7-inch 16-grayscale E-Paper display, ESP32-S3 dual-core MCU, and a dedicated low-power PMU. Designed purely for focused reading and personal journaling.',
    nav: {
      features: 'Features',
      gallery: 'Interface',
      flasher: 'Web Flasher',
      specs: 'Specs',
      github: 'GitHub',
    },
    features: {
      tag: 'CRAFTED FOR READING',
      title: 'Focused Reading & Daily Reflection',
      subtitle: 'Zero distractions, bringing back the pure joy of typography and ink',
      reading: {
        title: 'Immersive Reader',
        desc: 'Native EPUB & TXT engine with rich typography.',
        bullet1: '16 gray-level antialiased rendering and smooth layout',
        bullet2: 'Instant resume with exact character-level memory',
        bullet3: 'Fine adjustments for margin, leading, size and bookmarks',
      },
      shelf: {
        title: 'Dual-Storage Shelf',
        desc: 'Seamless management across internal Flash and MicroSD.',
        bullet1: 'Instant search by pinyin, initials and English titles',
        bullet2: 'Automatic EPUB cover extraction with typographic fallback',
        bullet3: 'Batch management with clear progress marks',
      },
      journal: {
        title: 'Today Journal',
        desc: 'Reading footprint, habits, and personal reflections.',
        bullet1: 'Precise tracking for daily and 7-day focus minutes',
        bullet2: 'Calendar check-ins with monthly activity heatmaps',
        bullet3: 'Long-press quote clips saved into notes & cards',
      },
      transfer: {
        title: 'Wireless Transfer & USB Disk Mount',
        desc: 'Wire-free book uploads plus USB computer connection for direct TF card mounting.',
        bullet1: 'AP hotspot QR code transfer & local LAN browser drag-and-drop',
        bullet2: 'One-tap USB connection in settings to mount TF card directly as a computer disk',
        bullet3: 'Kosync bidirectional cloud progress sync (KOReader compatible) & auto time sync',
      },
    },
    gallery: {
      tag: 'PIXEL PERFECT',
      title: '1:1 Native Screen Renders',
      subtitle: '684 × 1216 native resolution renders capturing genuine e-paper precision',
      tabs: {
        reading: 'Reading View',
        shelf: 'Book Shelf (with Covers)',
        home: 'Now Reading',
        journal: 'Journal · Today',
        calendar: 'Journal · Calendar',
        quotes: 'Journal · Quotes & Notes',
        transfer: 'WiFi Transfer',
      },
    },
    flasher: {
      tag: 'WEB SERIAL TOOL',
      title: 'Web Firmware Flasher',
      subtitle: 'Update your Read Pico directly from your browser via Web Serial',
      tabModeA: 'Mode A: Smooth Upgrade (Official Firmware Only)',
      tabModeB: 'Mode B: Full Reinstall / Cross-Firmware (Recommended for 3rd-Party OS / Unbrick)',
      modeADesc: 'Writes only the application partition to 0x10000. Books, progress and settings remain completely intact.',
      modeBDesc: 'Writes complete Bootloader, Partition Table and App. Resolves incompatibility from 3rd-party bootloader timings.',
      modeAWarning: '[CRITICAL WARNING] Mode A is ONLY for routine updates when the device is ALREADY running official Read Pico firmware! If your device is currently running 3rd-party firmware (Crossmux, Freeink, etc.), mismatched 120MHz bootloader and partitions will render the device UNBOOTABLE! Use Mode B to switch from other firmware.',
      modeBWarning: '[SAFE CROSS-FIRMWARE FLASH] Mode B rewrites Bootloader (0x0), Partitions (0x8000), and Application (0x10000). Optionally erase all flash to wipe legacy remnants.',
      modeBSelectType: 'Package Format:',
      modeBMerged: 'Single Merged Binary (merged-firmware.bin @ 0x0)',
      modeBTrio: 'Official 3-Bin Set (Bootloader + Partitions + App)',
      dropText: 'Drag and drop your firmware .bin file here, or',
      browseText: 'Browse File',
      fileSelected: 'Selected File',
      selectHint: 'Only .bin files are supported',
      startBtn: 'Connect & Flash',
      flashingBtn: 'Flashing…',
      baudRate: 'Baud Rate',
      eraseCheckbox: 'Erase entire flash memory (Recommended when switching firmware)',
      eraseHint: 'Wipes all internal data and returns to clean factory state',
      bootloaderLabel: '1. Bootloader (0x0000)',
      partitionLabel: '2. Partition Table (0x8000)',
      appLabel: '3. Application (0x10000)',
      mergedLabel: 'Merged Full Binary (0x0000)',
      readyToFlash: 'Partitions ready to flash',
      confirmTitle: 'Fashing Safety Confirmation',
      confirmModeAContent: 'You selected [Mode A: Smooth Upgrade]. Please confirm: Is your device ALREADY running official Read Pico firmware? If it is running 3rd-party firmware, cancel and switch to [Mode B] now, otherwise it will fail to boot!',
      confirmModeBContent: 'You selected [Mode B: Full Reinstall]. This writes the complete bootloader and partition table. If Erase Flash is checked, all internal data will be wiped. Proceed?',
      confirmBtn: 'I Understand, Proceed to Flash',
      cancelBtn: 'Go Back & Check',
      noticeHeader: 'Before Flashing:',
      notice1: 'If the device was in USB Computer Disk mode, safely eject on your PC and exit disk mode on device first to release the flashing port.',
      notice2: 'Use Chrome or Edge on desktop, and connect with a verified USB data cable.',
      notice3: 'The flasher sends a soft/hard reset pulse upon completion. If the screen does not update, long press the power key to reboot.',
      unsupportedBrowser: 'Your browser does not support the Web Serial API. Please use Google Chrome or Microsoft Edge on a desktop computer.',
      consoleLogs: 'Serial Terminal Logs',
    },
    specs: {
      tag: 'SPECIFICATIONS',
      title: 'Hardware & Architecture',
      mcu: 'Main MCU',
      mcuVal: 'ESP32-S3 dual-core LX7 240MHz, 16MB Flash, 8MB Octal PSRAM @ 120MHz',
      display: 'Display',
      displayVal: '4.7" E-Paper Display (684 × 1216, 16 gray levels, 16-bit parallel LCD bus)',
      pmu: 'PMU Coprocessor',
      pmuVal: 'CW32L010 low-power MCU with custom I2C protocol managing power, RTC and keys',
      storage: 'Storage',
      storageVal: 'Internal SPI Flash FAT filesystem + MicroSD (TF) card slot',
      battery: 'Input & Sensors',
      batteryVal: 'CST836U capacitive touch + 3 hardware physical keys + SC7A20H accelerometer',
      connectivity: 'Connectivity',
      connectivityVal: 'Wi-Fi 802.11 b/g/n (Dual AP/STA modes for book transfer and SNTP sync)',
    },
    footer: {
      license: 'Read Pico (小纸 Pico) is released under the Apache-2.0 License',
      disclaimer: 'Built with ESP-IDF and custom waveforms for the finest pocket reading experience.',
    },
  },
};
