#include "control106/secondary_control106.h"

#include "control106/control_math106.h"
#include "control106/dac106.h"

SecondaryMuxRoute106 secondary106_route(SecondaryLane106 lane)
{
    switch (lane) {
    case SECONDARY106_RESONANCE:
        return (SecondaryMuxRoute106){SECONDARY106_MUX_IC26_VCA, 6u};
    case SECONDARY106_GLOBAL_VCA:
        return (SecondaryMuxRoute106){SECONDARY106_MUX_IC23_VCF, 6u};
    case SECONDARY106_SUB_LEVEL:
        return (SecondaryMuxRoute106){SECONDARY106_MUX_IC24_RAMP, 6u};
    case SECONDARY106_PWM:
        return (SecondaryMuxRoute106){SECONDARY106_MUX_IC23_VCF, 7u};
    case SECONDARY106_NOISE:
        return (SecondaryMuxRoute106){SECONDARY106_MUX_IC26_VCA, 7u};
    default:
        return (SecondaryMuxRoute106){SECONDARY106_MUX_IC26_VCA, 0xffu};
    }
}

SecondaryMuxRoute106 secondary106_voice_vca_route(uint8_t voice)
{
    return (SecondaryMuxRoute106){SECONDARY106_MUX_IC26_VCA,
                                  voice < 6u ? voice : 0xffu};
}

uint16_t secondary106_patch_code14(uint8_t raw7)
{
    return (uint16_t)((uint16_t)(raw7 & UINT8_C(0x7f)) << 7u);
}

uint16_t secondary106_pwm_code14(uint8_t pwm_depth7,
                                  bool manual_mode,
                                  bool pulse_enabled,
                                  uint16_t lfo_magnitude,
                                  bool lfo_negative)
{
    if (!pulse_enabled) return 0u;
    if (lfo_magnitude > UINT16_C(0x1fff))
        lfo_magnitude = UINT16_C(0x1fff);

    const uint16_t carrier = manual_mode
        ? UINT16_C(0x3fff)
        : (lfo_negative
           ? (uint16_t)(UINT16_C(0x2000) + lfo_magnitude)
           : (uint16_t)(UINT16_C(0x2000) - lfo_magnitude));

    const uint8_t depth2 = (uint8_t)(
        (uint16_t)(pwm_depth7 & UINT8_C(0x7f)) << 1u);
    const uint16_t scaled = control106_mul16x8_hi(carrier, depth2);
    return (uint16_t)(UINT16_C(0x3fff) - scaled);
}

uint16_t secondary106_voice_vca_code14(uint16_t envelope14,
                                       bool gate_mode,
                                       bool voice_running)
{
    if (gate_mode)
        return voice_running ? UINT16_C(0x3fff) : 0u;
    return envelope14 > UINT16_C(0x3fff)
        ? UINT16_C(0x3fff) : envelope14;
}

void secondary106_commit(SecondaryHold106 *hold,
                         uint16_t code14,
                         Fm1junoTimeQ32 time,
                         uint32_t pass_id)
{
    hold->code14 = (uint16_t)(code14 & UINT16_C(0x3fff));
    hold->dac12 = dac106_code14_to_dac12(hold->code14);
    hold->committed_at = time;
    hold->pass_id = pass_id;
    hold->valid = true;
}
