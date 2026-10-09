/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：借用章节 UTF-8 文本，生成分页并绘制正文。
 * English: Paginate borrowed chapter UTF-8 text and draw its body.
 *
 * 冻结：不释放原文、不刷新屏幕；调用方持有字体绘制互斥锁。
 * Frozen: Never free source text or present the display; caller holds the font draw lock.
 * 用户确认仅翻页转场旧线穿字：纯文字辅助线启用稳定行网格，改变分页时保持文本字节锚点。
 * The user confirmed old rules cross text only during turns: text-only guides use a stable row grid; retain byte anchors when repaginating.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "epdiy.h"
#include "html_text.h"

/// 重排借用文本；空文一页，失败清空布局，超过 4096 页返回 false。/ Borrow and paginate; empty text has one page, failure clears layout, over 4096 pages fails.
bool book_layout_build(const char* utf8, size_t len, EpdRect rect, int px);
/// 借用块表；标题字号加8，块间单换行；原文与块表须存活至free。/ Borrow blocks; headings add 8 px, with one newline between blocks; text and blocks must outlive layout.
bool book_layout_build_blocks(const char* utf8, size_t len, const blk_t* blocks, size_t count, EpdRect rect, int px);
/// 初始化并只排前两页；后续由调用方分批推进，原文继续借用。/ Initialize the first two pages; caller advances later batches while retaining source ownership.
bool book_layout_begin_blocks(const char* utf8, size_t len, const blk_t* blocks, size_t count, EpdRect rect, int px);
/// 最多增加指定页数；失败清空布局，调用方必须停止阅读。/ Add at most the requested pages; failure clears layout and requires ending reading.
bool book_layout_extend(size_t pages);
/// 是否完成整章分页。/ Whether the whole chapter has been paginated.
bool book_layout_complete(void);
/// 释放页表，不释放原文。/ Free layout storage, never the borrowed text.
void book_layout_free(void);
/// 返回已完成页数；未建立布局时为零，分批排版时不是整章总页数。/ Return completed pages, zero without a layout; incremental counts are not the chapter total.
size_t book_layout_page_count(void);
/// 使用建立布局时的宽高与字号绘图；不匹配或越界时不绘制。/ Draw with the built dimensions and size; mismatches or invalid pages do nothing.
void book_layout_draw_page(uint8_t* fb, size_t page, EpdRect rect, int px);
/// 命中本页未加载图片占位，返回块索引及可选矩形；未命中为 SIZE_MAX；与绘制使用相同锁。
/// Hit an unloaded image placeholder, returning its block index and optional bounds; SIZE_MAX if absent; use the drawing lock.
size_t book_layout_image_at(size_t page, EpdRect rect, int x, int y, EpdRect* hit);
/// 查找字节偏移所属页，越界偏移夹到末页。/ Find page containing a byte offset; excessive offsets clamp to the last page.
size_t book_layout_page_for_offset(size_t off);
/// 返回页首偏移；已完成页数索引是待排页起点（完成后为文本末尾），更大索引返回文本长度。
/// Return a page start; index equal to completed count marks the pending page (EOF when complete), larger indexes return text length.
size_t book_layout_page_start_offset(size_t page);
/// 行距加成百分比（0..60），作用于后续 build 与 draw；改变后需重建分页。/ Extra leading percent (0..60) for later builds and draws; rebuild pagination after changing.
void book_layout_set_leading(int percent);
/// 夜间反色绘制（白字黑底、图片灰度翻转）；只影响 draw，不需要重建。/ Night-inverted drawing (white on black, flipped image grays); draw-only, no rebuild needed.
void book_layout_set_night(bool on);
/// 段首两字符缩进；影响 build，改变后需重建。/ Two-em paragraph indent; build-affecting, rebuild after changing.
void book_layout_set_indent(bool on);
/// 段落间距档 0=标准 1=加大；影响 build，改变后需重建。/ Paragraph gap tier 0=standard 1=relaxed; build-affecting, rebuild after changing.
void book_layout_set_paragraph(int tier);
/// 行辅助线 0=关 1=实线 2=虚线；开关须重建分页，实虚切换只影响绘制；含图片或占位的章节保持原图文布局。
/// Guide rules 0=off 1=solid 2=dashed; toggling off/on requires repagination, solid/dashed is draw-only; image/placeholder chapters keep their original layout.
void book_layout_set_guide(int style);
/// 纯文字辅助线网格的屏幕纵向原点；在建立布局前设置，改变后需重建；正文顶部变化仍对齐同一网格。
/// Screen-y origin for text-only guide grids; set before building and rebuild after changes; varying body tops align to the same grid.
void book_layout_set_guide_origin(int y);
/// 辅助线用日间黑/夜间白以适配真黑白直刷；否则用原灰阶，只影响绘制。/ Use day-black/night-white guides for binary direct; otherwise retain gray guides; draw-only.
void book_layout_set_guide_contrast(bool binary);
/// 纯文字直刷的原始覆盖率细边，绘制时选黑白字形；度量、页表及图片保持。
/// Raw-coverage fine edges for text direct choose binary glyph drawing only; retain metrics, pagination and illustrations.
void book_layout_set_direct_fine(bool on);
/// 正文对齐 0=左 1=居中 2=两端对齐；只影响 draw（断行不变），不需要重建。/ Body alignment 0=left 1=center 2=justified; draw-only (line breaks unchanged), no rebuild needed.
void book_layout_set_align(int align);

/// 命中实际正文字符，空白/图片返回 SIZE_MAX；与绘制共用锁。/ Hit actual body glyphs; whitespace/images return SIZE_MAX; share the drawing lock.
size_t book_layout_text_at(size_t page, EpdRect rect, int x, int y);
