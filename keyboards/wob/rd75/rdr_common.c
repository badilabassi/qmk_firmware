// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

/* Reimplementation of the vendor rdr_common.c (librdrcommon.a, firmware 0.1.2)
 * for the Womier RD75 (ES32 FS026): wireless (SPI to the radio), battery,
 * sleep, the per-key WS2812 driver, the logo LEDs, the knob and the settings
 * storage in flash.
 *
 * Reconstructed from the archive's machine code and DWARF debug info, keeping
 * the vendor's names (misspellings corrected, see rdr_renames.txt) and source
 * order. Built with ARM GCC 10.3, 96 of the 104 functions compile to exactly
 * the vendor's code; the rest are equivalent. See PARITY.md.
 *
 * Updated to the behaviour of Womier's firmware 0.1.5 (reconstructed from its
 * binary): selectable sleep timeout and debounce, an all-lighting toggle, the
 * Caps Lock indicator on the logo, LED supply cut when nothing is lit, and the
 * 24-byte settings layout.
 */

#include "rdr_common.h"

#if defined(DYNAMIC_KEYMAP_ENABLE)
#    include "dynamic_keymap.h"
#endif

#if defined(VIA_ENABLE)
#    include "via.h"
#endif

// Same value QMK uses privately in tmk_core/protocol/chibios/chibios.c.
#define USB_GETSTATUS_REMOTE_WAKEUP_ENABLED (2U)

/************************Basic variables**************************/
Keyboard_Info_t Keyboard_Info = {
    .Key_Mode     = INIT_WORK_MODE,
    .Ble_Channel  = INIT_BLE_CHANNEL,
    .Batt_Number  = INIT_BATT_NUMBER,
    .Nkro         = INIT_ALL_SIX_KEY,
    .Mac_Win_Mode = INIT_WIN_MAC_MODE,
    .Win_Lock     = INIT_WIN_LOCK_NLOCK,
    .All_Led_Off  = INIT_ALL_LED_ON,
    .Sleep_Time   = INIT_SLEEP_TIME,
    .Rf_Timer_2   = INIT_RF_TIMER_2,
    .Debounce     = INIT_DEBOUNCE,
#if LOGO_LED_ENABLE
    .Logo_On_Off     = INIT_LOGO_ON_OFF,
    .Logo_Mode       = INIT_LOGO_MODE,
    .Logo_Colour     = INIT_LOGO_COLOUR,
    .Logo_Saturation = INIT_LOGO_SATURATION,
    .Logo_Brightness = INIT_LOGO_BRIGHTNESS,
    .Logo_Speed      = INIT_LOGO_SPEED,
#endif
};
Keyboard_Status_t Keyboard_Status = {0};

bool     Key_2p4g_Status        = false;
bool     Key_Ble_1_Status       = false;
bool     Key_Ble_2_Status       = false;
bool     Key_Ble_3_Status       = false;
bool     Key_Fn_Status          = false;
bool     Key_Reset_Status       = false;
bool     Key_Debounce_Status    = false;
bool     Key_Sleep_Time_Status  = false;
bool     Func_Key_Long_Press    = false;
uint8_t  Systick_6ms_Count      = 0;
uint8_t  Systick_10ms_Count     = 0;
uint16_t Systick_Interval_Count = 0;
uint16_t Time_3s_Count          = 0;
uint16_t Func_Time_3s_Count     = 0;

/************************Debounce**************************/
uint8_t Debounce_Time = INIT_DEBOUNCE;

/************************Data queue**************************/
volatile uint8_t app_2g4_data_send = 0;
volatile uint8_t app_2g4_data_rev  = 0;

/**************************EMI****************************/
bool Emi_Test_Start = false;

/**************************SPI****************************/
volatile uint8_t Spi_Send_Recv_Flg    = 0;
uint16_t         Spi_Interval         = SPI_DELAY_RF_TIME;
uint8_t          g_es_spi_rx_buf[64]  = {0};
uint8_t          g_es_spi_tx_buf[64]  = {0};
uint8_t          Repeat_Send_Count    = 0;
uint8_t          Send_Key_Type        = 0;
bool             Init_Spi_Power_Up    = true;
uint8_t          Init_Spi_100ms_Delay = 0;
bool             Ble_Name_Spi_Send    = false;
uint8_t          Ble_Name_Spi_Count   = 1;
bool             Sleep_Time_Spi_Send  = false;
bool             Rf_Timer_2_Spi_Send  = false;
bool             Spi_Sync_Request     = false;
uint8_t          Spi_Ble_Send_Count   = 0;

const uint32_t g_es_dma_ch2pri_cfg = 0xAA008006;
const uint32_t g_es_dma_ch2alt_cfg = 0xC0000007;

const md_spi_inittypedef SPI2_InitStruct = {
    .Mode              = MD_SPI_MODE_MASTER,
    .ClockPhase        = MD_SPI_PHASE_2EDGE,
    .ClockPolarity     = MD_SPI_POLARITY_HIGH,
    .BaudRate          = MD_SPI_BAUDRATEPRESCALER_DIV8,
    .BitOrder          = MD_SPI_MSB_FIRST,
    .TransferDirection = MD_SPI_FULL_DUPLEX,
    .DataWidth         = MD_SPI_FRAME_FORMAT_8BIT,
    .NSS               = MD_SPI_NSS_HARD,
    .CRCCalculation    = MD_SPI_CRCCALCULATION_DISABLE,
    .CRCPoly           = 0,
};

/**************************Mode switching****************************/
volatile host_driver_t *es_qmk_driver = NULL;

/**************************System functions****************************/
bool     Save_Flash                  = false;
bool     Reset_Save_Flash            = false;
uint16_t Save_Flash_3S_Count         = 0;
bool     Led_Rf_Pair_Flg             = false;
bool     Usb_Change_Mode_Wakeup      = false;
uint8_t  Temp_System_Led_Status      = 0xFF;
bool     Mode_Synchronization_Signal = false;
uint16_t g_usb_sof_frame_id          = 0;
uint16_t g_usb_sof_frame_id_last     = 0;
bool     Usb_Dis_Connect             = false;
uint16_t Usb_Suspend_Delay           = 0;
uint16_t Usb_Change_Mode_Delay       = 0;

/************************Battery******************************/
uint16_t User_Adc_Batt[USER_BATT_SCAN_COUNT]  = {0};
uint16_t User_Scan_Batt[USER_BATT_SCAN_COUNT] = {0};
uint8_t  User_Adc_Batt_Count                  = 0;
uint8_t  User_Batt_BaiFen                     = 0;
uint8_t  User_Batt_Old_BaiFen                 = 0;
uint8_t  User_Batt_10ms_Count                 = 0;
uint16_t User_Batt_Time_15S_Count             = 0;
bool     User_Batt_Power_Up                   = false;
bool     User_Batt_Send_Spi                   = false;
uint16_t User_Batt_Power_Up_Delay_100ms_Count = 0;
bool     User_Batt_Power_Up_Delay             = false;
bool     User_Power_Low                       = false;
uint8_t  User_Power_Low_Count                 = 0;
uint8_t  es_stdby_pin_state                   = 0;
bool     User_Key_Batt_Num_Show               = false;
uint8_t  User_Key_Batt_Count                  = 0;
uint8_t  Batt_Led_Count                       = 0;

const md_adc_initial adc_initStruct = {
    .ALIGN            = 0,
    .RSEL             = 3,
    .Regular_Injected = 0,
    .Regular_CM       = 0,
    .Cnt              = 0,
    .CKDIV            = 3,
};

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

/************************Data queue**************************/
uint8_t app_2g4_buffer_full(void) {
    uint8_t tmp_rev = app_2g4_data_rev + 1;
    if (tmp_rev == APP_2G4_BUF_CNT) {
        tmp_rev = 0;
    }
    return (tmp_rev == app_2g4_data_send);
}

uint8_t app_2g4_buffer_empty(void) {
    return (app_2g4_data_rev == app_2g4_data_send);
}

void app_2g4_buffer_rev_add(void) {
    app_2g4_data_rev++;
    if (app_2g4_data_rev >= APP_2G4_BUF_CNT) {
        app_2g4_data_rev = 0;
    }
}

void app_2g4_buffer_send_add(void) {
    app_2g4_data_send++;
    if (app_2g4_data_send >= APP_2G4_BUF_CNT) {
        app_2g4_data_send = 0;
    }
}

/**************************EMI****************************/
void Emi_Init(void) {
    Spi_Interval                         = SPI_DELAY_USB_TIME;
    Keyboard_Status.System_Work_Status   = 0;
    Keyboard_Status.System_Sleep_Mode    = 0;
    Mode_Synchronization_Signal          = false;
    Led_Rf_Pair_Flg                      = false;
    Ble_Name_Spi_Send                    = false;
    Sleep_Time_Spi_Send                  = false;
    Rf_Timer_2_Spi_Send                  = false;
}

void Emi_Read_Data(uint8_t *User_Data, uint8_t User_Length) {
    if (!Emi_Test_Start) {
        return;
    }

    if (Init_Spi_Power_Up) {
        return;
    }

    if ((Spi_Send_Recv_Flg == 0) && (gpio_read_pin(ES_SPI_ACK_IO) == 0)) {
        Spi_Send_Recv_Flg = 1;
        Send_Key_Type     = 1;
        Repeat_Send_Count = 0;

        for (uint8_t i = 0; i < User_Length; i++) {
            g_es_spi_tx_buf[i] = User_Data[i];
        }
        es_spi_send_recv_by_dma(USER_KEYBOARD_LENGTH, g_es_spi_rx_buf, g_es_spi_tx_buf);
    }
}

void Emi_Write_Data(uint8_t *User_Data, uint8_t User_Length) {
#if defined(RAW_ENABLE)
    if (!Emi_Test_Start) {
        return;
    }

    if (User_Data[0] == USER_KEYBOARD_COMMAND) {
        return;
    }

    User_Data[0] = USER_EMI_COMMAND;
    raw_hid_send(User_Data, 32);
#else
    (void)User_Data;
    (void)User_Length;
#endif
}

/**************************SPI****************************/
void es_ble_spi_init(void) {
    md_gpio_inittypedef GPIO_InitStruct;

    md_rcu_enable_spi2(RCU);

    GPIO_InitStruct.Pin        = MD_GPIO_PIN_0;
    GPIO_InitStruct.Mode       = MD_GPIO_MODE_FUNCTION;
    GPIO_InitStruct.OutputType = MD_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull       = MD_GPIO_PULL_UP;
    GPIO_InitStruct.OutDrive   = MD_GPIO_DRIVING_8MA;
    GPIO_InitStruct.Function   = MD_GPIO_AF0;
    md_gpio_init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = MD_GPIO_PIN_1;
    md_gpio_init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = MD_GPIO_PIN_2;
    md_gpio_init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = MD_GPIO_PIN_3;
    md_gpio_init(GPIOC, &GPIO_InitStruct);

    md_spi_init(SPI2, (md_spi_inittypedef *)&SPI2_InitStruct);
}

void es_ble_spi_deinit(void) {
    md_rcu_enable_spi2_reset(RCU);
    md_rcu_disable_spi2_reset(RCU);
    md_rcu_disable_spi2(RCU);

    // PC0..PC3 back to analog, no pull, push-pull, 8 mA, no interrupt.
    GPIOC->MOD |= 0xFF;
    GPIOC->PUD &= ~0xFF;
    GPIOC->OT &= ~0xFF;
    GPIOC->DS &= ~0xFF;
    GPIOC->IST &= ~0xFF;
}

// Despite the name this is a polled transfer: the TX FIFO is kept topped up two
// bytes at a time while received bytes are drained. A tx_buf outside SRAM means
// "clock out zeros".
void es_spi_send_recv_by_dma(uint32_t num, uint8_t *rx_buf, uint8_t *tx_buf) {
    uint32_t tx_index = 0;
    uint32_t rx_index = 0;

    __disable_irq();

    if ((uint32_t)tx_buf >= SRAM_BASE) {
        if (num & 1) {
            SPI2->DATA = tx_buf[tx_index++];
        }

        while (tx_index < num) {
            if (md_spi_get_txfifo_value(SPI2) <= 2) {
                SPI2->DATA = tx_buf[tx_index++];
                SPI2->DATA = tx_buf[tx_index++];
            }

            if (md_spi_get_rxfifo_value(SPI2)) {
                rx_buf[rx_index++] = SPI2->DATA;
            }
        }

        while (rx_index < num) {
            if (md_spi_get_rxfifo_value(SPI2)) {
                rx_buf[rx_index++] = SPI2->DATA;
            }
        }
    } else {
        if (num & 1) {
            SPI2->DATA = 0;
        }

        while (tx_index < num) {
            if (md_spi_get_txfifo_value(SPI2) <= 2) {
                SPI2->DATA = 0;
                SPI2->DATA = 0;
                tx_index += 2;
            }

            if (md_spi_get_rxfifo_value(SPI2)) {
                rx_buf[rx_index++] = SPI2->DATA;
            }
        }

        while (num > rx_index) {
            if (md_spi_get_rxfifo_value(SPI2)) {
                rx_buf[rx_index++] = SPI2->DATA;
            }
        }
    }

    __enable_irq();
}

