#include <stdio.h>
#include <stdlib.h>

#include "common/fixed_time.h"
#include "control106/module_control106.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return EXIT_FAILURE; \
    } \
} while (0)

static ControlEvent106 service_until(ModuleControl106 *m,
                                     ControlEventType106 wanted,
                                     uint8_t voice,
                                     Fm1junoTimeQ32 *now)
{
    for (unsigned guard = 0u; guard < 128u; ++guard) {
        const ControlEvent106 e = module_control106_service_next(m, *now);
        *now += FM1JUNO_TIME_ONE_SAMPLE;
        if (e.type == wanted &&
            (voice == CONTROL106_NO_VOICE || e.voice == voice))
            return e;
    }

    ControlEvent106 failure = {0};
    failure.type = CONTROL106_NOISE_LEVEL;
    failure.voice = CONTROL106_NO_VOICE;
    return failure;
}

static EnvelopeParams106 test_env_params(void)
{
    EnvelopeParams106 p;
    p.attack_increment = 0x0800u;
    p.decay_coefficient = 0xf000u;
    p.sustain_level = 0x2000u;
    p.release_coefficient = 0xe000u;
    return p;
}

static int test_voice_on_to_timer_reset_then_queue(void)
{
    ModuleControl106 m;
    module_control106_init(&m);
    module_control106_set_envelope_params(&m, 0u, test_env_params());
    module_control106_set_dco_command(&m, 0u, 10u, TIMER106_RANGE_8);

    AssignerEvent106 on = {ASSIGNER106_EVENT_VOICE_ON, 0u, 60u};
    Fm1junoTimeQ32 now = fm1juno_time_from_samples(10u);
    module_control106_apply_assigner_event(&m, on, now);

    CHECK((m.note_gate_bits & 1u) != 0u);
    CHECK(m.voices[0].envelope.phase == ENVELOPE106_ATTACK);
    CHECK(m.voices[0].timer_reset_pending);
    CHECK(m.scheduler.abandoned_passes == 1u);

    now += FM1JUNO_TIME_ONE_SAMPLE;
    const ControlEvent106 start = module_control106_service_next(&m, now);
    CHECK(start.type == CONTROL106_FRAME_START);
    CHECK((m.voice_run_bits & 1u) != 0u);
    CHECK(!m.lfo_delay.armed_after_silence);

    const ControlEvent106 dco0 = service_until(&m,
                                               CONTROL106_DCO_VOICE,
                                               0u,
                                               &now);
    CHECK(dco0.type == CONTROL106_DCO_VOICE);
    CHECK(m.voices[0].timer_initialized);
    CHECK(!m.voices[0].timer_reset_pending);
    CHECK(m.voices[0].timer.current_count == 10u);
    CHECK(!m.voices[0].timer.pending_valid);

    m.voices[0].envelope.level = 0x1800u;
    module_control106_set_dco_command(&m, 0u, 8u, TIMER106_RANGE_8);
    module_control106_apply_assigner_event(&m, on, now);
    CHECK(m.voices[0].envelope.phase == ENVELOPE106_ATTACK);
    CHECK(m.voices[0].envelope.level == 0x1800u);
    CHECK(!m.voices[0].timer_reset_pending);

    now += FM1JUNO_TIME_ONE_SAMPLE;
    CHECK(module_control106_service_next(&m, now).type == CONTROL106_FRAME_START);
    (void)service_until(&m, CONTROL106_DCO_VOICE, 0u, &now);
    CHECK(m.voices[0].timer.current_count == 10u);
    CHECK(m.voices[0].timer.pending_valid);
    CHECK(m.voices[0].timer.pending_count == 8u);
    return EXIT_SUCCESS;
}

static int test_sustain_defers_release_and_lfo_rearm(void)
{
    ModuleControl106 m;
    module_control106_init(&m);
    module_control106_set_envelope_params(&m, 0u, test_env_params());

    Fm1junoTimeQ32 now = 0u;
    AssignerEvent106 on = {ASSIGNER106_EVENT_VOICE_ON, 0u, 60u};
    AssignerEvent106 off = {ASSIGNER106_EVENT_VOICE_OFF, 0u, 60u};

    module_control106_apply_assigner_event(&m, on, now);
    now += FM1JUNO_TIME_ONE_SAMPLE;
    CHECK(module_control106_service_next(&m, now).type == CONTROL106_FRAME_START);
    CHECK((m.voice_run_bits & 1u) != 0u);

    module_control106_set_sustain(&m, true, now);
    module_control106_apply_assigner_event(&m, off, now);
    CHECK((m.note_gate_bits & 1u) == 0u);
    CHECK(m.voices[0].envelope.phase != ENVELOPE106_RELEASE);

    now += FM1JUNO_TIME_ONE_SAMPLE;
    CHECK(module_control106_service_next(&m, now).type == CONTROL106_FRAME_START);
    CHECK((m.voice_run_bits & 1u) != 0u);
    CHECK(!m.lfo_delay.armed_after_silence);

    module_control106_set_sustain(&m, false, now);
    CHECK(m.voices[0].envelope.phase == ENVELOPE106_RELEASE);

    (void)service_until(&m, CONTROL106_NOISE_LEVEL, CONTROL106_NO_VOICE, &now);
    CHECK(module_control106_service_next(&m, now).type == CONTROL106_FRAME_START);
    CHECK((m.voice_run_bits & 1u) == 0u);
    CHECK(m.lfo_delay.armed_after_silence);
    return EXIT_SUCCESS;
}

