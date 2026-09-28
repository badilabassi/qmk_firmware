// Copyright 2017 Alex Ong <the.onga@gmail.com>
// Copyright 2020 Andrei Purdea <andrei@purdea.ro>
// Copyright 2021 Simon Arlott
// SPDX-License-Identifier: GPL-2.0-or-later
//
// QMK's asym_eager_defer_pk debounce (quantum/debounce/asym_eager_defer_pk.c)
// with the debounce time read at run time from Debounce_Time, which Fn+H
// switches between 2 and 5 ms (never 0, see Init_Keyboard_Information).
// Upstream takes DEBOUNCE at compile time.
//
// Asymetric per-key algorithm. After pressing a key, it immediately changes state,
// with no further inputs accepted until the debounce time has elapsed. After
// releasing a key, that state is pushed after no changes occur for the debounce time.

#include "debounce.h"
#include "timer.h"
#include "util.h"
#include "rdmctmzt_common.h"

// Maximum debounce: 127ms
#define DEBOUNCE_MAX 127

#define DEBOUNCE_ELAPSED 0

typedef struct {
    bool    pressed : 1;
    uint8_t time : 7;
} debounce_counter_t;

// Uses MATRIX_ROWS_PER_HAND instead of MATRIX_ROWS to support split keyboards
static debounce_counter_t debounce_counters[MATRIX_ROWS_PER_HAND * MATRIX_COLS] = {DEBOUNCE_ELAPSED};
static bool               counters_need_update;
static bool               matrix_need_update;
static bool               cooked_changed;

static inline void update_debounce_counters_and_transfer_if_expired(matrix_row_t raw[], matrix_row_t cooked[], uint8_t elapsed_time);
static inline void transfer_matrix_values(matrix_row_t raw[], matrix_row_t cooked[], uint8_t debounce_time);

void debounce_init(void) {}

bool debounce(matrix_row_t raw[], matrix_row_t cooked[], bool changed) {
    static fast_timer_t last_time;
    bool                updated_last  = false;
    uint8_t             debounce_time = MIN(Debounce_Time, DEBOUNCE_MAX);
    cooked_changed                    = false;

    if (counters_need_update) {
        fast_timer_t now          = timer_read_fast();
        fast_timer_t elapsed_time = TIMER_DIFF_FAST(now, last_time);

        last_time    = now;
        updated_last = true;

        if (elapsed_time > 0) {
            // Update debounce counters with elapsed timer clamped to 127 (maximum debounce)
            update_debounce_counters_and_transfer_if_expired(raw, cooked, MIN(elapsed_time, DEBOUNCE_MAX));
        }
    }

    if (changed || matrix_need_update) {
        if (!updated_last) {
            last_time = timer_read_fast();
        }

        transfer_matrix_values(raw, cooked, debounce_time);
    }

    return cooked_changed;
}

/**
 * @brief Processes per-key debounce counters and updates the debounced matrix state.
 *
 * This function iterates through each key in the matrix and updates its debounce counter
 * based on the elapsed time. If the debounce period has expired, the debounced state is
 * updated accordingly for key-down (eager) and key-up (defer) events.
 *
 * @param raw The current raw key state matrix.
 * @param cooked The debounced key state matrix to be updated.
 * @param elapsed_time The time elapsed since the last debounce update, in milliseconds.
 */
static inline void update_debounce_counters_and_transfer_if_expired(matrix_row_t raw[], matrix_row_t cooked[], uint8_t elapsed_time) {
    counters_need_update = false;
    matrix_need_update   = false;

    for (uint8_t row = 0; row < MATRIX_ROWS_PER_HAND; row++) {
        uint16_t row_offset = row * MATRIX_COLS;

        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            uint16_t index = row_offset + col;

            if (debounce_counters[index].time != DEBOUNCE_ELAPSED) {
                if (debounce_counters[index].time <= elapsed_time) {
                    debounce_counters[index].time = DEBOUNCE_ELAPSED;

                    if (debounce_counters[index].pressed) {
                        // key-down: eager
                        matrix_need_update = true;
                    } else {
                        // key-up: defer
                        matrix_row_t col_mask    = (MATRIX_ROW_SHIFTER << col);
                        matrix_row_t cooked_next = (cooked[row] & ~col_mask) | (raw[row] & col_mask);
                        cooked_changed |= cooked_next ^ cooked[row];
                        cooked[row] = cooked_next;
                    }
                } else {
                    debounce_counters[index].time -= elapsed_time;
                    counters_need_update = true;
                }
            }
        }
    }
}

/**
 * @brief Applies debounced changes to the matrix state based on per-key counters.
 *
 * This function compares the raw and cooked key state matrices to detect changes.
 * For each key, it updates the debounce counter and the debounced state according
 * to the debounce algorithm. Key-down events are handled eagerly, while key-up
 * events are deferred until the debounce period has elapsed.
 *
 * @param raw The current raw key state matrix.
 * @param cooked The debounced key state matrix to be updated.
 * @param debounce_time The debounce time, in milliseconds.
 */
static inline void transfer_matrix_values(matrix_row_t raw[], matrix_row_t cooked[], uint8_t debounce_time) {
    matrix_need_update = false;

    for (uint8_t row = 0; row < MATRIX_ROWS_PER_HAND; row++) {
        uint16_t     row_offset = row * MATRIX_COLS;
        matrix_row_t delta      = raw[row] ^ cooked[row];

        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            uint16_t     index    = row_offset + col;
            matrix_row_t col_mask = (MATRIX_ROW_SHIFTER << col);

            if (delta & col_mask) {
                if (debounce_counters[index].time == DEBOUNCE_ELAPSED) {
                    debounce_counters[index].pressed = (raw[row] & col_mask);
                    debounce_counters[index].time    = debounce_time;
                    counters_need_update             = true;

                    if (debounce_counters[index].pressed) {
                        // key-down: eager
                        cooked[row] ^= col_mask;
                        cooked_changed = true;
                    }
                }
            } else if (debounce_counters[index].time != DEBOUNCE_ELAPSED) {
                if (!debounce_counters[index].pressed) {
                    // key-up: defer
                    debounce_counters[index].time = DEBOUNCE_ELAPSED;
                }
            }
        }
    }
}
