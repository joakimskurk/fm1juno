#include <stdio.h>
#include <stdlib.h>

#include "common/fixed_time.h"
#include "control106/assigner106.h"
#include "control106/control_scheduler106.h"
#include "control106/control_math106.h"
#include "control106/envelope106.h"
#include "control106/lfo106.h"
#include "control106/lfo_delay106.h"
#include "control106/portamento106.h"
#include "control106/pitch106.h"
#include "control106/dac106.h"
#include "timer106/timer8253_106.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static int test_poly2_allocator(void)
{
    Assigner106 a;
    assigner106_init(&a, ASSIGNER106_POLY2);
    for (uint8_t v = 0u; v < 6u; ++v) {
        const AssignerEventBatch106 b = assigner106_note_on(&a, (uint8_t)(60u + v));
        CHECK(b.count == 1u);
        CHECK(b.events[0].voice == v);
    }
    CHECK(assigner106_note_on(&a, 72u).count == 0u);
    AssignerEventBatch106 off = assigner106_note_off(&a, 60u);
    CHECK(off.count == 1u && off.events[0].voice == 0u);
    CHECK(a.slots[0].free);
    CHECK(a.slots[0].note == 60u);
    const AssignerEventBatch106 next = assigner106_note_on(&a, 72u);
    CHECK(next.count == 1u && next.events[0].voice == 0u);
    return 0;
}

static int test_poly1_release_queue_and_same_note_reuse(void)
{
    Assigner106 a;
    assigner106_init(&a, ASSIGNER106_POLY1);
    CHECK(assigner106_note_on(&a, 60u).events[0].voice == 0u);
    CHECK(assigner106_note_on(&a, 62u).events[0].voice == 1u);
    CHECK(assigner106_note_on(&a, 64u).events[0].voice == 2u);
    const AssignerEventBatch106 off = assigner106_note_off(&a, 62u);
    CHECK(off.count == 1u && off.events[0].voice == 1u);
    CHECK(a.poly1_active_count == 2u);
    CHECK(a.poly1_order[0] == 0u);
    CHECK(a.poly1_order[1] == 2u);
    CHECK(a.poly1_order[5] == 1u);
    const AssignerEventBatch106 different = assigner106_note_on(&a, 65u);
    CHECK(different.count == 1u);
    CHECK(different.events[0].voice == 3u);
    const AssignerEventBatch106 replay = assigner106_note_on(&a, 62u);
    CHECK(replay.count == 1u);
    CHECK(replay.events[0].voice == 1u);
    CHECK(a.poly1_order[3] == 1u);
    return 0;
}

static int test_solo_scan_arbitration(void)
{
    Assigner106 a;
    assigner106_init(&a, ASSIGNER106_SOLO_UNISON);
    assigner106_begin_scan(&a);
    AssignerEventBatch106 on = assigner106_solo_changed_note(&a, 60u, true);
    CHECK(on.count == 6u);
    for (uint8_t v = 0u; v < 6u; ++v) {
        CHECK(on.events[v].type == ASSIGNER106_EVENT_VOICE_ON);
        CHECK(on.events[v].voice == v);
        CHECK(on.events[v].note == 60u);
    }
    CHECK(assigner106_solo_changed_note(&a, 64u, true).count == 0u);
    CHECK(!assigner106_end_scan(&a));
    assigner106_begin_scan(&a);
    AssignerEventBatch106 off = assigner106_solo_changed_note(&a, 60u, false);
    CHECK(off.count == 6u);
    CHECK(assigner106_solo_changed_note(&a, 64u, true).count == 0u);
    CHECK(assigner106_end_scan(&a));
    return 0;
}

static int test_module_retrigger_matrix(void)
{
    ModuleVoiceOnDecision106 d;
    d = module106_voice_on_decision(true, 60u, true, 60u);
    CHECK(d.request_attack && !d.request_timer_reset);
    d = module106_voice_on_decision(true, 60u, false, 60u);
    CHECK(d.request_attack && !d.request_timer_reset);
    d = module106_voice_on_decision(true, 60u, true, 62u);
    CHECK(d.request_attack && !d.request_timer_reset);
    d = module106_voice_on_decision(true, 60u, false, 62u);
    CHECK(d.request_attack && d.request_timer_reset);
    d = module106_voice_on_decision(false, 0u, false, 60u);
    CHECK(d.request_attack && d.request_timer_reset);
    CHECK(module106_update_voice_run(0x01u, 0x02u, false) == 0x01u);
    CHECK(module106_update_voice_run(0x01u, 0x02u, true) == 0x03u);
    CHECK(module106_update_voice_run(0xffu, 0xffu, true) == 0x3fu);
    return 0;
}

