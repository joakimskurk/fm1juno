#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/fixed_time.h"
#include "control106/event_replay106.h"
#include "control106/module_control106.h"
#include "control106/native_panel106.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return EXIT_FAILURE; \
    } \
} while (0)

static ControlEvent106 until(ModuleControl106 *m, ControlEventType106 type,
                             Fm1junoTimeQ32 *time)
{
    for (unsigned i = 0u; i < 80u; ++i) {
        ControlEvent106 ev = module_control106_service_next(m, *time);
        *time += FM1JUNO_TIME_ONE_SAMPLE;
        if (ev.type == type) return ev;
    }
    ControlEvent106 failure = {CONTROL106_FRAME_START, 255u, 0u, 0u};
    return failure;
}

static int test_switch_register_latch_masks_and_active_low(void)
{
    NativeSwitch106 s;
    native_switch106_init(&s);
    CHECK(s.ic40_register == 0x15u && s.ic40_output == 0x15u);
    CHECK(s.pf_prescaler_bits == 0x40u);

    native_switch106_receive1(&s, 0u);
    CHECK(s.ic40_register == 0x05u && s.ic40_output == 0x15u);
    CHECK(s.pf_prescaler_bits == 0xc0u);
    CHECK(native_switch106_pulse_enabled(&s));

    native_switch106_receive2(&s, 0x1fu);
    CHECK(s.ic40_register == 0x01u);
    CHECK(native_switch106_hpf_select(&s) == 0u);
    CHECK(!native_switch106_pwm_manual(&s));
    CHECK(!native_switch106_vcf_env_inverted(&s));
    CHECK(!native_switch106_vca_gate(&s));
    CHECK(s.ic40_output == 0x15u);

    native_switch106_receive2(&s, 0u);
    CHECK(s.ic40_register == 0x0du);
    CHECK(native_switch106_hpf_select(&s) == 3u);
    CHECK(native_switch106_pwm_manual(&s));
    CHECK(native_switch106_vcf_env_inverted(&s));
    CHECK(native_switch106_vca_gate(&s));

    native_switch106_receive1(&s, 0x76u);
    CHECK(s.ic40_register == 0x1eu);
    CHECK(s.pf_prescaler_bits == 0u);
    CHECK(native_switch106_pulse_enabled(&s));
    native_switch106_receive1(&s, 0x7eu);
    CHECK(!native_switch106_pulse_enabled(&s));
    CHECK(s.ic40_register == 0x1eu); /* pulse flag is not IC40 */
    native_switch106_frame_latch(&s);
    CHECK(s.ic40_output == 0x1eu);

    /* Three selection paths are literal active-low fallthroughs. */
    native_switch106_receive1(&s, 0x00u);
    CHECK(s.pf_prescaler_bits == 0xc0u);
    native_switch106_receive1(&s, 0x04u);
    CHECK(s.pf_prescaler_bits == 0x40u);
    native_switch106_receive1(&s, 0x06u);
    CHECK(s.pf_prescaler_bits == 0x00u);

    /* HPF selector is inverted in both incoming bits. */
    for (unsigned raw = 0u; raw < 4u; ++raw) {
        const uint8_t bytes = (uint8_t)(raw << 3u);
        native_switch106_receive2(&s, bytes);
        CHECK(native_switch106_hpf_select(&s) == ((raw ^ 3u) & 3u));
    }
    return EXIT_SUCCESS;
}

