#include "control106/vcf_control106.h"

#include "control106/control_math106.h"
#include "control106/dac106.h"

static uint8_t panel7(uint8_t raw)
{
    return (uint8_t)(raw & UINT8_C(0x7f));
}

static uint16_t clamp14(int32_t value)
{
    if (value < 0) return 0u;
    if (value > VCF106_MAX_CODE14) return VCF106_MAX_CODE14;
    return (uint16_t)value;
}

int32_t vcf106_global_raw(const VcfControlParams106 *p)
{
    const uint16_t cutoff = (uint16_t)((uint16_t)panel7(p->cutoff7) << 7u);
    const uint8_t lfo_depth = (uint8_t)((uint16_t)panel7(p->lfo_depth7) << 1u);
    const uint8_t after_delay = (uint8_t)(
        ((uint16_t)lfo_depth * (uint16_t)p->lfo_onset) >> 8u);
    const uint16_t magnitude = p->lfo_magnitude > UINT16_C(0x1fff)
        ? UINT16_C(0x1fff) : p->lfo_magnitude;
    const uint16_t lfo = (uint16_t)(
        control106_mul16x8_hi(magnitude, after_delay) >> 1u);
    const uint16_t bend = control106_vcf_bend_12(
        p->bend_magnitude, p->bend_sensitivity);

    int32_t sum = (int32_t)cutoff;
    sum += p->lfo_negative ? -(int32_t)lfo : (int32_t)lfo;
    sum += p->bend_negative ? -(int32_t)bend : (int32_t)bend;
    return sum;
}

int32_t vcf106_key_follow_delta(uint16_t pitch88, uint8_t key_follow7)
{
    /* Separate shifts intentionally preserve ROM's integer rounding:
     * floor(pitch/4) + floor(pitch/8), not floor(3*pitch/8). */
    const uint16_t pitch38 = (uint16_t)((pitch88 >> 2u) + (pitch88 >> 3u));
    const int32_t signed_distance =
        (int32_t)pitch38 - (int32_t)VCF106_KEY_PIVOT_3_8;
    const uint16_t magnitude = (uint16_t)(signed_distance < 0
        ? -signed_distance : signed_distance);
    const uint8_t key_depth = (uint8_t)(
        (uint16_t)panel7(key_follow7) << 1u);
    const int32_t scaled = (int32_t)control106_mul16x8_hi(
        magnitude, key_depth);
    return signed_distance < 0 ? -scaled : scaled;
}

uint16_t vcf106_voice_code14(const VcfControlParams106 *p,
                              int32_t global_raw,
                              uint16_t envelope14,
                              uint16_t portamento_pitch_8_8)
{
    const uint16_t env = envelope14 > VCF106_MAX_CODE14
        ? VCF106_MAX_CODE14 : envelope14;
    const uint8_t depth = (uint8_t)((uint16_t)panel7(p->env_depth7) << 1u);
    const int32_t env_amount =
        (int32_t)control106_mul16x8_hi(env, depth);

    int32_t value = global_raw;
    value += p->env_inverted ? -env_amount : env_amount;
    value += vcf106_key_follow_delta(portamento_pitch_8_8,
                                     p->key_follow7);
    return clamp14(value);
}

uint16_t vcf106_voice_dac12(const VcfControlParams106 *p,
                             int32_t global_raw,
                             uint16_t envelope14,
                             uint16_t portamento_pitch_8_8)
{
    return dac106_code14_to_dac12(
        vcf106_voice_code14(p, global_raw, envelope14,
                            portamento_pitch_8_8));
}
