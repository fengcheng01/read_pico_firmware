/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * USB 一次性引导及专用磁盘生命周期回归。/ One-shot USB boot and dedicated disk lifecycle regressions.
 */
#include "usb_disk_test_env.h"
#include "../main/os/os_usb_disk_pico.c"
#include "../main/usb/usb_disk.c"
int main(void) {
    assert(!os_usb_disk_take_request());
    nvs_error=ESP_FAIL;assert(os_usb_disk_request()==ESP_FAIL && !restarts);
    nvs_error=0;commit_error=ESP_FAIL;
    assert(os_usb_disk_request()==ESP_FAIL && !restarts && !stored);
    commit_error=0;assert(os_usb_disk_request()==ESP_OK && restarts==1 && stored==1);
    commit_error=ESP_FAIL;assert(!os_usb_disk_take_request() && stored==1);
    commit_error=0;assert(os_usb_disk_take_request() && !stored);
    assert(!os_usb_disk_take_request());
    raw_error=ESP_FAIL;assert(start_disk()==ESP_FAIL && !s_usb && !s_card);assert(stop_disk());
    raw_error=0;msc_error=ESP_FAIL;assert(start_disk()==ESP_FAIL);assert(stop_disk() && raw_closes==1);
    msc_error=0;storage_error=ESP_FAIL;assert(start_disk()==ESP_FAIL);assert(stop_disk() && raw_closes==2);
    storage_error=0;usb_error=ESP_FAIL;assert(start_disk()==ESP_FAIL);assert(stop_disk() && raw_closes==3);
    usb_error=0;assert(start_disk()==ESP_OK && s_usb && s_storage);
    tinyusb_event_t event={TINYUSB_EVENT_ATTACHED};usb_event(&event,NULL);assert(atomic_load(&s_attached));
    owner=TINYUSB_MSC_STORAGE_MOUNT_USB;assert(!disk_ejected());
    owner=TINYUSB_MSC_STORAGE_MOUNT_APP;assert(disk_ejected());
    event.id=TINYUSB_EVENT_DETACHED;usb_event(&event,NULL);assert(!atomic_load(&s_attached));
    delete_busy=1;assert(!stop_disk() && s_usb && s_storage && raw_closes==3 && !usb_stops);
    assert(!stop_disk() && !usb_stops);
    run_deferred();
    assert(!stop_disk() && !usb_stops);
    run_deferred();
    assert(stop_disk() && !s_usb && !s_storage && raw_closes==4 && usb_stops==1);
    usb_disk_restore_serial();
    puts("USB host lifecycle PASS: one-shot boot, no-format TF ownership, failure cleanup, eject, deferred-write drain, serial restoration");
}
