/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：将常见 EPUB 插图变为适合墨水屏的白底灰度图。
 * English: Convert common EPUB illustrations to white-backed grayscale for the panel.
 * 冻结修订：书源/查看器串行解码，分配有界且不在render调用；按运行时间让出CPU，保持原图像像素。
 * Frozen revision: Serialize source/viewer decoding with bounded allocation outside render; yield by runtime while preserving image pixels.
 * 实机反馈（两轮）：约 1MP 的 RGBA 壁纸截图先撞平面 1MP 上限，放宽后又因“文件数据+压缩流+整幅
 * 原始行缓冲”并发峰值挤爆 PSRAM。PNG 改为 ROM miniz 环形流式还原：滚动两行缓冲边解边缩，
 * 不再分配整幅原始缓冲；输入上限按通道峰值堆判定，总量上界 6 MiB。
 * Device feedback (two rounds): ~1MP RGBA wallpaper shots first hit the flat 1-megapixel cap, and
 * after relaxing it the concurrent peak of file data + packed stream + the full raw scanline buffer
 * still exhausted PSRAM. PNG now streams through ROM miniz's ring mode: a rolling two-row buffer
 * unfilters and downsamples on the fly without any full raw buffer; input limits derive from the
 * per-channel peak heap under a 6 MiB total bound.
 * 实机修订（第三轮）：直开大图偶发复发——文件数据+压缩流副本两块 1.4MB 连续内存同时
 * 常驻，碎片化后分配失败。IDAT 改为分段直接喂入（HAS_MORE_INPUT 跨段续压），副本取消，
 * 唯一大块只剩调用方的文件缓冲；各失败阶段留串口日志便于定位。
 * Device revision (third round): large-image opening regressed intermittently — file data plus
 * the packed-stream copy held two concurrent 1.4MB blocks and fragmentation killed the alloc.
 * IDATs now feed the decompressor segment by segment (HAS_MORE_INPUT across segments), the copy
 * is gone and the caller's file buffer remains the only large block; each failure stage logs
 * for diagnosis.
 * 文件流式轮（第四轮）：调用方仍需先把整个文件读进内存，2 MiB 输入上限把普通手机照片挡在
 * 门外。三个参考固件的共同解法是从 FILE* 直接流式喂入，本模块照做：PNG 经固定窗口喂
 * tinfl（PLTE/tRNS 落小缓冲），JPEG 读回调改指 FILE*；压缩数据不再整驻内存，字节上限
 * 只作 50 MiB 异常防护，尺寸与峰值预算规则不变。
 * File-streaming round (fourth): callers still had to read the whole file into RAM first, so
 * the 2 MiB input cap kept ordinary phone photos out. All three reference firmwares stream
 * straight from FILE*; this module now does the same: PNG feeds tinfl through a fixed window
 * (PLTE/tRNS land in small buffers) and the JPEG read callback points at FILE*. Compressed
 * data never resides whole in RAM, the byte cap is only a 50 MiB pathological guard, and the
 * dimension and peak-budget rules are unchanged.
 * 实机修订（第六轮）：渐进 JPEG 大于 2 MiB 整类打不开——tjpgd 拒收后 FILE* 停在流中段，
 * SOF 嗅探未回绕 SOI 永远找不到帧头，渐进回退成死代码。嗅探先回绕再读；窗口按 EXIF
 * 64KB 规范上限收小，减小渐进路径的 SD 读量与分配。
 * Device revision (sixth round): progressive JPEGs past 2 MiB failed wholesale — after
 * tjpgd's rejection FILE* sits mid-stream, so the SOF sniff never rewound to SOI and the
 * progressive fallback was dead code. The sniff now rewinds first; its window shrinks to
 * the 64KB EXIF spec bound, trimming SD reads and the allocation on the progressive path.
 * 渐进封面轮（第五轮）：stbi 解渐进要整幅系数缓冲（约 14.7MB/百万像素），PSRAM 装不下，
 * 渐进封面的书整类没有封面。引入参考固件同款 JPEGDEC fork（DC 扫描 only，1/8 平面，
 * 常驻约 20KB），内存与文件两路都接：渐进 JPEG 不再整读、不再受 2 MiB 回退闸门限制。
 * Progressive-cover round (fifth): stbi's progressive path needs the full coefficient buffer
 * (~14.7MB per megapixel) that PSRAM cannot hold, so books with progressive covers lost their
 * covers wholesale. The JPEGDEC fork shared by all three reference firmwares (DC scan only,
 * 1/8 plane, ~20KB resident) is wired into both the memory and file paths: progressive JPEGs
 * no longer whole-read and no longer hit the 2 MiB fallback gate.
 * 平滑缩放轮（第七轮）：抽稀/最近邻在缩小时丢行出摩尔纹，JPEG 放大又出块斑。PNG 流式
 * 路径改为按行累加的面积平均（box）；基线 JPEG 先按 1:1 落解码平面（档位优先让平面覆盖
 * 目标、按解码成本封顶），再统一缩小面积平均/放大双线性；渐进 1/8 平面与 stbi 兜底共用
 * 同一重采样器。字节与峰值预算规则不变。
 * Smooth-scaling round (seventh): decimation/nearest dropped rows into moire when shrinking
 * and blocky JPEG upscales. The streaming PNG path accumulates area-average (box) rows;
 * baseline JPEG lands its decode plane 1:1 (scale picks prefer planes covering the target,
 * capped by decode cost), then one resample pass covers both directions; the progressive
 * 1/8 plane and stbi fallback share the resampler. Byte and peak budgets are unchanged.
 */
#include "book_image.h"
#include "book_image_yield.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "miniz.h"
#include "vendor/tjpgd.h"
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

// RGBA 壁纸的解压行缓冲即可达 4–5 MiB；峰值=解压缓冲+max(压缩流,输出)，见 png_decode 预检。
// RGBA wallpapers need 4-5 MiB raw scanline buffers alone; the peak is raw plus
// max(packed stream, output), pre-checked in png_decode.
#define IMAGE_HEAP_MAX (6u * 1024u * 1024u)
#define PSRAM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
typedef union { size_t size; max_align_t align; } allocation_t;
static size_t allocated;

static void* image_alloc(size_t n) {
    if (n > SIZE_MAX - sizeof(allocation_t) || n + sizeof(allocation_t) > IMAGE_HEAP_MAX - allocated) return NULL;
    allocation_t* p = heap_caps_malloc(sizeof(*p) + n, PSRAM_CAPS);
    if (!p) return NULL;
    p->size = n; allocated += n + sizeof(*p);
    return p + 1;
}
static void image_free(void* ptr) {
    if (!ptr) return;
    allocation_t* p = (allocation_t*)ptr - 1;
    allocated -= p->size + sizeof(*p);
    free(p);
}
static void* image_realloc(void* ptr, size_t n) {
    if (!ptr) return image_alloc(n);
    allocation_t* old = (allocation_t*)ptr - 1;
    size_t prior = old->size;
    if (n > SIZE_MAX - sizeof(*old) || n > IMAGE_HEAP_MAX - (allocated - prior)) return NULL;
    allocation_t* p = heap_caps_realloc(old, sizeof(*old) + n, PSRAM_CAPS);
    if (!p) return NULL;
    p->size = n; allocated = allocated - prior + n;
    return p + 1;
}

