#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/fixed_time.h"
#include "control106/control_math106.h"
#include "control106/event_replay106.h"
#include "control106/module_control106.h"
#include "control106/portamento106.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return EXIT_FAILURE; \
    } \
} while (0)

static ControlEvent106 advance_until(ModuleControl106 *m,
                                     ControlEventType106 type,
                                     uint8_t voice,
                                     Fm1junoTimeQ32 *now)
{
    for (unsigned i = 0u; i < 80u; ++i) {
        const ControlEvent106 e = module_control106_service_next(m, *now);
        *now += FM1JUNO_TIME_ONE_SAMPLE;
        if (e.type == type && (voice == CONTROL106_NO_VOICE || e.voice == voice))
            return e;
    }
    ControlEvent106 bad = {CONTROL106_NOISE_LEVEL, 255u, 0u, 0u};
    return bad;
}

static int test_parameter_validity_and_slot_causality(void)
{
    ModuleControl106 m;
    module_control106_init(&m);
    CHECK(!m.raw_params_active);
    CHECK(!module_control106_receive_parameter(
        &m, RAW106_LFO_RATE7, 128u, 0u));
    CHECK(!module_control106_receive_parameter(
        &m, RAW106_BEND_NEGATIVE, 2u, 0u));
    CHECK(!module_control106_receive_parameter(
        &m, (RawParameterId106)999, 1u, 0u));
    CHECK(!m.raw_params_active);
    CHECK(m.scheduler.parameter_interrupts == 0u);

    CHECK(module_control106_receive_parameter(
        &m, RAW106_TUNE_ADC8, 128u, 0u));
    CHECK(module_control106_receive_parameter(
        &m, RAW106_PORTAMENTO_ADC8, 64u, 0u));
    CHECK(m.raw_params_active);
    CHECK(m.scheduler.parameter_interrupts == 2u);
    CHECK(m.scheduler.abandoned_passes == 0u);
    CHECK(m.raw_params.portamento_adc8 == 64u);

    const AssignerEvent106 on = {
        ASSIGNER106_EVENT_VOICE_ON, 0u, 72u
    };
    module_control106_apply_assigner_event(&m, on, 0u);
    Fm1junoTimeQ32 now = FM1JUNO_TIME_ONE_SAMPLE;
    CHECK(advance_until(&m, CONTROL106_LFO_DELAY_AND_MOD_SCALE,
                        CONTROL106_NO_VOICE, &now).type ==
          CONTROL106_LFO_DELAY_AND_MOD_SCALE);

    /* Reception modifies raw RAM immediately. Existing pitch CV must
     * remain unchanged until PITCH_AND_PORTAMENTO executes. */
    CHECK(module_control106_receive_parameter(
        &m, RAW106_TUNE_ADC8, 129u, now));
    CHECK(m.active_global_pitch_offset_8_8 == PITCH106_GLOBAL_BASE);
    CHECK(advance_until(&m, CONTROL106_PITCH_AND_PORTAMENTO,
                        CONTROL106_NO_VOICE, &now).type ==
          CONTROL106_PITCH_AND_PORTAMENTO);
    CHECK(m.active_global_pitch_offset_8_8 ==
          (uint16_t)(PITCH106_GLOBAL_BASE + 1u));
    const uint8_t step = portamento106_step_from_raw(64u);
    CHECK(step > 0u);
    CHECK(m.voices[0].portamento_pitch_8_8 ==
          (uint16_t)((60u << 8u) + step));

    CHECK(module_control106_receive_parameter(
        &m, RAW106_TUNE_ADC8, 130u, now));
    CHECK(m.active_global_pitch_offset_8_8 ==
          (uint16_t)(PITCH106_GLOBAL_BASE + 1u));
    CHECK(m.scheduler.abandoned_passes == 1u);
    CHECK(advance_until(&m, CONTROL106_DCO_VOICE, 0u, &now).type ==
          CONTROL106_DCO_VOICE);
    CHECK(m.active_global_pitch_offset_8_8 ==
          (uint16_t)(PITCH106_GLOBAL_BASE + 1u));
    (void)advance_until(&m, CONTROL106_PITCH_AND_PORTAMENTO,
                        CONTROL106_NO_VOICE, &now);
    CHECK(m.active_global_pitch_offset_8_8 ==
          (uint16_t)(PITCH106_GLOBAL_BASE + 2u));
    CHECK(m.voices[0].portamento_pitch_8_8 ==
          (uint16_t)((60u << 8u) + 2u * step));
    return EXIT_SUCCESS;
}