void Spi_Main_Loop(void) {
    if ((Init_Spi_Power_Up == false) && (Keyboard_Status.System_Work_Status == 0)) {
        if ((app_2g4_buffer_empty() == 0) && (Spi_Send_Recv_Flg == 0) && (gpio_read_pin(ES_SPI_ACK_IO) == 0)) {
            Spi_Send_Recv_Flg = 1;
            Send_Key_Type     = 0;
            es_spi_send_recv_by_dma(USER_KEYBOARD_LENGTH, g_es_spi_rx_buf, app_2g4_data[app_2g4_data_send]);
            app_2g4_buffer_send_add();
        }
    }
}

void Spi_Send_Command(uint8_t Command) {
    if ((Init_Spi_Power_Up == false) && (Keyboard_Status.System_Work_Status == 0)) {
        uint16_t Time_Delay = 2000;

        while ((Spi_Send_Recv_Flg != 0) || (gpio_read_pin(ES_SPI_ACK_IO) != 0)) {
            if (--Time_Delay == 0) {
                return;
            }
        }

        Spi_Send_Recv_Flg = 1;
        Send_Key_Type     = 0;
        Spi_Interval      = SPI_DELAY_RF_TIME;

        g_es_spi_tx_buf[0] = USER_KEYBOARD_COMMAND;
        g_es_spi_tx_buf[1] = USER_KEYBOARD_LENGTH;
        g_es_spi_tx_buf[2] = Command;
        if (Command == USER_BATTERY_DATA) {
            g_es_spi_tx_buf[3] = Keyboard_Info.Batt_Number;
        } else if (Command == USER_BLE1_WRITE_NAME) {
            g_es_spi_tx_buf[3] = USER_BLE_ID >> 8;
            g_es_spi_tx_buf[4] = USER_BLE_ID & 0xFF;
            g_es_spi_tx_buf[5] = strlen(USER_BLE1_NAME);
            uint8_t len        = strlen(USER_BLE1_NAME);
            memcpy(&g_es_spi_tx_buf[6], USER_BLE1_NAME, len);
        } else if (Command == USER_BLE2_WRITE_NAME) {
            g_es_spi_tx_buf[3] = USER_BLE_ID >> 8;
            g_es_spi_tx_buf[4] = USER_BLE_ID & 0xFF;
            g_es_spi_tx_buf[5] = strlen(USER_BLE2_NAME);
            uint8_t len        = strlen(USER_BLE2_NAME);
            memcpy(&g_es_spi_tx_buf[6], USER_BLE2_NAME, len);
        } else if (Command == USER_BLE3_WRITE_NAME) {
            g_es_spi_tx_buf[3] = USER_BLE_ID >> 8;
            g_es_spi_tx_buf[4] = USER_BLE_ID & 0xFF;
            g_es_spi_tx_buf[5] = strlen(USER_BLE3_NAME);
            uint8_t len        = strlen(USER_BLE3_NAME);
            memcpy(&g_es_spi_tx_buf[6], USER_BLE3_NAME, len);
        } else if ((Command == USER_SLEEP_TIME_DATA) || (Command == USER_RF_TIMER_2_DATA)) {
            uint32_t value     = (Command == USER_SLEEP_TIME_DATA) ? Keyboard_Info.Sleep_Time : Keyboard_Info.Rf_Timer_2;
            g_es_spi_tx_buf[3] = value >> 24;
            g_es_spi_tx_buf[4] = value >> 16;
            g_es_spi_tx_buf[5] = value >> 8;
            g_es_spi_tx_buf[6] = value;
        }

        es_spi_send_recv_by_dma(USER_KEYBOARD_LENGTH, g_es_spi_rx_buf, g_es_spi_tx_buf);
    }
}

uint8_t Spi_Ack_Send_Command(uint8_t Command) {
    if (Init_Spi_Power_Up) {
        return SPI_NACK;
    }

    if ((Spi_Send_Recv_Flg == 0) && (gpio_read_pin(ES_SPI_ACK_IO) == 0)) {
        Spi_Send_Recv_Flg = 1;
        Send_Key_Type     = 1;
        Repeat_Send_Count = 0;

        g_es_spi_tx_buf[0] = USER_KEYBOARD_COMMAND;
        g_es_spi_tx_buf[1] = USER_KEYBOARD_LENGTH;
        g_es_spi_tx_buf[2] = Command;
        es_spi_send_recv_by_dma(USER_KEYBOARD_LENGTH, g_es_spi_rx_buf, g_es_spi_tx_buf);
        return SPI_ACK;
    }

    return SPI_NACK;
}

void Get_Spi_Return_Data(uint8_t *Data) {
    if (Emi_Test_Start) {
        Emi_Write_Data(Data, USER_KEYBOARD_LENGTH);
        return;
    }

    if (Data[2] == USER_GET_RF_STATUS) {
        Keyboard_Status.System_Work_Status = Data[3];
        if (Keyboard_Status.System_Work_Status) {
            if (Keyboard_Info.Key_Mode != QMK_USB_MODE) {
                if (Usb_Change_Mode_Wakeup) {
                    User_Sleep();
                } else {
                    Keyboard_Status.System_Work_Status = 0;
                }
            } else {
                Keyboard_Status.System_Work_Status = 0;
                Spi_Interval                       = SPI_DELAY_USB_TIME;
            }
        }

        Keyboard_Status.System_Work_Mode = Data[4];
        if (Keyboard_Status.System_Work_Mode != Keyboard_Info.Key_Mode) {
            Mode_Synchronization_Signal = true;
            if (Keyboard_Status.System_Work_Status) {
                Keyboard_Status.System_Work_Status = 0;
            }
        } else if (Keyboard_Info.Key_Mode == QMK_USB_MODE) {
            Spi_Interval = SPI_DELAY_USB_TIME;
        }

        Keyboard_Status.System_Work_Channel = Data[5];
        if ((Keyboard_Status.System_Work_Channel != Keyboard_Info.Ble_Channel) && (Keyboard_Info.Key_Mode == QMK_BLE_MODE)) {
            Mode_Synchronization_Signal = true;
            if (Keyboard_Status.System_Work_Status) {
                Keyboard_Status.System_Work_Status = 0;
            }
        }

        Keyboard_Status.System_Connect_Status = Data[6];
        if (Temp_System_Led_Status != Keyboard_Status.System_Connect_Status) {
            Temp_System_Led_Status = Keyboard_Status.System_Connect_Status;
            if (Keyboard_Info.Key_Mode != QMK_USB_MODE) {
                Led_Rf_Pair_Flg = true;
            } else {
                Led_Rf_Pair_Flg = false;
            }
        }

        Keyboard_Status.System_Led_Status = Data[7];

        uint16_t Ble_ID = (Data[8] << 8) | Data[9];
        if (Ble_ID != USER_BLE_ID) {
            if (Ble_Name_Spi_Send == false) {
                Ble_Name_Spi_Send  = true;
                Ble_Name_Spi_Count = 1;
            }
        }

        // The radio reports its copies of the two timers; resend ours if they differ.
        uint32_t Rf_Sleep_Time = ((uint32_t)Data[11] << 24) | ((uint32_t)Data[12] << 16) | ((uint32_t)Data[13] << 8) | Data[14];
        if (Rf_Sleep_Time != Keyboard_Info.Sleep_Time) {
            Sleep_Time_Spi_Send = true;
        }

        uint32_t Rf_Timer_2 = ((uint32_t)Data[15] << 24) | ((uint32_t)Data[16] << 16) | ((uint32_t)Data[17] << 8) | Data[18];
        if (Rf_Timer_2 != Keyboard_Info.Rf_Timer_2) {
            Rf_Timer_2_Spi_Send = true;
        }
    } else if (Data[2] == USER_KEYBOARD_SLEEP) {
        if (Keyboard_Status.System_Work_Status && (Data[3] == USER_SLEEP_PASS)) {
            if (Keyboard_Info.Key_Mode != QMK_USB_MODE) {
                Keyboard_Status.System_Sleep_Mode = 1;
            } else {
                Keyboard_Status.System_Sleep_Mode = 0;
            }
        } else if (Data[3] == USER_SLEEP_FAIL) {
            Keyboard_Status.System_Work_Status = 0;
            Keyboard_Status.System_Sleep_Mode  = 0;
        }
    }

    if (Data[10] == USER_EMI_COMMAND) {
        Emi_Test_Start = true;
        Emi_Init();
    }
}

/**************************Mode switching****************************/
uint8_t es_keyboard_leds(void) {
    if (es_qmk_driver != NULL) {
        return es_qmk_driver->keyboard_leds();
    }

    return 0;
}

void es_send_keyboard(report_keyboard_t *report) {
    if ((Keyboard_Info.Key_Mode == QMK_BLE_MODE) || (Keyboard_Info.Key_Mode == QMK_2P4G_MODE)) {
        User_bluetooth_send_keyboard((uint8_t *)report, 8);
    }

    if (es_qmk_driver != NULL) {
        es_qmk_driver->send_keyboard(report);
    }
}

void es_send_nkro(report_nkro_t *report) {
    if ((Keyboard_Info.Key_Mode == QMK_BLE_MODE) || (Keyboard_Info.Key_Mode == QMK_2P4G_MODE)) {
        User_bluetooth_send_keyboard((uint8_t *)report, 32);
    }

    if (es_qmk_driver != NULL) {
        es_qmk_driver->send_nkro(report);
    }
}

void es_send_mouse(report_mouse_t *report) {
    if ((Keyboard_Info.Key_Mode == QMK_BLE_MODE) || (Keyboard_Info.Key_Mode == QMK_2P4G_MODE)) {
        User_bluetooth_send_keyboard((uint8_t *)report, 6);
    }

    if (es_qmk_driver != NULL) {
        es_qmk_driver->send_mouse(report);
    }
}

void es_send_extra(report_extra_t *report) {
    if ((Keyboard_Info.Key_Mode == QMK_BLE_MODE) || (Keyboard_Info.Key_Mode == QMK_2P4G_MODE)) {
        User_bluetooth_send_keyboard((uint8_t *)report, 3);
    }

    if (es_qmk_driver != NULL) {
        es_qmk_driver->send_extra(report);
    }
}

const host_driver_t es_user_driver = {
    .keyboard_leds = es_keyboard_leds,
    .send_keyboard = es_send_keyboard,
    .send_nkro     = es_send_nkro,
    .send_mouse    = es_send_mouse,
    .send_extra    = es_send_extra,
};

void Mode_Synchronization(void) {
    switch (Keyboard_Info.Key_Mode) {
        case QMK_BLE_MODE:
            switch (Keyboard_Info.Ble_Channel) {
                case QMK_BLE_CHANNEL_1:
                    Spi_Send_Command(USER_SWITCH_BLE_1_MODE);
                    break;
                case QMK_BLE_CHANNEL_2:
                    Spi_Send_Command(USER_SWITCH_BLE_2_MODE);
                    break;
                case QMK_BLE_CHANNEL_3:
                    Spi_Send_Command(USER_SWITCH_BLE_3_MODE);
                    break;
            }
            break;
        case QMK_2P4G_MODE:
            Spi_Send_Command(USER_SWITCH_2P4G_MODE);
            break;
        case QMK_USB_MODE:
            Spi_Send_Command(USER_SWITCH_USB_MODE);
            break;
    }
}

void Ble_Name_Synchronization(void) {
    switch (Ble_Name_Spi_Count) {
        case 1: Spi_Send_Command(USER_BLE1_WRITE_NAME); Ble_Name_Spi_Count++; break;
        case 2: Spi_Send_Command(USER_BLE2_WRITE_NAME); Ble_Name_Spi_Count++; break;
        case 3: Spi_Send_Command(USER_BLE3_WRITE_NAME); Ble_Name_Spi_Count++; break;
    }

    if (Ble_Name_Spi_Count > 3) {
        Ble_Name_Spi_Count = 1;
        Ble_Name_Spi_Send  = false;
    }
}

// Pending radio updates, run from the main loop every 10 ms (Vector78 sets
// Spi_Sync_Request) rather than from the interrupt.
void Spi_Synchronization(void) {
    if (Spi_Sync_Request == false) {
        return;
    }
    Spi_Sync_Request = false;

    if (Mode_Synchronization_Signal) {
        Mode_Synchronization_Signal = false;
        Mode_Synchronization();
    }

    if (Ble_Name_Spi_Send) {
        Ble_Name_Synchronization();
    }

    if (Sleep_Time_Spi_Send) {
        Spi_Send_Command(USER_SLEEP_TIME_DATA);
        Sleep_Time_Spi_Send = false;
    }

    if (Rf_Timer_2_Spi_Send) {
        Spi_Send_Command(USER_RF_TIMER_2_DATA);
        Rf_Timer_2_Spi_Send = false;
    }
}

