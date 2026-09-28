// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rdmctmzt_common.h"

#if defined(VIA_ENABLE)
#    include "via.h"
#endif

/************************MCU-driven LEDs**************************/
uint8_t Led_Colour_Tab[9][3] = {
    {255, 0, 0}, {255, 128, 0}, {255, 255, 0}, {0, 255, 0}, {0, 255, 255}, {0, 0, 255}, {128, 0, 255}, {255, 255, 255}, {0, 0, 0},
};
uint8_t Led_Wave_Pwm_Tab[128] = {
      0,   4,   8,  12,  16,  20,  24,  28,  32,  36,  40,  44,  48,  52,  56,  60,
     64,  68,  72,  76,  80,  84,  88,  92,  96, 100, 104, 108, 112, 116, 120, 124,
    128, 132, 136, 140, 144, 148, 152, 156, 160, 164, 168, 172, 176, 180, 184, 188,
    192, 196, 200, 204, 208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 248, 255,
    255, 248, 244, 240, 236, 232, 228, 224, 220, 216, 212, 208, 204, 200, 196, 192,
    188, 184, 180, 176, 172, 168, 164, 160, 156, 152, 148, 144, 140, 136, 132, 128,
    124, 120, 116, 112, 108, 104, 100,  96,  92,  88,  84,  80,  76,  72,  68,  64,
     60,  56,  52,  48,  44,  40,  36,  32,  28,  24,  20,  16,  12,   8,   4,   0,
};

uint8_t Led_Batt_Index_Tab[10] = {16, 17, 18, 19, 20, 21, 22, 23, 24, 25};

uint8_t  Systick_Led_Count      = 0;
uint8_t  Led_Point_Count        = 0;
uint8_t  Mac_Win_Point_Count    = 0;
uint8_t  Debounce_Point_Count   = 0;
uint8_t  Sleep_Time_Point_Count = 0;
bool     Led_Flash_Busy         = false;
bool     Led_Off_Start          = false;
bool     Led_Power_Off          = false;
bool     Led_Power_Up           = false;
uint16_t Led_Power_Up_Delay     = 0;
bool     Usb_If_Ok_Led          = false;
bool     Test_Led               = false;
uint8_t  Test_Colour            = 0;
rgb_led_t rgb_matrix_ws2812_array[RGB_MATRIX_LED_COUNT] = {0};

uint8_t                       g_es_pwm_rgb_matrix_array_dma_buf[(RGB_MATRIX_LED_COUNT * ES_PWM_LED_BYTE) + 2] = {0};
md_dma_channel_config_typedef DMA_list[5]                                                                   = {0};

