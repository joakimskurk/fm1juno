#ifndef FM1JUNO_ENVELOPE106_H
#define FM1JUNO_ENVELOPE106_H

#include <stdint.h>

#define ENVELOPE106_MAX UINT16_C(0x3fff)

/* ROM-equivalent three-part multiply. The low-low partial product is
 * intentionally omitted. */
uint16_t envelope106_calc_decay(uint16_t value, uint16_t coefficient);

uint16_t envelope106_attack_tick(uint16_t current, uint16_t increment);
uint16_t envelope106_decay_tick(uint16_t current,
                                uint16_t sustain,
                                uint16_t coefficient);
uint16_t envelope106_release_tick(uint16_t current, uint16_t coefficient);

#endif
