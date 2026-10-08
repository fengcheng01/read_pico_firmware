# Read Pico Reading Firmware (In Development)

**Languages:** [English](./README.md) | [简体中文](./README.zh-CN.md) | [日本語](./README.ja-JP.md)

[Contributing](CONTRIBUTING.md) · [Support](SUPPORT.md) · [Security](SECURITY.md) · [Code of Conduct](CODE_OF_CONDUCT.md)

[![License](https://img.shields.io/github/license/MindReset/read_pico_firmware?style=for-the-badge&logo=apache&logoColor=white)](LICENSE)
[![Build](https://img.shields.io/github/actions/workflow/status/MindReset/read_pico_firmware/build.yml?branch=main&style=for-the-badge&logo=githubactions&logoColor=white)](https://github.com/MindReset/read_pico_firmware/actions)
![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v6.1-E7352C?style=for-the-badge&logo=espressif&logoColor=white)
![Target](https://img.shields.io/badge/target-ESP32--S3-E7352C?style=for-the-badge&logo=espressif&logoColor=white)

Read Pico is an ESP32-S3 development board in the Read series with a 4.7" monochrome e-paper panel,
made by Shenzhen MindReset Technology Co., Ltd. for developers building open-source
e-paper firmware. This branch develops a reading-first product on the official factory demo firmware.

The firmware provides separate demo and diagnostic pages for the display, touch,
accelerometer, power, keys, TF card, fonts, sleep and wake. Use them to check the
hardware and as reference implementations for your own firmware.

The board support, PMU protocol host and chip drivers are independent components
that can be reused. The product UI now has four roots, a cover-first home and three-card shelf, compact reading tools and grouped Settings, backed by real saved history and explicit resume.
RTC/todos, restyling every Settings child and system-wide recovery remain incomplete; this is not a complete OS release.

Normal boot opens Now reading; daily roots are Now reading / Library / Today / Settings, with original tests under Settings → Diagnostics.
Explicit home resume skips a second prompt, root-bar and menu Library enter the shelf directly, saving the open reader before leaving.
See [product UI, maintenance boundaries and verification](docs/READING_HOME.md).
No second-device driver or flashable image is supplied without developer documentation.

Agent-facing layout, `app_desc_t` contract, glossary and comment style are in
[AGENTS.md](AGENTS.md).

The refined toolbar exposes TOC/bookmarks, Add Bookmark, a size subpanel, More Settings, Clean and Library. Settings group typography, turns/display and progress sync. Bookmark deletion requires confirmation. Sync runs in a worker, confirms pulled positions before applying, excludes uploads/deletions during sync and cancels on exit. Reader Clean closes tools/settings before cleaning the body; Home avoids a permanent font notice; the reader offers a full-font entry only when glyphs are missing.


TF also supports **USB computer connection**: Settings → Storage → USB connection. Confirm to reboot into disk mode; safely eject on the computer before exiting/restarting the device. Reading/transfer pause, and the flashing serial port returns after exit. See [USB disk usage](docs/USB_SD.md).


Home/Today reuse complete summaries and Library retains its catalog/page until books or progress change. Tap a home book to open directly; EPUB uses its extracted cover with filename fallback for TXT/coverless books. Saved WiFi reconnects without password entry and is the default on Transfer entry. Reader tools → More Settings → Progress Sync connects automatically for testing/push/pull; pulls require confirmation and foreign positions use approximate percentages. Generic-page/night-turn cleanup defaults to every 5 updates; upgrades retain selected intervals (choose 5 under turns/display if desired). Cleaning covers the whole panel with complete GC16 and the product scan clock is 12 MHz. Wake transitions directly to centered PIN entry; enabled boxes show checkmarks.

Reader tools → More Settings → Turns/Display selects standard gray (default) with antialiasing, or flicker-free direct using vendor black/white DU and binary edges. Standard day reconditions unchanged black/gray strokes; ordinary night retains selective holds to avoid flashing the black background light. Night darkens the entire reading frame, including margins, footer, tools and reading image previews; TOC/settings and other pages keep their palette. Night uses the saved cleanup interval: 0 disables it, otherwise both profiles share successful turns. On turn N, night manual Clean and actual returns to the night reading layout, the panel physically clears white before one vendor GC16 commits the retained reading target. Day turns omit this schedule and retain their existing cleanup path. Night cleanup is slower and can flash light/dark several times during the physical clear; it does not guarantee zero ghosts. An independent touch queue retains complete taps through scans.

Library Search and its keyboard omit pressed-state scans. Complete taps update only the input field in binary direct; continuous typing stays on this local path. After two seconds without input following presentation, the input settles to gray locally; failed settling retries after two seconds. Cancel preserves the applied query; Apply performs matching once. The 64-character limit and pinyin/initials/English matching remain unchanged.

With guides enabled, text-only chapters share a fixed row grid for body text, headings and paragraph gaps so old rules avoid new text during turns. Direct uses black guides by day and white guides at night. Toggling guides repaginates at the saved text position; solid/dashed changes only drawing. Unguided and illustrated layouts retain their behavior. The user confirmed guide placement and direct speed on 0.5.20 hardware; night cleanliness and the selected cleanup interval still need device comparison, without claims of whiter backgrounds or eliminated ghosts. See the [refresh contract and test commands](docs/READING_HOME.md).

Footer titles show only chapters. Network-time measurements learn and persist a sleep-only clock rate. Sync after upgrading, remain booted through lock sleep and obtain a second time sample; see the Time page for automatic maintenance. Long-run accuracy still requires device comparison.

Version 0.5.15 adds enabled-by-default lock automatic time sync on the Time page. Sync after upgrading, remain booted through about an hour of lock sleep to learn the measured rate automatically, then maintain time every six hours. Sessions use saved WiFi only, last at most 30 seconds, retry failures after an hour and cancel on unlock; the switch can disable them. Time displays measured ppm/save status and offline use retains the rate; long-run accuracy needs hardware testing. EPUB KOReader sync prefers real chapter/paragraph paths with a paragraph-location notice. TXT/unsupported structures retain rp1, unresolved positions use percentage fallback, and pulls require confirmation.

Actual app/menu layout changes, reader-view changes and return from displayed reader overlays use one cleanup operation. Returning to night reading physically clears white before its GC16 target update and may flash several times; day retains one GC16 from the actual prior frame. Ordinary redraws, controls and loading ticks do not repeat boundary cleaning; night turns separately follow the configured interval. Home Upload Progress sends saved progress. Reader sync shows actions/results, with account configuration in Transfer. EPUB KOReader positions retain leaf text nodes and Unicode character offsets; unsupported structures show approximate fallback and pulls require confirmation. Actual cross-device precision needs comparison with the same book.

## Documentation & More Devices

- [Official Read Pico documentation](https://dot.mindreset.tech/docs/read_0)
- [Dot Open Platform](https://github.com/MindReset/dot_open_platform): explore other Dot devices and projects, including Quote/0 hardware resources and Rand/0 local display integration, with firmware examples, pin maps, and enclosure files.

## Hardware

| Item | Specification |
| --- | --- |
| MCU | ESP32-S3, 16 MB flash and 8 MB Octal PSRAM, both at 120 MHz |
| Display | 4.7" monochrome e-paper, 1216 × 684, 16 gray levels, 16-bit parallel interface driven by the LCD peripheral |
| EPD power | SY7636A; PGOOD read through the IO expander |
| PMU | CW32L010 with a custom I2C protocol for battery, charging/discharging, indicator LED, RTC, alarms and power control |
| Touch | CST836U, two touch points, interrupt and deep-sleep wake |
| Accelerometer | SC7A20H, tap, orientation, free-fall and FIFO |
| IO expander | FCA9555, EPD control pins and card detection |
| Storage | TF card over 1-bit SDMMC; fonts loaded from the card |
| Other | Buzzer and three capacitive key zones |

## Build & Flash

Requires ESP-IDF v6.1. `components/read_pico/read_pico_flash_hpm.c` depends on
`esp_flash_chips/spi_flash_override.h`, which is available only in v6.

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```

The 120 MHz flash / PSRAM timing in `sdkconfig.defaults` depends on the flash part
installed on the board. CI uses `sdkconfig.ci` with default timing only to check
compilation; see [.github/workflows/build.yml](.github/workflows/build.yml).

If the device is not recognized after waking from sleep during development or
flashing:

1. Try a USB Type-A data cable.
2. Put the device to sleep and wake it again.
3. Reboot the board and retry.

The panel VCOM is calibrated at the factory and stored in the PMU. The firmware
reads it once at boot to configure the driver; it is neither stored locally nor
user-editable.

## Desktop UI Preview

With Python 3.9+ and a C compiler (Xcode or Command Line Tools on macOS), run:

```sh
python3 tools/preview/serve.py
```

Open <http://127.0.0.1:8765> for the redesigned home→shelf→reading/size/tools→return flow, Settings, empty/error states and original tests. No ESP-IDF or board is required. Product screens use native C drawing; host book state supplies four fixture titles sharing one bundled sample and real pagination, without user files or persistent progress. This is not a complete ESP32-S3 emulator; file lifecycle, peripherals, physical refresh and power still need device tests. See [coverage, limits and checks](tools/preview/README.md).

## Pages

The single catalog lives in [main/app/app_registry.c](main/app/app_registry.c).
Global navigation lists only four roots; existing features below are grouped under Settings.

| Page | Content |
| --- | --- |
| Now reading | Normal boot home, real saved progress, three recent books, resume and shelf/import actions |
| Today | Large clock and date (PMU RTC + timezone, explicit when uncalibrated), today/7-day reading minutes, current-book progress; todos stay explicitly unavailable |
| Time & timezone | Settings subpage; clock state, 15-minute timezone steps, direct sync over saved WiFi without leaving the page; auto SNTP during STA transfer |
| Settings | Reading, transfer, time, sleep, storage and separate diagnostic entries |
| Reading settings | Default size (36–72 in steps of 4), body-font entry and the shake experiment switch |
| Sleep settings | Post-lock mode: light/deep/off; pickup wake; static/clock/calendar/almanac face and 4-digit PIN. Dynamic faces use minute-timed light sleep even under the deep setting; off still powers down |
| Storage status | TF and internal library capacities, read-only probing with manual recheck; an abnormal-reset summary from the internal crash.log (no backtrace) |
| Overview | Boot I2C scan, IDs, battery and charging status, build time |
| EPD Refresh | Full-screen GC16, partial DU, 16-gray and fast 8-gray ladders, each with measured refresh time |
| Reading | Built-in text, DU / GL16 / GC16 page turns, font size adjustment in the header |
| Touch | Two-point tracking with continuous DU, full-page settle on lift, deep sleep and automatic wake |
| Accel | Live three-axis readings and tilt, tap count, orientation |
| IMU Lab | Sampling parameters and self-test |
| Power | Battery voltage, level and charging status; EPD rails, temperature, faults and read-only SY7636A configuration |
| PMU | Protocol status, events, configuration and commands |
| Power Key | `key_raw_events`, press/release levels and DOWN/UP/SHORT/LONG events |
| Sleep (diagnostic) | The original page that actually enters light/deep/off |
| Storage (diagnostic) | TF card capacity and mount status, remount, format and buzzer |
| Font | Six-row built-in/card TTF list with selection marks and a text preview |
| IOE | Port-0 levels and interrupts, touch-reset pulse from the bottom bar |
| Device | Device probes and command ACKs; power-cut items are recorded in the backend only |
| Books | UTF-8 / GBK TXT and EPUB from TF or internal storage, chapters, font size and per-book progress; swipe to turn, hold text for TOC and hold a shelf row for details, progress reset or confirmed deletion; filter storage sources, sort by name or recent reading, and search by pinyin, initials or English; single-book actions use a popup, while management supports batch selection, progress reset/deletion and rescan; experimental shake-to-turn defaults off. EPUB navigation supports NCX and nav documents. |
| Reader typography | The toolbar More Settings menu: font, leading, margins, first-line indent, paragraph gap, the per-line guide rule (solid/dashed), alignment (left/center/justified), tap zones, auto page turn and the ghost-cleanup period (off/3/5/10/14/20/30); Night inverts the complete reading frame |
| Bookmarks & jumping | TOC tabs for chapters, bookmarks (16 per book, tap to jump, long-press to confirm deletion, cleaned with deletion) and 10%–100% percent jumps |
| End-of-book picks | Paging past the last page suggests up to three same-source books (recency first) |
| Reader status bar | Footer clock/battery percent/reading percentage toggles, without ruler ticks; Today always shows battery |
| Idle lock | Auto-lock after off/5/10/30 idle minutes; auto page turns defer it |
| Progress sync | Transfer and reader More Settings → Progress Sync: kosync (KOReader-compatible), on-demand saved-WiFi connection, manual push/pull and transfer auto-push |
| Transfer | Device hotspot or existing WiFi, with browser TXT/EPUB upload and complete TTF font uploads to TF; TF card preferred, internal storage limited to 1 MB per file. Scan the hotspot QR to join, or select a 2.4 GHz network and enter its password on the touchscreen. Web provisioning remains available. The browser lists and searches books in the current upload destination, confirms replacement or deletion, and supports upload cancellation and retry. Saved WiFi can be forgotten on the device. Leaving the page stops networking. |

In Books, KEY1 / KEY2 / KEY3 select previous page / toolbar / next page. Toolbar/settings Clean first closes overlays, then fully cleans the body. Hold KEY2 for 500 ms to open the demo menu. Other pages retain KEY2 full GC16 refresh and KEY3 menu. Menu rows select on release; slide away to cancel.

## Repository Layout

```text
main/
  app_main.c        Boot wiring, then app_loop
  app/              App interface (app.h), registry and event loop
  apps/             One demo per file, exporting only app_desc_t
  ui/               ui_kit drawing primitives and layout constants; ui_menu two-level menu
  font/             stb_truetype glyph cache
  factory/          Device self-test and factory VCOM calibration
components/
  read_pico/        Board BSP: I2C, EPD definition/timing, TF card, buzzer, flash HPM
  read_pico_pmu/    CW32L010 protocol host
  epdiy/            E-paper renderer, trimmed to the LCD peripheral path
  continuous_du/    Continuous DU with phases accumulated across scans for finger tracking
  cst836u/ sc7a20h/ fca9555/ sy7636a/    Chip drivers
  e0470_epaper_waveform/                 Panel waveform tables and trimming functions
  pwm_audio/        LEDC PWM audio, one buzzer backend
assets/             Image sources for main/assets/*.bin
tools/              Font and image conversion scripts
```

To add a demo page, create a file in `main/apps/`, implement the `app_desc_t`
callbacks you need and add it to the menu table in `main/app/app_registry.c`.
The main loop stays untouched.

## Pinout

| Function | GPIO |
| --- | --- |
| I2C SCL / SDA | 40 / 39 (400 kHz) |
| EPD data D0–D15 | 4–18, 45 |
| EPD XLE / XSTL / XCL / SPV / CKV | 3 / 46 / 21 / 47 / 48 |
| FCA9555 INT# | 41 (also a light-sleep wake source) |
| CST836U INT# | 43 |
| SC7A20H INT1 | 1 |
| TF card CLK / CMD / D0 | 38 / 42 / 44 |
| Buzzer | 2 |

EPD power enable, XOE, MODE, VCOM_EN, touch reset and card detection are on FCA9555
Port-0; see the pin table in [main/apps/app_ioe.c](main/apps/app_ioe.c).

Boot/static-lock images inflate then rotate into scan memory to avoid boot stripes. Standard day turns use all 48 vendor GL16 phases and black/gray diagonals plus one neutral phase; night uses the same changed paths with unchanged holds. Black/white direct retains all 20 vendor DU phases plus one neutral. Ordinary root/menu/Journal redraws retain vendor GL49; actual layout boundaries clean once, with a physical white clear before the retained night target uses complete GC16. White holds during ordinary GL redraws without historical glyph erasure, and actual baselines commit only on success. Board power sequencing owns MODE/XOE without extra scan-end MODE switching. Day turns never trigger scheduled GC16; night standard/direct share the saved successful-turn interval. Failed turns do not count, and successful layout/manual/recovery GC16 resets it. Local controls/minute bands do not consume whole-screen cleanup counts; ordinary updates power off immediately. Recent reading has pagination and totals for every valid record. Home Upload Progress sends saved progress; pulls remain confirmed in sync settings. Hold a body sentence and confirm an excerpt; Journal Notes page through the latest 16 real excerpts and jump to their source positions, with bookmarks retained in TOC. Light sleep uses divided fast RC and preserves its time anchor on wake. Sync once after upgrading; long-term drift and panel results need device testing. The two-row reader footer updates independently on minute changes.

## Transfer and limitations

AP and existing WiFi modes provide a QR code for the upload page. AP can switch between joining WiFi and opening the page. The device labels its build timestamp as UTC.

The transfer server runs only on its page and stops on exit. It uses local-network HTTP without separate login or TLS; peers on that network can manage books in the active upload storage, so use a trusted network. Saved WiFi credentials reside in device NVS and are not returned by public endpoints or logs. NVS/flash encryption is not enabled, so this does not provide physical-access protection.

Stopping transfer returns to the entry page or menu position. When a mounted TF card becomes unavailable, affected reading/transfer stops and fonts fall back; explicitly remount from the TF page after reinserting it. Removing a card during writes can damage the filesystem. EPUB chapter loading decodes local JPEG/PNG illustrations for inline pagination with text. It automatically attempts up to 64 images per chapter, sharing a 768 KiB grayscale budget and scaling proportionally. Failed or over-limit images retain tappable placeholders with a separate preview that returns to the same position. Page turns and drawing do not decode again. ZIP entries, manifest items and spine chapters are each limited to 32768; OPF/navigation metadata is limited to 4 MiB uncompressed and chapter/image reads to 2 MiB. The ZIP central directory has an 8 MiB limit; title storage has a 1 MiB budget, with numbered fallback titles beyond it. Entries above 4 MiB are rejected even when unused. These are independent memory/resource limits, so chapter count alone does not guarantee acceptance; ZIP64 and books over 32768 entries or chapters are unsupported. Externally replaced files or another card with the same path and file size may still match old reading progress. Pending retries after failed saves are not guaranteed to survive power loss.

Entry without an explicit shelf/open request offers to resume the previous book; staying on the shelf does not open it (KEY1 cancels, KEY3 resumes). The reader parses metadata and the current chapter, then paginates the first two pages or through the saved position. Remaining pagination advances two pages at a time; other chapters load on demand. The page total shows “…” while incomplete; entering the previous chapter at its last page still requires paginating that chapter. Width measurement uses resident font tables without rasterizing a whole chapter. Opening and chapter loading retain waiting hints and visible failure reasons (memory, limits, unsupported format or invalid file).

Swipe page turns commit on release at 64 pixels (previously 120); movement beyond the 24-pixel tap tolerance but below the swipe threshold cancels, and an image-placeholder swipe turns the page without loading the image.

Illustration limits: baseline JPEG favors decode-time scaling with at most 2x enlargement to the display size, up to 16M source pixels and 8192 per side; PNG and progressive JPEG allow up to 1M source pixels and a 4 MiB decoder heap. Output fits 648x1000 grayscale pixels; transparent PNG uses a white background. Inline images are freed with their chapter; the separate enlarged preview additionally caches the most recently viewed image; closing a preview and reopening that same image reuses it. Missing, corrupt, oversized or unsupported images show an explanation on request and leave text readable. Repeated references to the same normalized EPUB resource path show “重复图片” and the earliest numbered section among chapters visited during this book opening (EPUB section order includes covers and front matter, so it can differ from printed chapter numbers). Opening image blocks immediately followed by a heading, and recognized references to those same resources, use a separate “标题图” label without the first-location line; other illustrations keep the visited-origin hint. This is not a scan of unread chapters or a claim about the first occurrence in the whole book; the history resets when the book closes. Identical content under different resource paths is not matched. JPEG/PNG references inside SVG wrappers work; pure SVG vectors, CSS backgrounds and remote images do not. See [decoder sources and licenses](main/book/vendor/README.md).

The built-in font includes all 6763 GB2312 Chinese characters, punctuation and UI text, so common Chinese titles and text work without a card. For rare characters, additional traditional Chinese or other languages, place a complete Chinese TTF in `fonts/` or `assets/fonts/` on the TF card and select it on the Fonts page. The default path is `fonts/ChillDuanSansVF.ttf`; for missing characters, check that the file exists and the selected font covers them. A complete font and its copyright notices are provided in the [TF deployment package](sdcard/README.md). The webpage also accepts fonts into `/sdcard/fonts`: up to 32 MiB per TTF with TrueType outlines, excluding OTF/CFF, TTC and WOFF. Capacity is checked first, replacements require confirmation, and interrupted or invalid uploads retain the old file. Stop transfer, then select the font on the device. Transfer temporarily uses the built-in font to avoid replacing an open font; the saved selection is retained.

See [Changelog](docs/CHANGELOG.md) for feature changes. Offline pinyin data comes from pypinyin under MIT; see the [component license and regeneration notes](components/read_pico_search/README.md).

## Acknowledgments & License

- Firmware: Apache-2.0, see [LICENSE](LICENSE).
- [epdiy](https://github.com/vroland/epdiy): e-paper timing and rendering. This is a
  trimmed fork for the board's LCD path, licensed under LGPL-3.0-or-later.
  Local changes are listed in [components/epdiy/LICENSE](components/epdiy/LICENSE).
- [stb_truetype](https://github.com/nothings/stb): glyph rasterization, public domain.
- [pwm_audio](https://github.com/espressif/esp-iot-solution/tree/master/components/audio/pwm_audio):
  Espressif LEDC PWM audio, trimmed copy, Apache-2.0. See
  [components/pwm_audio/LICENSE](components/pwm_audio/LICENSE).
- Panel waveform tables ship with the board as-is under Apache-2.0. See
  [components/e0470_epaper_waveform/LICENSE](components/e0470_epaper_waveform/LICENSE).
- The built-in font, `main/assets/builtin.ttf`, is a subset of the
  [ChillDuanSans](https://github.com/Warren2060/ChillDuanSans) variable font by
  Warren2060, generated by `tools/gen_builtin_font.py`. The unmodified full font
  and original license are bundled in `sdcard/fonts/`; the modified UI subset
  is named Read Pico UI. Both remain under SIL OFL-1.1. See [deployment notes](sdcard/README.md).

Thank you for your patience and support.

Shenzhen MindReset Technology Co., Ltd.
