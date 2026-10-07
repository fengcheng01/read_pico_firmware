/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
typedef int nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
esp_err_t nvs_open(const char*, int, nvs_handle_t*);
void nvs_close(nvs_handle_t);
esp_err_t nvs_get_blob(nvs_handle_t, const char*, void*, size_t*);
esp_err_t nvs_set_blob(nvs_handle_t, const char*, const void*, size_t);
esp_err_t nvs_get_str(nvs_handle_t, const char*, char*, size_t*);
esp_err_t nvs_set_str(nvs_handle_t, const char*, const char*);
esp_err_t nvs_erase_key(nvs_handle_t, const char*);
esp_err_t nvs_commit(nvs_handle_t);
esp_err_t nvs_get_u32(nvs_handle_t, const char*, uint32_t*);
esp_err_t nvs_set_u32(nvs_handle_t, const char*, uint32_t);
typedef struct nvs_iter* nvs_iterator_t;
typedef struct { char namespace_name[16], key[16]; } nvs_entry_info_t;
#define NVS_TYPE_U16 2
esp_err_t nvs_get_u16(nvs_handle_t, const char*, uint16_t*);
esp_err_t nvs_set_u16(nvs_handle_t, const char*, uint16_t);
esp_err_t nvs_entry_find(const char*, const char*, int, nvs_iterator_t*);
esp_err_t nvs_entry_next(nvs_iterator_t*);
void nvs_entry_info(nvs_iterator_t, nvs_entry_info_t*);
void nvs_release_iterator(nvs_iterator_t);