static int test_adc_hysteresis_and_two_slope_conditioner(void)
{
    const struct {uint8_t raw, expected;} cases[] = {
        {0u, 0u}, {1u, 0u}, {4u, 0u}, {5u, 1u},
        {238u, 234u}, {239u, 235u}, {240u, 237u},
        {249u, 255u}, {250u, 255u}, {255u, 255u}
    };
    for (size_t i = 0u; i < sizeof cases/sizeof cases[0]; ++i)
        CHECK(native_adc106_condition(cases[i].raw) == cases[i].expected);

    NativeAdc106 a;
    native_adc106_init(&a);
    uint8_t bank = 255u;
    CHECK(!native_adc106_receive_bank(&a, 2u, 0u));
    CHECK(!native_adc106_process_latest(&a, &bank));
    CHECK(native_adc106_receive_bank(&a, 0u, UINT32_C(0x04030201)));
    CHECK(native_adc106_process_latest(&a, &bank));
    CHECK(bank == 0u && a.held[0] == 0u); /* ±1 deadband */
    CHECK(a.held[1] == 2u && a.held[2] == 3u && a.held[3] == 4u);
    CHECK(a.conditioned[0] == 0u && a.conditioned[3] == 0u);

    CHECK(native_adc106_receive_bank(&a, 0u, UINT32_C(0x05040302)));
    CHECK(native_adc106_process_latest(&a, &bank));
    CHECK(a.held[0] == 2u && a.held[1] == 2u);
    CHECK(a.held[2] == 3u && a.held[3] == 4u);

    CHECK(native_adc106_receive_bank(&a, 0u, UINT32_C(0x64646464)));
    CHECK(native_adc106_process_latest(&a, &bank));
    CHECK(a.held[0] == 100u && a.conditioned[0] == 96u);
    CHECK(native_adc106_receive_bank(&a, 0u, UINT32_C(0x65656565)));
    CHECK(native_adc106_process_latest(&a, &bank));
    CHECK(a.held[0] == 100u && a.conditioned[0] == 96u);
    CHECK(native_adc106_receive_bank(&a, 0u, UINT32_C(0x66666666)));
    CHECK(native_adc106_process_latest(&a, &bank));
    CHECK(a.held[0] == 102u && a.conditioned[0] == 98u);

    /* ISR publishes two full banks; main-loop service reads last bank. */
    CHECK(native_adc106_receive_bank(&a, 0u, UINT32_C(0x11111111)));
    CHECK(native_adc106_receive_bank(&a, 1u, UINT32_C(0xf9efeeff)));
    CHECK(native_adc106_process_latest(&a, &bank));
    CHECK(bank == 1u);
    CHECK(a.held[4] == 255u && a.conditioned[4] == 255u);
    CHECK(a.held[5] == 238u && a.conditioned[5] == 234u);
    CHECK(a.held[6] == 239u && a.conditioned[6] == 235u);
    CHECK(a.held[7] == 249u && a.conditioned[7] == 255u);
    CHECK(a.raw[0] == 17u && a.held[0] == 102u);
    CHECK(a.published_snapshots == 7u && a.serviced_banks == 6u);
    return EXIT_SUCCESS;
}