#define STBI_MALLOC(n) image_alloc(n)
#define STBI_REALLOC(p,n) image_realloc(p,n)
#define STBI_FREE(p) image_free(p)
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"

static uint32_t png_u32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static unsigned png_u16(const uint8_t* p) { return (unsigned)p[0] << 8 | p[1]; }
static uint8_t png_paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return (uint8_t)(pa <= pb && pa <= pc ? a : pb <= pc ? b : c);
}

// 限制像素总量时等比缩小，避免多图章节直接退回手动查看。
// Reduce dimensions proportionally to the pixel budget instead of forcing manual viewing in image-heavy chapters.
static void fit_budget(unsigned* w, unsigned* h, size_t budget) {
    while ((size_t)*w * *h > budget && (*w > 1 || *h > 1)) {
        *w = *w > 1 ? (*w * 3) / 4 : 1;
        *h = *h > 1 ? (*h * 3) / 4 : 1;
    }
}

// 常见8位非交错PNG流式还原：环形解压喂给滚动两行缓冲，边去滤边缩，不再整幅驻留。
// Unfilter common 8-bit noninterlaced PNG on the fly: the ring inflater feeds a rolling
// two-row buffer that unfilters and downsamples without ever holding the full raw image.
typedef struct {
    unsigned w, h, channels, type, dw, dh, dy, y, slot, rows_acc;
    const uint8_t *palette, *trans;
    size_t palette_n, trans_n, row_n, row_cap, fill;
    uint8_t *rows, *out;
    uint32_t acc[648];
    int64_t yielded_at;
    bool failed;
} png_stream_t;

// 与原整幅路径相同的逐字节去滤；prev 为空即首行。/ Same per-byte unfilter as the old
// full-buffer path; a NULL prev marks the first row.
static void png_row_finish(png_stream_t *s) {
    uint8_t *cur = s->rows + (size_t)s->slot * s->row_cap;
    uint8_t *row = cur + 1;
    const uint8_t *prev = s->y ? s->rows + (size_t)(s->slot ^ 1) * s->row_cap + 1 : NULL;
    unsigned filter = cur[0];
    if (filter <= 4) {
        // 按行选一次滤波器；None不扫描，其他类型只读各自需要的邻字节。
        // Pick the filter once per row: None skips work, others read only their required neighbors.
        if (filter == 1) {
            for (size_t x = s->channels; x < s->row_n; ++x) row[x] += row[x - s->channels];
        } else if (filter == 2 && prev) {
            for (size_t x = 0; x < s->row_n; ++x) row[x] += prev[x];
        } else if (filter == 3 || filter == 4) {
            for (size_t x = 0; x < s->row_n; ++x) {
                unsigned a = x >= s->channels ? row[x - s->channels] : 0;
                unsigned b = prev ? prev[x] : 0, c = prev && x >= s->channels ? prev[x - s->channels] : 0;
                row[x] += filter == 3 ? (a + b) / 2 : png_paeth(a, b, c);
            }
        }
        if (s->dy < s->dh) {
            // 面积平均：每源列只算一次灰度，按列区间落进对应输出列的累加器。
            // 输出列 x 的源区间是 [x*w/dw, (x+1)*w/dw)，恰好划分全部源列。
            // Area average: each source column computes gray once and lands in the output
            // column owning it. Output x spans [x*w/dw, (x+1)*w/dw), partitioning [0, w).
            for (unsigned c = 0; c < s->w; ++c) {
                const uint8_t *p = row + (size_t)c * s->channels;
                unsigned gray = 0, alpha = 255;
                if (s->type == 0 || s->type == 4) {
                    gray = p[0];
                    if (s->type == 4) alpha = p[1];
                    else if (s->trans && p[0] == png_u16(s->trans)) alpha = 0;
                } else if (s->type == 3) {
                    unsigned index = p[0];
                    if (index >= s->palette_n) { s->failed = true; return; }
                    const uint8_t *color = s->palette + index * 3;
                    gray = (77u * color[0] + 150u * color[1] + 29u * color[2]) >> 8;
                    if (index < s->trans_n) alpha = s->trans[index];
                } else {
                    gray = (77u * p[0] + 150u * p[1] + 29u * p[2]) >> 8;
                    if (s->type == 6) alpha = p[3];
                    else if (s->trans && p[0] == png_u16(s->trans) && p[1] == png_u16(s->trans + 2) && p[2] == png_u16(s->trans + 4)) alpha = 0;
                }
                s->acc[(size_t)((uint64_t)c * s->dw / s->w)] += (gray * alpha + 255u * (255u - alpha) + 127u) / 255u;
            }
            ++s->rows_acc;
            // 该输出行的源行区间走完即产出。/ Emit once the output row's source span is complete.
            if ((uint64_t)(s->y + 1) * s->dh >= (uint64_t)(s->dy + 1) * s->h) {
                for (unsigned x = 0; x < s->dw; ++x) {
                    // 列计数与归属同式：floor 所有权、ceil 区间端点。
                    // Column counts match ownership: floor ownership with ceil span ends.
                    unsigned ca = (x * s->w + s->dw - 1) / s->dw;
                    unsigned cb = ((x + 1) * s->w + s->dw - 1) / s->dw;
                    unsigned cols = cb > ca ? cb - ca : 1;
                    uint32_t count = (uint32_t)s->rows_acc * cols;
                    s->out[(size_t)s->dy * s->dw + x] = (uint8_t)((s->acc[x] + count / 2) / count);
                    s->acc[x] = 0;
                }
                s->rows_acc = 0;
                ++s->dy;
            }
        }
        ++s->y;
    } else s->failed = true;
}

static int png_sink(const void *buf, int len, void *user) {
    png_stream_t *s = user;
    const uint8_t *p = buf;
    while (len > 0 && !s->failed) {
        size_t need = s->row_cap - s->fill;
        size_t take = need < (size_t)len ? need : (size_t)len;
        memcpy(s->rows + (size_t)s->slot * s->row_cap + s->fill, p, take);
        s->fill += take; p += take; len -= take;
        if (s->fill == s->row_cap) {
            png_row_finish(s);
            s->slot ^= 1; s->fill = 0;
            if (!(s->y % 32)) book_image_yield(&s->yielded_at);
        }
    }
    return s->failed ? 0 : 1;
}

// ROM miniz 环形驱动：32KB 环写满即回调消费，掩码回绕；字典与状态由本模块分配。
// 输入以任意长度的段/窗口喂入（跨边界以 HAS_MORE_INPUT 告知后续还有字节），不再复制整条压缩流。
// ROM miniz ring driver: the 32KB ring fills, the sink consumes, the offset wraps by
// mask; this module owns the dictionary and state allocations. Input arrives as
// segments/windows of any length (HAS_MORE_INPUT across boundaries) with no whole-stream copy.
typedef struct { const uint8_t *p; size_t n; } png_idat_t;
typedef struct {
    tinfl_decompressor *state;
    uint8_t *dict;
    size_t dict_ofs;
    bool done, failed;
} png_inflate_t;