#if LOGO_LED_ENABLE
uint8_t Logo_Flash_Count = 0;
uint8_t Logo_Led_Count   = 0;
uint8_t LED_Mix_Colour_Tab[256][3] = {
    {255,   0,   0}, {255,   1,   0}, {255,   3,   0}, {255,   4,   0}, {255,   7,   0}, {255,   8,   0}, {255,  10,   0}, {255,  12,   0},
    {255,  14,   0}, {255,  15,   0}, {255,  17,   0}, {255,  21,   0}, {255,  24,   0}, {255,  27,   0}, {255,  30,   0}, {255,  35,   0},
    {255,  38,   0}, {255,  41,   0}, {255,  44,   0}, {255,  45,   0}, {255,  48,   0}, {255,  51,   0}, {255,  55,   0}, {255,  58,   0},
    {255,  62,   0}, {255,  64,   0}, {255,  68,   0}, {255,  71,   0}, {255,  74,   0}, {255,  78,   0}, {255,  80,   0}, {255,  82,   0},
    {255,  84,   0}, {255,  88,   0}, {255,  90,   0}, {255,  95,   0}, {255, 100,   0}, {255, 103,   0}, {255, 107,   0}, {255, 110,   0},
    {255, 113,   0}, {255, 117,   0}, {255, 120,   0}, {255, 124,   0}, {255, 128,   0}, {255, 130,   0}, {255, 132,   0}, {255, 136,   0},
    {255, 140,   0}, {255, 142,   0}, {255, 147,   0}, {255, 149,   0}, {255, 151,   0}, {255, 153,   0}, {255, 158,   0}, {255, 160,   0},
    {255, 162,   0}, {255, 168,   0}, {255, 171,   0}, {255, 174,   0}, {255, 177,   0}, {255, 180,   0}, {255, 183,   0}, {255, 187,   0},
    {255, 190,   0}, {255, 193,   0}, {255, 196,   0}, {255, 199,   0}, {255, 201,   0}, {255, 205,   0}, {255, 208,   0}, {255, 212,   0},
    {255, 220,   0}, {255, 225,   0}, {255, 230,   0}, {255, 235,   0}, {255, 238,   0}, {255, 240,   0}, {255, 245,   0}, {255, 250,   0},
    {255, 254,   0}, {255, 255,   0}, {245, 255,   0}, {231, 255,   0}, {224, 255,   0}, {217, 255,   0}, {203, 255,   0}, {196, 255,   0},
    {189, 255,   0}, {175, 255,   0}, {168, 255,   0}, {161, 255,   0}, {147, 255,   0}, {140, 255,   0}, {133, 255,   0}, {126, 255,   0},
    {119, 255,   0}, {105, 255,   0}, { 98, 255,   0}, { 91, 255,   0}, { 84, 255,   0}, { 77, 255,   0}, { 70, 255,   0}, { 63, 255,   0},
    { 56, 255,   0}, { 49, 255,   0}, { 42, 255,   0}, { 35, 255,   0}, { 28, 255,   0}, { 21, 255,   0}, { 14, 255,   0}, {  7, 255,   0},
    {  0, 255,   0}, {  0, 255,   7}, {  0, 255,  14}, {  0, 255,  21}, {  0, 255,  28}, {  0, 255,  35}, {  0, 255,  42}, {  0, 255,  49},
    {  0, 255,  56}, {  0, 255,  63}, {  0, 255,  70}, {  0, 255,  77}, {  0, 255,  84}, {  0, 255,  91}, {  0, 255,  98}, {  0, 255, 105},
    {  0, 255, 119}, {  0, 255, 126}, {  0, 255, 133}, {  0, 255, 140}, {  0, 255, 154}, {  0, 255, 161}, {  0, 255, 168}, {  0, 255, 182},
    {  0, 255, 189}, {  0, 255, 196}, {  0, 255, 210}, {  0, 255, 217}, {  0, 255, 224}, {  0, 255, 238}, {  0, 255, 245}, {  0, 255, 255},
    {  0, 245, 255}, {  0, 231, 255}, {  0, 224, 255}, {  0, 217, 255}, {  0, 203, 255}, {  0, 196, 255}, {  0, 189, 255}, {  0, 175, 255},
    {  0, 168, 255}, {  0, 161, 255}, {  0, 147, 255}, {  0, 140, 255}, {  0, 133, 255}, {  0, 126, 255}, {  0, 119, 255}, {  0, 105, 255},
    {  0,  98, 255}, {  0,  91, 255}, {  0,  84, 255}, {  0,  77, 255}, {  0,  70, 255}, {  0,  63, 255}, {  0,  56, 255}, {  0,  49, 255},
    {  0,  42, 255}, {  0,  35, 255}, {  0,  28, 255}, {  0,  21, 255}, {  0,  14, 255}, {  0,   7, 255}, {  0,   0, 255}, {  5,   0, 255},
    { 10,   0, 255}, { 15,   0, 255}, { 20,   0, 255}, { 25,   0, 255}, { 30,   0, 255}, { 35,   0, 255}, { 40,   0, 255}, { 45,   0, 255},
    { 50,   0, 255}, { 55,   0, 255}, { 60,   0, 255}, { 65,   0, 255}, { 70,   0, 255}, { 75,   0, 255}, { 85,   0, 255}, { 90,   0, 255},
    { 95,   0, 255}, {100,   0, 255}, {105,   0, 255}, {110,   0, 255}, {115,   0, 255}, {120,   0, 255}, {125,   0, 255}, {130,   0, 255},
    {135,   0, 255}, {140,   0, 255}, {145,   0, 255}, {150,   0, 255}, {155,   0, 255}, {160,   0, 255}, {165,   0, 255}, {170,   0, 255},
    {175,   0, 255}, {185,   0, 255}, {180,   0, 255}, {195,   0, 255}, {190,   0, 255}, {200,   0, 255}, {205,   0, 255}, {210,   0, 255},
    {215,   0, 255}, {220,   0, 255}, {225,   0, 255}, {230,   0, 255}, {235,   0, 255}, {240,   0, 255}, {245,   0, 255}, {250,   0, 255},
    {255,   0, 255}, {255,   0, 255}, {255,   0, 245}, {255,   0, 231}, {255,   0, 224}, {255,   0, 210}, {255,   0, 203}, {255,   0, 196},
    {255,   0, 182}, {255,   0, 175}, {255,   0, 168}, {255,   0, 161}, {255,   0, 147}, {255,   0, 140}, {255,   0, 133}, {255,   0, 126},
    {255,   0, 119}, {255,   0, 105}, {255,   0,  98}, {255,   0,  91}, {255,   0,  84}, {255,   0,  77}, {255,   0,  70}, {255,   0,  63},
    {255,   0,  56}, {255,   0,  49}, {255,   0,  42}, {255,   0,  35}, {255,   0,  28}, {255,   0,  21}, {255,   0,  14}, {255,   0,   7},
};
// Logo LEDs follow the 83 per-key LEDs in g_led_config.
uint8_t Logo_Index_Tab[LOGO_LED_SIZE] = {83, 84, 85, 86};
#endif

#if SIDE_LED_ENABLE
uint8_t Side_Flash_Count              = 0;
uint8_t Side_Led_Count                = 0;
uint8_t Side_Index_Tab[SIDE_LED_SIZE] = {0};
#endif

/************************MCU-driven LEDs**************************/
// WS2812 chain on PA2 driven by GP16C2T1 CH1 PWM at 800 kHz (48 MHz / 60). Each
// LED is 24 compare values (43 = one, 17 = zero) streamed into CCVAL1 by DMA1
// channel 2 in peripheral scatter-gather mode: three tasks of 1008, 1008 and 74
// bytes. The last two bytes of the buffer are zero (reset/latch).
uint8_t Point_Flash_Count = 0;

