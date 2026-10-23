#pragma once
#include <stdbool.h>
#include <stdint.h>
#define PMU_STATUS_BATTERY_VALID 1
typedef struct {bool status_ok;uint16_t flags,soc_permille,battery_mv;uint8_t charge_state;} pmu_snapshot_t;
bool read_pico_pmu_ready(void);
const pmu_snapshot_t* read_pico_pmu_get(void);