static int test_module_switch_latch_adc_slot_and_restarts(void)
{
    ModuleControl106 m;
    module_control106_init(&m);
    const uint8_t original_output = m.native_switch.ic40_output;
    CHECK(!module_control106_receive_native_switch(&m, 0u, 1u, 0u));
    CHECK(!module_control106_receive_native_switch(&m, 1u, 256u, 0u));
    CHECK(!module_control106_receive_native_adc_bank(&m, 2u, 0u, 0u));
    CHECK(m.scheduler.parameter_interrupts == 0u);
    CHECK(m.scheduler.adc_interrupts == 0u);

    CHECK(module_control106_receive_native_switch(&m, 2u, 0u, 0u));
    CHECK(module_control106_receive_native_switch(&m, 1u, 0x06u, 0u));
    CHECK(m.raw_params_active);
    CHECK(m.raw_params.pwm_manual && m.raw_params.vca_gate_mode);
    CHECK(m.raw_params.vcf_env_inverted);
    CHECK(m.raw_params.pulse_enabled);
    CHECK(m.native_switch.pf_prescaler_bits == 0u);
    CHECK(m.native_switch.ic40_output == original_output);
    CHECK(m.scheduler.parameter_interrupts == 2u);
    CHECK(m.scheduler.abandoned_passes == 0u);
    Fm1junoTimeQ32 now = FM1JUNO_TIME_ONE_SAMPLE;
    CHECK(module_control106_service_next(&m, now).type == CONTROL106_FRAME_START);
    now += FM1JUNO_TIME_ONE_SAMPLE;
    CHECK(m.native_switch.ic40_output == m.native_switch.ic40_register);
    CHECK(m.native_switch.ic40_output != original_output);

    /* Four-channel ADC ISR, with no immediate semantic control change. */
    const uint32_t packed0 = UINT32_C(0xff904085);
    CHECK(module_control106_receive_native_adc_bank(&m, 0u, packed0, now));
    CHECK(m.scheduler.adc_interrupts == 1u);
    CHECK(m.raw_params.tune_adc8 == 128u);
    CHECK(m.raw_params.portamento_adc8 == 0u);
    const uint32_t pass = m.scheduler.pass_id;
    (void)until(&m, CONTROL106_PITCH_AND_PORTAMENTO, &now);
    CHECK(m.active_global_pitch_offset_8_8 == PITCH106_GLOBAL_BASE);
    CHECK(m.scheduler.pass_id == pass);

    CHECK(until(&m, CONTROL106_ADC_PERFORMANCE, &now).type ==
          CONTROL106_ADC_PERFORMANCE);
    CHECK(m.raw_params.tune_adc8 == 129u); /* 133 - 4 */
    CHECK(m.raw_params.portamento_adc8 == 60u);
    CHECK(m.raw_params.performance_trigger); /* 0x90 - 4 -> 0x8c */
    CHECK(m.raw_params.performance_sensitivity8 == 255u);
    CHECK(m.scheduler.pass_id == pass);
    CHECK(m.native_adc.serviced_banks == 1u);

    /* Semantics change later in the *next* PITCH_AND_PORTAMENTO. */
    (void)until(&m, CONTROL106_PITCH_AND_PORTAMENTO, &now);
    CHECK(m.active_global_pitch_offset_8_8 ==
          (uint16_t)(PITCH106_GLOBAL_BASE + 1u));

    const uint32_t packed1 = UINT32_C(0xc8008264);
    CHECK(module_control106_receive_native_adc_bank(&m, 1u, packed1, now));
    CHECK(m.raw_params.bend_magnitude8 == 0u);
    (void)until(&m, CONTROL106_ADC_PERFORMANCE, &now);
    CHECK(m.raw_params.vcf_bend_sensitivity8 == 96u);
    CHECK(m.raw_params.dco_bend_sensitivity8 == 126u);
    CHECK(m.raw_params.bend_negative); /* processed 0 -> bit7 clear */
    CHECK(m.raw_params.bend_magnitude8 == 196u);
    CHECK(!m.trace.overflowed);

    /* A native packet is a parameter/ADC interrupt, NOT note-restart. */
    const uint32_t abandoned = m.scheduler.abandoned_passes;
    CHECK(module_control106_receive_native_switch(&m, 1u, 0x0eu, now));
    CHECK(m.scheduler.abandoned_passes == abandoned);
    return EXIT_SUCCESS;
}

static int test_native_script_callback_invariance(void)
{
    ScriptEvent106 script[80];
    size_t n = 0u;
#define ADD(t,kind,vc,val,auxval) do { \
    script[n].time = (t); script[n].type = (kind); \
    script[n].voice = (vc); script[n].value = (val); \
    script[n].aux = (auxval); ++n; \
} while (0)
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_SWITCH1,
        255u, 0x06u, 0u);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_SWITCH2,
        255u, 0x18u, 0u);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_ADC_BANK,
        0u, UINT32_C(0x80904085), 0u);
    ADD(fm1juno_time_from_samples(0u), SCRIPT106_VOICE_ON,
        0u, 60u, 0u);
    for (uint32_t i = 1u; i <= 15u; ++i)
        ADD(fm1juno_time_from_samples(i), SCRIPT106_CONTROL_STEP,
            0u, 0u, 0u);
    ADD(fm1juno_time_from_samples(16u), SCRIPT106_SWITCH2,
        255u, 0x00u, 0u);
    ADD(fm1juno_time_from_samples(16u) + 1u, SCRIPT106_ADC_BANK,
        1u, UINT32_C(0xc8808264), 0u);
    for (uint32_t i = 17u; i <= 48u; ++i)
        ADD(fm1juno_time_from_samples(i), SCRIPT106_CONTROL_STEP,
            0u, 0u, 0u);
