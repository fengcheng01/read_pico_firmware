/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：kosync 协议核心的替身回归：请求拼装、状态映射、JSON 解析与 rp1 编解码。
 * English: Fake-transport regressions for the kosync core: request assembly, status mapping, JSON parsing and rp1 codec.
 */
#include "os_sync.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char s_method[8], s_url[224], s_user[64], s_key[64], s_body[288], s_type[40];
static const char* s_resp;
static int s_status = 200;
static unsigned s_calls;

static int fake_request(const char* method, const char* url, const char* user, const char* key,
                        const char* content_type, const char* body, char* resp, size_t cap) {
    ++s_calls;
    snprintf(s_method, sizeof(s_method), "%s", method);
    snprintf(s_url, sizeof(s_url), "%s", url);
    snprintf(s_user, sizeof(s_user), "%s", user ? user : "(null)");
    snprintf(s_key, sizeof(s_key), "%s", key ? key : "(null)");
    snprintf(s_type, sizeof(s_type), "%s", content_type ? content_type : "(null)");
    snprintf(s_body, sizeof(s_body), "%s", body ? body : "(null)");
    if (resp && cap) snprintf(resp, cap, "%s", s_resp ? s_resp : "");
    return s_status;
}
static bool fake_doc_id(const char* path, char out[33]) {
    (void)path;
    strcpy(out, "0123456789abcdef0123456789abcdef");
    return true;
}
static const os_sync_io_t s_io = {.request = fake_request, .doc_id = fake_doc_id};

static os_sync_config_t config(void) {
    os_sync_config_t cfg = {{0}, {0}, {0}};
    strcpy(cfg.url, "https://sync.example.com/");
    strcpy(cfg.user, "reader");
    strcpy(cfg.key, "00112233445566778899aabbccddeeff");
    return cfg;
}

