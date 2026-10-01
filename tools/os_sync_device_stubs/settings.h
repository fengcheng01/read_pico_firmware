/* 宿主设备边界替身。/ Host device-boundary fake. */
#pragma once
const char* app_settings_sync_url(void);
const char* app_settings_sync_user(void);
const char* app_settings_sync_key(void);
void app_settings_set_sync_key(const char*);
int app_settings_book_px(void);
