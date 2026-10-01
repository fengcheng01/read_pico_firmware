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
#include <math.h>
#include "cJSON.h"

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

static bool json_escape(char* out, size_t cap, const char* text) {
    size_t at = 0;
    for (const unsigned char* p = (const unsigned char*)(text ? text : ""); *p; ++p) {
        size_t need = *p < 0x20 ? 6 : (*p == '"' || *p == '\\') ? 2 : 1;
        if (need >= cap - at) return false;
        if (*p < 0x20) { snprintf(out + at, cap - at, "\\u%04x", *p); at += 6; }
        else { if (need == 2) out[at++] = '\\'; out[at++] = (char)*p; }
    }
    out[at] = 0;
    return true;
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
    char user[OS_SYNC_USER_MAX * 6 + 1], key[67];
    if (!json_escape(user, sizeof(user), config->user)) return OS_SYNC_IO;
    if (!json_escape(key, sizeof(key), config->key)) return OS_SYNC_IO;
    char body[320];
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
    char doc[67], prog[OS_SYNC_PROGRESS_MAX * 6 + 1], body[640];
    if (!json_escape(doc, sizeof(doc), doc_id)) return OS_SYNC_IO;
    if (!json_escape(prog, sizeof(prog), progress) || !isfinite(percent)) return OS_SYNC_IO;
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

// 响应很小且结构浅，解析前限制嵌套，避免恶意 JSON 消耗任务栈。
// Responses are small and shallow; bound nesting before parsing to protect the task stack.
static bool bounded_json(const char* text) {
    unsigned depth = 0;
    bool quoted = false, escaped = false;
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
        if (quoted) {
            if (escaped) escaped = false;
            else if (*p == '\\') escaped = true;
            else if (*p == '"') quoted = false;
        } else if (*p == '"') quoted = true;
        else if (*p == '{' || *p == '[') { if (++depth > 8) return false; }
        else if (*p == '}' || *p == ']') { if (!depth) return false; --depth; }
    }
    return !quoted && !depth;
}

os_sync_result_t os_sync_pull(const os_sync_config_t* config, const char* doc_id,
                              char* progress, size_t cap, float* percent) {
    os_sync_result_t pre = check(config);
    if (pre != OS_SYNC_OK) return pre;
    if (!doc_id || strlen(doc_id) != 32 || !progress || !cap || !percent) return OS_SYNC_IO;
    char url[OS_SYNC_URL_MAX + 64], resp[512] = {0};
    snprintf(url, sizeof(url), "%s/syncs/progress/%s", s_url, doc_id);
    os_sync_result_t result = status_result(s_io->request("GET", url, config->user, config->key,
                                                          NULL, NULL, resp, sizeof(resp)));
    if (result != OS_SYNC_OK) return result;
    resp[sizeof(resp) - 1] = 0;
    if (!bounded_json(resp)) return OS_SYNC_PARSE;
    cJSON* json = cJSON_ParseWithOpts(resp, NULL, true);
    if (!json) return OS_SYNC_PARSE;
    cJSON* position = cJSON_GetObjectItemCaseSensitive(json, "progress");
    cJSON* pct = cJSON_GetObjectItemCaseSensitive(json, "percentage");
    bool valid = cJSON_IsObject(json) && cJSON_IsString(position) && cJSON_IsNumber(pct) &&
        isfinite(pct->valuedouble) && pct->valuedouble >= 0 && pct->valuedouble <= 1 &&
        position->valuestring[0] && strlen(position->valuestring) < cap;
    if (valid) {
        memcpy(progress, position->valuestring, strlen(position->valuestring) + 1);
        *percent = (float)pct->valuedouble;
    }
    cJSON_Delete(json);
    return valid ? OS_SYNC_OK : OS_SYNC_PARSE;
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
