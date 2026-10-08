/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：真实相位准备和环形队列回归；只替换扫描与任务通知边界。
 * English: Regress production phase preparation and circular queues, mocking scan and task-notification seams only.
 */
#include "render_context.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef void (*frame_done_func_t)(void*);
static RenderContext_t* active;
static frame_done_func_t done_cb;
static int notifications, joins, phases, panel_mode, failure, warnings;
static bool line_callback_active;
static const EpdWaveformPhases waveform_phases = {.phases = 36};
static const EpdWaveformPhases* phase_ranges[] = {&waveform_phases};
static const EpdWaveformMode waveform_mode = {.range_data = phase_ranges};
static const EpdWaveformMode* waveform_modes[] = {&waveform_mode};
static const EpdWaveform waveform = {.mode_data = waveform_modes};

void phase_test_log(const char* tag, const char* format, ...) {
    assert(!strcmp(tag, "epdiy"));
    assert(strstr(format, "phase queue pending:"));
    assert(notifications % NUM_RENDER_THREADS == 0);
    warnings++;
}

static void build_lut(uint8_t* lut, const EpdWaveformPhases* values, int frame) {
    assert(values == &waveform_phases && frame == active->current_frame);
    // 本相位通知尚未开始；上一相位两个生产者必须已汇合。
    // This phase has not notified feeders; both previous producers must have joined.
    assert(notifications == phases * NUM_RENDER_THREADS);
    assert(phases == 0 || joins == NUM_RENDER_THREADS);
    if (lut) lut[0] = (uint8_t)frame;
    phases++;
}

static void put_row(LineQueue_t* queue, uint8_t value) {
    uint8_t* row = lq_current(queue);
    assert(row); row[0] = value; lq_commit(queue);
}

static uint8_t take_row(LineQueue_t* queue) {
    uint8_t row = 0; assert(lq_read(queue, &row) == 0); return row;
}

static void epd_lcd_frame_done_cb(frame_done_func_t cb, void* ctx) {
    done_cb = cb; if (ctx) active = ctx;
}
static void epd_lcd_line_source_cb(void* cb, void* ctx) {
    (void)ctx; line_callback_active = cb != NULL;
}
static void xSemaphoreGiveFromISR(int sem, int* awoken) {(void)sem; (void)awoken;}
void epd_set_mode(bool value) {panel_mode = value;}
static int xPortGetCoreID(void) {return 0;}
static void xTaskNotifyGive(int task) {
    (void)task;
    if (notifications % NUM_RENDER_THREADS == 0) joins = 0;
    notifications++; line_callback_active = true;
}
static void xSemaphoreTake(int sem, int timeout) {
    (void)timeout;
    if (sem == 10) {
        // 使用真实生产和消费；正常异常场景留一行，下一相位只观察而不清除。
        // Use real production and consumption; the anomalous normal scenario leaves one row for observation without clearing.
        for (int i = 0; i < NUM_RENDER_THREADS; i++) {
            LineQueue_t* queue = &active->line_queues[i];
            while (lq_pending(queue)) take_row(queue);
            put_row(queue, 0x40); put_row(queue, 0x41);
            if (failure == 0 || (failure == 3 && phases > 1)) {
                take_row(queue); take_row(queue);
            } else if (failure >= 3) take_row(queue);
        }
        active->lines_consumed = failure == 1 ? 676 : 688;
        if (failure == 2) active->error = EPD_DRAW_EMPTY_LINE_QUEUE;
        assert(done_cb); done_cb(active);
    } else {
        assert(!done_cb && !line_callback_active);
        // 失败行也须留到两个生产者汇合；过早 reset 会让这条断言失败。
        // Failed rows must survive both producer joins; an early reset fails this assertion.
        if (failure == 1 || failure == 2)
            for (int i = 0; i < 2; i++) assert(lq_pending(&active->line_queues[i]) == 2);
        joins++;
    }
}
static void vTaskDelay(int ticks) {(void)ticks;}

#include "lcd_scheduler.inc"

static RenderContext_t make_context(uint8_t* threads, uint8_t* lut) {
    RenderContext_t ctx = {.lines_total = 688, .cycle_frames = 36, .frame_done = 10,
        .feed_done_smphr = {11, 12}, .feed_tasks = {0, 1}, .waveform = &waveform,
        .lut_build_func = build_lut, .line_threads = threads, .conversion_lut = lut};
    for (int i = 0; i < NUM_RENDER_THREADS; i++) {
        ctx.line_queues[i] = lq_init(8, 1);
        for (int j = 0; j < 8; j++) ctx.line_queues[i].bufs[j][0] = 0;
    }
    return ctx;
}

static void free_context(RenderContext_t* ctx) {
    for (int i = 0; i < NUM_RENDER_THREADS; i++) lq_free(&ctx->line_queues[i]);
}

