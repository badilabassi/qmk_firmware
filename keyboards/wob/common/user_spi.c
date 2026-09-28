// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rdmctmzt_common.h"

/************************Data queue**************************/
uint8_t          app_2g4_data[APP_2G4_BUF_CNT][APP_2G4_BUF_SIZE] = {0};
volatile uint8_t app_2g4_data_send = 0;
volatile uint8_t app_2g4_data_rev  = 0;

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