static uint64_t run_trace_scenario(void)
{
    ModuleControl106 m;
    module_control106_init(&m);
    for (uint8_t v = 0u; v < MODULE106_VOICES; ++v) {
        module_control106_set_envelope_params(&m, v, test_env_params());
        module_control106_set_dco_command(&m,
                                          v,
                                          (uint16_t)(100u + v),
                                          TIMER106_RANGE_8);
    }

    Assigner106 a;
    assigner106_init(&a, ASSIGNER106_SOLO_UNISON);
    assigner106_begin_scan(&a);
    const AssignerEventBatch106 batch = assigner106_solo_changed_note(&a,
                                                                       60u,
                                                                       true);
    const Fm1junoTimeQ32 spacing = FM1JUNO_TIME_ONE_SAMPLE / 4u;
    module_control106_apply_assigner_batch(&m, &batch, 0u, spacing);

    Fm1junoTimeQ32 now = fm1juno_time_from_samples(2u);
    for (unsigned i = 0u; i < 40u; ++i) {
        (void)module_control106_service_next(&m, now);
        now += FM1JUNO_TIME_ONE_SAMPLE;
    }
    CHECK(!m.trace.overflowed);
    return module_control106_trace_signature(&m.trace);
}

static int test_trace_determinism(void)
{
    const uint64_t a = run_trace_scenario();
    const uint64_t b = run_trace_scenario();
    CHECK(a == b);
    CHECK(a != 0u);
    return EXIT_SUCCESS;
}


static int test_vcf_slot_order_and_restart(void)
{
    ModuleControl106 m;
    module_control106_init(&m);
    VcfControlParams106 params = {0};
    params.cutoff7 = 64u;
    params.key_follow7 = 127u;
    module_control106_set_vcf_params(&m, params);

    const uint8_t notes[MODULE106_VOICES] = {48u, 54u, 60u, 66u, 72u, 84u};
    for (uint8_t v = 0u; v < MODULE106_VOICES; ++v) {
        module_control106_set_envelope_params(&m, v, test_env_params());
        module_control106_set_voice_pitch(&m, v, (uint16_t)notes[v] << 8u);
    }

    Fm1junoTimeQ32 now = 0u;
    (void)service_until(&m, CONTROL106_VCF_GLOBAL_BASE,
                        CONTROL106_NO_VOICE, &now);
    CHECK(m.vcf_global_raw == 8192);

    /* A newly received parameter changes RAM now, not the VCF base
     * that was already calculated earlier in this pass. */
    params.cutoff7 = 1u;
    module_control106_set_vcf_params(&m, params);
    CHECK(m.vcf_global_raw == 8192);

    (void)service_until(&m, CONTROL106_ENV_VCA_VCF_VOICE, 0u, &now);
    const uint16_t expected_old = vcf106_voice_code14(
        &params, 8192, 0u, (uint16_t)notes[0] << 8u);
    CHECK(m.voices[0].vcf_code14 == expected_old);
    CHECK(m.voices[0].vcf_dac12 == (expected_old >> 2u));
    CHECK(m.trace.events[m.trace.count - 1u].type == MODULE_TRACE106_VCF_S_H);
    CHECK(m.trace.events[m.trace.count - 1u].voice == 0u);

    /* Note-event restart abandons all still-unserviced voice slots. */
    const AssignerEvent106 on = {
        ASSIGNER106_EVENT_VOICE_ON, 0u, notes[0]
    };
    module_control106_apply_assigner_event(&m, on, now);
    for (uint8_t v = 1u; v < MODULE106_VOICES; ++v)
        CHECK(m.voices[v].vcf_code14 == 0u);
    CHECK(module_control106_service_next(&m, now).type ==
          CONTROL106_FRAME_START);
    now += FM1JUNO_TIME_ONE_SAMPLE;

    (void)service_until(&m, CONTROL106_VCF_GLOBAL_BASE,
                        CONTROL106_NO_VOICE, &now);
    CHECK(m.vcf_global_raw == 128);

    uint64_t sequence = 0u;
    for (uint8_t v = 0u; v < MODULE106_VOICES; ++v) {
        const ControlEvent106 event = service_until(
            &m, CONTROL106_ENV_VCA_VCF_VOICE, v, &now);
        CHECK(event.type == CONTROL106_ENV_VCA_VCF_VOICE);
        CHECK(event.voice == v && event.sequence > sequence);
        sequence = event.sequence;
        const uint16_t expected = vcf106_voice_code14(
            &params, 128, m.voices[v].envelope.level,
            (uint16_t)notes[v] << 8u);
        CHECK(m.voices[v].vcf_code14 == expected);
        CHECK(m.voices[v].vcf_dac12 == (expected >> 2u));
        const ModuleTraceEvent106 *last = &m.trace.events[m.trace.count - 1u];
        CHECK(last->type == MODULE_TRACE106_VCF_S_H);
        CHECK(last->voice == v);
        CHECK(last->value == m.voices[v].vcf_dac12);
    }
    CHECK(!m.trace.overflowed);
    return EXIT_SUCCESS;
}

