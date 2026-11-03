#include "timer106/timer8253_106.h"

uint32_t timer8253_106_effective_count(uint16_t count_register)
{
    return count_register == 0u ? UINT32_C(65536) : (uint32_t)count_register;
}

uint32_t timer8253_106_high_clocks(uint16_t count_register)
{
    const uint32_t n = timer8253_106_effective_count(count_register);
    return (n + 1u) / 2u;
}

uint32_t timer8253_106_low_clocks(uint16_t count_register)
{
    const uint32_t n = timer8253_106_effective_count(count_register);
    return n / 2u;
}

void timer8253_106_reset_and_load(Timer8253Voice106 *t, uint16_t count)
{
    t->current_count = count;
    t->pending_count = 0u;
    t->pending_valid = false;
    t->output_high = true;
    t->clocks_remaining = timer8253_106_high_clocks(count);
}

void timer8253_106_request_count(Timer8253Voice106 *t, uint16_t count)
{
    t->pending_count = count;
    t->pending_valid = true;
}

void timer8253_106_advance_half_cycle(Timer8253Voice106 *t)
{
    t->output_high = !t->output_high;
    if (t->pending_valid) {
        t->current_count = t->pending_count;
        t->pending_valid = false;
    }
    t->clocks_remaining = t->output_high
        ? timer8253_106_high_clocks(t->current_count)
        : timer8253_106_low_clocks(t->current_count);
}