// 喂入一段压缩输入：内部消化 HAS_MORE_OUTPUT，段/窗口边界对解码器透明；行消费失败后
// 继续吞输入排空到终态（不回调），压缩失败立即停。
// Feed one span of compressed input: HAS_MORE_OUTPUT is digested inside so segment or
// window boundaries stay transparent; after a row-consumer failure input keeps flowing
// to a terminal state (no callbacks), while a decompress failure stops immediately.
static void png_inflate_feed(png_inflate_t *z, const uint8_t *p, size_t n, png_stream_t *s) {
    while (n && !z->done && !z->failed) {
        size_t in_size = n, out_size = TINFL_LZ_DICT_SIZE - z->dict_ofs;
        tinfl_status status = tinfl_decompress(z->state, p, &in_size, z->dict, z->dict + z->dict_ofs,
                                               &out_size, TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_HAS_MORE_INPUT);
        p += in_size; n -= in_size;
        if (out_size && !s->failed && !png_sink(z->dict + z->dict_ofs, (int)out_size, s)) s->failed = true;
        z->dict_ofs = (z->dict_ofs + out_size) & (TINFL_LZ_DICT_SIZE - 1);
        if (status == TINFL_STATUS_DONE) z->done = true;
        else if (status == TINFL_STATUS_FAILED) z->failed = true;
    }
}

// 分配即清零：ROM 容忍任意初值，宿主替身要求指针域为零；后续任何路径的 re-init 都读到安全的零状态。
// Zero right after the allocation: ROM tolerates any initial bytes while the host
// stand-in requires NULL pointer fields, and every later re-init then reads a safe zero state.
static bool png_inflate_open(png_inflate_t *z) {
    z->state = image_alloc(sizeof(*z->state));
    z->dict = image_alloc(TINFL_LZ_DICT_SIZE);
    if (z->state) memset(z->state, 0, sizeof(*z->state));
    if (z->state && z->dict) tinfl_init(z->state);
    return z->state && z->dict;
}

// 宿主替身只在终态归还 zlib 内部状态：提前退出路径先 re-init 强制释放；ROM 上是空操作。
// The host stand-in releases zlib internals only on terminal states: re-init forces
// the release on early-exit paths; a no-op on ROM.
static void png_inflate_close(png_inflate_t *z) {
    if (z->state) tinfl_init(z->state);
    image_free(z->state); image_free(z->dict);
    z->state = NULL; z->dict = NULL;
}

static bool png_inflate_stream(const png_idat_t *segs, size_t seg_count, png_stream_t *s) {
    png_inflate_t z = {0};
    bool ok = png_inflate_open(&z);
    if (ok) {
        for (size_t seg = 0; seg < seg_count && !z.done && !z.failed; ++seg)
            png_inflate_feed(&z, segs[seg].p, segs[seg].n, s);
        ok = z.done && !z.failed;
    }
    png_inflate_close(&z);
    return ok && !s->failed;
}

// 设备侧失败阶段留一条串口日志：图片页只显示笼统提示，日志才能定位复发根因；
// 宿主侧同样输出到 stderr，供本地夹具定位失败阶段。
// One serial line per device-side failure stage: the page shows a generic hint only,
// so the log is what pinpoints a recurrence; host builds mirror it on stderr for
// local fixture diagnosis.
#ifdef ESP_PLATFORM
#include "esp_log.h"
#define IMG_LOGW(...) ESP_LOGW("book_image", __VA_ARGS__)
#else
#define IMG_LOGW(...) do { fprintf(stderr, "IMG %s:%d ", __func__, __LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while (0)
#endif
#define PNG_IDAT_MAX 128
// 文件入口的字节上限只防异常扫描与坏块头；内存约束由尺寸上限与峰值预算承担（对齐参考固件）。
// The file-entry byte cap only guards pathological scans and bad chunk headers; memory
// stays bounded by the dimension caps and the peak budget (matching the references).
#define IMAGE_FILE_MAX (50u * 1024u * 1024u)
#define PNG_FILE_WINDOW 8192
static bool png_decode(const uint8_t* data, size_t len, size_t budget,
                       uint8_t** pixels, uint16_t* width, uint16_t* height) {
    if (len < 33 || memcmp(data,"\x89PNG\r\n\x1a\n",8) || png_u32(data+8)!=13 || memcmp(data+12,"IHDR",4)) return false;
    unsigned w=png_u32(data+16), h=png_u32(data+20), type=data[25];
    unsigned channels=type==0 ? 1 : type==2 ? 3 : type==3 ? 1 : type==4 ? 2 : type==6 ? 4 : 0;
    if (!w || !h || w>4096 || h>4096 || !channels ||
        data[24]!=8 || data[26] || data[27] || data[28]) { IMG_LOGW("png header w=%u h=%u type=%u", w, h, type); return false; }
    const uint8_t *palette=NULL,*trans=NULL; size_t palette_n=0,trans_n=0,packed_n=0;
    bool idat=false,ended=false;
    // IDAT 段表：直读原缓冲，不再复制压缩流。
    // IDAT segment table: read straight from the caller buffer, no stream copy.
    png_idat_t segs[PNG_IDAT_MAX];
    size_t seg_count=0;
    bool seg_overflow=false;
    for (size_t at=33;at<len;) {
        if (len-at<12) { IMG_LOGW("png chunk tail"); return false; }
        size_t n=png_u32(data+at);
        if (n>len-at-12) { IMG_LOGW("png chunk overflow"); return false; }
        const uint8_t* tag=data+at+4;
        if (!memcmp(tag,"IDAT",4)) {
            if (n) {
                if (seg_count < PNG_IDAT_MAX) segs[seg_count++] = (png_idat_t){data+at+8, n};
                else seg_overflow = true;
            }
            packed_n+=n; idat=true;
        }
        else if (!memcmp(tag,"PLTE",4)) {
            if (idat || palette || !n || n%3 || n>768) { IMG_LOGW("png plte"); return false; }
            palette=data+at+8;palette_n=n/3;
        } else if (!memcmp(tag,"tRNS",4)) {
            if (idat || trans || (type==0 ? n!=2 : type==2 ? n!=6 : type==3 ? !n || n>256 : true)) { IMG_LOGW("png trns"); return false; }
            trans=data+at+8;trans_n=n;
        } else if (!memcmp(tag,"IEND",4)) { if(n){IMG_LOGW("png iend len");return false;} ended=true;break; }
        else if (!(tag[0]&32)) { IMG_LOGW("png critical chunk"); return false; }
        at+=n+12;
    }
    if (!idat || !seg_count || seg_overflow) { IMG_LOGW("png idat segs=%zu overflow=%d", seg_count, (int)seg_overflow); return false; }
    if (!ended || !packed_n || (type==3 && (!palette || trans_n>palette_n))) { IMG_LOGW("png layout ended=%d", (int)ended); return false; }
    unsigned dw=w,dh=h;
    if (dw>648) { dh=dh*648/dw;dw=648; }
    if (dh>1000) { dw=dw*1000/dh;dh=1000; }
    if(!dw)dw=1;
    if(!dh)dh=1;
    fit_budget(&dw, &dh, budget);
    if((size_t)dw*dh>budget)return false;
    // 峰值=压缩流+32KB环+解压状态+两行缓冲（滚动，不再有整幅原始缓冲）+输出。
    // The peak is the packed stream plus the 32KB ring, the decompressor state, the two
    // rolling rows (no full raw buffer anymore) and the output.
    png_stream_t s = {.w=w, .h=h, .channels=channels, .type=type, .dw=dw, .dh=dh,
        .palette=palette, .trans=trans, .palette_n=palette_n, .trans_n=trans_n,
        .row_n=(size_t)w*channels, .row_cap=(size_t)w*channels+1};
    // 峰值=32KB环+解压状态+两行滚动缓冲+输出；压缩流直读原缓冲，无副本。
    // The peak is the 32KB ring, the decompressor state, the two rolling rows and the
    // output; the compressed stream reads straight from the caller buffer, no copy.
    if (TINFL_LZ_DICT_SIZE + sizeof(tinfl_decompressor) + 2*s.row_cap > IMAGE_HEAP_MAX - (size_t)dw*dh) {
        IMG_LOGW("png precheck row=%zu out=%u", s.row_cap, (unsigned)(dw*dh));
        return false;
    }
    // 累加器上界：行×列≤4096×4096 源像素、每像素≤255，255×2^24<2^32，uint32 不溢出。
    // Accumulator bound: rows*cols<=4096*4096 source pixels at <=255 each, 255*2^24<2^32, so uint32 cannot overflow.
    s.rows=image_alloc(2*s.row_cap);
    s.out=heap_caps_malloc((size_t)dw*dh,PSRAM_CAPS);
    bool ok=false;
    if(!s.rows || !s.out){
        IMG_LOGW("png alloc rows=%p out=%p psram=%u", (void*)s.rows, (void*)s.out,
                 (unsigned)heap_caps_get_free_size(PSRAM_CAPS));
        goto done;
    }
    // 流结束必须恰好铺满所有行；多一字节或少一行都按损坏拒绝。
    // The stream must fill exactly every row; a spare byte or a missing row rejects as corrupt.
    ok = png_inflate_stream(segs, seg_count, &s);
    if (ok && (s.y!=h || s.dy!=dh || s.fill)) { IMG_LOGW("png rows y=%u/%u dy=%u/%u fill=%zu", s.y, h, s.dy, dh, s.fill); ok=false; }
    if(!ok)goto done;
    *pixels=s.out;*width=(uint16_t)dw;*height=(uint16_t)dh;s.out=NULL;
done:
    image_free(s.rows); free(s.out); return ok;
}

