/*
 * SPDX-FileCopyrightText: 2026 mindreset
 *
 * 中文：图片查看与锁屏壁纸共用的 TF 图片装载、居中裁切与 16 级有序抖动绘制。
 * English: Shared TF image loading, center-cropping and 16-level ordered-dither
 * drawing for the image viewer and lock wallpaper.
 * 冻结修订：用户要求减少重复打开等待；最多八份TF成品缓存，校验失败/写入失败回退，不更改原图。
 * Frozen revision: Reduce repeated-open waits with at most eight SD previews; failed checks/writes fall back without modifying originals.
 */
#include "ui_image.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "book_image.h"
#include "esp_heap_caps.h"
#include "ui_dither.h"

// 流式解码后字节上限只防异常文件；普通照片大小不再受 2 MiB 限制（对齐解码端）。
// After streaming decode the byte cap only guards pathological files; ordinary photos
// are no longer limited to 2 MiB (aligned with the decoder).
#define UI_IMAGE_FILE_MAX (50u * 1024u * 1024u)
#define UI_IMAGE_BUDGET (684u * 1096u)

void ui_image_auto_levels(uint8_t* gray, size_t count) {
    if (!gray || count < 64) return;
    // 0.5%/99.5% 百分位拉伸：照片对比度在 16 级灰上明显改善；已满幅（跨度≥248）或
    // 近乎单色（跨度<24，含纯色图）不动，避免放大噪点或破坏示意图形。
    // Percentile stretch at 0.5%/99.5%: photo contrast reads far better across 16 grays;
    // full-range images (span>=248) and near-mono ones (span<24, including flat art) stay
    // untouched to avoid amplifying noise or distorting diagrams.
    unsigned hist[256] = {0};
    for (size_t i = 0; i < count; ++i) ++hist[gray[i]];
    size_t low_at = count / 200, high_at = count - count / 200;
    size_t seen = 0;
    int low = 0, high = 255;
    for (int v = 0; v < 256; ++v) {
        seen += hist[v];
        if (seen > low_at) { low = v; break; }
    }
    seen = 0;
    for (int v = 255; v >= 0; --v) {
        seen += hist[v];
        if (seen > count - high_at) { high = v; break; }
    }
    if (high - low < 24 || high - low >= 248) return;
    for (size_t i = 0; i < count; ++i) {
        int v = gray[i];
        v = v <= low ? 0 : v >= high ? 255 : (v - low) * 255 / (high - low);
        gray[i] = (uint8_t)v;
    }
}

static bool image_extension_ok(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return false;
    return !strcasecmp(ext, ".png") || !strcasecmp(ext, ".jpg") || !strcasecmp(ext, ".jpeg");
}

// 查看成品使用八个固定槽，坏缓存只回退解码，不改原图；版本随像素处理变化递增。
// Eight fixed preview slots fall back on corruption without changing originals; bump the version when pixel processing changes.
#define PREVIEW_DIR UI_IMAGE_MEDIA_ROOT "/.pagerest-images"
#define PREVIEW_MAGIC 0x31565250u
typedef struct {
    uint32_t magic, version, digest;
    uint16_t width, height;
    int64_t size, mtime;
    char source[192];
} preview_header_t;
static uint32_t preview_digest(const uint8_t* data, size_t n) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < n; ++i) hash = (hash ^ data[i]) * 16777619u;
    return hash;
}
static uint32_t preview_checksum(const preview_header_t* input, const uint8_t* data, size_t n) {
    preview_header_t header; memcpy(&header, input, sizeof(header)); header.digest = 0;
    uint32_t hash = preview_digest((const uint8_t*)&header, sizeof(header));
    for (size_t i = 0; i < n; ++i) hash = (hash ^ data[i]) * 16777619u;
    return hash;
}
static void preview_path(const char* source, char path[192]) {
    unsigned slot = preview_digest((const uint8_t*)source, strlen(source)) % 8;
    snprintf(path, 192, PREVIEW_DIR "/%u.gray", slot);
}
static bool preview_load(const char* source, const struct stat* st, uint8_t** gray, uint16_t* width, uint16_t* height) {
    if (strlen(source) >= sizeof(((preview_header_t*)0)->source)) return false;
    char path[192]; preview_path(source, path);
    FILE* f = fopen(path, "rb"); if (!f) return false;
    preview_header_t h = {0}; uint8_t* data = NULL; bool ok = false;
    if (fread(&h, 1, sizeof(h), f) != sizeof(h) || h.magic != PREVIEW_MAGIC || h.version != 1 ||
        h.size != st->st_size || h.mtime != st->st_mtime || !memchr(h.source, 0, sizeof(h.source)) ||
        strcmp(source, h.source) || !h.width || !h.height || (size_t)h.width * h.height > UI_IMAGE_BUDGET) goto done;
    size_t n = (size_t)h.width * h.height;
    data = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!data || fread(data, 1, n, f) != n || fgetc(f) != EOF || ferror(f) || preview_checksum(&h, data, n) != h.digest) goto done;
    *gray = data; *width = h.width; *height = h.height; data = NULL; ok = true;
