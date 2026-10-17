#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""真实 /fs 与字体/图片删除的主机回归：路径约束、隐藏规则、排序、截断、转义、TXT预览与删除语义。/ Production
/fs browser and font/image deletion against real POSIX directories: path constraints, hidden files, sorting,
truncation, escaping, TXT preview and deletion semantics."""
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "components/read_pico_transfer/read_pico_transfer.c").read_text()
HEX_BODY = SOURCE[SOURCE.index("static int hex_value("):SOURCE.index("// 名字按类别限扩展名")]
NAME_BODY = SOURCE[SOURCE.index("// 名字按类别限扩展名"):SOURCE.index("// 两个存储根都是FAT")]
RESOLVE_BODY = SOURCE[SOURCE.index("// 两个存储根都是FAT"):SOURCE.index("static int check_length(")]
RESULT_BODY = SOURCE[SOURCE.index("typedef struct { const char *stage;"):SOURCE.index("static int receive_file_detail(")] + SOURCE[SOURCE.index("typedef int (*file_changed_cb_t)(const char *);"):SOURCE.index("static file_result_t upload_managed(")]
DELETE_BODY = SOURCE[SOURCE.index("static file_result_t delete_managed("):SOURCE.index("#define BOOK_LIST_PAGE_SIZE")]
DECODE_PATH_BODY = SOURCE[SOURCE.index("// 目录浏览的相对路径解码"):SOURCE.index("static esp_err_t send_json")]
RESP_BODY = SOURCE[SOURCE.index("static esp_err_t send_json"):SOURCE.index("static esp_err_t books_handler")]
FS_BODY = SOURCE[SOURCE.index("/* ---- 只读目录浏览"):SOURCE.index("static esp_err_t upload_error_response")]
KIND_DELETE_BODY = SOURCE[SOURCE.index("// 字体/图片删除"):SOURCE.index("/* ---- 生命周期")]

HARNESS = r'''
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

typedef int esp_err_t;
enum { ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_NOT_FOUND = 0x102, ESP_ERR_NO_MEM = 0x101 };
typedef struct { const char *query; void *user_ctx; size_t content_len; } httpd_req_t;
// httpd边界只收集分块与查询串；不模拟真实网络。/ The httpd boundary collects chunks and the query string only.
static char s_chunks[512][640];
static size_t s_chunk_count;
static char s_http_status[32] = "200 OK";
static const char *s_last_error_body;
static int s_error_code;
static char s_root[160];
static char s_font_root[160];
static char s_image_root[160];
static struct { int is_flash; } s_cfg;
static void set_error(int code) { s_error_code = code; }
#define ESP_LOGW(tag, ...) do { (void)(tag); } while (0)
// 删除准入的临界区桩与共享状态：只验证语义，不模拟多核。/ Critical-section stubs plus
// shared state for delete admission: semantics only, no multicore emulation.
typedef int portMUX_TYPE;
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
static portMUX_TYPE s_lock;
static bool s_stopping, s_upload_active, s_sync_active;
static struct { unsigned changed_count; } s_status;
static bool admission_begin(bool stopping, bool *uploading) {
    if (stopping || *uploading) return false;
    *uploading = true; return true;
}
// cJSON只被失败分支引用；桩不执行。/ cJSON is referenced by failure branches only; stubs never run.
typedef struct cJSON cJSON;
static cJSON *cJSON_CreateObject(void) { return NULL; }
static cJSON *cJSON_AddObjectToObject(cJSON *o, const char *k) { (void)o; (void)k; return NULL; }
static cJSON *cJSON_AddStringToObject(cJSON *o, const char *k, const char *v) { (void)o; (void)k; (void)v; return NULL; }
static cJSON *cJSON_AddBoolToObject(cJSON *o, const char *k, int v) { (void)o; (void)k; (void)v; return NULL; }
static void cJSON_Delete(cJSON *c) { (void)c; }
static char *cJSON_PrintUnformatted(cJSON *c) { (void)c; return NULL; }
static void cJSON_free(void *p) { (void)p; }
static esp_err_t httpd_resp_set_type(httpd_req_t *req, const char *type) { (void)req; (void)type; return ESP_OK; }
static esp_err_t httpd_resp_set_hdr(httpd_req_t *req, const char *k, const char *v) { (void)req; (void)k; (void)v; return ESP_OK; }
static esp_err_t httpd_resp_set_status(httpd_req_t *req, const char *status) { (void)req; snprintf(s_http_status, sizeof(s_http_status), "%s", status); return ESP_OK; }
static esp_err_t httpd_resp_send(httpd_req_t *req, const char *body, size_t n) {
    (void)req; assert(n < sizeof(s_chunks[0]));
    memcpy(s_chunks[s_chunk_count++], body, n); s_chunks[s_chunk_count - 1][n] = 0;
    return ESP_OK;
}
static esp_err_t httpd_resp_send_chunk(httpd_req_t *req, const char *body, ssize_t n) {
    if (!body && !n) return ESP_OK;
    (void)req; assert(n > 0 && (size_t)n < sizeof(s_chunks[0]));
    memcpy(s_chunks[s_chunk_count++], body, (size_t)n); s_chunks[s_chunk_count - 1][n] = 0;
    return ESP_OK;
}
static esp_err_t httpd_resp_sendstr(httpd_req_t *req, const char *body) { return httpd_resp_send(req, body, strlen(body)); }
static size_t httpd_req_get_url_query_len(httpd_req_t *req) { return strlen(req->query); }
static esp_err_t httpd_req_get_url_query_str(httpd_req_t *req, char *out, size_t cap) {
    if (strlen(req->query) >= cap) return ESP_FAIL;
    strcpy(out, req->query); return ESP_OK;
}
static esp_err_t httpd_query_key_value(const char *query, const char *key, char *out, size_t cap) {
    char pattern[64]; snprintf(pattern, sizeof(pattern), "%s=", key); size_t plen = strlen(pattern);
    const char *at = strstr(query, pattern); if (!at) return ESP_ERR_NOT_FOUND;
    const char *value = at + plen, *end = strchr(value, '&'); size_t n = end ? (size_t)(end - value) : strlen(value);
    if (!n || n >= cap) return -1; memcpy(out, value, n); out[n] = 0; return ESP_OK;
}
static void* heap_caps_realloc(void *ptr, size_t size, unsigned caps) { (void)caps; return realloc(ptr, size); }
static void* heap_caps_malloc(size_t size, unsigned caps) { (void)caps; return malloc(size); }
enum { MALLOC_CAP_SPIRAM = 1, MALLOC_CAP_8BIT = 2 };
static bool management_gate = true;
static bool management_request(httpd_req_t *req) { (void)req; return management_gate; }
static esp_err_t wifi_response(httpd_req_t *req, const char *status, const char *body) {
    (void)req; snprintf(s_http_status, sizeof(s_http_status), "%s", status); s_last_error_body = body; return ESP_FAIL;
}
static esp_err_t respond_error(httpd_req_t *req, int code) { (void)req; s_error_code = code; s_last_error_body = ""; return ESP_FAIL; }
''' + HEX_BODY + NAME_BODY + RESOLVE_BODY + RESULT_BODY + DELETE_BODY + DECODE_PATH_BODY + RESP_BODY + FS_BODY + KIND_DELETE_BODY + r'''
static void reset(const char *query) {
    memset(s_chunks, 0, sizeof(s_chunks)); s_chunk_count = 0;
    snprintf(s_http_status, sizeof(s_http_status), "200 OK");
    s_last_error_body = NULL; s_error_code = 0;
    httpd_req_t req = {.query = query};
    fs_handler(&req);
}
static void json_body(char *out, size_t cap) {
    size_t used = 0;
    for (size_t i = 0; i < s_chunk_count && used + 1 < cap; ++i)
        used += (size_t)snprintf(out + used, cap - used, "%s", s_chunks[i]);
}
static esp_err_t kind_delete(const char *query, int kind) {
    memset(s_chunks, 0, sizeof(s_chunks)); s_chunk_count = 0;
    snprintf(s_http_status, sizeof(s_http_status), "200 OK");
    s_last_error_body = NULL; s_error_code = 0;
    httpd_req_t req = {.query = query, .user_ctx = (void *)(intptr_t)kind, .content_len = 0};
    return kind_delete_handler(&req);
}
int main(int argc, char **argv) {
    (void)argc;
    char body[160 * 1024];
    char target[448];
    struct stat probe;
    snprintf(s_root, sizeof(s_root), "%s", argv[1]);
    if (argv[2]) snprintf(s_font_root, sizeof(s_font_root), "%s", argv[2]);

    // 根目录清单由Python侧解析核对；C侧只输出JSON。/ Root listing JSON is parsed and checked by Python.
    reset(""); assert(!s_error_code); json_body(body, sizeof(body)); printf("%s\n", body);
    reset("path=nested"); assert(!s_error_code); json_body(body, sizeof(body)); printf("%s\n", body);

    // 越界与非法路径一律拒绝。/ Traversal and invalid paths are rejected.
    reset("path=.."); assert(s_error_code == 400);
    reset("path=a%2F..%2Fb"); assert(s_error_code == 400);
    reset("path=%2Fetc"); assert(s_error_code == 400);
    reset("path=missing"); assert(s_error_code == 404);

    // TXT预览返回原始字节；EPUB预览被拒。/ TXT preview returns raw bytes; EPUB preview is refused.
    reset("path=b.txt&preview=1"); assert(!s_error_code && s_chunk_count == 1 && s_error_code != 422);
    assert(!strcmp(s_chunks[0], "hello\r\n"));
    reset("path=a.epub&preview=1"); assert(s_last_error_body && strstr(s_http_status, "415"));

    // 字体根浏览：root=fonts落在字体目录，临时文件同样隐藏；越界仍拒绝；空字体根409。
    // Font-root browsing: root=fonts resolves under the font directory with temps hidden;
    // traversal stays refused and an empty font root answers 409.
    reset("root=fonts"); assert(!s_error_code); json_body(body, sizeof(body)); printf("%s\n", body);
    reset("root=fonts&path=.."); assert(s_error_code == 400);
    s_font_root[0] = 0; reset("root=fonts"); assert(s_last_error_body && strstr(s_http_status, "409")); s_font_root[0] = 'x';

    // 图片根浏览：root=images落在图片目录；越界拒绝；空图片根409。
    // Image-root browsing: root=images resolves under the image directory; traversal stays
    // refused and an empty image root answers 409.
    snprintf(s_image_root, sizeof(s_image_root), "%s", argv[1]);
    reset("root=images"); assert(!s_error_code); json_body(body, sizeof(body)); printf("%s\n", body);
    reset("root=images&path=.."); assert(s_error_code == 400);
    s_image_root[0] = 0; reset("root=images"); assert(s_last_error_body && strstr(s_http_status, "409")); s_image_root[0] = 'x';

    // 非管理来源请求被拒。/ Non-management origins are refused.
    management_gate = false; reset(""); assert(s_last_error_body && strstr(s_http_status, "403")); management_gate = true;

    // 字体/图片删除：成功后文件消失、准入复位、计数前进；扩展名与存在性门槛生效。
    // Font/image deletion: files disappear with admission released and the counter advanced;
    // extension and existence gates apply.
    snprintf(s_font_root, sizeof(s_font_root), "%s", argv[2]);
    snprintf(s_image_root, sizeof(s_image_root), "%s", argv[3]);
    s_status.changed_count = 0;
    assert(kind_delete("name=NotoSerifSC.ttf", 1) == ESP_OK && !s_error_code);
    snprintf(target, sizeof(target), "%s/NotoSerifSC.ttf", argv[2]);
    assert(stat(target, &probe) != 0);
    assert(!s_upload_active && s_status.changed_count == 1);
    assert(kind_delete("name=NotoSerifSC.ttf", 1) != ESP_OK && s_error_code == 404);
    assert(kind_delete("name=evil.txt", 1) != ESP_OK && s_error_code == 400);
    assert(kind_delete("name=%E5%B1%B1%E6%99%AF.jpg", 2) == ESP_OK && !s_error_code);
    snprintf(target, sizeof(target), "%s/山景.jpg", argv[3]);
    assert(stat(target, &probe) != 0 && s_status.changed_count == 2);
    // 仅剩备份的图片：删除一并清掉恢复出的文件与备份。/ Backup-only image: the delete
    // clears both the restored file and the backup.
    assert(kind_delete("name=only.jpg", 2) == ESP_OK);
    snprintf(target, sizeof(target), "%s/only.jpg", argv[3]); assert(stat(target, &probe) != 0);
    snprintf(target, sizeof(target), "%s/only.jpg.rename-backup", argv[3]); assert(stat(target, &probe) != 0);
    // 忙线、空根与带正文的请求全拒。/ Busy admission, empty roots and bodies with content all refuse.
    s_upload_active = true;
    assert(kind_delete("name=x.jpg", 2) != ESP_OK && strstr(s_http_status, "409") && strstr(s_last_error_body, "正在上传"));
    s_upload_active = false;
    char keep_image = s_image_root[0]; s_image_root[0] = 0;
    assert(kind_delete("name=x.jpg", 2) != ESP_OK && strstr(s_last_error_body, "图片需要 TF 卡"));
    s_image_root[0] = keep_image;
    char keep_font = s_font_root[0]; s_font_root[0] = 0;
    assert(kind_delete("name=x.ttf", 1) != ESP_OK && strstr(s_last_error_body, "字体需要 TF 卡"));
    s_font_root[0] = keep_font;
    management_gate = false;
    assert(kind_delete("name=x.jpg", 2) != ESP_OK && strstr(s_http_status, "403"));
    management_gate = true;

    puts("transfer fs browsing plus font/image deletion PASS");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="pico-fs-") as directory:
    directory = Path(directory)
    root = directory / "card"
    root.mkdir()
    (root / "b.txt").write_bytes(b"hello\r\n")
    (root / "a.epub").write_bytes(b"PK\x03\x04")
    (root / ".hidden").write_bytes(b"x")
    (root / "x.txt.part").write_bytes(b"x")
    (root / "y.epub.rename-backup").write_bytes(b"x")
    (root / 'quote"name.txt').write_bytes(b"quote")
    (root / "nested").mkdir()
    (root / "nested" / "deep.txt").write_bytes(b"deep")
    for index in range(410):
        (root / "nested" / f"many{index:03d}.txt").write_bytes(b"m")
    fonts = directory / "fonts"
    fonts.mkdir()
    (fonts / "NotoSerifSC.ttf").write_bytes(b"\x00\x01\x00\x00font")
    (fonts / "old.part").write_bytes(b"x")
    images = directory / "images"
    images.mkdir()
    (images / "山景.jpg").write_bytes(b"\xff\xd8\xffjpg")
    (images / "only.jpg.rename-backup").write_bytes(b"\xff\xd8\xffold")

    build = directory / "test.c"
    build.write_text(HARNESS)
    binary = directory / "test"
    flags = ["-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-g", "-fsanitize=address,undefined"]
    if os.uname().sysname == "Darwin":
        flags += ["-isysroot", "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"]
    subprocess.run([os.environ.get("CC", "cc"), *flags, str(build), "-o", str(binary)], check=True)
    result = subprocess.run([str(binary), str(root), str(fonts), str(images)], capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(result.stderr or result.stdout)
    lines = [line for line in result.stdout.splitlines() if line.startswith("{")]
    assert len(lines) == 4, lines

    listing = json.loads(lines[0])
    assert listing["path"] == "" and listing["parent"] == "" and listing["truncated"] is False
    names = [(item["name"], item["dir"], item["txt"]) for item in listing["entries"]]
    assert names == [("nested", 1, 0), ("a.epub", 0, 0), ("b.txt", 0, 1), ('quote"name.txt', 0, 1)], names
    assert listing["entries"][0]["size"] == 0 and listing["entries"][2]["size"] == 7
    assert listing["root_label"] == "TF 卡"
    assert not any(item["name"].startswith(".") or item["name"].endswith((".part", ".rename-backup")) for item in listing["entries"])

    nested = json.loads(lines[1])
    assert nested["path"] == "nested" and nested["parent"] == "" and nested["truncated"] is True
    assert len(nested["entries"]) == 400 and nested["entries"][0]["name"].endswith(".txt")
    assert all(nested["entries"][i]["name"] <= nested["entries"][i + 1]["name"] for i in range(len(nested["entries"]) - 1))
    assert all(item["dir"] is False and item["txt"] is True for item in nested["entries"])

    fonts_listing = json.loads(lines[2])
    assert fonts_listing["root"].endswith("fonts") and fonts_listing["path"] == ""
    assert [(item["name"], item["dir"]) for item in fonts_listing["entries"]] == [("NotoSerifSC.ttf", False)]
    assert not any(item["name"].endswith(".part") for item in fonts_listing["entries"])

    # 图片根清单落在图片目录（此处与书库根同夹具验证路由）。/ The images-root listing
    # resolves under the image directory (sharing the card fixture here to check routing).
    images_listing = json.loads(lines[3])
    assert images_listing["path"] == "" and images_listing["truncated"] is False
    assert [item["name"] for item in images_listing["entries"]] == [item["name"] for item in listing["entries"]]

    # 删除用例在C侧断言状态码；这里核对真实文件系统效果。/ The C side asserts status
    # codes; here the real filesystem effects are verified.
    assert not (fonts / "NotoSerifSC.ttf").exists()
    assert not (images / "山景.jpg").exists()
    assert not (images / "only.jpg").exists()
    assert not (images / "only.jpg.rename-backup").exists()

    print("Directory JSON parsed: sorted, hidden temps, quote escaping, 400-entry truncation flag PASS")
    print(result.stdout.strip().splitlines()[-1])