#undef ADD
    CHECK(event_replay106_validate(script, n));
    const uint32_t blocks[] = {16u, 32u, 64u, 128u, 256u};
    ReplayResult106 baseline = {0};
    CHECK(event_replay106_run(script, n, 128u, blocks[0], &baseline));
    CHECK(!baseline.trace_overflowed);
    CHECK(baseline.abandoned_passes == 1u);
    for (size_t i = 1u; i < sizeof blocks/sizeof blocks[0]; ++i) {
        ReplayResult106 next = {0};
        CHECK(event_replay106_run(script, n, 128u, blocks[i], &next));
        CHECK(next.trace_signature == baseline.trace_signature);
        CHECK(next.trace_count == baseline.trace_count);
        CHECK(!next.trace_overflowed);
        CHECK(next.abandoned_passes == baseline.abandoned_passes);
    }

    ScriptEvent106 invalid = {
        fm1juno_time_from_samples(0u),
        SCRIPT106_SWITCH1, 255u, 256u, 0u
    };
    CHECK(!event_replay106_validate(&invalid, 1u));
    invalid.value = 255u;
    invalid.aux = 1u;
    CHECK(!event_replay106_validate(&invalid, 1u));
    invalid.aux = 0u;
    invalid.voice = 0u;
    CHECK(!event_replay106_validate(&invalid, 1u));
    invalid.type = SCRIPT106_ADC_BANK;
    invalid.voice = 2u;
    CHECK(!event_replay106_validate(&invalid, 1u));
    invalid.voice = 1u;
    CHECK(event_replay106_validate(&invalid, 1u));
    invalid.aux = 1u;
    CHECK(!event_replay106_validate(&invalid, 1u));
    return EXIT_SUCCESS;
}


