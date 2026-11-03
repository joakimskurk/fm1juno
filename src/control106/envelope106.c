#include "control106/envelope106.h"

uint16_t envelope106_sustain_level_from_raw(uint8_t raw)
{
    return (uint16_t)((uint16_t)(raw & UINT8_C(0x7f)) << 7u);
}

uint16_t envelope106_decay_release_coefficient(uint8_t index)
{
    /* Exact compact form of the monotone ROM coefficient behavior.
     * Segment boundaries follow the places where the integer increment
     * changes. No original 128-entry table is stored in the implementation. */
    if (index <= 4u)
        return (uint16_t)(UINT16_C(0x1000) +
                          (uint16_t)index * UINT16_C(0x2000));

    if (index <= 15u)
        return (uint16_t)(UINT16_C(0xa000) +
                          (uint16_t)(index - 5u) * UINT16_C(0x0800));

    if (index <= 43u)
        return (uint16_t)(UINT16_C(0xf000) +
                          (uint16_t)(index - 15u) * UINT16_C(0x0080));

    if (index <= 65u)
        return (uint16_t)(UINT16_C(0xfe00) +
                          (uint16_t)(index - 43u) * UINT16_C(0x000c));

    if (index <= 123u)
        return (uint16_t)(UINT16_C(0xff08) +
                          (uint16_t)(index - 65u) * UINT16_C(0x0004));

    return (uint16_t)(UINT16_C(0xfff0) +
                      (uint16_t)(index - 123u));
}

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

void envelope106_init(Envelope106 *env)
{
    env->level = 0u;
    env->phase = ENVELOPE106_RELEASE;
}

void envelope106_note_on(Envelope106 *env)
{
    env->phase = ENVELOPE106_ATTACK;
}

void envelope106_note_off(Envelope106 *env)
{
    env->phase = ENVELOPE106_RELEASE;
}

uint16_t envelope106_tick(Envelope106 *env, const EnvelopeParams106 *p)
{
    switch (env->phase) {
    case ENVELOPE106_ATTACK:
        env->level = envelope106_attack_tick(env->level, p->attack_increment);
        if (env->level >= ENVELOPE106_MAX)
            env->phase = ENVELOPE106_DECAY;
        break;
    case ENVELOPE106_DECAY:
        env->level = envelope106_decay_tick(env->level,
                                             p->sustain_level,
                                             p->decay_coefficient);
        if (env->level <= p->sustain_level) {
            env->level = p->sustain_level;
            env->phase = ENVELOPE106_SUSTAIN;
        }
        break;
    case ENVELOPE106_SUSTAIN:
        env->level = p->sustain_level;
        break;
    case ENVELOPE106_RELEASE:
    default:
        env->level = envelope106_release_tick(env->level,
                                               p->release_coefficient);
        break;
    }
    return env->level;
}