// Queues a HID report for the radio: [command, length, report type, report...].
void User_bluetooth_send_keyboard(uint8_t *report, uint32_t len) {
    if (app_2g4_buffer_full()) {
        return;
    }

    if (len > (USER_KEYBOARD_LENGTH - 3)) {
        len = USER_KEYBOARD_LENGTH - 3;
    }

    uint8_t Temp_Tx_Buf[USER_KEYBOARD_LENGTH] = {0};

    Temp_Tx_Buf[0] = USER_KEYBOARD_COMMAND;
    Temp_Tx_Buf[1] = USER_KEYBOARD_LENGTH;

    if (len == USER_KEY_BYTE_LENGTH) {
        memcpy(&Temp_Tx_Buf[3], report, len);
        Temp_Tx_Buf[2] = USER_KEY_BYTE_DATA;
    } else {
        switch (report[0]) {
            case KB_REPORT_ID:
                memcpy(&Temp_Tx_Buf[3], report, len);
                Temp_Tx_Buf[2] = USER_KEY_BIT_DATA;
                break;
            case SYS_REPORT_ID:
                memcpy(&Temp_Tx_Buf[3], report, len);
                Temp_Tx_Buf[2] = USER_SYSTEM_DATA;
                break;
            case CON_REPORT_ID:
                memcpy(&Temp_Tx_Buf[3], report, len);
                Temp_Tx_Buf[2] = USER_CONSUMER_DATA;
                break;
            case MOUSE_REPORT_ID:
                memcpy(&Temp_Tx_Buf[3], report, len);
                Temp_Tx_Buf[2] = USER_MOUSE_DATA;
                break;
        }
    }

    memcpy(app_2g4_data[app_2g4_data_rev], Temp_Tx_Buf, USER_KEYBOARD_LENGTH);
    app_2g4_buffer_rev_add();
}

/**************************System functions****************************/
void es_mcu_reset(void) {
    mcu_reset();
}

// Waits (bounded) for any SPI exchange or pending settings save to finish, then
// remaps main flash to 0 and resets into the bootloader.
void bootloader_jump(void) {
    uint16_t Time_Delay = 36000;

    while ((Spi_Send_Recv_Flg != 0) || (gpio_read_pin(ES_SPI_ACK_IO) != 0) || Reset_Save_Flash) {
        if (--Time_Delay == 0) {
            break;
        }
    }

    gpio_write_pin_low(ES_LED_POWER_IO);

    md_fc_lock();
    md_syscfg_set_memory_mapping(SYSCFG, MD_SYSCFG_MEMMOD_MAIN);
    md_syscfg_set_flash_remap_base(SYSCFG, 0);
    md_syscfg_enable_memory_remap(SYSCFG);
    NVIC_SystemReset();
}

void mcu_reset(void) {
    uint16_t Time_Delay = 36000;

    while ((Spi_Send_Recv_Flg != 0) || (gpio_read_pin(ES_SPI_ACK_IO) != 0) || Reset_Save_Flash) {
        if (--Time_Delay == 0) {
            break;
        }
    }

    gpio_write_pin_low(ES_LED_POWER_IO);

    NVIC_SystemReset();
}

void User_Keyboard_Reset(void) {
    Keyboard_Info.Ble_Channel  = INIT_BLE_CHANNEL;
    Keyboard_Info.Nkro         = INIT_ALL_SIX_KEY;
    Keyboard_Info.Mac_Win_Mode = INIT_WIN_MAC_MODE;
    Keyboard_Info.Win_Lock     = INIT_WIN_LOCK_NLOCK;
    Keyboard_Info.All_Led_Off  = INIT_ALL_LED_ON;
    Keyboard_Info.Sleep_Time   = INIT_SLEEP_TIME;
    Keyboard_Info.Rf_Timer_2   = INIT_RF_TIMER_2;
    Keyboard_Info.Debounce     = INIT_DEBOUNCE;
#if LOGO_LED_ENABLE
    Keyboard_Info.Logo_On_Off     = INIT_LOGO_ON_OFF;
    Keyboard_Info.Logo_Mode       = INIT_LOGO_MODE;
    Keyboard_Info.Logo_Colour     = INIT_LOGO_COLOUR;
    Keyboard_Info.Logo_Saturation = INIT_LOGO_SATURATION;
    Keyboard_Info.Logo_Brightness = INIT_LOGO_BRIGHTNESS;
    Keyboard_Info.Logo_Speed      = INIT_LOGO_SPEED;
    Logo_Init();
#endif
#if SIDE_LED_ENABLE
    Keyboard_Info.Side_On_Off     = INIT_SIDE_ON_OFF;
    Keyboard_Info.Side_Mode       = INIT_SIDE_MODE;
    Keyboard_Info.Side_Colour     = INIT_SIDE_COLOUR;
    Keyboard_Info.Side_Saturation = INIT_SIDE_SATURATION;
    Keyboard_Info.Side_Brightness = INIT_SIDE_BRIGHTNESS;
    Keyboard_Info.Side_Speed      = INIT_SIDE_SPEED;
    Side_Init();
#endif
    Reset_Save_Flash = true;
    eeprom_write_block_user(&Keyboard_Info, 0, sizeof(Keyboard_Info_t));
    Reset_Save_Flash = false;
    Debounce_Time    = Keyboard_Info.Debounce;

    eeconfig_disable();
    soft_reset_keyboard();
}

// 5 ms <-> 2 ms; the logo then flashes green (5 ms) or red (2 ms).
void User_Debounce_Toggle(void) {
    Keyboard_Info.Debounce = (Keyboard_Info.Debounce == DEBOUNCE_FAST) ? DEBOUNCE_SLOW : DEBOUNCE_FAST;
    Debounce_Time          = Keyboard_Info.Debounce;
    Debounce_Point_Count   = 3;
    Save_Flash_Set();
}

// 1 -> 3 -> 10 -> 30 -> 1 min; the logo then flashes red, green, blue or white.
void User_Sleep_Time_Next(void) {
    switch (Keyboard_Info.Sleep_Time) {
        case SLEEP_TIME_1MIN:  Keyboard_Info.Sleep_Time = SLEEP_TIME_3MIN;  break;
        case SLEEP_TIME_3MIN:  Keyboard_Info.Sleep_Time = SLEEP_TIME_10MIN; break;
        case SLEEP_TIME_10MIN: Keyboard_Info.Sleep_Time = SLEEP_TIME_30MIN; break;
        case SLEEP_TIME_30MIN: Keyboard_Info.Sleep_Time = SLEEP_TIME_1MIN;  break;
        default:               Keyboard_Info.Sleep_Time = SLEEP_TIME_3MIN;  break;
    }

    Spi_Send_Command(USER_SLEEP_TIME_DATA);
    Sleep_Time_Spi_Send    = false;
    Sleep_Time_Point_Count = 3;
    Systick_Led_Count      = 0;
    Save_Flash_Set();
}

// Runs the action of a function key held for 3 s (see Vector78).
void User_Func_Key_Long_Press(void) {
    if (Func_Key_Long_Press == false) {
        return;
    }
    Func_Key_Long_Press = false;

    if (Key_Reset_Status) {
        Key_Reset_Status = false;
        User_Keyboard_Reset();
    } else if (Key_Debounce_Status) {
        Key_Debounce_Status = false;
        User_Debounce_Toggle();
    } else if (Key_Sleep_Time_Status) {
        Key_Sleep_Time_Status = false;
        User_Sleep_Time_Next();
    }
}

void Save_Flash_Set(void) {
    Save_Flash          = true;
    Save_Flash_3S_Count = 0;
}

// BS16T1: 48 MHz / 48 = 1 MHz, reload 2000 -> 2 ms tick (Vector78).
void User_Systime_Init(void) {
    md_rcu_enable_bs16t1(RCU);
    BS16T1->PRES = 47;
    BS16T1->AR   = 2000;
    BS16T1->IER  = 1;
    BS16T1->CON1 = 1;
    NVIC_SetPriority(BS16T1_IRQn, 2);
    NVIC_EnableIRQ(BS16T1_IRQn);
}

void User_Systime_Deinit(void) {
    md_rcu_enable_bs16t1_reset(RCU);
    md_rcu_disable_bs16t1_reset(RCU);
    md_rcu_disable_bs16t1(RCU);
}

void User_Sleep(void) {
    gpio_set_pin_output(ES_LED_POWER_IO);
    gpio_write_pin_low(ES_LED_POWER_IO);
    gpio_set_pin_output(ES_SDB_POWER_IO);
    gpio_write_pin_low(ES_SDB_POWER_IO);
    gpio_set_pin_output(ES_WAKEUP_IO);
    gpio_write_pin_low(ES_WAKEUP_IO);
}

void User_Wakeup(void) {
    gpio_set_pin_output(ES_WAKEUP_IO);
    gpio_write_pin_high(ES_WAKEUP_IO);
}

void Init_Gpio_Information(void) {
    gpio_write_pin_low(ES_LED_POWER_IO);
    gpio_set_pin_output(ES_LED_POWER_IO);
    gpio_write_pin_low(ES_LED_POWER_IO);

    gpio_write_pin_high(ES_SDB_POWER_IO);
    gpio_set_pin_output(ES_SDB_POWER_IO);
    gpio_write_pin_high(ES_SDB_POWER_IO);

    gpio_write_pin_high(ES_WAKEUP_IO);
    gpio_set_pin_output(ES_WAKEUP_IO);
    gpio_write_pin_high(ES_WAKEUP_IO);

    gpio_set_pin_input_high(ES_BATT_STDBY_IO);

    gpio_set_pin_input(ES_USB_POWER_IO);

    gpio_set_pin_input_high(WHEEL_ZA_IO);
    gpio_set_pin_input_high(WHEEL_ZB_IO);

    // The radio's ACK line (PA4) raises an interrupt on both edges.
    md_gpio_inittypedef gpiox;

    gpiox.Pin        = MD_GPIO_PIN_4;
    gpiox.Mode       = MD_GPIO_MODE_INPUT;
    gpiox.OutputType = MD_GPIO_OUTPUT_PUSHPULL;
    gpiox.Pull       = MD_GPIO_PULL_DOWN;
    gpiox.OutDrive   = MD_GPIO_DRIVING_8MA;
    gpiox.Function   = MD_GPIO_AF0;
    md_gpio_init(GPIOA, &gpiox);

    md_exti_set_interrupt_pin_0_7(EXTI, MD_EXTI_GPIOA4);
    md_exti_enable_it_gpio_pin(EXTI, MD_EXTI_GPIO4);
    md_exti_enable_rising_edge_trigger(EXTI, MD_EXTI_GPIO4);
    md_exti_enable_falling_edge_trigger(EXTI, MD_EXTI_GPIO4);
    NVIC_SetPriority(EXTI_4to15_IRQn, 3);
    NVIC_EnableIRQ(EXTI_4to15_IRQn);
}

void Board_Wakeup_Init(void) {
    md_rcu_enable_csu(RCU);
    CSU->CON |= CSU_CON_AUTOEN;
    CSU->CON |= CSU_CON_CNTEN;

    es_ble_spi_init();

    User_Adc_Init();

    rgb_matrix_driver_init();

    Init_Gpio_Information();

    User_Systime_Init();

    if (Keyboard_Info.Key_Mode == QMK_USB_MODE) {
        User_Usb_Init();
    } else {
        Usb_Disconnect();
    }

    Init_Spi_Power_Up    = true;
    Init_Spi_100ms_Delay = 0;
    Spi_Interval         = SPI_DELAY_RF_TIME;

    NVIC_SetPriority(PendSV_IRQn, 3);
    NVIC_SetPriority(SysTick_IRQn, 3);

    Keyboard_Status.System_Work_Status = 0;
    Keyboard_Status.System_Sleep_Mode  = 0;
    Usb_Change_Mode_Wakeup             = false;

    Init_Batt_Information();

    Led_Power_Up   = false;
    Emi_Test_Start = false;

#if LOGO_LED_ENABLE
    Logo_Init();
#endif
}

