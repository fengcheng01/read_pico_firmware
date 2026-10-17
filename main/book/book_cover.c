/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：EPUB 封面定位（container→OPF→manifest）与等比缩放；只读 ZIP 条目。
 * English: EPUB cover discovery (container→OPF→manifest) with proportional scaling; reads ZIP entries only.
 * 修订：区分确认无封面和提取失败，禁止把IO/解码/内存失败缓存成生成封面。
 * Revision: Separate confirmed absence from extraction errors; never cache IO/decode/OOM as generated art.
 */
#include "book_cover.h"
#include "book_cache.h"
#include "book_image.h"
#include "zip_reader.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define COVER_TEXT_MAX (256 * 1024)
#define COVER_DATA_MAX (2 * 1024 * 1024)

static char* extract_xml(zip_reader_t* zip, int index);

// 在 XML 片段里取属性值（单双引号皆可）；找不到返回 NULL。/ Fetch an attribute value from an XML snippet; either quote style.
static const char* attr_value(const char* tag, const char* name, size_t* len) {
    size_t name_len = strlen(name);
    const char* at = tag;
    while ((at = strstr(at, name))) {
        bool edge = at == tag || at[-1] == ' ' || at[-1] == '\t' || at[-1] == '\n';
        const char* eq = at + name_len;
        while (*eq == ' ' || *eq == '\t' || *eq == '\r' || *eq == '\n') ++eq;
        if (edge && eq[0] == '=') {
            ++eq;
            while (*eq == ' ' || *eq == '\t' || *eq == '\r' || *eq == '\n') ++eq;
            char quote = *eq;
            if (quote == '"' || quote == '\'') {
                const char* start = eq + 1;
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

// XML命名空间前缀不改变元素名；只匹配完整开始标签。
// XML namespace prefixes do not change local names; match complete opening tags only.
static const char* xml_tag(const char* text, const char* local) {
    size_t n = strlen(local);
    while ((text = strchr(text, '<'))) {
        const char* start = ++text;
        while (*text && *text != '>' && *text != '/' && *text != ' ' && *text != '\t' && *text != '\r' && *text != '\n') ++text;
        const char* colon = memchr(start, ':', (size_t)(text - start));
        if (colon) start = colon + 1;
        if ((size_t)(text - start) == n && !memcmp(start, local, n)) return start;
        if (!*text) break;
    }
    return NULL;
}

static int cover_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// EPUB地址先反转义和百分号解码，再规范化点段；不越出ZIP根目录。
// Decode EPUB entities/percent escapes before normalizing dot segments, never escaping the ZIP root.
static bool join_dir(char* out, size_t cap, const char* opf_path, const char* href, size_t href_len) {
    char ref[512], combined[768];
    if (href_len >= sizeof(ref)) return false;
    memcpy(ref, href, href_len); ref[href_len] = 0; unescape(ref);
    char* fragment = strpbrk(ref, "#?"); if (fragment) *fragment = 0;
    char* dst = ref;
    for (const char* src = ref; *src; ++src) {
        unsigned char c = (unsigned char)*src;
        if (c == '%' && src[1] && src[2] && cover_hex(src[1]) >= 0 && cover_hex(src[2]) >= 0) {
            c = (unsigned char)((cover_hex(src[1]) << 4) | cover_hex(src[2])); src += 2;
        }
        if (c < 32 || c == 127 || c == ':' || c == '\\') return false;
        *dst++ = (char)c;
    }
    *dst = 0;
    const char* slash = strrchr(opf_path, '/');
    size_t dir = ref[0] != '/' && slash ? (size_t)(slash - opf_path) + 1 : 0;
    size_t len = strlen(ref);
    if (dir + len >= sizeof(combined)) return false;
    memcpy(combined, opf_path, dir); memcpy(combined + dir, ref, len + 1);
    size_t used = 0;
    const char* at = combined;
    while (*at) {
        while (*at == '/') ++at;
        const char* end = strchr(at, '/'); if (!end) end = at + strlen(at);
        size_t n = (size_t)(end - at);
        if (n == 2 && at[0] == '.' && at[1] == '.') {
            if (!used) return false;
            while (used && out[used - 1] != '/') --used;
            if (used) --used;
        } else if (n && !(n == 1 && at[0] == '.')) {
            if (used + (used != 0) + n >= cap) return false;
            if (used) out[used++] = '/';
            memcpy(out + used, at, n); used += n;
        }
        at = end;
    }
    out[used] = 0;
    return used != 0;
}

// 从 OPF 文本定位封面 href：properties=cover-image 优先，其次 meta name=cover→item id，再次常见文件名。
// / Locate the cover href in OPF text: cover-image property first, then meta name=cover→item id, then common names.
static bool find_cover_href(const char* opf, char* href, size_t cap, bool* quoted_id) {    (void)quoted_id;
    const char* item = opf;
    while ((item = xml_tag(item, "item"))) {
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
    const char* meta = xml_tag(opf, "meta");
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
                while ((scan = xml_tag(scan, "item"))) {
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
        meta = xml_tag(end, "meta");
    }
    return false;
}

// spine 首文档启发式：首个 spine 项的 xhtml 里第一个图片引用（img src 或 image xlink:href）。
// KOReader 同款最后一级回退：只认声明过的封面，spine 首文档常是封面页。
// Spine-first heuristic: the first image reference inside the first spine document
// (img src or image xlink:href). The same last-resort tier as KOReader: declared
// covers take precedence, and the first spine document is usually the cover page.
static int find_spine_cover(zip_reader_t* zip, const char* opf, const char* opf_path,
                            char* cover_path, size_t cap) {
    const char* spine = xml_tag(opf, "spine");
    if (!spine) return -1;
    const char* itemref = xml_tag(spine, "itemref");
    if (!itemref) return -1;
    const char* end = strchr(itemref, '>');
    if (!end) return -1;
    size_t len = (size_t)(end - itemref);
    char tag[512];
    if (len >= sizeof(tag)) return -1;
    memcpy(tag, itemref, len);
    tag[len] = 0;
    size_t id_len = 0;
    const char* idref = attr_value(tag, "idref", &id_len);
    if (!idref || !id_len || id_len >= 128) return -1;
    char want[129];
    memcpy(want, idref, id_len);
    want[id_len] = 0;
    // idref → manifest href（限 xhtml/spine 文档）。/ idref to the manifest href (xhtml spine documents only).
    char doc_path[300];
    const char* item = opf;
    while ((item = xml_tag(item, "item"))) {
        const char* item_end = strchr(item, '>');
        if (!item_end) break;
        size_t item_len = (size_t)(item_end - item);
        if (item_len >= sizeof(tag)) { item = item_end + 1; continue; }
        memcpy(tag, item, item_len);
        tag[item_len] = 0;
        size_t item_id_len = 0, href_len = 0;
        const char* item_id = attr_value(tag, "id", &item_id_len);
        const char* href = attr_value(tag, "href", &href_len);
        if (item_id && item_id_len == id_len && !strncmp(item_id, want, id_len) && href && href_len) {
            if (!join_dir(doc_path, sizeof(doc_path), opf_path, href, href_len)) return -1;
            int doc_index = zip_find(zip, doc_path);
            if (doc_index < 0) return -1;
            char* text = extract_xml(zip, doc_index);
            if (!text) return -1;
            const char* found = NULL;
            size_t found_len = 0;
            for (const char* probe = text; (probe = strstr(probe, "<")); ) {
                const char* tag_end = strchr(probe, '>');
                if (!tag_end) break;
                size_t n = (size_t)(tag_end - probe);
                if (n < sizeof(tag)) {
                    memcpy(tag, probe, n + 1);
                    tag[n] = 0;
                    // img src 与 image xlink:href 各取一次。/ One pass each for img src and image xlink:href.
                    if (!strncmp(tag, "<img", 4) || !strncmp(tag, "<image", 6)) {
                        const char* value = attr_value(tag, "xlink:href", &found_len);
                        if (!value) value = attr_value(tag, "src", &found_len);
                        if (value && found_len) {
                            found = value;
                            bool ok = join_dir(cover_path, cap, doc_path, found, found_len);
                            free(text);
                            return ok ? zip_find(zip, cover_path) : -1;
                        }
                    }
                }
                probe = tag_end + 1;
            }
            free(text);
            return -1;
        }
        item = item_end + 1;
    }
    return -1;
}

// 等比缩放进白底封面位（contain）；重采样与解码同规则（缩小面积平均/放大双线性）。
// Scale proportionally onto a white cover canvas (contain); resampling shares the
// decoder's rules (box-average shrink / bilinear grow).
static bool scale_cover(const uint8_t* src, uint16_t sw, uint16_t sh, uint8_t* out) {
    memset(out, 0xFF, (size_t)BOOK_COVER_W * BOOK_COVER_H);
    if (!sw || !sh) return false;
    int scale = sw * BOOK_COVER_H <= sh * BOOK_COVER_W ? (int)BOOK_COVER_H * 1000 / sh : (int)BOOK_COVER_W * 1000 / sw;
    int dw = (int)sw * scale / 1000, dh = (int)sh * scale / 1000;
    if (dw <= 0 || dh <= 0) return false;
    uint8_t* tmp = malloc((size_t)dw * dh);
    if (!tmp) return false;
    book_image_resample(src, sw, sw, sh, tmp, (unsigned)dw, (unsigned)dh);
    int x0 = (BOOK_COVER_W - dw) / 2, y0 = (BOOK_COVER_H - dh) / 2;
    for (int y = 0; y < dh; ++y)
        memcpy(out + (size_t)(y0 + y) * BOOK_COVER_W + x0, tmp + (size_t)y * dw, (size_t)dw);
    free(tmp);
    return true;
}

// XML 条目按实际大小分配并补终止符；ZIP 接口只返回原始字节。
// Allocate XML by its bounded entry size and terminate it; ZIP returns raw bytes only.
static char* extract_xml(zip_reader_t* zip, int index) {
    size_t size = zip_entry_size(zip, index);
    if (!size || size > COVER_TEXT_MAX) return NULL;
    char* text = malloc(size + 1);
    if (!text) return NULL;
    if (zip_extract(zip, index, text, size) != ESP_OK || memchr(text, 0, size)) {
        free(text);
        return NULL;
    }
    text[size] = 0;
    return text;
}

book_cover_status_t book_cover_read(const char* path, uint8_t** gray_out) {
    if (!path || !gray_out) return BOOK_COVER_RETRY;
    *gray_out = NULL;
    const char* ext = strrchr(path, '.');
    if (ext && !strcasecmp(ext, ".txt")) return BOOK_COVER_ABSENT;
    zip_reader_t* zip = NULL;
    uint8_t* data = NULL;
    uint8_t* pixels = NULL;
    book_cover_status_t status = BOOK_COVER_RETRY;
    if (zip_open(path, &zip) != ESP_OK || !zip) return BOOK_COVER_RETRY;
    uint32_t source_bytes, directory_bytes, directory_crc;
    book_cache_identity_t identity = {0};
    bool cacheable = zip_directory_identity(zip, &source_bytes, &directory_bytes, &directory_crc);
    if (cacheable) {
        // 高位代次区分像素处理变化（如平滑重采样），旧缓存自动失效重生成。
        // The high generation separates pixel-processing changes (smooth resampling);
        // old caches invalidate and regenerate.
        identity = (book_cache_identity_t){.bytes = source_bytes, .digest = directory_bytes,
            .crc = directory_crc,
            .variant = ((uint32_t)BOOK_COVER_W << 16) | BOOK_COVER_H | (2u << 28)};
        void* cached = NULL; size_t bytes = 0;
        if (book_cache_load(path, BOOK_CACHE_COVER, &identity, (size_t)BOOK_COVER_W * BOOK_COVER_H, &cached, &bytes)) {
            if (bytes == (size_t)BOOK_COVER_W * BOOK_COVER_H) {
                *gray_out = cached; zip_close(zip); return BOOK_COVER_READY;
            }
            free(cached);
        }
    }
    // container.xml → OPF 路径。/ container.xml → the OPF path.
    int opf_index = -1;
    char opf_path[256] = {0};
    char* opf_text = NULL;
    int container = zip_find(zip, "META-INF/container.xml");
    if (container >= 0) {
        char* text = extract_xml(zip, container);
        if (text) {
            const char* root = xml_tag(text, "rootfile");
            if (root) {
                size_t len = 0;
                const char* value = attr_value(root, "full-path", &len);
                if (value && len < sizeof(opf_path)) {
                    if (join_dir(opf_path, sizeof(opf_path), "", value, len)) opf_index = zip_find(zip, opf_path);
                }
            }
        }
        free(text);
    }
    bool manifest_read = false, cover_declared = false;
    char href[256];
    int cover_index = -1;
    char cover_path[300];
    if (opf_index >= 0) opf_text = extract_xml(zip, opf_index);
    manifest_read = opf_text != NULL;
    if (opf_text && find_cover_href(opf_text, href, sizeof(href), NULL)) {
        cover_declared = true;
        if (join_dir(cover_path, sizeof(cover_path), opf_path, href, strlen(href))) cover_index = zip_find(zip, cover_path);
    }
    // 常见文件名回退（OPF 目录或根），再退到 spine 首文档启发式。/ Common-name fallbacks
    // (OPF dir or root), then the spine-first-document heuristic.
    if (cover_index < 0) {
        static const char* names[] = {"cover.jpeg", "cover.jpg", "cover.png"};
        for (unsigned i = 0; cover_index < 0 && i < sizeof(names) / sizeof(names[0]); ++i) {
            if (opf_path[0]) {
                if (join_dir(cover_path, sizeof(cover_path), opf_path, names[i], strlen(names[i]))) cover_index = zip_find(zip, cover_path);
            }
            if (cover_index < 0) cover_index = zip_find(zip, names[i]);
        }
    }
    if (cover_index < 0 && opf_text && !cover_declared)
        cover_index = find_spine_cover(zip, opf_text, opf_path, cover_path, sizeof(cover_path));
    free(opf_text);
    uint16_t width = 0, height = 0;
    if (cover_index >= 0) {
        size_t size = zip_entry_size(zip, cover_index);
        if (size && size <= COVER_DATA_MAX) {
            data = malloc(size);
            if (data && zip_extract(zip, cover_index, data, size) == ESP_OK &&
                book_image_decode(data, size, 128u * 1024u, &pixels, &width, &height) && pixels) {
                uint8_t* cover = malloc((size_t)BOOK_COVER_W * BOOK_COVER_H);
                if (cover) {
                    scale_cover(pixels, width, height, cover);
                    *gray_out = cover;
                    status = BOOK_COVER_READY;
                    if (cacheable) (void)book_cache_save(path, BOOK_CACHE_COVER, &identity, cover,
                                                       (size_t)BOOK_COVER_W * BOOK_COVER_H);
                }
            }
        }
    }
    free(pixels);
    free(data);
    zip_close(zip);
    if (cover_index < 0 && manifest_read && !cover_declared) status = BOOK_COVER_ABSENT;
    return status;
}

bool book_cover_load(const char* path, uint8_t** gray_out) {
    return book_cover_read(path, gray_out) == BOOK_COVER_READY;
}