static int test_control_math(void)
{
    CHECK(control106_mul16x8_hi(0x3fffu, 0xffu) ==
          (uint16_t)(((uint32_t)0x3fffu * 0xffu) >> 8u));
    CHECK(control106_dco_bend_8_8(0u, 255u) == 0u);
    CHECK(control106_dco_bend_8_8(255u, 255u) == 3063u);
    CHECK(control106_vcf_bend_12(255u, 255u) == 4064u);
    CHECK(control106_tune_8_8(0u) == -127);
    CHECK(control106_tune_8_8(128u) == 0);
    CHECK(control106_tune_8_8(255u) == 127);
    CHECK(pitch106_global_offset(127, 0, 3063) ==
          (uint16_t)(PITCH106_GLOBAL_BASE + 127u + 3063u));
    CHECK(pitch106_final(0x3000u, 0x1818u) == 0x4818u);
    return 0;
}

static int test_stateful_envelope(void)
{
    Envelope106 env;
    EnvelopeParams106 p = {0x0800u, 0xf000u, 0x2000u, 0xe000u};
    envelope106_init(&env);
    CHECK(env.level == 0u && env.phase == ENVELOPE106_RELEASE);
    envelope106_note_on(&env);
    CHECK(envelope106_tick(&env, &p) == 0x0800u);
    CHECK(envelope106_tick(&env, &p) == 0x1000u);
    env.level = 0x1800u;
    envelope106_note_on(&env);
    CHECK(envelope106_tick(&env, &p) == 0x2000u);
    while (env.phase == ENVELOPE106_ATTACK)
        (void)envelope106_tick(&env, &p);
    CHECK(env.level == ENVELOPE106_MAX);
    CHECK(env.phase == ENVELOPE106_DECAY);
    for (unsigned i = 0u; i < 4096u && env.phase == ENVELOPE106_DECAY; ++i)
        (void)envelope106_tick(&env, &p);
    CHECK(env.phase == ENVELOPE106_SUSTAIN);
    CHECK(env.level == p.sustain_level);
    envelope106_note_off(&env);
    CHECK(env.phase == ENVELOPE106_RELEASE);
    CHECK(envelope106_tick(&env, &p) < p.sustain_level);
    return 0;
}

static int test_lfo_delay_and_performance_path(void)
{
    LfoDelay106 d;
    lfo_delay106_init(&d);
    CHECK(d.armed_after_silence);
    CHECK(!lfo_delay106_observe_voice_run(&d, 0u));
    CHECK(lfo_delay106_observe_voice_run(&d, 1u));
    CHECK(!d.armed_after_silence);
    CHECK(lfo_delay106_tick(&d, 0x2000u, 0x1000u) == 0u);
    CHECK(d.phase == LFO_DELAY106_HOLD);
    CHECK(lfo_delay106_tick(&d, 0x2000u, 0x1000u) == 0x10u);
    CHECK(d.phase == LFO_DELAY106_RAMP);
    CHECK(lfo_delay106_tick(&d, 0x2000u, 0xf000u) == 0xffu);
    CHECK(d.phase == LFO_DELAY106_FULL);
    CHECK(!lfo_delay106_observe_voice_run(&d, 0u));
    CHECK(d.armed_after_silence);
    CHECK(lfo_delay106_observe_voice_run(&d, 0x20u));
    CHECK(d.phase == LFO_DELAY106_HOLD && d.ramp_accum == 0u);

    PerformanceLfo106 perf;
    performance_lfo106_init(&perf);
    CHECK(performance_lfo106_tick(&perf, true, 25u, 0u) == 10u);
    CHECK(performance_lfo106_tick(&perf, true, 25u, 0u) == 20u);
    CHECK(performance_lfo106_tick(&perf, true, 25u, 0u) == 25u);
    CHECK(performance_lfo106_tick(&perf, false, 25u, 255u) == 24u);
    CHECK(lfo_delay106_combine_depth(200u, 0u, 50u) == 50u);
    CHECK(lfo_delay106_combine_depth(200u, 128u, 0u) == 100u);
    CHECK(lfo_delay106_combine_depth(255u, 255u, 20u) == 255u);
    return 0;
}

