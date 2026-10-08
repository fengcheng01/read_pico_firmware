/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：独立 TF 卡缓存的有界读取、校验、临时提交及容量淘汰。
 * English: Bounded SD cache reads, validation, temporary commits and capacity eviction.
 * 冻结：不写内置 Flash、不修改原书或进度；完整身份与校验通过后才复用。
 * Frozen: Never write internal flash, source books or progress; reuse only after identity and checksum validation.
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "book_cache.h"
#include "book_store.h"
#include "esp_heap_caps.h"
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef BOOK_CACHE_MEDIA_ROOT
#define BOOK_CACHE_MEDIA_ROOT "/sdcard"
#endif
#define CACHE_DIR BOOK_CACHE_MEDIA_ROOT "/.read-pico-cache"
#define CACHE_HEADER 52U
#define CACHE_PAYLOAD_MAX (100U * 1024U)
#define CACHE_BYTES_MAX (8U * 1024U * 1024U)
#define CACHE_ITEMS_MAX 128U
#define CACHE_SCAN_MAX 512U
#define CACHE_PATH_MAX (BOOK_STORE_PATH_MAX + 64U)

static int cache_stat(const char* path, struct stat* st) {
#ifdef ESP_PLATFORM
    return stat(path, st);
#else
    // FAT 没有符号链接；主机缓存不得通过链接改写其他文件。
    // FAT has no symlinks; host caches must not follow links to rewrite unrelated files.
    return lstat(path, st);
#endif
}

