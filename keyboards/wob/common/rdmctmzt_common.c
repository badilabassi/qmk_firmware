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
 * binary): selectable debounce, an all-lighting toggle, the Caps Lock indicator
 * on the logo, LED supply cut when nothing is lit, and the 24-byte settings
 * layout. The 0.1.5 radio-timer protocol is intentionally disabled because the
 * production RD75 radio firmware does not support it and stops carrying HID
 * reports when either timer command is synchronized.
 */

#include "rdmctmzt_common.h"

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
