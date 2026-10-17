/*
 * SPDX-FileCopyrightText: 2026 mindreset
 *
 * 中文：图片查看与锁屏壁纸共用的 TF 图片装载、居中裁切与 16 级有序抖动绘制。
 * English: Shared TF image loading, center-cropping and 16-level ordered-dither
 * drawing for the image viewer and lock wallpaper.
 *
 * 冻结：只接受 /sdcard/ 下的 PNG/JPG/JPEG；失败不动帧缓冲；抖动仅用于图片路径，
 * 文字与界面线条不得经过。决策修订（实机反馈）：普通手机照片普遍大于 2 MiB，
 * 解码已改为 FILE* 流式（压缩数据不整驻内存），装载的字节上限放宽到 50 MiB
 * 仅作异常防护——内存约束由解码端的尺寸与峰值预算承担。
 * Frozen: Accept only /sdcard/ PNG/JPG/JPEG; failures leave the framebuffer
 * untouched; the dither serves image paths only, never text or UI rules.
 * Decision revision (device feedback): ordinary phone photos exceed 2 MiB and
 * decoding now streams from FILE* (compressed data never resides whole), so the
 * load byte cap relaxes to 50 MiB as a pathological guard — memory is bounded by
 * the decoder's dimension and peak-budget rules.
 * 实机修订：阅读中锁屏解码壁纸在内存压力下偶发失败回退默认图；设壁纸时把已解码灰度
 * 快照存为 TF 侧车，锁屏直读快照免解码，快照与源路径/大小/修改时间不符则回退实时解码。
 * Device revision: locking while reading occasionally failed wallpaper decode under memory
 * pressure and fell back to the default; setting a wallpaper now persists the decoded gray
 * as a TF sidecar the lock reads directly, falling back to live decoding on identity mismatch.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "epdiy.h"
#include "ui_dither.h"

#ifndef UI_IMAGE_MEDIA_ROOT
#define UI_IMAGE_MEDIA_ROOT "/sdcard"
#endif

/// 设壁纸时写入的灰度快照固定落点。/ Where the wallpaper gray snapshot lives.
#define UI_IMAGE_WALLPAPER_CACHE UI_IMAGE_MEDIA_ROOT "/.wallpaper"

/// 就地 0.5%/99.5% 百分位对比度拉伸；跨度不足或已满幅的图不动，纯色图保持原样。
/// In-place 0.5%/99.5% percentile contrast stretch; narrow or already full spans stay, flat art unchanged.
void ui_image_auto_levels(uint8_t* gray, size_t count);

/// 读取并解码 TF 图片为灰度（输出不超过 684x1096 像素）；*gray 由调用方 free。
/// Read and decode a TF image to grayscale (at most 684x1096 pixels); caller frees *gray.
bool ui_image_load_path(const char* path, uint8_t** gray, uint16_t* width, uint16_t* height);

/// 把灰度图按原比例居中裁切铺满区域并抖动绘制到帧缓冲。
/// Cover-crop the grayscale image at its original aspect into the area and dither it onto the framebuffer.
void ui_image_blit(uint8_t* fb, const uint8_t* gray, uint16_t width, uint16_t height, EpdRect area);

/// 装载并整块绘制；成功返回 true。/ Load and draw in one call; true on success.
bool ui_image_draw_path(uint8_t* fb, const char* path, EpdRect area);

/// 把已解码灰度写入壁纸侧车（含源身份：路径哈希/大小/修改时间）；*gray 内容为快照本体。
/// Persist decoded gray into the wallpaper sidecar keyed by source identity (path hash/size/mtime).
bool ui_image_wallpaper_cache_store(const char* cache_path, const char* source_path,
                                    long size, long mtime, const uint8_t* gray, uint16_t width, uint16_t height);
/// 读壁纸侧车；身份不符、文件缺失或损坏返回 false 且不分配。/gray 由调用方 free。
/// Load the wallpaper sidecar; identity mismatch, missing or corrupt files return false
/// without allocating. Caller frees *gray.
bool ui_image_wallpaper_cache_load(const char* cache_path, const char* source_path,
                                    long size, long mtime, uint8_t** gray, uint16_t* width, uint16_t* height);