static int test_native_serial_running_status_and_note_restart(void)
{
    ModuleControl106 m;
    module_control106_init(&m);
    CHECK(!module_control106_receive_serial_byte(&m, 256u, 0u));
    CHECK(m.serial_command == 0u);
    CHECK(m.scheduler.parameter_interrupts == 0u);
    /* Data without command is ignored, like the original ISR. */
    CHECK(module_control106_receive_serial_byte(&m, 77u, 0u));
    CHECK(m.scheduler.parameter_interrupts == 0u);

    CHECK(module_control106_receive_serial_byte(&m, 0x8eu, 0u));
    CHECK(m.serial_command == 0x8eu);
    CHECK(module_control106_receive_serial_byte(&m, 0x06u, 0u));
    CHECK(m.native_switch.switch1_seen && m.native_switch.switch1 == 6u);
    CHECK(m.serial_command == 0x8fu);
    CHECK(module_control106_receive_serial_byte(&m, 0x18u, 0u));
    CHECK(m.native_switch.switch2_seen && m.native_switch.switch2 == 0x18u);
    CHECK(m.serial_command == 0x90u);
    CHECK(module_control106_receive_serial_byte(&m, 127u, 0u));
    CHECK(m.raw_params.lfo_rate7 == 127u);
    CHECK(m.serial_command == 0x91u);
    CHECK(module_control106_receive_serial_byte(&m, 126u, 0u));
    CHECK(m.raw_params.lfo_delay7 == 126u);
    CHECK(m.serial_command == 0x92u);

    /* Every original patch entry 0x90..0x9f has a real destination;
     * the native event is NOT the same as a semantic PARAMETER ID. */
    const struct {uint8_t cmd; RawParameterId106 id;} entries[] = {
        {0x90u, RAW106_LFO_RATE7},
        {0x91u, RAW106_LFO_DELAY7},
        {0x92u, RAW106_DCO_LFO_DEPTH7},
        {0x93u, RAW106_PWM_DEPTH7},
        {0x94u, RAW106_NOISE_LEVEL7},
        {0x95u, RAW106_VCF_CUTOFF7},
        {0x96u, RAW106_VCF_RESONANCE7},
        {0x97u, RAW106_VCF_ENV_DEPTH7},
        {0x98u, RAW106_VCF_LFO_DEPTH7},
        {0x99u, RAW106_VCF_KEY_FOLLOW7},
        {0x9au, RAW106_GLOBAL_VCA_LEVEL7},
        {0x9bu, RAW106_ENV_ATTACK7},
        {0x9cu, RAW106_ENV_DECAY7},
        {0x9du, RAW106_ENV_SUSTAIN7},
        {0x9eu, RAW106_ENV_RELEASE7},
        {0x9fu, RAW106_SUB_LEVEL7}
    };
    for (size_t i = 0u; i < sizeof entries/sizeof entries[0]; ++i) {
        CHECK(module_control106_receive_serial_byte(&m, entries[i].cmd, 0u));
        CHECK(module_control106_receive_serial_byte(&m, 77u, 0u));
        CHECK(m.serial_command == (uint8_t)(entries[i].cmd + 1u));
        RawParams106 probe;
        raw_params106_init(&probe);
        CHECK(raw_params106_set(&probe, entries[i].id, 77u));
        /* Compare one changed field by structurally matching after setting
         * it in a fresh zeroed parameter state. */
        switch (entries[i].id) {
        case RAW106_LFO_RATE7: CHECK(m.raw_params.lfo_rate7 == 77u); break;
        case RAW106_LFO_DELAY7: CHECK(m.raw_params.lfo_delay7 == 77u); break;
        case RAW106_DCO_LFO_DEPTH7: CHECK(m.raw_params.dco_lfo_depth7 == 77u); break;
        case RAW106_PWM_DEPTH7: CHECK(m.raw_params.pwm_depth7 == 77u); break;
        case RAW106_NOISE_LEVEL7: CHECK(m.raw_params.noise_level7 == 77u); break;
        case RAW106_VCF_CUTOFF7: CHECK(m.raw_params.vcf_cutoff7 == 77u); break;
        case RAW106_VCF_RESONANCE7: CHECK(m.raw_params.vcf_resonance7 == 77u); break;
        case RAW106_VCF_ENV_DEPTH7: CHECK(m.raw_params.vcf_env_depth7 == 77u); break;
        case RAW106_VCF_LFO_DEPTH7: CHECK(m.raw_params.vcf_lfo_depth7 == 77u); break;
        case RAW106_VCF_KEY_FOLLOW7: CHECK(m.raw_params.vcf_key_follow7 == 77u); break;
        case RAW106_GLOBAL_VCA_LEVEL7: CHECK(m.raw_params.global_vca_level7 == 77u); break;
        case RAW106_ENV_ATTACK7: CHECK(m.raw_params.env_attack7 == 77u); break;
        case RAW106_ENV_DECAY7: CHECK(m.raw_params.env_decay7 == 77u); break;
        case RAW106_ENV_SUSTAIN7: CHECK(m.raw_params.env_sustain7 == 77u); break;
        case RAW106_ENV_RELEASE7: CHECK(m.raw_params.env_release7 == 77u); break;
        case RAW106_SUB_LEVEL7: CHECK(m.raw_params.sub_level7 == 77u); break;
        default: return EXIT_FAILURE;
        }
    }

    CHECK(module_control106_receive_serial_byte(&m, 0xa0u, 0u));
    CHECK(module_control106_receive_serial_byte(&m, 127u, 0u));
    CHECK(m.raw_params.bend_negative && m.raw_params.bend_magnitude8 == 255u);
    CHECK(m.serial_command == 0xa1u);
    CHECK(module_control106_receive_serial_byte(&m, 0u, 0u));
    CHECK(!m.raw_params.bend_negative && m.raw_params.bend_magnitude8 == 0u);
    CHECK(m.serial_command == 0xa2u);
    CHECK(module_control106_receive_serial_byte(&m, 127u, 0u));
    CHECK(m.raw_params.midi_mod_depth8 == 254u);
    CHECK(m.serial_command == 0xa3u);
    CHECK(module_control106_receive_serial_byte(&m, 1u, 0u));
    CHECK(m.serial_command == 0xa4u); /* test mode deliberately ignored */

    /* Native voice note path aborts current macro pass; switch/patch
     * receive paths above only increment the parameter interrupt count. */
    const uint32_t before = m.scheduler.abandoned_passes;
    CHECK(module_control106_receive_serial_byte(&m, 0x88u, 0u));
    CHECK(module_control106_receive_serial_byte(&m, 60u, 0u));
    CHECK(m.note_gate_bits == 1u);
    CHECK(m.scheduler.abandoned_passes == before + 1u);
    CHECK(module_control106_receive_serial_byte(&m, 0x86u, 0u));
    CHECK(m.sustain_on);
    CHECK(module_control106_receive_serial_byte(&m, 0x87u, 0u));
    CHECK(!m.sustain_on);
    CHECK(module_control106_receive_serial_byte(&m, 0x80u, 0u));
    CHECK(m.note_gate_bits == 0u);
    CHECK(m.scheduler.abandoned_passes == before + 2u);
    CHECK(!m.trace.overflowed);
    return EXIT_SUCCESS;
}


