// Copyright 2026 Badi Labassi
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// The RD75's flash-backed EEPROM driver (rdr_common.c) never stores offsets
// 1088 and up (see ee_write_variable), so keep VIA's macros below them.
#define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR 1087
