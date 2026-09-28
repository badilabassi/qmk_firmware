// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"
#include "raw_hid.h"
#include "usb_main.h"

/************************IO pins**************************/
/************************IO pins**************************/
/************************IO pins**************************/
#define WHEEL_ZA_IO         (B2)
#define WHEEL_ZB_IO         (B10)

#define ES_BATT_ADC_IO      (C4)
#define ES_BATT_STDBY_IO    (A13)
#define ES_USB_POWER_IO     (C5)
#define ES_SPI_ACK_IO       (A4)
#define ES_PWM_DMA_IO       (A2)

#define ES_WAKEUP_IO        (D1)
#define ES_SDB_POWER_IO     (A3)
#define ES_LED_POWER_IO     (D0)

/************************SPI commands**************************/
/************************SPI commands**************************/
/************************SPI commands**************************/
#define USER_EMI_COMMAND	    0XBB
#define USER_KEYBOARD_COMMAND	0X0A
#define USER_KEYBOARD_LENGTH    (64)

#define USER_SWITCH_2P4G_MODE	0X00
#define USER_SWITCH_BLE_1_MODE	0X01
#define USER_SWITCH_BLE_2_MODE	0X02
#define USER_SWITCH_BLE_3_MODE	0X03
#define USER_SWITCH_2P4G_PAIR	0X04
#define USER_SWITCH_BLE_1_PAIR	0X05
#define USER_SWITCH_BLE_2_PAIR	0X06
#define USER_SWITCH_BLE_3_PAIR	0X07
#define USER_SWITCH_USB_MODE	0X08

#define USER_KEYBOARD_SLEEP		0X09
#define USER_KEYBOARD_WAKEUP	0X0A

#define USER_KEY_BYTE_DATA		0X0B
#define USER_KEY_BIT_DATA		0X0C
#define USER_MOUSE_DATA			0X0D
#define USER_CONSUMER_DATA		0X0E
#define USER_SYSTEM_DATA		0X0F

#define USER_BATTERY_DATA		0X10

#define USER_GET_RF_STATUS	    0X11

#define USER_BLE1_WRITE_NAME	0X12
#define USER_BLE2_WRITE_NAME    0X13
#define USER_BLE3_WRITE_NAME    0X14


#define USER_KEY_BYTE_LENGTH	0X08
#define USER_KEY_BIT_LENGTH		0X0F
#define USER_MOUSE_LENGTH		0X08
#define USER_CONSUMER_LENGTH	0X03
#define USER_SYSTEM_LENGTH		0X03
#define USER_BATTERY_LENGTH		0X02

#define KB_REPORT_ID            0x06                 // Extend keyboard report ID.
#define SYS_REPORT_ID     	    0x03                 // Extend System   report ID.
#define CON_REPORT_ID     	    0x04                 // Extend Consumer report ID.
#define MOUSE_REPORT_ID  	    0x02                 // Extend mouse	report ID.

#define LOGO_LED_ENABLE         (1)
#define SIDE_LED_ENABLE         (0)

/************************Core definitions**************************/
/************************Core definitions**************************/
/************************Core definitions**************************/
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
    KB_MODE_CONNECT_OK,  	        // connected
    KB_MODE_CONNECT_PAIR,	        // pairing
    KB_MODE_CONNECT_RETURN,	        // reconnecting
} keyboard_System_state_e;

typedef enum {
    USER_SLEEP_PASS,	            // sleep succeeded
    USER_SLEEP_FAIL,	            // sleep failed
} keyboard_System_Sleep_Status_s;

/************************Default work mode**************************/
/************************Default work mode**************************/
/************************Default work mode**************************/
#define INIT_WORK_MODE          (QMK_USB_MODE)       // default work mode
#define INIT_BLE_CHANNEL        (QMK_BLE_CHANNEL_1)  // default Bluetooth channel
#define INIT_BATT_NUMBER        (50)                 // default battery level at power-up

