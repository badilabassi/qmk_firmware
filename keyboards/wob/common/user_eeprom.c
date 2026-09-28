// Copyright 2023 Finalkey
// Copyright 2023 LiWenLiu <https://github.com/LiuLiuQMK>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rdmctmzt_common.h"

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
