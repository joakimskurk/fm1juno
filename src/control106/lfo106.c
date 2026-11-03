#include "control106/lfo106.h"

uint16_t lfo106_rate_coefficient(uint8_t i)
{
    if (i <= 2u) return (uint16_t)(5u + 10u * i);
    if (i <= 5u) return (uint16_t)(40u + 15u * (i - 3u));
    if (i <= 7u) return (uint16_t)(80u + 10u * (i - 6u));
    if (i <= 63u) return (uint16_t)(100u + 10u * (i - 8u));
    if (i <= 95u) return (uint16_t)(666u + 16u * (i - 64u));
    if (i <= 102u) return (uint16_t)(1214u + 52u * (i - 96u));
    if (i == 103u) return 1580u;
    if (i <= 105u) return (uint16_t)(1650u + 70u * (i - 104u));
    if (i <= 109u) return (uint16_t)(1800u + 80u * (i - 106u));
    if (i <= 119u) return (uint16_t)(2140u + 100u * (i - 110u));
    if (i <= 122u) return (uint16_t)(3160u + 120u * (i - 120u));
    if (i <= 126u) return (uint16_t)(3550u + 150u * (i - 123u));
    return 4096u;
}

uint8_t lfo106_depth_scaler(uint8_t raw_depth)
{
    const uint8_t i = (uint8_t)(raw_depth & UINT8_C(0x7f));

    if (i <= 2u) return 0u;
    if (i <= 64u) return (uint8_t)(i - 2u);
    if (i <= 95u) return (uint8_t)(2u * i - 66u);
    if (i <= 124u) return (uint8_t)(4u * i - 256u);
    if (i == 125u) return 248u;
    return UINT8_C(0xff);
}

uint16_t lfo106_half_steps(uint16_t coefficient)
{
    if (coefficient == 0u) return 0u;
    return (uint16_t)((8192u + coefficient - 1u) / coefficient);
}

void lfo106_init(Lfo106State *lfo, uint8_t rate_index)
{
    lfo->magnitude = 0u;
    lfo->rate_coefficient = lfo106_rate_coefficient(rate_index);
    lfo->phase_flags = 0u;
}

int16_t lfo106_signed_magnitude(const Lfo106State *lfo)
{
    const int16_t mag = (int16_t)lfo->magnitude;
    return (lfo->phase_flags & LFO106_POLARITY_BIT) != 0u ? (int16_t)-mag : mag;
}

int16_t lfo106_tick(Lfo106State *lfo)
{
    const uint16_t c = lfo->rate_coefficient;

    if ((lfo->phase_flags & LFO106_DIRECTION_BIT) == 0u) {
        const uint32_t next = (uint32_t)lfo->magnitude + c;
        if (next >= UINT32_C(0x2000)) {
            lfo->magnitude = LFO106_MAG_MAX;
            ++lfo->phase_flags;
        } else {
            lfo->magnitude = (uint16_t)next;
        }
    } else {
        if (lfo->magnitude < c) {
            lfo->magnitude = 0u;
            ++lfo->phase_flags;
        } else {
            lfo->magnitude = (uint16_t)(lfo->magnitude - c);
        }
    }

    return lfo106_signed_magnitude(lfo);
}
