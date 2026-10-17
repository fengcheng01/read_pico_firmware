/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：以真实文件验证书源解析。
 * English: Exercise source parsing with real files.
 * 冻结：仅供主机验证。/ Frozen: Host validation only.
 */
#include "book_source.h"
#include "gbk.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *path = "/tmp/book-source-test.txt";
static void write_bytes(const char *s, size_t n) {
    FILE *f = fopen(path, "wb"); assert(f); assert(fwrite(s, 1, n, f) == n); fclose(f);
}
static void roundtrip(const char *s, size_t n) {
    size_t seen = 0; char *out; size_t len;
    assert(book_open(path) == ESP_OK); assert(book_total_bytes() == n);
    for (size_t i = 0; i < book_chapter_count(); ++i) {
        assert(book_chapter_byte_offset(i) == seen);
        assert(book_chapter_load(i, &out, &len) == ESP_OK);
        assert(len <= 96 * 1024); assert(memcmp(out, s + seen, len) == 0);
        assert(out[len] == 0); seen += len; free(out);
    }
    assert(seen == n); book_close();
}
int main(int argc, char **argv) {
    const char *s = "Chapter 1 Start\nhello\nChapter 2 End\nworld\n";
    write_bytes(s, strlen(s)); assert(book_open(path) == ESP_OK);
    assert(book_chapter_count() == 2); char title[41];
    assert(book_chapter_title(1, title, sizeof(title)) == ESP_OK);
    assert(strcmp(title, "Chapter 2 End") == 0); book_close(); roundtrip(s, strlen(s));
    s = "第一章 开始\n你好\n第二章 结束\n再见\n";
    write_bytes(s, strlen(s)); assert(book_open(path) == ESP_OK); assert(book_chapter_count() == 2); book_close();
    char *large = malloc(300000); assert(large);
    for (size_t i = 0; i < 100000; ++i) memcpy(large + i * 3, "中", 3);
    write_bytes(large, 300000); roundtrip(large, 300000); free(large);
    const char gbk[] = { (char)0xd6,(char)0xd0,(char)0xce,(char)0xc4,'\n' };
    write_bytes(gbk, sizeof(gbk)); assert(book_open(path) == ESP_OK);
    char *out; size_t len; assert(book_chapter_load(0, &out, &len) == ESP_OK);
    assert(strcmp(out, "中文\n") == 0); free(out); book_close();
    char tiny[5] = {0};
    assert(gbk_to_utf8(gbk, sizeof(gbk), tiny, 4) == 3); assert(strcmp(tiny, "中") == 0);
    assert(gbk_to_utf8("\x81", 1, tiny, sizeof(tiny)) == 3); assert(strcmp(tiny, "\xef\xbf\xbd") == 0);
    s = "\xef\xbb\xbf" "Chapter 1\r\nx\xff\nChapter 2\r\ny";
    write_bytes(s, strlen(s)); assert(book_open(path) == ESP_OK); assert(book_chapter_count() == 2);
    assert(book_chapter_load(0, &out, &len) == ESP_OK);
    assert(strcmp(out, "Chapter 1\r\nx\xef\xbf\xbd\n") == 0); free(out);
    assert(book_chapter_title(0, tiny, sizeof(tiny)) == ESP_ERR_INVALID_SIZE);
    assert(book_chapter_load(99, &out, &len) == ESP_ERR_INVALID_ARG); assert(!out && !len); book_close();
    large = malloc(70000); assert(large); memset(large, 'a', 65535);
    memcpy(large + 65535, "中\n", 4); write_bytes(large, 65539); roundtrip(large, 65539);
    large[65535] = (char)0xd6; large[65536] = (char)0xd0; large[65537] = '\n';
    write_bytes(large, 65538); assert(book_open(path) == ESP_OK);
    assert(book_chapter_load(0, &out, &len) == ESP_OK); assert(len == 65539);
    assert(!memcmp(out + 65535, "中\n", 4)); free(out); free(large); book_close();
    FILE *f = fopen(path, "wb"); assert(f);
    for (size_t i = 0; i < 2200; ++i) fprintf(f, "Chapter %zu\nbody\n", i);
    fclose(f); assert(book_open(path) == ESP_OK); assert(book_chapter_count() <= 2048); book_close();
    s = "preface\nChapter 1\nbody\nChapter 2\nend";
    write_bytes(s, strlen(s)); roundtrip(s, strlen(s));
    // 部名+章号同行（“明朝那些事儿”式）：空白分隔的干净部名前缀可识别。
    // Part name plus chapter on one line (as in “明朝那些事儿”): a clean part
    // prefix separated by whitespace is recognized.
    s = "洪武大帝 引子\n引子正文\n洪武大帝 第一章 童年\n正文\n永乐大帝 第二章 苦难\n正文\n";
    write_bytes(s, strlen(s)); assert(book_open(path) == ESP_OK);
    assert(book_chapter_count() == 2);
    assert(book_chapter_title(0, title, sizeof(title)) == ESP_OK && !strcmp(title, "洪武大帝 第一章 童年"));
    assert(book_chapter_title(1, title, sizeof(title)) == ESP_OK && !strcmp(title, "永乐大帝 第二章 苦难"));
    book_close(); roundtrip(s, strlen(s));
    // 17k 导出格式：“N-M 标题　[本章字数：…最新更新时间…]”长尾不否决标记行，标题截到标记前。
    // The 17k export: “N-M title　[本章字数：… latest…]” — long tails never reject the
    // marker line and titles cut before the marker.
    s = " \t1-1 王者归来　[本章字数：3665　最新更新时间：2010-01-23 00:29:00.0]\n正文。\n 1-2 医院逞威　[本章字数：4175　最新更新时间：2010-04-06 18:47:46.0]\n正文。\n";
    write_bytes(s, strlen(s)); assert(book_open(path) == ESP_OK);
    assert(book_chapter_count() == 2);
    assert(book_chapter_title(0, title, sizeof(title)) == ESP_OK && !strcmp(title, "1-1 王者归来"));
    assert(book_chapter_title(1, title, sizeof(title)) == ESP_OK && !strcmp(title, "1-2 医院逞威"));
    book_close(); roundtrip(s, strlen(s));
    // 正文里的行内章号不算题头：第 前无空白或前缀含标点都拒绝。
    // Inline chapter mentions in body text are not headings: no whitespace before
    // 第 or a punctuated prefix both reject.
    s = "他说第一章很重要\n正文\n第二章 结束\n再见\n";
    write_bytes(s, strlen(s)); assert(book_open(path) == ESP_OK);
    assert(book_chapter_count() == 1);
    book_close();
    s = "根据安全法，第二章第十九条如下\n正文\n";
    write_bytes(s, strlen(s)); assert(book_open(path) == ESP_OK);
    assert(book_chapter_count() == 1);
    book_close();
    // 前导空白的章号仍识别（单章会走既有的 count<2 回退，用两章验证）。
    // Leading whitespace before a chapter marker still works (a lone heading takes
    // the pre-existing count<2 fallback, so this uses two).
    s = "  第一章 开始\n正文\n  第二章 结束\n再见\n";
    write_bytes(s, strlen(s)); assert(book_open(path) == ESP_OK);
    assert(book_chapter_count() == 2);
    assert(book_chapter_title(0, title, sizeof(title)) == ESP_OK && !strcmp(title, "第一章 开始"));
    book_close();
    // RickyOS 合并项：〇数字、第/数字/单位间空格、英文 book/part+罗马数字、Markdown 标题与围栏。
    // RickyOS merges: the 〇 numeral, spaces around the numeral run, English
    // book/part plus Roman numerals, Markdown headings with fences.
    s = "第一百〇八章 转折\n正文\n第 一百一十 章 收束\n正文\n";
    write_bytes(s, strlen(s)); assert(book_open(path) == ESP_OK);
    assert(book_chapter_count() == 2);
    assert(book_chapter_title(0, title, sizeof(title)) == ESP_OK && !strcmp(title, "第一百〇八章 转折"));
    book_close(); roundtrip(s, strlen(s));
    s = "Book 1 The Beginning\nbody\nPart II Rising\nbody\nChapter IV Storm\nbody\n";
    write_bytes(s, strlen(s)); assert(book_open(path) == ESP_OK);
    assert(book_chapter_count() == 3);
    assert(book_chapter_title(2, title, sizeof(title)) == ESP_OK && !strcmp(title, "Chapter IV Storm"));
    book_close(); roundtrip(s, strlen(s));
    s = "# 序章\n\n正文\n\n```\n## 围栏内的伪标题\n```\n\n## 第一章 之外\n正文\n";
    write_bytes(s, strlen(s)); assert(book_open(path) == ESP_OK);
    assert(book_chapter_count() == 2);
    assert(book_chapter_title(0, title, sizeof(title)) == ESP_OK && !strcmp(title, "序章"));
    assert(book_chapter_title(1, title, sizeof(title)) == ESP_OK && !strcmp(title, "第一章 之外"));
    book_close(); roundtrip(s, strlen(s));
    // 严格编码检测：UTF-8 文件单字节损坏不再整本误判 GBK。
    // Strict detection: one stray byte in a UTF-8 file no longer flips it to GBK.
    s = "第一章 完好\nutf8 \xe6\x96\x87\xe5\xad\x97 ok\x80\n第二章 完好\n";
    write_bytes(s, strlen(s)); assert(book_open(path) == ESP_OK);
    assert(book_chapter_count() == 2);
    assert(book_chapter_load(0, &out, &len) == ESP_OK);
    assert(strstr(out, "utf8 \xe6\x96\x87\xe5\xad\x97 ok\xef\xbf\xbd") != NULL);
    free(out); book_close();
    write_bytes("", 0); assert(book_open(path) == ESP_OK); assert(book_chapter_count() == 1); book_close();
    assert(book_open("/tmp/missing.epub") == ESP_ERR_NOT_FOUND);
    assert(book_open("/tmp/missing.unknown") == ESP_ERR_NOT_SUPPORTED);
    assert(book_chapter_count() == 0);
    for (int a = 1; a < argc; ++a) {
        assert(book_open(argv[a]) == ESP_OK); assert(book_chapter_count() > 0);
        bool epub = strstr(argv[a], ".epub") != NULL;
        assert(book_kind() == (epub ? BOOK_KIND_EPUB : BOOK_KIND_TXT));
        size_t sum = 0;
        for (size_t i = 0; i < book_chapter_count(); ++i) {
            assert(book_chapter_load(i, &out, &len) == ESP_OK); assert(out[len] == 0);
            html_text_t blocks = {0}; assert(book_chapter_load_blocks(i, &blocks) == ESP_OK);
            assert(blocks.len == len && !memcmp(blocks.utf8, out, len));
            assert(epub ? blocks.blocks != NULL && blocks.count > 0 : blocks.blocks == NULL && blocks.count == 0);
            html_text_free(&blocks);
            assert(book_chapter_title(i, title, sizeof(title)) == ESP_OK);
            sum += len; free(out);
        }
        // TXT 目录补扫是无操作，不改变章节。/ TXT TOC preparation is a no-op and never renames chapters.
        if (!epub) book_prepare_toc();
        printf("fixture %s: %zu chapters, %zu UTF-8 bytes\n", argv[a], book_chapter_count(), sum);
        book_close();
        assert(book_kind() == BOOK_KIND_TXT && book_chapter_count() == 0);
    }
    // 无目录书的正文补目：h1/h2/段落编号题头在随后正文出现后生效；只有连续编号行的章节保持未命名。
    // Body-heading fallback for navigation-less books: h1/h2/paragraph numbered titles apply once
    // prose follows; a chapter of only consecutive numbered lines stays unnamed.
    const char *body = NULL, *authored = NULL;
    for (int a = 1; a < argc; ++a) {
        if (strstr(argv[a], "_body.epub")) body = argv[a];
        if (strstr(argv[a], "_ncx.epub")) authored = argv[a];
    }
    assert(body && authored);
    assert(book_open(body) == ESP_OK); assert(book_chapter_count() == 4);
    assert(book_chapter_title(0, title, sizeof(title)) == ESP_OK && !strcmp(title, "第 1 节"));
    book_prepare_toc();
    assert(book_chapter_title(0, title, sizeof(title)) == ESP_OK && !strcmp(title, "第一章 河边清晨"));
    assert(book_chapter_title(1, title, sizeof(title)) == ESP_OK && !strcmp(title, "Chapter II 雨后小路"));
    assert(book_chapter_title(2, title, sizeof(title)) == ESP_OK && !strcmp(title, "第三章 山间书屋"));
    assert(book_chapter_title(3, title, sizeof(title)) == ESP_OK && !strcmp(title, "第 4 节"));
    book_prepare_toc();
    assert(book_chapter_title(3, title, sizeof(title)) == ESP_OK && !strcmp(title, "第 4 节"));
    book_close();
    // 有正式目录的书补扫后标题不被改写。/ Authored-TOC books keep their titles after preparation.
    assert(book_open(authored) == ESP_OK);
    assert(book_chapter_title(0, title, sizeof(title)) == ESP_OK && !strcmp(title, "第一章 河边清晨"));
    book_prepare_toc();
    assert(book_chapter_title(0, title, sizeof(title)) == ESP_OK && !strcmp(title, "第一章 河边清晨"));
    book_close();
    puts("book source host tests passed"); return 0;
}