#define INIT_SIX_KEY            (0)                  // 6-key rollover
#define INIT_ALL_KEY            (1)                  // N-key rollover
#define INIT_ALL_SIX_KEY        (INIT_ALL_KEY)       // N-key rollover

#define INIT_WIN_MODE           (0)                  // Windows
#define INIT_MAC_MODE           (1)                  // Mac
#define INIT_WIN_MAC_MODE       (INIT_WIN_MODE)      // Windows

#define INIT_WIN_NLOCK          (0)                  // Win key not locked
#define INIT_WIN_LOCK           (1)                  // Win key locked
#define INIT_WIN_LOCK_NLOCK     (INIT_WIN_NLOCK)     // Win key not locked

#define INIT_ALL_LED_ON         (0)                  // lighting on
#define INIT_ALL_LED_OFF        (1)                  // all lighting (keys and logo) off

#define SLEEP_TIME_1MIN         (60)                 // sleep timeouts, in seconds
#define SLEEP_TIME_3MIN         (180)
#define SLEEP_TIME_10MIN        (600)
#define SLEEP_TIME_30MIN        (1800)
#define INIT_SLEEP_TIME         (SLEEP_TIME_3MIN)

#define INIT_RF_TIMER_2         (0xFFFFFFFE)         // firmware 0.1.5 default

#define DEBOUNCE_FAST           (2)                  // debounce times, in ms
#define DEBOUNCE_SLOW           (5)
#define INIT_DEBOUNCE           (DEBOUNCE_SLOW)

