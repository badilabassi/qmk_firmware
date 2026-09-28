// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rdmctmzt_common.h"

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

