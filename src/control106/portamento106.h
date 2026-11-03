#ifndef FM1JUNO_PORTAMENTO106_H
#define FM1JUNO_PORTAMENTO106_H

#include <stdint.h>

uint8_t portamento106_index_from_raw(uint8_t raw_portamento);
uint16_t portamento106_step_toward(uint16_t current_pitch_8_8,
                                   uint16_t target_pitch_8_8,
                                   uint8_t step);

#endif
