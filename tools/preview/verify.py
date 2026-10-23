#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""真实页面与灰度导出的集成检查。/ Integration checks for real pages and grayscale export."""
import argparse
import io
import os
import struct
import unittest
import zipfile
import zlib

from build import OUT, ROOT, build, metadata
import parity
from serve import Preview, action_command, png_from_pgm


class PreviewTests(unittest.TestCase):
    def test_soft_animation_three_choices_persist_and_restore_original(self):
        self.open_fixture_reader()
        self.reader_display_settings()
        self.preview.command("tap 180 340")
        self.preview.command("tap 260 224")
        self.preview.command("tap 300 564")
        off=self.preview.png
        self.preview.command("tap 300 452")
        choices=self.preview.png
        self.preview.command("tap 390 1000")
        soft=self.preview.png
        self.assertNotEqual(off,soft)
        self.assertNotEqual(choices,soft)
        self.preview.command("tap 424 1140")
        self.preview.command("tap 600 500")
        self.reader_display_settings()
        self.preview.command("tap 260 224")
        self.preview.command("tap 300 564")
        self.assertEqual(soft,self.preview.png,"Soft comparison is retained after reading/navigation")
        self.preview.command("tap 300 452")
        self.preview.command("style_fail 1")
        self.preview.command("tap 250 1000")
        self.assertNotEqual(soft,self.preview.png,"Failed save is visible, not silently reported as enabled")
        self.preview.command("tap 300 452")
        self.preview.command("tap 250 1000")
        original=self.preview.png
        self.assertNotEqual(original,soft)
        self.preview.command("tap 300 452")
        self.preview.command("tap 110 1000")
        self.assertEqual(off,self.preview.png)

    def test_battery_history_entry_tabs_gaps_and_return(self):
        self.preview.command("history_fixture")
        self.page("app_os_storage")
        state=self.preview.command("tap 330 1020")
        self.assertEqual(state["page"],self.indices["app_os_battery"])
        pixels=self.reader_pixels()
        self.assertTrue(any(v==0 for y in range(490,600) for v in pixels[y*684+95:y*684+255]))
        self.assertFalse(any(v==0 for y in range(490,600) for v in pixels[y*684+265:y*684+310]),"Sleep gap is not interpolated")
        day=self.preview.png
        self.preview.command("tap 490 380")
        self.assertNotEqual(day,self.preview.png)
        self.preview.command("tap 550 100")
        self.assertEqual(self.state()["page"],self.indices["app_os_storage"])
        self.assertTrue((self.preview.frame.parent/"history/battery-0.bin").exists())

    def test_reading_archive_daily_weekly_paging_and_card_removal(self):
        self.preview.command("history_fixture")
        self.page("app_os_today")
        self.preview.command("tap 550 100")
        self.assertEqual(self.state()["page"],self.indices["app_os_history"])
        day=self.preview.png
        self.preview.command("tap 180 1030")
        self.assertNotEqual(day,self.preview.png)
        self.preview.command("tap 490 1030")
        self.assertEqual(day,self.preview.png)
        self.preview.command("tap 490 240")
        self.assertNotEqual(day,self.preview.png)
        self.assertTrue((self.preview.frame.parent/"history/reading/2026.bin").exists())
        week=self.preview.png
        self.preview.command("history_media 0")
        self.assertNotEqual(week,self.preview.png)
        self.preview.command("tap 550 100")
        self.assertEqual(self.state()["page"],self.indices["app_os_today"])

    def test_history_uncalibrated_and_empty_states(self):
        self.preview.command("time_valid 0")
        self.page("app_os_battery")
        self.preview.command("tap 490 380")
        self.page("app_os_history")
        first=self.preview.png
        self.preview.command("tap 180 1030")
        self.assertEqual(first,self.preview.png,"Uncalibrated histories never paginate invented dates")

    def test_home_battery_fill_proportional_and_header_bounded(self):
        self.preview.command("fixture 0")
        self.settle()
        reference = None
        for charge in (-1, 0, 10, 150, 500, 860, 1000):
            self.preview.command(f"battery {charge}")
            pixels = self.reader_pixels()
            filled = (64 * (charge // 10) + 50) // 100 if charge >= 10 else 0
            # 真实倒置竖屏输出：填充顶沿无字形，像素数精确跟随电量。
            # Actual inverted-portrait output: the fill's top row has no glyph ink and follows charge exactly.
            for x in range(571, 635):
                self.assertEqual(pixels[9 * 684 + x], 0 if x < 571 + filled else 255)
            if reference is None:
                reference = pixels
            else:
                self.assertEqual(reference[:6 * 684], pixels[:6 * 684])
                self.assertEqual(reference[34 * 684:], pixels[34 * 684:])
                for y in range(6, 34):
                    self.assertEqual(reference[y * 684:y * 684 + 568], pixels[y * 684:y * 684 + 568])

    def connect_bluetooth_fixture(self):
        self.page("app_os_bluetooth")
        self.preview.command("tap 540 238")
        self.assertTrue(self.state()["ble_enabled"])
        self.preview.command("tick")
        self.preview.command("tap 150 614")
        scan = self.preview.png
        self.preview.command("tap 530 948")
        self.assertNotEqual(scan, self.preview.png)
        self.preview.command("tap 130 948")
        self.preview.command("tap 200 378")
        self.assertFalse(self.state()["ble_saved"])
        self.preview.command("tap 300 710")
        self.preview.command("tick")
        self.assertTrue(self.state()["ble_saved"])
        self.assertEqual(self.state()["ble_phase"], 6)

    def test_bluetooth_selection_learning_and_forget(self):
        self.connect_bluetooth_fixture()
        ready = self.preview.png
        self.preview.command("tap 170 722")
        self.assertEqual(self.state()["ble_learning"], -1)
        self.preview.command("ble_key 4")
        self.preview.command("tick")
        self.assertEqual(self.state()["ble_learning"], 0)
        self.assertNotEqual(ready, self.preview.png)
        self.preview.command("tap 485 926")
        confirm = self.preview.png
        self.assertTrue(self.state()["ble_saved"])
        self.preview.command("tap 160 1030")
        self.assertNotEqual(confirm, self.preview.png)
        self.assertTrue(self.state()["ble_saved"])
        self.preview.command("tap 485 926")
        self.preview.command("tap 485 1030")
        self.assertFalse(self.state()["ble_saved"])

    def test_bluetooth_body_turns_overlay_and_menu_discard(self):
        self.connect_bluetooth_fixture()
        self.open_excerpt_fixture()
        self.preview.command("tick")
        self.preview.command("tick")
        body = self.preview.png
        self.preview.command("ble_key 78")
        self.preview.command("tick")
        turned = self.preview.png
        self.assertNotEqual(body, turned)
        self.preview.command("ble_key 78")
        self.preview.command("tick")
        self.assertEqual(turned, self.preview.png, "Held HID reports never synthesize repeats")
        self.preview.command("key 1")
        overlay = self.preview.png
        self.preview.command("ble_key 0")
        self.preview.command("ble_key 78")
        self.preview.command("tick")
        self.assertEqual(overlay, self.preview.png)
        self.preview.command("key 1")
        body = self.preview.png
        self.preview.command("menu")
        self.preview.command("ble_key 0")
        self.preview.command("ble_key 78")
        self.preview.command("tap 612 1150")
        self.preview.command("tick")
        self.assertEqual(body, self.preview.png, "Menu input must never replay after closing")
        self.page("app_os_settings")
        self.assertEqual(self.state()["ble_phase"], 6, "Connected radio survives navigation; non-reader keys stay gated")

    def test_reader_bluetooth_entry_returns_to_same_body_with_connection(self):
        self.open_excerpt_fixture()
        original = self.preview.png
        self.preview.command("key 1")
        self.preview.command("tap 140 1140")
        self.assertEqual(self.state()["page"], self.indices["app_os_bluetooth"])
        self.preview.command("tap 540 238")
        self.preview.command("tick")
        self.preview.command("tap 150 614")
        self.preview.command("tap 200 378")
        self.preview.command("tap 300 710")
        self.preview.command("tick")
        self.assertEqual(self.state()["ble_phase"], 6)
        self.preview.command("tap 580 120")
        self.settle(32)
        self.assertTrue(self.state()["reading"])
        self.assertEqual(self.state()["ble_phase"], 6)
        self.assertEqual(original, self.preview.png, "Bluetooth return restores the same body pixels")
        self.preview.command("ble_key 78")
        self.preview.command("tick")
        self.assertNotEqual(original, self.preview.png, "The connected remote turns body text without another settings visit")

    def test_bluetooth_learned_lock_uses_reader_request(self):
        self.connect_bluetooth_fixture()
        self.preview.command("tap 170 830")
        self.assertEqual(self.state()["ble_learning"], 2)
        self.preview.command("ble_key 78")
        self.preview.command("tick")
        self.assertFalse(self.state()["ble_bound_lock"], "A standard turn key cannot become lock")
        self.preview.command("ble_key 0")
        self.preview.command("ble_key 4")
        self.preview.command("tick")
        self.assertTrue(self.state()["ble_bound_lock"])
        self.assertEqual(self.state()["lock_requests"], 0)
        self.open_excerpt_fixture()
        self.preview.command("tick")
        self.preview.command("tick")
        self.preview.command("ble_key 0")
        self.preview.command("ble_key 4")
        self.preview.command("tick")
        self.assertEqual(self.state()["lock_requests"], 1)
        self.assertEqual(self.state()["ble_phase"], 0, "Preview boundary records lock and powers BLE off")

    def test_bluetooth_start_stop_and_forget_failures(self):
        self.connect_bluetooth_fixture()
        self.preview.command("ble_fail 3")
        self.preview.command("tap 485 926")
        self.preview.command("tap 485 1030")
        self.assertTrue(self.state()["ble_saved"])
        self.preview.command("ble_fail 2")
        self.preview.command("tap 540 238")
        self.assertTrue(self.state()["ble_enabled"], "Failed stop must retain enabled preference")
        self.preview.command("ble_fail 0")
        self.preview.command("tap 540 238")
        self.assertFalse(self.state()["ble_enabled"])
        self.preview.command("ble_fail 1")
        self.preview.command("tap 540 238")
        self.assertEqual(self.state()["ble_phase"], 7)
        failed = self.preview.png
        self.preview.command("tick")
        self.assertEqual(failed, self.preview.png)

    def open_excerpt_fixture(self):
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        self.assertTrue(self.state()["reading"])

    def test_excerpt_cancel_retry_and_key_meanings(self):
        self.open_excerpt_fixture()
        body = self.preview.png
        strip = lambda: self.reader_pixels()[-684*24:]
        body_hints = strip()
        self.assertEqual(set(body_hints), {255}, "Body does not keep permanent key hints")
        body_pixels = self.reader_pixels()[:684*1096]
        self.preview.command("hold 180 420")
        confirmation = self.preview.png
        self.assertNotEqual(body_hints, strip())
        self.preview.command("key 0")
        self.assertEqual(self.state()["quote_count"], 0)
        self.assertEqual(body_pixels, self.reader_pixels()[:684*1096])
        self.assertEqual(body_hints, strip())
        body = self.preview.png
        self.preview.command("hold 180 420")
        self.preview.command("quote_fail 1")
        self.preview.command("key 2")
        self.assertEqual(self.state()["quote_count"], 0)
        self.assertNotEqual(confirmation, self.preview.png)
        self.preview.command("key 2")
        self.assertEqual(self.state()["quote_count"], 1)
        self.assertEqual(body, self.preview.png)
        self.preview.command("key 1")
        self.assertEqual(body_hints, strip())
        self.preview.command("key 1")
        self.assertEqual(body, self.preview.png)
        for key in (2, 0):
            self.preview.command(f"key {key}")
            self.settle()
            self.assertEqual(body_hints, strip(), "Prepared page turns retain the current key labels")
        self.preview.command("hold 180 420")
        self.preview.command("key_hold 1")
        self.assertTrue(self.state()["menu"])
        self.assertEqual(self.state()["quote_count"], 1)

    def test_personal_excerpt_selection_preview_cancel_and_pin(self):
        self.preview.command("lock_quote")
        empty = self.preview.png
        self.preview.command("asset 1")
        self.assertEqual(empty,self.preview.png)
        self.page("app_os_sleep")
        self.preview.command("tap 300 880")
        self.assertEqual(self.state()["lock_style"],0)
        self.preview.command("key 0")
        self.preview.command("quote_fixture 9")
        self.preview.command("tap 300 880")
        first_page = self.preview.png
        self.preview.command("tap 450 1140")
        self.assertNotEqual(first_page,self.preview.png)
        self.preview.command("tap 300 334")
        preview = self.preview.png
        self.assertEqual(self.state()["lock_seq"],0)
        self.preview.command("tap 180 64")
        self.preview.command("key 0")
        self.assertEqual(self.state()["lock_style"],0,"Browsing/cancel must not enable the style")
        self.preview.command("tap 300 880")
        self.preview.command("tap 450 1140")
        self.preview.command("tap 300 334")
        self.preview.command("tap 500 64")
        self.assertEqual(self.state()["lock_seq"],5,"Choose a non-newest record from page two")
        self.assertEqual(self.state()["lock_style"],5)
        self.assertEqual(self.state()["lock_mode"],0)
        self.preview.command("lock_quote")
        self.assertNotEqual(empty,self.preview.png)
        self.page("app_os_settings")
        self.page("app_os_sleep")
        self.assertEqual(self.state()["lock_seq"],5)
        self.preview.command("quote_fixture 32")
        self.assertEqual(self.state()["quote_count"],16)
        self.assertEqual(self.state()["lock_seq"],5,"Rolling eviction must retain the pinned snapshot")
        self.preview.command("time_valid 0")
        self.preview.command("lock_quote")
        self.assertNotEqual(empty,self.preview.png,"Uncalibrated time still shows the selected quote")
        self.assertEqual(self.state()["lock_seq"],5)

    def test_personal_excerpt_failed_replacement_keeps_selection(self):
        self.preview.command("quote_fixture 5")
        self.page("app_os_sleep")
        self.preview.command("tap 300 880")
        self.preview.command("tap 300 334")
        self.preview.command("tap 500 64")
        self.assertEqual(self.state()["lock_seq"],5)
        self.preview.command("tap 300 880")
        self.preview.command("tap 300 530")
        preview=self.preview.png
        self.preview.command("quote_fail 1")
        self.preview.command("tap 500 64")
        self.assertNotEqual(preview,self.preview.png)
        self.assertEqual(self.state()["lock_seq"],5)
        self.preview.command("tap 500 64")
        self.assertEqual(self.state()["lock_seq"],4)
        self.preview.command("tap 300 880")
        self.preview.command("tap 550 212")
        self.preview.command("tap 180 64")
        self.preview.command("key 0")
        self.assertEqual(self.state()["lock_seq"],4)

    def test_personal_excerpt_style_failure_keeps_actual_activation(self):
        self.preview.command("quote_fixture 2")
        self.page("app_os_sleep")
        self.preview.command("tap 300 880")
        self.preview.command("tap 300 334")
        self.preview.command("style_fail 1")
        self.preview.command("tap 500 64")
        self.assertEqual(self.state()["lock_seq"],2)
        self.assertEqual(self.state()["lock_style"],0,"Saved content must not falsely report style activation")
        self.preview.command("tap 180 64")
        self.preview.command("key 0")
        self.page("app_os_settings")
        self.page("app_os_sleep")
        self.assertEqual(self.state()["lock_style"],0)
        self.preview.command("tap 300 880")
        self.preview.command("tap 550 212")
        self.preview.command("tap 500 64")
        self.assertEqual(self.state()["lock_style"],5)
        self.assertEqual(self.state()["lock_seq"],2)

    def test_home_syncs_without_opening_book(self):
        self.preview.command("fixture 1")
        self.settle()
        starts = self.state()["sync_starts"]
        before = self.preview.png
        self.preview.command("tap 584 425")
        self.assertEqual(self.state()["page"], self.indices["app_os_home"])
        self.assertFalse(self.state()["reading"])
        self.assertNotEqual(before, self.preview.png)
        self.preview.command("tick")
        self.assertEqual(self.state()["sync_starts"], starts + 1)
        self.assertEqual(self.state()["sync_job"], 4)  # OS_SYNC_JOB_MERGE
        self.settle()
        self.assertEqual(self.state()["page"], self.indices["app_os_home"])
        self.assertEqual(self.state()["sync_starts"], starts + 1)
        self.preview.command("tap 400 425")
        self.settle()
        self.assertTrue(self.state()["reading"])

    def test_reader_menu_library_is_shelf_without_resume(self):
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        self.assertTrue(self.state()["reading"])
        self.preview.command("tap 650 1150")
        self.assertTrue(self.state()["menu"])
        self.assertEqual(self.state()["refresh_mode"], 2)
        self.preview.command("key 1")  # 手动清理仍为 GC16。/ Manual cleaning retains GC16.
        self.assertEqual(self.state()["refresh_mode"], 2)
        before = self.state()["presents"]
        self.preview.command("tap 300 450")
        self.assertEqual(self.state()["page"], self.indices["app_book"])
        self.assertFalse(self.state()["menu"])
        self.assertFalse(self.state()["reading"])
        self.assertEqual(self.state()["presents"], before + 1)
        self.assertEqual(self.state()["refresh_mode"], 2)
        self.settle()
        self.assertFalse(self.state()["reading"])
        # 明确书架入口不弹续读，可直接打开第一行。/ Explicit shelf entry skips resume so the first row opens directly.
        self.preview.command("tap 300 400")
        self.settle()
        self.assertTrue(self.state()["reading"])

    def test_home_pages_all_six_reading_records(self):
        self.preview.command("fixture 3")
        self.settle()
        self.assertEqual(self.state()["history_count"], 6)
        first = self.preview.png
        self.preview.command("tap 570 1048")
        self.settle()
        self.assertEqual(self.state()["history_page"], 1)
        self.assertNotEqual(first, self.preview.png)
        self.preview.command("tap 110 1048")
        self.settle()
        self.assertEqual(self.state()["history_page"], 0)
        self.assertEqual(first, self.preview.png)

    def test_body_sentence_excerpt_and_source_jump(self):
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        first = self.preview.png
        self.preview.command("hold 180 420")
        self.assertTrue(self.state()["reading"])
        self.assertNotEqual(first, self.preview.png)
        self.preview.command("tap 490 974")
        self.assertEqual(self.state()["quote_count"], 1)
        self.preview.command("key 2")
        self.preview.command("key 2")
        self.preview.command("key 2")
        self.settle()
        later = self.preview.png
        self.page("app_os_today")
        self.settle()
        self.preview.command("tap 534 301")
        self.preview.command("tap 300 410")
        self.settle()
        self.assertTrue(self.state()["reading"])
        self.assertNotEqual(later, self.preview.png)
        self.assertEqual(first, self.preview.png)

    def test_reader_minute_updates_only_footer(self):
        self.page("app_os_reading")
        self.preview.command("tap 300 900")
        self.preview.command("tap 300 966")
        self.page("app_os_home")
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        before = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
        presents = self.state()["presents"]
        self.preview.command("time_step 60")
        after = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
        changed = [i for i, (a, b) in enumerate(zip(before, after)) if a != b]
        self.assertTrue(changed)
        self.assertTrue(all(1096 <= i // 684 < 1192 and 40 <= i % 684 < 548 for i in changed))
        self.assertEqual(self.state()["presents"], presents + 1)
        self.assertEqual(self.state()["refresh_mode"], 5)
        self.preview.command("tick")
        self.assertEqual(self.state()["presents"], presents + 1)

    def test_home_sync_cancel_and_hit_boundaries(self):
        self.preview.command("fixture 1")
        self.settle()
        starts = self.state()["sync_starts"]
        self.preview.command("swipe 584 425 400 425")
        self.assertEqual(self.state()["page"], self.indices["app_os_home"])
        self.assertEqual(self.state()["sync_starts"], starts)
        self.preview.command("tap 627 461")
        self.preview.command("tap 584 425")
        self.preview.command("tick")
        self.assertEqual(self.state()["sync_starts"], starts + 1)
        self.preview.command("tap 584 425")
        self.preview.command("tap 480 1140")
        self.preview.command("tick")
        self.assertEqual(self.state()["page"], self.indices["app_os_settings"])
        self.assertEqual(self.state()["sync_starts"], starts + 2)

    def test_home_sync_progress_accepts_edges_without_stealing_resume(self):
        # 边缘容差内轻点及小幅移动仍同步；继续阅读与卡片封面仍开书。
        # Taps and small movement within edge tolerance sync; resume and the cover still open the book.
        for command in ("tap 466 386", "tap 634 484", "tap 490 470", "swipe 476 400 467 394"):
            self.preview.command("fixture 1")
            self.settle()
            starts = self.state()["sync_starts"]
            self.preview.command(command)
            self.assertEqual(self.state()["page"], self.indices["app_os_home"])
            self.assertFalse(self.state()["reading"])
            self.preview.command("tick")
            self.assertEqual(self.state()["sync_starts"], starts + 1)
            self.assertEqual(self.state()["sync_job"], 4)
        for x, y in ((425, 440), (200, 380)):
            self.preview.command("fixture 1")
            self.settle()
            starts = self.state()["sync_starts"]
            self.preview.command(f"tap {x} {y}")
            self.settle()
            self.assertTrue(self.state()["reading"])
            self.assertEqual(self.state()["sync_starts"], starts)

    def test_lock_clock_glyphs_and_minute_band(self):
        # 编译真实锁屏绘制与字体：10:12→10:13 的所有变化必须在实际刷新区域中。
        # Compile real lock painting/fonts: every 10:12-to-10:13 change must fit the actual update band.
        import datetime
        import re
        utc = int(datetime.datetime(2026, 10, 4, 2, 12, tzinfo=datetime.timezone.utc).timestamp())
        self.preview.command("glyph 44")
        small = self.preview.png
        self.preview.command(f"clock {utc}")
        before = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
        self.preview.command(f"clock {utc + 60}")
        after = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
        source = (ROOT / "main/sleep.c").read_text()
        match = re.search(r"style == 1\) return \(EpdRect\)\{0, (\d+), UI_LOCK_WIDTH, (\d+)\}", source)
        top, height = map(int, match.groups())
        changed = [i for i, (a, b) in enumerate(zip(before, after)) if a != b]
        self.assertTrue(changed)
        self.assertTrue(all(top <= i // 684 < top + height for i in changed))
        ink = [i for i, gray in enumerate(after) if gray < 128 and 260 <= i // 684 < 640]
        self.assertGreater(max(i // 684 for i in ink) - min(i // 684 for i in ink), 150)
        self.assertGreater(max(i % 684 for i in ink) - min(i % 684 for i in ink), 350)
        # 44与300不能共用截断后的缓存键；多字号触发淘汰后大字仍一致。
        # Sizes 44 and 300 must never share a truncated cache key; eviction must retain identical large glyphs.
        for px in range(12, 321):
            self.preview.command(f"glyph {px}")
        self.preview.command("glyph 44")
        self.assertEqual(small, self.preview.png)
        for minute in range(1, 120):
            self.preview.command(f"clock {utc + minute * 60}")
        self.preview.command(f"clock {utc + 60}")
        self.assertEqual(after, self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):])

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

    def reader_display_settings(self):
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.preview.command("tap 420 224")

    def reader_pixels(self):
        frame = self.preview.frame.read_bytes()
        header = b"P5\n684 1216\n255\n"
        self.assertTrue(frame.startswith(header))
        self.assertEqual(len(frame) - len(header), 684 * 1216)
        return frame[len(header):]

    def open_fixture_reader(self):
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        self.assertTrue(self.state()["reading"])
        # 先保存首帧进度，使主题往返比较不混入首次保存的页脚变化。
        # Save the initial progress before comparing themes so the first footer save cannot affect the comparison.
        self.reader_display_settings()
        self.preview.command("tap 424 1140")

    def assert_reader_dark_margins(self):
        pixels = self.reader_pixels()
        self.assertEqual(set(pixels[:684 * 24]), {0})
        self.assertEqual(set(pixels[684 * 1192:]), {0})
        self.assertEqual(set(pixels[::684]), {0})
        self.assertEqual(set(pixels[683::684]), {0})

    def open_fixture_search(self):
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 220 1140")
        self.settle()
        before = self.state()
        self.preview.command("tap 420 208")
        self.assertEqual(self.state()["presents"], before["presents"] + 1)
        self.assertEqual(self.state()["gc_presents"], before["gc_presents"] + 1)

    def test_search_fast_input_and_idle_settle(self):
        self.open_fixture_search()
        empty = self.reader_pixels()
        before = self.state()
        for x in (67, 128, 189, 250, 311, 372, 433):
            self.preview.command(f"tap {x} 432")
            state = self.state()
            self.assertEqual(state["presents"], before["presents"] + 1)
            self.assertEqual(state["refresh_wave"], 6)
            self.assertEqual(state["refresh_area"], [40, 190, 604, 168])
            self.assertEqual(state["gc_presents"], before["gc_presents"])
            self.assertEqual(state["physical_clears"], before["physical_clears"])
            self.assertEqual(state["body_presents"], before["body_presents"])
            before = state
        quick = self.reader_pixels()
        field = [y * 684 + x for y in range(190, 358) for x in range(40, 644)]
        changed = [i for i, (a, b) in enumerate(zip(empty, quick)) if a != b]
        self.assertTrue(changed)
        self.assertTrue(all(190 <= i // 684 < 358 and 40 <= i % 684 < 644 for i in changed))
        self.assertEqual({quick[i] for i in field}, {0, 255})
        self.preview.command("time_step 1")
        self.assertEqual(self.state()["presents"], before["presents"])
        self.preview.command("time_step 1")
        gray = self.reader_pixels()
        self.assertEqual(self.state()["presents"], before["presents"] + 1)
        self.assertEqual(self.state()["refresh_wave"], 0)
        self.assertEqual(self.state()["refresh_area"], [40, 190, 604, 168])
        self.assertGreater(len({gray[i] for i in field}), 2)
        self.assertEqual(quick, bytes((0 if value < 128 else 255) if
            190 <= i // 684 < 358 and 40 <= i % 684 < 644 else value for i, value in enumerate(gray)))
        settled = self.state()["presents"]
        self.preview.command("time_step 3")
        self.assertEqual(self.state()["presents"], settled)
        # 已经过一次定稿后，新的输入仍完整等待两秒，不能继承旧采样时间。
        # A new edit after settling still gets a full idle interval instead of inheriting an old sampled timestamp.
        self.preview.command("tap 494 432")
        self.assertEqual(self.state()["presents"], settled + 1)
        self.assertEqual(self.state()["refresh_wave"], 6)
        self.preview.command("tick")
        self.assertEqual(self.state()["presents"], settled + 1)

    def test_search_limits_noops_and_cancel_apply(self):
        self.open_fixture_search()
        empty = self.reader_pixels()
        before = self.state()["presents"]
        for _ in range(64):
            self.preview.command("tap 67 432")
        self.assertEqual(self.state()["presents"], before + 64)
        self.assertEqual(self.state()["refresh_wave"], 6)
        full = self.reader_pixels()
        self.preview.command("tap 128 432")
        self.assertEqual(self.state()["presents"], before + 64)
        self.assertEqual(self.reader_pixels(), full)
        for _ in range(64):
            self.preview.command("tap 342 848")
        before = self.state()["presents"]
        self.preview.command("tap 342 848")
        self.preview.command("tap 544 848")
        self.assertEqual(self.state()["presents"], before)
        self.preview.command("time_step 2")
        self.assertEqual(self.reader_pixels(), empty)
        self.preview.command("tap 67 432")
        self.preview.command("tap 136 848")
        self.preview.command("tap 544 848")
        self.preview.command("time_step 2")
        self.assertEqual(self.reader_pixels(), empty)
        self.preview.command("tap 67 432")
        before = self.state()
        self.preview.command("tap 180 1140")
        self.assertEqual(self.state()["presents"], before["presents"] + 1)
        self.assertEqual(self.state()["gc_presents"], before["gc_presents"] + 1)
        self.preview.command("tap 420 208")
        self.assertEqual(self.reader_pixels(), empty)
        before = self.state()
        self.preview.command("tap 424 1140")
        self.assertEqual(self.state()["presents"], before["presents"] + 1)
        self.assertEqual(self.state()["gc_presents"], before["gc_presents"] + 1)

    def test_search_cancelled_gestures_do_not_type_or_scan(self):
        self.open_fixture_search()
        empty = self.reader_pixels()
        before = self.state()["presents"]
        self.preview.command("hold 67 432")
        self.preview.command("swipe 67 432 97 432")
        self.preview.command("swipe 67 432 128 432")
        self.assertEqual(self.reader_pixels(), empty)
        self.assertEqual(self.state()["presents"], before)

    def test_reader_day_size_controls_keep_body_count_and_refresh_profile(self):
        self.open_fixture_reader()
        self.preview.command("key 1")
        self.preview.command("tap 544 944")
        state = self.state()
        for _ in range(12):
            self.preview.command("tap 544 944")
            self.preview.command("tap 342 944")
            self.preview.command("time_step 2")
            self.assertEqual(self.state()["refresh_wave"], 7)
            self.assertEqual(self.state()["body_presents"], state["body_presents"])
            self.assertEqual(self.state()["gc_presents"], state["gc_presents"])
            self.assertEqual(self.state()["night_turns"], 0)

    def test_reader_image_placeholder_preview(self):
        # 已解码内嵌图直接绘制；缺失资源保留可点击占位，进入真实预览错误页。
        # Decoded images draw inline; a missing resource keeps a tappable placeholder leading to the real preview error page.
        path = OUT / "fx/f/封面之书.epub"
        original = path.read_bytes()
        self.addCleanup(path.write_bytes, original)
        with zipfile.ZipFile(io.BytesIO(original)) as source, zipfile.ZipFile(path, "w") as target:
            for info in source.infolist():
                data = source.read(info)
                if info.filename == "OEBPS/c1.xhtml":
                    data = data.replace(b'src="cover.png"', b'src="missing.png"')
                target.writestr(info, data)
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")
        self.settle()
        self.preview.command("tap 300 650")
        self.settle()
        # 先完成夹具后台分页，避免页脚总数变化混入反色和预览返回比较。
        # Complete fixture background pagination so changing footer totals cannot affect theme or preview-return comparisons.
        for _ in range(64):
            self.preview.command("tick")
        self.reader_display_settings()
        self.preview.command("tap 424 1140")
        day_body = self.reader_pixels()
        self.preview.command("tap 330 300")
        day_image = self.reader_pixels()
        self.assertNotEqual(day_image, day_body)
        self.preview.command("tap 300 1140")
        self.assertEqual(self.reader_pixels(), day_body)

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

    def test_font_picker_cleans_only_entries_and_keeps_row_updates_gray(self):
        self.page("app_os_reading")
        cleans = self.state()["gc_presents"]
        self.preview.command("tap 200 732")
        self.assertEqual(self.preview.state["page"], self.indices["app_font_pick"])
        self.assertEqual(self.preview.state["refresh_mode"], 2)
        self.assertEqual(self.state()["gc_presents"], cleans + 1)
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
        self.assertEqual(self.state()["gc_presents"], cleans + 1)
        self.preview.command("tap 590 100")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_reading"])
        self.assertEqual(self.preview.state["refresh_mode"], 2)
        self.assertEqual(self.state()["gc_presents"], cleans + 2)
        self.preview.command("tick")
        self.assertEqual(self.state()["gc_presents"], cleans + 2)

    def test_packed_static_assets_match_portrait_pixels(self):
        # 资源0是开机图，资源1是默认图片锁屏；两者按产品要求共用同一插画。
        # Asset0 is boot and asset1 is the default image lock; both intentionally share one artwork.
        for which, name in enumerate(("loading", "loading")):
            self.preview.command(f"asset {which}")
            packed = (ROOT / f"main/assets/{name}_4bpp.bin").read_bytes()
            pixels = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
            expected = bytes(((value >> 4) if i % 2 else (value & 15)) * 17
                             for i in range(684 * 1216) for value in [packed[i // 2]])
            self.assertEqual(pixels, expected)
            self.assertEqual(self.preview.state["asset"], which)

    def test_inline_epub_and_fast_gl16_turn(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")
        self.settle()
        self.preview.command("tap 300 650")
        self.settle()
        self.assertTrue(self.preview.state["reading"])
        pixels = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
        image_region = [pixels[y * 684 + x] for y in range(200, 390) for x in range(240, 430)]
        self.assertGreater(sum(value < 255 for value in image_region), 10000)
        # 选择直刷也不能把插图章节二值化。/ Selecting direct must not binarize an image chapter.
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.preview.command("tap 420 224")
        self.preview.command("tap 494 340")
        self.preview.command("tap 424 1140")
        before = self.preview.state["presents"]
        first = self.preview.png
        self.preview.command("key 2")
        self.assertEqual(self.preview.state["presents"], before + 1)
        # 普通翻页保持快速 GL16。/ Ordinary body turns retain fast GL16.
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
        # 书封、宽主按钮与最近行均开真实书；统计行不触发开书。
        # Cover, wide primary action and recent rows open real books; stats never do.
        for x, y in ((320, 300), (200, 820), (50, 530), (634, 604), (100, 584), (320, 584), (540, 584)):
            self.preview.command("fixture 1")
            self.settle()
            self.preview.command(f"tap {x} {y}")
            settled = self.settle()
            self.assertEqual(settled["page"], self.indices["app_book"])
            self.assertNotEqual(settled["page"], self.indices["app_reading"])
        # 统计行不可点：轻点后仍留在首页、未开书。
        # The stats row stays inert: taps leave Home without opening a book.
        for x, y in ((100, 658), (320, 658), (540, 658)):
            self.preview.command("fixture 1")
            self.settle()
            self.preview.command(f"tap {x} {y}")
            settled = self.settle()
            self.assertEqual(settled["page"], self.indices["app_os_home"])
            self.assertFalse(settled["reading"])

    def test_home_primary_action_has_no_stats_overlap_and_empty_routes(self):
        # 旧空态中央混画统计，新布局的主按钮内芯在所有状态均为黑底。
        # The old empty center mixed stats into the action; the new action core stays black in every state.
        for fixture in (0, 1, 2, 3, 4):
            self.preview.command(f"fixture {fixture}")
            self.settle()
            pixels = self.preview.frame.read_bytes().split(b"\n", 3)[3]
            self.assertEqual(pixels[604 * 684 + 320], 0)
            self.assertEqual(pixels[584 * 684 + 100], 0)
            self.assertEqual(pixels[584 * 684 + 540], 0)
        self.preview.command("fixture 4")
        self.settle()
        self.preview.command("tap 320 568")
        self.settle()
        self.assertEqual(self.state()["page"], self.indices["app_book"])
        self.assertFalse(self.state()["reading"])
        self.preview.command("fixture 0")
        self.settle()
        self.preview.command("tap 320 568")
        self.assertEqual(self.state()["page"], self.indices["app_transfer"])

    def test_home_loading_keeps_shelf_access_and_hidden_paging_inert(self):
        self.preview.command("fixture 4")
        self.preview.command("tap 320 568")
        self.assertEqual(self.state()["page"], self.indices["app_book"])
        self.assertFalse(self.state()["reading"])
        self.preview.command("fixture 1")
        self.settle()
        before = self.preview.png
        self.preview.command("tap 570 1048")
        self.settle()
        self.assertEqual(before, self.preview.png)
        self.assertEqual(self.state()["page"], self.indices["app_os_home"])

    def test_home_long_title_preserves_sync_and_primary_action(self):
        self.preview.command("fixture 5")
        self.settle()
        starts = self.state()["sync_starts"]
        pixels = self.preview.frame.read_bytes().split(b"\n", 3)[3]
        self.assertEqual(pixels[604 * 684 + 320], 0)
        self.preview.command("tap 584 425")
        self.preview.command("tick")
        self.assertEqual(self.state()["sync_starts"], starts + 1)
        self.assertFalse(self.state()["reading"])
        self.settle()
        self.preview.command("tap 320 568")
        self.settle()
        self.assertTrue(self.state()["reading"])

    def test_home_all_books_link_routes_to_shelf(self):
        self.preview.command("fixture 1")
        self.page("app_os_home")
        self.settle()
        self.preview.command("tap 600 734")  # 全部图书 › / All books link
        settled = self.settle()
        self.assertEqual(settled["page"], self.indices["app_book"])

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
        self.preview.command("tap 300 610")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_sleep"])
        light = self.preview.png
        self.preview.command("tap 340 270")
        deep = self.preview.png
        self.assertNotEqual(light, deep)
        self.preview.command("tap 136 270")
        self.assertNotEqual(deep, self.preview.png)
        self.preview.command("tap 136 270")
        self.assertEqual(light, self.preview.png)
        self.preview.command("tap 300 456")
        self.assertNotEqual(light, self.preview.png)
        self.preview.command("tap 136 704")
        clock_style = self.preview.png
        self.assertNotEqual(light, clock_style)
        self.preview.command("tap 419 704")  # 黄历 / almanac style (3rd pill)
        almanac_style = self.preview.png
        self.assertNotEqual(clock_style, almanac_style)
        # 第四枚“图片”芯片只选中样式0；标题行的“去挑选”才进入锁屏图片页；返回回睡眠设置。
        # The fourth "image" pill only selects style 0; the header's "去挑选" link opens the
        # lock-image page; without TF directories it shows the empty state and Return goes back.
        self.preview.command("tap 574 704")
        self.assertEqual(self.state()["page"], self.indices["app_os_sleep"])
        self.preview.command("tap 559 548")
        self.assertEqual(self.state()["page"], self.indices["app_image"])
        images_page = self.preview.png
        self.preview.command("tap 342 1144")
        self.assertEqual(self.state()["page"], self.indices["app_os_sleep"])
        self.assertNotEqual(images_page, self.preview.png)
        self.preview.command("tap 136 704")  # 回时钟 / back to clock
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
        self.preview.command("tap 300 730")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_storage"])
        probing = self.preview.png
        self.preview.command("tick")
        self.assertNotEqual(probing, self.preview.png)
        self.preview.command("tap 557 102")
        self.assertEqual(self.preview.state["page"], self.indices["app_os_settings"])
        self.assertNotEqual(light, probing)

    def test_storage_usb_entry_confirmation_and_failure(self):
        self.page("app_os_storage")
        self.preview.command("tick")
        empty = self.preview.png
        self.preview.command("tap 480 920")
        self.assertNotEqual(empty, self.preview.png)
        self.preview.command("key 0")
        self.assertEqual(empty, self.preview.png)
        self.preview.command("sd 1")
        # 同页请求不重新初始化；离页后重入才重新检测卡状态。
        # Same-page requests do not initialize again; leave and reenter to reprobe card state.
        self.page("app_os_settings")
        self.page("app_os_storage")
        self.preview.command("tick")
        ready = self.preview.png
        # 确认走底部面板：USB 键只开面板，取消/确认在面板两键，面板上方轻点等同取消。
        # Confirming uses the bottom sheet: the USB key only opens it, Cancel/Confirm live on the sheet, and a tap above dismisses.
        self.preview.command("tap 480 920")
        confirm = self.preview.png
        self.assertNotEqual(ready, confirm)
        self.preview.command("tap 198 1030")
        self.assertEqual(ready, self.preview.png)
        self.preview.command("tap 480 920")
        self.preview.command("tap 486 1030")
        self.preview.command("tick")
        self.assertEqual(self.state()["page"], self.indices["app_os_storage"])
        self.assertNotEqual(confirm, self.preview.png)

    def test_reader_typography_menu_and_auto_row(self):
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
        self.preview.command("tap 300 836")  # 行辅助线 / guide rule (row5 y792-880)
        solid = self.preview.png
        self.assertNotEqual(relaxed, solid)
        self.preview.command("tap 424 1140")  # 返回阅读（底栏）/ return via the bottom bar
        reader = self.preview.png
        self.assertNotEqual(solid, reader)
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.preview.command("tap 260 224")  # 操作组 / input group
        normal = self.preview.png
        # 自动翻页在操作组，选择面板外轻点关闭，空白区不触发新控件。
        # Auto-turn lives under Input; an outside tap dismisses the sheet and blank space activates nothing.
        self.preview.command("tap 300 452")
        auto = self.preview.png
        self.assertNotEqual(normal, auto)
        self.preview.command("tap 300 680")
        closed = self.preview.png
        self.preview.command("tap 300 958")
        self.assertEqual(closed, self.preview.png)



    def test_transfer_completion_opens_shelf_after_stop(self):
        self.page("app_os_settings")
        self.preview.command("tap 300 410")
        self.preview.command("tick")
        ready = self.preview.png
        self.preview.command("transfer_fixture 1")
        self.assertNotEqual(ready, self.preview.png)
        self.preview.command("transfer_fixture 3")
        error = self.preview.png
        self.preview.command("transfer_fixture 2")
        self.assertNotEqual(error, self.preview.png)
        self.preview.command("tap 424 1140")
        self.assertEqual(self.state()["page"], self.indices["app_book"])
        self.assertFalse(self.state()["reading"])
        self.assertFalse(self.state()["menu"])

    def test_transfer_home_and_sync_view_in_preview(self):
        # 传书首页：夹具状态与卡片；进度同步入口 → 同步视图；账号键盘可编辑保存。
        # Transfer home: fixture status cards; the sync entry opens the sync view; the keyboard edits accounts.
        self.page("app_transfer")
        home = self.preview.png
        self.preview.command("tick")
        self.assertNotEqual(home, self.preview.png)
        ready = self.preview.png
        self.preview.command("transfer_fixture 4")
        failed = self.preview.png
        self.assertNotEqual(ready, failed)
        self.preview.command("tap 200 544")
        self.preview.command("tick")
        self.assertNotEqual(failed, self.preview.png, "Connection-card Retry restarts the current mode")
        self.preview.command("tap 320 690")
        self.assertEqual(self.preview.state["page"], self.indices["app_transfer"])
        sync = self.preview.png
        self.assertNotEqual(home, sync)
        self.preview.command("tap 300 234")
        keyboard = self.preview.png
        self.assertNotEqual(sync, keyboard)
        self.preview.command("tap 557 102")
        self.assertEqual(self.preview.state["page"], self.indices["app_transfer"])

    def test_transfer_password_error_reopens_editor(self):
        self.preview.command("transfer_fixture 6")
        self.page("app_transfer")
        self.preview.command("tick")
        ready = self.preview.png
        self.preview.command("transfer_fixture 5")
        failed = self.preview.png
        self.assertNotEqual(ready, failed)
        self.preview.command("tap 310 544")
        self.assertNotEqual(failed, self.preview.png, "Re-enter must open the password editor")
        self.preview.command("tap 140 1140")
        self.assertEqual(self.state()["page"], self.indices["app_transfer"])

    def test_reader_sync_and_direct_cached_home(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 300 370")
        self.assertEqual(self.state()["page"], self.indices["app_book"])
        self.assertTrue(self.state()["reading"], "Home book opens before the first shelf frame")
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.preview.command("tap 550 224")
        sync = self.preview.png
        self.preview.command("tap 300 340")
        self.assertTrue(sync != self.preview.png, "Reader settings admit sync directly")
        self.assertEqual(self.state()["sync_job"], 0)
        self.preview.command("tap 424 1140")
        self.assertTrue(self.state()["reading"])
        self.page("app_os_home")
        self.settle()
        home = self.preview.png
        self.page("app_os_today")
        self.page("app_os_home")
        self.assertTrue(home == self.preview.png, "Cached roots paint immediately without loading")

    def test_reader_sync_hides_account_details_but_keeps_configuration(self):
        def reader_sync():
            self.page("app_os_home")
            self.settle()
            self.preview.command("tap 400 425")
            self.settle()
            self.preview.command("key 1")
            self.preview.command("tap 136 1038")
            self.preview.command("tap 550 224")
            return self.preview.png

        def account_band():
            data = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
            return data[200 * 684:500 * 684]

        self.preview.command("fixture 1")
        self.settle()
        original = reader_sync()
        self.page("app_transfer")
        self.preview.command("tap 320 690")
        before = account_band()
        # 通过真实账号键盘修改地址和用户名，不引入只供测试的设置入口。
        # Change URL and username through the real account keyboard, without a test-only settings entry.
        for y, x in ((234, 67), (310, 124)):
            self.preview.command(f"tap 300 {y}")
            self.preview.command(f"tap {x} 464")
            self.preview.command("tap 470 1140")
        configured = account_band()
        self.assertNotEqual(before, configured)
        self.assertEqual(original, reader_sync(), "Reader sync must not reveal configured URL or username")
        self.page("app_transfer")
        self.preview.command("tap 320 690")
        self.assertEqual(configured, account_band(), "Account configuration must survive reader entry")

    def test_image_viewer_async_load_and_cached_neighbors(self):
        # 清除夹具目录的成品缓存，明确验证冷打开；其他测试覆盖热命中。
        # Clear only fixture previews to explicitly test a cold open; warm hits are covered separately.
        import shutil
        shutil.rmtree(OUT / "sd/.pagerest-images", ignore_errors=True)
        self.page("app_image")
        listing = self.preview.png
        before = self.state()["gc_presents"]
        self.preview.command("tap 280 270")
        loading = self.preview.png
        self.assertNotEqual(listing, loading)
        self.assertEqual(self.state()["gc_presents"], before, "Loading feedback must not flash an image frame")
        self.settle(32)
        first = self.preview.png
        self.assertNotEqual(first, loading)
        self.assertEqual(self.state()["gc_presents"], before + 1)
        self.settle(32)
        self.preview.command("tap 480 1050")
        self.settle(32)
        second = self.preview.png
        self.assertNotEqual(first, second)
        self.preview.command("tap 160 1050")
        self.settle(32)
        self.assertEqual(first, self.preview.png, "A cached previous image must preserve exact pixels")
        self.preview.command("tap 470 1144")
        self.assertEqual(listing, self.preview.png)
        self.preview.command("tap 280 270")
        self.settle(32)
        self.assertEqual(first, self.preview.png)

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

    def test_home_epub_encoded_cover_is_original_artwork(self):
        path = OUT / "fx/f/封面之书.epub"
        original = path.read_bytes()
        self.addCleanup(path.write_bytes, original)
        with zipfile.ZipFile(io.BytesIO(original)) as source, zipfile.ZipFile(path, "w") as target:
            for info in source.infolist():
                data = source.read(info)
                if info.filename.endswith(".opf"):
                    data = data.replace(b'href="cover.png"', b'href = "./images/../cover%2Epng#front"')
                target.writestr(info, data)
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")
        self.settle()
        self.preview.command("tap 300 650")
        self.assertTrue(self.state()["reading"])
        self.page("app_os_home")
        self.settle()
        pixels = self.preview.frame.read_bytes().split(b"\n", 3)[3]
        grays = {pixels[y * 684 + x] for y in range(238, 504) for x in range(52, 254)}
        self.assertGreater(len(grays), 6, "Encoded EPUB cover links must render original gradient artwork")

    def test_pin_digits_survive_persistent_font_allocation_failure(self):
        self.page("app_os_pin")
        self.preview.command("font_reclaim")
        self.preview.command("heap_fail 1000000")
        self.page("app_os_settings")
        self.page("app_os_pin")
        pixels = self.reader_pixels()
        for row in range(4):
            for col in range(3 if row < 3 else 1):
                left = 40 + col * 205 if row < 3 else 245
                top = 372 + row * 124
                ink = sum(pixels[y*684+x] < 128 for y in range(top+30,top+74) for x in range(left+70,left+120))
                self.assertGreater(ink, 35, "Every PIN digit remains visible without allocating a font glyph")
        before = self.preview.png
        self.preview.command("tap 342 770")
        self.assertNotEqual(before, self.preview.png)
        self.preview.command("tap 548 770")
        self.assertEqual(before, self.preview.png)
        self.preview.command("heap_fail 0")

    def test_reader_bluetooth_return_keeps_tf_text_across_pages_and_pin(self):
        self.page("app_os_reading")
        self.preview.command("tap 200 732")
        self.preview.command("tap 300 370")
        self.assertFalse(self.state()["font_builtin"])
        headers = {}
        for target in ("app_os_settings","app_os_today","app_os_pin"):
            self.page(target)
            headers[target] = bytes(self.reader_pixels()[:160*684])
        self.page("app_os_home")
        self.open_excerpt_fixture()
        original = self.preview.png
        self.preview.command("key 1")
        self.preview.command("tap 140 1140")
        self.preview.command("tap 540 238")
        self.preview.command("tick")
        self.preview.command("tap 150 614")
        self.preview.command("tap 200 378")
        self.preview.command("tap 300 710")
        self.preview.command("tick")
        self.assertEqual(self.state()["ble_phase"], 6)
        self.preview.command("heap_fail 1")
        self.preview.command("tap 580 120")
        self.settle(32)
        self.assertTrue(self.state()["reading"], "Connected BLE returns to body text rather than the shelf")
        self.assertTrue(self.state()["font_ready"])
        self.assertFalse(self.state()["font_builtin"])
        self.assertEqual(original, self.preview.png)
        for page in ("app_os_settings","app_os_today","app_os_pin","app_os_settings"):
            self.page(page)
            self.assertTrue(self.state()["font_ready"])
            pixels = self.reader_pixels()[:160*684]
            self.assertEqual(bytes(pixels), headers[page], "Pairing/return cannot change or drop any header glyph")

        settings = self.preview.png
        self.preview.command("tap 300 850")
        self.assertEqual(self.state()["page"], self.indices["app_os_bluetooth"])
        self.preview.command("tap 580 120")
        self.assertEqual(self.state()["page"], self.indices["app_os_settings"])
        self.assertEqual(settings, self.preview.png, "Settings entry returns to Settings, never to Library")

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

    def test_reader_stroke_preview_limits_reset_and_body_anchor(self):
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 320 568")
        self.settle()
        baseline_body = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n") + 100 * 684:len(b"P5\n684 1216\n255\n") + 1080 * 684]
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        original = self.preview.png
        self.assertEqual(self.state()["reader_stroke"], 0)
        self.preview.command("tap 590 946")
        self.assertEqual(self.state()["reader_stroke"], 1)
        self.assertNotEqual(original, self.preview.png)
        for _ in range(24):
            self.preview.command("tap 590 946")
        self.assertEqual(self.state()["reader_stroke"], 16)
        self.preview.command("tap 424 1140")
        thick = self.preview.png
        self.assertTrue(self.state()["reading"])
        # 粗细改变后仍从同一页返回，减到下限，再点击数值恢复默认。
        # Return to the same page, reach the thin limit, then tap the value to restore defaults.
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        for _ in range(40):
            self.preview.command("tap 394 946")
        self.assertEqual(self.state()["reader_stroke"], -16)
        self.preview.command("tap 424 1140")
        self.assertNotEqual(thick, self.preview.png)
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.preview.command("tap 500 946")
        self.assertEqual(self.state()["reader_stroke"], 0)
        self.preview.command("tap 424 1140")
        self.assertTrue(self.state()["reading"])
        reset_body = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n") + 100 * 684:len(b"P5\n684 1216\n255\n") + 1080 * 684]
        self.assertEqual(baseline_body, reset_body)

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
        # 对齐方式走选择面板：开面板、选“居中”、再开后面板上方轻点收起并保留所选。
        # Alignment uses the choice sheet: open it, pick "center", reopen and dismiss above the sheet keeping the pick.
        self.preview.command("tap 300 858")
        self.assertNotEqual(menu, self.preview.png)
        self.preview.command("tap 270 1020")
        self.assertNotEqual(menu, self.preview.png)
        self.preview.command("tap 300 858")
        self.assertNotEqual(menu, self.preview.png)
        self.preview.command("tap 300 858")
        self.preview.command("tap 300 680")
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

    def test_time_sync_stays_on_page_and_can_stop(self):
        self.page("app_os_time")
        self.preview.command("tap 424 740")
        self.preview.command("tap 300 848")
        self.assertEqual(self.state()["page"], self.indices["app_os_time"])
        for _ in range(3): self.preview.command("tick")
        self.assertEqual(self.state()["page"], self.indices["app_os_time"])
        self.preview.command("tap 557 102")
        self.assertEqual(self.state()["page"], self.indices["app_os_settings"])

    def test_time_auto_sync_toggle_persists_after_reentry(self):
        self.page("app_os_time")
        enabled = self.preview.png
        self.preview.command("tap 300 1044")
        disabled = self.preview.png
        self.assertNotEqual(enabled, disabled)
        self.page("app_os_settings")
        self.page("app_os_time")
        self.assertEqual(disabled, self.preview.png)
        self.preview.command("tap 300 1044")
        self.assertEqual(enabled, self.preview.png)

    def test_root_tabs_and_reader_clean_once_at_entries(self):
        self.preview.command("fixture 1")
        self.settle()
        for x, symbol in ((220, "app_book"), (350, "app_os_today"),
                          (480, "app_os_settings"), (70, "app_os_home")):
            before = self.state()["gc_presents"]
            self.preview.command(f"tap {x} 1140")
            self.assertEqual(self.state()["page"], self.indices[symbol])
            self.assertEqual(self.state()["gc_presents"], before + 1)
            self.settle()
            self.assertEqual(self.state()["gc_presents"], before + 1)
        before = self.state()["gc_presents"]
        self.preview.command("tap 400 425")
        self.settle()
        self.assertTrue(self.state()["reading"])
        self.assertEqual(self.state()["gc_presents"], before + 1)
        before = self.state()["gc_presents"]
        self.preview.command("key 2")
        self.preview.command("key 0")
        self.assertEqual(self.state()["gc_presents"], before)
        self.preview.command("key 1")
        self.assertEqual(self.state()["gc_presents"], before)
        self.preview.command("tap 136 1038")
        self.assertEqual(self.state()["gc_presents"], before + 1)
        # 行距开选择面板并选“舒展”：面板与选档都是页内 GL16，不计清理。
        # The leading row opens the sheet and picks a tier: both stay in-page GL16 outside the cleanup count.
        self.preview.command("tap 300 452")
        self.assertEqual(self.state()["gc_presents"], before + 1)
        self.preview.command("tap 270 1020")
        self.assertEqual(self.state()["gc_presents"], before + 1)
        self.preview.command("tap 424 1140")
        self.assertEqual(self.state()["gc_presents"], before + 2)
        self.preview.command("tick")
        self.assertEqual(self.state()["gc_presents"], before + 2)

    def test_menu_cleans_only_show_close_not_unchanged_or_disabled_navigation(self):
        cleans = self.state()["gc_presents"]
        self.preview.command("menu")
        self.assertTrue(self.state()["menu"])
        self.assertEqual(self.state()["gc_presents"], cleans + 1)
        self.preview.command("menu")
        self.preview.command("tap 10 500")
        self.assertEqual(self.state()["gc_presents"], cleans + 1)
        self.preview.command("tap 424 1140")
        # 当前四个产品入口只有一叶，空白及不可用翻页不请求清理。
        # The four product entries occupy one leaf; empty taps and unavailable paging do not request cleanup.
        self.assertEqual(self.state()["menu_leaf"], 0)
        self.assertEqual(self.state()["gc_presents"], cleans + 1)
        self.preview.command("key 0")
        self.assertEqual(self.state()["menu_leaf"], 0)
        self.assertEqual(self.state()["gc_presents"], cleans + 1)
        self.preview.command("key 2")
        self.assertFalse(self.state()["menu"])
        self.assertEqual(self.state()["gc_presents"], cleans + 2)
        self.preview.command("tick")
        self.assertEqual(self.state()["gc_presents"], cleans + 2)

    def test_reader_toolbar_and_settings_returns_clean_once_then_turns_stay_gray(self):
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        cleans = self.state()["gc_presents"]
        self.preview.command("key 1")
        self.assertEqual(self.state()["gc_presents"], cleans)
        self.preview.command("key 1")
        self.assertEqual(self.state()["gc_presents"], cleans + 1)
        self.preview.command("tick")
        self.assertEqual(self.state()["gc_presents"], cleans + 1)
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.assertEqual(self.state()["gc_presents"], cleans + 2)
        self.preview.command("tap 300 452")
        self.assertEqual(self.state()["gc_presents"], cleans + 2)
        self.preview.command("tap 270 1020")
        self.assertEqual(self.state()["gc_presents"], cleans + 2)
        self.preview.command("tap 424 1140")
        self.assertEqual(self.state()["gc_presents"], cleans + 3)
        for key in (2, 0, 2, 0):
            self.preview.command(f"key {key}")
            self.settle()
            self.assertEqual(self.state()["refresh_mode"], 5)
            self.assertEqual(self.state()["gc_presents"], cleans + 3)

    def test_loading_shelf_accepts_root_navigation(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")
        self.preview.command("tap 480 1140")
        self.assertEqual(self.state()["page"], self.indices["app_os_settings"])
        self.preview.command("tap 350 1140")
        self.preview.command("tap 542 320")
        self.assertEqual(self.state()["page"], self.indices["app_os_today"])

    def test_journal_legend_is_inert_and_goal_saves_cancel_retry(self):
        self.preview.command("fixture 1")
        self.settle()
        self.page("app_os_today")
        self.settle()
        self.preview.command("tap 342 320")
        calendar = self.preview.png
        presents = self.state()["presents"]
        for command in ("tap 300 944", "tap 300 980", "hold 300 960"):
            self.preview.command(command)
            self.assertEqual(self.state()["page"], self.indices["app_os_today"])
            self.assertEqual(calendar, self.preview.png)
        self.assertEqual(self.state()["presents"], presents)
        self.assertEqual(self.state()["daily_goal"], 0)
        self.preview.command("tap 300 1050")
        self.preview.command("tap 548 582")  # 30-minute preset
        self.preview.command("tap 180 946")  # cancel
        self.assertEqual(self.state()["daily_goal"], 0)
        self.preview.command("tap 300 1050")
        self.preview.command("tap 548 582")
        self.preview.command("tap 512 765")  # +5
        self.preview.command("tap 500 946")
        self.assertEqual(self.state()["daily_goal"], 35)
        self.preview.command("tap 300 1050")
        self.preview.command("tap 342 658")  # 60-minute preset
        self.preview.command("goal_fail 1")
        self.preview.command("tap 500 946")
        self.assertEqual(self.state()["daily_goal"], 35)
        failed = self.preview.png
        self.preview.command("tap 500 946")
        self.assertEqual(self.state()["daily_goal"], 60)
        self.assertNotEqual(failed, self.preview.png)
        self.page("app_os_settings")
        self.page("app_os_today")
        self.assertEqual(self.state()["daily_goal"], 60)
        self.preview.command("tap 300 1050")
        self.preview.command("tap 136 582")  # unset
        self.preview.command("tap 500 946")
        self.assertEqual(self.state()["daily_goal"], 0)

    def test_excerpt_clock_advances_without_changing_quote(self):
        self.preview.command("quote_fixture -1")
        self.page("app_os_sleep")
        self.preview.command("tap 300 880")
        self.preview.command("tap 300 334")
        self.preview.command("tap 500 64")
        self.preview.command("lock_quote")
        first = self.reader_pixels()
        self.assertEqual(self.state()["lock_mode"], 0)
        self.preview.command("time_step 61")
        self.preview.command("lock_quote")
        second = self.reader_pixels()
        self.assertEqual(first[:1036*684], second[:1036*684])
        self.assertNotEqual(first[1036*684:], second[1036*684:])
        self.preview.command("time_step 3600")
        self.preview.command("lock_quote")
        self.assertEqual(self.reader_pixels()[:1036*684], first[:1036*684])

    def test_gray_balance_opt_in_routing_and_restore(self):
        self.open_fixture_reader()
        self.reader_display_settings()
        self.preview.command("tap 494 340")
        self.preview.command("tap 320 590")
        self.preview.command("tap 424 1140")
        original = self.reader_pixels()[:1096*684]
        self.reader_display_settings()
        self.preview.command("tap 300 503")
        self.preview.command("tap 424 1140")
        self.assertEqual(original, self.reader_pixels()[:1096*684], "Balance changes drive, not raster or spatial codes")
        self.preview.command("key 2")
        self.settle()
        self.assertEqual(self.state()["refresh_wave"], 10)
        self.reader_display_settings()
        self.preview.command("tap 300 503")
        self.preview.command("tap 424 1140")
        self.preview.command("key 2")
        self.settle()
        self.assertEqual(self.state()["refresh_wave"], 9)

        self.reader_display_settings()
        self.preview.command("tap 300 503")
        self.preview.command("tap 424 1140")
        self.preview.command("key 2")
        self.settle()
        self.assertEqual(self.state()["refresh_wave"], 10, "Gray refresh cycles only Fast/Fine")

    def test_gray_speed_full_width_toggle_has_no_edge_comparison(self):
        self.open_fixture_reader()
        self.reader_display_settings()
        self.preview.command("tap 494 340")
        self.preview.command("tap 320 590")
        self.preview.command("tap 424 1140")
        original = self.reader_pixels()[:1096*684]
        self.reader_display_settings()
        self.preview.command("tap 500 590")
        binary_settings = self.preview.png
        self.preview.command("tap 300 503")
        self.assertEqual(binary_settings, self.preview.png, "Gray refresh is inactive under binary edges")
        self.preview.command("tap 424 1140")
        binary = self.reader_pixels()[:1096*684]
        self.assertNotEqual(original, binary, "The former right edge selector now toggles the whole gray/binary row")
        self.reader_display_settings()
        self.preview.command("tap 500 590")
        self.preview.command("tap 424 1140")
        self.assertEqual(self.state()["reader_stroke"], 0)
        self.assertEqual(original, self.reader_pixels()[:1096*684], "Re-enabling gray always restores original edge rendering")
        self.reader_display_settings()
        self.preview.command("tap 100 590")
        self.preview.command("tap 424 1140")
        self.assertEqual(binary, self.reader_pixels()[:1096*684], "Both halves operate the same full-width control")

    def test_gray_speed_precise_curves_preserve_metrics_and_quality(self):
        self.open_fixture_reader()
        quality = self.reader_pixels()[:1096*684]
        self.reader_display_settings()
        self.preview.command("tap 494 340")
        self.preview.command("tap 320 590")
        self.preview.command("tap 424 1140")
        speed = self.reader_pixels()[:1096*684]
        self.assertNotEqual(quality, speed, "Only speed gray uses more precise curve flattening")
        self.assertEqual(self.state()["reader_stroke"], 0, "No extra stroke compensation")
        self.reader_display_settings()
        self.preview.command("tap 180 340")
        self.preview.command("tap 424 1140")
        self.assertEqual(quality, self.reader_pixels()[:1096*684])

    def test_transfer_return_restores_saved_font_before_first_settings_frame(self):
        self.page("app_os_reading")
        self.preview.command("tap 200 732")
        self.preview.command("tap 300 370")
        self.assertFalse(self.state()["font_builtin"])
        self.page("app_os_settings")
        saved = self.preview.png
        for _ in range(3):
            self.preview.command("tap 300 370")
            self.assertTrue(self.state()["font_builtin"], "Transfer keeps the SD font suspended")
            self.preview.command("tap 300 1140")
            self.assertEqual(self.state()["page"], self.indices["app_os_settings"])
            self.assertFalse(self.state()["font_builtin"], "The first return frame already uses the saved SD font")
            self.assertEqual(saved, self.preview.png)

    def test_transfer_font_allocation_failure_keeps_text_and_navigation(self):
        self.page("app_os_reading")
        self.preview.command("tap 200 732")
        self.preview.command("tap 300 370")
        self.assertFalse(self.state()["font_builtin"])
        self.page("app_os_settings")
        saved = self.preview.png
        self.preview.command("heap_fail 1")
        self.page("app_transfer")
        self.assertTrue(self.state()["font_ready"], "A failed built-in switch must preserve the working SD font")
        self.assertFalse(self.state()["font_builtin"])
        header = self.reader_pixels()[:150*684]
        self.assertGreater(sum(p < 200 for p in header), 300, "The first transfer frame retains its title and labels")
        self.page("app_os_settings")
        self.assertTrue(self.state()["font_ready"])
        self.assertFalse(self.state()["font_builtin"])
        self.assertEqual(saved, self.preview.png, "Navigation recovers directly without visiting Home")
        self.page("app_transfer")
        self.assertTrue(self.state()["font_ready"])
        self.assertTrue(self.state()["font_builtin"])
        self.preview.command("tick")
        self.assertTrue(self.state()["font_ready"])
        self.page("app_os_settings")
        self.assertEqual(saved, self.preview.png)

    def test_failed_sd_font_switch_keeps_all_pages_legible(self):
        self.page("app_os_home")
        home = self.preview.png
        self.page("app_font_pick")
        self.preview.command("heap_fail 100")
        self.preview.command("tap 280 376")
        self.preview.command("heap_fail 0")
        self.assertTrue(self.state()["font_ready"], "A failed SD font build must retain the current font")
        self.assertTrue(self.state()["font_builtin"])
        self.page("app_os_home")
        self.assertEqual(home, self.preview.png, "Navigation after a failed font switch must retain text")

    def test_sd_glyph_read_failure_preserves_cache_and_recovers_builtin(self):
        self.page("app_os_reading")
        self.preview.command("tap 200 732")
        self.preview.command("tap 300 370")
        self.assertFalse(self.state()["font_builtin"])
        self.preview.command("font_reclaim")
        self.page("app_os_home")
        home = self.preview.png
        self.preview.command("font_io_fail 1")
        self.page("app_os_reading")
        self.assertTrue(self.state()["font_io_failed"])
        self.page("app_os_home")
        self.assertEqual(home, self.preview.png, "One failed SD read must not drop every cached glyph")
        self.preview.command("font_builtin")
        self.assertTrue(self.state()["font_ready"])
        self.assertTrue(self.state()["font_builtin"])
        self.assertFalse(self.state()["font_io_failed"])
        self.page("app_os_home")
        self.assertGreater(sum(p < 200 for p in self.reader_pixels()[:150*684]), 300)

    def test_sd_glyph_memory_failure_requests_font_recovery(self):
        self.page("app_os_reading")
        self.preview.command("tap 200 732")
        self.preview.command("tap 300 370")
        self.assertFalse(self.state()["font_builtin"])
        self.preview.command("font_reclaim")
        self.preview.command("heap_fail 100")
        self.page("app_os_home")
        self.assertTrue(self.state()["font_render_failed"])
        self.assertFalse(self.state()["font_io_failed"])
        self.preview.command("heap_fail 0")
        self.preview.command("font_builtin")
        self.assertTrue(self.state()["font_builtin"])
        self.assertFalse(self.state()["font_render_failed"])

    def test_boot_fade_uses_real_artwork_and_preserves_brand(self):
        self.preview.command("boot 0")
        light = self.reader_pixels()
        self.preview.command("boot 1")
        settled = self.reader_pixels()
        self.assertNotEqual(light, settled)
        self.assertEqual(light[160*684:280*684], settled[160*684:280*684], "Brand stays stable during the fade")
        self.assertTrue(all(a >= b for a,b in zip(light,settled)), "The illustration only gains ink")
        self.assertLess(sum(settled[330*684:1000*684]), 255*684*670, "Real illustration is visible")
        self.assertEqual(set(settled[1192*684:]), {255}, "No watermark at the lower edge")

    def test_short_quote_centers_actual_selected_content(self):
        self.preview.command("quote_fixture -1")
        self.page("app_os_sleep")
        self.preview.command("tap 300 880")
        self.preview.command("tap 300 334")
        self.preview.command("tap 500 64")
        self.preview.command("lock_quote")
        pixels = self.reader_pixels()
        xs = [x for y in range(300,950) for x in range(40,644) if pixels[y*684+x] < 200]
        self.assertTrue(xs)
        self.assertTrue(290 <= (min(xs)+max(xs))/2 <= 390, "Short quote and source form a centered group")

    def open_grid_shelf(self, fixture=4):
        self.preview.command(f"fixture {fixture}")
        self.settle()
        self.preview.command("tap 220 1140")
        self.settle()
        self.assertFalse(self.state()["shelf_grid"])
        self.preview.command("tap 610 208")
        self.settle()
        self.assertTrue(self.state()["shelf_grid"])

    def test_grid_layout_roundtrip_and_persistent_root_return(self):
        self.preview.command("fixture 4")
        self.settle()
        self.preview.command("tap 220 1140")
        self.settle()
        self.preview.command("tap 544 1046")
        self.assertEqual(self.state()["leaf"], 1)
        self.preview.command("tap 610 208")
        self.settle()
        self.assertEqual(self.state()["leaf"], 0)
        self.assertTrue(self.state()["shelf_grid"])
        before = self.preview.png
        self.preview.command("tap 610 208")
        self.assertEqual(before, self.preview.png, "Selecting the current layout is inert")
        self.preview.command("tap 540 208")
        self.assertEqual(self.state()["leaf"], 1)
        self.preview.command("tap 610 208")
        self.page("app_os_today")
        self.preview.command("tap 220 1140")
        self.settle()
        self.assertTrue(self.state()["shelf_grid"])
        self.assertEqual(self.state()["leaf"], 0)
        before = self.preview.png
        self.preview.command("tap 544 1046")
        self.assertEqual(self.state()["leaf"], 0, "Six fixture books occupy exactly one grid page")
        self.assertEqual(before, self.preview.png)

    def test_grid_sixth_tile_long_press_and_bulk_return(self):
        self.open_grid_shelf()
        grid = self.preview.png
        self.preview.command("hold 494 880")
        self.assertNotEqual(grid, self.preview.png)
        self.assertFalse(self.state()["reading"])
        self.preview.command("key 0")
        self.assertEqual(grid, self.preview.png)
        self.preview.command("tap 342 1046")
        bulk = self.preview.png
        self.preview.command("tap 300 350")
        self.assertNotEqual(bulk, self.preview.png)
        self.preview.command("tap 342 1140")
        self.settle()
        self.assertTrue(self.state()["shelf_grid"])
        self.preview.command("tap 494 880")
        self.settle()
        self.assertTrue(self.state()["reading"])

    def test_grid_long_title_and_empty_fallback(self):
        self.open_grid_shelf(5)
        pixels = self.reader_pixels()
        self.assertEqual({pixels[y*684+x] for y in range(300,625) for x in range(248,644)}, {255})
        self.preview.command("tap 190 380")
        self.settle()
        self.assertTrue(self.state()["reading"])
        self.preview.command("fixture 0")
        self.settle()
        self.preview.command("tap 220 1140")
        self.settle()
        self.assertFalse(self.state()["reading"])
        self.preview.command("tap 610 208")
        self.assertEqual(self.state()["page"], self.indices["app_book"])

    def test_journal_unsynced_calendar_and_minute_state(self):
        self.preview.command("fixture 0")
        self.settle()
        self.page("app_os_today")
        valid = self.preview.png
        self.preview.command("time_valid 0")
        self.assertNotEqual(valid, self.preview.png)
        self.assertNotEqual(self.state()["time_state"], 2)
        self.preview.command("tap 342 320")
        invalid_calendar = self.preview.png
        self.preview.command("time_valid 1")
        self.assertNotEqual(invalid_calendar, self.preview.png)
        calendar = self.preview.png
        self.preview.command("time_step 60")
        self.assertEqual(calendar, self.preview.png, "A minute change does not repaint a daily calendar")
        self.preview.command("time_valid 0")
        self.assertEqual(invalid_calendar, self.preview.png)

    def test_sleep_effective_mode_and_disabled_pickup(self):
        self.page("app_os_sleep")
        self.assertEqual(self.state()["lock_mode"], 0)
        before = self.preview.png
        self.preview.command("tap 300 456")
        self.assertNotEqual(before, self.preview.png)
        self.preview.command("tap 340 270")
        self.assertEqual(self.state()["lock_mode"], 1)
        self.preview.command("tap 136 704")
        self.assertEqual(self.state()["lock_mode"], 0, "Dynamic face retains light sleep under the Deep selection")
        self.preview.command("tap 544 270")
        self.assertEqual(self.state()["lock_mode"], 2)
        before = self.preview.png
        self.preview.command("tap 300 456")
        self.assertEqual(before, self.preview.png)
        self.preview.command("tap 136 270")
        self.assertEqual(self.state()["lock_mode"], 0)
        self.preview.command("tap 300 456")
        self.assertNotEqual(before, self.preview.png)

    def test_sleep_unsynced_thumbnails_and_low_memory_fallback(self):
        self.preview.command("time_valid 0")
        self.page("app_os_sleep")
        pixels = self.reader_pixels()
        tiles = [bytes(pixels[y*684+x] for y in range(592,770) for x in range(left,left+100)) for left in (61,215,369,523)]
        self.assertEqual(tiles[0], tiles[1])
        self.assertEqual(tiles[1], tiles[2])
        self.assertEqual(tiles[2], tiles[3])
        self.assertGreater(len(set(tiles[0])), 2)
        ready = self.preview.png
        self.page("app_os_settings")
        self.preview.command("heap_fail 1")
        self.page("app_os_sleep")
        self.assertNotEqual(ready, self.preview.png)
        self.page("app_os_settings")
        self.page("app_os_sleep")
        self.assertEqual(ready, self.preview.png)

    def test_today_clock_summary_and_time_settings(self):
        self.preview.command("fixture 1")
        for _ in range(20): self.preview.command("tick")
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
        # 手帐子标签切换：打卡月历、金句便签、今日手记
        self.page("app_os_today")
        self.settle()
        tab_today = self.preview.png
        self.preview.command("tap 342 320")  # 打卡月历 / Month calendar tab
        tab_month = self.preview.png
        self.assertNotEqual(tab_today, tab_month)
        self.preview.command("tap 542 320")  # 金句便签 / Quotes tab
        tab_quotes = self.preview.png
        self.assertNotEqual(tab_month, tab_quotes)
        self.preview.command("tap 142 320")  # 切回今日手记 / Back to Today tab
        self.assertEqual(tab_today, self.preview.png)
        self.preview.command("fixture 0")
        self.preview.command("tap 350 1140")
        self.assertEqual(self.state()["page"], self.indices["app_os_today"])
        # 设置 → 时间与时区：步进时区即时重排，返回回到设置。
        self.preview.command("tap 480 1140")
        self.preview.command("tap 300 500")  # 卡片本体；行间空隙已不触发命中 / card body; row gaps no longer hit
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
        presents = self.state()["presents"]
        cleans = self.state()["gc_presents"]
        self.preview.command("tap 342 1038")  # 清除残影 / clean ghosting (GC16)
        self.assertEqual(self.preview.state["refresh_mode"], 2)
        # 按下局推反馈，抬起仅一次正文清理。/ Local press feedback, then one body cleanup on release.
        self.assertEqual(self.state()["presents"], presents + 2)
        self.assertEqual(self.state()["gc_presents"], cleans + 1)
        self.assertNotEqual(tools, self.preview.png)
        night = self.preview.png
        self.preview.command("key 1")
        self.preview.command("key 1")
        self.assertEqual(night, self.preview.png)
        self.preview.command("key 1")
        self.preview.command("tap 544 1040")  # 书架 / shelf
        shelf = self.preview.png
        self.assertNotEqual(night, shelf)
        self.preview.command("tap 480 1140")  # 根导航设置 / root to Settings
        self.assertEqual(self.state()["page"], self.indices["app_os_settings"])

    def test_day_standard_period_counts_turns_and_holds_idle(self):
        self.open_excerpt_fixture()
        cleans = self.state()["gc_presents"]
        for turn in range(1, 13):
            self.preview.command("key 2" if turn % 2 else "key 0")
            self.settle()
            self.assertEqual(self.state()["gc_presents"], cleans + turn // 10)
            self.assertEqual(self.state()["refresh_mode"], 2 if turn % 10 == 0 else 5)
            self.preview.command("tick")
            self.assertEqual(self.state()["gc_presents"], cleans + turn // 10)

    def test_reader_direct_profile_and_return_to_gray(self):
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.preview.command("tap 420 224")
        standard = self.preview.png
        self.preview.command("tap 494 340")
        self.assertNotEqual(standard, self.preview.png)
        self.preview.command("tap 424 1140")
        pixels = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
        self.assertEqual(set(pixels), {0, 255}, "Direct entry commits the same binary target as turns")
        cleans = self.state()["gc_presents"]
        turn = 0
        for _ in range(6):
            for key in (2, 0):
                self.preview.command(f"key {key}")
                self.settle()
                turn += 1
                self.assertEqual(self.state()["refresh_mode"], 2 if turn % 10 == 0 else 5)
                self.assertEqual(self.state()["gc_presents"], cleans + turn // 10)
                pixels = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
                self.assertEqual(set(pixels), {0, 255})
        # 离页重进仍保留选项，切回灰阶后不留下虚假参考帧。
        # Leaving/reentering retains the choice; switching back restores grayscale rather than a false reference.
        self.page("app_os_home")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        pixels = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
        self.assertEqual(set(pixels), {0, 255})
        self.preview.command("key 2")
        self.settle()
        self.assertEqual(self.state()["refresh_mode"], 5)
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.preview.command("tap 420 224")
        self.preview.command("tap 190 340")
        self.assertEqual(standard, self.preview.png)
        self.preview.command("tap 424 1140")
        self.preview.command("key 0")
        self.settle()
        self.assertEqual(self.state()["refresh_mode"], 5)
        pixels = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
        self.assertGreater(len(set(pixels)), 2)

    def test_reader_gray_edges_retain_actual_levels_and_original_direct(self):
        self.open_fixture_reader()
        self.reader_display_settings()
        self.preview.command("tap 494 340")
        self.preview.command("tap 320 590")
        gray_menu = self.preview.png
        self.preview.command("tap 424 1140")
        self.assertGreater(len(set(self.reader_pixels())), 2)
        before = self.state()
        self.preview.command("key 2")
        self.settle()
        self.assertEqual(self.state()["refresh_wave"], 9)
        self.assertEqual(self.state()["gc_presents"], before["gc_presents"])
        self.assertEqual(self.state()["night_turns"], 0)
        gray_page = self.reader_pixels()
        self.reader_display_settings()
        self.assertEqual(self.preview.png, gray_menu)
        self.preview.command("tap 320 590")
        self.preview.command("tap 424 1140")
        self.assertEqual(set(self.reader_pixels()), {0, 255})
        self.assertNotEqual(self.reader_pixels(), gray_page)
        self.preview.command("key 0")
        self.settle()
        self.assertEqual(self.state()["refresh_wave"], 6)
        self.assertEqual(set(self.reader_pixels()), {0, 255})

    def test_reader_turn_rows_dead_zones_and_direct_edge_controls(self):
        self.open_fixture_reader()
        self.reader_display_settings()
        current = self.preview.png
        presents = self.state()["presents"]
        # 灰字缘只有速度模式可用；说明文字和两选项的间隙不触发动作。
        # Gray edges require speed mode; explanatory text and the profile gap activate nothing.
        for command in ("tap 320 590", "tap 342 340", "tap 300 680", "hold 300 680", "swipe 300 680 300 670"):
            self.preview.command(command)
            self.assertEqual(self.preview.png, current)
            self.assertEqual(self.state()["presents"], presents)
        self.preview.command("tap 494 340")
        direct = self.preview.png
        self.assertNotEqual(direct, current)
        self.preview.command("tap 320 590")
        self.assertNotEqual(self.preview.png, direct)
        self.preview.command("tap 260 224")
        self.preview.command("tap 300 564")
        experiments = self.preview.png
        self.preview.command("tap 300 452")
        self.assertNotEqual(experiments, self.preview.png)
        # 选择面板是模态的：先点外部只收起面板，再点标签才切组。
        # The choice sheet is modal: the first outside tap dismisses it, and the next switches tabs.
        self.preview.command("tap 260 224")
        self.assertEqual(experiments, self.preview.png)
        self.preview.command("tap 260 224")
        self.assertNotEqual(experiments, self.preview.png)

    def test_reader_size_preview_keeps_anchor_after_repeated_resize(self):
        self.open_fixture_reader()
        for _ in range(3):
            self.preview.command("key 2")
            self.settle()
        body = self.reader_pixels()[:684 * 1096]
        self.preview.command("key 1")
        self.preview.command("tap 544 944")
        original = self.reader_pixels()[684 * 112:684 * 762]
        self.assertGreater(len(set(original)), 2)
        self.preview.command("tap 544 944")
        self.assertNotEqual(original, self.reader_pixels()[684 * 112:684 * 762])
        self.preview.command("tap 544 944")
        for _ in range(2):
            self.preview.command("tap 342 944")
        self.assertEqual(original, self.reader_pixels()[684 * 112:684 * 762])
        self.preview.command("tap 424 1140")
        self.assertEqual(body, self.reader_pixels()[:684 * 1096])

    def test_reader_font_return_restores_source_panel(self):
        self.open_fixture_reader()
        self.preview.command("key 2")
        self.settle()
        self.preview.command("key 1")
        self.preview.command("tap 544 944")
        sizes = self.reader_pixels()[:684 * 1096]
        self.preview.command("tap 136 1038")
        self.assertEqual(self.state()["page"], self.indices["app_font_pick"])
        picker = self.preview.png
        self.preview.command("tap 300 370")
        self.assertNotEqual(picker, self.preview.png)
        self.preview.command("tap 557 102")
        self.assertTrue(self.state()["reading"])
        self.assertEqual(self.state()["page"], self.indices["app_book"])
        self.assertEqual(sizes, self.reader_pixels()[:684 * 1096])
        self.preview.command("tap 342 1038")
        text_settings = self.preview.png
        self.preview.command("tap 300 340")
        self.assertEqual(self.state()["page"], self.indices["app_font_pick"])
        self.preview.command("tap 557 102")
        self.assertTrue(self.state()["reading"])
        self.assertEqual(text_settings, self.preview.png)
        self.preview.command("tap 424 1140")
        self.assertTrue(self.state()["reading"])

    def test_reader_binary_guides_keep_grid_through_turns(self):
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        # 行辅助线走选择面板：先开面板选“实线”，再切显示组开直刷。
        # Guide rules use the choice sheet: open it and pick "solid", then switch groups to enable direct.
        self.preview.command("tap 300 770")
        self.preview.command("tap 270 1020")
        self.preview.command("tap 420 224")
        self.preview.command("tap 494 340")
        self.preview.command("tap 424 1140")
        cleans = self.state()["gc_presents"]
        for key in (2, 0, 2, 0):
            self.preview.command(f"key {key}")
            self.settle()
            self.assertEqual(self.state()["gc_presents"], cleans)
            pixels = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
            self.assertEqual(set(pixels), {0, 255})
            # 检查量化后的线仍可见且位置固定；字形避线由真实轮廓回归验证。
            # Quantized rules stay visible on one grid; real-outline regression checks glyph separation.
            rules = [y for y in range(88, 1088)
                     if set(pixels[y * 684 + 40:y * 684 + 644]) == {0}]
            self.assertTrue(rules, "Binary quantization must preserve visible guide rules")
            self.assertTrue(all((y - 24) % 72 in (53, 54) for y in rules))

    def test_reader_settings_clean_returns_to_body_once(self):
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        # 首次保存会更新底栏已读比例；先取得正常返回后的正文基准。
        # The first save updates the footer's saved percentage; use a normal return as the body reference.
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.preview.command("tap 424 1140")
        body = self.preview.png
        for group in (0, 1, 2):
            self.preview.command("key 1")
            self.preview.command("tap 136 1038")
            self.preview.command(f"tap {136 + 206 * group} 224")
            self.assertNotEqual(body, self.preview.png)
            presents = self.state()["presents"]
            cleans = self.state()["gc_presents"]
            self.preview.command("tap 164 1140")
            self.assertEqual(self.state()["refresh_mode"], 2)
            self.assertEqual(self.state()["presents"], presents + 2)
            self.assertEqual(self.state()["gc_presents"], cleans + 1)
            self.assertEqual(body, self.preview.png)
            self.preview.command("tick")
            self.assertEqual(self.state()["presents"], presents + 2)
            self.assertEqual(self.state()["gc_presents"], cleans + 1)
        self.preview.command("key 2")
        self.settle()
        self.assertEqual(self.state()["refresh_mode"], 5)
        self.assertNotEqual(body, self.preview.png)
        self.preview.command("key 0")
        self.settle()
        self.assertEqual(self.state()["refresh_mode"], 5)
        self.assertEqual(body, self.preview.png)


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
