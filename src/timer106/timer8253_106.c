#include "timer106/timer8253_106.h"

/* Rounded Q32.32 sample durations for one 8253 input clock at 44.1 kHz.
 * Generated offline from round((44100 / f_clock) * 2^32). */
#define TIMER106_Q32_PER_4MHZ UINT64_C(47352014)
#define TIMER106_Q32_PER_2MHZ UINT64_C(94704029)
#define TIMER106_Q32_PER_1MHZ UINT64_C(189408058)

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

uint32_t timer8253_106_input_clock_hz(TimerRange106 range)
{
    switch (range) {
    case TIMER106_RANGE_4: return UINT32_C(4000000);
    case TIMER106_RANGE_8: return UINT32_C(2000000);
    case TIMER106_RANGE_16:
    default: return UINT32_C(1000000);
    }
}

Fm1junoTimeQ32 timer8253_106_clock_q32(TimerRange106 range)
{
    switch (range) {
    case TIMER106_RANGE_4: return TIMER106_Q32_PER_4MHZ;
    case TIMER106_RANGE_8: return TIMER106_Q32_PER_2MHZ;
    case TIMER106_RANGE_16:
    default: return TIMER106_Q32_PER_1MHZ;
    }
}

Fm1junoTimeQ32 timer8253_106_clocks_to_time_q32(TimerRange106 range,
                                                uint32_t clocks)
{
    return timer8253_106_clock_q32(range) * (Fm1junoTimeQ32)clocks;
}

void timer8253_106_reset_and_load(Timer8253Voice106 *t, uint16_t count)
{
    timer8253_106_reset_and_load_at(t, count, TIMER106_RANGE_8, 0u);
}

void timer8253_106_reset_and_load_at(Timer8253Voice106 *t,
                                     uint16_t count,
                                     TimerRange106 range,
                                     Fm1junoTimeQ32 now)
{
    t->current_count = count;
    t->pending_count = 0u;
    t->pending_valid = false;
    t->output_high = true;
    t->range = range;
    t->clocks_remaining = timer8253_106_high_clocks(count);
    t->next_edge_time = now +
        timer8253_106_clocks_to_time_q32(range, t->clocks_remaining);
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

Fm1junoTimeQ32 timer8253_106_commit_edge(Timer8253Voice106 *t)
{
    const Fm1junoTimeQ32 edge = t->next_edge_time;
    timer8253_106_advance_half_cycle(t);
    t->next_edge_time = edge +
        timer8253_106_clocks_to_time_q32(t->range, t->clocks_remaining);
    return edge;
}
