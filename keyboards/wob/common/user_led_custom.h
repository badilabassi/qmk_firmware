// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"

#define ES_PWM_LED_SIZE       (42)
#define ES_PWM_LED_BYTE       (24)
#define ES_PWM_DMA_SIZE       (ES_PWM_LED_SIZE * ES_PWM_LED_BYTE)
#define ES_PWM_WS2812_H_VALUE (43)
#define ES_PWM_WS2812_L_VALUE (17)
#define U_PWM                 (RGB_MATRIX_MAXIMUM_BRIGHTNESS)

#define LED_CAP_INDEX   (45)
#define LED_CAP_1_INDEX (46)
#define LED_WIN_L_INDEX (75)
#define LED_BLE_1_INDEX (16)
#define LED_BLE_2_INDEX (17)
#define LED_BLE_3_INDEX (18)
#define LED_2P4G_INDEX  (19)
#define LED_USB_INDEX   (20)

extern uint8_t Led_Colour_Tab[9][3];
extern uint8_t Led_Wave_Pwm_Tab[128];
extern uint8_t Led_Batt_Index_Tab[10];
extern uint8_t Systick_Led_Count;
extern uint8_t Led_Point_Count;
extern uint8_t Mac_Win_Point_Count;
extern uint8_t Debounce_Point_Count;
extern uint8_t Sleep_Time_Point_Count;
extern bool    Led_Flash_Busy;
extern bool    Led_Off_Start;
extern bool    Led_Power_Off;
extern bool    Led_Power_Up;
extern uint16_t Led_Power_Up_Delay;
extern bool     Usb_If_Ok_Led;
extern bool     Test_Led;
extern uint8_t  Test_Colour;

extern rgb_led_t                     rgb_matrix_ws2812_array[RGB_MATRIX_LED_COUNT];
extern uint8_t                       g_es_pwm_rgb_matrix_array_dma_buf[(RGB_MATRIX_LED_COUNT * ES_PWM_LED_BYTE) + 2];
extern md_dma_channel_config_typedef DMA_list[5];
extern const rgb_matrix_driver_t     rgb_matrix_driver;

void rgb_matrix_driver_init(void);
void User_Pwm_Deinit(void);
void rgb_matrix_driver_flush_pwm_dma_start(void);
void rgb_matrix_driver_flush(void);
void rgb_matrix_driver_set_color(int index, uint8_t r, uint8_t g, uint8_t b);
void rgb_matrix_driver_set_color_all(uint8_t r, uint8_t g, uint8_t b);
void Led_All_Off_Show(void);
void Led_Power_Low_Show(void);
void Led_Rf_Mode_Show(void);
void Led_Batt_Number_Show(void);
void Led_Point_Flash_Show(void);
void User_Point_Show(void);
void User_Led_Show(void);
void User_Test_Colour_Show(void);

#if LOGO_LED_ENABLE
#    define LOGO_LED_PLAY_SPEED (0)
#    define LOGO_LED_SIZE       (4)
#    define LOGO_LED_ON         (0)
#    define LOGO_LED_OFF        (1)
#    define LOGO_WAVE_RGB_MODE  (1)
#    define LOGO_WAVE_DS_MODE   (2)
#    define LOGO_SPECTRUM_MODE  (3)
#    define LOGO_BREATH_MODE    (4)
#    define LOGO_LIGHT_MODE     (5)
#    define LOGO_OFF_MODE       (6)
#    define LOGO_MAX_COLOUR     (255)
#    define LOGO_MIN_COLOUR     (0)
#    define COLOUR_LEVEL        (15)
#    define LOGO_MAX_SATURATION (0)
#    define LOGO_MIN_SATURATION (255)
#    define SATURATION_LEVEL    (15)
#    define LOGO_MAX_BRIGHTNESS (RGB_MATRIX_MAXIMUM_BRIGHTNESS)
#    define LOGO_MIN_BRIGHTNESS (0)
#    define BRIGHTNESS_LEVEL    (15)
#    define LOGO_MAX_SPEED      (4)
#    define LOGO_MIN_SPEED      (0)
#    define SPEED_LEVEL         (1)
#    define INIT_LOGO_ON_OFF    (LOGO_LED_ON)
#    define INIT_LOGO_MODE      (LOGO_WAVE_RGB_MODE)
#    define INIT_LOGO_COLOUR    (LOGO_MIN_COLOUR)
#    define INIT_LOGO_SATURATION (LOGO_MAX_SATURATION)
#    define INIT_LOGO_BRIGHTNESS (LOGO_MAX_BRIGHTNESS)
#    define INIT_LOGO_SPEED      (2)

extern uint8_t Logo_Flash_Count;
extern uint8_t Logo_Led_Count;
extern uint8_t LED_Mix_Colour_Tab[256][3];
extern uint8_t Logo_Index_Tab[LOGO_LED_SIZE];

void Logo_Init(void);
void Logo_Mode_Show(void);
#    if defined(VIA_ENABLE)
void User_Via_Qmk_Logo_Get_Value(uint8_t *data);
void User_Via_Qmk_Logo_Set_Value(uint8_t *data);
void User_Via_Qmk_Logo_Command(uint8_t *data, uint8_t length);
#    endif
#endif

#if SIDE_LED_ENABLE
#    define SIDE_LED_PLAY_SPEED  (0)
#    define SIDE_LED_SIZE        (38)
#    define SIDE_LED_ON          (0)
#    define SIDE_LED_OFF         (1)
#    define SIDE_WAVE_RGB_MODE   (1)
#    define SIDE_WAVE_DS_MODE    (2)
#    define SIDE_SPECTRUM_MODE   (3)
#    define SIDE_BREATH_MODE     (4)
#    define SIDE_LIGHT_MODE      (5)
#    define SIDE_OFF_MODE        (6)
#    define SIDE_MAX_COLOUR      (255)
#    define SIDE_MIN_COLOUR      (0)
#    define SIDE_COLOUR_LEVEL    (15)
#    define SIDE_MAX_SATURATION  (0)
#    define SIDE_MIN_SATURATION  (255)
#    define SIDE_SATURATION_LEVEL (15)
#    define SIDE_MAX_BRIGHTNESS   (RGB_MATRIX_MAXIMUM_BRIGHTNESS)
#    define SIDE_MIN_BRIGHTNESS   (0)
#    define SIDE_BRIGHTNESS_LEVEL (15)
#    define SIDE_MAX_SPEED        (4)
#    define SIDE_MIN_SPEED        (0)
#    define SIDE_SPEED_LEVEL      (1)
#    define INIT_SIDE_ON_OFF      (SIDE_LED_ON)
#    define INIT_SIDE_MODE        (SIDE_WAVE_RGB_MODE)
#    define INIT_SIDE_COLOUR      (SIDE_MIN_COLOUR)
#    define INIT_SIDE_SATURATION  (SIDE_MAX_SATURATION)
#    define INIT_SIDE_BRIGHTNESS  (SIDE_MAX_BRIGHTNESS)
#    define INIT_SIDE_SPEED       (2)

extern uint8_t Side_Flash_Count;
extern uint8_t Side_Led_Count;
extern uint8_t Side_Index_Tab[SIDE_LED_SIZE];

void Side_Init(void);
void Side_Mode_Show(void);
void User_Via_Qmk_Side_Get_Value(uint8_t *data);
void User_Via_Qmk_Side_Set_Value(uint8_t *data);
void User_Via_Qmk_Side_Command(uint8_t *data, uint8_t length);
#endif