void rgb_matrix_driver_init(void) {
    md_rcu_enable_dma1(RCU);
    md_dma_set_configuration(DMA1, ENABLE);

    md_rcu_enable_gp16c2t1(RCU);
    md_timer_set_auto_reload_value_arrv(GP16C2T1, 60);
    md_timer_set_output_compare1_mode_ch1mod(GP16C2T1, MD_TIMER_OUTPUTMODE_PWMMODE1);
    md_timer_set_capture_compare1_value_ccrv1(GP16C2T1, 0);
    md_timer_enable_cc1_output_cc1en(GP16C2T1);
    md_timer_enable_main_output_goen(GP16C2T1);
    md_timer_enable_output_compare1_preload_ch1pen(GP16C2T1);
    md_timer_enable_dma_upd(GP16C2T1);
    md_timer_enable_counter_cnten(GP16C2T1);

    gpio_set_pin_output(ES_PWM_DMA_IO);

    // PA2 -> AF5 (GP16C2T1 CH1).
    GPIOA->AFL &= ~(0xF << 8);
    GPIOA->AFL |= (5 << 8);

    GPIOA->MOD &= ~(3 << 4);
    GPIOA->MOD |= (2 << 4);

    md_dma_set_request_peripherals(DMA1, MD_DMA_CHANNEL2, MD_DMA_PRS_GP16C2T1_UP);

    if (rgb_matrix_get_val() == 0) {
        memset(g_es_pwm_rgb_matrix_array_dma_buf, ES_PWM_WS2812_L_VALUE, sizeof(g_es_pwm_rgb_matrix_array_dma_buf));
    }
}

void User_Pwm_Deinit(void) {
    md_rcu_enable_gp16c2t1_reset(RCU);
    md_rcu_disable_gp16c2t1_reset(RCU);
    md_rcu_disable_gp16c2t1(RCU);

    md_rcu_enable_dma1_reset(RCU);
    md_rcu_disable_dma1_reset(RCU);
    md_rcu_disable_dma1(RCU);

    gpio_set_pin_output(ES_PWM_DMA_IO);
    gpio_write_pin_low(ES_PWM_DMA_IO);
}

void rgb_matrix_driver_flush_pwm_dma_start(void) {
    while (DMA1->CHENSET & (1 << 2)) {
    }

    if (Keyboard_Status.System_Sleep_Mode || ((Keyboard_Info.Key_Mode != QMK_USB_MODE) && Usb_Change_Mode_Wakeup && Keyboard_Status.System_Work_Status) || (Led_Power_Up == false)) {
        Led_Off_Start = true;
        gpio_write_pin_low(ES_LED_POWER_IO);
        return;
    }

    if (rgb_matrix_is_enabled() && (Led_Power_Off == false)) {
        gpio_write_pin_high(ES_LED_POWER_IO);
        if (Led_Off_Start) {
            Led_Off_Start = false;
            wait_ms(3);
        }
    } else {
        Led_Off_Start = true;
        gpio_write_pin_low(ES_LED_POWER_IO);
    }

    md_timer_disable_dma_upd(GP16C2T1);

    g_es_pwm_rgb_matrix_array_dma_buf[RGB_MATRIX_LED_COUNT * ES_PWM_LED_BYTE]     = 0;
    g_es_pwm_rgb_matrix_array_dma_buf[RGB_MATRIX_LED_COUNT * ES_PWM_LED_BYTE + 1] = 0;

    DMA_list[0].control.word                  = g_es_dma_ch2alt_cfg | ((ES_PWM_DMA_SIZE - 1) << 4);
    DMA_list[0].source_data_end_address      = (uint32_t)&g_es_pwm_rgb_matrix_array_dma_buf[ES_PWM_DMA_SIZE - 1];
    DMA_list[0].destination_data_end_address = (uint32_t)&GP16C2T1->CCVAL1;

    DMA_list[1].control.word                  = g_es_dma_ch2alt_cfg | ((ES_PWM_DMA_SIZE - 1) << 4);
    DMA_list[1].source_data_end_address      = (uint32_t)&g_es_pwm_rgb_matrix_array_dma_buf[(ES_PWM_DMA_SIZE * 2) - 1];
    DMA_list[1].destination_data_end_address = (uint32_t)&GP16C2T1->CCVAL1;

    uint16_t Data_Size                        = sizeof(g_es_pwm_rgb_matrix_array_dma_buf) - (ES_PWM_DMA_SIZE * 2);
    DMA_list[2].control.word                  = g_es_dma_ch2alt_cfg | ((Data_Size - 1) << 4);
    DMA_list[2].source_data_end_address      = (uint32_t)&g_es_pwm_rgb_matrix_array_dma_buf[(ES_PWM_DMA_SIZE * 2) + Data_Size - 1];
    DMA_list[2].destination_data_end_address = (uint32_t)&GP16C2T1->CCVAL1;

    DMA1->PRI_CH02_SRC_DATA_END_PTR = (uint32_t)&DMA_list[2].reserved;
    DMA1->PRI_CH02_CHANNEL_CFG      = g_es_dma_ch2pri_cfg | (((3 * 4) - 1) << 4);
    DMA1->PRI_CH02_DST_DATA_END_PTR = (uint32_t)&DMA1->RESERVED13;
    DMA1->CHENSET                   = (1 << 2);

    md_timer_enable_dma_upd(GP16C2T1);
}

void rgb_matrix_driver_flush(void) {
    Led_Flash_Busy = false;
    if (GP16C2T1->AR != 60) {
        rgb_matrix_driver_init();
    }

    rgb_matrix_driver_flush_pwm_dma_start();
}