// Keycode order follows firmware 0.1.5, so the values match Womier's VIA definition.
#define USER_DEFINE_KEY         (QK_KB)
enum Custom_Keycodes {
    QMK_KB_MODE_2P4G = USER_DEFINE_KEY,
    QMK_KB_MODE_BLE1,
    QMK_KB_MODE_BLE2,
    QMK_KB_MODE_BLE3,
    QMK_KB_MODE_USB,
    QMK_BATT_NUM,
    QMK_WIN_LOCK,
    QMK_KB_SIX_N_CH,
    QMK_TEST_COLOUR,                        // +
    QMK_SLEEP_TIME,                         // hold 3 s: next sleep timeout
    QMK_RF_TIMER_2_ADD,                     // adds 15 to the second radio timer (not in the default keymap)
    QMK_DEBOUNCE,                           // hold 3 s: toggle 2 ms / 5 ms debounce
    QMK_ALL_LED_TOG,                        // all lighting on/off
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

#define WIN_COL                 (1)
#define WIN_ROW                 (3)

#define MAC_COL                 (2)
#define MAC_ROW                 (3)

#define KC_K29 	KC_BACKSLASH
#define KC_K42 	KC_NONUS_HASH
#define KC_K45 	KC_NONUS_BACKSLASH
#define KC_K56 	KC_INTERNATIONAL_1
#define KC_K14  KC_INTERNATIONAL_3
#define KC_K132	KC_INTERNATIONAL_4
#define KC_K131	KC_INTERNATIONAL_5
#define KC_K133	KC_INTERNATIONAL_2
#define KC_K151	KC_LANGUAGE_1
#define KC_K150	KC_LANGUAGE_2

#define MD_24G	QMK_KB_MODE_2P4G
#define MD_BLE1	QMK_KB_MODE_BLE1
#define MD_BLE2	QMK_KB_MODE_BLE2
#define MD_BLE3	QMK_KB_MODE_BLE3
#define MD_USB	QMK_KB_MODE_USB
#define QK_BAT  QMK_BATT_NUM
#define QK_WLO	QMK_WIN_LOCK
#define SIX_N	QMK_KB_SIX_N_CH
#define TEST_CL	QMK_TEST_COLOUR     // +
#define SLP_TIM	QMK_SLEEP_TIME
#define DEB_TOG	QMK_DEBOUNCE
#define LED_TOG	QMK_ALL_LED_TOG

/************************Basic variables**************************/
/************************Basic variables**************************/
/************************Basic variables**************************/
// Saved in flash; the layout matches firmware 0.1.5.
typedef struct {
    uint8_t  Key_Mode;              // keyboard work mode
    uint8_t  Ble_Channel;           // Bluetooth channel
    uint8_t  Batt_Number;           // battery level
    uint8_t  Nkro;                  // 6-key / N-key rollover
    uint8_t  Mac_Win_Mode;          // Mac or Windows mode
    uint8_t  Win_Lock;              // Win key lock
    uint8_t  All_Led_Off;           // all lighting (keys and logo) off
    uint8_t  Reserved;
    uint32_t Sleep_Time;            // radio sleep timeout, in seconds
    uint32_t Rf_Timer_2;            // second timer kept by the radio; meaning unknown
    uint8_t  Debounce;              // debounce time, in ms
#if LOGO_LED_ENABLE
    uint8_t Logo_On_Off;            // logo light on/off
    uint8_t Logo_Mode;              // logo light mode
    uint8_t Logo_Colour;            // logo light colour
    uint8_t Logo_Saturation;        // logo light saturation
    uint8_t Logo_Brightness;        // logo light brightness
    uint8_t Logo_Speed;             // logo light speed
#endif
#if SIDE_LED_ENABLE
    uint8_t Side_On_Off;            // side light on/off
    uint8_t Side_Mode;              // side light mode
    uint8_t Side_Colour;            // side light colour
    uint8_t Side_Saturation;        // side light saturation
    uint8_t Side_Brightness;        // side light brightness
    uint8_t Side_Speed;             // side light speed
#endif
} Keyboard_Info_t;
extern Keyboard_Info_t Keyboard_Info;
#if LOGO_LED_ENABLE && !SIDE_LED_ENABLE
_Static_assert(sizeof(Keyboard_Info_t) == 24, "Keyboard_Info_t must keep the firmware 0.1.5 layout");
#endif

typedef struct {
    uint8_t System_Work_Status;     // system status
    uint8_t System_Work_Mode;       // work mode
    uint8_t System_Work_Channel;    // work channel
    uint8_t System_Connect_Status;  // connection status
    uint8_t System_Led_Status;      // system indicator LED
    uint8_t System_Sleep_Mode;      // system sleep
} Keyboard_Status_t;
extern Keyboard_Status_t Keyboard_Status;

bool     Key_2p4g_Status;
bool     Key_Ble_1_Status;
bool     Key_Ble_2_Status;
bool     Key_Ble_3_Status;
bool     Key_Fn_Status;
bool     Key_Reset_Status;
bool     Key_Debounce_Status;
bool     Key_Sleep_Time_Status;
bool     Func_Key_Long_Press;
uint8_t  Systick_6ms_Count;
uint8_t  Systick_10ms_Count;
uint16_t Systick_Interval_Count;
uint16_t Time_3s_Count;
uint16_t Func_Time_3s_Count;

/************************Debounce**************************/
/************************Debounce**************************/
/************************Debounce**************************/
uint8_t Debounce_Time;                  // used by rd75_debounce.c

/************************Data queue**************************/
/************************Data queue**************************/
/************************Data queue**************************/
#define APP_2G4_BUF_SIZE            (USER_KEYBOARD_LENGTH)
#define APP_2G4_BUF_CNT             (20)

uint8_t app_2g4_data[APP_2G4_BUF_CNT][APP_2G4_BUF_SIZE];
volatile uint8_t app_2g4_data_send;
volatile uint8_t app_2g4_data_rev;

uint8_t app_2g4_buffer_full(void);
uint8_t app_2g4_buffer_empty(void);
void app_2g4_buffer_rev_add(void);
void app_2g4_buffer_send_add(void);

/**************************EMI****************************/
/**************************EMI****************************/
/**************************EMI****************************/
bool Emi_Test_Start;
void Emi_Init(void);
void Emi_Read_Data(uint8_t *User_Data, uint8_t User_Length);
void Emi_Write_Data(uint8_t *User_Data, uint8_t User_Length);

/**************************SPI****************************/
/**************************SPI****************************/
/**************************SPI****************************/
#define SPI_DELAY_RF_TIME           (60)
#define SPI_DELAY_USB_TIME          (500 * 3)

#define MAX_NAME_LEN                (18)
#define USER_BLE_ID                 (0X0001)
#define USER_BLE1_NAME              "RD 75-1"
#define USER_BLE2_NAME              "RD 75-2"
#define USER_BLE3_NAME              "RD 75-3"

volatile uint8_t Spi_Send_Recv_Flg;
uint16_t Spi_Interval;
uint8_t  g_es_spi_rx_buf[64];
uint8_t  g_es_spi_tx_buf[64];
uint8_t  Repeat_Send_Count;
uint8_t  Send_Key_Type;
bool     Init_Spi_Power_Up;
uint8_t  Init_Spi_100ms_Delay;
bool     Ble_Name_Spi_Send;
uint8_t  Ble_Name_Spi_Count;
bool     Spi_Sync_Request;
uint8_t  Spi_Ble_Send_Count;

const uint32_t g_es_dma_ch2pri_cfg;
const uint32_t g_es_dma_ch2alt_cfg;
const md_spi_inittypedef SPI2_InitStruct;

void es_ble_spi_init(void);
void es_ble_spi_deinit(void);
void es_spi_send_recv_by_dma(uint32_t num, uint8_t *rx_buf, uint8_t *tx_buf);
void Spi_Main_Loop(void);
void Spi_Send_Command(uint8_t Command);
uint8_t Spi_Ack_Send_Command(uint8_t Command);
void Get_Spi_Return_Data(uint8_t *Data);

/**************************Mode switching****************************/
/**************************Mode switching****************************/
/**************************Mode switching****************************/
volatile host_driver_t *es_qmk_driver;
const    host_driver_t es_user_driver;

uint8_t es_keyboard_leds(void);
void es_send_keyboard(report_keyboard_t *report);
void es_send_nkro(report_nkro_t *report);
void es_send_mouse(report_mouse_t *report);
void es_send_extra(report_extra_t *report);
void Mode_Synchronization(void);
void Ble_Name_Synchronization(void);
void Spi_Synchronization(void);
void User_bluetooth_send_keyboard(uint8_t *report, uint32_t len);

/**************************System functions****************************/
/**************************System functions****************************/
/**************************System functions****************************/
#define KEYBOARD_COL                (16)
#define KEYBOARD_ROW                (7)

#define MATRIX_USER_COL_PINS        { D15, D14, C15, C14, C13, D3, D2, C12, C11, C10, A14, C9, C8, C7, C6, B15 }
#define MATRIX_USER_ROW_PINS        { B0, B3, B4, B5, B6, B7, B1 }

bool     Save_Flash;
bool     Reset_Save_Flash;
uint16_t Save_Flash_3S_Count;
bool     Led_Rf_Pair_Flg;
bool     Usb_Change_Mode_Wakeup;
uint8_t  Temp_System_Led_Status;
bool     Mode_Synchronization_Signal;
uint16_t g_usb_sof_frame_id;
uint16_t g_usb_sof_frame_id_last;
bool     Usb_Dis_Connect;
uint16_t Usb_Suspend_Delay;
uint16_t Usb_Change_Mode_Delay;

void es_mcu_reset(void);
void bootloader_jump(void);
void mcu_reset(void);
void User_Keyboard_Reset(void);
void User_Debounce_Toggle(void);
void User_Sleep_Time_Next(void);
void User_Func_Key_Long_Press(void);
void Save_Flash_Set(void);
void User_Systime_Init(void);
void User_Systime_Deinit(void);
void Init_Gpio_Information(void);
void User_Sleep(void);
void User_Wakeup(void);
void Board_Wakeup_Init(void);
void es_chibios_user_idle_loop_hook(void);
void Init_Keyboard_Information(void);
void es_change_qmk_nkro_mode_enable(void);
void es_change_qmk_nkro_mode_disable(void);
void User_Keyboard_Init(void);
void User_Keyboard_Post_Init(void);

/************************USB plug-in**************************/
/************************USB plug-in**************************/
/************************USB plug-in**************************/
void User_Usb_Init(void);
void es_restart_usb_driver(void);
void Usb_Disconnect(void);
void User_Usb_Deinit(void);

/**************************FLASH****************************/
/**************************FLASH****************************/
/**************************FLASH****************************/
void eeprom_driver_init(void);
void eeprom_write_block_user(const void *buf, void *addr, size_t len);
void eeprom_read_block_user(void *buf, const void *addr, size_t len);

/************************Battery******************************/
/************************Battery******************************/
/************************Battery******************************/
#define USER_BATT_POWER_SCAN_COUNT  (10)
#define USER_BATT_SCAN_COUNT        (10)

#define USER_BATT_HIGH_POWER        (2555)      // full:     2555 * 3.3 / 4096 = 2.058 V -> 4.12 V   The real circuit has a voltage drop:
#define USER_BATT_LOW_POWER         (2065)      // low:      2065 * 3.3 / 4096 = 1.663 V -> 3.32 V   even with the keyboard lights off, a full 4.2 V battery
#define USER_BATT_SHUTDOWN_POWER      (1865)      // shutdown: 1865 * 3.3 / 4096 = 1.502 V -> 3.04 V   only delivers about 4.1 V to the board.

#define USER_BATT_DELAY_TIME        (100 * 25)  //25S
#define USER_TIME_3S_TIME           (100 * 3)   //3S

uint16_t User_Adc_Batt[USER_BATT_SCAN_COUNT];
uint16_t User_Scan_Batt[USER_BATT_SCAN_COUNT];
uint8_t  User_Adc_Batt_Count;
uint8_t  User_Batt_BaiFen;
uint8_t  User_Batt_Old_BaiFen;
uint8_t  User_Batt_10ms_Count;
uint16_t User_Batt_Time_15S_Count;
bool     User_Batt_Power_Up;
bool     User_Batt_Send_Spi;
uint16_t User_Batt_Power_Up_Delay_100ms_Count;
bool     User_Batt_Power_Up_Delay;
bool     User_Power_Low;
uint8_t  User_Power_Low_Count;
uint8_t  es_stdby_pin_state;
bool     User_Key_Batt_Num_Show;
uint8_t  User_Key_Batt_Count;
uint8_t  Batt_Led_Count;

const md_adc_initial adc_initStruct;

void Init_Batt_Information(void);
void User_Adc_Init(void);
void User_Adc_Deinit(void);
void U16_Buff_Clear(uint16_t *Buff, uint8_t Len);
void User_Adc_Batt_Power_Up_Init(void);
void User_Adc_Batt_Number(void);

/************************MCU-driven LEDs**************************/
/************************MCU-driven LEDs**************************/
/************************MCU-driven LEDs**************************/
#define ES_PWM_LED_SIZE         (42)
#define ES_PWM_LED_BYTE         (24)
#define ES_PWM_DMA_SIZE         (ES_PWM_LED_SIZE * ES_PWM_LED_BYTE)

#define ES_PWM_WS2812_H_VALUE   (43)
#define ES_PWM_WS2812_L_VALUE   (17)

#define U_PWM                   (RGB_MATRIX_MAXIMUM_BRIGHTNESS)

#define LED_CAP_INDEX       (45)
#define LED_CAP_1_INDEX     (46)
#define LED_WIN_L_INDEX     (75)

#define LED_BLE_1_INDEX     (16)
#define LED_BLE_2_INDEX     (17)
#define LED_BLE_3_INDEX     (18)
#define LED_2P4G_INDEX      (19)
#define LED_USB_INDEX       (20)

uint8_t Led_Colour_Tab[9][3];
uint8_t Led_Wave_Pwm_Tab[128];
uint8_t Led_Batt_Index_Tab[10];

uint8_t  Systick_Led_Count;
// uint8_t  Point_Flash_Count;		   // -
uint8_t  Led_Point_Count;
uint8_t  Mac_Win_Point_Count;
uint8_t  Debounce_Point_Count;
uint8_t  Sleep_Time_Point_Count;
bool     Led_Flash_Busy;
bool     Led_Off_Start;
bool     Led_Power_Off;                 // nothing lit: keep the LED supply off
bool     Led_Power_Up;
uint16_t Led_Power_Up_Delay;
bool     Usb_If_Ok_Led;
bool     Test_Led;                      // +
uint8_t  Test_Colour;                   // +

rgb_led_t rgb_matrix_ws2812_array[RGB_MATRIX_LED_COUNT];
uint8_t g_es_pwm_rgb_matrix_array_dma_buf[(RGB_MATRIX_LED_COUNT * ES_PWM_LED_BYTE) + 2];
md_dma_channel_config_typedef DMA_list[5];

void  rgb_matrix_driver_init(void);
void  User_Pwm_Deinit(void);
void  rgb_matrix_driver_flush_pwm_dma_start(void);
void  rgb_matrix_driver_flush(void);
void  rgb_matrix_driver_set_color(int index, uint8_t r, uint8_t g, uint8_t b);
void  rgb_matrix_driver_set_color_all(uint8_t r, uint8_t g, uint8_t b);
const rgb_matrix_driver_t rgb_matrix_driver;

void Led_All_Off_Show(void);
void Led_Power_Low_Show(void);
void Led_Rf_Mode_Show(void);
void Led_Batt_Number_Show(void);
void Led_Point_Flash_Show(void);
void User_Point_Show(void);
void User_Led_Show(void);
void User_Test_Colour_Show(void);   // +

/************************Side lights**************************/
/************************Side lights**************************/
/************************Side lights**************************/
#if LOGO_LED_ENABLE
#define LOGO_LED_PLAY_SPEED	        (0)                                                     // light refresh speed
#define LOGO_LED_SIZE	            (4)                                                     // number of LEDs

#define LOGO_LED_ON                 (0)                                                     // light on
#define LOGO_LED_OFF                (1)                                                     // light off

#define LOGO_WAVE_RGB_MODE          (1)                                                     // rainbow wave
#define LOGO_WAVE_DS_MODE           (2)                                                     // single-colour wave
#define LOGO_SPECTRUM_MODE          (3)                                                     // spectrum
#define LOGO_BREATH_MODE            (4)                                                     // single-colour breathing
#define LOGO_LIGHT_MODE             (5)                                                     // single-colour static
#define LOGO_OFF_MODE               (6)                                                     // off

#define LOGO_MAX_COLOUR             (255)                                                   // maximum colour
#define LOGO_MIN_COLOUR             (0)                                                     // minimum colour
#define COLOUR_LEVEL                (15)                                                    // colour step

#define LOGO_MAX_SATURATION         (0)                                                     // maximum saturation
#define LOGO_MIN_SATURATION         (255)                                                   // minimum saturation
#define SATURATION_LEVEL            (15)                                                    // saturation step

#define LOGO_MAX_BRIGHTNESS         (RGB_MATRIX_MAXIMUM_BRIGHTNESS)                         // maximum brightness
#define LOGO_MIN_BRIGHTNESS         (0)                                                     // minimum brightness
#define BRIGHTNESS_LEVEL            (15)                                                    // brightness step

#define LOGO_MAX_SPEED              (4)                                                     // maximum speed
#define LOGO_MIN_SPEED              (0)                                                     // minimum speed
#define SPEED_LEVEL                 (1)                                                     // speed step

#define INIT_LOGO_ON_OFF            (LOGO_LED_ON)                                           // light on
#define INIT_LOGO_MODE              (LOGO_WAVE_RGB_MODE)                                    // rainbow wave
#define INIT_LOGO_COLOUR            (LOGO_MIN_COLOUR)                                       // minimum colour
#define INIT_LOGO_SATURATION        (LOGO_MAX_SATURATION)                                   // maximum saturation
#define INIT_LOGO_BRIGHTNESS        (LOGO_MAX_BRIGHTNESS)                                   // maximum brightness
#define INIT_LOGO_SPEED             (2)                                                     // medium speed

uint8_t Logo_Flash_Count;
uint8_t Logo_Led_Count;
uint8_t LED_Mix_Colour_Tab[256][3];
uint8_t Logo_Index_Tab[LOGO_LED_SIZE];
void Logo_Init(void);
void Logo_Mode_Show(void);
#if defined(VIA_ENABLE)
void User_Via_Qmk_Logo_Get_Value(uint8_t *data);
void User_Via_Qmk_Logo_Set_Value(uint8_t *data);
void User_Via_Qmk_Logo_Command(uint8_t *data, uint8_t length);
#endif

#endif
//--------------------------------------------------------------------------------------------------------
#if SIDE_LED_ENABLE
#define SIDE_LED_PLAY_SPEED	        (0)                                                     // light refresh speed
#define SIDE_LED_SIZE	            (38)                                                    // number of LEDs

#define SIDE_LED_ON                 (0)                                                     // light on
#define SIDE_LED_OFF                (1)                                                     // light off

#define SIDE_WAVE_RGB_MODE          (1)                                                     // rainbow wave
#define SIDE_WAVE_DS_MODE           (2)                                                     // single-colour wave
#define SIDE_SPECTRUM_MODE          (3)                                                     // spectrum
#define SIDE_BREATH_MODE            (4)                                                     // single-colour breathing
#define SIDE_LIGHT_MODE             (5)                                                     // single-colour static
#define SIDE_OFF_MODE               (6)                                                     // off

#define SIDE_MAX_COLOUR             (255)                                                   // maximum colour
#define SIDE_MIN_COLOUR             (0)                                                     // minimum colour
#define SIDE_COLOUR_LEVEL           (15)                                                    // colour step

#define SIDE_MAX_SATURATION         (0)                                                     // maximum saturation
#define SIDE_MIN_SATURATION         (255)                                                   // minimum saturation
#define SIDE_SATURATION_LEVEL       (15)                                                    // saturation step

#define SIDE_MAX_BRIGHTNESS         (RGB_MATRIX_MAXIMUM_BRIGHTNESS)                         // maximum brightness
#define SIDE_MIN_BRIGHTNESS         (0)                                                     // minimum brightness
#define SIDE_BRIGHTNESS_LEVEL       (15)                                                    // brightness step

#define SIDE_MAX_SPEED              (4)                                                     // maximum speed
#define SIDE_MIN_SPEED              (0)                                                     // minimum speed
#define SIDE_SPEED_LEVEL            (1)                                                     // speed step

#define INIT_SIDE_ON_OFF            (SIDE_LED_ON)                                           // light on
#define INIT_SIDE_MODE              (SIDE_WAVE_RGB_MODE)                                    // rainbow wave
#define INIT_SIDE_COLOUR            (SIDE_MIN_COLOUR)                                       // minimum colour
#define INIT_SIDE_SATURATION        (SIDE_MAX_SATURATION)                                   // maximum saturation
#define INIT_SIDE_BRIGHTNESS        (SIDE_MAX_BRIGHTNESS)                                   // maximum brightness
#define INIT_SIDE_SPEED             (2)                                                     // medium speed

uint8_t Side_Flash_Count;
uint8_t Side_Led_Count;
uint8_t Side_Index_Tab[SIDE_LED_SIZE];
void Side_Init(void);
void Side_Mode_Show(void);
void User_Via_Qmk_Side_Get_Value(uint8_t *data);
void User_Via_Qmk_Side_Set_Value(uint8_t *data);
void User_Via_Qmk_Side_Command(uint8_t *data, uint8_t length);

#endif