static int test_lfo_bender_env_vcf_and_restart(void)
{
    ModuleControl106 m;
    module_control106_init(&m);
    PitchTimerBase106 profile;
    pitch106_timer_build_derived_candidate(&profile);
    module_control106_set_pitch_timer_profile(&m, &profile);

    CHECK(module_control106_receive_parameter(
        &m, RAW106_LFO_RATE7, 127u, 0u));
    CHECK(module_control106_receive_parameter(
        &m, RAW106_LFO_DELAY7, 0u, 0u));
    CHECK(module_control106_receive_parameter(
        &m, RAW106_DCO_LFO_DEPTH7, 127u, 0u));
    CHECK(module_control106_receive_parameter(
        &m, RAW106_VCF_CUTOFF7, 64u, 0u));
    CHECK(module_control106_receive_parameter(
        &m, RAW106_VCF_ENV_DEPTH7, 127u, 0u));
    CHECK(module_control106_receive_parameter(
        &m, RAW106_ENV_ATTACK7, 0u, 0u));
    CHECK(module_control106_receive_parameter(
        &m, RAW106_ENV_SUSTAIN7, 64u, 0u));
    CHECK(module_control106_receive_parameter(
        &m, RAW106_BEND_MAGNITUDE8, 100u, 0u));
    CHECK(module_control106_receive_parameter(
        &m, RAW106_DCO_BEND_SENSITIVITY8, 128u, 0u));
    CHECK(module_control106_receive_parameter(
        &m, RAW106_VCF_BEND_SENSITIVITY8, 128u, 0u));

    AssignerEvent106 on = {
        ASSIGNER106_EVENT_VOICE_ON, 0u, 60u
    };
    module_control106_apply_assigner_event(&m, on, 0u);
    Fm1junoTimeQ32 now = FM1JUNO_TIME_ONE_SAMPLE;
    (void)advance_until(&m, CONTROL106_LFO_DELAY_AND_MOD_SCALE,
                        CONTROL106_NO_VOICE, &now);
    CHECK(m.lfo_onset == 255u);
    CHECK(m.combined_dco_lfo_depth == 254u);

    (void)advance_until(&m, CONTROL106_PITCH_AND_PORTAMENTO,
                        CONTROL106_NO_VOICE, &now);
    const uint16_t bend = control106_dco_bend_8_8(100u, 128u);
    CHECK(m.active_global_pitch_offset_8_8 ==
          (uint16_t)(PITCH106_GLOBAL_BASE + bend));

    (void)advance_until(&m, CONTROL106_DCO_VOICE, 0u, &now);
    CHECK(m.voices[0].final_dco_pitch_8_8 ==
          (uint16_t)((60u << 8u) + PITCH106_GLOBAL_BASE + bend));
    CHECK(m.voices[0].timer.current_count ==
          pitch106_timer_count_from_pitch(&profile,
                    m.voices[0].final_dco_pitch_8_8));
    CHECK(m.voices[0].dco_cv_dac12 ==
          pitch106_dco_cv12_from_pitch(
                    m.voices[0].final_dco_pitch_8_8));

    (void)advance_until(&m, CONTROL106_VCF_GLOBAL_BASE,
                        CONTROL106_NO_VOICE, &now);
    CHECK(m.vcf_global_raw == (int32_t)(64u << 7u) +
          (int32_t)control106_vcf_bend_12(100u, 128u));

    (void)advance_until(&m, CONTROL106_ENV_VCA_VCF_VOICE, 0u, &now);
    CHECK(m.voices[0].envelope.level == ENVELOPE106_MAX);
    CHECK(m.voices[0].vcf_code14 > (64u << 7u));

    (void)advance_until(&m, CONTROL106_LFO_ADVANCE,
                        CONTROL106_NO_VOICE, &now);
    CHECK(m.lfo.magnitude == 4096u);
    CHECK(m.lfo.phase_flags == 0u);

    /* Next frame uses the previously advanced LFO waveform. */
    (void)advance_until(&m, CONTROL106_PITCH_AND_PORTAMENTO,
                        CONTROL106_NO_VOICE, &now);
    const uint16_t lfo_pitch = (uint16_t)(
        control106_mul16x8_hi(4096u, 254u) >> 3u);
    CHECK(m.active_global_pitch_offset_8_8 ==
          (uint16_t)(PITCH106_GLOBAL_BASE + bend + lfo_pitch));

    /* A note-on mid-pass aborts the remaining DCO/VCF services. */
    (void)advance_until(&m, CONTROL106_DCO_VOICE, 0u, &now);
    const uint32_t old_abandoned = m.scheduler.abandoned_passes;
    const uint16_t vcf_5_before = m.voices[5].vcf_code14;
    on.note = 67u;
    module_control106_apply_assigner_event(&m, on, now);
    CHECK(m.scheduler.abandoned_passes == old_abandoned + 1u);
    CHECK(module_control106_service_next(&m, now).type ==
          CONTROL106_FRAME_START);
    CHECK(m.voices[5].vcf_code14 == vcf_5_before);
    return EXIT_SUCCESS;
}

