#include "control106/raw_params106.h"

#include <stddef.h>

void raw_params106_init(RawParams106 *p)
{
    /* Startup working defaults for the Phase-1 control harness.
     * NOT a claim that every slider matches the factory patch defaults. */
    *p = (RawParams106){0};
    p->tune_adc8 = 128u;
    p->env_sustain7 = 127u;
    p->vcf_cutoff7 = 127u;
    p->global_vca_level7 = 64u;
}

bool raw_params106_valid(RawParameterId106 id, uint32_t value)
{
    if ((unsigned)id >= (unsigned)RAW106_PARAMETER_COUNT) return false;

    switch (id) {
    case RAW106_BEND_NEGATIVE:
    case RAW106_VCF_ENV_INVERTED:
    case RAW106_PERFORMANCE_TRIGGER:
    case RAW106_PWM_MANUAL:
    case RAW106_PULSE_ENABLED:
    case RAW106_VCA_GATE_MODE:
        return value <= 1u;
    case RAW106_TUNE_ADC8:
    case RAW106_PORTAMENTO_ADC8:
    case RAW106_BEND_MAGNITUDE8:
    case RAW106_DCO_BEND_SENSITIVITY8:
    case RAW106_VCF_BEND_SENSITIVITY8:
    case RAW106_PERFORMANCE_SENSITIVITY8:
    case RAW106_MIDI_MOD_DEPTH8:
        return value <= 255u;
    default:
        return value <= 127u;
    }
}

bool raw_params106_set(RawParams106 *p, RawParameterId106 id,
                        uint32_t value)
{
    if (p == NULL || !raw_params106_valid(id, value)) return false;
    const uint8_t v = (uint8_t)value;
    switch (id) {
    case RAW106_TUNE_ADC8: p->tune_adc8 = v; break;
    case RAW106_PORTAMENTO_ADC8: p->portamento_adc8 = v; break;
    case RAW106_LFO_RATE7: p->lfo_rate7 = v; break;
    case RAW106_LFO_DELAY7: p->lfo_delay7 = v; break;
    case RAW106_DCO_LFO_DEPTH7: p->dco_lfo_depth7 = v; break;
    case RAW106_BEND_MAGNITUDE8: p->bend_magnitude8 = v; break;
    case RAW106_DCO_BEND_SENSITIVITY8: p->dco_bend_sensitivity8 = v; break;
    case RAW106_VCF_BEND_SENSITIVITY8: p->vcf_bend_sensitivity8 = v; break;
    case RAW106_BEND_NEGATIVE: p->bend_negative = v != 0u; break;
    case RAW106_VCF_CUTOFF7: p->vcf_cutoff7 = v; break;
    case RAW106_VCF_LFO_DEPTH7: p->vcf_lfo_depth7 = v; break;
    case RAW106_VCF_ENV_DEPTH7: p->vcf_env_depth7 = v; break;
    case RAW106_VCF_ENV_INVERTED: p->vcf_env_inverted = v != 0u; break;
    case RAW106_VCF_KEY_FOLLOW7: p->vcf_key_follow7 = v; break;
    case RAW106_ENV_ATTACK7: p->env_attack7 = v; break;
    case RAW106_ENV_DECAY7: p->env_decay7 = v; break;
    case RAW106_ENV_SUSTAIN7: p->env_sustain7 = v; break;
    case RAW106_ENV_RELEASE7: p->env_release7 = v; break;
    case RAW106_PERFORMANCE_TRIGGER: p->performance_trigger = v != 0u; break;
    case RAW106_PERFORMANCE_SENSITIVITY8: p->performance_sensitivity8 = v; break;
    case RAW106_MIDI_MOD_DEPTH8: p->midi_mod_depth8 = v; break;
    case RAW106_PWM_DEPTH7: p->pwm_depth7 = v; break;
    case RAW106_PWM_MANUAL: p->pwm_manual = v != 0u; break;
    case RAW106_PULSE_ENABLED: p->pulse_enabled = v != 0u; break;
    case RAW106_VCF_RESONANCE7: p->vcf_resonance7 = v; break;
    case RAW106_SUB_LEVEL7: p->sub_level7 = v; break;
    case RAW106_NOISE_LEVEL7: p->noise_level7 = v; break;
    case RAW106_GLOBAL_VCA_LEVEL7: p->global_vca_level7 = v; break;
    case RAW106_VCA_GATE_MODE: p->vca_gate_mode = v != 0u; break;
    default: return false;
    }
    return true;
}