// 文件流式 PNG：单趟块表遍历——PLTE/tRNS 读入栈缓冲，IDAT 经固定窗口直喂 tinfl，
// 其余块 fseek 跳过；行处理与输出校验和内存路径逐字节一致。
// File-streaming PNG: one chunk walk — PLTE/tRNS land in stack buffers, IDAT feeds
// tinfl through the fixed window, other chunks are fseek-skipped; row handling and
// output validation are byte-for-byte the memory path.
static bool png_decode_file(FILE* f, size_t len, size_t budget,
                            uint8_t** pixels, uint16_t* width, uint16_t* height) {
    uint8_t head[33];
    if (fread(head,1,sizeof(head),f)!=sizeof(head) || memcmp(head,"\x89PNG\r\n\x1a\n",8) ||
        png_u32(head+8)!=13 || memcmp(head+12,"IHDR",4)) return false;
    unsigned w=png_u32(head+16), h=png_u32(head+20), type=head[25];
    unsigned channels=type==0 ? 1 : type==2 ? 3 : type==3 ? 1 : type==4 ? 2 : type==6 ? 4 : 0;
    if (!w || !h || w>4096 || h>4096 || !channels ||
        head[24]!=8 || head[26] || head[27] || head[28]) { IMG_LOGW("png file header w=%u h=%u type=%u", w, h, type); return false; }
    unsigned dw=w,dh=h;
    if (dw>648) { dh=dh*648/dw;dw=648; }
    if (dh>1000) { dw=dw*1000/dh;dh=1000; }
    if(!dw)dw=1;
    if(!dh)dh=1;
    fit_budget(&dw, &dh, budget);
    if((size_t)dw*dh>budget)return false;
    png_stream_t s = {.w=w, .h=h, .channels=channels, .type=type, .dw=dw, .dh=dh,
        .row_n=(size_t)w*channels, .row_cap=(size_t)w*channels+1};
    // 峰值=输入窗口+32KB环+解压状态+两行滚动缓冲+输出；压缩数据不整驻内存。
    // The peak is the input window, the 32KB ring, the decompressor state, the two
    // rolling rows and the output; compressed data never resides whole.
    if (PNG_FILE_WINDOW + TINFL_LZ_DICT_SIZE + sizeof(tinfl_decompressor) + 2*s.row_cap > IMAGE_HEAP_MAX - (size_t)dw*dh) {
        IMG_LOGW("png file precheck row=%zu out=%u", s.row_cap, (unsigned)(dw*dh));
        return false;
    }
    uint8_t palette_buf[768], trans_buf[256];
    const uint8_t *palette=NULL,*trans=NULL;
    size_t palette_n=0,trans_n=0,packed_n=0,at=33;
    bool idat=false,ended=false,ok=false;
    png_inflate_t z = {0};
    uint8_t* window=NULL;
    // 累加器上界：行×列≤4096×4096 源像素、每像素≤255，255×2^24<2^32，uint32 不溢出。
    // Accumulator bound: rows*cols<=4096*4096 source pixels at <=255 each, 255*2^24<2^32, so uint32 cannot overflow.
    s.rows=image_alloc(2*s.row_cap);
    s.out=heap_caps_malloc((size_t)dw*dh,PSRAM_CAPS);
    if(!s.rows || !s.out){
        IMG_LOGW("png file alloc rows=%p out=%p psram=%u", (void*)s.rows, (void*)s.out,
                 (unsigned)heap_caps_get_free_size(PSRAM_CAPS));
        goto done;
    }
    if (!png_inflate_open(&z) || !(window=image_alloc(PNG_FILE_WINDOW))) {
        IMG_LOGW("png file inflate open psram=%u", (unsigned)heap_caps_get_free_size(PSRAM_CAPS));
        goto done;
    }
    for (; at<len;) {
        if (len-at<12) { IMG_LOGW("png file chunk tail"); goto done; }
        uint8_t ch[8];
        if (fread(ch,1,8,f)!=8) { IMG_LOGW("png file chunk read"); goto done; }
        size_t n=png_u32(ch);
        if (n>len-at-12) { IMG_LOGW("png file chunk overflow"); goto done; }
        at+=n+12;
        if (!memcmp(ch+4,"IDAT",4)) {
            idat=true; packed_n+=n;
            while (n && !z.done && !z.failed) {
                size_t want = n < PNG_FILE_WINDOW ? n : PNG_FILE_WINDOW;
                if (fread(window,1,want,f)!=want) { IMG_LOGW("png file idat read"); goto done; }
                png_inflate_feed(&z, window, want, &s);
                n-=want;
            }
            // 提前到达流终态时剩余载荷与 CRC 一起跳过。/ On early stream end, skip the
            // leftover payload together with the CRC.
            if (fseek(f,(long)n+4,SEEK_CUR)) { IMG_LOGW("png file idat skip"); goto done; }
        } else if (!memcmp(ch+4,"PLTE",4)) {
            if (idat || palette || !n || n%3 || n>768) { IMG_LOGW("png file plte"); goto done; }
            if (fread(palette_buf,1,n,f)!=n || fseek(f,4,SEEK_CUR)) { IMG_LOGW("png file plte read"); goto done; }
            palette=palette_buf;palette_n=n/3;
        } else if (!memcmp(ch+4,"tRNS",4)) {
            if (idat || trans || (type==0 ? n!=2 : type==2 ? n!=6 : type==3 ? !n || n>256 : true)) { IMG_LOGW("png file trns"); goto done; }
            if (fread(trans_buf,1,n,f)!=n || fseek(f,4,SEEK_CUR)) { IMG_LOGW("png file trns read"); goto done; }
            trans=trans_buf;trans_n=n;
        } else if (!memcmp(ch+4,"IEND",4)) { if(n){IMG_LOGW("png file iend len");goto done;} ended=true; break; }
        else if (!(ch[4]&32)) { IMG_LOGW("png file critical chunk"); goto done; }
        else if (fseek(f,(long)n+4,SEEK_CUR)) { IMG_LOGW("png file chunk skip"); goto done; }
    }
    if (!idat || !packed_n || !ended || (type==3 && (!palette || trans_n>palette_n))) { IMG_LOGW("png file layout ended=%d", (int)ended); goto done; }
    // 流结束必须恰好铺满所有行；多一字节或少一行都按损坏拒绝。
    // The stream must fill exactly every row; a spare byte or a missing row rejects as corrupt.
    if (!z.done || z.failed) { IMG_LOGW("png file inflate done=%d failed=%d", (int)z.done, (int)z.failed); goto done; }
    if (s.y!=h || s.dy!=dh || s.fill) { IMG_LOGW("png file rows y=%u/%u dy=%u/%u fill=%zu", s.y, h, s.dy, dh, s.fill); goto done; }
    *pixels=s.out;*width=(uint16_t)dw;*height=(uint16_t)dh;s.out=NULL;ok=true;
done:
    png_inflate_close(&z);
    image_free(window); image_free(s.rows); free(s.out); return ok;
}