static int test_script_queue_invariance(void)
{
    ScriptEvent106 script[112];
    size_t n = 0u;
#define ADD(t,kind,v,val,a) do { \
    CHECK(n < sizeof script / sizeof script[0]); \
    script[n].time = (t); script[n].type = (kind); \
    script[n].voice = (v); script[n].value = (val); \
    script[n].aux = (a); ++n; \
} while (0)
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_PITCH_PROFILE,
        255u, 1u, 0u);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_PARAMETER,
        255u, 127u, RAW106_LFO_RATE7);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_PARAMETER,
        255u, 127u, RAW106_DCO_LFO_DEPTH7);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_PARAMETER,
        255u, 0u, RAW106_LFO_DELAY7);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_PARAMETER,
        255u, 64u, RAW106_PORTAMENTO_ADC8);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_PARAMETER,
        255u, 127u, RAW106_PWM_DEPTH7);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_PARAMETER,
        255u, 1u, RAW106_PULSE_ENABLED);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_PARAMETER,
        255u, 0u, RAW106_PWM_MANUAL);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_PARAMETER,
        255u, 95u, RAW106_VCF_RESONANCE7);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_PARAMETER,
        255u, 51u, RAW106_SUB_LEVEL7);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_PARAMETER,
        255u, 65u, RAW106_NOISE_LEVEL7);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_PARAMETER,
        255u, 64u, RAW106_GLOBAL_VCA_LEVEL7);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_PARAMETER,
        255u, 0u, RAW106_VCA_GATE_MODE);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_VOICE_ON,
        0u, 72u, 0u);
    for (uint32_t i = 1u; i <= 24u; ++i)
        ADD(fm1juno_time_from_samples(i), SCRIPT106_CONTROL_STEP,
            0u, 0u, 0u);
    ADD(fm1juno_time_from_samples(25u), SCRIPT106_PARAMETER,
        255u, 130u, RAW106_TUNE_ADC8);
    ADD(fm1juno_time_from_samples(25u) + 1u, SCRIPT106_PARAMETER,
        255u, 100u, RAW106_BEND_MAGNITUDE8);
    ADD(fm1juno_time_from_samples(25u) + 2u, SCRIPT106_PARAMETER,
        255u, 128u, RAW106_DCO_BEND_SENSITIVITY8);
    for (uint32_t i = 26u; i <= 56u; ++i)
        ADD(fm1juno_time_from_samples(i), SCRIPT106_CONTROL_STEP,
            0u, 0u, 0u);
    ADD(fm1juno_time_from_samples(57u), SCRIPT106_VOICE_ON,
        0u, 67u, 0u);
    for (uint32_t i = 58u; i <= 76u; ++i)
        ADD(fm1juno_time_from_samples(i), SCRIPT106_CONTROL_STEP,
            0u, 0u, 0u);
#undef ADD
    CHECK(event_replay106_validate(script, n));

    const uint32_t blocks[] = {16u, 32u, 64u, 128u, 256u};
    ReplayResult106 baseline = {0};
    CHECK(event_replay106_run(script, n, 128u, blocks[0], &baseline));
    CHECK(!baseline.trace_overflowed);
    CHECK(baseline.abandoned_passes == 2u);
    for (size_t i = 1u; i < sizeof blocks / sizeof blocks[0]; ++i) {
        ReplayResult106 next = {0};
        CHECK(event_replay106_run(script, n, 128u, blocks[i], &next));
        CHECK(!next.trace_overflowed);
        CHECK(next.trace_signature == baseline.trace_signature);
        CHECK(next.trace_count == baseline.trace_count);
        CHECK(next.pass_id == baseline.pass_id);
        CHECK(next.abandoned_passes == baseline.abandoned_passes);
    }

    ScriptEvent106 invalid = {
        fm1juno_time_from_samples(2u), SCRIPT106_PARAMETER,
        255u, 128u, RAW106_LFO_RATE7
    };
    CHECK(!event_replay106_validate(&invalid, 1u));
    invalid.value = 127u;
    invalid.voice = 0u;
    CHECK(!event_replay106_validate(&invalid, 1u));
    invalid.voice = 255u;
    invalid.aux = 999u;
    CHECK(!event_replay106_validate(&invalid, 1u));
    return EXIT_SUCCESS;
}

int main(void)
{
    if (test_parameter_validity_and_slot_causality()) return EXIT_FAILURE;
    if (test_lfo_bender_env_vcf_and_restart()) return EXIT_FAILURE;
    if (test_script_queue_invariance()) return EXIT_FAILURE;
    puts("fm1juno: timestamped raw parameter / control flow passed");
    return EXIT_SUCCESS;
}