static int test_envelope_primitives(void)
{
    CHECK(envelope106_attack_tick(0u, 0x4000u) == ENVELOPE106_MAX);
    CHECK(envelope106_attack_tick(0x1000u, 0x0200u) == 0x1200u);
    CHECK(envelope106_attack_tick(0x3f00u, 0x0200u) == ENVELOPE106_MAX);
    const uint16_t value = 0x2a55u;
    const uint16_t coeff = 0xf680u;
    const uint16_t expected = (uint16_t)(
        ((uint32_t)0x2au * 0xf6u) +
        (((uint32_t)0x2au * 0x80u) >> 8u) +
        (((uint32_t)0x55u * 0xf6u) >> 8u));
    CHECK(envelope106_calc_decay(value, coeff) == expected);
    CHECK(envelope106_release_tick(value, coeff) == expected);
    CHECK(envelope106_decay_tick(0x2000u, 0x1000u, 0xf000u) >= 0x1000u);
    CHECK(envelope106_decay_tick(0x0800u, 0x1000u, 0xf000u) == 0x1000u);
    return 0;
}

static int test_lfo_rate_law(void)
{
    struct Anchor { uint8_t i; uint16_t c; } anchors[] = {
        {0u, 5u}, {2u, 25u}, {3u, 40u}, {5u, 70u}, {6u, 80u}, {7u, 90u},
        {8u, 100u}, {63u, 650u}, {64u, 666u}, {95u, 1162u},
        {96u, 1214u}, {102u, 1526u}, {103u, 1580u}, {104u, 1650u},
        {105u, 1720u}, {106u, 1800u}, {109u, 2040u}, {110u, 2140u},
        {119u, 3040u}, {120u, 3160u}, {122u, 3400u}, {123u, 3550u},
        {126u, 4000u}, {127u, 4096u}
    };
    for (size_t n = 0; n < sizeof anchors / sizeof anchors[0]; ++n)
        CHECK(lfo106_rate_coefficient(anchors[n].i) == anchors[n].c);
    uint16_t prev = 0u;
    for (unsigned i = 0; i < 128u; ++i) {
        const uint16_t c = lfo106_rate_coefficient((uint8_t)i);
        CHECK(c > prev);
        CHECK(lfo106_half_steps(c) == (uint16_t)((8192u + c - 1u) / c));
        prev = c;
    }
    CHECK(lfo106_half_steps(4096u) == 2u);
    return 0;
}

static int test_lfo_phase_sequence(void)
{
    Lfo106State lfo;
    lfo106_init(&lfo, 127u);
    CHECK(lfo.magnitude == 0u && lfo.phase_flags == 0u);
    CHECK(lfo106_tick(&lfo) == 4096);
    CHECK(lfo.phase_flags == 0u);
    CHECK(lfo106_tick(&lfo) == 8191);
    CHECK(lfo.magnitude == 8191u && lfo.phase_flags == 1u);
    CHECK(lfo106_tick(&lfo) == 4095);
    CHECK(lfo106_tick(&lfo) == 0);
    CHECK(lfo.phase_flags == 2u);
    CHECK(lfo106_tick(&lfo) == -4096);
    CHECK(lfo106_tick(&lfo) == -8191);
    CHECK(lfo.phase_flags == 3u);
    CHECK(lfo106_tick(&lfo) == -4095);
    CHECK(lfo106_tick(&lfo) == 0);
    CHECK((lfo.phase_flags & 3u) == 0u);
    return 0;
}

static int test_portamento(void)
{
    CHECK(portamento106_index_from_raw(0u) == 0u);
    CHECK(portamento106_index_from_raw(1u) == 0u);
    CHECK(portamento106_index_from_raw(2u) == 1u);
    CHECK(portamento106_index_from_raw(255u) == 127u);
    CHECK(portamento106_step_toward(0x1000u, 0x1100u, 0u) == 0x1100u);
    CHECK(portamento106_step_toward(0x1000u, 0x1100u, 0x40u) == 0x1040u);
    CHECK(portamento106_step_toward(0x10f0u, 0x1100u, 0x40u) == 0x1100u);
    CHECK(portamento106_step_toward(0x1100u, 0x1000u, 0x40u) == 0x10c0u);
    CHECK(portamento106_step_toward(0x1010u, 0x1000u, 0x40u) == 0x1000u);
    return 0;
}