void rgb_matrix_driver_set_color(int index, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t *buf;

    if (index == (RGB_MATRIX_LED_COUNT - 1)) {
        Led_Flash_Busy = false;
    } else if (index == 0) {
        Led_Flash_Busy = true;
    }

    if ((rgb_matrix_ws2812_array[index].r == r) && (rgb_matrix_ws2812_array[index].g == g) && (rgb_matrix_ws2812_array[index].b == b)) {
        return;
    }

    rgb_matrix_ws2812_array[index].r = r;
    rgb_matrix_ws2812_array[index].g = g;
    rgb_matrix_ws2812_array[index].b = b;

    buf = &g_es_pwm_rgb_matrix_array_dma_buf[index * ES_PWM_LED_BYTE];

    for (unsigned char bit = 0; bit < 8; bit++) {
        bool is_one = g & (1 << (7 - bit));

        *buf = is_one ? ES_PWM_WS2812_H_VALUE : ES_PWM_WS2812_L_VALUE;
        buf++;
    }

    for (unsigned char bit = 0; bit < 8; bit++) {
        bool is_one = r & (1 << (7 - bit));

        *buf = is_one ? ES_PWM_WS2812_H_VALUE : ES_PWM_WS2812_L_VALUE;
        buf++;
    }

    for (unsigned char bit = 0; bit < 8; bit++) {
        bool is_one = b & (1 << (7 - bit));

        *buf = is_one ? ES_PWM_WS2812_H_VALUE : ES_PWM_WS2812_L_VALUE;
        buf++;
    }
}

void rgb_matrix_driver_set_color_all(uint8_t r, uint8_t g, uint8_t b) {
    for (uint32_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        rgb_matrix_driver_set_color(i, r, g, b);
    }
}

const rgb_matrix_driver_t rgb_matrix_driver = {
    .init          = rgb_matrix_driver_init,
    .flush         = rgb_matrix_driver_flush,
    .set_color     = rgb_matrix_driver_set_color,
    .set_color_all = rgb_matrix_driver_set_color_all,
};

// Decides whether anything is lit (Led_Power_Off lets the flush cut the LED
// supply; the indicators below clear it when they draw), and blanks every LED
// while all lighting is switched off.
void Led_All_Off_Show(void) {
    bool Keys_Off = (Keyboard_Info.All_Led_Off == INIT_ALL_LED_OFF) || (rgb_matrix_get_val() == 0);
    bool Logo_Off = (Keyboard_Info.Logo_On_Off == LOGO_LED_OFF) || (Keyboard_Info.Logo_Brightness == 0) || (Keyboard_Info.Logo_Mode == LOGO_OFF_MODE);

    Led_Power_Off = Keys_Off && Logo_Off;

    if (Keyboard_Info.All_Led_Off == INIT_ALL_LED_OFF) {
        for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
            rgb_matrix_set_color(i, 0, 0, 0);
        }
    }
}

void Led_Power_Low_Show(void) {
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        rgb_matrix_set_color(i, 0, 0, 0);
    }

    if (Systick_Led_Count < 25) {
        for (uint8_t i = 0; i < LOGO_LED_SIZE; i++) {
            rgb_matrix_set_color(Logo_Index_Tab[i], U_PWM, 0, 0);
        }
    } else {
        for (uint8_t i = 0; i < LOGO_LED_SIZE; i++) {
            rgb_matrix_set_color(Logo_Index_Tab[i], 0, 0, 0);
        }
    }

    if (Systick_Led_Count >= 50) {
        Systick_Led_Count = 0;
    }
}

// Connection indicator on the mode key: fast blink while pairing, slow blink
// while reconnecting, solid for a while once connected.
void Led_Rf_Mode_Show(void) {
    uint8_t Temp_Colour = 0, Led_Index = 0;

    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        rgb_matrix_set_color(i, 0, 0, 0);
    }

    if (Keyboard_Info.Key_Mode == QMK_BLE_MODE) {
        if (Keyboard_Info.Ble_Channel == QMK_BLE_CHANNEL_1) {
            Temp_Colour = 5;
            Led_Index   = LED_BLE_1_INDEX;
        } else if (Keyboard_Info.Ble_Channel == QMK_BLE_CHANNEL_2) {
            Temp_Colour = 5;
            Led_Index   = LED_BLE_2_INDEX;
        } else if (Keyboard_Info.Ble_Channel == QMK_BLE_CHANNEL_3) {
            Temp_Colour = 5;
            Led_Index   = LED_BLE_3_INDEX;
        }
    } else if (Keyboard_Info.Key_Mode == QMK_2P4G_MODE) {
        Temp_Colour = 3;
        Led_Index   = LED_2P4G_INDEX;
    }

    if (Keyboard_Status.System_Connect_Status == KB_MODE_CONNECT_PAIR) {
        if (Systick_Led_Count < 10) {
            rgb_matrix_set_color(Led_Index, Led_Colour_Tab[Temp_Colour][0], Led_Colour_Tab[Temp_Colour][1], Led_Colour_Tab[Temp_Colour][2]);
        } else {
            rgb_matrix_set_color(Led_Index, 0, 0, 0);
        }

        if (Systick_Led_Count >= 20) {
            Systick_Led_Count = 0;
        }
    } else if (Keyboard_Status.System_Connect_Status == KB_MODE_CONNECT_RETURN) {
        if (Systick_Led_Count < 25) {
            rgb_matrix_set_color(Led_Index, Led_Colour_Tab[Temp_Colour][0], Led_Colour_Tab[Temp_Colour][1], Led_Colour_Tab[Temp_Colour][2]);
        } else {
            rgb_matrix_set_color(Led_Index, 0, 0, 0);
        }

        if (Systick_Led_Count >= 50) {
            Systick_Led_Count = 0;
        }
    } else {
        rgb_matrix_set_color(Led_Index, Led_Colour_Tab[Temp_Colour][0], Led_Colour_Tab[Temp_Colour][1], Led_Colour_Tab[Temp_Colour][2]);

        if (Systick_Led_Count >= 240) {
            Systick_Led_Count = 0;
            Led_Rf_Pair_Flg   = false;
            if (Keyboard_Info.Key_Mode == QMK_BLE_MODE) {
                User_Batt_Send_Spi = true;
            }
        }
    }
}

