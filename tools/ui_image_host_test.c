/* SPDX-FileCopyrightText: 2026 mindreset
 * 中文：图片装载/裁切/抖动模块的主机回归：路径与扩展名边界、抖动端点与稳定性、
 * 居中裁切铺满不拉伸、整链失败不动缓冲。
 * English: Host regression for the image load/crop/dither module: path and extension
 * bounds, dither endpoints and stability, aspect-preserving cover crop, and untouched
 * buffers on whole-chain failures.
 * 冻结：仅供主机验证。/ Frozen: Host validation only.
 */
#include "ui_image.h"
#include "book_image.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void* heap_caps_calloc(size_t n, size_t size, unsigned caps) { (void)caps; return calloc(n, size); }
void* heap_caps_malloc(size_t size, unsigned caps) { (void)caps; return malloc(size); }
// 主机测试只走拒绝路径与裁切；真实解码与装载转交由 book_image 自身的测试覆盖
// （宿主机没有 /sdcard 目录，load_path 的成功分支无法在主机侧触达）。
// Host tests exercise rejection paths and cropping only; real decoding and the
// load hand-off stay covered by book_image's own tests (the host has no /sdcard
// directory, so load_path's success branch is unreachable here).
// （宿主机没有 /sdcard 目录，load_path 的成功分支无法在主机侧触达）。
// (The host has no /sdcard directory, so load_path's success branch is unreachable here.)
bool book_image_decode(const uint8_t* data, size_t len, size_t budget,
                       uint8_t** pixels, uint16_t* width, uint16_t* height) {
    (void)data; (void)len; (void)budget; (void)pixels; (void)width; (void)height;
    return false;
}
bool book_image_decode_file(const char* path, size_t budget,
                            uint8_t** pixels, uint16_t* width, uint16_t* height) {
    (void)path; (void)budget; (void)pixels; (void)width; (void)height;
    return false;
}

static int s_draws;
void epd_draw_pixel(int x, int y, uint8_t color, uint8_t *fb) {
    assert(x >= 0 && x < 684 && y >= 0 && y < 1216);
    fb[(size_t)y * 684 + x] = color;
    ++s_draws;
}

