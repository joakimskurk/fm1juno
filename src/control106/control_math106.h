#ifndef FM1JUNO_CONTROL_MATH106_H
#define FM1JUNO_CONTROL_MATH106_H

#include <stdint.h>

uint16_t control106_mul16x8_hi(uint16_t value, uint8_t multiplier);
uint16_t control106_dco_bend_8_8(uint8_t magnitude, uint8_t sensitivity);
uint16_t control106_vcf_bend_12(uint8_t magnitude, uint8_t sensitivity);
int16_t control106_tune_8_8(uint8_t raw_tune);

#endif