done:
    free(data); fclose(f); return ok;
}
static void preview_store(const char* source, const struct stat* st, const uint8_t* gray, uint16_t width, uint16_t height) {
    if (strlen(source) >= sizeof(((preview_header_t*)0)->source)) return;
    if (mkdir(PREVIEW_DIR, 0755) && errno != EEXIST) return;
    char path[192], part[208]; preview_path(source, path); snprintf(part, sizeof(part), "%s.part", path);
    preview_header_t h;
    memset(&h, 0, sizeof(h));
    h.magic = PREVIEW_MAGIC; h.version = 1; h.width = width; h.height = height;
    h.size = st->st_size; h.mtime = st->st_mtime; strcpy(h.source, source);
    h.digest = preview_checksum(&h, gray, (size_t)width * height);
    FILE* f = fopen(part, "wb"); if (!f) return;
    bool ok = fwrite(&h, 1, sizeof(h), f) == sizeof(h) &&
        fwrite(gray, 1, (size_t)width * height, f) == (size_t)width * height;
    if (fclose(f)) ok = false;
    struct stat now;
    if (stat(source, &now) || now.st_size != st->st_size || now.st_mtime != st->st_mtime) ok = false;
    if (ok && rename(part, path) && (errno == EEXIST || errno == ENOTEMPTY)) {
        // FAT不覆盖既有目标时只替换自有槽；中断最多丢缓存，不丢原图。
        // When FAT cannot replace an existing target, replace only our slot; interruption can lose cache, never the source.
        if (!remove(path)) (void)rename(part, path);
    }
    (void)remove(part);
}

bool ui_image_load_path(const char* path, uint8_t** gray, uint16_t* width, uint16_t* height) {
    if (gray) *gray = NULL;
    if (width) *width = 0;
    if (height) *height = 0;
    if (!path || !gray || !width || !height || strncmp(path, UI_IMAGE_MEDIA_ROOT "/", sizeof(UI_IMAGE_MEDIA_ROOT)) || !image_extension_ok(path))
        return false;
    struct stat st;
    if (stat(path, &st) || st.st_size <= 0 || (uint64_t)st.st_size > UI_IMAGE_FILE_MAX) return false;
    if (preview_load(path, &st, gray, width, height)) return true;
    // 压缩数据由解码器直接从 FILE* 流式读取，这里不再整文件驻留内存。
    // The decoder streams compressed data straight from FILE*; no whole-file buffer here.
    bool ok = book_image_decode_file(path, UI_IMAGE_BUDGET, gray, width, height);
    if (ok) {
        ui_image_auto_levels(*gray, (size_t)*width * *height);
        preview_store(path, &st, *gray, *width, *height);
    }
    return ok;
}

void ui_image_blit(uint8_t* fb, const uint8_t* gray, uint16_t width, uint16_t height, EpdRect area) {
    if (!fb || !gray || !width || !height || area.width <= 0 || area.height <= 0) return;
    // 居中裁切窗口与目标同比例，铺满不拉伸。/ The centered crop window matches the
    // target aspect so the area fills without stretching.
    uint32_t crop_w = width, crop_h = height;
    if ((uint64_t)crop_w * area.height > (uint64_t)crop_h * area.width) {
        crop_w = (uint32_t)(((uint64_t)crop_h * area.width + area.height - 1) / area.height);
        if (!crop_w) crop_w = 1;
    } else {
        crop_h = (uint32_t)(((uint64_t)crop_w * area.height + area.width - 1) / area.width);
        if (!crop_h) crop_h = 1;
    }
    uint32_t crop_x = ((uint32_t)width - crop_w) / 2;
    uint32_t crop_y = ((uint32_t)height - crop_h) / 2;
    // 每列映射只算一次，避免每像素重复执行64位除法。
    // Map each column once instead of repeating a64-bit division for every pixel.
    uint16_t columns[684];
    if (area.width > 684) return;
    for (int x = 0; x < area.width; ++x)
        columns[x] = crop_x + (uint32_t)((uint64_t)x * crop_w / area.width);
    for (int y = 0; y < area.height; ++y) {
        uint32_t sy = crop_y + (uint32_t)((uint64_t)y * crop_h / area.height);
        const uint8_t* row = gray + (size_t)sy * width;
        for (int x = 0; x < area.width; ++x)
            epd_draw_pixel(area.x + x, area.y + y,
                           ui_image_dither(row[columns[x]], area.x + x, area.y + y), fb);
    }
}

