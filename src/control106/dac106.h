#ifndef FM1JUNO_DAC106_H
#define FM1JUNO_DAC106_H

#include <stdint.h>

/* The module firmware commonly carries controls in a 14-bit 0..0x3fff
 * domain, but the physical DAC path exposes 12 significant bits. */
uint16_t dac106_code14_to_dac12(uint16_t code14);

#endif