// ---- 平滑重采样 / Smooth resampling ----
// 缩小按面积平均（box）、放大按双线性，两轴各自判定支持混合比例；区间边界用
// floor(i*s/d) 划分源轴，双线性取像素中心。和式上界 255×4096×4096<2^32，uint32 不溢出。
// Box-area averaging when shrinking, bilinear when growing, decided per axis for mixed
// ratios; spans use floor(i*s/d) bounds and bilinear samples pixel centers. Sums stay
// under 255*4096*4096<2^32, so uint32 accumulators cannot overflow.
void book_image_resample(const uint8_t* src, unsigned stride, unsigned sw, unsigned sh,
                            uint8_t* dst, unsigned dw, unsigned dh) {
    if (!src || !dst || !sw || !sh || !dw || !dh) return;
    bool up_x = dw > sw, up_y = dh > sh;
    for (unsigned dy = 0; dy < dh; ++dy) {
        uint8_t* out = dst + (size_t)dy * dw;
        unsigned ry0, rspan, fy = 0;
        const uint8_t *row_a = NULL, *row_b = NULL;
        if (up_y) {
            // 像素中心8.8定点；末行复制不越界。/ Pixel center in 8.8 fixed point; the last row replicates.
            uint64_t center = ((uint64_t)dy * 2 + 1) * 256ull * sh / (2ull * dh);
            ry0 = (unsigned)(center >> 8);
            fy = (unsigned)(center & 255);
            rspan = 1;
            row_a = src + (size_t)ry0 * stride;
            row_b = src + (size_t)(ry0 + 1 < sh ? ry0 + 1 : ry0) * stride;
            if (row_b == row_a) fy = 255;
        } else {
            ry0 = (unsigned)((uint64_t)dy * sh / dh);
            rspan = (unsigned)((uint64_t)(dy + 1) * sh / dh) - ry0;
        }
        for (unsigned dx = 0; dx < dw; ++dx) {
            unsigned value;
            if (up_x) {
                uint64_t center = ((uint64_t)dx * 2 + 1) * 256ull * sw / (2ull * dw);
                unsigned x0 = (unsigned)(center >> 8), fx = (unsigned)(center & 255);
                unsigned x1 = x0 + 1 < sw ? x0 + 1 : x0;
                if (x1 == x0) fx = 255;
                if (up_y) {
                    unsigned top = (row_a[x0] * (255u - fx) + row_a[x1] * fx + 127u) / 255u;
                    unsigned bot = (row_b[x0] * (255u - fx) + row_b[x1] * fx + 127u) / 255u;
                    value = (top * (255u - fy) + bot * fy + 127u) / 255u;
                } else {
                    uint32_t sum = 0;
                    for (unsigned r = 0; r < rspan; ++r) {
                        const uint8_t* row = src + (size_t)(ry0 + r) * stride;
                        sum += (row[x0] * (255u - fx) + row[x1] * fx + 127u) / 255u;
                    }
                    value = (sum + rspan / 2) / rspan;
                }
            } else {
                unsigned cx0 = (unsigned)((uint64_t)dx * sw / dw);
                unsigned cx1 = (unsigned)((uint64_t)(dx + 1) * sw / dw);
                if (up_y) {
                    uint32_t s0 = 0, s1 = 0;
                    for (unsigned c = cx0; c < cx1; ++c) { s0 += row_a[c]; s1 += row_b[c]; }
                    unsigned cnt = cx1 - cx0;
                    unsigned top = (s0 + cnt / 2) / cnt, bot = (s1 + cnt / 2) / cnt;
                    value = (top * (255u - fy) + bot * fy + 127u) / 255u;
                } else {
                    uint32_t sum = 0;
                    for (unsigned r = 0; r < rspan; ++r) {
                        const uint8_t* row = src + (size_t)(ry0 + r) * stride;
                        for (unsigned c = cx0; c < cx1; ++c) sum += row[c];
                    }
                    uint32_t count = (uint32_t)rspan * (cx1 - cx0);
                    value = (sum + count / 2) / count;
                }
            }
            out[dx] = (uint8_t)value;
        }
    }
}

