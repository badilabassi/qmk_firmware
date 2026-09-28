// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rdmctmzt_common.h"

/**************************EMI****************************/
bool Emi_Test_Start = false;

/**************************EMI****************************/
void Emi_Init(void) {
    Spi_Interval                         = SPI_DELAY_USB_TIME;
    Keyboard_Status.System_Work_Status   = 0;
    Keyboard_Status.System_Sleep_Mode    = 0;
    Mode_Synchronization_Signal          = false;
    Led_Rf_Pair_Flg                      = false;
    Ble_Name_Spi_Send                    = false;
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
