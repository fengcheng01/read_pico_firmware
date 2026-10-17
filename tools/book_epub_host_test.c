/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：EPUB 容器、spine、目录与正文集成测试。
 * English: EPUB container, spine, navigation and text integration tests.
 * 冻结：仅用于主机测试。/ Frozen: Host tests only.
 */
#include "book_epub.h"
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#ifdef BOOK_HEAP_TRACK
void book_heap_reset(void);
void book_heap_report(void);
#endif
extern int book_epub_cache_hits;  // 解析结构缓存命中计数 / structure-cache hit counter
int main(int argc, char **argv) {
    assert(argc > 1);
    bool inspect = !strcmp(argv[1], "--inspect");
    for (int a = inspect ? 2 : 1; a < argc; ++a) {
#ifdef BOOK_HEAP_TRACK
        book_heap_reset();
#endif
        book_epub_t *book = NULL;
        if (strstr(argv[a], "/bad_")) {
            assert(book_epub_open(argv[a], &book) != ESP_OK && !book);
            printf("epub rejection passed: %s\n", argv[a]); continue;
        }
        assert(book_epub_open(argv[a], &book) == ESP_OK && book);
        size_t count = book_epub_chapter_count(book);
        bool large = strstr(argv[a], "good_large") != NULL;
        bool many = strstr(argv[a], "good_many") != NULL;
        bool limit = strstr(argv[a], "good_spine_limit") != NULL;
        bool previous_limit = strstr(argv[a], "good_spine_2048") != NULL;
        if (!inspect) assert(count == (many ? 30000u : large ? 1486u : limit ? 32768u : previous_limit ? 2048u : 4u));
        assert(count);
        uint32_t previous = 0;
        size_t readable = 0, image_refs = 0, repeated_images = 0, title_images = 0;
        for (size_t i = 0; i < count; ++i) {
            char title[160]; html_text_t text = {0};
            assert(book_epub_chapter_title(book, i, title, sizeof(title)) == ESP_OK);
            assert(title[0]);
            if (strstr(argv[a], "good_many_titled")) {
                char expected[80]; snprintf(expected, sizeof(expected), "Chapter %zu", i);
                assert(!strcmp(title, expected));
            }
            else if (strstr(argv[a], "good_defaults") || large || many) assert(!strncmp(title, "第 ", strlen("第 ")));
            // 无目录正文夹具在补目扫描前保持回退编号；扫描效果由 book_source 侧断言。
            // The navigation-less body fixture keeps fallback numbering until the TOC
            // scan runs; the scan itself is asserted by the book_source side.
            else if (!inspect && !strstr(argv[a], "_body")) assert(strncmp(title, "第 ", strlen("第 ")));
            if (strstr(argv[a], "good_paths") && i < 2) assert(!strcmp(title, i ? "Child Two" : "Parent & One"));
            if (strstr(argv[a], "good_navfallback")) assert(!strncmp(title, "NAV ", 4));
            uint32_t offset = book_epub_chapter_byte_offset(book, i);
            assert(i ? offset > previous : offset == 0); previous = offset;
            assert(book_epub_load(book, i, &text) == ESP_OK);
            for (size_t b = 0; b < text.count; ++b) if (text.blocks[b].image_src) {
                assert(!text.blocks[b].image);
                ++image_refs;
                if (text.blocks[b].image_title) ++title_images;
                if (text.blocks[b].image_repeated) {
                    assert(text.blocks[b].image_first_chapter < i);
                    ++repeated_images;
                }
            }
            if (text.len) {
                assert(text.utf8 && text.blocks && text.count);
                ++readable;
            } else assert(inspect);
            if (strstr(argv[a], "good_paths") && i == 1) {
                char position[512]; uint16_t chapter; uint32_t byte;
                assert(text.count > 1);
                assert(book_epub_sync_encode(argv[a], i, text.blocks[1].offset + 3, position, sizeof(position)));
                assert(!strcmp(position, "/body/DocFragment[2]/body/div[1]/text()[1].1"));
                assert(book_epub_sync_decode(argv[a], position, &chapter, &byte));
                assert(chapter == i && byte == text.blocks[1].offset + 3);
                // 独立同步读取不破坏已打开书源。/ Independent sync reads preserve the open book source.
                assert(book_epub_chapter_count(book) == count);
            }
            html_text_free(&text);
        }
        assert(book_epub_total_bytes(book) > previous);
        assert(readable);
        if (!inspect) {
            // 解析结构缓存：重开必命中（超限压力书允许跳过），章节数/总字节一致；
            // 常规规模双书并持逐章比对偏移与标题，万章书先关再开以免双份解析堆超限。
            // Structure cache: reopening must hit (oversized stress books may skip)
            // with matching counts and totals; regular sizes hold both books and
            // compare offsets and titles per chapter, while ten-thousand-chapter
            // fixtures close before reopening so double parse heaps stay bounded.
            bool held = count <= 4096;
            int hits = book_epub_cache_hits;
            if (!held) { book_epub_close(book); book = NULL; }
            book_epub_t *again = NULL;
            assert(book_epub_open(argv[a], &again) == ESP_OK && again);
            assert(book_epub_cache_hits == hits + (count <= 20000 ? 1 : 0));
            assert(book_epub_chapter_count(again) == count);
            if (held) {
                assert(book_epub_total_bytes(again) == book_epub_total_bytes(book));
                for (size_t i = 0; i < count; ++i) {
                    char t1[160], t2[160];
                    assert(book_epub_chapter_title(book, i, t1, sizeof(t1)) == ESP_OK);
                    assert(book_epub_chapter_title(again, i, t2, sizeof(t2)) == ESP_OK);
                    assert(!strcmp(t1, t2));
                    assert(book_epub_chapter_byte_offset(again, i) == book_epub_chapter_byte_offset(book, i));
                }
                book_epub_close(again);
            } else {
                book = again;  // 接力持有，后续流程（补目）继续有实例 / relay the instance
            }
            // 无目录正文夹具：补目回写后重开仍命中且标题保留。
            // The navigation-less body fixture: after the TOC rewrite a further
            // open still hits and keeps the titles.
            if (strstr(argv[a], "_body")) {
                book_epub_prepare_toc(book);
                hits = book_epub_cache_hits;
                book_epub_t *third = NULL;
                assert(book_epub_open(argv[a], &third) == ESP_OK && third);
                assert(book_epub_cache_hits == hits + 1);
                assert(book_epub_chapter_count(third) == count);
                for (size_t i = 0; i < count; ++i) {
                    char t1[160], t3[160];
                    assert(book_epub_chapter_title(book, i, t1, sizeof(t1)) == ESP_OK);
                    assert(book_epub_chapter_title(third, i, t3, sizeof(t3)) == ESP_OK);
                    assert(!strcmp(t1, t3));
                }
                book_epub_close(third);
            }
        }
        book_epub_close(book); printf("epub fixture passed: %s (%zu chapters, %zu with text)\n", argv[a], count, readable);
        if (image_refs) printf("image placeholders=%zu, repeated=%zu, title images=%zu, automatic decodes=0\n", image_refs, repeated_images, title_images);
#ifdef BOOK_HEAP_TRACK
        book_heap_report();
#endif
    }
    puts("epub host tests passed");
}
