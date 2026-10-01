/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：EPUB 封面定位（container→OPF→manifest）与等比缩放；只读 ZIP 条目。
 * English: EPUB cover discovery (container→OPF→manifest) with proportional scaling; reads ZIP entries only.
 */
#include "book_cover.h"
#include "book_image.h"
#include "zip_reader.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define COVER_TEXT_MAX (256 * 1024)
#define COVER_DATA_MAX (2 * 1024 * 1024)

// 在 XML 片段里取属性值（单双引号皆可）；找不到返回 NULL。/ Fetch an attribute value from an XML snippet; either quote style.
static const char* attr_value(const char* tag, const char* name, size_t* len) {
    size_t name_len = strlen(name);
    const char* at = tag;
    while ((at = strstr(at, name))) {
        bool edge = at == tag || at[-1] == ' ' || at[-1] == '\t' || at[-1] == '\n';
        const char* eq = at + name_len;
        if (edge && eq[0] == '=') {
            char quote = eq[1];
            if (quote == '"' || quote == '\'') {
                const char* start = eq + 2;
                const char* end = strchr(start, quote);
                if (end) {
                    *len = (size_t)(end - start);
                    return start;
                }
            }
        }
        ++at;
    }
    return NULL;
}

// 极简反转义：&amp; &lt; &gt; &quot; &apos;；原地写回。/ Minimal unescape written back in place.
static void unescape(char* text) {
    char* out = text;
    for (const char* in = text; *in; ++in) {
        if (*in != '&') { *out++ = *in; continue; }
        if (!strncmp(in, "&amp;", 5)) { *out++ = '&'; in += 4; }
        else if (!strncmp(in, "&lt;", 4)) { *out++ = '<'; in += 3; }
        else if (!strncmp(in, "&gt;", 4)) { *out++ = '>'; in += 3; }
        else if (!strncmp(in, "&quot;", 6)) { *out++ = '"'; in += 5; }
        else if (!strncmp(in, "&apos;", 6)) { *out++ = '\''; in += 5; }
        else *out++ = *in;
    }
    *out = 0;
}

// OPF 目录内相对路径拼接。/ Join a path relative to the OPF directory.
static void join_dir(char* out, size_t cap, const char* opf_path, const char* href, size_t href_len) {
    if (href_len >= cap) href_len = cap - 1;
    const char* slash = strrchr(opf_path, '/');
    size_t dir = slash ? (size_t)(slash - opf_path) + 1 : 0;
    if (dir + href_len >= cap) dir = 0;
    memcpy(out, opf_path, dir);
    memcpy(out + dir, href, href_len);
    out[dir + href_len] = 0;
    unescape(out);
}

// 从 OPF 文本定位封面 href：properties=cover-image 优先，其次 meta name=cover→item id，再次常见文件名。
// / Locate the cover href in OPF text: cover-image property first, then meta name=cover→item id, then common names.
static bool find_cover_href(const char* opf, char* href, size_t cap, bool* quoted_id) {
    (void)quoted_id;
    const char* item = opf;
    while ((item = strstr(item, "<item"))) {
        const char* end = strchr(item, '>');
        if (!end) break;
        size_t len = (size_t)(end - item);
        char tag[512];
        if (len < sizeof(tag)) {
            memcpy(tag, item, len);
            tag[len] = 0;
            size_t value_len = 0;
            const char* props = attr_value(tag, "properties", &value_len);
            if (props && value_len <= 64) {
                char prop[65];
                memcpy(prop, props, value_len);
                prop[value_len] = 0;
                if (strstr(prop, "cover-image")) {
                    size_t href_len = 0;
                    const char* value = attr_value(tag, "href", &href_len);
                    if (value && href_len < cap) {
                        memcpy(href, value, href_len);
                        href[href_len] = 0;
                        return true;
                    }
                }
            }
        }
        item = end + 1;
    }
    // <meta name="cover" content="id"/> → <item id="id" ... href=...>
    const char* meta = strstr(opf, "<meta");
    while (meta) {
        const char* end = strchr(meta, '>');
        if (!end) break;
        size_t len = (size_t)(end - meta);
        char tag[512];
        if (len < sizeof(tag)) {
            memcpy(tag, meta, len);
            tag[len] = 0;
            size_t name_len = 0, id_len = 0;
            const char* name = attr_value(tag, "name", &name_len);
            const char* id = attr_value(tag, "content", &id_len);
            if (name && name_len == 5 && !strncmp(name, "cover", 5) && id && id_len < 128) {
                char want[129];
                memcpy(want, id, id_len);
                want[id_len] = 0;
                const char* scan = opf;
                while ((scan = strstr(scan, "<item"))) {
                    const char* item_end = strchr(scan, '>');
                    if (!item_end) break;
                    size_t item_len = (size_t)(item_end - scan);
                    if (item_len < sizeof(tag)) {
                        memcpy(tag, scan, item_len);
                        tag[item_len] = 0;
                        size_t item_id_len = 0, href_len = 0;
                        const char* item_id = attr_value(tag, "id", &item_id_len);
                        const char* value = attr_value(tag, "href", &href_len);
                        if (item_id && item_id_len == id_len && !strncmp(item_id, want, id_len) &&
                            value && href_len < cap) {
                            memcpy(href, value, href_len);
                            href[href_len] = 0;
                            return true;
                        }
                    }
                    scan = item_end + 1;
                }
            }
        }
        meta = strstr(end, "<meta");
    }
    return false;
}