// Deep sleep. Entered when the USB host has suspended the bus (no new SOF for
// 800 ticks) or, in wireless mode, when the radio reports it is sleeping. The
// matrix is parked with columns low and rows pulled up so any key (or the knob,
// or USB power changing) wakes the MCU through EXTI. Row B5 shares EXTI line 5
// with the USB power detect on C5, so it wakes through GP16C4T2 input capture.
void es_chibios_user_idle_loop_hook(void) {
    uint32_t i;

    if (Keyboard_Info.Key_Mode == QMK_USB_MODE) {
        if (Usb_Dis_Connect == false) {
            return;
        }
        Usb_Dis_Connect = false;

        if ((g_usb_sof_frame_id_last != g_usb_sof_frame_id) || (Usb_Change_Mode_Wakeup == false)) {
            g_usb_sof_frame_id_last = g_usb_sof_frame_id;
            Usb_Suspend_Delay       = 0;
            return;
        }

        Usb_If_Ok_Led = false;
        Usb_Suspend_Delay++;
        if (Usb_Suspend_Delay < 800) {
            return;
        }
        Usb_Suspend_Delay = 0;

        while (DMA1->CHENSET & (1 << 2)) {
        }

        User_Sleep();

        {
            uint32_t delay = 3600;
            while (Spi_Ack_Send_Command(USER_KEYBOARD_SLEEP) != SPI_ACK) {
                if (--delay == 0) {
                    User_Wakeup();
                    Usb_Suspend_Delay = 600;
                    return;
                }
            }
        }

        i = 36000;
        while (Spi_Send_Recv_Flg) {
            if (--i == 0) {
                User_Wakeup();
                return;
            }
        }
    } else if (Keyboard_Status.System_Sleep_Mode == 0) {
        return;
    }

    if (Spi_Send_Recv_Flg) {
        return;
    }

    uint32_t delay = 100;
    while (gpio_read_pin(ES_SPI_ACK_IO) == 0) {
        wait_ms(1);
        if (--delay == 0) {
            return;
        }
    }

    gpio_set_pin_input_high(ES_SPI_ACK_IO);

    md_tick_disable_csr_tickie(TICK);

    User_Systime_Deinit();

    es_ble_spi_deinit();

    User_Pwm_Deinit();

    User_Usb_Deinit();

    User_Adc_Deinit();

    Save_Flash             = false;
    Save_Flash_3S_Count    = 0;
    Usb_Change_Mode_Wakeup = false;
    Usb_Change_Mode_Delay  = 0;
    Led_Power_Up           = false;

    ioline_t User_Pin_Tab_Col[KEYBOARD_COL] = MATRIX_USER_COL_PINS;
    ioline_t User_Pin_Tab_Rol[KEYBOARD_ROW] = MATRIX_USER_ROW_PINS;

    for (i = 0; i < KEYBOARD_COL; i++) {
        gpio_set_pin_output(User_Pin_Tab_Col[i]);
        gpio_write_pin_low(User_Pin_Tab_Col[i]);
    }

    for (i = 0; i < KEYBOARD_ROW; i++) {
        gpio_set_pin_input_high(User_Pin_Tab_Rol[i]);
    }

    gpio_set_pin_input(WHEEL_ZA_IO);
    gpio_set_pin_input(WHEEL_ZB_IO);

    gpio_set_pin_input(ES_USB_POWER_IO);

    // EXTI lines: rows 0,1,3,4,6,7 (+ line 5 = USB power), knob 2 and 10. Each
    // line arms the edge that moves it away from its current level.
    uint32_t Gpio_Enable   = 0x4FF;
    uint32_t Gpio_Status_H = 0;
    uint32_t Gpio_Status_L = 0xDB;

    if (gpio_read_pin(ES_USB_POWER_IO)) {
        Gpio_Status_L |= 0x20;
    } else {
        Gpio_Status_H |= 0x20;
    }

    if (gpio_read_pin(WHEEL_ZA_IO)) {
        Gpio_Status_L |= 0x04;
    } else {
        Gpio_Status_H |= 0x04;
    }

    if (gpio_read_pin(WHEEL_ZB_IO)) {
        Gpio_Status_L |= 0x400;
    } else {
        Gpio_Status_H |= 0x400;
    }

    // Row B5 -> AF3 (GP16C4T2 CH2).
    GPIOB->MOD &= ~(3 << 10);
    GPIOB->MOD |= (2 << 10);

    GPIOB->AFL &= ~(0xF << 20);
    GPIOB->AFL |= (3 << 20);

    md_exti_set_interrupt_pin_0_7(EXTI, 0x11211111);
    md_exti_set_interrupt_pin_8_15(EXTI, 0x00000100);
    md_exti_enable_it_gpio_pin(EXTI, Gpio_Enable);
    md_exti_enable_rising_edge_trigger(EXTI, Gpio_Status_H);
    md_exti_enable_falling_edge_trigger(EXTI, Gpio_Status_L);
    NVIC_EnableIRQ(EXTI_0to1_IRQn);
    NVIC_EnableIRQ(EXTI_2to3_IRQn);
    NVIC_EnableIRQ(EXTI_4to15_IRQn);

    md_rcu_enable_gp16c4t2(RCU);
    md_timer_set_prescaler_value_pscv(GP16C4T2, 1);
    md_timer_set_auto_reload_value_arrv(GP16C4T2, 0xFFFF);
    md_timer_set_cc2_func_cc2ssel(GP16C4T2, MD_TIMER_CHMODE_INPUT_DIRECT);
    md_timer_set_cc2_input_edge_cc2pol(GP16C4T2, 1); // falling edge
    md_timer_enable_cc2_output_cc2en(GP16C4T2);
    md_timer_enable_counter_cnten(GP16C4T2);
    md_timer_enable_it_ch2(GP16C4T2);
    NVIC_EnableIRQ(GP16C4T2_IRQn);
    md_rcu_enable_gp16c4t2_in_sleep_mode(RCU);

    uint8_t Usb_Sleep_Status = 0xAA;
    if ((gpio_read_pin(ES_USB_POWER_IO) == 0) && (Keyboard_Info.Key_Mode == QMK_USB_MODE)) {
        NVIC_DisableIRQ(USB_IRQn);
        Usb_Sleep_Status = 0xFF;
    }

    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    md_rcu_set_system_clock_source(RCU, MD_RCU_SW_SYSCLK_HRC);
    md_rcu_disable_pll0(RCU);

    if (Keyboard_Info.Key_Mode != QMK_USB_MODE) {
        md_rcu_disable_usb(RCU);
        md_rcu_disable_hrc48(RCU);

        __WFI();

        md_rcu_enable_hrc48(RCU);

        for (i = 0; i < 99999; i++) {
            if (md_rcu_is_active_flag_hrc48_ready(RCU)) {
                break;
            }
        }

        md_rcu_enable_usb(RCU);
    } else {
        md_rcu_enable_usb_in_sleep_mode(RCU);

        __WFI();
    }

    md_rcu_set_system_clock_source(RCU, MD_RCU_SW_SYSCLK_HRC48);

    if ((Keyboard_Info.Key_Mode == QMK_USB_MODE) && (Usb_Sleep_Status == 0xFF)) {
        NVIC_EnableIRQ(USB_IRQn);
    }

    // Which rows are held low, i.e. which key woke us.
    uint8_t Sleep_Status = 0;
    for (i = 0; i < KEYBOARD_ROW; i++) {
        if (gpio_read_pin(User_Pin_Tab_Rol[i]) == 0) {
            Sleep_Status |= (1 << i);
        }
    }

    NVIC_DisableIRQ(EXTI_0to1_IRQn);
    NVIC_DisableIRQ(EXTI_2to3_IRQn);
    NVIC_DisableIRQ(EXTI_4to15_IRQn);
    NVIC_DisableIRQ(GP16C4T2_IRQn);

    for (i = 0; i < KEYBOARD_COL; i++) {
        gpio_write_pin_high(User_Pin_Tab_Col[i]);
    }

    // 0xFF: no key found (0.1.2 then replayed the key at 0/0, Esc).
    uint8_t Rol_Count = 0xFF, Col_Count = 0xFF;
    for (i = 0; i < KEYBOARD_ROW; i++) {
        if (Sleep_Status & (1 << i)) {
            for (uint8_t j = 0; j < KEYBOARD_COL; j++) {
                gpio_write_pin_low(User_Pin_Tab_Col[j]);

                if (gpio_read_pin(User_Pin_Tab_Rol[i]) == 0) {
                    Rol_Count = i;
                    Col_Count = j;
                    break;
                }

                for (uint8_t k = 0; k < KEYBOARD_COL; k++) {
                    gpio_write_pin_high(User_Pin_Tab_Col[j]);
                }
            }
        }
    }

    for (i = 0; i < KEYBOARD_COL; i++) {
        gpio_write_pin_high(User_Pin_Tab_Col[i]);
    }

    gpio_set_pin_input_low(ES_SPI_ACK_IO);

    md_tick_enable_csr_tickie(TICK);

    // In USB mode, wake the host and replay the key that woke the keyboard.
    if (Keyboard_Info.Key_Mode == QMK_USB_MODE) {
        if (USBD1.status & USB_GETSTATUS_REMOTE_WAKEUP_ENABLED) {
            usb_lld_wakeup_host(&USBD1);

            uint16_t cnt = 200;
            while (cnt--) {
                if (USBD1.state != USB_ACTIVE) {
                    wait_ms(10);
                }
            }
        }

        if ((Rol_Count != 0xFF) && (Col_Count != 0xFF)) {
            uint16_t wake_keycode;
#if defined(DYNAMIC_KEYMAP_ENABLE)
            wake_keycode = dynamic_keymap_get_keycode(0, Rol_Count, Col_Count);
#else
            wake_keycode = keymap_key_to_keycode(0, (keypos_t){.row = Rol_Count, .col = Col_Count});
#endif
            register_code(wake_keycode);
            wait_ms(2);
            unregister_code(wake_keycode);
            wait_ms(2);
        }
    }

    Board_Wakeup_Init();
}

// Loads the settings from flash. Blank (all 0xFF) or zeroed flash gets the
// defaults; otherwise each field is clamped into range.
void Init_Keyboard_Information(void) {
    eeprom_read_block_user(&Keyboard_Info, 0, sizeof(Keyboard_Info_t));

    bool Blank = (Keyboard_Info.Key_Mode == 0xFF) && (Keyboard_Info.Ble_Channel == 0xFF) && (Keyboard_Info.Batt_Number == 0xFF) && (Keyboard_Info.Nkro == 0xFF) && (Keyboard_Info.Mac_Win_Mode == 0xFF) && (Keyboard_Info.Win_Lock == 0xFF) && (Keyboard_Info.Sleep_Time == 0xFFFFFFFF) && (Keyboard_Info.Rf_Timer_2 == 0xFFFFFFFF) && (Keyboard_Info.Debounce == 0xFF);
    bool Zero  = (Keyboard_Info.Key_Mode == 0) && (Keyboard_Info.Ble_Channel == 0) && (Keyboard_Info.Batt_Number == 0) && (Keyboard_Info.Nkro == 0) && (Keyboard_Info.Mac_Win_Mode == 0) && (Keyboard_Info.Win_Lock == 0) && (Keyboard_Info.Sleep_Time == 0) && (Keyboard_Info.Rf_Timer_2 == 0) && (Keyboard_Info.Debounce == 0);

    if (Blank || Zero) {
        Keyboard_Info.Key_Mode     = INIT_WORK_MODE;
        Keyboard_Info.Ble_Channel  = INIT_BLE_CHANNEL;
        Keyboard_Info.Batt_Number  = INIT_BATT_NUMBER;
        Keyboard_Info.Nkro         = INIT_ALL_SIX_KEY;
        Keyboard_Info.Mac_Win_Mode = INIT_WIN_MAC_MODE;
        Keyboard_Info.Win_Lock     = INIT_WIN_LOCK_NLOCK;
        Keyboard_Info.All_Led_Off  = INIT_ALL_LED_ON;
        Keyboard_Info.Sleep_Time   = INIT_SLEEP_TIME;
        Keyboard_Info.Rf_Timer_2   = INIT_RF_TIMER_2;
        Keyboard_Info.Debounce     = INIT_DEBOUNCE;
#if LOGO_LED_ENABLE
        Keyboard_Info.Logo_On_Off     = INIT_LOGO_ON_OFF;
        Keyboard_Info.Logo_Mode       = INIT_LOGO_MODE;
        Keyboard_Info.Logo_Colour     = INIT_LOGO_COLOUR;
        Keyboard_Info.Logo_Saturation = INIT_LOGO_SATURATION;
        Keyboard_Info.Logo_Brightness = INIT_LOGO_BRIGHTNESS;
        Keyboard_Info.Logo_Speed      = INIT_LOGO_SPEED;
#endif
#if SIDE_LED_ENABLE
        Keyboard_Info.Side_On_Off     = INIT_SIDE_ON_OFF;
        Keyboard_Info.Side_Mode       = INIT_SIDE_MODE;
        Keyboard_Info.Side_Colour     = INIT_SIDE_COLOUR;
        Keyboard_Info.Side_Saturation = INIT_SIDE_SATURATION;
        Keyboard_Info.Side_Brightness = INIT_SIDE_BRIGHTNESS;
        Keyboard_Info.Side_Speed      = INIT_SIDE_SPEED;
#endif
    } else {
        // Settings saved by firmware 0.1.2 (12 bytes, logo settings at offset 6)
        // leave the bytes after them erased. Not done by the vendor's 0.1.5.
        if ((Keyboard_Info.Rf_Timer_2 == 0xFFFFFFFF) && (Keyboard_Info.Debounce == 0xFF)) {
            uint8_t Old_Logo[6];
            memcpy(Old_Logo, &((uint8_t *)&Keyboard_Info)[6], sizeof(Old_Logo));

            Keyboard_Info.All_Led_Off = INIT_ALL_LED_ON;
            Keyboard_Info.Reserved    = 0;
            Keyboard_Info.Sleep_Time  = INIT_SLEEP_TIME;
            Keyboard_Info.Rf_Timer_2  = INIT_RF_TIMER_2;
            Keyboard_Info.Debounce    = INIT_DEBOUNCE;
#if LOGO_LED_ENABLE
            Keyboard_Info.Logo_On_Off     = Old_Logo[0];
            Keyboard_Info.Logo_Mode       = Old_Logo[1];
            Keyboard_Info.Logo_Colour     = Old_Logo[2];
            Keyboard_Info.Logo_Saturation = Old_Logo[3];
            Keyboard_Info.Logo_Brightness = Old_Logo[4];
            Keyboard_Info.Logo_Speed      = Old_Logo[5];
#endif
        }

        if (Keyboard_Info.Key_Mode > QMK_USB_MODE) {
            Keyboard_Info.Key_Mode = QMK_USB_MODE;
        }

        if (Keyboard_Info.Ble_Channel > QMK_BLE_CHANNEL_3) {
            Keyboard_Info.Ble_Channel = QMK_BLE_CHANNEL_3;
        }

        if (Keyboard_Info.Batt_Number > 100) {
            Keyboard_Info.Batt_Number = 100;
        }

        if (Keyboard_Info.Nkro > INIT_ALL_KEY) {
            Keyboard_Info.Nkro = INIT_ALL_KEY;
        }

        if (Keyboard_Info.Mac_Win_Mode > INIT_MAC_MODE) {
            Keyboard_Info.Mac_Win_Mode = INIT_WIN_MODE;
        }

        if (Keyboard_Info.Win_Lock > INIT_WIN_LOCK) {
            Keyboard_Info.Win_Lock = INIT_WIN_NLOCK;
        }

        if (Keyboard_Info.Sleep_Time > SLEEP_TIME_30MIN) {
            Keyboard_Info.Sleep_Time = INIT_SLEEP_TIME;
        }

        if (Keyboard_Info.Rf_Timer_2 == 0xFFFFFFFF) {
            Keyboard_Info.Rf_Timer_2 = INIT_RF_TIMER_2;
        }

        // 0.1.5 only clamps values above 5; 0 would leave released keys held
        // in its debounce, so it is treated as invalid too.
        if ((Keyboard_Info.Debounce == 0) || (Keyboard_Info.Debounce > DEBOUNCE_SLOW)) {
            Keyboard_Info.Debounce = DEBOUNCE_SLOW;
        }
#if LOGO_LED_ENABLE
        if (Keyboard_Info.Logo_On_Off > LOGO_LED_OFF) {
            Keyboard_Info.Logo_On_Off = LOGO_LED_ON;
        }

        if (Keyboard_Info.Logo_Mode > LOGO_OFF_MODE) {
            Keyboard_Info.Logo_Mode = INIT_LOGO_MODE;
        }

        if (Keyboard_Info.Logo_Brightness > LOGO_MAX_BRIGHTNESS) {
            Keyboard_Info.Logo_Brightness = LOGO_MAX_BRIGHTNESS;
        }

        if (Keyboard_Info.Logo_Speed > LOGO_MAX_SPEED) {
            Keyboard_Info.Logo_Speed = INIT_LOGO_SPEED;
        }
#endif
#if SIDE_LED_ENABLE
        if (Keyboard_Info.Side_On_Off > SIDE_LED_OFF) {
            Keyboard_Info.Side_On_Off = SIDE_LED_ON;
        }

        if (Keyboard_Info.Side_Mode > SIDE_OFF_MODE) {
            Keyboard_Info.Side_Mode = INIT_SIDE_MODE;
        }

        if (Keyboard_Info.Side_Brightness > SIDE_MAX_BRIGHTNESS) {
            Keyboard_Info.Side_Brightness = SIDE_MAX_BRIGHTNESS;
        }

        if (Keyboard_Info.Side_Speed > SIDE_MAX_SPEED) {
            Keyboard_Info.Side_Speed = INIT_SIDE_SPEED;
        }
#endif
    }

    Debounce_Time = Keyboard_Info.Debounce;
}

