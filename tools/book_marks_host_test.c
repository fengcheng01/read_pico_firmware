/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 书签存储测试：排序、原位更新、满员淘汰与遗忘，全部走内存 NVS 模拟。
 * Bookmark store tests: ordering, in-place updates, oldest-eviction and
 * forgetting, all through an in-memory NVS mock.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "book_marks.h"
#include "nvs.h"

/* ---- 内存 NVS 模拟 / In-memory NVS mock ---- */
typedef struct {
    char ns[16], key[16];
    uint8_t data[512];
    size_t len;
    bool used, is_u32;
    uint32_t u32;
} cell_t;
static cell_t cells[32];
static int handle_count;

static cell_t* find(const char* ns, const char* key) {
    for (int i = 0; i < 32; ++i)
        if (cells[i].used && !strcmp(cells[i].ns, ns) && !strcmp(cells[i].key, key)) return &cells[i];
    return NULL;
}
esp_err_t nvs_open(const char* ns, int mode, nvs_handle_t* out) {
    // 模拟只服务 rp_marks；句柄只增不减，配对 close 即可。/ The mock serves rp_marks only; handles grow monotonically with paired closes.
    (void)ns; (void)mode;
    *out = ++handle_count;
    return ESP_OK;
}
void nvs_close(nvs_handle_t h) { (void)h; }
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }
esp_err_t nvs_get_blob(nvs_handle_t h, const char* key, void* out, size_t* len) {
    (void)h;
    cell_t* c = find("rp_marks", key);
    if (!c || c->is_u32) return ESP_ERR_NVS_NOT_FOUND;
    if (*len < c->len) return ESP_ERR_NVS_INVALID_LENGTH;
    memcpy(out, c->data, c->len);
    *len = c->len;
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char* key, const void* data, size_t len) {
    (void)h;
    cell_t* c = find("rp_marks", key);
    if (!c) {
        for (int i = 0; i < 32; ++i) if (!cells[i].used) { c = &cells[i]; break; }
        if (!c) return -1;
        c->used = true;
        snprintf(c->ns, 16, "rp_marks");
        snprintf(c->key, 16, "%s", key);
    }
    c->is_u32 = false;
    memcpy(c->data, data, len);
    c->len = len;
    return ESP_OK;
}
esp_err_t nvs_get_u32(nvs_handle_t h, const char* key, uint32_t* out) {
    (void)h;
    cell_t* c = find("rp_marks", key);
    if (!c || !c->is_u32) return ESP_ERR_NVS_NOT_FOUND;
    *out = c->u32;
    return ESP_OK;
}
esp_err_t nvs_set_u32(nvs_handle_t h, const char* key, uint32_t value) {
    (void)h;
    cell_t* c = find("rp_marks", key);
    if (!c) {
        for (int i = 0; i < 32; ++i) if (!cells[i].used) { c = &cells[i]; break; }
        if (!c) return -1;
        c->used = true;
        snprintf(c->ns, 16, "rp_marks");
        snprintf(c->key, 16, "%s", key);
    }
    c->is_u32 = true;
    c->u32 = value;
    return ESP_OK;
}
esp_err_t nvs_erase_key(nvs_handle_t h, const char* key) {
    (void)h;
    cell_t* c = find("rp_marks", key);
    if (!c) return ESP_ERR_NVS_NOT_FOUND;
    c->used = false;
    return ESP_OK;
}
esp_err_t nvs_get_str(nvs_handle_t h, const char* k, char* o, size_t* l) { (void)h; (void)k; (void)o; (void)l; return ESP_ERR_NVS_NOT_FOUND; }
esp_err_t nvs_set_str(nvs_handle_t h, const char* k, const char* v) { (void)h; (void)k; (void)v; return ESP_OK; }

/* ---- 断言 / Assertions ---- */
int main(void) {
    const char* path = "/sdcard/books/日常阅读.txt";
    book_mark_t marks[BOOK_MARKS_MAX];
    size_t n = 99;

    // 无记录时 list 为 false 且 n=0。/ No record: list is false with n zeroed.
    assert(!book_marks_list(path, marks, BOOK_MARKS_MAX, &n) && n == 0);

    // 乱序加入三章，list 按位置排序。/ Add three chapters out of order; list stays position-sorted.
    assert(book_marks_add(path, 2, 100, 30) == ESP_OK);
    assert(book_marks_add(path, 0, 50, 5) == ESP_OK);
    assert(book_marks_add(path, 1, 10, 12) == ESP_OK);
    assert(book_marks_list(path, marks, BOOK_MARKS_MAX, &n) && n == 3);
    assert(marks[0].chapter == 0 && marks[1].chapter == 1 && marks[2].chapter == 2);
    assert(marks[0].byte_off == 50 && marks[1].byte_off == 10 && marks[2].byte_off == 100);

    // 同位置重复添加为原位更新，不增容量。/ Re-adding the same spot updates in place.
    assert(book_marks_add(path, 1, 10, 15) == ESP_OK);
    assert(book_marks_list(path, marks, BOOK_MARKS_MAX, &n) && n == 3);
    assert(marks[1].pct == 15 && marks[1].seq > marks[2].seq);

    // 同章内按偏移排序。/ Same chapter sorts by offset.
    assert(book_marks_add(path, 1, 5, 11) == ESP_OK);
    assert(book_marks_list(path, marks, BOOK_MARKS_MAX, &n) && n == 4);
    assert(marks[1].byte_off == 5 && marks[2].byte_off == 10);

    // 删除中间一条。/ Remove a middle entry.
    assert(book_marks_remove_at(path, 1) == ESP_OK);
    assert(book_marks_list(path, marks, BOOK_MARKS_MAX, &n) && n == 3);
    assert(marks[1].byte_off == 10);
    assert(book_marks_remove_at(path, 3) == ESP_ERR_INVALID_SIZE);

    // 满员淘汰最旧（首条 chapter2，seq 最小）。/ Full house evicts the oldest (chapter 2, the smallest seq).
    for (int i = 5; i <= 18; ++i) assert(book_marks_add(path, (uint16_t)i, (uint32_t)i, (uint8_t)i) == ESP_OK);
    assert(book_marks_list(path, marks, BOOK_MARKS_MAX, &n) && n == BOOK_MARKS_MAX);
    bool saw_chapter2 = false;
    for (size_t i = 0; i < n; ++i) if (marks[i].chapter == 2) saw_chapter2 = true;
    assert(!saw_chapter2);
    for (size_t i = 1; i < n; ++i) {
        assert(marks[i - 1].chapter < marks[i].chapter ||
               (marks[i - 1].chapter == marks[i].chapter && marks[i - 1].byte_off <= marks[i].byte_off));
    }

    // 遗忘后无记录。/ Forgetting clears the record.
    assert(book_marks_forget(path) == ESP_OK);
    assert(!book_marks_list(path, marks, BOOK_MARKS_MAX, &n) && n == 0);
    assert(book_marks_forget(path) == ESP_OK);  // 幂等。/ Idempotent.

    // 非法输入。/ Invalid inputs.
    assert(book_marks_add(NULL, 0, 0, 0) == ESP_ERR_INVALID_ARG);
    assert(book_marks_add("", 0, 0, 0) == ESP_ERR_INVALID_ARG);
    assert(book_marks_add(path, 0, 0, 101) == ESP_ERR_INVALID_ARG);
    printf("book_marks ok\n");
    return 0;
}
