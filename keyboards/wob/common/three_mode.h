// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"

extern volatile host_driver_t *es_qmk_driver;
extern const host_driver_t     es_user_driver;

uint8_t es_keyboard_leds(void);
void    es_send_keyboard(report_keyboard_t *report);
void    es_send_nkro(report_nkro_t *report);
void    es_send_mouse(report_mouse_t *report);
void    es_send_extra(report_extra_t *report);
#ifdef RAW_ENABLE
void    es_send_raw_hid(uint8_t *data, uint8_t length);
#endif
void    Mode_Synchronization(void);
void    Ble_Name_Synchronization(void);
void    Spi_Synchronization(void);
void    User_bluetooth_send_keyboard(uint8_t *report, uint32_t len);

void User_Usb_Init(void);
void es_restart_usb_driver(void);
void Usb_Disconnect(void);
void User_Usb_Deinit(void);