void es_change_qmk_nkro_mode_enable(void) {
    if (keymap_config.nkro == false) {
        clear_keyboard();
        keymap_config.nkro = true;

        Keyboard_Info.Nkro = INIT_ALL_KEY;
        Save_Flash_Set();
    }
}

void es_change_qmk_nkro_mode_disable(void) {
    if (keymap_config.nkro == true) {
        clear_keyboard();
        keymap_config.nkro = false;

        Keyboard_Info.Nkro = INIT_SIX_KEY;
        Save_Flash_Set();
    }
}

void User_Keyboard_Init(void) {
    es_ble_spi_init();

    User_Adc_Init();

    eeprom_driver_init();

    rgb_matrix_driver_init();

    Init_Gpio_Information();

    Init_Keyboard_Information();

    Init_Batt_Information();

    User_Systime_Init();

    if (Keyboard_Info.Key_Mode == QMK_USB_MODE) {
        User_Usb_Init();
        Led_Rf_Pair_Flg = false;
    } else {
        Usb_Disconnect();
    }

    Init_Spi_Power_Up    = true;
    Init_Spi_100ms_Delay = 0;
    Spi_Interval         = SPI_DELAY_RF_TIME;

    NVIC_SetPriority(PendSV_IRQn, 3);
    NVIC_SetPriority(SysTick_IRQn, 3);

    Usb_If_Ok_Led       = false;
    Led_Power_Up        = false;
    Emi_Test_Start      = false;
    Func_Key_Long_Press = false;

#if LOGO_LED_ENABLE
    Logo_Init();
#endif
#if SIDE_LED_ENABLE
    Side_Init();
#endif
}

void User_Keyboard_Post_Init(void) {
    if (keymap_config.nkro != Keyboard_Info.Nkro) {
        keymap_config.nkro = Keyboard_Info.Nkro;
    }

    if (Keyboard_Info.Mac_Win_Mode) {
        uint8_t current_layer = biton(layer_state);
        if (current_layer != 1) {
            layer_on(1);
        }
    }
}

/************************USB plug-in**************************/
void User_Usb_Init(void) {
    md_rcu_enable_csu(RCU);
    CSU->CON |= CSU_CON_AUTOEN;
    CSU->CON |= CSU_CON_CNTEN;
}

void es_restart_usb_driver(void) {
    md_rcu_enable_usb(RCU);

    ald_usb_device_components_init();
    USB->TXIER = 0x7F;
    USB->RXIER = 0x7E;
    USB->IER   = 0x2F;
    ald_usb_dev_connect();
    ald_usb_int_register();
}

void Usb_Disconnect(void) {
    ald_usb_int_unregister();
    ald_usb_dev_disconnect();
    md_rcu_enable_usb_reset(RCU);
    md_rcu_disable_usb_reset(RCU);
    md_rcu_disable_usb(RCU);
}

void User_Usb_Deinit(void) {
    md_rcu_enable_csu_reset(RCU);
    md_rcu_disable_csu_reset(RCU);
    md_rcu_disable_csu(RCU);
}

/**************************FLASH****************************/
// EEPROM emulation after ST AN2594, with 32-bit records (virtual address in the
// high half, data in the low half) over two 8 KB pages. QMK's EEPROM and a
// separate 64-byte user area (Keyboard_Info, one 512-byte sector below page 0)
// are mirrored in g_es_flash_eeprom_table: bytes 0..63 are the user area and
// QMK's EEPROM starts at byte 64. The records index the table as halfwords.
//
// The application runs remapped, so flash is read at (physical - remap base);
// the flash controller itself takes physical addresses.
#define EE_REMAP_OFFSET (SYSCFG->REMAP & 0x1F000)

#define PAGE_SIZE            0x2000
#define EE_SECTOR_SIZE       0x200
#define EE_PAGE_SECTORS      (PAGE_SIZE / EE_SECTOR_SIZE)
#define EEPROM_START_ADDRESS 0x1C000
#define PAGE0_BASE_ADDRESS   (EEPROM_START_ADDRESS)
#define PAGE1_BASE_ADDRESS   (EEPROM_START_ADDRESS + PAGE_SIZE)
#define EE_USER_ADDRESS      (EEPROM_START_ADDRESS - EE_SECTOR_SIZE)

#define EE_USER_SIZE    64
#define EE_QMK_SIZE     1152
#define EE_TRANSFER_VAR 577 // halfwords copied on a page transfer

#define PAGE0 0
#define PAGE1 1

#define NO_VALID_PAGE 0xAB
#define PAGE_FULL     0x80

#define ERASED       0xFFFFFFFF
#define RECEIVE_DATA 0xEEEEEEEE
#define VALID_PAGE   0x00000000

#define READ_FROM_VALID_PAGE 0
#define WRITE_IN_VALID_PAGE  1

#define FLASH_COMPLETE SUCCESS

// Accessed as halfwords and programmed from as words: must stay word-aligned
// (Cortex-M0 faults on unaligned access).
__attribute__((aligned(4))) static uint8_t g_es_flash_eeprom_table[1218] = {0};

volatile uint32_t g_tst_remap_offset;

static uint32_t IAPROM_PAGE_ERASE(uint32_t addr) {
    md_fc_ControlTypeDef SErasePara;

    __disable_irq();

    md_fc_unlock();

    SErasePara.SAddr  = addr;
    SErasePara.SAddrC = ~addr;

    md_fc_page_erase(&SErasePara);

    md_fc_lock();

    __enable_irq();

    return SUCCESS;
}

static uint32_t IAPROM_WORD_PROGRAM(uint32_t addr, uint32_t data) {
    md_fc_ControlTypeDef ProgramPara;

    __disable_irq();

    md_fc_unlock();

    ProgramPara.BCnt    = 4;
    ProgramPara.pU32Buf = &data;
    ProgramPara.SAddr   = addr;
    ProgramPara.SAddrC  = ~addr;

    md_fc_program(&ProgramPara);

    md_fc_lock();

    __enable_irq();

    return SUCCESS;
}

static uint32_t ee_format(void);
static uint32_t ee_verify_pagefull_write_variable(uint32_t virt_address, uint32_t data);

// Loads the valid page into the RAM table, then repairs any interrupted page
// transfer (same state machine as AN2594's EE_Init).
static uint32_t ee_init(void) {
    uint32_t page_status0, page_status1;
    uint32_t var_idx;
    uint32_t eeprom_status;
    uint32_t flash_status;
    uint8_t  addr_index;
    uint32_t rom_read_end;

    page_status0 = *(__IO uint32_t *)(PAGE0_BASE_ADDRESS - EE_REMAP_OFFSET);

    page_status1 = *(__IO uint32_t *)(PAGE1_BASE_ADDRESS - EE_REMAP_OFFSET);

    if (page_status0 == VALID_PAGE) {
        rom_read_end = (PAGE0_BASE_ADDRESS + PAGE_SIZE - 1) - EE_REMAP_OFFSET;
        for (var_idx = (PAGE0_BASE_ADDRESS + 4) - EE_REMAP_OFFSET; var_idx < rom_read_end; var_idx += 4) {
            if ((*(__IO uint32_t *)var_idx >> 16) < EE_QMK_SIZE) {
                ((uint16_t *)g_es_flash_eeprom_table)[*(__IO uint32_t *)var_idx >> 16] = *(__IO uint32_t *)var_idx;
            } else {
                break;
            }
        }
    }

    if (page_status1 == VALID_PAGE) {
        rom_read_end = (PAGE1_BASE_ADDRESS + PAGE_SIZE - 1) - EE_REMAP_OFFSET;
        for (var_idx = (PAGE1_BASE_ADDRESS + 4) - EE_REMAP_OFFSET; var_idx < rom_read_end; var_idx += 4) {
            if ((*(__IO uint32_t *)var_idx >> 16) < EE_QMK_SIZE) {
                ((uint16_t *)g_es_flash_eeprom_table)[*(__IO uint32_t *)var_idx >> 16] = *(__IO uint32_t *)var_idx;
            } else {
                break;
            }
        }
    }

    switch (page_status0) {
        case ERASED:
            if (page_status1 == VALID_PAGE) {
                for (addr_index = 0; addr_index < EE_PAGE_SECTORS; addr_index++) {
                    IAPROM_PAGE_ERASE(PAGE0_BASE_ADDRESS + (addr_index * EE_SECTOR_SIZE));
                }
            } else if (page_status1 == RECEIVE_DATA) {
                for (addr_index = 0; addr_index < EE_PAGE_SECTORS; addr_index++) {
                    IAPROM_PAGE_ERASE(PAGE0_BASE_ADDRESS + (addr_index * EE_SECTOR_SIZE));
                }

                IAPROM_WORD_PROGRAM(PAGE1_BASE_ADDRESS, VALID_PAGE);
            } else {
                flash_status = ee_format();
                if (flash_status != FLASH_COMPLETE) {
                    return flash_status;
                }
            }
            break;

        case RECEIVE_DATA:
            if (page_status1 == VALID_PAGE) {
                for (var_idx = 0; var_idx < EE_TRANSFER_VAR; var_idx++) {
                    if (((__IO uint16_t *)g_es_flash_eeprom_table)[var_idx] != 0) {
                        eeprom_status = ee_verify_pagefull_write_variable(var_idx, ((__IO uint16_t *)g_es_flash_eeprom_table)[var_idx]);

                        if (eeprom_status != FLASH_COMPLETE) {
                            return eeprom_status;
                        }
                    }
                }

                IAPROM_WORD_PROGRAM(PAGE0_BASE_ADDRESS, VALID_PAGE);

                for (addr_index = 0; addr_index < EE_PAGE_SECTORS; addr_index++) {
                    IAPROM_PAGE_ERASE(PAGE1_BASE_ADDRESS + (addr_index * EE_SECTOR_SIZE));
                }
            } else if (page_status1 == ERASED) {
                for (addr_index = 0; addr_index < EE_PAGE_SECTORS; addr_index++) {
                    IAPROM_PAGE_ERASE(PAGE1_BASE_ADDRESS + (addr_index * EE_SECTOR_SIZE));
                }

                IAPROM_WORD_PROGRAM(PAGE0_BASE_ADDRESS, VALID_PAGE);
            } else {
                flash_status = ee_format();
                if (flash_status != FLASH_COMPLETE) {
                    return flash_status;
                }
            }
            break;

        case VALID_PAGE:
            if (page_status1 == VALID_PAGE) {
                flash_status = ee_format();
                if (flash_status != FLASH_COMPLETE) {
                    return flash_status;
                }
            } else if (page_status1 == ERASED) {
                for (addr_index = 0; addr_index < EE_PAGE_SECTORS; addr_index++) {
                    IAPROM_PAGE_ERASE(PAGE1_BASE_ADDRESS + (addr_index * EE_SECTOR_SIZE));
                }
            } else {
                for (var_idx = 0; var_idx < EE_TRANSFER_VAR; var_idx++) {
                    if (((__IO uint16_t *)g_es_flash_eeprom_table)[var_idx] != 0) {
                        eeprom_status = ee_verify_pagefull_write_variable(var_idx, ((__IO uint16_t *)g_es_flash_eeprom_table)[var_idx]);

                        if (eeprom_status != FLASH_COMPLETE) {
                            return eeprom_status;
                        }
                    }
                }

                IAPROM_WORD_PROGRAM(PAGE1_BASE_ADDRESS, VALID_PAGE);

                for (addr_index = 0; addr_index < EE_PAGE_SECTORS; addr_index++) {
                    IAPROM_PAGE_ERASE(PAGE0_BASE_ADDRESS + (addr_index * EE_SECTOR_SIZE));
                }
            }
            break;

        default:
            flash_status = ee_format();
            break;
    }

    return FLASH_COMPLETE;
}

