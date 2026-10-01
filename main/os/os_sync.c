/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：kosync 协议核心的纯实现：请求拼装、微型 JSON 解析与 rp1 进度串。
 * English: Pure kosync protocol core: request assembly, a tiny JSON reader and the rp1 string.
 */
#include "os_sync.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const os_sync_io_t* s_io;
static char s_url[OS_SYNC_URL_MAX];

const char* os_sync_result_name(os_sync_result_t result) {
    switch (result) {
        case OS_SYNC_OK: return "成功";
        case OS_SYNC_NO_CONFIG: return "未配置账号";
        case OS_SYNC_OFFLINE: return "网络不可用";
        case OS_SYNC_AUTH: return "账号或密钥错误";
        case OS_SYNC_NOT_FOUND: return "服务器暂无此书进度";
        case OS_SYNC_EXISTS: return "用户名已存在";
        case OS_SYNC_SERVER: return "服务器错误";
        case OS_SYNC_PARSE: return "响应解析失败";
        default: return "参数错误";
    }
}

void os_sync_bind(const os_sync_io_t* io) { s_io = io; }

bool os_sync_config_ready(os_sync_config_t* config) {
    if (!config) return false;
    if (!config->url[0]) snprintf(config->url, sizeof(config->url), "%s", OS_SYNC_DEFAULT_URL);
    // 去掉结尾斜杠，统一拼 URL。/ Drop a trailing slash for URL joins.
    size_t len = strlen(config->url);
    while (len && config->url[len - 1] == '/') config->url[--len] = 0;
    if (!len || !config->user[0]) return false;
    return strlen(config->key) == 32;
}

static os_sync_result_t status_result(int status) {
    if (status < 0) return OS_SYNC_OFFLINE;
    if (status >= 200 && status < 300) return OS_SYNC_OK;
    if (status == 401 || status == 403) return OS_SYNC_AUTH;
    if (status == 402) return OS_SYNC_EXISTS;
    if (status == 404) return OS_SYNC_NOT_FOUND;
    return OS_SYNC_SERVER;
}

static void json_escape(char* out, size_t cap, const char* text) {
    size_t at = 0;
    for (const unsigned char* p = (const unsigned char*)(text ? text : ""); *p && at + 7 < cap; ++p) {
        if (*p == '"' || *p == '\\') out[at++] = '\\';
        out[at++] = (char)*p;
    }
    out[at] = 0;
}

// 校验配置并把基址规范进 s_url（去结尾斜杠）。/ Validate and normalize the base URL into s_url.
static os_sync_result_t check(const os_sync_config_t* config) {
    if (!config || !s_io) return OS_SYNC_IO;
    if (!config->user[0] || strlen(config->key) != 32) return OS_SYNC_NO_CONFIG;
    snprintf(s_url, sizeof(s_url), "%s", config->url[0] ? config->url : OS_SYNC_DEFAULT_URL);
    size_t len = strlen(s_url);
    while (len && s_url[len - 1] == '/') s_url[--len] = 0;
    return len ? OS_SYNC_OK : OS_SYNC_IO;
}

os_sync_result_t os_sync_register(const os_sync_config_t* config) {
    os_sync_result_t pre = check(config);
    if (pre != OS_SYNC_OK) return pre;
    char user[OS_SYNC_USER_MAX * 2 + 2], key[67];
    json_escape(user, sizeof(user), config->user);
    json_escape(key, sizeof(key), config->key);
    char body[160];
    snprintf(body, sizeof(body), "{\"username\":\"%s\",\"password\":\"%s\"}", user, key);
    char url[OS_SYNC_URL_MAX + 32];
    snprintf(url, sizeof(url), "%s/users/create", s_url);
    // 注册不带鉴权头；服务器只见密码的 MD5。/ Registration carries no auth headers; the server sees only the MD5.
    return status_result(s_io->request("POST", url, NULL, NULL, "application/json", body, NULL, 0));
}

os_sync_result_t os_sync_auth(const os_sync_config_t* config) {
    os_sync_result_t pre = check(config);
    if (pre != OS_SYNC_OK) return pre;
    char url[OS_SYNC_URL_MAX + 32];
    snprintf(url, sizeof(url), "%s/users/auth", s_url);
    return status_result(s_io->request("GET", url, config->user, config->key, NULL, NULL, NULL, 0));
}

