// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"
#include "raw_hid.h"
#include "usb_main.h"

// Board I/O shared by the common modules.
#define WHEEL_ZA_IO      (B2)
#define WHEEL_ZB_IO      (B10)
#define ES_BATT_ADC_IO   (C4)
#define ES_BATT_STDBY_IO (A13)
#define ES_USB_POWER_IO  (C5)
#define ES_SPI_ACK_IO    (A4)
#define ES_PWM_DMA_IO    (A2)
#define ES_WAKEUP_IO     (D1)
#define ES_SDB_POWER_IO  (A3)
#define ES_LED_POWER_IO  (D0)

#define LOGO_LED_ENABLE (1)
#define SIDE_LED_ENABLE (0)

enum Custom_KeyModes {
    QMK_BLE_MODE = 0,
    QMK_2P4G_MODE,
    QMK_USB_MODE
};

enum Custom_BleChannels {
    QMK_BLE_CHANNEL_1 = 1,
    QMK_BLE_CHANNEL_2,
    QMK_BLE_CHANNEL_3
};

enum Custom_Spi_Ack_S {
    SPI_NACK,
    SPI_ACK
};

enum Custom_Spi_Busy_S {
    SPI_BUSY,
    SPI_IDLE
};

enum Custom_Ble_24G_Status_S {
    BLE_24G_NONE,
    BLE_24G_PAIR,
    BLE_24G_RETURN
};

typedef enum {
    KB_MODE_CONNECT_OK,
    KB_MODE_CONNECT_PAIR,
    KB_MODE_CONNECT_RETURN,
} keyboard_System_state_e;

typedef enum {
    USER_SLEEP_PASS,
    USER_SLEEP_FAIL,
} keyboard_System_Sleep_Status_s;

// Defaults saved in the keyboard information record.
#define INIT_WORK_MODE      (QMK_USB_MODE)
#define INIT_BLE_CHANNEL    (QMK_BLE_CHANNEL_1)
#define INIT_BATT_NUMBER    (50)
#define INIT_SIX_KEY        (0)
#define INIT_ALL_KEY        (1)
#define INIT_ALL_SIX_KEY    (INIT_ALL_KEY)
#define INIT_WIN_MODE       (0)
#define INIT_MAC_MODE       (1)
#define INIT_WIN_MAC_MODE   (INIT_WIN_MODE)
#define INIT_WIN_NLOCK      (0)
#define INIT_WIN_LOCK       (1)
#define INIT_WIN_LOCK_NLOCK (INIT_WIN_NLOCK)
#define INIT_ALL_LED_ON     (0)
#define INIT_ALL_LED_OFF    (1)

#define SLEEP_TIME_1MIN  (60)
#define SLEEP_TIME_3MIN  (180)
#define SLEEP_TIME_10MIN (600)
#define SLEEP_TIME_30MIN (1800)
#define INIT_SLEEP_TIME  (SLEEP_TIME_3MIN)

#define INIT_RF_TIMER_2 (0xFFFFFFFE)
#define DEBOUNCE_FAST   (2)
#define DEBOUNCE_SLOW   (5)
#define INIT_DEBOUNCE   (DEBOUNCE_SLOW)

// Keep this order aligned with Womier firmware 0.1.5 and its VIA definition.
#define USER_DEFINE_KEY (QK_KB)
enum Custom_Keycodes {
    QMK_KB_MODE_2P4G = USER_DEFINE_KEY,
    QMK_KB_MODE_BLE1,
    QMK_KB_MODE_BLE2,
    QMK_KB_MODE_BLE3,
    QMK_KB_MODE_USB,
    QMK_BATT_NUM,
    QMK_WIN_LOCK,
    QMK_KB_SIX_N_CH,
    QMK_TEST_COLOUR,
    QMK_SLEEP_TIME,
    QMK_RF_TIMER_2_ADD,
    QMK_DEBOUNCE,
    QMK_ALL_LED_TOG,
#if LOGO_LED_ENABLE
    LOGO_TOG,
    LOGO_MOD,
    LOGO_RMOD,
    LOGO_HUI,
    LOGO_HUD,
    LOGO_SAI,
    LOGO_SAD,
    LOGO_VAI,
    LOGO_VAD,
    LOGO_SPI,
    LOGO_SPD,
#endif
#if SIDE_LED_ENABLE
    SIDE_TOG,
    SIDE_MOD,
    SIDE_RMOD,
    SIDE_HUI,
    SIDE_HUD,
    SIDE_SAI,
    SIDE_SAD,
    SIDE_VAI,
    SIDE_VAD,
    SIDE_SPI,
    SIDE_SPD,
#endif
    QMK_KB_2P4G_PAIR,
    QMK_KB_BLE1_PAIR,
    QMK_KB_BLE2_PAIR,
    QMK_KB_BLE3_PAIR
};