static uint32_t ee_page_transfer(uint32_t virt_address, uint32_t data);

static uint32_t ee_write_variable(uint32_t virt_address) {
    uint32_t status = 0;
    uint16_t data;

    if (virt_address >= EE_QMK_SIZE) {
        return status;
    }

    data = ((uint16_t *)g_es_flash_eeprom_table)[virt_address >> 1];

    status = ee_verify_pagefull_write_variable(virt_address >> 1, data);

    if (status == PAGE_FULL) {
        status = ee_page_transfer(virt_address >> 1, data);
    }

    return status;
}

static uint32_t ee_format(void) {
    uint8_t addr_index;

    for (addr_index = 0; addr_index < EE_PAGE_SECTORS; addr_index++) {
        IAPROM_PAGE_ERASE(PAGE0_BASE_ADDRESS + (addr_index * EE_SECTOR_SIZE));
    }

    IAPROM_WORD_PROGRAM(PAGE0_BASE_ADDRESS, VALID_PAGE);

    for (addr_index = 0; addr_index < EE_PAGE_SECTORS; addr_index++) {
        IAPROM_PAGE_ERASE(PAGE1_BASE_ADDRESS + (addr_index * EE_SECTOR_SIZE));
    }

    return FLASH_COMPLETE;
}

static uint32_t ee_find_valid_page(uint8_t operation) {
    uint32_t page_status0, page_status1;

    page_status0 = *(__IO uint32_t *)(PAGE0_BASE_ADDRESS - EE_REMAP_OFFSET);

    page_status1 = *(__IO uint32_t *)(PAGE1_BASE_ADDRESS - EE_REMAP_OFFSET);

    switch (operation) {
        case WRITE_IN_VALID_PAGE:
            if (page_status1 == VALID_PAGE) {
                if (page_status0 == RECEIVE_DATA) {
                    return PAGE0;
                } else {
                    return PAGE1;
                }
            } else if (page_status0 == VALID_PAGE) {
                if (page_status1 == RECEIVE_DATA) {
                    return PAGE1;
                } else {
                    return PAGE0;
                }
            } else {
                return NO_VALID_PAGE;
            }

        case READ_FROM_VALID_PAGE:
            if (page_status0 == VALID_PAGE) {
                return PAGE0;
            } else if (page_status1 == VALID_PAGE) {
                return PAGE1;
            } else {
                return NO_VALID_PAGE;
            }

        default:
            return PAGE0;
    }
}

static uint32_t ee_verify_pagefull_write_variable(uint32_t virt_address, uint32_t data) {
    uint32_t flash_status = FLASH_COMPLETE;
    uint32_t valid_page   = PAGE0;
    uint32_t address, page_endaddress;

    valid_page = ee_find_valid_page(WRITE_IN_VALID_PAGE);

    if (valid_page == NO_VALID_PAGE) {
        return NO_VALID_PAGE;
    }

    address = (EEPROM_START_ADDRESS + (valid_page * PAGE_SIZE)) - EE_REMAP_OFFSET;

    page_endaddress = ((EEPROM_START_ADDRESS - 4) + ((1 + valid_page) * PAGE_SIZE)) - EE_REMAP_OFFSET;

    while (address <= page_endaddress) {
        if ((*(__IO uint32_t *)address) == 0xFFFFFFFF) {
            flash_status = IAPROM_WORD_PROGRAM(address + EE_REMAP_OFFSET, (virt_address << 16) | data);

            return flash_status;
        } else {
            address = address + 4;
        }
    }

    return PAGE_FULL;
}

static uint32_t ee_page_transfer(uint32_t virt_address, uint32_t data) {
    uint32_t flash_status    = FLASH_COMPLETE;
    uint32_t new_pageaddress = PAGE1_BASE_ADDRESS;
    uint32_t old_pageaddress = PAGE0_BASE_ADDRESS;
    uint32_t valid_page = PAGE0, var_idx = 0;
    uint32_t eeprom_status = 0;
    uint8_t  addr_index;

    valid_page = ee_find_valid_page(READ_FROM_VALID_PAGE);

    if (valid_page == PAGE1) {
        new_pageaddress = PAGE0_BASE_ADDRESS;

        old_pageaddress = PAGE1_BASE_ADDRESS;
    } else if (valid_page == PAGE0) {
        new_pageaddress = PAGE1_BASE_ADDRESS;

        old_pageaddress = PAGE0_BASE_ADDRESS;
    } else {
        return NO_VALID_PAGE;
    }

    for (addr_index = 0; addr_index < EE_PAGE_SECTORS; addr_index++) {
        IAPROM_PAGE_ERASE(new_pageaddress + (addr_index * EE_SECTOR_SIZE));
    }

    IAPROM_WORD_PROGRAM(new_pageaddress, RECEIVE_DATA);

    eeprom_status = ee_verify_pagefull_write_variable(virt_address, data);
    if (eeprom_status != FLASH_COMPLETE) {
        return eeprom_status;
    }

    for (var_idx = 0; var_idx < EE_TRANSFER_VAR; var_idx++) {
        if (((__IO uint16_t *)g_es_flash_eeprom_table)[var_idx] != 0) {
            eeprom_status = ee_verify_pagefull_write_variable(var_idx, ((__IO uint16_t *)g_es_flash_eeprom_table)[var_idx]);

            if (eeprom_status != FLASH_COMPLETE) {
                return eeprom_status;
            }
        }
    }

    for (addr_index = 0; addr_index < EE_PAGE_SECTORS; addr_index++) {
        IAPROM_PAGE_ERASE(old_pageaddress + (addr_index * EE_SECTOR_SIZE));
    }

    flash_status = IAPROM_WORD_PROGRAM(new_pageaddress, VALID_PAGE);

    return flash_status;
}

size_t clamp_length(intptr_t offset, size_t len) {
    if (offset + len > EE_QMK_SIZE) {
        len = EE_QMK_SIZE - offset;
    }

    return len;
}

size_t clamp_length_user(intptr_t offset, size_t len) {
    if (offset + len > EE_USER_SIZE) {
        len = EE_USER_SIZE - offset;
    }

    return len;
}

void eeprom_driver_erase(void) {
    ee_format();
    memset(g_es_flash_eeprom_table, 0x00, sizeof(g_es_flash_eeprom_table));
}

volatile uint8_t es_eeprom_init_flag = 0;

void eeprom_driver_init(void) {
    if (es_eeprom_init_flag == 0) {
        g_tst_remap_offset = EE_REMAP_OFFSET;
        (void)g_tst_remap_offset;
        memset(g_es_flash_eeprom_table, 0x00, sizeof(g_es_flash_eeprom_table));
        ee_init();
        memcpy(g_es_flash_eeprom_table, (uint8_t *)(EE_USER_ADDRESS - EE_REMAP_OFFSET), EE_USER_SIZE);
        es_eeprom_init_flag = 1;
    }
}

void eeprom_read_block(void *buf, const void *addr, size_t len) {
    intptr_t offset = (intptr_t)addr;
    memset(buf, 0x00, len);
    len = clamp_length(offset, len);
    if (len > 0) {
        memcpy(buf, &g_es_flash_eeprom_table[EE_USER_SIZE + offset], len);
    }
}

void eeprom_write_block(const void *buf, void *addr, size_t len) {
    uint16_t i;
    intptr_t offset = (intptr_t)addr;
    len             = clamp_length(offset, len);
    if (len > 0) {
        for (i = 0; i < len; i++) {
            if (g_es_flash_eeprom_table[offset + i + EE_USER_SIZE] != ((uint8_t *)buf)[i]) {
                g_es_flash_eeprom_table[offset + i + EE_USER_SIZE] = ((uint8_t *)buf)[i];
                ee_write_variable(offset + i + EE_USER_SIZE);
            }
        }
        memcpy(&g_es_flash_eeprom_table[EE_USER_SIZE + offset], buf, len);
    }
}

void eeprom_read_block_user(void *buf, const void *addr, size_t len) {
    intptr_t offset = (intptr_t)addr;
    memset(buf, 0x00, len);
    len = clamp_length_user(offset, len);
    if (len > 0) {
        memcpy(buf, (uint8_t *)(offset - EE_REMAP_OFFSET + EE_USER_ADDRESS), len);
    }
}

// The user area lives in its own sector, rewritten whole when it changes.
void eeprom_write_block_user(const void *buf, void *addr, size_t len) {
    md_fc_ControlTypeDef ProgramPara;
    intptr_t             offset = (intptr_t)addr;
    len                         = clamp_length_user(offset, len);
    if (len > 0) {
        __disable_irq();
        if (memcmp(buf, (uint8_t *)(offset + (EE_USER_ADDRESS - EE_REMAP_OFFSET)), len) != 0) {
            memcpy(&g_es_flash_eeprom_table[offset], buf, len);

            md_fc_unlock();
            ProgramPara.SAddr  = EE_USER_ADDRESS;
            ProgramPara.SAddrC = ~EE_USER_ADDRESS;
            md_fc_page_erase(&ProgramPara);
            md_fc_lock();

            md_fc_unlock();
            ProgramPara.BCnt    = EE_USER_SIZE;
            ProgramPara.pU32Buf = (uint32_t *)g_es_flash_eeprom_table;
            ProgramPara.SAddr   = EE_USER_ADDRESS;
            ProgramPara.SAddrC  = ~EE_USER_ADDRESS;
            md_fc_program(&ProgramPara);
            md_fc_lock();
        }
        __enable_irq();
    }
}

/************************Encoder**************************/
// QMK's drivers/encoder/encoder_quadrature.c trimmed to the single knob, with a
// resolution of 2 pulses per step (QMK's default is 4). The vendor removed the
// ENCODER_DRIVER selection from builddefs/common_features.mk, so this is the
// only encoder driver in the build.
#ifndef ENCODER_RESOLUTION
#    define ENCODER_RESOLUTION 2
#endif

#define ENCODER_CLOCKWISE true
#define ENCODER_COUNTER_CLOCKWISE false

static pin_t encoders_pad_a[NUM_ENCODERS_MAX_PER_SIDE] = ENCODER_A_PINS;
static pin_t encoders_pad_b[NUM_ENCODERS_MAX_PER_SIDE] = ENCODER_B_PINS;

__attribute__((weak)) void encoder_wait_pullup_charge(void) {
    wait_us(100);
}

__attribute__((weak)) void encoder_quadrature_init_pin(uint8_t index, bool pad_b) {
    pin_t pin = pad_b ? encoders_pad_b[index] : encoders_pad_a[index];
    if (pin != NO_PIN) {
        gpio_set_pin_input_high(pin);
    }
}