static int test_fixed_time(void)
{
    const Fm1junoTimeQ32 t = fm1juno_time_from_samples(42u);
    CHECK(fm1juno_time_sample_index(t) == 42u);
    CHECK(fm1juno_time_fraction(t) == 0u);
    CHECK(fm1juno_time_add(t, FM1JUNO_TIME_ONE_SAMPLE) == fm1juno_time_from_samples(43u));
    return 0;
}

static int test_dac_boundary(void)
{
    CHECK(dac106_code14_to_dac12(0x0000u) == 0x000u);
    CHECK(dac106_code14_to_dac12(0x0003u) == 0x000u);
    CHECK(dac106_code14_to_dac12(0x0004u) == 0x001u);
    CHECK(dac106_code14_to_dac12(0x3fffu) == 0xfffu);
    CHECK(dac106_code14_to_dac12(0xffffu) == 0xfffu);
    return 0;
}

static int test_timer_subsample_timeline(void)
{
    CHECK(timer8253_106_input_clock_hz(TIMER106_RANGE_16) == 1000000u);
    CHECK(timer8253_106_input_clock_hz(TIMER106_RANGE_8) == 2000000u);
    CHECK(timer8253_106_input_clock_hz(TIMER106_RANGE_4) == 4000000u);
    CHECK(timer8253_106_clock_q32(TIMER106_RANGE_16) == UINT64_C(189408058));
    CHECK(timer8253_106_clock_q32(TIMER106_RANGE_8) == UINT64_C(94704029));
    CHECK(timer8253_106_clock_q32(TIMER106_RANGE_4) == UINT64_C(47352014));
    Timer8253Voice106 t;
    const Fm1junoTimeQ32 start = fm1juno_time_from_samples(100u);
    timer8253_106_reset_and_load_at(&t, 10u, TIMER106_RANGE_8, start);
    const Fm1junoTimeQ32 first_edge = start + timer8253_106_clocks_to_time_q32(TIMER106_RANGE_8, 5u);
    CHECK(t.next_edge_time == first_edge);
    timer8253_106_request_count(&t, 8u);
    CHECK(t.next_edge_time == first_edge);
    CHECK(timer8253_106_commit_edge(&t) == first_edge);
    CHECK(!t.output_high && t.current_count == 8u && !t.pending_valid && t.clocks_remaining == 4u);
    CHECK(t.next_edge_time == first_edge + timer8253_106_clocks_to_time_q32(TIMER106_RANGE_8, 4u));
    const Fm1junoTimeQ32 reset_at = fm1juno_time_from_samples(123u) + 17u;
    timer8253_106_reset_and_load_at(&t, 9u, TIMER106_RANGE_4, reset_at);
    CHECK(t.output_high);
    CHECK(t.next_edge_time == reset_at + timer8253_106_clocks_to_time_q32(TIMER106_RANGE_4, 5u));
    return 0;
}

static int test_timer_mode3_lengths(void)
{
    CHECK(timer8253_106_high_clocks(10u) == 5u);
    CHECK(timer8253_106_low_clocks(10u) == 5u);
    CHECK(timer8253_106_high_clocks(9u) == 5u);
    CHECK(timer8253_106_low_clocks(9u) == 4u);
    CHECK(timer8253_106_effective_count(0u) == 65536u);
    CHECK(timer8253_106_high_clocks(0u) == 32768u);
    CHECK(timer8253_106_low_clocks(0u) == 32768u);
    return 0;
}

static int test_timer_pending_vs_reset(void)
{
    Timer8253Voice106 t;
    timer8253_106_reset_and_load(&t, 10u);
    CHECK(t.output_high && t.current_count == 10u && t.clocks_remaining == 5u);
    timer8253_106_request_count(&t, 8u);
    CHECK(t.current_count == 10u && t.pending_valid && t.clocks_remaining == 5u);
    timer8253_106_advance_half_cycle(&t);
    CHECK(!t.output_high && t.current_count == 8u && !t.pending_valid && t.clocks_remaining == 4u);
    timer8253_106_reset_and_load(&t, 9u);
    CHECK(t.output_high && t.current_count == 9u && t.clocks_remaining == 5u);
    return 0;
}

