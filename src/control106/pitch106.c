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