__attribute__((weak)) uint8_t encoder_quadrature_read_pin(uint8_t index, bool pad_b) {
    pin_t pin = pad_b ? encoders_pad_b[index] : encoders_pad_a[index];
    if (pin != NO_PIN) {
        return gpio_read_pin(pin) ? 1 : 0;
    }
    return 0;
}

static int8_t encoder_LUT[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

static uint8_t encoder_state[NUM_ENCODERS]  = {0};
static int8_t  encoder_pulses[NUM_ENCODERS] = {0};

static uint8_t thisCount;

__attribute__((weak)) void encoder_quadrature_post_init_kb(void) {}

void encoder_quadrature_post_init(void) {
    for (uint8_t i = 0; i < thisCount; i++) {
        encoder_quadrature_init_pin(i, false);
        encoder_quadrature_init_pin(i, true);
    }
    encoder_wait_pullup_charge();
    for (uint8_t i = 0; i < thisCount; i++) {
        encoder_state[i] = (encoder_quadrature_read_pin(i, false) << 0) | (encoder_quadrature_read_pin(i, true) << 1);
    }

    encoder_quadrature_post_init_kb();
}

void encoder_driver_init(void) {
    thisCount = NUM_ENCODERS;

    encoder_quadrature_post_init();
}

static void encoder_handle_state_change(uint8_t index, uint8_t state) {
    uint8_t i = index;

    const uint8_t resolution = ENCODER_RESOLUTION;

    encoder_pulses[i] += encoder_LUT[state & 0xF];

    if (encoder_pulses[i] >= resolution) {
        encoder_queue_event(index, ENCODER_COUNTER_CLOCKWISE);
    }

    if (encoder_pulses[i] <= -resolution) {
        encoder_queue_event(index, ENCODER_CLOCKWISE);
    }
    encoder_pulses[i] %= resolution;
}

void encoder_quadrature_handle_read(uint8_t index, uint8_t pin_a_state, uint8_t pin_b_state) {
    uint8_t state = pin_a_state | (pin_b_state << 1);
    if ((encoder_state[index] & 0x3) != state) {
        encoder_state[index] <<= 2;
        encoder_state[index] |= state;
        encoder_handle_state_change(index, encoder_state[index]);
    }
}

__attribute__((weak)) void encoder_driver_task(void) {
    for (uint8_t i = 0; i < thisCount; i++) {
        encoder_quadrature_handle_read(i, encoder_quadrature_read_pin(i, false), encoder_quadrature_read_pin(i, true));
    }
}

/************************Battery******************************/
void Init_Batt_Information(void) {
    if (gpio_read_pin(ES_USB_POWER_IO)) {
        User_Batt_BaiFen     = Keyboard_Info.Batt_Number;
        User_Batt_Old_BaiFen = Keyboard_Info.Batt_Number;
        User_Batt_Power_Up   = false;
    } else {
        User_Batt_Power_Up = true;
    }

    User_Batt_10ms_Count     = 0;
    User_Adc_Batt_Count      = 0;
    User_Batt_Time_15S_Count = 0;
    User_Power_Low           = false;
    User_Power_Low_Count     = 0;

    U16_Buff_Clear(User_Adc_Batt, USER_BATT_SCAN_COUNT);
    U16_Buff_Clear(User_Scan_Batt, USER_BATT_SCAN_COUNT);

    User_Batt_Power_Up_Delay_100ms_Count = 0;
    User_Batt_Power_Up_Delay             = true;
}

// Battery voltage on PC4 = ADC channel 14.
void User_Adc_Init(void) {
    md_gpio_inittypedef gpiox;

    gpiox.OutputType = MD_GPIO_OUTPUT_PUSHPULL;
    gpiox.Pull       = MD_GPIO_PULL_FLOATING;
    gpiox.OutDrive   = MD_GPIO_DRIVING_8MA;
    gpiox.Function   = MD_GPIO_AF0;
    gpiox.Mode       = MD_GPIO_MODE_ANALOG;
    gpiox.Pin        = MD_GPIO_PIN_4;
    md_gpio_init(GPIOC, &gpiox);

    md_rcu_enable_adc(RCU);
    md_adc_calibration(ADC, (md_adc_initial *)&adc_initStruct);
    md_adc_set_sampletime_channel_14(ADC, 0x40);
    md_adc_init(ADC, (md_adc_initial *)&adc_initStruct);

    while ((ADC->RIF & ADC_RIF_ARDY) == 0) {
    }

    md_adc_set_normal_sequence_length(ADC, 0);
    md_adc_set_normal_sequence_selection_1th(ADC, 14);
    md_adc_set_start_normal(ADC, MD_ADC_CON_NSTART_START_REGULAR);
}

void User_Adc_Deinit(void) {
    md_rcu_enable_adc_reset(RCU);
    md_rcu_disable_adc_reset(RCU);
    md_rcu_disable_adc(RCU);
}

void U16_Buff_Clear(uint16_t *Buff, uint8_t Len) {
    memset(Buff, 0, Len * sizeof(uint16_t));
}

// First reading after power-up: average of the samples without the highest and
// lowest, mapped linearly from shutdown (0 %) to full (100 %).
void User_Adc_Batt_Power_Up_Init(void) {
    uint8_t Min_Batt = 0, Max_Batt = 0;
    for (uint8_t i = 0; i < USER_BATT_SCAN_COUNT; i++) {
        if (User_Scan_Batt[i] > User_Scan_Batt[Max_Batt]) {
            Max_Batt = i;
        }

        if (User_Scan_Batt[i] < User_Scan_Batt[Min_Batt]) {
            Min_Batt = i;
        }
    }

    uint16_t Temp_Batt_Sub = 0;
    if (Max_Batt == Min_Batt) {
        for (uint8_t i = 0; i < (USER_BATT_SCAN_COUNT - 2); i++) {
            Temp_Batt_Sub += User_Scan_Batt[i];
        }
    } else {
        for (uint8_t i = 0; i < USER_BATT_SCAN_COUNT; i++) {
            if ((i != Max_Batt) && (i != Min_Batt)) {
                Temp_Batt_Sub += User_Scan_Batt[i];
            }
        }
    }

    uint16_t Temp_Average = Temp_Batt_Sub / (USER_BATT_SCAN_COUNT - 2);

    uint8_t Temp_Batt_Number = 0;
    if (Temp_Average >= USER_BATT_HIGH_POWER) {
        Temp_Batt_Number = 100;
    } else if (Temp_Average > USER_BATT_SHUTDOWN_POWER) {
        Temp_Batt_Number = ((Temp_Average - USER_BATT_SHUTDOWN_POWER) * 100) / (USER_BATT_HIGH_POWER - USER_BATT_SHUTDOWN_POWER);
    }

    if (Temp_Average <= USER_BATT_SHUTDOWN_POWER) {
        User_Power_Low_Count++;
        if (User_Power_Low_Count > 3) {
            User_Power_Low_Count = 0;
            User_Power_Low       = true;
        }
    } else if (Temp_Average <= USER_BATT_LOW_POWER) {
        User_Power_Low_Count++;
        if (User_Power_Low_Count > 9) {
            User_Power_Low_Count = 0;
            User_Power_Low       = true;
        }
    } else {
        User_Power_Low_Count = 0;
    }

    User_Batt_BaiFen     = Temp_Batt_Number;
    User_Batt_Old_BaiFen = Temp_Batt_Number;

    if (Keyboard_Info.Batt_Number != Temp_Batt_Number) {
        Keyboard_Info.Batt_Number = Temp_Batt_Number;
        Save_Flash_Set();
    }

    User_Adc_Batt_Count      = 0;
    User_Batt_10ms_Count     = 0;
    User_Batt_Time_15S_Count = 0;
    User_Batt_Power_Up       = false;
}

// Periodic update. es_stdby_pin_state: 2 = charged, 1 = charging, 0 = on
// battery. The reported level moves by at most 1 % per 2500 ticks, upwards
// while charging and downwards on battery.
void User_Adc_Batt_Number(void) {
    if (es_stdby_pin_state == 2) {
        User_Batt_BaiFen     = 100;
        User_Batt_Old_BaiFen = 100;
        if (Keyboard_Info.Batt_Number != User_Batt_BaiFen) {
            Keyboard_Info.Batt_Number = User_Batt_BaiFen;
            User_Batt_Send_Spi        = true;
            Save_Flash_Set();
        }

        User_Power_Low       = false;
        User_Power_Low_Count = 0;
    } else {
        uint8_t Min_Batt = 0, Max_Batt = 0;
        for (uint8_t i = 0; i < USER_BATT_SCAN_COUNT; i++) {
            if (User_Scan_Batt[i] > User_Scan_Batt[Max_Batt]) {
                Max_Batt = i;
            }

            if (User_Scan_Batt[i] < User_Scan_Batt[Min_Batt]) {
                Min_Batt = i;
            }
        }

        uint16_t Temp_Batt_Sub = 0;
        if (Max_Batt == Min_Batt) {
            for (uint8_t i = 0; i < (USER_BATT_SCAN_COUNT - 2); i++) {
                Temp_Batt_Sub += User_Scan_Batt[i];
            }
        } else {
            for (uint8_t i = 0; i < USER_BATT_SCAN_COUNT; i++) {
                if ((i != Max_Batt) && (i != Min_Batt)) {
                    Temp_Batt_Sub += User_Scan_Batt[i];
                }
            }
        }

        uint16_t Temp_Average = Temp_Batt_Sub / (USER_BATT_SCAN_COUNT - 2);

        uint8_t Temp_Batt_Number = 0;
        if (Temp_Average >= USER_BATT_HIGH_POWER) {
            Temp_Batt_Number = 100;
        } else if (Temp_Average > USER_BATT_SHUTDOWN_POWER) {
            Temp_Batt_Number = ((Temp_Average - USER_BATT_SHUTDOWN_POWER) * 100) / (USER_BATT_HIGH_POWER - USER_BATT_SHUTDOWN_POWER);
        }

        if (es_stdby_pin_state == 1) {
            if (Temp_Batt_Number == 0) {
                Temp_Batt_Number = 1;
            } else if (Temp_Batt_Number > 99) {
                Temp_Batt_Number = 99;
            }
        } else {
            if (Temp_Average <= USER_BATT_SHUTDOWN_POWER) {
                User_Power_Low_Count++;
                if (User_Power_Low_Count > 4) {
                    User_Power_Low_Count = 0;
                    User_Power_Low       = true;
                }
            } else if (Temp_Average <= USER_BATT_LOW_POWER) {
                User_Power_Low_Count++;
                if (User_Power_Low_Count > 9) {
                    User_Power_Low_Count = 0;
                    User_Power_Low       = true;
                }
            } else {
                User_Power_Low_Count = 0;
            }
        }

        if (es_stdby_pin_state) {
            if (User_Batt_Old_BaiFen < Temp_Batt_Number) {
                if (User_Batt_Time_15S_Count > 2499) {
                    User_Batt_Time_15S_Count = 0;
                    if (User_Batt_BaiFen < 99) {
                        User_Batt_BaiFen++;
                    }
                    User_Batt_Old_BaiFen = User_Batt_BaiFen;
                }
            } else {
                User_Batt_Time_15S_Count = 0;
            }

            User_Power_Low       = false;
            User_Power_Low_Count = 0;
        } else {
            if (User_Batt_Old_BaiFen > Temp_Batt_Number) {
                if (User_Batt_Time_15S_Count > 2499) {
                    User_Batt_Time_15S_Count = 0;
                    if (User_Batt_BaiFen > 0) {
                        User_Batt_BaiFen--;
                    }
                    User_Batt_Old_BaiFen = User_Batt_BaiFen;
                }
            } else {
                User_Batt_Time_15S_Count = 0;
            }
        }

        if (Keyboard_Info.Batt_Number != User_Batt_BaiFen) {
            Keyboard_Info.Batt_Number = User_Batt_BaiFen;
            User_Batt_Send_Spi        = true;
            Save_Flash_Set();
        }
    }
}


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

/************************Interrupts**************************/
// WAKEUP (IRQ 3): only clears the wake-up flags.
CH_FAST_IRQ_HANDLER(Vector4C) {
    md_syscfg_clear_flag_wakeup(SYSCFG);
    md_exti_clear_it_wakeup(EXTI);
}

// EXTI lines 0-1 (IRQ 5) and 2-3 (IRQ 6): sleep wake-up sources, just acknowledged.
OSAL_IRQ_HANDLER(Vector54) {
    uint32_t irq_ifm;
    OSAL_IRQ_PROLOGUE();

    irq_ifm   = EXTI->IFM;
    EXTI->ICR = irq_ifm;

    OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(Vector58) {
    uint32_t irq_ifm;
    OSAL_IRQ_PROLOGUE();

    irq_ifm   = EXTI->IFM;
    EXTI->ICR = irq_ifm;

    OSAL_IRQ_EPILOGUE();
}

// GP32C4T1 (IRQ 15), GP16C4T1 (IRQ 16), GP16C4T2 (IRQ 17, row B5 wake-up):
// just acknowledged.
OSAL_IRQ_HANDLER(Vector7C) {
    uint32_t irq_ifm;
    OSAL_IRQ_PROLOGUE();

    irq_ifm       = GP32C4T1->IFM;
    GP32C4T1->ICR = irq_ifm;

    OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(Vector80) {
    uint32_t irq_ifm;
    OSAL_IRQ_PROLOGUE();

    irq_ifm       = GP16C4T1->IFM;
    GP16C4T1->ICR = irq_ifm;

    OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(Vector84) {
    uint32_t irq_ifm;
    OSAL_IRQ_PROLOGUE();

    irq_ifm       = GP16C4T2->IFM;
    GP16C4T2->ICR = irq_ifm;

    OSAL_IRQ_EPILOGUE();
}

// DMA1 channels 1-2 (IRQ 10): just acknowledged.
OSAL_IRQ_HANDLER(Vector68) {
    uint32_t irq_ifm;
    OSAL_IRQ_PROLOGUE();

    irq_ifm   = DMA1->IFM;
    DMA1->ICR = irq_ifm;

    OSAL_IRQ_EPILOGUE();
}

// EXTI lines 4-15 (IRQ 7): the radio's ACK line on PA4 drives the SPI exchange.
// After a send (Spi_Send_Recv_Flg = 1) the radio raises ACK when it has a reply;
// we clock it in (flag 2), and when ACK drops the reply is parsed, or the frame
// is resent up to three times.
OSAL_IRQ_HANDLER(Vector5C) {
    uint32_t irq_ifm;
    OSAL_IRQ_PROLOGUE();

    irq_ifm   = EXTI->IFM;
    EXTI->ICR = irq_ifm;

    if (Init_Spi_Power_Up == false) {
        if (irq_ifm & MD_EXTI_GPIO4) {
            if (Spi_Send_Recv_Flg) {
                if (Send_Key_Type == 0) {
                    if (gpio_read_pin(ES_SPI_ACK_IO) == 0) {
                        Spi_Send_Recv_Flg = 0;
                    }
                } else {
                    if (gpio_read_pin(ES_SPI_ACK_IO)) {
                        if (Spi_Send_Recv_Flg == 1) {
                            Spi_Send_Recv_Flg = 2;
                            memset(g_es_spi_rx_buf, 0, sizeof(g_es_spi_rx_buf));
                            // A tx address outside SRAM makes the transfer clock out zeros.
                            es_spi_send_recv_by_dma(USER_KEYBOARD_LENGTH, g_es_spi_rx_buf, (uint8_t *)0x1000);
                        }
                    } else {
                        if (Spi_Send_Recv_Flg == 2) {
                            if (Emi_Test_Start && ((g_es_spi_rx_buf[0] & 0x7F) == (USER_EMI_COMMAND & 0x7F))) {
                                Get_Spi_Return_Data(g_es_spi_rx_buf);
                                Spi_Send_Recv_Flg = 0;
                            } else if (g_es_spi_rx_buf[0] == USER_KEYBOARD_COMMAND) {
                                Get_Spi_Return_Data(g_es_spi_rx_buf);
                                Spi_Send_Recv_Flg = 0;
                            } else {
                                Repeat_Send_Count++;
                                if (Repeat_Send_Count > 2) {
                                    Repeat_Send_Count = 0;
                                    Spi_Send_Recv_Flg = 0;
                                } else {
                                    Spi_Send_Recv_Flg = 1;
                                    es_spi_send_recv_by_dma(USER_KEYBOARD_LENGTH, g_es_spi_rx_buf, g_es_spi_tx_buf);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    OSAL_IRQ_EPILOGUE();
}

// BS16T1 (IRQ 14): the 2 ms housekeeping tick.
OSAL_IRQ_HANDLER(Vector78) {
    OSAL_IRQ_PROLOGUE();

    BS16T1->ICR = BS16T1->IFM;

    // Give the radio 100 ms after power-up, then wait for it to release ACK.
    if (Init_Spi_Power_Up) {
        Init_Spi_100ms_Delay++;
        if (Init_Spi_100ms_Delay >= 10) {
            Init_Spi_100ms_Delay = 0;
            if (gpio_read_pin(ES_SPI_ACK_IO) == 0) {
                Init_Spi_Power_Up = false;
            } else {
                Init_Spi_100ms_Delay = 5;
            }
        }
    }

    // Queued reports go out every tick in 2.4G mode, every 4th tick (8 ms) over
    // Bluetooth.
    if (Keyboard_Info.Key_Mode != QMK_USB_MODE) {
        if (Keyboard_Status.System_Work_Status && (Keyboard_Status.System_Sleep_Mode == 0)) {
            Spi_Ack_Send_Command(USER_KEYBOARD_SLEEP);
        } else if ((Keyboard_Info.Key_Mode != QMK_BLE_MODE) || (++Spi_Ble_Send_Count > 3)) {
            Spi_Ble_Send_Count = 0;
            Spi_Main_Loop();
        }
    }

    // Battery: after a 100 ms settling delay, sample every 4 ticks at power-up
    // and every 20 ticks afterwards; each 10 samples update the level.
    if (User_Batt_Power_Up_Delay) {
        User_Batt_Power_Up_Delay_100ms_Count++;
        if (User_Batt_Power_Up_Delay_100ms_Count >= 50) {
            User_Batt_Power_Up_Delay_100ms_Count = 0;
            User_Batt_Power_Up_Delay             = false;
        }
    } else if (User_Batt_Power_Up) {
        User_Batt_10ms_Count++;
        if (User_Batt_10ms_Count > 3) {
            User_Batt_10ms_Count = 0;
            if (md_adc_is_active_flag_normal_status(ADC) == 0) {
                User_Adc_Batt[User_Adc_Batt_Count] = md_adc_get_normal_data(ADC);
                md_adc_set_start_normal(ADC, MD_ADC_CON_NSTART_START_REGULAR);
                User_Adc_Batt_Count++;
                if (User_Adc_Batt_Count >= USER_BATT_SCAN_COUNT) {
                    User_Adc_Batt_Count = 0;
                    for (uint8_t i = 0; i < USER_BATT_SCAN_COUNT; i++) {
                        User_Scan_Batt[i] = User_Adc_Batt[i];
                    }
                    User_Adc_Batt_Power_Up_Init();
                }
            }
        }
    } else {
        User_Batt_10ms_Count++;
        if (User_Batt_10ms_Count > 19) {
            User_Batt_10ms_Count = 0;
            if (md_adc_is_active_flag_normal_status(ADC) == 0) {
                User_Adc_Batt[User_Adc_Batt_Count] = md_adc_get_normal_data(ADC);
                md_adc_set_start_normal(ADC, MD_ADC_CON_NSTART_START_REGULAR);
                User_Adc_Batt_Count++;
                if (User_Adc_Batt_Count >= USER_BATT_SCAN_COUNT) {
                    User_Adc_Batt_Count = 0;
                    for (uint8_t i = 0; i < USER_BATT_SCAN_COUNT; i++) {
                        User_Scan_Batt[i] = User_Adc_Batt[i];
                    }
                    User_Adc_Batt_Number();
                }
            }
            User_Batt_Time_15S_Count++;
        }
    }

    // Route QMK's reports through es_user_driver (USB and/or radio).
    Systick_6ms_Count++;
    if (Systick_6ms_Count >= 3) {
        Systick_6ms_Count = 0;

        host_driver_t *temp_driver;
        temp_driver = host_get_driver();
        if ((temp_driver != &es_user_driver) && (temp_driver != NULL)) {
            es_qmk_driver = host_get_driver();
            host_set_driver((host_driver_t *)&es_user_driver);
        }
    }

    Systick_10ms_Count++;
    if (Systick_10ms_Count >= 5) {
        Systick_10ms_Count = 0;

        Spi_Sync_Request = true;

        Systick_Led_Count++;
        if (Systick_Led_Count == 255) {
            Systick_Led_Count = 0;
        }

        Batt_Led_Count++;
        if (Batt_Led_Count == 255) {
            Batt_Led_Count = 0;
        }

        if (Led_Power_Up == false) {
            Led_Power_Up_Delay++;
            if (Led_Power_Up_Delay >= 100) {
                Led_Power_Up_Delay = 0;
                Led_Power_Up       = true;
                if (Keyboard_Info.Key_Mode == QMK_BLE_MODE) {
                    User_Batt_Send_Spi = true;
                }
            }
        }

        Logo_Flash_Count++;
        if (Logo_Flash_Count == 255) {
            Logo_Flash_Count = 0;
        }

        Logo_Led_Count++;
        if (Logo_Led_Count == 255) {
            Logo_Led_Count = 0;
        }

        Usb_Change_Mode_Delay++;
        if (Usb_Change_Mode_Delay >= 300) {
            Usb_Change_Mode_Delay  = 0;
            Usb_Change_Mode_Wakeup = true;
        }

        // Settings are written 3 s after the last change, when the SPI link is
        // idle and an LED frame is being composed (Led_Flash_Busy is set between
        // LED 0 and the last LED, before the flush starts the DMA).
        if (Save_Flash) {
            Save_Flash_3S_Count++;
            if (Save_Flash_3S_Count >= 300) {
                if ((Spi_Send_Recv_Flg != 0) || (gpio_read_pin(ES_SPI_ACK_IO) != 0) || (Led_Flash_Busy == false)) {
                    Save_Flash_3S_Count = 290;
                } else {
                    Reset_Save_Flash = true;

                    eeprom_write_block_user(&Keyboard_Info, 0, sizeof(Keyboard_Info_t));
                    Reset_Save_Flash    = false;
                    Save_Flash          = false;
                    Save_Flash_3S_Count = 0;
                }
            }
        } else {
            Save_Flash_3S_Count = 0;
        }

        if (Keyboard_Info.Key_Mode != QMK_USB_MODE) {
            if (User_Batt_Send_Spi) {
                User_Batt_Send_Spi = false;
                Spi_Send_Command(USER_BATTERY_DATA);
            }
            Usb_If_Ok_Led = false;
        } else {
            g_usb_sof_frame_id = USB->FRAME1 | (USB->FRAME2 << 8);
            Usb_Dis_Connect    = true;
        }

        // Charger: USB power present and STDBY high = charging, low = charged.
        if (gpio_read_pin(ES_USB_POWER_IO)) {
            if (gpio_read_pin(ES_BATT_STDBY_IO)) {
                es_stdby_pin_state = 1;
            } else {
                es_stdby_pin_state = 2;
            }
        } else {
            es_stdby_pin_state = 0;
        }

        // Pairing keys take effect after being held for 3 s.
        Time_3s_Count++;
        if (Time_3s_Count >= 300) {
            Time_3s_Count = 0;

            if (Keyboard_Info.Key_Mode != QMK_USB_MODE) {
                if (Key_2p4g_Status || Key_Ble_1_Status || Key_Ble_2_Status || Key_Ble_3_Status) {
                    switch (Keyboard_Info.Key_Mode) {
                        case QMK_2P4G_MODE:
                            if (Key_2p4g_Status) {
                                Key_2p4g_Status = false;
                                Spi_Send_Command(USER_SWITCH_2P4G_PAIR);
                                Led_Rf_Pair_Flg = true;
                            }
                            break;
                        case QMK_BLE_MODE:
                            if ((Keyboard_Info.Ble_Channel == QMK_BLE_CHANNEL_1) && Key_Ble_1_Status) {
                                Key_Ble_1_Status = false;
                                Spi_Send_Command(USER_SWITCH_BLE_1_PAIR);
                                Led_Rf_Pair_Flg = true;
                            } else if ((Keyboard_Info.Ble_Channel == QMK_BLE_CHANNEL_2) && Key_Ble_2_Status) {
                                Key_Ble_2_Status = false;
                                Spi_Send_Command(USER_SWITCH_BLE_2_PAIR);
                                Led_Rf_Pair_Flg = true;
                            } else if ((Keyboard_Info.Ble_Channel == QMK_BLE_CHANNEL_3) && Key_Ble_3_Status) {
                                Key_Ble_3_Status = false;
                                Spi_Send_Command(USER_SWITCH_BLE_3_PAIR);
                                Led_Rf_Pair_Flg = true;
                            }
                            break;
                    }
                }
            }
        }

        // Factory reset, debounce and sleep-time keys: take effect after being
        // held for 3 s (User_Func_Key_Long_Press runs the action).
        if (Key_Reset_Status || Key_Debounce_Status || Key_Sleep_Time_Status) {
            Func_Time_3s_Count++;
            if (Func_Time_3s_Count >= 300) {
                Func_Time_3s_Count  = 0;
                Func_Key_Long_Press = true;
            }
        }
    }

    // Poll the radio's status every Spi_Interval ticks.
    Systick_Interval_Count++;
    if (Systick_Interval_Count >= Spi_Interval) {
        Systick_Interval_Count = 0;

        if (Keyboard_Status.System_Work_Status == 0) {
            if (Spi_Ack_Send_Command(USER_GET_RF_STATUS) == SPI_NACK) {
                Systick_Interval_Count = Spi_Interval - 10;
            }
        }
    }

    OSAL_IRQ_EPILOGUE();
}