// Battery gauge on the number row: green wave while charging, all green when
// charged, otherwise one key per full 10 %, red below 30 %, yellow below 60 %,
// green above.
void Led_Batt_Number_Show(void) {
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        rgb_matrix_set_color(i, 0, 0, 0);
    }

    if (es_stdby_pin_state == 1) {
        if (Batt_Led_Count > 1) {
            Batt_Led_Count = 0;

            if (User_Key_Batt_Count > 3) {
                User_Key_Batt_Count -= 3;
            } else {
                User_Key_Batt_Count = 127;
            }
        }

        uint8_t Tmep_Pwm = User_Key_Batt_Count;
        for (uint8_t i = 0; i < 10; i++) {
            rgb_matrix_set_color(Led_Batt_Index_Tab[i], 0, Led_Wave_Pwm_Tab[Tmep_Pwm], 0);
            Tmep_Pwm += 8;
            if (Tmep_Pwm > 127) {
                Tmep_Pwm -= 128;
            }
        }
    } else if (es_stdby_pin_state == 2) {
        for (uint8_t i = 0; i < 10; i++) {
            rgb_matrix_set_color(Led_Batt_Index_Tab[i], 0, 255, 0);
        }
    } else {
        uint8_t Colour_R, Colour_G, Colour_B;
        uint8_t Temp_Count = Keyboard_Info.Batt_Number / 10;

        if (Keyboard_Info.Batt_Number < 30) {
            Colour_R = 255, Colour_G = 0, Colour_B = 0;
        } else if (Keyboard_Info.Batt_Number < 60) {
            Colour_R = 255, Colour_G = 255, Colour_B = 0;
        } else {
            Colour_R = 0, Colour_G = 255, Colour_B = 0;
        }

        for (uint8_t i = 0; i < Temp_Count; i++) {
            rgb_matrix_set_color(Led_Batt_Index_Tab[i], Colour_R, Colour_G, Colour_B);
        }
    }
}

// Logo flash confirming a setting change, one blink per count: white
// (Led_Point_Count), yellow (Mac_Win_Point_Count), green 5 ms / red 2 ms
// (Debounce_Point_Count), or red 1 / green 3 / blue 10 / white 30 min
// (Sleep_Time_Point_Count).
static void Logo_Set_Colour(uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t i = 0; i < LOGO_LED_SIZE; i++) {
        rgb_matrix_set_color(Logo_Index_Tab[i], r, g, b);
    }
}

void Led_Point_Flash_Show(void) {
    if (Logo_Flash_Count < 25) {
        if (Led_Point_Count) {
            Logo_Set_Colour(U_PWM, U_PWM, U_PWM);
        } else if (Mac_Win_Point_Count) {
            Logo_Set_Colour(U_PWM, U_PWM, 0);
        } else if (Debounce_Point_Count) {
            if (Keyboard_Info.Debounce != DEBOUNCE_FAST) {
                Logo_Set_Colour(0, U_PWM, 0);
            } else {
                Logo_Set_Colour(U_PWM, 0, 0);
            }
        } else {
            switch (Keyboard_Info.Sleep_Time) {
                case SLEEP_TIME_1MIN:  Logo_Set_Colour(U_PWM, 0, 0);         break;
                case SLEEP_TIME_3MIN:  Logo_Set_Colour(0, U_PWM, 0);         break;
                case SLEEP_TIME_10MIN: Logo_Set_Colour(0, 0, U_PWM);         break;
                case SLEEP_TIME_30MIN: Logo_Set_Colour(U_PWM, U_PWM, U_PWM); break;
            }
        }
    } else {
        for (uint8_t i = 0; i < LOGO_LED_SIZE; i++) {
            rgb_matrix_set_color(Logo_Index_Tab[i], 0, 0, 0);
        }
    }

    if (Logo_Flash_Count >= 50) {
        Logo_Flash_Count = 0;

        if (Led_Point_Count) {
            Led_Point_Count--;
        } else if (Mac_Win_Point_Count) {
            Mac_Win_Point_Count--;
        } else if (Debounce_Point_Count) {
            Debounce_Point_Count--;
        } else if (Sleep_Time_Point_Count) {
            Sleep_Time_Point_Count--;
        }
    }
}