static int test_scheduler_order(void)
{
    ControlScheduler106 s;
    control_scheduler106_init(&s);
    const ControlEventType106 global_prefix[] = {
        CONTROL106_FRAME_START, CONTROL106_LATCH_AND_VOICERUN,
        CONTROL106_VCF_RESONANCE, CONTROL106_LFO_DELAY_AND_MOD_SCALE,
        CONTROL106_GLOBAL_VCA_LEVEL, CONTROL106_PITCH_AND_PORTAMENTO,
        CONTROL106_SUB_LEVEL
    };
    for (size_t i = 0; i < sizeof global_prefix / sizeof global_prefix[0]; ++i) {
        const ControlEvent106 e = control_scheduler106_next(&s);
        CHECK(e.type == global_prefix[i] && e.voice == CONTROL106_NO_VOICE && e.pass_id == 0u);
    }
    for (uint8_t v = 0; v < 6u; ++v) {
        const ControlEvent106 e = control_scheduler106_next(&s);
        CHECK(e.type == CONTROL106_DCO_VOICE && e.voice == v);
    }
    CHECK(control_scheduler106_next(&s).type == CONTROL106_VCF_GLOBAL_BASE);
    for (uint8_t v = 0; v < 6u; ++v) {
        const ControlEvent106 e = control_scheduler106_next(&s);
        CHECK(e.type == CONTROL106_ENV_VCA_VCF_VOICE && e.voice == v);
    }
    CHECK(control_scheduler106_next(&s).type == CONTROL106_ADC_PERFORMANCE);
    CHECK(control_scheduler106_next(&s).type == CONTROL106_FINAL_VCA_VOICE6);
    CHECK(control_scheduler106_next(&s).type == CONTROL106_LFO_ADVANCE);
    CHECK(control_scheduler106_next(&s).type == CONTROL106_NOISE_LEVEL);
    const ControlEvent106 next = control_scheduler106_next(&s);
    CHECK(next.type == CONTROL106_FRAME_START && next.pass_id == 1u);
    return 0;
}

static int test_scheduler_interrupt_classes(void)
{
    ControlScheduler106 s;
    control_scheduler106_init(&s);
    (void)control_scheduler106_next(&s);
    (void)control_scheduler106_next(&s);
    const uint8_t macro_before = s.macro_index;
    const uint32_t pass_before = s.pass_id;
    control_scheduler106_parameter_interrupt(&s);
    CHECK(s.macro_index == macro_before && s.pass_id == pass_before && s.parameter_interrupts == 1u);
    control_scheduler106_adc_interrupt(&s);
    CHECK(s.macro_index == macro_before && s.pass_id == pass_before && s.adc_interrupts == 1u);
    control_scheduler106_voice_event_restart(&s);
    CHECK(s.abandoned_passes == 1u && s.pass_id == pass_before + 1u && s.macro_index == 0u && s.voice_index == 0u);
    const ControlEvent106 restart = control_scheduler106_next(&s);
    CHECK(restart.type == CONTROL106_FRAME_START && restart.pass_id == pass_before + 1u);
    return 0;
}

int main(void)
{
    if (test_poly2_allocator()) return EXIT_FAILURE;
    if (test_poly1_release_queue_and_same_note_reuse()) return EXIT_FAILURE;
    if (test_solo_scan_arbitration()) return EXIT_FAILURE;
    if (test_module_retrigger_matrix()) return EXIT_FAILURE;
    if (test_control_math()) return EXIT_FAILURE;
    if (test_stateful_envelope()) return EXIT_FAILURE;
    if (test_lfo_delay_and_performance_path()) return EXIT_FAILURE;
    if (test_envelope_primitives()) return EXIT_FAILURE;
    if (test_lfo_rate_law()) return EXIT_FAILURE;
    if (test_lfo_phase_sequence()) return EXIT_FAILURE;
    if (test_portamento()) return EXIT_FAILURE;
    if (test_fixed_time()) return EXIT_FAILURE;
    if (test_dac_boundary()) return EXIT_FAILURE;
    if (test_timer_subsample_timeline()) return EXIT_FAILURE;
    if (test_timer_mode3_lengths()) return EXIT_FAILURE;
    if (test_timer_pending_vs_reset()) return EXIT_FAILURE;
    if (test_scheduler_order()) return EXIT_FAILURE;
    if (test_scheduler_interrupt_classes()) return EXIT_FAILURE;
    puts("fm1juno: all scaffold tests passed");
    return EXIT_SUCCESS;
}
