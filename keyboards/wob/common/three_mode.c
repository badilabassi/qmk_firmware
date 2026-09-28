// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rdmctmzt_common.h"

/**************************Mode switching****************************/
volatile host_driver_t *es_qmk_driver = NULL;

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

#ifdef RAW_ENABLE
void es_send_raw_hid(uint8_t *data, uint8_t length) {
    // Raw HID is available only over the wired USB transport. Do not forward
    // browser/Vial traffic while the keyboard is switching to a radio mode.
    if ((Keyboard_Info.Key_Mode == QMK_USB_MODE) && (es_qmk_driver != NULL) && (es_qmk_driver->send_raw_hid != NULL)) {
        es_qmk_driver->send_raw_hid(data, length);
    }
}
#endif

const host_driver_t es_user_driver = {
    .keyboard_leds = es_keyboard_leds,
    .send_keyboard = es_send_keyboard,
    .send_nkro     = es_send_nkro,
    .send_mouse    = es_send_mouse,
    .send_extra    = es_send_extra,
#ifdef RAW_ENABLE
    .send_raw_hid  = es_send_raw_hid,
#endif
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