#define WIN_COL (1)
#define WIN_ROW (3)
#define MAC_COL (2)
#define MAC_ROW (3)

#define KC_K29  KC_BACKSLASH
#define KC_K42  KC_NONUS_HASH
#define KC_K45  KC_NONUS_BACKSLASH
#define KC_K56  KC_INTERNATIONAL_1
#define KC_K14  KC_INTERNATIONAL_3
#define KC_K132 KC_INTERNATIONAL_4
#define KC_K131 KC_INTERNATIONAL_5
#define KC_K133 KC_INTERNATIONAL_2
#define KC_K151 KC_LANGUAGE_1
#define KC_K150 KC_LANGUAGE_2

#define MD_24G  QMK_KB_MODE_2P4G
#define MD_BLE1 QMK_KB_MODE_BLE1
#define MD_BLE2 QMK_KB_MODE_BLE2
#define MD_BLE3 QMK_KB_MODE_BLE3
#define MD_USB  QMK_KB_MODE_USB
#define QK_BAT  QMK_BATT_NUM
#define QK_WLO  QMK_WIN_LOCK
#define SIX_N   QMK_KB_SIX_N_CH
#define TEST_CL QMK_TEST_COLOUR
#define SLP_TIM QMK_SLEEP_TIME
#define DEB_TOG QMK_DEBOUNCE
#define LED_TOG QMK_ALL_LED_TOG

// Saved in flash; this layout is part of the firmware 0.1.5 compatibility contract.
typedef struct {
    uint8_t  Key_Mode;
    uint8_t  Ble_Channel;
    uint8_t  Batt_Number;
    uint8_t  Nkro;
    uint8_t  Mac_Win_Mode;
    uint8_t  Win_Lock;
    uint8_t  All_Led_Off;
    uint8_t  Reserved;
    uint32_t Sleep_Time;
    uint32_t Rf_Timer_2;
    uint8_t  Debounce;
#if LOGO_LED_ENABLE
    uint8_t Logo_On_Off;
    uint8_t Logo_Mode;
    uint8_t Logo_Colour;
    uint8_t Logo_Saturation;
    uint8_t Logo_Brightness;
    uint8_t Logo_Speed;
#endif
#if SIDE_LED_ENABLE
    uint8_t Side_On_Off;
    uint8_t Side_Mode;
    uint8_t Side_Colour;
    uint8_t Side_Saturation;
    uint8_t Side_Brightness;
    uint8_t Side_Speed;
#endif
} Keyboard_Info_t;

extern Keyboard_Info_t Keyboard_Info;

#if LOGO_LED_ENABLE && !SIDE_LED_ENABLE
_Static_assert(sizeof(Keyboard_Info_t) == 24, "Keyboard_Info_t must keep the firmware 0.1.5 layout");
#endif

typedef struct {
    uint8_t System_Work_Status;
    uint8_t System_Work_Mode;
    uint8_t System_Work_Channel;
    uint8_t System_Connect_Status;
    uint8_t System_Led_Status;
    uint8_t System_Sleep_Mode;
} Keyboard_Status_t;

extern Keyboard_Status_t Keyboard_Status;
extern bool              Key_2p4g_Status;
extern bool              Key_Ble_1_Status;
extern bool              Key_Ble_2_Status;
extern bool              Key_Ble_3_Status;
extern bool              Key_Fn_Status;
extern bool              Key_Reset_Status;
extern bool              Key_Debounce_Status;
extern bool              Key_Sleep_Time_Status;
extern bool              Func_Key_Long_Press;
extern uint8_t           Systick_6ms_Count;
extern uint8_t           Systick_10ms_Count;
extern uint16_t          Systick_Interval_Count;
extern uint16_t          Time_3s_Count;
extern uint16_t          Func_Time_3s_Count;
extern uint8_t           Debounce_Time;

// Public interfaces for the common subsystems.
#include "three_mode.h"
#include "user_battery.h"
#include "user_eeprom.h"
#include "user_emi.h"
#include "user_led_custom.h"
#include "user_spi.h"
#include "user_system.h"
