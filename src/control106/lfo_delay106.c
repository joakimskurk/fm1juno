#include "control106/lfo_delay106.h"

void lfo_delay106_init(LfoDelay106 *d)
{
    d->armed_after_silence = true;
    d->holdoff_accum = 0u;
    d->ramp_accum = 0u;
    d->phase = LFO_DELAY106_HOLD;
}

bool lfo_delay106_observe_voice_run(LfoDelay106 *d, uint8_t voice_run_bits)
{
    if ((voice_run_bits & UINT8_C(0x3f)) == 0u) {
        d->armed_after_silence = true;
        return false;
    }

    if (!d->armed_after_silence) return false;

    d->armed_after_silence = false;
    d->holdoff_accum = 0u;
    d->ramp_accum = 0u;
    d->phase = LFO_DELAY106_HOLD;
    return true;
}

uint8_t lfo_delay106_onset(const LfoDelay106 *d)
{
    if (d->phase == LFO_DELAY106_HOLD) return 0u;
    if (d->phase == LFO_DELAY106_FULL) return UINT8_C(0xff);
    return (uint8_t)(d->ramp_accum >> 8u);
}

static uint8_t ramp_tick(LfoDelay106 *d, uint16_t ramp_increment)
{
    const uint32_t sum = (uint32_t)d->ramp_accum + ramp_increment;
    if (sum > UINT16_MAX) {
        d->phase = LFO_DELAY106_FULL;
        return UINT8_C(0xff);
    }

    d->ramp_accum = (uint16_t)sum;
    d->phase = LFO_DELAY106_RAMP;
    return (uint8_t)(d->ramp_accum >> 8u);
}

uint8_t lfo_delay106_tick(LfoDelay106 *d,
                          uint16_t holdoff_increment,
                          uint16_t ramp_increment)
{
    if (d->phase == LFO_DELAY106_FULL) return UINT8_C(0xff);

    if (d->phase == LFO_DELAY106_HOLD) {
        d->holdoff_accum = (uint16_t)(d->holdoff_accum + holdoff_increment);
        if ((d->holdoff_accum & UINT16_C(0xc000)) == 0u)
            return 0u;
        d->phase = LFO_DELAY106_RAMP;
    }

    return ramp_tick(d, ramp_increment);
}

void performance_lfo106_init(PerformanceLfo106 *p)
{
    p->trigger_declick = 0u;
}

uint8_t performance_lfo106_tick(PerformanceLfo106 *p,
                                bool physical_trigger_active,
                                uint8_t trigger_sensitivity,
                                uint8_t midi_mod_depth)
{
    if (!physical_trigger_active) {
        p->trigger_declick = 0u;
    } else if (p->trigger_declick < trigger_sensitivity) {
        const uint16_t next = (uint16_t)p->trigger_declick + 0x0au;
        p->trigger_declick = next >= trigger_sensitivity
            ? trigger_sensitivity : (uint8_t)next;
    }

    const uint8_t midi_perf = (uint8_t)(
        ((uint16_t)midi_mod_depth * trigger_sensitivity) >> 8u);
    return p->trigger_declick >= midi_perf ? p->trigger_declick : midi_perf;
}

uint8_t lfo_delay106_combine_depth(uint8_t panel_depth,
                                   uint8_t onset,
                                   uint8_t performance_depth)
{
    const uint8_t delayed = (uint8_t)(
        ((uint16_t)panel_depth * onset) >> 8u);
    const uint16_t combined = (uint16_t)delayed + performance_depth;
    return combined > UINT8_MAX ? UINT8_MAX : (uint8_t)combined;
}
