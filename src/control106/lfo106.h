#ifndef FM1JUNO_LFO106_H
#define FM1JUNO_LFO106_H

#include <stdint.h>

#define LFO106_MAG_MAX UINT16_C(0x1fff)
#define LFO106_DIRECTION_BIT UINT8_C(0x01)
#define LFO106_POLARITY_BIT UINT8_C(0x02)

typedef struct Lfo106State {
    uint16_t magnitude;
    uint16_t rate_coefficient;
    uint8_t phase_flags;
} Lfo106State;

uint16_t lfo106_rate_coefficient(uint8_t index);
uint8_t lfo106_depth_scaler(uint8_t raw_depth);
uint16_t lfo106_half_steps(uint16_t coefficient);
void lfo106_init(Lfo106State *lfo, uint8_t rate_index);
int16_t lfo106_signed_magnitude(const Lfo106State *lfo);
int16_t lfo106_tick(Lfo106State *lfo);

#endif
