#ifndef FM1JUNO_PITCH106_H
#define FM1JUNO_PITCH106_H

#include <stdint.h>

#define PITCH106_GLOBAL_BASE UINT16_C(0x1818)
#define PITCH106_LOOKUP_MIN_NOTE 48u
#define PITCH106_LOOKUP_MAX_NOTE 150u
#define PITCH106_LOOKUP_MAX_INDEX 102u
#define PITCH106_DCO_CV_MAX UINT16_C(0x0fff)

typedef struct PitchLookupCoordinate106 {
    uint8_t index;
    uint8_t fraction;
} PitchLookupCoordinate106;

uint16_t pitch106_global_offset(int16_t tune_8_8,
                                int16_t lfo_pitch_8_8,
                                int16_t bend_pitch_8_8);
uint16_t pitch106_final(uint16_t voice_portamento_pitch_8_8,
                        uint16_t global_offset_8_8);

/* Convert final 8.8 pitch to the clamped ROM lookup coordinate.
 * High byte <=47 clamps to index 0 / fraction 0.
 * High byte >=151 clamps to index 102 / fraction 0. */
PitchLookupCoordinate106 pitch106_lookup_coordinate(uint16_t pitch_8_8);

/* Exact ROM-equivalent interpolation primitives once adjacent base entries
 * are known. The multiplication truncates via floor(diff*fraction/256). */
uint16_t pitch106_interpolate_descending(uint16_t current,
                                         uint16_t next,
                                         uint8_t fraction);
uint16_t pitch106_interpolate_ascending(uint16_t current,
                                        uint16_t next,
                                        uint8_t fraction);

/* Clean-room derived DCO-CV base law. This is generated from 32*2^(n/12)
 * using an integer Q24 twelfth-root-of-two recurrence and saturates at the
 * physical 12-bit maximum. It is intentionally not a copied ROM table. */
uint16_t pitch106_dco_cv_base_derived(uint8_t index);
uint16_t pitch106_dco_cv12_from_pitch(uint16_t pitch_8_8);

#endif