static void test_phase_preparation(void) {
    uint8_t threads[688], lut = 0;
    RenderContext_t ctx = make_context(threads, &lut); active = &ctx;
    EpdPhaseQueueDiagnostics diagnostics;
    epd_reset_phase_queue_diagnostics(); notifications = joins = phases = warnings = 0;
    epd_get_phase_queue_diagnostics(NULL);
    // 空队列相位保留默认时序、真实 LUT 构建和线程表初始化。
    // Empty entry preserves default timing, real LUT construction and thread-map initialization.
    ctx.lines_consumed = 42; ctx.lines_prepared = 87;
    prepare_context_for_next_frame(&ctx);
    assert(ctx.frame_time == 120 && !ctx.lines_consumed && !ctx.lines_prepared);
    for (int i = 0; i < 688; i++) assert(threads[i] == i % NUM_RENDER_THREADS);
    epd_get_phase_queue_diagnostics(&diagnostics);
    assert(diagnostics.examined_phases == 1 && !diagnostics.stale_phases && !warnings);

    // 先消费至环尾再生产环绕行，准备下一相位不能改索引、行内容或原错误。
    // Consume to the ring end, then wrap production; preparation must preserve indices, rows and prior errors.
    for (int i = 0; i < 6; i++) {put_row(&ctx.line_queues[0], (uint8_t)i); take_row(&ctx.line_queues[0]);}
    put_row(&ctx.line_queues[0], 0x61); put_row(&ctx.line_queues[0], 0x62); put_row(&ctx.line_queues[0], 0x63);
    put_row(&ctx.line_queues[1], 0x71); put_row(&ctx.line_queues[1], 0x72);
    assert(ctx.line_queues[0].current == 1 && ctx.line_queues[0].last == 6);
    assert(lq_pending(&ctx.line_queues[0]) == 3 && lq_pending(&ctx.line_queues[1]) == 2);
    int current[2] = {ctx.line_queues[0].current, ctx.line_queues[1].current};
    int last[2] = {ctx.line_queues[0].last, ctx.line_queues[1].last};
    uint8_t saved[2][8];
    for (int q = 0; q < 2; q++) for (int j = 0; j < 8; j++) saved[q][j] = ctx.line_queues[q].bufs[j][0];
    ctx.error = EPD_DRAW_INVALID_PACKING_MODE;
    const int times[] = {148}; ctx.phase_times = times;
    notifications = NUM_RENDER_THREADS; joins = NUM_RENDER_THREADS;
    prepare_context_for_next_frame(&ctx);
    assert(ctx.frame_time == 148 && ctx.error == EPD_DRAW_INVALID_PACKING_MODE && ctx.current_frame == 0);
    for (int q = 0; q < 2; q++) {
        assert(ctx.line_queues[q].current == current[q] && ctx.line_queues[q].last == last[q]);
        for (int j = 0; j < 8; j++) assert(saved[q][j] == ctx.line_queues[q].bufs[j][0]);
    }
    epd_get_phase_queue_diagnostics(&diagnostics);
    assert(diagnostics.examined_phases == 2 && diagnostics.stale_phases == 1 && warnings == 1);
    assert(diagnostics.max_pending_lines == 5 && diagnostics.last_pending_lines[0] == 3 && diagnostics.last_pending_lines[1] == 2);
    assert(take_row(&ctx.line_queues[0]) == 0x61 && take_row(&ctx.line_queues[0]) == 0x62 && take_row(&ctx.line_queues[0]) == 0x63);
    assert(take_row(&ctx.line_queues[1]) == 0x71 && take_row(&ctx.line_queues[1]) == 0x72);

    ctx.mode = MODE_EPDIY_MONOCHROME; notifications = 2 * NUM_RENDER_THREADS;
    prepare_context_for_next_frame(&ctx);
    assert(ctx.frame_time == MONOCHROME_FRAME_TIME);
    epd_get_phase_queue_diagnostics(&diagnostics);
    assert(diagnostics.examined_phases == 3 && diagnostics.stale_phases == 1 && diagnostics.max_pending_lines == 5 && warnings == 1);
    epd_reset_phase_queue_diagnostics(); epd_get_phase_queue_diagnostics(&diagnostics);
    assert(!diagnostics.examined_phases && !diagnostics.stale_phases && !diagnostics.max_pending_lines);
    assert(!diagnostics.last_pending_lines[0] && !diagnostics.last_pending_lines[1]);
    LineQueue_t absent = {0}; assert(lq_pending(&absent) == 0);
    free_context(&ctx);
}

int main(void) {
    test_phase_preparation();
    for (failure = 0; failure < 5; failure++) {
        uint8_t threads[688], lut = 0;
        RenderContext_t ctx = make_context(threads, &lut);
        notifications = joins = phases = warnings = 0;
        epd_reset_phase_queue_diagnostics();
        lcd_do_update(&ctx);
        assert(panel_mode == 0 && !done_cb && !line_callback_active);
        EpdPhaseQueueDiagnostics diagnostics; epd_get_phase_queue_diagnostics(&diagnostics);
        if (failure == 1 || failure == 2) {
            assert(ctx.error & EPD_DRAW_EMPTY_LINE_QUEUE);
            assert(phases == 1 && notifications == 2 && joins == 2);
            for (int i = 0; i < 2; i++) assert(!lq_pending(&ctx.line_queues[i]));
            assert(diagnostics.examined_phases == 1 && !diagnostics.stale_phases);
        } else {
            assert(!ctx.error && phases == 36 && ctx.current_frame == 36 && notifications == 72);
            assert(diagnostics.examined_phases == 36);
            assert(diagnostics.stale_phases == (failure == 4 ? 35U : failure == 3 ? 1U : 0U));
            assert(warnings == (failure == 4 ? 7 : failure == 3 ? 1 : 0));
        }
        free_context(&ctx);
    }
    puts("LCD lifecycle PASS: real prepare/queues, empty/nonempty/wrapped entry, nonmutating diagnostics, timings/LUT preservation, producer joins, incomplete scan and underrun recovery");
    return 0;
}