static uint32_t crc_update(uint32_t crc, const void* data, size_t size) {
    static uint32_t table[256];
    static bool ready;
    if (!ready) {
        for (unsigned n = 0; n < 256; ++n) {
            uint32_t value = n;
            for (unsigned bit = 0; bit < 8; ++bit)
                value = (value >> 1) ^ (UINT32_C(0xedb88320) & (0U - (value & 1U)));
            table[n] = value;
        }
        ready = true;
    }
    const uint8_t* p = data;
    while (size--) crc = table[(crc ^ *p++) & 255U] ^ (crc >> 8);
    return crc;
}
static uint64_t hash_update(uint64_t hash, const void* data, size_t size) {
    const uint8_t* p = data;
    while (size--) hash = (hash ^ *p++) * UINT64_C(1099511628211);
    return hash;
}
void book_cache_fingerprint_begin(book_cache_identity_t* id) {
    *id = (book_cache_identity_t){.digest = UINT64_C(14695981039346656037)};
}
void book_cache_fingerprint_update(book_cache_identity_t* id, const void* data, size_t size) {
    id->digest = hash_update(id->digest, data, size);
    id->crc = ~crc_update(~id->crc, data, size);
    id->bytes += size;
}
bool book_cache_fingerprint_file(FILE* file, book_cache_identity_t* id) {
    if (!file || !id || fseek(file, 0, SEEK_SET)) return false;
    clearerr(file);
    uint8_t* bytes = heap_caps_malloc(64U * 1024U, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!bytes) return false;
    book_cache_fingerprint_begin(id);
    size_t got;
    while ((got = fread(bytes, 1, 64U * 1024U, file))) book_cache_fingerprint_update(id, bytes, got);
    bool ok = !ferror(file);
    free(bytes);
    clearerr(file);
    return fseek(file, 0, SEEK_SET) == 0 && ok;
}
static void put16(uint8_t* p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void put32(uint8_t* p, uint32_t v) { for (unsigned n = 0; n < 4; ++n) p[n] = v >> (8 * n); }
static void put64(uint8_t* p, uint64_t v) { for (unsigned n = 0; n < 8; ++n) p[n] = v >> (8 * n); }
static uint16_t get16(const uint8_t* p) { return p[0] | ((uint16_t)p[1] << 8); }
static uint32_t get32(const uint8_t* p) { uint32_t v = 0; for (unsigned n = 0; n < 4; ++n) v |= (uint32_t)p[n] << (8 * n); return v; }
static uint64_t get64(const uint8_t* p) { uint64_t v = 0; for (unsigned n = 0; n < 8; ++n) v |= (uint64_t)p[n] << (8 * n); return v; }
static bool paths(const char* source, book_cache_kind_t kind, char out[CACHE_PATH_MAX]) {
    size_t prefix = strlen(BOOK_CACHE_MEDIA_ROOT);
    if (!source || strlen(source) >= BOOK_STORE_PATH_MAX || strncmp(source, BOOK_CACHE_MEDIA_ROOT, prefix) ||
        source[prefix] != '/' || kind < BOOK_CACHE_COVER || kind > BOOK_CACHE_TXT_TOC) return false;
    const char* part = source + prefix + 1;
    for (const char* at = part;; ++at) {
        if (((unsigned char)*at < 32 && *at) || *at == 127 || *at == '\\' || *at == ':') return false;
        if (*at && *at != '/') continue;
        size_t len = at - part;
        if (!len || (len == 1 && part[0] == '.') || (len == 2 && part[0] == '.' && part[1] == '.')) return false;
        if (!*at) break;
        part = at + 1;
    }
    uint64_t hash = hash_update(UINT64_C(14695981039346656037), source, strlen(source));
    int n = snprintf(out, CACHE_PATH_MAX, CACHE_DIR "/%016" PRIx64 "-%u.bin", hash, (unsigned)kind);
    return n > 0 && n < (int)CACHE_PATH_MAX;
}
bool book_cache_exists(const char* source, book_cache_kind_t kind) {
    char path[CACHE_PATH_MAX]; struct stat st;
    return paths(source, kind, path) && cache_stat(path, &st) == 0 && S_ISREG(st.st_mode);
}
bool book_cache_load(const char* source, book_cache_kind_t kind, const book_cache_identity_t* id,
                     size_t limit, void** payload, size_t* size) {
    if (!payload || !size) return false;
    *payload = NULL; *size = 0;
    char path[CACHE_PATH_MAX];
    if (!id || !paths(source, kind, path)) return false;
    struct stat st;
    if (cache_stat(CACHE_DIR, &st) || !S_ISDIR(st.st_mode) || cache_stat(path, &st) || !S_ISREG(st.st_mode)) return false;
    FILE* file = fopen(path, "rb");
    if (!file) return false;
    uint8_t h[CACHE_HEADER]; char original[BOOK_STORE_PATH_MAX]; void* data = NULL;
    bool ok = false;
    if (fread(h, 1, sizeof(h), file) != sizeof(h) || memcmp(h, "RPBCACHE", 8) || get16(h + 8) != 1 ||
        get16(h + 10) != kind || get32(h + 12) != id->variant || get64(h + 16) != id->bytes ||
        get64(h + 24) != id->digest || get32(h + 32) != id->crc || get16(h + 42) ||
        get32(h + 48) != ~crc_update(UINT32_MAX, h, 48)) goto done;
    size_t bytes = get32(h + 36), name = get16(h + 40);
    if (!bytes || bytes > limit || bytes > CACHE_PAYLOAD_MAX || name != strlen(source) || name >= sizeof(original) ||
        fread(original, 1, name, file) != name || memcmp(original, source, name)) goto done;
    data = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!data || fread(data, 1, bytes, file) != bytes || fgetc(file) != EOF || ferror(file)) goto done;
    uint32_t crc = crc_update(UINT32_MAX, original, name);
    if (get32(h + 44) != ~crc_update(crc, data, bytes)) goto done;
    *payload = data; *size = bytes; data = NULL; ok = true;
done:
    free(data);
    fclose(file);
    return ok;
}