os_sync_result_t os_sync_push(const os_sync_config_t* config, const char* doc_id,
                              const char* progress, float percent) {
    os_sync_result_t pre = check(config);
    if (pre != OS_SYNC_OK) return pre;
    if (!doc_id || strlen(doc_id) != 32 || !progress || !progress[0]) return OS_SYNC_IO;
    char doc[67], prog[OS_SYNC_PROGRESS_MAX * 2 + 2], body[256];
    json_escape(doc, sizeof(doc), doc_id);
    json_escape(prog, sizeof(prog), progress);
    if (percent < 0) percent = 0;
    if (percent > 1) percent = 1;
    snprintf(body, sizeof(body),
             "{\"document\":\"%s\",\"progress\":\"%s\",\"percentage\":%.4f,"
             "\"device\":\"" OS_SYNC_DEVICE "\",\"device_id\":\"readpico\"}",
             doc, prog, percent);
    char url[OS_SYNC_URL_MAX + 32];
    snprintf(url, sizeof(url), "%s/syncs/progress", s_url);
    return status_result(s_io->request("PUT", url, config->user, config->key, "application/json", body, NULL, 0));
}

// 在平坦 JSON 里定位 "key":"值"；返回值区间与长度。/ Locate a flat "key":"value"; returns the value span.
static const char* json_string(const char* resp, const char* key, size_t* len) {
    char pattern[24];
    int n = snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    if (n < 0 || (size_t)n >= sizeof(pattern)) return NULL;
    const char* at = strstr(resp, pattern);
    if (!at) return NULL;
    at += n;
    while (*at == ' ') ++at;
    if (*at++ != '"') return NULL;
    const char* start = at;
    while (*at && *at != '"') {
        if (*at == '\\' && at[1]) ++at;
        ++at;
    }
    if (*at != '"') return NULL;
    *len = (size_t)(at - start);
    return start;
}

os_sync_result_t os_sync_pull(const os_sync_config_t* config, const char* doc_id,
                              char* progress, size_t cap, float* percent) {
    os_sync_result_t pre = check(config);
    if (pre != OS_SYNC_OK) return pre;
    if (!doc_id || strlen(doc_id) != 32) return OS_SYNC_IO;
    char url[OS_SYNC_URL_MAX + 64];
    snprintf(url, sizeof(url), "%s/syncs/progress/%s", s_url, doc_id);
    char resp[512];
    os_sync_result_t result = status_result(s_io->request("GET", url, config->user, config->key,
                                                          NULL, NULL, resp, sizeof(resp)));
    if (result != OS_SYNC_OK) return result;
    size_t len = 0;
    const char* value = json_string(resp, "progress", &len);
    if (!value) return OS_SYNC_PARSE;
    if (progress && cap) {
        // 反转义仅处理 \" 与 \\；其它转义按单字符降级。/ Unescape \" and \\; other escapes degrade to one char.
        size_t used = 0;
        for (size_t i = 0; i < len && used + 1 < cap; ++i) {
            char c = value[i];
            if (c == '\\' && i + 1 < len) c = value[++i];
            progress[used++] = c;
        }
        progress[used] = 0;
    }
    // percentage 是数字不是字符串，直接找冒号后的数值。/ percentage is a number, so read past the colon.
    const char* pct = strstr(resp, "\"percentage\"");
    if (!pct || !(pct = strchr(pct, ':'))) return OS_SYNC_PARSE;
    ++pct;
    char* end = NULL;
    double parsed = strtod(pct, &end);
    if (end == pct) return OS_SYNC_PARSE;
    if (percent) *percent = parsed < 0 ? 0 : parsed > 1 ? 1 : (float)parsed;
    return OS_SYNC_OK;
}

bool os_sync_progress_encode(char* out, size_t cap, uint32_t file_size, uint16_t chapter,
                             uint32_t byte_off, uint8_t px) {
    if (!out || cap < 32) return false;
    int n = snprintf(out, cap, "rp1|%lu|%lu|%lu|%lu", (unsigned long)file_size, (unsigned long)chapter, (unsigned long)byte_off, (unsigned long)px);
    return n > 0 && (size_t)n < cap;
}

bool os_sync_progress_decode(const char* in, uint32_t* file_size, uint16_t* chapter,
                             uint32_t* byte_off, uint8_t* px) {
    if (!in) return false;
    // KOReader 端的进度可能是 XPath 等任意串，这里只认自己的 rp1。
    // Remote progress may be any KOReader string; only our rp1 parses here.
    unsigned long size = 0, ch = 0, off = 0, text_px = 0;
    char head[4] = {0};
    int used = 0;
    if (sscanf(in, "%3[^|]|%lu|%lu|%lu|%lu%n", head, &size, &ch, &off, &text_px, &used) != 5) return false;
    if (strcmp(head, "rp1") || (size_t)used != strlen(in)) return false;
    if (size == 0 || size > 0xFFFFFFFFUL || ch > 0xFFFF || off > 0xFFFFFFFFUL || text_px > 255) return false;
    if (file_size) *file_size = (uint32_t)size;
    if (chapter) *chapter = (uint16_t)ch;
    if (byte_off) *byte_off = (uint32_t)off;
    if (px) *px = (uint8_t)text_px;
    return true;
}
