#include "control106/pitch106.h"

#define PITCH106_SEMITONE_RATIO_Q24 UINT32_C(17774841)
#define PITCH106_Q24_HALF UINT64_C(0x00800000)

uint16_t pitch106_global_offset(int16_t tune,
                                int16_t lfo_pitch,
                                int16_t bend_pitch)
{
    const int32_t value = (int32_t)PITCH106_GLOBAL_BASE
        + (int32_t)tune
        + (int32_t)lfo_pitch
        + (int32_t)bend_pitch;
    return (uint16_t)value;
}

uint16_t pitch106_final(uint16_t voice_pitch, uint16_t global_offset)
{
    return (uint16_t)(voice_pitch + global_offset);
}

PitchLookupCoordinate106 pitch106_lookup_coordinate(uint16_t pitch_8_8)
{
    const uint8_t note = (uint8_t)(pitch_8_8 >> 8u);
    const uint8_t fraction = (uint8_t)pitch_8_8;
    PitchLookupCoordinate106 out;

    if (note <= 47u) {
        out.index = 0u;
        out.fraction = 0u;
    } else if (note >= 151u) {
        out.index = PITCH106_LOOKUP_MAX_INDEX;
        out.fraction = 0u;
    } else {
        out.index = (uint8_t)(note - PITCH106_LOOKUP_MIN_NOTE);
        out.fraction = fraction;
    }
    return out;
}

uint16_t pitch106_interpolate_descending(uint16_t current,
                                         uint16_t next,
                                         uint8_t fraction)
{
    const uint16_t difference = (uint16_t)(current - next);
    const uint16_t scaled = (uint16_t)(
        ((uint32_t)difference * (uint32_t)fraction) >> 8u);
    return (uint16_t)(current - scaled);
}

uint16_t pitch106_interpolate_ascending(uint16_t current,
                                        uint16_t next,
                                        uint8_t fraction)
{
    const uint16_t difference = (uint16_t)(next - current);
    const uint16_t scaled = (uint16_t)(
        ((uint32_t)difference * (uint32_t)fraction) >> 8u);
    return (uint16_t)(current + scaled);
}

uint16_t pitch106_dco_cv_base_derived(uint8_t index)
{
    /* Q24 representation of 32 * 2^(index/12). Keeping the recurrence at
     * high precision until the final round avoids a copied semitone table. */
    uint64_t value_q24 = UINT64_C(32) << 24u;

    for (uint8_t i = 0u; i < index; ++i) {
        value_q24 = (value_q24 * PITCH106_SEMITONE_RATIO_Q24
                     + PITCH106_Q24_HALF) >> 24u;

        /* The original 12-bit DAC table saturates in the upper range. Stop
         * once the rounded result has reached physical full scale. */
        if (((value_q24 + PITCH106_Q24_HALF) >> 24u) >= PITCH106_DCO_CV_MAX)
            return PITCH106_DCO_CV_MAX;
    }

    const uint64_t rounded = (value_q24 + PITCH106_Q24_HALF) >> 24u;
    return rounded >= PITCH106_DCO_CV_MAX
        ? PITCH106_DCO_CV_MAX
        : (uint16_t)rounded;
}

uint16_t pitch106_dco_cv12_from_pitch(uint16_t pitch_8_8)
{
    const PitchLookupCoordinate106 coordinate =
        pitch106_lookup_coordinate(pitch_8_8);
    const uint16_t current =
        pitch106_dco_cv_base_derived(coordinate.index);

    if (coordinate.fraction == 0u || current == PITCH106_DCO_CV_MAX)
        return current;

    const uint16_t next =
        pitch106_dco_cv_base_derived((uint8_t)(coordinate.index + 1u));
    return pitch106_interpolate_ascending(current, next, coordinate.fraction);
}

/* Clean-room engineering candidate from a compact reciprocal-frequency
 * law. The correction term compensates the low-register systematic error.
 * Upper-register rational branches avoid one-count quantization errors
 * that otherwise amount to several cents at low divisor values.
 *
 * 0..71: 61355*x + 210*x*x; 72..83: 61355*x; 84..103: 61325*x,
 * where x = 2^(-index/12), held in integer Q24.
 *
 * Offline comparison against public behavior vectors gives <= 0.48 cent
 * across 104 semitone base points with Q24 arithmetic. The profile is
 * APPROXIMATE / NOT ROM BIT-IDENTICAL; no original ROM table is embedded.
 * 64-bit multiplies execute only at initialization. */
#define PITCH106_INV_RATIO_Q24 UINT32_C(15835583)

void pitch106_timer_build_derived_candidate(PitchTimerBase106 *profile)
{
    uint64_t ratio_q24 = UINT64_C(1) << 24u;
    for (uint32_t i = 0u; i < PITCH106_TIMER_ENTRY_COUNT; ++i) {
        uint64_t scaled;
        if (i < 72u) {
            const uint64_t sq_q24 = (ratio_q24 * ratio_q24 +
                                    PITCH106_Q24_HALF) >> 24u;
            scaled = UINT64_C(61355) * ratio_q24 +
                     UINT64_C(210) * sq_q24;
        } else if (i < 84u) {
            scaled = UINT64_C(61355) * ratio_q24;
        } else {
            scaled = UINT64_C(61325) * ratio_q24;
        }
        const uint64_t count = (scaled + PITCH106_Q24_HALF) >> 24u;
        profile->counts[i] = count > UINT16_MAX ? UINT16_MAX
                            : count == 0u ? 1u : (uint16_t)count;
        ratio_q24 = (ratio_q24 * PITCH106_INV_RATIO_Q24 +
                     PITCH106_Q24_HALF) >> 24u;
    }
}

uint16_t pitch106_timer_count_from_pitch(const PitchTimerBase106 *profile,
                                          uint16_t pitch_8_8)
{
    const PitchLookupCoordinate106 coordinate =
        pitch106_lookup_coordinate(pitch_8_8);
    const uint16_t current = profile->counts[coordinate.index];
    if (coordinate.fraction == 0u)
        return current;
    const uint16_t next = profile->counts[(uint32_t)coordinate.index + 1u];
    return pitch106_interpolate_descending(current, next,
                                            coordinate.fraction);
}
