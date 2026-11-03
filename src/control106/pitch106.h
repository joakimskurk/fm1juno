#ifndef FM1JUNO_PITCH106_H
#define FM1JUNO_PITCH106_H

#include <stdint.h>

#define PITCH106_GLOBAL_BASE UINT16_C(0x1818)

uint16_t pitch106_global_offset(int16_t tune_8_8,
                                int16_t lfo_pitch_8_8,
                                int16_t bend_pitch_8_8);
uint16_t pitch106_final(uint16_t voice_portamento_pitch_8_8,
                        uint16_t global_offset_8_8);

#endif
