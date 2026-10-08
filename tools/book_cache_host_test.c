/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：真实缓存/TXT/ZIP/封面链路及文件故障回归，缓存 syscall 仅注入写失败。
 * English: Real cache/TXT/ZIP/cover regressions, with cache syscalls injecting write failures only.
 * 冻结：只操作 runner 创建的临时媒体；不模拟显示光学。
 * Frozen: Operate only on runner-created temporary media; never simulate panel optics.
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "book_cache.h"
#include "book_source_internal.h"
#include "book_cover.h"
#include "book_image.h"

static unsigned writes;
static bool fail_write, fail_sync, fail_rename;
static size_t cache_test_write(const void* p, size_t size, size_t count, FILE* file) {
    ++writes;
    if (fail_write) { errno = ENOSPC; return 0; }
    return fwrite(p, size, count, file);
}
static int cache_test_sync(int fd) { if (fail_sync) { errno = EIO; return -1; } return fsync(fd); }
static int cache_test_rename(const char* from, const char* to) {
    if (fail_rename) { errno = EIO; return -1; }
    return rename(from, to);
}
#define fwrite cache_test_write
#define fsync cache_test_sync
#define rename cache_test_rename
#include "../main/book/book_cache.c"
#undef fwrite
#undef fsync
#undef rename