int main(void) {
    // 抖动：端点保持纯黑纯白，同坐标稳定，值域合法。
    // Dither: pure endpoints, per-coordinate stability, legal range.
    assert(ui_image_dither(0, 7, 9) == 0 && ui_image_dither(255, 3, 2) == 255);
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
        uint8_t a = ui_image_dither(128, x, y), b = ui_image_dither(128, x, y);
        assert(a == b);
        for (int g = 0; g < 256; ++g) {
            uint8_t v = ui_image_dither((uint8_t)g, x, y);
            assert(v % 17u == 0);
        }
    }
    // 自动色阶：窄跨度拉伸到满幅、满幅与近单色不动。
    // Auto levels: narrow spans stretch to full range; full-range and near-mono images stay.
    {
        uint8_t ramp[256];
        for (int i = 0; i < 256; ++i) ramp[i] = (uint8_t)(60 + i * 60 / 255); // 60..120
        ui_image_auto_levels(ramp, sizeof(ramp));
        assert(ramp[0] == 0 && ramp[255] == 255);
        uint8_t full[256];
        for (int i = 0; i < 256; ++i) full[i] = (uint8_t)i;
        ui_image_auto_levels(full, sizeof(full));
        for (int i = 0; i < 256; ++i) assert(full[i] == (uint8_t)i);
        uint8_t flat[256];
        memset(flat, 0x80, sizeof(flat));
        ui_image_auto_levels(flat, sizeof(flat));
        for (int i = 0; i < 256; ++i) assert(flat[i] == 0x80);
        uint8_t mono[2] = {1, 2};
        ui_image_auto_levels(mono, sizeof(mono)); // 数量不足直接返回 / Too few pixels return at once
        assert(mono[0] == 1 && mono[1] == 2);
    }
    // 整屏裁切逐像素对照原公式，优化坐标计算不得改变抖动或灰阶。
    // Compare full-screen crops pixel-for-pixel with the original formula, preserving dither and gray levels.
    {
        const int width=371,height=563;uint8_t* source=malloc((size_t)width*height);
        uint8_t* frame=malloc(684u*1216u);assert(source&&frame);
        for(int i=0;i<width*height;++i)source[i]=(uint8_t)(i*17+i/width*13);
        EpdRect areas[]={{0,0,684,1024},{22,31,400,730},{42,80,600,300}};
        for(unsigned i=0;i<sizeof(areas)/sizeof(areas[0]);++i){
            EpdRect area=areas[i];memset(frame,0x73,684u*1216u);ui_image_blit(frame,source,width,height,area);
            uint32_t cw=width,ch=height;
            if((uint64_t)cw*area.height>(uint64_t)ch*area.width)cw=((uint64_t)ch*area.width+area.height-1)/area.height;
            else ch=((uint64_t)cw*area.height+area.width-1)/area.width;
            uint32_t cx=(width-cw)/2,cy=(height-ch)/2;
            for(int y=0;y<area.height;++y)for(int x=0;x<area.width;++x){
                uint32_t sx=cx+(uint64_t)x*cw/area.width,sy=cy+(uint64_t)y*ch/area.height;
                assert(frame[(area.y+y)*684+area.x+x]==ui_image_dither(source[sy*width+sx],area.x+x,area.y+y));
            }
        }
        free(source);free(frame);
    }
    // 装载边界：非 /sdcard、坏扩展、不存在文件一律拒绝且输出清空。
    // Load bounds: non-/sdcard paths, bad extensions and missing files all reject with cleared outputs.
    uint8_t *gray = (uint8_t *)0x1;
    uint16_t w = 9, h = 9;
    assert(!ui_image_load_path(NULL, &gray, &w, &h) && gray == NULL && !w && !h);
    assert(!ui_image_load_path("/flash/a.png", &gray, &w, &h) && !gray);
    assert(!ui_image_load_path("/sdcard/a.gif", &gray, &w, &h) && !gray);
    assert(!ui_image_load_path("/sdcard/missing.png", &gray, &w, &h) && !gray);
    assert(!ui_image_draw_path(NULL, "/sdcard/missing.png", (EpdRect){0, 0, 684, 1216}));
    // 裁切铺满：2x1 灰度图铺进 4x2 区域，横向全覆盖、纵向重复，不越界。
    // Cover crop: a 2x1 gray image fills a 4x2 area, full width, repeated height, staying inside.
    uint8_t source[2] = {0x00, 0xff};
    uint8_t fb[684 * 300];
    memset(fb, 0x55, sizeof(fb));
    s_draws = 0;
    ui_image_blit(fb, source, 2, 1, (EpdRect){100, 50, 4, 2});
    assert(s_draws == 8);
    int black = 0, white = 0;
    for (int y = 50; y < 52; ++y) for (int x = 100; x < 104; ++x) {
        uint8_t v = fb[(size_t)y * 684 + x];
        black += v == 0; white += v == 255;
    }
    assert(black == 4 && white == 4);
    assert(fb[0] == 0x55 && fb[(size_t)52 * 684 + 100] == 0x55);
    // 壁纸侧车：写入-读回一致；身份（路径/大小/修改时间）变化、文件缺失与截断损坏一律未命中。
    // Wallpaper sidecar: store/load roundtrip; identity changes (path/size/mtime),
    // missing files and truncated corruption all count as misses.
    const char* cache = "/tmp/pico-wp-cache";
    remove(cache);
    uint8_t snap[12];
    for (size_t i = 0; i < sizeof(snap); ++i) snap[i] = (uint8_t)(i * 20);
    assert(ui_image_wallpaper_cache_store(cache, "/sdcard/山景.png", 111, 222, snap, 4, 3));
    uint8_t* loaded = (uint8_t*)0x1;
    uint16_t lw = 9, lh = 9;
    assert(ui_image_wallpaper_cache_load(cache, "/sdcard/山景.png", 111, 222, &loaded, &lw, &lh));
    assert(loaded && lw == 4 && lh == 3 && !memcmp(loaded, snap, sizeof(snap)));
    free(loaded);
    loaded = (uint8_t*)0x1;
    assert(!ui_image_wallpaper_cache_load(cache, "/sdcard/山景.png", 112, 222, &loaded, &lw, &lh) && !loaded && !lw && !lh);
    assert(!ui_image_wallpaper_cache_load(cache, "/sdcard/山景.png", 111, 223, &loaded, &lw, &lh) && !loaded);
    assert(!ui_image_wallpaper_cache_load(cache, "/sdcard/其他.png", 111, 222, &loaded, &lw, &lh) && !loaded);
    assert(!ui_image_wallpaper_cache_load("/tmp/pico-wp-none", "/sdcard/山景.png", 111, 222, &loaded, &lw, &lh) && !loaded);
    FILE* cut = fopen(cache, "r+b");
    assert(cut);
    assert(ftruncate(fileno(cut), 6) == 0);
    fclose(cut);
    assert(!ui_image_wallpaper_cache_load(cache, "/sdcard/山景.png", 111, 222, &loaded, &lw, &lh) && !loaded);
    remove(cache);
    puts("ui image host tests passed");
    return 0;
}
