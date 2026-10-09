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
    def test_home_sync_uploads_without_opening_book(self):
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
        self.assertEqual(self.state()["sync_job"], 2)  # OS_SYNC_JOB_PUSH
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
        self.preview.command("tap 570 972")
        self.settle()
        self.assertEqual(self.state()["history_page"], 1)
        self.assertNotEqual(first, self.preview.png)
        self.preview.command("tap 110 972")
        self.settle()
        self.assertEqual(self.state()["history_page"], 0)
        self.assertEqual(first, self.preview.png)

    def test_body_sentence_excerpt_and_source_jump(self):
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        first = self.preview.png
        self.preview.command("hold 180 432")
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

    def test_home_upload_progress_accepts_edges_without_stealing_resume(self):
        # 边缘容差内轻点及小幅移动仍上传；继续阅读与卡片封面仍开书。
        # Taps and small movement within edge tolerance upload; resume and the cover still open the book.
        for command in ("tap 466 386", "tap 634 484", "tap 490 470", "swipe 476 400 467 394"):
            self.preview.command("fixture 1")
            self.settle()
            starts = self.state()["sync_starts"]
            self.preview.command(command)
            self.assertEqual(self.state()["page"], self.indices["app_os_home"])
            self.assertFalse(self.state()["reading"])
            self.preview.command("tick")
            self.assertEqual(self.state()["sync_starts"], starts + 1)
            self.assertEqual(self.state()["sync_job"], 2)
        for x, y in ((450, 440), (200, 380)):
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
        match = re.search(r"style == 1 \? \(EpdRect\)\{0, (\d+), UI_LOCK_WIDTH, (\d+)\}", source)
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
        self.preview.command("tap 342 224")

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
        self.preview.command("tap 544 212")
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
        self.preview.command("tap 544 212")
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

    def check_reader_night_manual_clean(self, direct):
        self.open_fixture_reader()
        self.reader_display_settings()
        self.preview.command("tap 300 676")
        if direct:
            self.preview.command("tap 300 1006")
        self.preview.command("tap 424 1140")
        for _ in range(64):
            self.preview.command("tick")
        self.preview.command("key 1")
        self.preview.command("key 1")
        self.preview.command("key 2")
        body = self.reader_pixels()
        self.assertEqual(self.state()["night_turns"], 1)
        self.reader_display_settings()
        self.assertEqual(self.reader_pixels()[0], 255)
        before = self.state()
        self.preview.command("tap 170 1140")
        self.assertEqual(self.reader_pixels(), body)
        self.assert_reader_dark_margins()
        self.assertEqual(self.state()["physical_clears"], before["physical_clears"] + 1)
        self.assertEqual(self.state()["gc_presents"], before["gc_presents"] + 1)
        self.assertEqual(self.state()["refresh_mode"], 2)
        self.assertEqual(self.state()["night_turns"], 0)
        if direct:
            self.assertEqual(set(self.reader_pixels()), {0, 255})
        before = self.state()
        self.preview.command("key 2")
        self.assertEqual(self.state()["physical_clears"], before["physical_clears"])
        self.assertEqual(self.state()["gc_presents"], before["gc_presents"])
        self.assertEqual(self.state()["night_turns"], 1)

    def test_reader_night_manual_clean_standard(self):
        self.check_reader_night_manual_clean(False)

    def test_reader_night_manual_clean_direct(self):
        self.check_reader_night_manual_clean(True)

    def test_reader_night_whole_frame_and_cached_turns(self):
        self.open_fixture_reader()
        day = self.reader_pixels()
        self.reader_display_settings()
        self.preview.command("tap 300 676")
        # 设置保持日间，返回阅读后所有像素共同反色，包括边距和页脚。
        # Settings stay day-themed; all reader pixels invert together on return, including margins and footer.
        self.assertEqual(self.reader_pixels()[0], 255)
        self.preview.command("tap 424 1140")
        night = self.reader_pixels()
        self.assertEqual(night, bytes(255 - value for value in day))
        self.assert_reader_dark_margins()
        for _ in range(2):
            self.preview.command("key 2")
            self.settle()
            self.assertNotEqual(self.reader_pixels(), night)
            self.assert_reader_dark_margins()
            self.preview.command("key 0")
            self.settle()
            self.assertEqual(self.reader_pixels(), night)
        self.reader_display_settings()
        self.preview.command("tap 300 676")
        self.preview.command("tap 424 1140")
        self.assertEqual(self.reader_pixels(), day)

    def test_reader_night_footer_and_size_updates_do_not_count_turns(self):
        self.page("app_os_reading")
        self.preview.command("tap 300 900")
        self.preview.command("tap 300 966")
        self.page("app_os_home")
        self.open_fixture_reader()
        self.reader_display_settings()
        self.preview.command("tap 300 676")
        self.preview.command("tap 424 1140")
        self.preview.command("key 2")
        self.settle()
        before = self.reader_pixels()
        state = self.state()
        self.assertEqual(state["night_turns"], 1)
        self.preview.command("time_step 60")
        after = self.reader_pixels()
        changed = [i for i, (a, b) in enumerate(zip(before, after)) if a != b]
        self.assertTrue(changed)
        self.assertTrue(all(1096 <= i // 684 < 1192 and 40 <= i % 684 < 548 for i in changed))
        self.assertEqual(self.state()["night_turns"], 1)
        self.assertEqual(self.state()["gc_presents"], state["gc_presents"])
        self.assertEqual(self.state()["body_presents"], state["body_presents"])
        self.assertEqual(self.state()["night_area_presents"], state["night_area_presents"] + 1)
        self.assertEqual(self.state()["refresh_wave"], 5)
        self.assert_reader_dark_margins()
        self.preview.command("key 1")
        self.preview.command("tap 544 944")
        self.preview.command("tap 544 944")
        self.preview.command("time_step 2")
        self.assertEqual(self.state()["night_turns"], 1)
        self.assertEqual(self.state()["body_presents"], state["body_presents"])
        self.assertEqual(self.state()["gc_presents"], state["gc_presents"])
        self.assertEqual(self.state()["refresh_wave"], 5)
        self.assert_reader_dark_margins()

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

    def test_reader_night_period_and_menu_return(self):
        self.open_fixture_reader()
        self.reader_display_settings()
        self.preview.command("tap 300 676")
        # 默认5切到3：10、14、20、30、关、3。
        # Cycle the fresh default 5 to 3 through 10, 14, 20, 30, off and 3.
        for _ in range(6):
            self.preview.command("tap 300 900")
        self.preview.command("tap 424 1140")
        for direct in (False, True):
            if direct:
                self.reader_display_settings()
                self.preview.command("tap 300 1006")
                self.preview.command("tap 424 1140")
            state = self.state()
            self.assertEqual(state["night_turns"], 0)
            for turn in range(1, 7):
                self.preview.command("key 2" if turn % 2 else "key 0")
                self.settle()
                self.assertEqual(self.state()["night_turns"], turn % 3)
                self.assertEqual(self.state()["gc_presents"], state["gc_presents"] + turn // 3)
                self.assertEqual(self.state()["physical_clears"], state["physical_clears"] + turn // 3)
                self.assertEqual(self.state()["refresh_mode"], 2 if turn % 3 == 0 else 5)
                self.assert_reader_dark_margins()
                if direct:
                    self.assertEqual(set(self.reader_pixels()), {0, 255})
            self.preview.command("key 2")
            self.assertEqual(self.state()["night_turns"], 1)
            before = self.reader_pixels()
            cleans = self.state()["gc_presents"]
            self.preview.command("menu")
            self.preview.command("tap 650 1150")
            self.assertFalse(self.state()["menu"])
            self.assertEqual(self.state()["gc_presents"], cleans + 2)
            self.assertEqual(self.state()["night_turns"], 0)
            self.assertEqual(self.reader_pixels(), before)

    def test_reader_night_disabled_cleaning_and_overlay(self):
        self.open_fixture_reader()
        self.reader_display_settings()
        self.preview.command("tap 300 676")
        # 默认5经10/14/20/30到关闭。
        # Cycle the fresh default 5 through 10/14/20/30 to off.
        for _ in range(5):
            self.preview.command("tap 300 900")
        self.preview.command("tap 424 1140")
        cleans = self.state()["gc_presents"]
        for _ in range(6):
            for key in (2, 0):
                self.preview.command(f"key {key}")
                self.settle()
                self.assertEqual(self.state()["gc_presents"], cleans)
                self.assertEqual(self.state()["night_turns"], 0)
        body = self.reader_pixels()
        self.preview.command("hold 180 432")
        self.assertNotEqual(self.reader_pixels(), body)
        self.assert_reader_dark_margins()
        self.assertEqual(self.state()["refresh_wave"], 5)
        self.assertEqual(self.state()["gc_presents"], cleans)
        self.preview.command("tap 190 974")
        self.assertEqual(self.state()["gc_presents"], cleans + 1)
        self.assertEqual(self.reader_pixels(), body)

    def test_reader_night_illustrated_chapter_retains_gray(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")
        self.settle()
        self.preview.command("tap 300 650")
        self.settle()
        self.assertTrue(self.state()["reading"])
        self.reader_display_settings()
        self.preview.command("tap 424 1140")
        day_body = self.reader_pixels()
        self.reader_display_settings()
        self.preview.command("tap 300 676")
        self.preview.command("tap 300 1006")
        self.preview.command("tap 424 1140")
        night_body = self.reader_pixels()
        self.assertEqual(night_body, bytes(255 - value for value in day_body))
        self.assertGreater(len(set(night_body)), 2)
        self.preview.command("key 2")
        self.settle()
        # 插图章节在直刷设置下仍使用灰阶。
        # Illustrated chapters retain gray in direct settings.
        self.assertEqual(self.state()["refresh_wave"], 5)
        self.assertEqual(self.state()["night_turns"], 1)
        self.assertGreater(len(set(self.reader_pixels())), 2)
        self.assert_reader_dark_margins()

    def test_reader_night_image_placeholder_preview(self):
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
        self.reader_display_settings()
        self.preview.command("tap 300 676")
        self.preview.command("tap 424 1140")
        night_body = self.reader_pixels()
        self.assertEqual(night_body, bytes(255 - value for value in day_body))
        state = self.state()
        self.preview.command("tap 330 300")
        self.assertEqual(self.reader_pixels(), bytes(255 - value for value in day_image))
        self.assert_reader_dark_margins()
        self.assertEqual(self.state()["refresh_wave"], 5)
        self.assertEqual(self.state()["night_turns"], 0)
        self.assertEqual(self.state()["body_presents"], state["body_presents"])
        self.assertEqual(self.state()["night_area_presents"], state["night_area_presents"] + 1)
        self.assertEqual(self.state()["gc_presents"], state["gc_presents"])
        self.preview.command("tap 300 1140")
        self.assertEqual(self.reader_pixels(), night_body)
        self.assertEqual(self.state()["gc_presents"], state["gc_presents"] + 1)

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
        for which, name in enumerate(("loading", "lock")):
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
        self.preview.command("tap 342 224")
        self.preview.command("tap 300 1006")
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
        for x, y in ((320, 604), (200, 820)):
            self.preview.command("fixture 1")
            self.settle()
            self.preview.command(f"tap {x} {y}")
            settled = self.settle()
            self.assertEqual(settled["page"], self.indices["app_book"])
            self.assertNotEqual(settled["page"], self.indices["app_reading"])

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
        # 同页请求不重新初始化；离页后重入才重新检测卡状态。
        # Same-page requests do not initialize again; leave and reenter to reprobe card state.
        self.page("app_os_settings")
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
        self.preview.command("tap 300 900")  # 行辅助线 / guide rule (row5 y852-948)
        solid = self.preview.png
        self.assertNotEqual(relaxed, solid)
        self.preview.command("tap 424 1140")  # 返回阅读（底栏）/ return via the bottom bar
        reader = self.preview.png
        self.assertNotEqual(solid, reader)
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.preview.command("tap 342 224")  # 翻页与显示 / turns and display
        normal = self.preview.png
        self.preview.command("tap 300 676")  # 夜间开关 / night switch (row9 y628-724)
        night = self.preview.png
        self.assertNotEqual(normal, night)
        self.preview.command("tap 300 630")
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
        self.preview.command("tap 320 638")
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
        self.preview.command("tap 320 638")
        self.assertEqual(configured, account_band(), "Account configuration must survive reader entry")

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
        self.preview.command("tap 300 452")
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

    def test_reader_direct_profile_and_return_to_gray(self):
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.preview.command("tap 342 224")
        standard = self.preview.png
        self.preview.command("tap 300 1006")
        self.assertNotEqual(standard, self.preview.png)
        self.preview.command("tap 424 1140")
        pixels = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
        self.assertEqual(set(pixels), {0, 255}, "Direct entry commits the same binary target as turns")
        cleans = self.state()["gc_presents"]
        for _ in range(6):
            for key in (2, 0):
                self.preview.command(f"key {key}")
                self.settle()
                self.assertEqual(self.state()["refresh_mode"], 5)
                self.assertEqual(self.state()["gc_presents"], cleans)
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
        self.preview.command("tap 342 224")
        self.preview.command("tap 300 1006")
        self.assertEqual(standard, self.preview.png)
        self.preview.command("tap 424 1140")
        self.preview.command("key 0")
        self.settle()
        self.assertEqual(self.state()["refresh_mode"], 5)
        pixels = self.preview.frame.read_bytes()[len(b"P5\n684 1216\n255\n"):]
        self.assertGreater(len(set(pixels)), 2)

    def test_reader_crossmux_selection_gestures_persist_without_changing_day(self):
        self.open_fixture_reader()
        day = self.reader_pixels()
        self.reader_display_settings()
        current = self.preview.png
        presents = self.state()["presents"]
        # 绘制和命中共用48像素按钮边界，邻接空隙不得切换方案或刷新。
        # Paint and hit testing share the 48-pixel bounds; adjacent gaps must neither switch profiles nor refresh.
        for command in ("tap 39 1072", "tap 644 1072", "tap 300 1047", "tap 10 1096"):
            self.preview.command(command)
            self.assertEqual(self.preview.png, current)
            self.assertEqual(self.state()["presents"], presents)
        for command in ("hold 300 1072", "swipe 300 1072 300 940"):
            self.preview.command(command)
            self.assertEqual(self.preview.png, current)
        self.preview.command("tap 40 1048")
        selected = self.preview.png
        self.assertNotEqual(selected, current)
        crossmux_presents = self.state()["crossmux_presents"]
        self.preview.command("tap 424 1140")
        self.assertEqual(self.reader_pixels(), day)
        self.assertGreater(len(set(self.reader_pixels())), 2)
        self.preview.command("key 2")
        self.settle()
        self.assertEqual(self.state()["refresh_mode"], 5)
        self.assertEqual(self.state()["refresh_wave"], 7)
        self.assertEqual(self.state()["crossmux_presents"], crossmux_presents)
        self.page("app_os_home")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        self.reader_display_settings()
        self.assertEqual(self.preview.png, selected)
        self.preview.command("tap 643 1095")
        self.assertNotEqual(self.preview.png, current)  # 新黑基准档 / New black-baseline profile
        self.preview.command("tap 300 1072")
        self.assertEqual(self.preview.png, current)

    def test_reader_direct_fine_edges_keep_du_and_page_geometry(self):
        self.open_fixture_reader()
        self.reader_display_settings()
        self.preview.command("tap 300 1006")
        self.preview.command("tap 424 1140")
        body = lambda pixels: pixels[:1090 * 684]
        original = self.reader_pixels()
        self.assertEqual(set(original), {0, 255})
        self.reader_display_settings()
        self.preview.command("tap 480 1006")
        fine_menu = self.preview.png
        self.preview.command("tap 424 1140")
        fine = self.reader_pixels()
        self.assertEqual(set(fine), {0, 255})
        self.assertNotEqual(fine, original)
        self.preview.command("key 2")
        self.settle()
        self.assertEqual(self.state()["refresh_wave"], 6)
        self.assertEqual(set(self.reader_pixels()), {0, 255})
        self.preview.command("key 0")
        self.settle()
        self.assertEqual(body(self.reader_pixels()), body(fine))
        self.page("app_os_home")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        self.assertEqual(body(self.reader_pixels()), body(fine))
        self.reader_display_settings()
        self.assertEqual(self.preview.png, fine_menu)
        self.preview.command("tap 480 1006")
        self.preview.command("tap 424 1140")
        self.assertEqual(body(self.reader_pixels()), body(original))
        self.reader_display_settings()
        self.preview.command("tap 300 1006")
        self.preview.command("tap 424 1140")
        gray = self.reader_pixels()
        self.assertGreater(len(set(gray)), 2)
        self.reader_display_settings()
        self.preview.command("tap 480 1006")
        self.preview.command("tap 424 1140")
        self.assertEqual(self.reader_pixels(), gray, "Fine mode does not change standard gray")

    def test_reader_black_baseline_night_clean_points_keep_gray_and_du(self):
        self.open_fixture_reader()
        self.reader_display_settings()
        self.preview.command("tap 300 1072")
        self.preview.command("tap 300 1072")
        self.preview.command("tap 300 676")
        for _ in range(6):
            self.preview.command("tap 300 900")  # 周期3页 / Three-page interval
        before = self.state()
        self.preview.command("tap 424 1140")
        self.assertEqual(self.state()["black_baseline_action"], 1)
        self.assertEqual(self.state()["gc_presents"], before["gc_presents"] + 2)
        self.assertEqual(self.state()["physical_clears"], before["physical_clears"])
        self.assertGreater(len(set(self.reader_pixels())), 2)
        cleans = self.state()["gc_presents"]
        for turn in range(1, 4):
            self.preview.command("key 2")
            self.settle()
            self.assertEqual(self.state()["black_baseline_action"], 0)
            self.assertEqual(self.state()["gc_presents"], cleans + (2 if turn == 3 else 0))
        self.reader_display_settings()
        self.preview.command("tap 300 1006")
        self.preview.command("tap 424 1140")
        self.assertEqual(set(self.reader_pixels()), {0, 255})
        self.preview.command("key 2")
        self.settle()
        self.assertEqual(self.state()["refresh_wave"], 6)
        self.reader_display_settings()
        cleans = self.state()["gc_presents"]
        self.preview.command("tap 150 1140")
        self.assertEqual(self.state()["black_baseline_action"], 2)
        self.assertEqual(self.state()["gc_presents"], cleans + 2)
        self.assertEqual(set(self.reader_pixels()), {0, 255})

    def test_reader_crossmux_night_standard_uses_binary_turns_and_single_clean(self):
        self.open_fixture_reader()
        self.reader_display_settings()
        self.preview.command("tap 300 1072")
        self.preview.command("tap 300 676")
        for _ in range(6):
            self.preview.command("tap 300 900")
        before = self.state()
        self.preview.command("tap 424 1140")
        self.assertEqual(self.state()["refresh_mode"], 5)
        self.assertEqual(self.state()["gc_presents"], before["gc_presents"])
        self.assertEqual(self.state()["physical_clears"], before["physical_clears"])
        self.assertEqual(self.state()["night_turns"], 0)
        self.assertEqual(self.state()["crossmux_action"], 1)
        self.assertEqual(self.state()["crossmux_presents"], before["crossmux_presents"] + 1)
        self.assertEqual(self.state()["refresh_wave"], 8)
        self.assertEqual(set(self.reader_pixels()), {0, 255})
        self.assert_reader_dark_margins()
        state = self.state()
        for turn in range(1, 7):
            self.preview.command("key 2" if turn % 2 else "key 0")
            self.settle()
            self.assertEqual(self.state()["refresh_mode"], 2 if turn % 3 == 0 else 1)
            self.assertEqual(self.state()["night_turns"], turn % 3)
            self.assertEqual(self.state()["gc_presents"], state["gc_presents"] + turn // 3)
            self.assertEqual(self.state()["physical_clears"], state["physical_clears"])
            self.assertEqual(self.state()["crossmux_action"], 0)
            self.assertEqual(self.state()["crossmux_presents"], state["crossmux_presents"] + turn)
            self.assertEqual(self.state()["refresh_wave"], 8)
            self.assertEqual(set(self.reader_pixels()), {0, 255})
        self.preview.command("key 2")
        self.assertEqual(self.state()["night_turns"], 1)
        night_body = self.reader_pixels()
        self.reader_display_settings()
        before = self.state()
        self.preview.command("tap 164 1140")
        self.assertEqual(self.state()["gc_presents"], before["gc_presents"] + 1)
        self.assertEqual(self.state()["physical_clears"], before["physical_clears"])
        self.assertEqual(self.state()["night_turns"], 0)
        self.assertEqual(self.state()["crossmux_action"], 2)
        self.assertEqual(self.reader_pixels(), night_body)
        # 切回当前方案恢复原标准灰阶与入口物理清理；选择未被对照模式改写。
        # Returning to current restores standard grayscale and physical entry cleanup without changing the user's turn-effect choice.
        self.reader_display_settings()
        self.preview.command("tap 300 1072")
        self.preview.command("tap 300 1072")
        before = self.state()
        self.preview.command("tap 424 1140")
        self.assertEqual(self.state()["physical_clears"], before["physical_clears"] + 1)
        self.assertEqual(self.state()["gc_presents"], before["gc_presents"] + 1)
        self.assertGreater(len(set(self.reader_pixels())), 2)
        self.assert_reader_dark_margins()

    def test_reader_crossmux_illustrated_chapter_excludes_comparison(self):
        self.preview.command("fixture 1")
        self.preview.command("tap 220 1140")
        self.settle()
        self.preview.command("tap 300 650")
        self.settle()
        self.assertTrue(self.state()["reading"])
        self.reader_display_settings()
        self.preview.command("tap 424 1140")
        day_body = self.reader_pixels()
        self.reader_display_settings()
        self.preview.command("tap 300 1072")
        self.preview.command("tap 300 676")
        crossmux_presents = self.state()["crossmux_presents"]
        self.preview.command("tap 424 1140")
        self.assertEqual(self.reader_pixels(), bytes(255 - value for value in day_body))
        self.assertGreater(len(set(self.reader_pixels())), 2)
        self.assertEqual(self.state()["crossmux_presents"], crossmux_presents)
        self.preview.command("key 2")
        self.settle()
        self.assertEqual(self.state()["refresh_mode"], 5)
        self.assertEqual(self.state()["refresh_wave"], 5)
        self.assertEqual(self.state()["night_turns"], 1)
        self.assertGreater(len(set(self.reader_pixels())), 2)
        self.assertEqual(self.state()["crossmux_presents"], crossmux_presents)

    def test_reader_crossmux_footer_and_controls_keep_gray_without_counting_turns(self):
        self.page("app_os_reading")
        self.preview.command("tap 300 900")
        self.preview.command("tap 300 966")
        self.page("app_os_home")
        self.open_fixture_reader()
        self.reader_display_settings()
        self.preview.command("tap 300 1072")
        self.preview.command("tap 300 676")
        self.preview.command("tap 424 1140")
        self.preview.command("key 2")
        self.settle()
        state = self.state()
        self.assertEqual(state["night_turns"], 1)
        self.preview.command("time_step 60")
        self.assertEqual(self.state()["refresh_mode"], 5)
        self.assertEqual(self.state()["refresh_wave"], 5)
        self.assertEqual(self.state()["night_turns"], 1)
        self.assertEqual(self.state()["body_presents"], state["body_presents"])
        self.assertEqual(self.state()["gc_presents"], state["gc_presents"])
        self.assertEqual(self.state()["night_area_presents"], state["night_area_presents"] + 1)
        self.assertEqual(self.state()["crossmux_presents"], state["crossmux_presents"])
        self.preview.command("key 1")
        self.preview.command("tap 544 944")
        self.preview.command("time_step 2")
        self.assertEqual(self.state()["refresh_mode"], 5)
        self.assertEqual(self.state()["refresh_wave"], 5)
        self.assertEqual(self.state()["night_turns"], 1)
        self.assertEqual(self.state()["body_presents"], state["body_presents"])
        self.assertEqual(self.state()["gc_presents"], state["gc_presents"])
        self.assertEqual(self.state()["crossmux_presents"], state["crossmux_presents"])
        before = self.state()
        self.preview.command("key 1")
        self.assertEqual(self.state()["refresh_mode"], 5)
        self.assertEqual(self.state()["night_turns"], 0)
        self.assertEqual(self.state()["physical_clears"], before["physical_clears"])
        self.assertEqual(self.state()["gc_presents"], before["gc_presents"])
        self.assertEqual(self.state()["crossmux_action"], 1)
        self.assertEqual(self.state()["crossmux_presents"], before["crossmux_presents"] + 1)
        self.assertEqual(set(self.reader_pixels()), {0, 255})

    def test_reader_binary_guides_keep_grid_through_turns(self):
        self.preview.command("fixture 1")
        self.settle()
        self.preview.command("tap 400 425")
        self.settle()
        self.preview.command("key 1")
        self.preview.command("tap 136 1038")
        self.preview.command("tap 300 900")
        self.preview.command("tap 342 224")
        self.preview.command("tap 300 1006")
        self.preview.command("tap 424 1140")
        for night in (False, True):
            if night:
                self.preview.command("key 1")
                self.preview.command("tap 136 1038")
                self.preview.command("tap 342 224")
                self.preview.command("tap 300 676")
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
                ink = 255 if night else 0
                rules = [y for y in range(88, 1088)
                         if set(pixels[y * 684 + 40:y * 684 + 644]) == {ink}]
                self.assertTrue(rules, "Binary quantization must preserve visible guide rules")
                self.assertTrue(all((y - 24) % 72 in (69, 70) for y in rules))

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