static unsigned decodes;
bool book_image_decode(const uint8_t* data, size_t size, size_t budget,
                       uint8_t** out, uint16_t* width, uint16_t* height) {
    (void)budget;
    assert(size == 16); ++decodes;
    *width = *height = 4; *out = malloc(16); assert(*out);
    memcpy(*out, data, 16); return true;
}
static void write_file(const char* path, const void* data, size_t size) {
    FILE* file = fopen(path, "wb"); assert(file);
    assert(fwrite(data, 1, size, file) == size); assert(fclose(file) == 0);
}
static void txt_close(book_txt_t* book) { fclose(book->file); free(book->entries); memset(book, 0, sizeof(*book)); }
static book_cache_identity_t file_identity(const char* path) {
    FILE* file = fopen(path, "rb"); assert(file);
    book_cache_identity_t id; assert(book_cache_fingerprint_file(file, &id)); fclose(file); return id;
}
static void check_txt(const char* path, bool hit) {
    book_txt_t book = {0}; unsigned before = writes;
    assert(book_txt_open(&book, path) == ESP_OK);
    assert(book.count == 2 && !book.gbk && book.bom == 0);
    assert(!strcmp(book.entries[0].title, "Chapter 1 First"));
    char* text = NULL; size_t bytes = 0;
    assert(book_txt_load(&book, 1, &text, &bytes) == ESP_OK);
    assert(strstr(text, "world") && text[bytes] == 0); free(text);
    assert(hit ? writes == before : writes > before);
    txt_close(&book);
}
static void check_cover(const char* path, bool hit, uint8_t shade) {
    unsigned before = decodes; uint8_t* gray = NULL;
    assert(book_cover_load(path, &gray) && gray);
    assert(hit ? decodes == before : decodes == before + 1);
    assert(gray[(BOOK_COVER_H / 2) * BOOK_COVER_W + BOOK_COVER_W / 2] == shade);
    free(gray);
}
static void generic(const char* source) {
    const char* sample = "ordinary cached payload";
    book_cache_identity_t id = file_identity(source); id.variant = 7;
    assert(book_cache_save(source, BOOK_CACHE_TXT_TOC, &id, sample, strlen(sample)));
    void* data = NULL; size_t bytes = 0;
    assert(book_cache_load(source, BOOK_CACHE_TXT_TOC, &id, 100, &data, &bytes));
    assert(bytes == strlen(sample) && !memcmp(sample, data, bytes)); free(data);
    book_cache_identity_t different = id; ++different.digest;
    assert(!book_cache_load(source, BOOK_CACHE_TXT_TOC, &different, 100, &data, &bytes));
    assert(!data && !bytes);
    different = id; ++different.variant;
    assert(!book_cache_load(source, BOOK_CACHE_TXT_TOC, &different, 100, &data, &bytes));
    assert(!book_cache_load(source, BOOK_CACHE_TXT_TOC, &id, 1, &data, &bytes));
    assert(!book_cache_save("/flash/books/book.txt", BOOK_CACHE_TXT_TOC, &id, sample, strlen(sample)));
    assert(!book_cache_save(BOOK_CACHE_MEDIA_ROOT "/../book.txt", BOOK_CACHE_TXT_TOC, &id, sample, strlen(sample)));
    char path[CACHE_PATH_MAX]; assert(paths(source, BOOK_CACHE_TXT_TOC, path));
    FILE* file = fopen(path, "r+b"); assert(file); assert(fseek(file, -1, SEEK_END) == 0);
    assert(fputc('!', file) != EOF); fclose(file);
    assert(!book_cache_load(source, BOOK_CACHE_TXT_TOC, &id, 100, &data, &bytes));
    assert(book_cache_save(source, BOOK_CACHE_TXT_TOC, &id, sample, strlen(sample)));
    fail_write = true;
    assert(!book_cache_save(source, BOOK_CACHE_TXT_TOC, &id, sample, strlen(sample))); fail_write = false;
    assert(book_cache_load(source, BOOK_CACHE_TXT_TOC, &id, 100, &data, &bytes)); free(data);
    fail_sync = true;
    assert(!book_cache_save(source, BOOK_CACHE_TXT_TOC, &id, sample, strlen(sample))); fail_sync = false;
    assert(book_cache_load(source, BOOK_CACHE_TXT_TOC, &id, 100, &data, &bytes)); free(data);
    fail_rename = true;
    assert(!book_cache_save(source, BOOK_CACHE_TXT_TOC, &id, sample, strlen(sample))); fail_rename = false;
    assert(!book_cache_load(source, BOOK_CACHE_TXT_TOC, &id, 100, &data, &bytes));
    char temp[CACHE_PATH_MAX]; strcpy(temp, path); memcpy(temp + strlen(temp) - 3, "tmp", 3);
    write_file(temp, "RPBCACHE", 8);
    assert(!book_cache_exists(source, BOOK_CACHE_TXT_TOC));
    assert(book_cache_save(source, BOOK_CACHE_TXT_TOC, &id, sample, strlen(sample)));
    assert(access(temp, F_OK) != 0);
    assert(rename(path, temp) == 0);
    assert(!book_cache_load(source, BOOK_CACHE_TXT_TOC, &id, 100, &data, &bytes));
    assert(book_cache_save(source, BOOK_CACHE_TXT_TOC, &id, sample, strlen(sample)));
    assert(symlink(source, temp) == 0);
    assert(!book_cache_save(source, BOOK_CACHE_TXT_TOC, &id, sample, strlen(sample)));
    assert(unlink(temp) == 0);
    assert(truncate(path, 20) == 0);
    assert(!book_cache_load(source, BOOK_CACHE_TXT_TOC, &id, 100, &data, &bytes));
}
static void corrupt_toc(const char* path) {
    book_cache_identity_t id = file_identity(path); id.variant = 1;
    uint8_t bad[12 + 45] = {0}; bad[8] = 1; bad[12] = 1;
    assert(book_cache_save(path, BOOK_CACHE_TXT_TOC, &id, bad, sizeof(bad)));
    check_txt(path, false);
}
static void quota(void) {
    uint8_t* payload = malloc(CACHE_PAYLOAD_MAX); assert(payload); memset(payload, 0x5a, CACHE_PAYLOAD_MAX);
    book_cache_identity_t id = {.bytes = 1, .digest = 1, .variant = 1};
    for (unsigned i = 0; i < 100; ++i) {
        char source[BOOK_STORE_PATH_MAX]; snprintf(source, sizeof(source), BOOK_CACHE_MEDIA_ROOT "/books/quota%u.txt", i);
        assert(book_cache_save(source, BOOK_CACHE_TXT_TOC, &id, payload, CACHE_PAYLOAD_MAX));
    }
    // 小条目单独触发数量上限，避免只覆盖字节预算。
    // Small entries separately exercise the item cap rather than only the byte budget.
    for (unsigned i = 100; i < 230; ++i) {
        char source[BOOK_STORE_PATH_MAX]; snprintf(source, sizeof(source), BOOK_CACHE_MEDIA_ROOT "/books/quota%u.txt", i);
        assert(book_cache_save(source, BOOK_CACHE_TXT_TOC, &id, payload, 16));
    }
    free(payload);
    DIR* dir = opendir(CACHE_DIR); assert(dir);
    uint64_t total = 0; unsigned count = 0; struct dirent* entry;
    while ((entry = readdir(dir))) {
        bool temporary; if (!owned_name(entry->d_name, &temporary)) continue;
        assert(!temporary);
        char path[CACHE_PATH_MAX]; snprintf(path, sizeof(path), CACHE_DIR "/%s", entry->d_name);
        struct stat st; assert(stat(path, &st) == 0 && S_ISREG(st.st_mode)); total += st.st_size; ++count;
    }
    closedir(dir); assert(total <= CACHE_BYTES_MAX && count == CACHE_ITEMS_MAX);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    char path[BOOK_STORE_PATH_MAX], cover[BOOK_STORE_PATH_MAX];
    snprintf(path, sizeof(path), BOOK_CACHE_MEDIA_ROOT "/books/sample.txt");
    snprintf(cover, sizeof(cover), BOOK_CACHE_MEDIA_ROOT "/books/cover.epub");
    if (!strcmp(argv[1], "cold")) { check_txt(path, false); check_cover(cover, false, 0x33); }
    else if (!strcmp(argv[1], "warm")) { check_txt(path, true); check_cover(cover, true, 0x33); }
    else if (!strcmp(argv[1], "replace")) {
        unsigned before = writes; book_txt_t book = {0}; assert(book_txt_open(&book, path) == ESP_OK);
        assert(!strcmp(book.entries[0].title, "Chapter 1 Other") && writes > before); txt_close(&book);
        check_cover(cover, false, 0x77);
    } else if (!strcmp(argv[1], "boundaries")) {
        generic(path);
        const char* text = "Chapter 1 First\nhello\nChapter 2 Last\nworld\n";
        write_file(path, text, strlen(text)); check_txt(path, false); check_txt(path, true); corrupt_toc(path);
        book_cache_identity_t before = file_identity(path);
        fail_write = true;
        text = "Chapter 1 First\nHELLO\nChapter 2 Last\nworld\n";
        write_file(path, text, strlen(text)); book_txt_t book = {0}; assert(book_txt_open(&book, path) == ESP_OK);
        assert(book.count == 2); txt_close(&book); fail_write = false;
        book_cache_identity_t after = file_identity(path); assert(before.digest != after.digest && before.bytes == after.bytes);
        const uint8_t gbk[] = {0xd6, 0xd0, '\n'};
        write_file(path, gbk, sizeof(gbk)); assert(book_txt_open(&book, path) == ESP_OK && book.gbk); txt_close(&book);
        assert(book_txt_open(&book, path) == ESP_OK && book.gbk); txt_close(&book);
        text = "\xef\xbb\xbf" "Chapter 1\nx\nChapter 2\ny\n";
        write_file(path, text, strlen(text)); assert(book_txt_open(&book, path) == ESP_OK && book.bom == 3); txt_close(&book);
        assert(book_txt_open(&book, path) == ESP_OK && book.bom == 3); txt_close(&book);
        write_file(path, "", 0); assert(book_txt_open(&book, path) == ESP_OK && book.count == 1); txt_close(&book);
        assert(book_txt_open(&book, path) == ESP_OK && book.count == 1); txt_close(&book);
        quota();
        assert(unlink(path) == 0); assert(book_txt_open(&book, path) == ESP_ERR_NOT_FOUND);
        uint8_t* gray = (uint8_t*)1; assert(unlink(cover) == 0); assert(!book_cover_load(cover, &gray) && !gray);
    } else assert(false);
    puts("book_cache: real source/cache path passed");
    return 0;
}