static int test_shared_dco_pitch_count_cv_order(void)
{
    ModuleControl106 m;
    module_control106_init(&m);
    PitchTimerBase106 base;
    pitch106_timer_build_derived_candidate(&base);
    module_control106_set_pitch_timer_profile(&m, &base);
    module_control106_set_voice_pitch(&m, 0u, (uint16_t)(60u << 8u));
    module_control106_set_dco_command(&m, 0u, 500u, TIMER106_RANGE_8);
    module_control106_set_global_pitch_offset(&m, PITCH106_GLOBAL_BASE);

    Fm1junoTimeQ32 now = 0u;
    (void)service_until(&m, CONTROL106_PITCH_AND_PORTAMENTO,
                        CONTROL106_NO_VOICE, &now);
    CHECK(m.active_global_pitch_offset_8_8 == PITCH106_GLOBAL_BASE);

    /* Change requested Tune while a pass is in flight: the earlier
     * global pitch slot remains latched for all six DCO services. */
    module_control106_set_global_pitch_offset(
        &m, (uint16_t)(PITCH106_GLOBAL_BASE + 0x0100u));

    (void)service_until(&m, CONTROL106_DCO_VOICE, 0u, &now);
    const uint16_t old_pitch =
        pitch106_final((uint16_t)(60u << 8u), PITCH106_GLOBAL_BASE);
    const uint16_t old_count =
        pitch106_timer_count_from_pitch(&base, old_pitch);
    CHECK(m.voices[0].final_dco_pitch_8_8 == old_pitch);
    CHECK(m.voices[0].timer.current_count == old_count);
    CHECK(m.voices[0].timer.range == TIMER106_RANGE_8);
    CHECK(m.voices[0].dco_cv_dac12 ==
          pitch106_dco_cv12_from_pitch(old_pitch));
    CHECK(m.trace.count >= 2u);
    CHECK(m.trace.events[m.trace.count - 2u].type ==
          MODULE_TRACE106_TIMER_RESET_LOAD);
    CHECK(m.trace.events[m.trace.count - 1u].type ==
          MODULE_TRACE106_DCO_CV_S_H);
    CHECK(m.trace.events[m.trace.count - 1u].voice == 0u);

    /* Following frame takes the new global pitch; an ordinary timer
     * count update stays pending until its Mode-3 transition. */
    (void)service_until(&m, CONTROL106_PITCH_AND_PORTAMENTO,
                        CONTROL106_NO_VOICE, &now);
    CHECK(m.active_global_pitch_offset_8_8 ==
          (uint16_t)(PITCH106_GLOBAL_BASE + 0x0100u));
    (void)service_until(&m, CONTROL106_DCO_VOICE, 0u, &now);
    const uint16_t new_pitch = (uint16_t)(old_pitch + 0x0100u);
    const uint16_t new_count =
        pitch106_timer_count_from_pitch(&base, new_pitch);
    CHECK(new_count != old_count);
    CHECK(m.voices[0].final_dco_pitch_8_8 == new_pitch);
    CHECK(m.voices[0].timer.current_count == old_count);
    CHECK(m.voices[0].timer.pending_count == new_count);
    CHECK(m.voices[0].timer.pending_valid);
    CHECK(m.voices[0].dco_cv_dac12 ==
          pitch106_dco_cv12_from_pitch(new_pitch));
    CHECK(m.trace.events[m.trace.count - 2u].type ==
          MODULE_TRACE106_TIMER_QUEUE_COUNT);
    CHECK(m.trace.events[m.trace.count - 1u].type ==
          MODULE_TRACE106_DCO_CV_S_H);
    CHECK(!m.trace.overflowed);
    return EXIT_SUCCESS;
}

int main(void)
{
    if (test_voice_on_to_timer_reset_then_queue()) return EXIT_FAILURE;
    if (test_sustain_defers_release_and_lfo_rearm()) return EXIT_FAILURE;
    if (test_trace_determinism()) return EXIT_FAILURE;
    if (test_vcf_slot_order_and_restart()) return EXIT_FAILURE;
    if (test_shared_dco_pitch_count_cv_order()) return EXIT_FAILURE;
    puts("fm1juno: module-control integration tests passed");
    return EXIT_SUCCESS;
}