static int test_adc_only_snapshot_does_not_activate_early_controls(void)
{
    ModuleControl106 m;
    module_control106_init(&m);
    const EnvelopeParams106 injected = {
        0x0800u, 0xf000u, 0x2000u, 0xe000u
    };
    module_control106_set_envelope_params(&m, 0u, injected);
    AssignerEvent106 note = {ASSIGNER106_EVENT_VOICE_ON, 0u, 60u};
    module_control106_apply_assigner_event(&m, note, 0u);

    CHECK(!m.raw_params_active);
    CHECK(module_control106_receive_native_adc_bank(&m, 0u,
                 UINT32_C(0xff904085), 0u));
    CHECK(!m.raw_params_active);
    CHECK(m.raw_params.tune_adc8 == 128u);
    Fm1junoTimeQ32 now = FM1JUNO_TIME_ONE_SAMPLE;
    (void)until(&m, CONTROL106_ENV_VCA_VCF_VOICE, &now);
    CHECK(!m.raw_params_active);
    CHECK(m.voices[0].envelope_params.attack_increment == 0x0800u);
    CHECK(m.voices[0].envelope.level == 0x0800u);
    CHECK(m.raw_params.tune_adc8 == 128u);

    (void)until(&m, CONTROL106_ADC_PERFORMANCE, &now);
    CHECK(m.raw_params_active);
    CHECK(m.raw_params.tune_adc8 == 129u);
    CHECK(m.raw_params.portamento_adc8 == 60u);
    CHECK(!m.trace.overflowed);
    return EXIT_SUCCESS;
}

int main(void)
{
    if (test_switch_register_latch_masks_and_active_low()) return EXIT_FAILURE;
    if (test_adc_hysteresis_and_two_slope_conditioner()) return EXIT_FAILURE;
    if (test_module_switch_latch_adc_slot_and_restarts()) return EXIT_FAILURE;
    if (test_native_script_callback_invariance()) return EXIT_FAILURE;
    if (test_native_serial_running_status_and_note_restart()) return EXIT_FAILURE;
    if (test_adc_only_snapshot_does_not_activate_early_controls()) return EXIT_FAILURE;
    puts("fm1juno: native switch and ADC preprocessing tests passed");
    return EXIT_SUCCESS;
}