// 交错灰度+alpha 的面积平均缩小；入口上限保证 dw<=w、dh<=h（与 PNG 路径同式）。
// Area-average shrink for interleaved gray+alpha; entry caps guarantee dw<=w and dh<=h
// (same rule as the PNG path).
static void gray_alpha_resample(const uint8_t* ga, unsigned w, unsigned h,
                                uint8_t* dst, unsigned dw, unsigned dh) {
    for (unsigned dy = 0; dy < dh; ++dy) {
        unsigned ry0 = (unsigned)((uint64_t)dy * h / dh);
        unsigned rspan = (unsigned)((uint64_t)(dy + 1) * h / dh) - ry0;
        for (unsigned dx = 0; dx < dw; ++dx) {
            unsigned cx0 = (unsigned)((uint64_t)dx * w / dw);
            unsigned cx1 = (unsigned)((uint64_t)(dx + 1) * w / dw);
            uint32_t sum = 0;
            for (unsigned r = 0; r < rspan; ++r) {
                const uint8_t* row = ga + (size_t)(ry0 + r) * w * 2u;
                for (unsigned c = cx0; c < cx1; ++c) {
                    unsigned alpha = row[2u * c + 1];
                    sum += (row[2u * c] * alpha + 255u * (255u - alpha) + 127u) / 255u;
                }
            }
            uint32_t count = (uint32_t)rspan * (cx1 - cx0);
            dst[(size_t)dy * dw + dx] = (uint8_t)((sum + count / 2) / count);
        }
    }
}

typedef struct {
    FILE* f;                // 文件模式：读回调直接 fread/skip，压缩数据不整驻内存。
    const uint8_t* data;    // 内存模式。/ Memory mode.
    size_t len, at;
    uint8_t *out, *plane;   // 最终输出与解码平面（平面1:1落块，重采样到out）。/ Final output and decode plane (blocks land 1:1, resampled to out).
    unsigned width, height, scaled_width, scaled_height, blocks, scale;
    int64_t yielded_at;
} jpeg_io_t;

static size_t jpeg_read(JDEC* jd, uint8_t* out, size_t count) {
    jpeg_io_t* io = jd->device;
    if (io->f) {
        if (out) return fread(out, 1, count, io->f);
        return fseek(io->f, (long)count, SEEK_CUR) == 0 ? count : 0;
    }
    if (count > io->len - io->at) count = io->len - io->at;
    if (out) memcpy(out, io->data + io->at, count);
    io->at += count;
    return count;
}
static int jpeg_write(JDEC* jd, void* bitmap, JRECT* r) {
    jpeg_io_t* io = jd->device;
    // 越界块保持旧判失败：渐进文件在此中止才会落到 JPEGDEC 的 DC-only 回退，
    // 裁剪续解会把坏数据误判为成功。块内按 1:1 落平面，缩放交给统一重采样。
    // Out-of-range blocks keep the old failure verdict: progressive files must abort
    // here to reach JPEGDEC's DC-only fallback, and clipping would misread broken data
    // as success. Blocks land 1:1 in the plane; scaling belongs to the shared resample.
    if (r->right >= (int)io->scaled_width || r->bottom >= (int)io->scaled_height ||
        r->left > r->right || r->top > r->bottom) return 0;
    const uint8_t* rgb = bitmap;
    for (int y = r->top; y <= r->bottom; ++y) {
        uint8_t* out = io->plane + (size_t)y * io->scaled_width;
        const uint8_t* row = rgb + (size_t)(y - r->top) * (r->right - r->left + 1) * 3;
        for (int x = r->left; x <= r->right; ++x)
            out[x] = (uint8_t)((77u*row[(size_t)(x - r->left) * 3]+150u*row[(size_t)(x - r->left) * 3 + 1]+29u*row[(size_t)(x - r->left) * 3 + 2])>>8);
    }
    if (!(++io->blocks % 32)) book_image_yield(&io->yielded_at);
    return 1;
}
// 内存/文件两路共用的输出定尺与 tjpgd 缩放档选择。
// Shared output sizing and tjpgd scale pick for the memory and file paths.
// 解码平面成本上限：约百万像素量级，权衡整解小图的质量与整解大图的耗时。
// Decode-plane cost ceiling: around one megapixel, trading small-image quality
// against huge-image decode time.
#define JPEG_DECODE_PLANE_MAX (1000000ull)
static bool jpeg_pick_output(jpeg_io_t* io, unsigned src_w, unsigned src_h, size_t budget) {
    if (!src_w || !src_h || src_w > 8192 || src_h > 8192 ||
        (uint64_t)src_w * src_h > 16u * 1024u * 1024u) return false;
    io->width = src_w; io->height = src_h;
    if (io->width > 648) { io->height = io->height * 648 / io->width; io->width = 648; }
    if (io->height > 1000) { io->width = io->width * 1000 / io->height; io->height = 1000; }
    if (!io->width) io->width = 1;
    if (!io->height) io->height = 1;
    unsigned bw = io->width, bh = io->height;
    fit_budget(&bw, &bh, budget);
    io->width = bw; io->height = bh;
    if ((size_t)io->width * io->height > budget) return false;
    // 档位优先让平面覆盖目标（后续只缩小）；平面按解码成本封顶，超大源退到更粗档、
    // 由双线性放大承担。小图因此全分辨率解码，不再先丢一半细节。
    // Scales prefer planes covering the target (later passes only shrink); plane size is
    // capped by decode cost, so huge sources take coarser scales and upscale bilinearly.
    // Small images now decode full-resolution instead of losing half up front.
    io->scale = 0;
    uint64_t plane_cap = JPEG_DECODE_PLANE_MAX;
    uint64_t budget_cap = 4ull * (uint64_t)io->width * io->height + 65536ull;
    if (budget_cap < plane_cap) plane_cap = budget_cap;
    while (io->scale < 3 &&
           (uint64_t)(src_w >> io->scale) * (src_h >> io->scale) > plane_cap) ++io->scale;
    io->scaled_width = src_w >> io->scale; io->scaled_height = src_h >> io->scale;
    return true;
}
static bool jpeg_decode(const uint8_t* data, size_t len, size_t budget,
                        uint8_t** pixels, uint16_t* width, uint16_t* height) {
    void* work = heap_caps_malloc(8192, PSRAM_CAPS);
    if (!work) return false;
    JDEC jd;
    jpeg_io_t io = {.data=data, .len=len};
    bool ok = false;
    if (jd_prepare(&jd, jpeg_read, work, 8192, &io) != JDR_OK ||
        !jpeg_pick_output(&io, jd.width, jd.height, budget)) goto done;
    io.plane = image_alloc((size_t)io.scaled_width * io.scaled_height);
    io.out = heap_caps_malloc((size_t)io.width * io.height, PSRAM_CAPS);
    if (!io.plane || !io.out) goto done;
    memset(io.plane, 255, (size_t)io.scaled_width * io.scaled_height);
    if (jd_decomp(&jd, jpeg_write, (uint8_t)io.scale) == JDR_OK) {
        book_image_resample(io.plane, io.scaled_width, io.scaled_width, io.scaled_height,
                      io.out, io.width, io.height);
        *pixels = io.out; *width = (uint16_t)io.width; *height = (uint16_t)io.height;
        io.out = NULL; ok = true;
    }
done:
    image_free(io.plane);
    free(io.out);
    free(work);
    return ok;
}