// Logo: setting flashes, else its effect with Caps Lock shown in white.
void User_Point_Show(void) {
    if (Led_Point_Count || Mac_Win_Point_Count || Debounce_Point_Count || Sleep_Time_Point_Count) {
        Led_Power_Off = false;
        Led_Point_Flash_Show();
    } else {
        Logo_Flash_Count = 0;
        Logo_Mode_Show();

        bool Caps_Lock;
        if (Keyboard_Info.Key_Mode == QMK_USB_MODE) {
            Caps_Lock = host_keyboard_led_state().caps_lock && Usb_If_Ok_Led;
        } else {
            Caps_Lock = Keyboard_Status.System_Led_Status & (1 << 1);
        }

        if (Caps_Lock) {
            Led_Power_Off = false;
            Logo_Set_Colour(U_PWM, U_PWM, U_PWM);
        }
    }

    if ((Led_Rf_Pair_Flg == false) || (Keyboard_Info.Key_Mode == QMK_USB_MODE)) {
        if ((User_Key_Batt_Num_Show == false) && Keyboard_Info.Win_Lock && ((Keyboard_Info.Key_Mode != QMK_USB_MODE) || Usb_If_Ok_Led)) {
            Led_Power_Off = false;
            rgb_matrix_set_color(LED_WIN_L_INDEX, U_PWM, U_PWM, U_PWM);
        }
    }
}

void User_Test_Colour_Show(void) {
    uint8_t Test_R, Test_G, Test_B;
    switch (Test_Colour) {
        case 0: Test_R = U_PWM, Test_G = 0, Test_B = 0; break;
        case 1: Test_R = 0, Test_G = U_PWM, Test_B = 0; break;
        case 2: Test_R = 0, Test_G = 0, Test_B = U_PWM; break;
        case 3: Test_R = U_PWM, Test_G = U_PWM, Test_B = U_PWM; break;
        default: Test_R = U_PWM, Test_G = U_PWM, Test_B = U_PWM; break;
    }

    rgb_matrix_driver_set_color_all(Test_R, Test_G, Test_B);
}

void User_Led_Show(void) {
    Led_All_Off_Show();

    if (User_Power_Low) {
        Led_Power_Off = false;
        Led_Power_Low_Show();
    } else if (Test_Led) {
        Led_Power_Off = false;
        User_Test_Colour_Show();
    } else if (Led_Rf_Pair_Flg && (Keyboard_Info.Key_Mode != QMK_USB_MODE)) {
        Led_Power_Off = false;
        Led_Rf_Mode_Show();
        User_Point_Show();
    } else if (User_Key_Batt_Num_Show) {
        Led_Power_Off = false;
        Led_Batt_Number_Show();
        User_Point_Show();
    } else {
        User_Point_Show();

        if (Key_Fn_Status) {
            switch (Keyboard_Info.Key_Mode) {
                case QMK_BLE_MODE:
                    switch (Keyboard_Info.Ble_Channel) {
                        case QMK_BLE_CHANNEL_1: rgb_matrix_set_color(LED_BLE_1_INDEX, U_PWM, U_PWM, U_PWM); break;
                        case QMK_BLE_CHANNEL_2: rgb_matrix_set_color(LED_BLE_2_INDEX, U_PWM, U_PWM, U_PWM); break;
                        case QMK_BLE_CHANNEL_3: rgb_matrix_set_color(LED_BLE_3_INDEX, U_PWM, U_PWM, U_PWM); break;
                    }
                    break;
                case QMK_2P4G_MODE:
                    rgb_matrix_set_color(LED_2P4G_INDEX, U_PWM, U_PWM, U_PWM);
                    break;
                case QMK_USB_MODE:
                    rgb_matrix_set_color(LED_USB_INDEX, U_PWM, U_PWM, U_PWM);
                    break;
            }
            Led_Power_Off = false;
        }
    }
}


/************************Side lights (logo)**************************/
#if LOGO_LED_ENABLE
uint8_t Logo_Play_Point = 0;
uint8_t Logo_Pwm_R      = 0;
uint8_t Logo_Pwm_G      = 0;
uint8_t Logo_Pwm_B      = 0;

void Logo_Init(void) {
    for (uint8_t i = 0; i < LOGO_LED_SIZE; i++) {
        rgb_matrix_set_color(Logo_Index_Tab[i], 0, 0, 0);
    }
    Logo_Play_Point = 0x40;
    Logo_Pwm_R      = 0;
    Logo_Pwm_G      = 0;
    Logo_Pwm_B      = 0;
}

// Saturation first (it is stored inverted: 0 = full colour), then brightness.
void Logo_Pwm_Rgb_Update(uint8_t Pwm) {
    Logo_Pwm_R |= Keyboard_Info.Logo_Saturation;
    Logo_Pwm_G |= Keyboard_Info.Logo_Saturation;
    Logo_Pwm_B |= Keyboard_Info.Logo_Saturation;

    uint16_t Temp_Pwm;
    Temp_Pwm   = Logo_Pwm_R * Pwm;
    Logo_Pwm_R = Temp_Pwm >> 8;
    Temp_Pwm   = Logo_Pwm_G * Pwm;
    Logo_Pwm_G = Temp_Pwm >> 8;
    Temp_Pwm   = Logo_Pwm_B * Pwm;
    Logo_Pwm_B = Temp_Pwm >> 8;
}

