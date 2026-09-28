// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rdmctmzt_common.h"

#if defined(DYNAMIC_KEYMAP_ENABLE)
#    include "dynamic_keymap.h"
#endif

// Same value QMK uses privately in tmk_core/protocol/chibios/chibios.c.
#define USB_GETSTATUS_REMOTE_WAKEUP_ENABLED (2U)

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
