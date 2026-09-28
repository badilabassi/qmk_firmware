// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"

#define KEYBOARD_COL (16)
#define KEYBOARD_ROW (7)

#define MATRIX_USER_COL_PINS {D15, D14, C15, C14, C13, D3, D2, C12, C11, C10, A14, C9, C8, C7, C6, B15}
#define MATRIX_USER_ROW_PINS {B0, B3, B4, B5, B6, B7, B1}

extern bool     Save_Flash;
extern bool     Reset_Save_Flash;
extern uint16_t Save_Flash_3S_Count;
extern bool     Led_Rf_Pair_Flg;
extern bool     Usb_Change_Mode_Wakeup;
extern uint8_t  Temp_System_Led_Status;
extern bool     Mode_Synchronization_Signal;
extern uint16_t g_usb_sof_frame_id;
extern uint16_t g_usb_sof_frame_id_last;
extern bool     Usb_Dis_Connect;
extern uint16_t Usb_Suspend_Delay;
extern uint16_t Usb_Change_Mode_Delay;

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
