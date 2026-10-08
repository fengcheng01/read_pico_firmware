#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""真实PMU命令的TIME_GET新鲜度回归。/ TIME_GET freshness regressions for the production PMU command function."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "components/read_pico_pmu/read_pico_pmu.c").read_text()
command_body = source.split("esp_err_t read_pico_pmu_cmd(", 1)[1].split("esp_err_t read_pico_pmu_vcom_get", 1)[0]
command_body = "esp_err_t read_pico_pmu_cmd(" + command_body
harness = r'''
#include "read_pico_pmu_protocol.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE -2
#define ESP_ERR_TIMEOUT -3
#define ESP_ERR_INVALID_RESPONSE -4
#define ESP_LOGW(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
static void* s_dev;
static struct {
    bool time_ok, alarm_ok, rtc_raw_ok, uid_ok;
    uint32_t unix_sec, rtc_raw[9], alarm_remain, alarm_target;
    uint16_t last_op_code, last_op_status;
    int last_err;
    uint8_t time_synced, alarm_mode, uid[PMU_CHIP_UID_LEN];
} s_snap;
static uint8_t s_last_payload[PMU_FRAME_PAYLOAD_SIZE], s_last_plen;
static pmu_frame_t response;
static int write_error, read_error, reads, recoveries;
static uint32_t rd32(const uint8_t* p) {return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void wr32(uint8_t* p,uint32_t v) {for(int i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8));}
static void recover_seq(void) {++recoveries;}
static void vTaskDelay(int ticks) {(void)ticks;}
static void fill_req(pmu_frame_t* req,uint16_t code,const uint8_t* payload,uint8_t plen) {
    memset(req,0,sizeof(*req)); req->sequence=1; req->code=code;
    assert(plen<=PMU_FRAME_PAYLOAD_SIZE);
    if(plen) {assert(payload);memcpy(req->payload,payload,plen);}
}
static int pmu_write_reg(uint8_t reg,const uint8_t* data,size_t size) {
    assert(reg==PMU_REG_COMMAND && data && size==sizeof(pmu_frame_t));return write_error;
}
static int read_response_for(uint16_t seq,pmu_frame_t* out) {
    assert(seq==1);++reads;
    if(read_error)return read_error;
    *out=response;return ESP_OK;
}
static void parse_config(const uint8_t* payload) {(void)payload;}
''' + command_body + r'''
static void reset(void) {
    memset(&s_snap,0,sizeof(s_snap));s_snap.time_ok=true;s_snap.unix_sec=1791400000;
    s_dev=&s_snap;write_error=read_error=reads=recoveries=0;
    memset(&response,0,sizeof(response));response.status=PMU_STATUS_OK;
    response.payload_length=8;wr32(response.payload,1791400001);response.payload[6]=1;
}
static void failed_reads(void) {
    reset();write_error=ESP_FAIL;
    assert(read_pico_pmu_cmd(PMU_CMD_TIME_GET,NULL,0)==ESP_FAIL && !s_snap.time_ok && reads==0);
    assert(s_snap.unix_sec==1791400000);
    reset();read_error=ESP_ERR_TIMEOUT;
    assert(read_pico_pmu_cmd(PMU_CMD_TIME_GET,NULL,0)==ESP_ERR_TIMEOUT && !s_snap.time_ok);
    reset();response.status=PMU_STATUS_INVALID_ARGUMENT;
    assert(read_pico_pmu_cmd(PMU_CMD_TIME_GET,NULL,0)==ESP_ERR_INVALID_RESPONSE && !s_snap.time_ok);
    reset();s_dev=NULL;
    assert(read_pico_pmu_cmd(PMU_CMD_TIME_GET,NULL,0)==ESP_ERR_INVALID_STATE && !s_snap.time_ok);
    reset();response.status=PMU_STATUS_STALE_SESSION;
    assert(read_pico_pmu_cmd(PMU_CMD_TIME_GET,NULL,0)==ESP_ERR_INVALID_RESPONSE && !s_snap.time_ok);
    assert(reads==3 && recoveries==2);
    puts("Freshness: transport/timeout/status/disconnected/session failures invalidate cached TIME_GET validity PASS");
}
static void payload_validation(void) {
    for(uint8_t n=0;n<8;++n) {
        reset();response.payload_length=n;
        assert(read_pico_pmu_cmd(PMU_CMD_TIME_GET,NULL,0)==ESP_ERR_INVALID_RESPONSE);
        assert(!s_snap.time_ok && s_snap.unix_sec==1791400000);
    }
    reset();assert(read_pico_pmu_cmd(PMU_CMD_TIME_GET,NULL,0)==ESP_OK);
    assert(s_snap.time_ok && s_snap.time_synced==1 && s_snap.unix_sec==1791400001);
    write_error=ESP_FAIL;assert(read_pico_pmu_cmd(PMU_CMD_TIME_GET,NULL,0)==ESP_FAIL && !s_snap.time_ok);
    write_error=0;assert(read_pico_pmu_cmd(PMU_CMD_TIME_GET,NULL,0)==ESP_OK && s_snap.time_ok);
    reset();response.status=PMU_STATUS_ACCEPTED;
    assert(read_pico_pmu_cmd(PMU_CMD_TIME_GET,NULL,0)==ESP_OK && !s_snap.time_ok);
    reset();assert(read_pico_pmu_cmd(PMU_CMD_TIME_SYNC,NULL,0)==ESP_OK && s_snap.time_ok);
    puts("Payloads: short OK responses rejected, complete read recovers, other commands do not erase time validity PASS");
}
int main(void) {failed_reads();payload_validation();puts("Production PMU TIME_GET freshness PASS");}
'''
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--verify-regression", action="store_true")
args = parser.parse_args()
flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined"]
if os.uname().sysname == "Darwin":
    flags += ["-isysroot", "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"]
with tempfile.TemporaryDirectory(prefix="pico-pmu-time-", dir="/tmp") as folder:
    d = Path(folder); test_source = d / "test.c"; test_source.write_text(harness)
    command = [os.environ.get("CC", "cc"), *flags, "-Icomponents/read_pico_pmu/include", str(test_source), "-o", str(d / "test")]
    subprocess.run(command, cwd=ROOT, check=True);subprocess.run([str(d / "test")], check=True)
    if args.verify_regression:
        fixed = "if (code == PMU_CMD_TIME_GET) s_snap.time_ok = false;"
        assert harness.count(fixed)==1
        test_source.write_text(harness.replace(fixed,""))
        subprocess.run(command, cwd=ROOT, check=True)
        result=subprocess.run([str(d / "test")], capture_output=True, text=True)
        assert result.returncode!=0 and "!s_snap.time_ok" in result.stderr
        print("Former retained time_ok fails stale read rejection (negative control) PASS")