// 基线 JPEG 文件流式解码：tjpgd 的读回调直接指向 FILE*（渐进式会在此失败并向上回退）。
// Baseline JPEG streamed from FILE*: the tjpgd read callback points straight at FILE*
// (progressive files fail here and the caller falls back).
static bool jpeg_decode_file(FILE* f, size_t budget,
                             uint8_t** pixels, uint16_t* width, uint16_t* height) {
    void* work = heap_caps_malloc(8192, PSRAM_CAPS);
    if (!work) return false;
    JDEC jd;
    jpeg_io_t io = {.f=f};
    bool ok = false;
    if (jd_prepare(&jd, jpeg_read, work, 8192, &io) != JDR_OK ||
        !jpeg_pick_output(&io, jd.width, jd.height, budget)) goto done;
    io.plane = image_alloc((size_t)io.scaled_width * io.scaled_height);
    io.out = heap_caps_malloc((size_t)io.width * io.height, PSRAM_CAPS);
    if (!io.plane || !io.out) goto done;
    memset(io.plane, 255, (size_t)io.scaled_width * io.scaled_height);
    if (jd_decomp(&jd, jpeg_write, (uint8_t)io.scale) == JDR_OK) {
        book_image_resample(io.plane, io.scaled_width, io.scaled_width, io.scaled_height,
                      io.out, io.width, io.height);
        *pixels = io.out; *width = (uint16_t)io.width; *height = (uint16_t)io.height;
        io.out = NULL; ok = true;
    }
done:
    image_free(io.plane);
    free(io.out);
    free(work);
    return ok;
}

// ---- 渐进式 JPEG：JPEGDEC 只解 DC 扫描 / Progressive JPEG: JPEGDEC decodes the DC scan ----
// stbi 解渐进要整幅系数缓冲（960×1280×3 分量即约 14.7MB），8MB PSRAM 装不下，封面因此
// 整类消失。JPEGDEC（三个参考固件同款 fork）只读 DC 扫描输出 1/8 灰度平面，代价是分辨率
// 与锐度降级；平面再经统一重采样器到目标（缩小面积平均、放大双线性）。
// stbi's progressive path needs the full coefficient buffer (960x1280x3 components alone
// is ~14.7MB), 8MB PSRAM cannot hold, so covers vanished wholesale. JPEGDEC (the same
// fork all three reference firmwares carry) reads the DC scan only and emits a 1/8 grayscale
// plane at reduced resolution and sharpness; the plane reaches the target through the
// shared resampler (box averaging when shrinking, bilinear when growing).
#include "JPEGDEC.h"
// ESP-IDF 下 JPEGDEC 只有 C++ 类；组件提供 extern "C" 包装（RAM 与 FILE 两路）。
// Under ESP-IDF JPEGDEC exposes only the C++ class; the component adds an
// extern "C" wrapper (RAM and FILE variants).
extern int jpegdec_gray_progressive(const uint8_t *data, int size, JPEG_DRAW_CALLBACK *draw,
                                    void *user, int *drawn_width, int *drawn_height);
extern int jpegdec_gray_file(FILE *file, int size, unsigned source_w, unsigned source_h,
                             unsigned out_w, unsigned out_h, bool progressive,
                             JPEG_DRAW_CALLBACK *draw, void *user, unsigned *scaled_w, unsigned *scaled_h);
// EXIF/APP 段按规范至多 64KB，SOF 必在其内；嗅探窗取 64KB 兼顾 SD 读量与分配。
// EXIF/APP segments are capped at 64KB by spec with SOF inside; a 64KB sniff window
// balances SD reads against the allocation.
#define JPEG_SNIFF_MAX (64u * 1024u)

typedef struct { uint8_t* plane; unsigned stride, height, draws; int64_t yielded_at; } jpegdec_plane_t;

static int jpegdec_collect(JPEGDRAW *draw) {
    jpegdec_plane_t *plane = draw->pUser;
    if (!plane || !plane->plane || draw->iBpp != 8 || !draw->pPixels || draw->iWidth < 1) return 0;
    const uint8_t *src = (const uint8_t *)draw->pPixels;
    const int columns = draw->iWidthUsed < draw->iWidth ? draw->iWidthUsed : draw->iWidth;
    for (int row = 0; row < draw->iHeight; ++row) {
        size_t y = (size_t)draw->y + row;
        if (draw->y + row < 0 || y >= plane->height) break;
        for (int column = 0; column < columns; ++column) {
            if (draw->x + column < 0) continue;
            size_t x = (size_t)draw->x + column;
            if (x >= plane->stride) continue;
            plane->plane[y * plane->stride + x] = src[(size_t)row * draw->iWidth + column];
        }
    }
    if (!(++plane->draws % 64)) book_image_yield(&plane->yielded_at);
    return 1;
}

// 在 SOI 后扫段标记找首个 SOF：给出尺寸与渐进标记（SOF2）。
// Scan segment markers after SOI for the first SOF: yields dimensions and the
// progressive flag (SOF2).
static bool jpeg_sof_at(const uint8_t* p, size_t n, unsigned* w, unsigned* h, bool* progressive) {
    if (n < 4 || p[0] != 0xff || p[1] != 0xd8) return false;
    size_t at = 2;
    while (at + 4 <= n) {
        if (p[at] != 0xff) return false;
        unsigned m = p[at + 1];
        if (m == 0xd8 || m == 0x01 || (m >= 0xd0 && m <= 0xd7)) { at += 2; continue; }
        if (m == 0xd9 || m == 0xda) return false; // 熵数据前无帧头 / entropy data before any frame
        size_t len = (size_t)p[at + 2] << 8 | p[at + 3];
        if (len < 2 || at + 2 + len > n) return false;
        if (m >= 0xc0 && m <= 0xcf && m != 0xc4 && m != 0xc8 && m != 0xcc) {
            if (len < 7) return false;
            if (progressive) *progressive = m == 0xc2;
            if (h) *h = (unsigned)p[at + 5] << 8 | p[at + 6];
            if (w) *w = (unsigned)p[at + 7] << 8 | p[at + 8];
            return true;
        }
        at += 2 + len;
    }
    return false;
}
static bool jpeg_sof_file(FILE* f, unsigned* w, unsigned* h, bool* progressive) {
    uint8_t* head = heap_caps_malloc(JPEG_SNIFF_MAX, PSRAM_CAPS);
    if (!head) return false;
    // 前一解码尝试已把 FILE* 推到流中段；嗅探必须回到 SOI 重新扫标记。
    // A prior decode attempt left FILE* mid-stream; sniffing must rewind to SOI first.
    bool ok = fseek(f, 0, SEEK_SET) == 0;
    size_t got = ok ? fread(head, 1, JPEG_SNIFF_MAX, f) : 0;
    ok = ok && jpeg_sof_at(head, got, w, h, progressive);
    free(head);
    return ok;
}

// 1/8 小平面经统一重采样器到输出：缩小面积平均、放大双线性（与 PNG/基线路径同规则）。
// The 1/8 plane reaches the output through the shared resampler: box averaging when
// shrinking, bilinear when growing (same rule as the PNG and baseline paths).
static void jpeg_progressive_scale(const jpegdec_plane_t* pl, uint8_t* out, unsigned dw, unsigned dh) {
    book_image_resample(pl->plane, pl->stride, pl->stride, pl->height, out, dw, dh);
}

