#include "control106/envelope106.h"

uint16_t envelope106_calc_decay(uint16_t value, uint16_t coefficient)
{
    const uint16_t vh = (uint16_t)(value >> 8u);
    const uint16_t vl = (uint16_t)(value & UINT16_C(0x00ff));
    const uint16_t ch = (uint16_t)(coefficient >> 8u);
    const uint16_t cl = (uint16_t)(coefficient & UINT16_C(0x00ff));

    const uint32_t result = (uint32_t)vh * ch
        + (((uint32_t)vh * cl) >> 8u)
        + (((uint32_t)vl * ch) >> 8u);
    return (uint16_t)result;
}

uint16_t envelope106_attack_tick(uint16_t current, uint16_t increment)
{
    const uint32_t next = (uint32_t)current + (uint32_t)increment;
    return next >= ENVELOPE106_MAX ? ENVELOPE106_MAX : (uint16_t)next;
}

uint16_t envelope106_decay_tick(uint16_t current,
                                uint16_t sustain,
                                uint16_t coefficient)
{
    if (current <= sustain) return sustain;

    const uint16_t delta = (uint16_t)(current - sustain);
    const uint32_t next = (uint32_t)sustain
        + (uint32_t)envelope106_calc_decay(delta, coefficient);
    return next > ENVELOPE106_MAX ? ENVELOPE106_MAX : (uint16_t)next;
}

uint16_t envelope106_release_tick(uint16_t current, uint16_t coefficient)
{
    return envelope106_calc_decay(current, coefficient);
}
