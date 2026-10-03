#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""真实页面与灰度导出的集成检查。/ Integration checks for real pages and grayscale export."""
import argparse
import os
import struct
import unittest
import zlib

from build import OUT, ROOT, build, metadata
import parity
from serve import Preview, action_command, png_from_pgm


class PreviewTests(unittest.TestCase):
    def setUp(self):
        self.preview = Preview(BINARY)
        self.addCleanup(self.preview.close)
        self.indices = {item["symbol"]: i for i, item in enumerate(self.preview.pages)}


    def state(self):
        """发一条无操作命令，取回最新状态行。/ Send a no-op command and read the fresh state line."""
        return self.preview.command("nop")

    def settle(self, limit=14):
        """tick 到画面稳定（真实扫描/打开是异步的）。/ Tick until the screen settles (real scans/opens are async)."""
        last = None
        for _ in range(limit):
            line = self.preview.command("tick")
            if line == last:
                return self.state()
            last = line
        return self.state()

    def page(self, symbol):
        return self.preview.command(f"page {self.indices[symbol]}")

    def test_registry_and_menu_navigation(self):
        self.assertEqual(self.preview.pages, metadata())
        self.assertFalse(self.preview.state["menu"])
        self.assertEqual(self.preview.state["page"], self.indices["app_os_home"])
        self.preview.command("menu")
        self.assertTrue(self.preview.state["menu"])
        self.preview.command("tap 300 500")
        settled = self.settle()
        self.assertEqual(settled["page"], self.indices["app_book"])



    def test_menu_selects_real_reading_callback(self):
        self.page("app_os_settings")
        self.preview.command("tap 300 970")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_tools"])
        self.preview.command("tap 300 494")
        self.assertFalse(self.preview.state["menu"])
        self.assertEqual(self.preview.state["page"], self.indices["app_reading"])
        first = self.preview.png
        self.preview.command("tap 300 700")
        self.assertEqual(self.preview.state["leaf"], 1)
        self.assertNotEqual(first, self.preview.png)
        self.preview.command("key 2")
        self.assertTrue(self.preview.state["menu"])
        self.preview.command("key 2")
        self.assertFalse(self.preview.state["menu"])
        self.assertEqual(self.preview.state["leaf"], 1)

    def test_reading_sizes_and_refresh_requests(self):
        self.page("app_reading")
        original = self.preview.png
        self.preview.command("tap 622 88")
        self.assertNotEqual(original, self.preview.png)
        for x, mode in ((100, 1), (300, 5), (470, 2)):
            self.preview.command(f"tap {x} 1150")
            self.assertEqual(self.preview.state["refresh_mode"], mode)
        self.preview.command("key 1")
        self.assertEqual(self.preview.state["refresh_mode"], 2)

    def test_sixteen_gray_levels_and_png_lossless(self):
        self.page("app_refresh")
        self.preview.command("tap 130 1150")
        pgm = self.preview.frame.read_bytes()
        pixels = pgm[len(b"P5\n684 1216\n255\n"):]
        self.assertEqual(set(pixels), set(range(0, 256, 17)))
        png = png_from_pgm(pgm)
        self.assertEqual(png[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(struct.unpack(">II", png[16:24]), (684, 1216))
        position, compressed = 8, b""
        while position < len(png):
            length = struct.unpack(">I", png[position:position + 4])[0]
            kind = png[position + 4:position + 8]
            payload = png[position + 8:position + 8 + length]
            crc = struct.unpack(">I", png[position + 8 + length:position + 12 + length])[0]
            self.assertEqual(crc, zlib.crc32(kind + payload) & 0xffffffff)
            if kind == b"IDAT":
                compressed += payload
            position += 12 + length
        scanlines = zlib.decompress(compressed)
        self.assertEqual(len(scanlines), 685 * 1216)
        self.assertEqual(set(scanlines[::685]), {0})
        decoded = b"".join(scanlines[y * 685 + 1:(y + 1) * 685] for y in range(1216))
        self.assertEqual(decoded, pixels)

    def test_font_picker_multiple_rows_and_no_entry_flash(self):
        self.page("app_os_reading")
        self.preview.command("tap 200 732")
        self.assertEqual(self.preview.state["page"], self.indices["app_font_pick"])
        self.assertEqual(self.preview.state["refresh_mode"], 5)
        first = self.preview.png
        self.preview.command("tap 300 370")
        self.assertNotEqual(first, self.preview.png)
        self.assertEqual(self.preview.state["refresh_mode"], 5)
        selected = self.preview.png
        self.preview.command("tap 450 890")
        self.assertEqual(self.preview.state["leaf"], 1)
        self.assertNotEqual(selected, self.preview.png)
        self.preview.command("tap 100 890")
        self.assertEqual(self.preview.state["leaf"], 0)
        self.preview.command("tap 590 100")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_reading"])
        self.assertEqual(self.preview.state["refresh_mode"], 5)

    def test_static_assets_match_portrait_pixels(self):
        for which, name in enumerate(("loading", "lock")):
            self.preview.command(f"asset {which}")
            packed = (ROOT / f"main/assets/{name}_4bpp.bin").read_bytes()
            pixels = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
            expected = bytes(((value >> 4) if i % 2 else (value & 15)) * 17
                             for i in range(684 * 1216) for value in [packed[i // 2]])
            self.assertEqual(pixels, expected)
            self.assertEqual(self.preview.state["asset"], which)

    def test_inline_epub_and_single_grayscale_turn(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")
        self.settle()
        self.preview.command("tap 300 650")
        self.settle()
        self.assertTrue(self.preview.state["reading"])
        pixels = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
        image_region = [pixels[y * 684 + x] for y in range(200, 390) for x in range(240, 430)]
        self.assertGreater(sum(value < 255 for value in image_region), 10000)
        before = self.preview.state["presents"]
        first = self.preview.png
        self.preview.command("key 2")
        self.assertEqual(self.preview.state["presents"], before + 1)
        self.assertEqual(self.preview.state["refresh_mode"], 5)
        self.assertNotEqual(first, self.preview.png)

    def test_unsupported_page_remains_menu(self):
        index = next(i for i, p in enumerate(self.preview.pages) if not p["supported"])
        self.preview.command(f"page {index}")
        self.assertEqual(self.preview.state["unsupported"], index)
        self.assertTrue(self.preview.state["menu"])

    def test_input_bounds(self):
        for action in ({"action": "tap", "x": -1, "y": 0},
                       {"action": "tap", "x": 0, "y": 1216},
                       {"action": "key", "key": True},
                       {"action": "page", "page": len(self.preview.pages)},
                       {"action": "asset", "asset": 2}, {"action": "shutdown"}):
            with self.assertRaises(ValueError):
                action_command(action, self.preview.pages)
        self.assertEqual(action_command({"action": "tap", "x": 683, "y": 1215}, self.preview.pages),
                         "tap 683 1215")
        with self.assertRaises(ValueError):
            png_from_pgm(b"broken frame")

    def test_home_real_rendering_and_empty_import(self):
        self.preview.command("fixture 0")
        empty = self.preview.png
        self.preview.command("fixture 1")
        self.settle()
        loaded = self.preview.png
        self.assertNotEqual(empty, loaded)
        self.preview.command("fixture 2")
        self.settle()
        self.assertNotEqual(empty, self.preview.png)
        self.preview.command("fixture 0")
        self.settle()
        self.preview.command("tap 320 604")
        self.assertEqual(self.state()["page"], self.indices["app_transfer"])



    def test_home_resume_routes_to_actual_book_app_not_demo(self):
        for x, y in ((320, 604), (200, 820)):
            self.preview.command("fixture 1")
            self.settle()
            self.preview.command(f"tap {x} {y}")
            settled = self.settle()
            self.assertEqual(settled["page"], self.indices["app_book"])
            self.assertNotEqual(settled["page"], self.indices["app_reading"])



    def test_product_roots_settings_and_empty_today(self):
        for x, symbol in ((350, "app_os_today"), (480, "app_os_settings"), (90, "app_os_home")):
            self.preview.command(f"tap {x} 1140")
            self.assertEqual(self.preview.state["page"], self.indices[symbol])
        self.page("app_os_settings")
        self.preview.command("tap 300 270")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_reading"])

    def test_settings_children_are_product_pages(self):
        # 阅读设置：字号步进即时重排，字体列表仍进原选择器。
        # Reading settings: size steps re-render at once and the font list still opens the picker.
        self.page("app_os_settings")
        self.preview.command("tap 300 270")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_reading"])
        base = self.preview.png
        self.preview.command("tap 575 310")
        grown = self.preview.png
        self.assertNotEqual(base, grown)
        self.preview.command("tap 429 310")
        self.assertEqual(base, self.preview.png)
        self.preview.command("tap 200 732")
        self.assertEqual(self.preview.state["page"], self.indices["app_font_pick"])
        # 睡眠设置：模式选择、拿起唤醒、锁屏样式与密码入口。
        # Sleep settings: mode selection, pickup wake, lock style and the PIN entry.
        self.page("app_os_settings")
        self.preview.command("tap 300 690")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_sleep"])
        plain = self.preview.png
        self.preview.command("tap 300 290")
        light = self.preview.png
        self.assertNotEqual(plain, light)
        self.preview.command("tap 300 290")
        self.assertEqual(light, self.preview.png)
        self.preview.command("tap 300 684")
        self.assertNotEqual(light, self.preview.png)
        self.preview.command("tap 136 852")
        clock_style = self.preview.png
        self.assertNotEqual(light, clock_style)
        self.preview.command("tap 573 852")  # 黄历 / almanac style (4th pill)
        almanac_style = self.preview.png
        self.assertNotEqual(clock_style, almanac_style)
        self.preview.command("tap 136 852")  # 回静态 / back to static
        self.assertEqual(clock_style, self.preview.png)
        self.preview.command("tap 300 984")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_pin"])
        keypad = self.preview.png
        self.preview.command("tap 136 424")
        self.assertNotEqual(keypad, self.preview.png)
        self.preview.command("tap 557 102")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_sleep"])
        self.page("app_os_settings")
        # 存储状态：检测完成后展示卡与内置容量，返回回设置。
        # Storage status: probing completes into card/internal capacities; back returns to Settings.
        self.preview.command("tap 300 830")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_storage"])
        probing = self.preview.png
        self.preview.command("tick")
        self.assertNotEqual(probing, self.preview.png)
        self.preview.command("tap 557 102")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_settings"])
        self.assertNotEqual(plain, probing)

    def test_storage_usb_entry_confirmation_and_failure(self):
        self.page("app_os_storage")
        self.preview.command("tick")
        empty = self.preview.png
        self.preview.command("tap 480 920")
        self.assertNotEqual(empty, self.preview.png)
        self.preview.command("key 0")
        self.assertEqual(empty, self.preview.png)
        self.preview.command("sd 1")
        self.page("app_os_storage")
        self.preview.command("tick")
        ready = self.preview.png
        self.preview.command("tap 480 920")
        confirm = self.preview.png
        self.assertNotEqual(ready, confirm)
        self.preview.command("tap 140 920")
        self.assertEqual(ready, self.preview.png)
        self.preview.command("tap 480 920")
        self.preview.command("tap 480 920")
        self.preview.command("tick")
        self.assertEqual(self.state()["page"], self.indices["app_os_storage"])
        self.assertNotEqual(confirm, self.preview.png)

    def test_reader_typography_menu_and_night(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")
        self.settle()
        self.preview.command("tap 300 400")
        self.settle()
        self.preview.command("key 1")
        toolbar = self.preview.png
        self.preview.command("tap 136 1038")  # 排版 / typography
        typography = self.preview.png
        self.assertNotEqual(toolbar, typography)
        self.preview.command("tap 300 452")  # 行距 / leading（菜单保持打开）
        relaxed = self.preview.png
        self.assertNotEqual(typography, relaxed)
        self.preview.command("tap 300 900")  # 行辅助线 / guide rule (row5 y546-612)
        solid = self.preview.png
        self.assertNotEqual(relaxed, solid)
        self.preview.command("tap 424 1140")  # 返回阅读（底栏）/ return via the bottom bar
        reader = self.preview.png
        self.assertNotEqual(solid, reader)
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.preview.command("tap 342 224")  # 翻页与显示 / turns and display
        normal = self.preview.png
        self.preview.command("tap 300 676")  # 夜间开关 / night switch
        night = self.preview.png
        self.assertNotEqual(normal, night)
        self.preview.command("tap 300 676")
        self.assertEqual(normal, self.preview.png)



    def test_transfer_home_and_sync_view_in_preview(self):
        # 传书首页：夹具状态与卡片；进度同步入口 → 同步视图；账号键盘可编辑保存。
        # Transfer home: fixture status cards; the sync entry opens the sync view; the keyboard edits accounts.
        self.page("app_transfer")
        home = self.preview.png
        self.preview.command("tick")
        self.assertNotEqual(home, self.preview.png)
        self.preview.command("tap 320 638")
        self.assertEqual(self.preview.state["page"], self.indices["app_transfer"])
        sync = self.preview.png
        self.assertNotEqual(home, sync)
        self.preview.command("tap 300 234")
        keyboard = self.preview.png
        self.assertNotEqual(sync, keyboard)
        self.preview.command("tap 557 102")
        self.assertEqual(self.preview.state["page"], self.indices["app_transfer"])

    def test_reader_sync_and_direct_cached_home(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 300 370")
        self.assertEqual(self.state()["page"], self.indices["app_book"])
        self.assertTrue(self.state()["reading"], "Home book opens before the first shelf frame")
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.preview.command("tap 550 224")
        sync = self.preview.png
        self.preview.command("tap 300 500")
        self.assertTrue(sync != self.preview.png, "Reader settings admit sync directly")
        self.preview.command("tap 424 1140")
        self.assertTrue(self.state()["reading"])
        self.page("app_os_home")
        self.settle()
        home = self.preview.png
        self.page("app_os_today")
        self.page("app_os_home")
        self.assertTrue(home == self.preview.png, "Cached roots paint immediately without loading")

    def test_home_epub_cover_and_shelf_cache(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")
        self.settle()
        self.preview.command("tap 300 650")
        self.assertTrue(self.state()["reading"])
        self.page("app_os_home")
        self.settle()
        pixels = self.preview.frame.read_bytes().split(b"\n", 3)[3]
        cover_grays = {pixels[y * 684 + x] for y in range(238, 504) for x in range(52, 254)}
        self.assertGreater(len(cover_grays), 6, "Home uses the extracted grayscale EPUB cover")
        self.preview.command("tap 220 1140")
        self.settle()
        shelf = self.preview.png
        self.page("app_os_today")
        self.preview.command("tap 220 1140")
        self.assertTrue(shelf == self.preview.png, "Shelf is cached before the first re-entry frame")

    def test_pin_zero_clear_and_backspace_visible(self):
        self.page("app_os_pin")
        empty = self.preview.png
        self.preview.command("tap 342 770")
        one = self.preview.png
        self.assertTrue(empty != one, "Zero key is visible and active in fourth row")
        self.preview.command("tap 548 770")
        self.assertTrue(empty == self.preview.png, "Backspace clears the zero digit")
        self.preview.command("tap 342 770")
        self.preview.command("tap 140 770")
        self.assertTrue(empty == self.preview.png, "Clear key is visible and active")

    def test_reader_footer_status_and_align(self):
        # 状态栏：阅读设置开时钟/电量后页脚出现状态串；排版对齐切换即时重绘。
        # Footer status: enabling clock/battery shows the cluster; alignment steps redraw at once.
        self.page("app_os_reading")
        base = self.preview.png
        self.preview.command("tap 320 901")
        self.assertNotEqual(base, self.preview.png)
        self.preview.command("tap 320 967")
        self.assertNotEqual(base, self.preview.png)
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")
        self.settle()
        self.preview.command("tap 300 400")
        self.settle()
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        menu = self.preview.png
        self.preview.command("tap 342 224")
        menu = self.preview.png
        self.preview.command("tap 300 340")
        self.assertNotEqual(menu, self.preview.png)
        self.preview.command("tap 300 340")
        self.assertNotEqual(menu, self.preview.png)

    def test_transfer_stop_returns_to_origin(self):
        # 停止并返回：回到进入传书前的设置页；预览与设备同语义。
        # Stop-and-return: back to the Settings page that entered transfer, matching the device.
        self.page("app_os_settings")
        self.preview.command("tap 300 410")
        self.assertEqual(self.preview.state["page"], self.indices["app_transfer"])
        self.preview.command("tick")
        self.preview.command("tap 164 1140")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_settings"])

    def test_reader_swipe_turns_pages(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")
        self.settle()
        self.preview.command("tap 300 400")
        self.settle()
        first = self.preview.png
        self.preview.command("swipe 560 600 120 600")  # 左滑下一页 / swipe left
        forward = self.preview.png
        self.assertNotEqual(first, forward)
        self.preview.command("swipe 120 600 560 600")  # 右滑上一页 / swipe right
        back = self.preview.png
        self.assertNotEqual(forward, back)
        self.preview.command("tap 600 600")  # 右分区下一页 / right tap zone
        self.assertNotEqual(back, self.preview.png)
        zoned = self.preview.png
        self.preview.command("tap 100 600")  # 左分区上一页 / left tap zone
        self.assertNotEqual(zoned, self.preview.png)



    def test_reader_toc_tabs_bookmarks_and_percent(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")  # 书架 / library root
        self.settle()
        self.preview.command("tap 300 400")   # 打开第一本书 / open the first book
        self.settle()
        self.preview.command("key 1")         # 中键工具条 / middle key opens the toolbar
        self.preview.command("tap 136 944")   # 目录 / TOC
        toc = self.preview.png
        # 书签页签：空列表 → 加一枚 → 列表出现。/ Bookmark tab: empty, add one, the list appears.
        self.preview.command("tap 342 210")   # 书签页签 / bookmark tab
        empty = self.preview.png
        self.assertNotEqual(toc, empty)
        self.preview.command("tap 342 300")   # ＋ 为本页加书签 / add a bookmark here
        one = self.preview.png
        self.assertNotEqual(empty, one)
        # 跳转页签：50% 行回到正文。/ Percent tab: the 50% row returns to the reader.
        self.preview.command("tap 546 210")   # 跳转页签 / percent tab
        percent = self.preview.png
        self.assertNotEqual(one, percent)
        self.preview.command("tap 342 624")   # 跳到 50% / jump to 50%
        reader = self.preview.png
        self.assertNotEqual(percent, reader)
        # 回书签页签长按删除，列表回落为空。/ Back to bookmarks; long-press removes the entry.
        self.preview.command("key 1")
        self.preview.command("tap 136 944")
        self.preview.command("tap 342 210")
        listed = self.preview.png
        self.assertNotEqual(reader, listed)
        self.preview.command("hold 342 381")  # 长按第一条书签 / long-press the bookmark
        confirmation = self.preview.png
        self.assertNotEqual(listed, confirmation)
        self.preview.command("tap 165 660")  # 保留书签 / keep it
        self.assertEqual(listed, self.preview.png)
        self.preview.command("hold 342 381")
        self.preview.command("tap 475 660")  # 确认删除 / confirm removal
        removed = self.preview.png
        self.assertNotEqual(listed, removed)

    def test_reader_end_panel_next_book(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")  # 书架 / library root
        self.settle()
        self.preview.command("swipe 342 900 342 400")  # 书架第二页 / shelf page two
        self.settle()
        self.preview.command("tap 300 400")   # 打开《短篇》/ open the short book
        self.settle()
        body = self.preview.png
        # 一两页即读完：右分区一次到达末页出现读完面板。
        # The short book fits a page or two: one right-zone tap reaches the end panel.
        self.preview.command("tap 600 600")
        panel = self.preview.png
        self.assertNotEqual(body, panel)
        self.preview.command("tap 188 922")   # 继续停留 / stay
        stayed = self.preview.png
        self.assertNotEqual(panel, stayed)
        self.preview.command("tap 600 600")   # 末页再向前 → 面板重现 / forward again reopens it
        again = self.preview.png
        self.assertNotEqual(stayed, again)
        self.preview.command("key 0")         # 三键先收面板 / keys dismiss it first
        dismissed = self.preview.png
        self.assertNotEqual(again, dismissed)
        self.preview.command("tap 600 600")   # 重现后选下一本 / reopen and pick a next book
        reopened = self.preview.png
        self.assertNotEqual(dismissed, reopened)
        self.preview.command("tap 342 568")   # 下一本候选行 / a next-book candidate
        opened = self.preview.png
        self.assertNotEqual(reopened, opened)

    def test_today_clock_summary_and_time_settings(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 350 1140")
        self.assertEqual(self.state()["page"], self.indices["app_os_today"])
        loading = self.preview.png
        self.settle()
        self.assertEqual(loading, self.preview.png)  # 完整摘要跨根页复用。/ Reuse complete summaries across roots.
        today = self.preview.png
        self.preview.command("tap 320 760")  # 正在读行 / current-book row
        settled = self.settle()
        self.assertEqual(settled["page"], self.indices["app_book"])
        self.assertNotEqual(today, self.preview.png)
        self.preview.command("fixture 0")
        self.preview.command("tap 350 1140")
        self.assertEqual(self.state()["page"], self.indices["app_os_today"])
        # 设置 → 时间与时区：步进时区即时重排，返回回到设置。
        self.preview.command("tap 480 1140")
        self.preview.command("tap 300 550")
        self.assertEqual(self.state()["page"], self.indices["app_os_time"])
        base = self.preview.png
        self.preview.command("tap 424 740")
        self.assertNotEqual(base, self.preview.png)
        self.preview.command("tap 164 740")
        self.assertEqual(base, self.preview.png)
        self.preview.command("tap 557 102")
        self.assertEqual(self.state()["page"], self.indices["app_os_settings"])



    def test_new_shelf_reader_and_tools(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")
        self.settle()
        self.preview.command("tap 300 400")
        self.settle()
        self.assertEqual(self.state()["page"], self.indices["app_book"])
        body = self.preview.png
        self.preview.command("key 1")  # 中键 = 工具条 / middle key opens the toolbar
        toolbar = self.preview.png
        self.assertNotEqual(body, toolbar)
        self.preview.command("tap 544 944")  # 字号子面板 / size subpanel
        size_panel = self.preview.png
        self.assertNotEqual(toolbar, size_panel)
        self.preview.command("tap 544 944")  # 字号 + / size up
        grown = self.preview.png
        self.assertNotEqual(size_panel, grown)
        self.preview.command("tap 136 944")  # 返回工具 / back to tools
        tools = self.preview.png
        self.preview.command("tap 342 1038")  # 无压黑清理 / white cleanup
        self.assertEqual(self.preview.state["refresh_mode"], 5)
        self.assertEqual(tools, self.preview.png)
        night = self.preview.png
        self.preview.command("tap 544 1040")  # 书架 / shelf
        shelf = self.preview.png
        self.assertNotEqual(night, shelf)
        self.preview.command("tap 480 1140")  # 根导航设置 / root to Settings
        self.assertEqual(self.state()["page"], self.indices["app_os_settings"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    if args.no_build and args.sanitize:
        parser.error("--sanitize requires a rebuild")
    if args.sanitize:
        os.environ["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    if not args.no_build and (code := parity.main()):
        raise SystemExit(code)
    BINARY = OUT / "preview" if args.no_build else build(args.sanitize)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(PreviewTests))
    raise SystemExit(0 if result.wasSuccessful() else 1)