// Wave/breath level (Pwm) first, then saturation and brightness.
void Logo_Pwm_Ds_Update(uint8_t Pwm) {
    uint16_t Temp_Pwm;
    Temp_Pwm   = Logo_Pwm_R * Pwm;
    Temp_Pwm   = Temp_Pwm >> 8;
    Temp_Pwm   = Temp_Pwm | Keyboard_Info.Logo_Saturation;
    Temp_Pwm   = Temp_Pwm * Keyboard_Info.Logo_Brightness;
    Logo_Pwm_R = Temp_Pwm >> 8;

    Temp_Pwm   = Logo_Pwm_G * Pwm;
    Temp_Pwm   = Temp_Pwm >> 8;
    Temp_Pwm   = Temp_Pwm | Keyboard_Info.Logo_Saturation;
    Temp_Pwm   = Temp_Pwm * Keyboard_Info.Logo_Brightness;
    Logo_Pwm_G = Temp_Pwm >> 8;

    Temp_Pwm   = Logo_Pwm_B * Pwm;
    Temp_Pwm   = Temp_Pwm >> 8;
    Temp_Pwm   = Temp_Pwm | Keyboard_Info.Logo_Saturation;
    Temp_Pwm   = Temp_Pwm * Keyboard_Info.Logo_Brightness;
    Logo_Pwm_B = Temp_Pwm >> 8;
}

void Logo_Wave_Rgb_mode_Show(void) {
    if (Logo_Led_Count) {
        Logo_Led_Count = 0;
        if (Keyboard_Info.Logo_Speed) {
            if (Logo_Play_Point >= Keyboard_Info.Logo_Speed) {
                Logo_Play_Point -= Keyboard_Info.Logo_Speed;
            } else {
                Logo_Play_Point = Logo_Play_Point + 255 - Keyboard_Info.Logo_Speed;
            }
        }
    }

    uint8_t Temp_Point = Logo_Play_Point;
    for (uint8_t i = 0; i < LOGO_LED_SIZE; i++) {
        Logo_Pwm_R = LED_Mix_Colour_Tab[Temp_Point][0];
        Logo_Pwm_G = LED_Mix_Colour_Tab[Temp_Point][1];
        Logo_Pwm_B = LED_Mix_Colour_Tab[Temp_Point][2];

        Logo_Pwm_Rgb_Update(Keyboard_Info.Logo_Brightness);

        rgb_matrix_set_color(Logo_Index_Tab[i], Logo_Pwm_R, Logo_Pwm_G, Logo_Pwm_B);

        Temp_Point += 12;
        if (Temp_Point == 255) {
            Temp_Point = 0;
        }
    }
}

void Logo_Wave_Ds_mode_Show(void) {
    if (Logo_Led_Count) {
        Logo_Led_Count = 0;
        if (Keyboard_Info.Logo_Speed) {
            if (Logo_Play_Point >= Keyboard_Info.Logo_Speed) {
                Logo_Play_Point -= Keyboard_Info.Logo_Speed;
            } else {
                Logo_Play_Point = Logo_Play_Point + 127 - Keyboard_Info.Logo_Speed;
            }
        }
    }

    uint8_t Temp_Point = Logo_Play_Point;
    for (uint8_t i = 0; i < LOGO_LED_SIZE; i++) {
        Logo_Pwm_R = LED_Mix_Colour_Tab[Keyboard_Info.Logo_Colour][0];
        Logo_Pwm_G = LED_Mix_Colour_Tab[Keyboard_Info.Logo_Colour][1];
        Logo_Pwm_B = LED_Mix_Colour_Tab[Keyboard_Info.Logo_Colour][2];

        Logo_Pwm_Ds_Update(Led_Wave_Pwm_Tab[Temp_Point]);

        rgb_matrix_set_color(Logo_Index_Tab[i], Logo_Pwm_R, Logo_Pwm_G, Logo_Pwm_B);

        Temp_Point += 8;
        if (Temp_Point > 126) {
            Temp_Point = 0;
        }
    }
}

void Logo_Spectrum_mode_Show(void) {
    if (Logo_Led_Count) {
        Logo_Led_Count = 0;
        if (Keyboard_Info.Logo_Speed) {
            if (Logo_Play_Point >= Keyboard_Info.Logo_Speed) {
                Logo_Play_Point -= Keyboard_Info.Logo_Speed;
            } else {
                Logo_Play_Point = Logo_Play_Point + 255 - Keyboard_Info.Logo_Speed;
            }
        }
    }

    Logo_Pwm_R = LED_Mix_Colour_Tab[Logo_Play_Point][0];
    Logo_Pwm_G = LED_Mix_Colour_Tab[Logo_Play_Point][1];
    Logo_Pwm_B = LED_Mix_Colour_Tab[Logo_Play_Point][2];

    Logo_Pwm_Rgb_Update(Keyboard_Info.Logo_Brightness);

    for (uint8_t i = 0; i < LOGO_LED_SIZE; i++) {
        rgb_matrix_set_color(Logo_Index_Tab[i], Logo_Pwm_R, Logo_Pwm_G, Logo_Pwm_B);
    }
}

void Logo_Breath_mode_Show(void) {
    if (Logo_Led_Count) {
        Logo_Led_Count = 0;
        if (Keyboard_Info.Logo_Speed) {
            if (Logo_Play_Point >= Keyboard_Info.Logo_Speed) {
                Logo_Play_Point -= Keyboard_Info.Logo_Speed;
            } else {
                Logo_Play_Point = Logo_Play_Point + 127 - Keyboard_Info.Logo_Speed;
            }
        }
    }

    Logo_Pwm_R = LED_Mix_Colour_Tab[Keyboard_Info.Logo_Colour][0];
    Logo_Pwm_G = LED_Mix_Colour_Tab[Keyboard_Info.Logo_Colour][1];
    Logo_Pwm_B = LED_Mix_Colour_Tab[Keyboard_Info.Logo_Colour][2];

    Logo_Pwm_Ds_Update(Led_Wave_Pwm_Tab[Logo_Play_Point]);

    for (uint8_t i = 0; i < LOGO_LED_SIZE; i++) {
        rgb_matrix_set_color(Logo_Index_Tab[i], Logo_Pwm_R, Logo_Pwm_G, Logo_Pwm_B);
    }
}

