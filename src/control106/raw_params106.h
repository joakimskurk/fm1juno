#ifndef FM1JUNO_RAW_PARAMS106_H
#define FM1JUNO_RAW_PARAMS106_H

#include <stdbool.h>
#include <stdint.h>

/* Stable event-script identifier for timestamped panel/performance edits.
 * Nominal 7-bit controls are distinct from physical 8-bit ADC / MIDI values. */
typedef enum RawParameterId106 {
    RAW106_TUNE_ADC8 = 0,
    RAW106_PORTAMENTO_ADC8,
    RAW106_LFO_RATE7,
    RAW106_LFO_DELAY7,
    RAW106_DCO_LFO_DEPTH7,
    RAW106_BEND_MAGNITUDE8,
    RAW106_DCO_BEND_SENSITIVITY8,
    RAW106_VCF_BEND_SENSITIVITY8,
    RAW106_BEND_NEGATIVE,
    RAW106_VCF_CUTOFF7,
    RAW106_VCF_LFO_DEPTH7,
    RAW106_VCF_ENV_DEPTH7,
    RAW106_VCF_ENV_INVERTED,
    RAW106_VCF_KEY_FOLLOW7,
    RAW106_ENV_ATTACK7,
    RAW106_ENV_DECAY7,
    RAW106_ENV_SUSTAIN7,
    RAW106_ENV_RELEASE7,
    RAW106_PERFORMANCE_TRIGGER,
    RAW106_PERFORMANCE_SENSITIVITY8,
    RAW106_MIDI_MOD_DEPTH8,
    /* Append only: numeric IDs 0..20 are used in existing replay scripts. */
    RAW106_PWM_DEPTH7,
    RAW106_PWM_MANUAL,
    RAW106_PULSE_ENABLED,
    RAW106_VCF_RESONANCE7,
    RAW106_SUB_LEVEL7,
    RAW106_NOISE_LEVEL7,
    RAW106_GLOBAL_VCA_LEVEL7,
    RAW106_VCA_GATE_MODE,
    RAW106_PARAMETER_COUNT
} RawParameterId106;

typedef struct RawParams106 {
    uint8_t tune_adc8;
    uint8_t portamento_adc8;
    uint8_t lfo_rate7;
    uint8_t lfo_delay7;
    uint8_t dco_lfo_depth7;
    uint8_t bend_magnitude8;
    uint8_t dco_bend_sensitivity8;
    uint8_t vcf_bend_sensitivity8;
    bool bend_negative;
    uint8_t vcf_cutoff7;
    uint8_t vcf_lfo_depth7;
    uint8_t vcf_env_depth7;
    bool vcf_env_inverted;
    uint8_t vcf_key_follow7;
    uint8_t env_attack7;
    uint8_t env_decay7;
    uint8_t env_sustain7;
    uint8_t env_release7;
    bool performance_trigger;
    uint8_t performance_sensitivity8;
    uint8_t midi_mod_depth8;
    uint8_t pwm_depth7;
    bool pwm_manual;
    bool pulse_enabled;
    uint8_t vcf_resonance7;
    uint8_t sub_level7;
    uint8_t noise_level7;
    uint8_t global_vca_level7;
    bool vca_gate_mode;
} RawParams106;

void raw_params106_init(RawParams106 *params);
bool raw_params106_valid(RawParameterId106 id, uint32_t value);
bool raw_params106_set(RawParams106 *params, RawParameterId106 id,
                        uint32_t value);

#endif
