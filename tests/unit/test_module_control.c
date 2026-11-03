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

int main(void)
{
    if (test_voice_on_to_timer_reset_then_queue()) return EXIT_FAILURE;
    if (test_sustain_defers_release_and_lfo_rearm()) return EXIT_FAILURE;
    if (test_trace_determinism()) return EXIT_FAILURE;
    puts("fm1juno: module-control integration tests passed");
    return EXIT_SUCCESS;
}