void Logo_Light_mode_Show(void) {
    Logo_Pwm_R = LED_Mix_Colour_Tab[Keyboard_Info.Logo_Colour][0];
    Logo_Pwm_G = LED_Mix_Colour_Tab[Keyboard_Info.Logo_Colour][1];
    Logo_Pwm_B = LED_Mix_Colour_Tab[Keyboard_Info.Logo_Colour][2];
    Logo_Pwm_Ds_Update(0xFF);
    for (uint8_t i = 0; i < LOGO_LED_SIZE; i++) {
        rgb_matrix_set_color(Logo_Index_Tab[i], Logo_Pwm_R, Logo_Pwm_G, Logo_Pwm_B);
    }
}

void Logo_Off_mode_Show(void) {
    for (uint8_t i = 0; i < LOGO_LED_SIZE; i++) {
        rgb_matrix_set_color(Logo_Index_Tab[i], 0, 0, 0);
    }
}

void Logo_Mode_Show(void) {
    if (Keyboard_Info.All_Led_Off == INIT_ALL_LED_OFF) {
        Logo_Off_mode_Show();
        return;
    }

    if (Keyboard_Info.Logo_On_Off == LOGO_LED_ON) {
        switch (Keyboard_Info.Logo_Mode) {
            case LOGO_WAVE_RGB_MODE: Logo_Wave_Rgb_mode_Show(); break;
            case LOGO_WAVE_DS_MODE: Logo_Wave_Ds_mode_Show(); break;
            case LOGO_SPECTRUM_MODE: Logo_Spectrum_mode_Show(); break;
            case LOGO_BREATH_MODE: Logo_Breath_mode_Show(); break;
            case LOGO_LIGHT_MODE: Logo_Light_mode_Show(); break;
            default: Logo_Off_mode_Show(); break;
        }
    } else {
        Logo_Off_mode_Show();
    }
}

#if defined(VIA_ENABLE)
// VIA's "rgblight" channel drives the logo LEDs.
void User_Via_Qmk_Logo_Get_Value(uint8_t *data) {
    // data = [ value_id, value_data ]
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);

    switch (*value_id) {
        case id_qmk_rgblight_brightness: {
            value_data[0] = Keyboard_Info.Logo_Brightness;
            break;
        }
        case id_qmk_rgblight_effect: {
            value_data[0] = Keyboard_Info.Logo_Mode;
            break;
        }
        case id_qmk_rgblight_effect_speed: {
            value_data[0] = Keyboard_Info.Logo_Speed;
            break;
        }
        case id_qmk_rgblight_color: {
            value_data[0] = Keyboard_Info.Logo_Colour;
            value_data[1] = ~Keyboard_Info.Logo_Saturation;
            break;
        }
    }
}

void User_Via_Qmk_Logo_Set_Value(uint8_t *data) {
    // data = [ value_id, value_data ]
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);

    switch (*value_id) {
        case id_qmk_rgblight_brightness: {
            if (value_data[0] > LOGO_MAX_BRIGHTNESS) {
                Keyboard_Info.Logo_Brightness = LOGO_MAX_BRIGHTNESS;
            } else {
                Keyboard_Info.Logo_Brightness = value_data[0];
            }
            break;
        }
        case id_qmk_rgblight_effect: {
            if (value_data[0] == 0) {
                Keyboard_Info.Logo_On_Off = LOGO_LED_OFF;
            } else {
                Keyboard_Info.Logo_On_Off = LOGO_LED_ON;
                if (value_data[0] > LOGO_OFF_MODE) {
                    Keyboard_Info.Logo_Mode = INIT_LOGO_MODE;
                } else {
                    Keyboard_Info.Logo_Mode = value_data[0];
                }
            }
            break;
        }
        case id_qmk_rgblight_effect_speed: {
            if (value_data[0] > LOGO_MAX_SPEED) {
                Keyboard_Info.Logo_Speed = LOGO_MAX_SPEED;
            } else {
                Keyboard_Info.Logo_Speed = value_data[0];
            }
            break;
        }
        case id_qmk_rgblight_color: {
            Keyboard_Info.Logo_Colour     = value_data[0];
            Keyboard_Info.Logo_Saturation = ~value_data[1];
            break;
        }
    }
}

void User_Via_Qmk_Logo_Command(uint8_t *data, uint8_t length) {
    // data = [ command_id, channel_id, value_id, value_data ]
    uint8_t *command_id        = &(data[0]);
    uint8_t *value_id_and_data = &(data[2]);

    switch (*command_id) {
        case id_custom_set_value: {
            User_Via_Qmk_Logo_Set_Value(value_id_and_data);
            break;
        }
        case id_custom_get_value: {
            User_Via_Qmk_Logo_Get_Value(value_id_and_data);
            break;
        }
        case id_custom_save: {
            Save_Flash_Set();
            break;
        }
        default: {
            *command_id = id_unhandled;
            break;
        }
    }
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    if (data[1] == id_qmk_rgblight_channel) {
        User_Via_Qmk_Logo_Command(data, length);
        return;
    }

    data[0] = id_unhandled;
}
#endif
#endif