int main(void) {
    os_sync_bind(&s_io);
    char rp1[OS_SYNC_PROGRESS_MAX];
    uint32_t size = 0, off = 0;
    uint16_t chapter = 0;
    uint8_t px = 0;

    // rp1 编解码与拒绝项。/ rp1 codec and rejections.
    assert(os_sync_progress_encode(rp1, sizeof(rp1), 123456, 7, 4321, 48));
    assert(!strcmp(rp1, "rp1|123456|7|4321|48"));
    assert(os_sync_progress_decode(rp1, &size, &chapter, &off, &px));
    assert(size == 123456 && chapter == 7 && off == 4321 && px == 48);
    assert(!os_sync_progress_decode("/body/div[3]", &size, &chapter, &off, &px));
    assert(!os_sync_progress_decode("rp2|123456|7|4321|48", &size, &chapter, &off, &px));
    assert(!os_sync_progress_decode("rp1|123456|7|4321|48|9", &size, &chapter, &off, &px));
    assert(!os_sync_progress_decode("rp1|0|7|4321|48", &size, &chapter, &off, &px));

    // 未配置账号一律拒绝。/ Missing credentials refuse everything.
    os_sync_config_t empty = {{0}, {0}, {0}};
    assert(os_sync_auth(&empty) == OS_SYNC_NO_CONFIG);
    assert(os_sync_register(&empty) == OS_SYNC_NO_CONFIG);
    assert(os_sync_push(&empty, "0123456789abcdef0123456789abcdef", "rp1|1|0|0|48", 0.5f) == OS_SYNC_NO_CONFIG);
    assert(s_calls == 0);

    // 鉴权：GET users/auth，带 x-auth 头；401 映射 AUTH。/ Auth: GET users/auth with x-auth headers; 401 maps to AUTH.
    os_sync_config_t cfg = config();
    assert(os_sync_auth(&cfg) == OS_SYNC_OK);
    assert(!strcmp(s_method, "GET") && !strcmp(s_url, "https://sync.example.com/users/auth"));
    assert(!strcmp(s_user, "reader") && !strcmp(s_key, "00112233445566778899aabbccddeeff"));
    s_status = 401;
    assert(os_sync_auth(&cfg) == OS_SYNC_AUTH);

    // 注册：POST users/create，不带鉴权，body 只有用户名与 MD5。/ Register: POST users/create without auth headers.
    s_status = 200;
    assert(os_sync_register(&cfg) == OS_SYNC_OK);
    assert(!strcmp(s_method, "POST") && strstr(s_url, "/users/create"));
    assert(!strcmp(s_user, "(null)"));
    assert(strstr(s_body, "\"username\":\"reader\"") && strstr(s_body, "\"password\":\"00112233445566778899aabbccddeeff\""));
    s_status = 402;
    assert(os_sync_register(&cfg) == OS_SYNC_EXISTS);

    // 上传：PUT syncs/progress，body 含 document/rp1/percentage。/ Push: PUT syncs/progress with document/rp1/percentage.
    s_status = 200;
    assert(os_sync_push(&cfg, "0123456789abcdef0123456789abcdef", rp1, 0.4239f) == OS_SYNC_OK);
    assert(!strcmp(s_method, "PUT") && strstr(s_url, "/syncs/progress"));
    assert(strstr(s_body, "\"document\":\"0123456789abcdef0123456789abcdef\""));
    assert(strstr(s_body, "\"progress\":\"rp1|123456|7|4321|48\""));
    assert(strstr(s_body, "\"percentage\":0.42"));
    assert(os_sync_push(&cfg, "short", rp1, 0.5f) == OS_SYNC_IO);
    s_status = -1;
    assert(os_sync_push(&cfg, "0123456789abcdef0123456789abcdef", rp1, 0.5f) == OS_SYNC_OFFLINE);

    // 拉取：JSON 解析、转义、404 与垃圾响应。/ Pull: JSON parsing, escapes, 404 and garbage.
    s_status = 200;
    char remote[OS_SYNC_PROGRESS_MAX];
    float percent = -1;
    s_resp = "{\"document\":\"d\",\"progress\":\"rp1|123456|7|4321|48\",\"percentage\":0.4239,\"device\":\"x\"}";
    assert(os_sync_pull(&cfg, "0123456789abcdef0123456789abcdef", remote, sizeof(remote), &percent) == OS_SYNC_OK);
    assert(!strcmp(remote, "rp1|123456|7|4321|48") && percent > 0.42f && percent < 0.43f);
    assert(os_sync_progress_decode(remote, &size, &chapter, &off, &px));
    s_resp = "{\"progress\":\"a\\\"b\\\\c\",\"percentage\":1}";
    assert(os_sync_pull(&cfg, "0123456789abcdef0123456789abcdef", remote, sizeof(remote), &percent) == OS_SYNC_OK);
    assert(!strcmp(remote, "a\"b\\c") && percent == 1.0f);
    s_resp = "{\"percentage\":0.5}";
    assert(os_sync_pull(&cfg, "0123456789abcdef0123456789abcdef", remote, sizeof(remote), &percent) == OS_SYNC_PARSE);
    s_resp = "not json";
    assert(os_sync_pull(&cfg, "0123456789abcdef0123456789abcdef", remote, sizeof(remote), &percent) == OS_SYNC_PARSE);
    s_status = 404;
    assert(os_sync_pull(&cfg, "0123456789abcdef0123456789abcdef", remote, sizeof(remote), &percent) == OS_SYNC_NOT_FOUND);
    s_status = 500;
    assert(os_sync_pull(&cfg, "0123456789abcdef0123456789abcdef", remote, sizeof(remote), &percent) == OS_SYNC_SERVER);

    // 默认服务器与结尾斜杠规范化。/ Default server and trailing-slash normalization.
    os_sync_config_t defaulted = {{0}, {0}, {0}};
    strcpy(defaulted.user, "reader");
    strcpy(defaulted.key, "00112233445566778899aabbccddeeff");
    assert(os_sync_config_ready(&defaulted) && strstr(defaulted.url, "sync.koreader.rocks"));
    strcpy(defaulted.url, "http://192.168.1.8:8323//");
    assert(os_sync_config_ready(&defaulted));
    s_status = 200;
    assert(os_sync_auth(&defaulted) == OS_SYNC_OK);
    assert(!strcmp(s_url, "http://192.168.1.8:8323/users/auth"));

    puts("os_sync: kosync assembly, status mapping, JSON parse and rp1 codec passed");
    return 0;
}
