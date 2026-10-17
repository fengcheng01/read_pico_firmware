/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * 中文：有界 JPEG/PNG 解码与灰度缩放，调用方持有输出。
 * English: Bounded JPEG/PNG decoding and grayscale resizing; caller owns output.
 * 冻结：不访问显示；失败交还空输出，不影响正文阅读。文件入口只读调用方路径，
 * 不写不删、不改文件位置语义之外的任何状态——决策变更：查看端照片普遍大于 2 MiB，
 * 内存路径的整读上限挡住了正常使用，流式文件入口（对齐参考固件）是唯一解。
 * Frozen: No display access; failure leaves empty output and must not block text reading.
 * The file entry only reads the caller-supplied path and never writes or deletes.
 * Decision change: ordinary phone photos exceed the whole-file 2 MiB memory cap,
 * so the streaming file entry (matching the reference firmwares) is required.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// 内存解码：输入至多2MiB（EPUB 插图来自 zip 缓冲）；基线JPEG至多16M像素，其余至多1M像素。
/// Decode from memory: input <=2MiB (EPUB illustrations come from zip buffers); baseline
/// JPEG <=16M pixels, others <=1M pixels.
bool book_image_decode(const uint8_t* data, size_t len, size_t budget,
                       uint8_t** pixels, uint16_t* width, uint16_t* height);

/// 文件解码：PNG 与基线 JPEG 从 FILE* 流式喂入，压缩数据不整驻内存；文件至多 50MiB，
/// 尺寸与输出预算与内存路径一致。渐进 JPEG（内存与文件两路）由 JPEGDEC 只解 DC 扫描，
/// 输出 1/8 分辨率（清晰度降级但可用）；小的奇异文件仍自动回退整读路径。
/// Decode from a file: PNG and baseline JPEG stream straight from FILE* so compressed
/// data never resides whole in RAM; file <=50MiB with the same dimension/output budget
/// as memory. Progressive JPEGs (both paths) decode via JPEGDEC's DC-scan-only mode at
/// 1/8 resolution (reduced sharpness but usable); small odd files still fall back to
/// the whole-file path automatically.
bool book_image_decode_file(const char* path, size_t budget,
                            uint8_t** pixels, uint16_t* width, uint16_t* height);

/// 平滑重采样 8 位灰度：缩小走面积平均、放大走双线性，两轴各自判定可混合；
/// stride 为源行距。与解码共用同一采样规则，供封面等外部消费者使用。
/// Smooth 8-bit gray resampling: box-average when shrinking, bilinear when growing,
/// decided per axis so mixed ratios work; stride is the source row pitch. Shares the
/// decoder's sampling rules for outside consumers such as covers.
void book_image_resample(const uint8_t* src, unsigned stride, unsigned sw, unsigned sh,
                         uint8_t* dst, unsigned dw, unsigned dh);
