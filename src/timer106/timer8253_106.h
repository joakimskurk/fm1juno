#ifndef FM1JUNO_TIMER8253_106_H
#define FM1JUNO_TIMER8253_106_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Timer8253Voice106 {
    uint16_t current_count;
    uint16_t pending_count;
    uint32_t clocks_remaining;
    bool pending_valid;
    bool output_high;
} Timer8253Voice106;

uint32_t timer8253_106_effective_count(uint16_t count_register);
uint32_t timer8253_106_high_clocks(uint16_t count_register);
uint32_t timer8253_106_low_clocks(uint16_t count_register);

void timer8253_106_reset_and_load(Timer8253Voice106 *t, uint16_t count);
void timer8253_106_request_count(Timer8253Voice106 *t, uint16_t count);
void timer8253_106_advance_half_cycle(Timer8253Voice106 *t);

#endif
