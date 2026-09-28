// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"

#define USER_BATT_POWER_SCAN_COUNT (10)
#define USER_BATT_SCAN_COUNT       (10)

#define USER_BATT_HIGH_POWER     (2555)
#define USER_BATT_LOW_POWER      (2065)
#define USER_BATT_SHUTDOWN_POWER (1865)

#define USER_BATT_DELAY_TIME (100 * 25)
#define USER_TIME_3S_TIME    (100 * 3)

extern uint16_t User_Adc_Batt[USER_BATT_SCAN_COUNT];
extern uint16_t User_Scan_Batt[USER_BATT_SCAN_COUNT];
extern uint8_t  User_Adc_Batt_Count;
extern uint8_t  User_Batt_BaiFen;
extern uint8_t  User_Batt_Old_BaiFen;
extern uint8_t  User_Batt_10ms_Count;
extern uint16_t User_Batt_Time_15S_Count;
extern bool     User_Batt_Power_Up;
extern bool     User_Batt_Send_Spi;
extern uint16_t User_Batt_Power_Up_Delay_100ms_Count;
extern bool     User_Batt_Power_Up_Delay;
extern bool     User_Power_Low;
extern uint8_t  User_Power_Low_Count;
extern uint8_t  es_stdby_pin_state;
extern bool     User_Key_Batt_Num_Show;
extern uint8_t  User_Key_Batt_Count;
extern uint8_t  Batt_Led_Count;

extern const md_adc_initial adc_initStruct;

void Init_Batt_Information(void);
void User_Adc_Init(void);
void User_Adc_Deinit(void);
void U16_Buff_Clear(uint16_t *Buff, uint8_t Len);
void User_Adc_Batt_Power_Up_Init(void);
void User_Adc_Batt_Number(void);