// 只淘汰本格式普通缓存；临时文件不参与命中，恢复时可直接丢弃。
// Evict only regular files owned by this format; interrupted temporaries never count as cache hits.
static bool owned_name(const char* name, bool* temporary) {
    if (strlen(name) != 22 || name[16] != '-' || (name[17] != '1' && name[17] != '2') || name[18] != '.') return false;
    for (unsigned i = 0; i < 16; ++i)
        if (!(name[i] >= '0' && name[i] <= '9') && !(name[i] >= 'a' && name[i] <= 'f')) return false;
    *temporary = !strcmp(name + 19, "tmp");
    return *temporary || !strcmp(name + 19, "bin");
}
static bool reserve_space(const char* target, size_t wanted) {
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        DIR* dir = opendir(CACHE_DIR);
        if (!dir) return false;
        uint64_t total = 0; unsigned count = 0, scanned = 0;
        char oldest[CACHE_PATH_MAX] = {0}; time_t age = 0;
        struct dirent* entry;
        bool ok = true;
        while ((entry = readdir(dir))) {
            if (++scanned > CACHE_SCAN_MAX) { ok = false; break; }
            bool temporary;
            if (!owned_name(entry->d_name, &temporary)) continue;
            char path[CACHE_PATH_MAX]; struct stat st;
            int n = snprintf(path, sizeof(path), CACHE_DIR "/%s", entry->d_name);
            if (n < 0 || n >= (int)sizeof(path) || cache_stat(path, &st) || !S_ISREG(st.st_mode)) continue;
            if (temporary) { (void)unlink(path); continue; }
            if (!strcmp(path, target)) continue;
            ++count; total += st.st_size > 0 ? (uint64_t)st.st_size : 0;
            if (!oldest[0] || st.st_mtime < age) { strcpy(oldest, path); age = st.st_mtime; }
        }
        closedir(dir);
        if (!ok) return false;
        if (count < CACHE_ITEMS_MAX && total + wanted <= CACHE_BYTES_MAX) return true;
        if (!oldest[0] || unlink(oldest)) return false;
    }
    return false;
}
bool book_cache_save(const char* source, book_cache_kind_t kind, const book_cache_identity_t* id,
                     const void* payload, size_t size) {
    char path[CACHE_PATH_MAX], temporary[CACHE_PATH_MAX]; struct stat st;
    if (!id || !payload || !size || size > CACHE_PAYLOAD_MAX || !paths(source, kind, path)) return false;
    if (mkdir(CACHE_DIR, 0777) && errno != EEXIST) return false;
    if (cache_stat(CACHE_DIR, &st) || !S_ISDIR(st.st_mode) ||
        !reserve_space(path, CACHE_HEADER + strlen(source) + size)) return false;
    strcpy(temporary, path); memcpy(temporary + strlen(temporary) - 3, "tmp", 3);
    if (cache_stat(temporary, &st) == 0 || errno != ENOENT) return false;
    uint8_t h[CACHE_HEADER] = {0};
    memcpy(h, "RPBCACHE", 8); put16(h + 8, 1); put16(h + 10, kind); put32(h + 12, id->variant);
    put64(h + 16, id->bytes); put64(h + 24, id->digest); put32(h + 32, id->crc);
    put32(h + 36, size); put16(h + 40, strlen(source));
    uint32_t crc = crc_update(UINT32_MAX, source, strlen(source));
    put32(h + 44, ~crc_update(crc, payload, size)); put32(h + 48, ~crc_update(UINT32_MAX, h, 48));
    FILE* file = fopen(temporary, "wb");
    if (!file) return false;
    bool ok = fwrite(h, 1, sizeof(h), file) == sizeof(h) &&
        fwrite(source, 1, strlen(source), file) == strlen(source) && fwrite(payload, 1, size, file) == size;
    if (ok) ok = fflush(file) == 0 && fsync(fileno(file)) == 0;
    if (fclose(file)) ok = false;
    // FAT 不保证覆盖重命名；移除旧缓存后中断只造成 miss，不暴露半成品。
    // FAT need not replace by rename; interruption after removing the old cache only causes a miss.
    if (ok && unlink(path) && errno != ENOENT) ok = false;
    if (ok) ok = rename(temporary, path) == 0;
    if (!ok) (void)unlink(temporary);
    return ok;
}