bool ui_image_draw_path(uint8_t* fb, const char* path, EpdRect area) {
    if (!fb || area.x < 0 || area.y < 0 || area.width < 8 || area.height < 8) return false;
    uint8_t* gray = NULL;
    uint16_t width = 0, height = 0;
    if (!ui_image_load_path(path, &gray, &width, &height)) return false;
    ui_image_blit(fb, gray, width, height, area);
    free(gray);
    return true;
}

// ---- 壁纸侧车 / Wallpaper sidecar ----

// 快照头：源身份不符、载荷不齐都按未命中处理，绝不部分绘制。
// Snapshot header: identity mismatch or short payload counts as a miss, never partial draws.
#define WALLPAPER_CACHE_MAGIC 0x31504d57u  // "WMP1"
typedef struct {
    uint32_t magic;
    uint32_t path_hash;
    long size;
    long mtime;
    uint16_t width, height;
} wallpaper_cache_header_t;

static uint32_t wallpaper_path_hash(const char* path) {
    // FNV-1a 32 位；仅作身份比对，不做安全用途。
    // 32-bit FNV-1a; identity comparison only, never a security use.
    uint32_t hash = 2166136261u;
    for (const unsigned char* p = (const unsigned char*)path; *p; ++p) {
        hash ^= *p;
        hash *= 16777619u;
    }
    return hash;
}

bool ui_image_wallpaper_cache_store(const char* cache_path, const char* source_path,
                                    long size, long mtime, const uint8_t* gray, uint16_t width, uint16_t height) {
    if (!cache_path || !source_path || !gray || !width || !height) return false;
    // 与查看端一致：快照保持解码输出上限，避免超大帧常驻。
    // Match the viewer: snapshots stay within the decode output cap.
    if ((size_t)width * height > UI_IMAGE_BUDGET) return false;
    wallpaper_cache_header_t header = {
        .magic = WALLPAPER_CACHE_MAGIC,
        .path_hash = wallpaper_path_hash(source_path),
        .size = size,
        .mtime = mtime,
        .width = width,
        .height = height,
    };
    FILE* f = fopen(cache_path, "wb");
    if (!f) return false;
    bool ok = fwrite(&header, sizeof(header), 1, f) == 1 &&
        fwrite(gray, (size_t)width * height, 1, f) == 1;
    if (fclose(f) != 0) ok = false;
    if (!ok) remove(cache_path);
    return ok;
}

bool ui_image_wallpaper_cache_load(const char* cache_path, const char* source_path,
                                    long size, long mtime, uint8_t** gray, uint16_t* width, uint16_t* height) {
    if (!gray || !width || !height) return false;
    *gray = NULL;
    *width = *height = 0;
    if (!cache_path || !source_path) return false;
    FILE* f = fopen(cache_path, "rb");
    if (!f) return false;
    wallpaper_cache_header_t header;
    bool ok = fread(&header, sizeof(header), 1, f) == 1 &&
        header.magic == WALLPAPER_CACHE_MAGIC &&
        header.path_hash == wallpaper_path_hash(source_path) &&
        header.size == size && header.mtime == mtime &&
        header.width && header.height && (size_t)header.width * header.height <= UI_IMAGE_BUDGET;
    if (ok) {
        uint8_t* out = heap_caps_malloc((size_t)header.width * header.height, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        ok = out && fread(out, (size_t)header.width * header.height, 1, f) == 1;
        if (ok) {
            *gray = out;
            *width = header.width;
            *height = header.height;
        } else {
            free(out);
        }
    }
    fclose(f);
    return ok;
}
