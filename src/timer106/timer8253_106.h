#ifndef FM1JUNO_TIMER8253_106_H
#define FM1JUNO_TIMER8253_106_H

#include <stdbool.h>
#include <stdint.h>

#include "common/fixed_time.h"

typedef enum TimerRange106 {
    TIMER106_RANGE_16 = 0,
    TIMER106_RANGE_8,
    TIMER106_RANGE_4
} TimerRange106;

typedef struct Timer8253Voice106 {
    uint16_t current_count;
    uint16_t pending_count;
    uint32_t clocks_remaining;
    bool pending_valid;
    bool output_high;
    TimerRange106 range;
    Fm1junoTimeQ32 next_edge_time;
} Timer8253Voice106;

uint32_t timer8253_106_effective_count(uint16_t count_register);
uint32_t timer8253_106_high_clocks(uint16_t count_register);
uint32_t timer8253_106_low_clocks(uint16_t count_register);
uint32_t timer8253_106_input_clock_hz(TimerRange106 range);
Fm1junoTimeQ32 timer8253_106_clock_q32(TimerRange106 range);
Fm1junoTimeQ32 timer8253_106_clocks_to_time_q32(TimerRange106 range,
                                                uint32_t clocks);

void timer8253_106_reset_and_load(Timer8253Voice106 *t, uint16_t count);
void timer8253_106_reset_and_load_at(Timer8253Voice106 *t,
                                     uint16_t count,
                                     TimerRange106 range,
                                     Fm1junoTimeQ32 now);
void timer8253_106_request_count(Timer8253Voice106 *t, uint16_t count);
void timer8253_106_advance_half_cycle(Timer8253Voice106 *t);
Fm1junoTimeQ32 timer8253_106_commit_edge(Timer8253Voice106 *t);

#endif