static bool jpeg_progressive_plane(unsigned sw, unsigned sh, size_t budget, jpegdec_plane_t* pl,
                                   uint8_t** out, unsigned* dw, unsigned* dh) {
    jpeg_io_t io = {0};
    if (!jpeg_pick_output(&io, sw, sh, budget)) return false;
    pl->stride = (sw + 7) / 8;
    pl->height = (sh + 7) / 8;
    pl->plane = image_alloc((size_t)pl->stride * pl->height);
    *out = heap_caps_malloc((size_t)io.width * io.height, PSRAM_CAPS);
    *dw = io.width; *dh = io.height;
    if (!pl->plane || !*out) return false;
    memset(pl->plane, 0xFF, (size_t)pl->stride * pl->height);
    return true;
}

static bool jpeg_progressive_decode(const uint8_t* data, size_t len, size_t budget,
                                    uint8_t** pixels, uint16_t* width, uint16_t* height) {
    unsigned sw = 0, sh = 0; bool prog = false;
    if (!jpeg_sof_at(data, len, &sw, &sh, &prog) || !prog) return false;
    jpegdec_plane_t pl = {0};
    uint8_t* out = NULL; unsigned dw = 0, dh = 0;
    bool ok = jpeg_progressive_plane(sw, sh, budget, &pl, &out, &dw, &dh) &&
        jpegdec_gray_progressive(data, (int)len, jpegdec_collect, &pl, NULL, NULL) != 0;
    if (ok) jpeg_progressive_scale(&pl, out, dw, dh);
    image_free(pl.plane);
    if (!ok) { free(out); return false; }
    *pixels = out; *width = (uint16_t)dw; *height = (uint16_t)dh;
    return true;
}

static bool jpeg_progressive_decode_file(FILE* f, size_t len, unsigned sw, unsigned sh, size_t budget,
                                         uint8_t** pixels, uint16_t* width, uint16_t* height) {
    jpegdec_plane_t pl = {0};
    uint8_t* out = NULL; unsigned dw = 0, dh = 0, plane_w = 0, plane_h = 0;
    bool ok = jpeg_progressive_plane(sw, sh, budget, &pl, &out, &dw, &dh) &&
        fseek(f, 0, SEEK_SET) == 0 &&
        jpegdec_gray_file(f, (int)len, sw, sh, dw, dh, true, jpegdec_collect, &pl, &plane_w, &plane_h) != 0;
    if (ok) jpeg_progressive_scale(&pl, out, dw, dh);
    image_free(pl.plane);
    if (!ok) { free(out); return false; }
    *pixels = out; *width = (uint16_t)dw; *height = (uint16_t)dh;
    return true;
}

bool book_image_decode(const uint8_t* data, size_t len, size_t budget,
                       uint8_t** pixels, uint16_t* width, uint16_t* height) {
    if (!pixels || !width || !height) return false;
    *pixels = NULL; *width = *height = 0;
    if (!data || !len || len > 2u * 1024u * 1024u || allocated) return false;
    if (png_decode(data,len,budget,pixels,width,height)) return true;
    // 基线 JPEG 优先使用 MCU 分块缩小；渐进式走 JPEGDEC 的 DC-only 1/8；stbi 兜底奇形文件。
    // Prefer MCU-scaled baseline JPEG, then JPEGDEC's DC-only 1/8 for progressive, with
    // bounded full stbi decoding as the last resort for odd files.
    if (len >= 2 && data[0] == 0xff && data[1] == 0xd8) {
        if (jpeg_decode(data, len, budget, pixels, width, height)) return true;
        if (jpeg_progressive_decode(data, len, budget, pixels, width, height)) return true;
    }
    int w, h, channels;
    // 兜底路径同样按通道估峰值（stbi 内部再要一份灰度输出）；上限对 RGBA 与旧 1MP 等效。
    // The fallback estimates its peak per channel too (stbi adds its own gray buffer);
    // for RGBA this matches the old 1-megapixel bound.
    if (!stbi_info_from_memory(data, (int)len, &w, &h, &channels) ||
        w <= 0 || h <= 0 || (uint64_t)w * h * (unsigned)channels > 4u * 1024u * 1024u) return false;
    int dw = w, dh = h;
    if (dw > 648) { dh = (int)((int64_t)dh * 648 / dw); dw = 648; }
    if (dh > 1000) { dw = (int)((int64_t)dw * 1000 / dh); dh = 1000; }
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    unsigned bw = dw, bh = dh;
    fit_budget(&bw, &bh, budget);
    dw = (int)bw; dh = (int)bh;
    size_t size = (size_t)dw * dh;
    if (size > budget) return false;
    uint8_t* gray = stbi_load_from_memory(data, (int)len, &w, &h, &channels, 2);
    if (!gray) return false;
    uint8_t* out = heap_caps_malloc(size, PSRAM_CAPS);
    if (out) gray_alpha_resample(gray, (unsigned)w, (unsigned)h, out, (unsigned)dw, (unsigned)dh);
    stbi_image_free(gray);
    if (!out) return false;
    *pixels = out; *width = (uint16_t)dw; *height = (uint16_t)dh;
    return true;
}

bool book_image_decode_file(const char* path, size_t budget,
                            uint8_t** pixels, uint16_t* width, uint16_t* height) {
    if (!pixels || !width || !height) return false;
    *pixels = NULL; *width = *height = 0;
    if (!path || allocated) return false;
    struct stat st;
    if (stat(path, &st) || st.st_size <= 0 || (uint64_t)st.st_size > IMAGE_FILE_MAX) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    uint8_t magic[8] = {0};
    size_t got = fread(magic, 1, sizeof(magic), f);
    rewind(f);
    bool ok = false;
    if (got >= 8 && !memcmp(magic, "\x89PNG\r\n\x1a\n", 8)) {
        ok = png_decode_file(f, (size_t)st.st_size, budget, pixels, width, height);
    } else if (got >= 2 && magic[0] == 0xff && magic[1] == 0xd8) {
        ok = jpeg_decode_file(f, budget, pixels, width, height);
        if (!ok) {
            // tjpgd 只认基线；渐进式大文件走 JPEGDEC 的 FILE 回调流式 DC-only 解码。
            // tjpgd is baseline-only; large progressive files stream through JPEGDEC's
            // FILE-callback DC-only decoding.
            unsigned sw = 0, sh = 0; bool prog = false;
            if (jpeg_sof_file(f, &sw, &sh, &prog) && prog)
                ok = jpeg_progressive_decode_file(f, (size_t)st.st_size, sw, sh, budget, pixels, width, height);
        }
        // 仍未成功且文件不大：整读回退让 stbi 兜底（含非渐进的奇异基线）。
        // Still failing on a small file: whole-read fallback lets stbi take over
        // (including odd baselines).
        if (!ok && st.st_size <= 2u * 1024u * 1024u) {
            uint8_t* data = heap_caps_malloc((size_t)st.st_size, PSRAM_CAPS);
            if (data && fseek(f, 0, SEEK_SET) == 0 && fread(data, 1, (size_t)st.st_size, f) == (size_t)st.st_size)
                ok = book_image_decode(data, (size_t)st.st_size, budget, pixels, width, height);
            free(data);
        } else if (!ok) {
            IMG_LOGW("jpeg file %s bytes=%lld decode failed", path, (long long)st.st_size);
        }
    }
    fclose(f);
    return ok;
}