// 等比缩放进白底封面位（contain）。/ Scale proportionally onto a white cover canvas (contain).
static void scale_cover(const uint8_t* src, uint16_t sw, uint16_t sh, uint8_t* out) {
    memset(out, 0xFF, (size_t)BOOK_COVER_W * BOOK_COVER_H);
    int scale = sw * BOOK_COVER_H <= sh * BOOK_COVER_W ? (int)BOOK_COVER_H * 1000 / sh : (int)BOOK_COVER_W * 1000 / sw;
    int dw = (int)sw * scale / 1000, dh = (int)sh * scale / 1000;
    if (dw <= 0 || dh <= 0) return;
    int x0 = (BOOK_COVER_W - dw) / 2, y0 = (BOOK_COVER_H - dh) / 2;
    for (int y = 0; y < dh; ++y) {
        const uint8_t* row = src + (size_t)((int64_t)y * sh / dh) * sw;
        for (int x = 0; x < dw; ++x)
            out[(size_t)(y0 + y) * BOOK_COVER_W + x0 + x] = row[(int64_t)x * sw / dw];
    }
}

bool book_cover_load(const char* path, uint8_t** gray_out) {
    if (!path || !gray_out) return false;
    *gray_out = NULL;
    zip_reader_t* zip = NULL;
    uint8_t* data = NULL;
    uint8_t* pixels = NULL;
    bool ok = false;
    if (zip_open(path, &zip) != ESP_OK || !zip) return false;
    // container.xml → OPF 路径。/ container.xml → the OPF path.
    int opf_index = -1;
    char opf_path[256] = {0};
    int container = zip_find(zip, "META-INF/container.xml");
    if (container >= 0) {
        char* text = malloc(COVER_TEXT_MAX);
        if (text && zip_extract(zip, container, text, COVER_TEXT_MAX) == ESP_OK) {
            const char* root = strstr(text, "rootfile");
            if (root) {
                size_t len = 0;
                const char* value = attr_value(root, "full-path", &len);
                if (value && len < sizeof(opf_path)) {
                    memcpy(opf_path, value, len);
                    opf_path[len] = 0;
                    unescape(opf_path);
                    opf_index = zip_find(zip, opf_path);
                }
            }
        }
        free(text);
    }
    char href[256];
    int cover_index = -1;
    char cover_path[300];
    if (opf_index >= 0) {
        char* text = malloc(COVER_TEXT_MAX);
        if (text && zip_extract(zip, opf_index, text, COVER_TEXT_MAX) == ESP_OK &&
            find_cover_href(text, href, sizeof(href), NULL)) {
            join_dir(cover_path, sizeof(cover_path), opf_path, href, strlen(href));
            cover_index = zip_find(zip, cover_path);
        }
        free(text);
    }
    // 常见文件名回退（OPF 目录或根）。/ Common-name fallbacks (OPF dir or root).
    if (cover_index < 0) {
        static const char* names[] = {"cover.jpeg", "cover.jpg", "cover.png"};
        for (unsigned i = 0; cover_index < 0 && i < sizeof(names) / sizeof(names[0]); ++i) {
            if (opf_path[0]) {
                join_dir(cover_path, sizeof(cover_path), opf_path, names[i], strlen(names[i]));
                cover_index = zip_find(zip, cover_path);
            }
            if (cover_index < 0) cover_index = zip_find(zip, names[i]);
        }
    }
    uint16_t width = 0, height = 0;
    if (cover_index >= 0) {
        size_t size = zip_entry_size(zip, cover_index);
        if (size && size <= COVER_DATA_MAX) {
            data = malloc(size);
            if (data && zip_extract(zip, cover_index, data, size) == ESP_OK &&
                book_image_decode(data, size, COVER_DATA_MAX, &pixels, &width, &height) && pixels) {
                uint8_t* cover = malloc((size_t)BOOK_COVER_W * BOOK_COVER_H);
                if (cover) {
                    scale_cover(pixels, width, height, cover);
                    *gray_out = cover;
                    ok = true;
                }
            }
        }
    }
    free(pixels);
    free(data);
    zip_close(zip);
    return ok;
}
