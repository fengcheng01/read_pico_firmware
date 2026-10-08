/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：TXT 与 EPUB 单实例书源分派，不依赖界面。
 * English: Singleton TXT and EPUB source dispatch without UI dependencies.
 * 冻结：TXT 保留源字节偏移，EPUB 采用累计 spine HTML 字节；兼容纯文本加载，不修改源文件。
 * 用户反馈后允许跨关闭保留一章和元数据，关闭文件并在复用前校验目录。
 * Frozen: TXT retains source offsets; EPUB uses cumulative spine HTML bytes; retain plain-text loading and never modify source files.
 * User feedback permits a chapter/metadata cache across close, with handles closed and directory validation before reuse.
 */
#include "book_source_internal.h"
#include "book_epub.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
static book_txt_t s_book;
static book_epub_t *s_epub;

// 保留最近 EPUB 目录及一章，离页关闭文件；重新进入须验证中央目录。
// Retain the last EPUB metadata/chapter, closing files on exit and validating the directory on re-entry.
static book_epub_t* s_cached_epub;
static char s_cached_path[512];
static size_t s_cache_chapter = SIZE_MAX;
static html_text_t s_cache;
static bool blocks_deep_copy(const html_text_t* src, html_text_t* dst) {
    memset(dst, 0, sizeof(*dst));
    size_t bytes = src->len + 1 + src->count * sizeof(blk_t);
    for (size_t b = 0; b < src->count; ++b) {
        const blk_t* block = &src->blocks[b];
        if (block->image_src) bytes += strlen(block->image_src) + 1;
        if (block->image) bytes += (size_t)block->image_width * block->image_height;
    }
    if (bytes > 2U * 1024U * 1024U) return false;
    dst->utf8 = malloc(src->len + 1);
    dst->blocks = src->count ? calloc(src->count, sizeof(blk_t)) : NULL;
    if (!dst->utf8 || (src->count && !dst->blocks)) goto fail;
    dst->len = src->len;
    memcpy(dst->utf8, src->utf8, src->len + 1);
    for (size_t b = 0; b < src->count; ++b) {
        const blk_t* block = &src->blocks[b];
        blk_t* copy = &dst->blocks[b];
        *copy = *block;
        copy->image_src = NULL; copy->image = NULL;
        dst->count = b + 1;
        if (block->image_src) {
            copy->image_src = malloc(strlen(block->image_src) + 1);
            if (!copy->image_src) goto fail;
            strcpy(copy->image_src, block->image_src);
        }
        if (block->image) {
            size_t pixels = (size_t)block->image_width * block->image_height;
            copy->image = malloc(pixels);
            if (!copy->image) goto fail;
            memcpy(copy->image, block->image, pixels);
        }
    }
    return true;
fail:
    html_text_free(dst);
    return false;
}
static void cache_store(size_t i, const html_text_t* text) {
    html_text_free(&s_cache); s_cache_chapter = SIZE_MAX;
    html_text_t copy;
    if (!blocks_deep_copy(text, &copy)) return;
    s_cache = copy;
    s_cache_chapter = i;
}
esp_err_t book_open(const char *path) {
    book_close();
    if (!path || !*path) return ESP_ERR_INVALID_ARG;
    if (s_cached_epub && !strcmp(path, s_cached_path) && book_epub_resume(s_cached_epub, path)) {
        s_epub = s_cached_epub; s_cached_epub = NULL; return ESP_OK;
    }
    book_epub_close(s_cached_epub); s_cached_epub = NULL;
    html_text_free(&s_cache); s_cache_chapter = SIZE_MAX;
    snprintf(s_cached_path, sizeof(s_cached_path), "%s", path);
    const char *ext = strrchr(path, '.');
    if (ext && !strcasecmp(ext, ".epub")) return book_epub_open(path, &s_epub);
    if (!ext || strcasecmp(ext, ".txt")) return ESP_ERR_NOT_SUPPORTED;
    esp_err_t err = book_txt_open(&s_book, path);
    if (err != ESP_OK) book_close();
    return err;
}
void book_close(void) {
    if (s_epub) {
        book_epub_close(s_cached_epub);
        book_epub_suspend(s_epub);
        s_cached_epub = s_epub; s_epub = NULL;
    }
    if (s_book.file) fclose(s_book.file);
    free(s_book.entries); memset(&s_book, 0, sizeof(s_book));
}
size_t book_chapter_count(void) { return s_epub ? book_epub_chapter_count(s_epub) : s_book.count; }
esp_err_t book_chapter_title(size_t i, char *buf, size_t cap) {
    if (s_epub) return book_epub_chapter_title(s_epub, i, buf, cap);
    if (!buf || !cap || i >= s_book.count) return ESP_ERR_INVALID_ARG;
    size_t n = strlen(s_book.entries[i].title);
    if (n >= cap) { buf[0] = 0; return ESP_ERR_INVALID_SIZE; }
    memcpy(buf, s_book.entries[i].title, n + 1); return ESP_OK;
}
esp_err_t book_chapter_load(size_t i, char **utf8, size_t *len) {
    if (!utf8 || !len) return ESP_ERR_INVALID_ARG;
    *utf8 = NULL; *len = 0;
    if (s_epub) {
        html_text_t text = {0}; esp_err_t err = book_epub_load(s_epub, i, &text);
        if (err != ESP_OK) return err;
        *utf8 = text.utf8; *len = text.len; text.utf8 = NULL;
        html_text_free(&text); return ESP_OK;
    }
    if (!s_book.file || i >= s_book.count) return ESP_ERR_INVALID_ARG;
    return book_txt_load(&s_book, i, utf8, len);
}
esp_err_t book_chapter_load_blocks(size_t i, html_text_t *out) {
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    if (s_epub) {
        // 命中深拷贝缓存：直接返回，跳过解压与解析。/ Deep-copy cache hit skips decompression and parsing.
        if (i == s_cache_chapter && s_cache.blocks) {
            if (blocks_deep_copy(&s_cache, out)) return ESP_OK;
            // 拷贝失败（内存不足）退回正常加载。/ On copy failure fall back to a normal load.
        }
        esp_err_t err = book_epub_load(s_epub, i, out);
        if (err != ESP_OK) return err;
        cache_store(i, out);
        return ESP_OK;
    }
    return book_chapter_load(i, &out->utf8, &out->len);
}
esp_err_t book_chapter_load_image(size_t i, const char *reference, uint8_t **pixels, uint16_t *width, uint16_t *height) {
    if (!pixels || !width || !height) return ESP_ERR_INVALID_ARG;
    *pixels = NULL; *width = *height = 0;
    if (!s_epub) return ESP_ERR_NOT_SUPPORTED;
    return book_epub_load_image(s_epub, i, reference, pixels, width, height);
}
void book_chapter_load_inline_images(size_t chapter, html_text_t* text) {
    if (!s_epub || !text || !text->blocks) return;
    size_t images = 0;
    for (size_t b = 0; b < text->count; ++b) if (text->blocks[b].image_src) ++images;
    if (!images) return;
    // 均分章节预算，图片多时降分辨率；原文偏移及资源引用保持不变。
    // Share the chapter budget, lowering resolution for image-heavy chapters; retain text offsets and resource references.
    size_t count = images > 64 ? 64 : images;
    size_t budget = (768u * 1024u) / count;
    for (size_t b = 0, tried = 0; b < text->count && tried < count; ++b) {
        blk_t* block = &text->blocks[b];
        if (!block->image_src) continue;
        ++tried;
        if (block->image) continue;
        (void)book_epub_load_image_budget(s_epub, chapter, block->image_src, budget,
                                         &block->image, &block->image_width, &block->image_height);
    }
    cache_store(chapter, text);
}
bool book_cached(const char* path) { return path && s_cached_epub && !strcmp(path, s_cached_path); }
uint32_t book_total_bytes(void) { return s_epub ? book_epub_total_bytes(s_epub) : s_book.total; }
uint32_t book_chapter_byte_offset(size_t i) {
    return s_epub ? book_epub_chapter_byte_offset(s_epub, i) : i < s_book.count ? s_book.entries[i].offset : 0;
}
book_kind_t book_kind(void) { return s_epub ? BOOK_KIND_EPUB : BOOK_KIND_TXT; }
