#include <stdio.h>
#include <stdlib.h>

#include "common/fixed_time.h"
#include "control106/control_scheduler106.h"
#include "control106/dac106.h"
#include "timer106/timer8253_106.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static int test_fixed_time(void)
{
    const Fm1junoTimeQ32 t = fm1juno_time_from_samples(42u);
    CHECK(fm1juno_time_sample_index(t) == 42u);
    CHECK(fm1juno_time_fraction(t) == 0u);
    CHECK(fm1juno_time_add(t, FM1JUNO_TIME_ONE_SAMPLE) ==
          fm1juno_time_from_samples(43u));
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
    CHECK(t.output_high);
    CHECK(t.current_count == 10u);
    CHECK(t.clocks_remaining == 5u);

    timer8253_106_request_count(&t, 8u);
    CHECK(t.current_count == 10u);
    CHECK(t.pending_valid);
    CHECK(t.clocks_remaining == 5u);

    timer8253_106_advance_half_cycle(&t);
    CHECK(!t.output_high);
    CHECK(t.current_count == 8u);
    CHECK(!t.pending_valid);
    CHECK(t.clocks_remaining == 4u);

    timer8253_106_reset_and_load(&t, 9u);
    CHECK(t.output_high);
    CHECK(t.current_count == 9u);
    CHECK(t.clocks_remaining == 5u);
    return 0;
}

static int test_scheduler_order(void)
{
    ControlScheduler106 s;
    control_scheduler106_init(&s);

    const ControlEventType106 global_prefix[] = {
        CONTROL106_FRAME_START,
        CONTROL106_LATCH_AND_VOICERUN,
        CONTROL106_VCF_RESONANCE,
        CONTROL106_LFO_DELAY_AND_MOD_SCALE,
        CONTROL106_GLOBAL_VCA_LEVEL,
        CONTROL106_PITCH_AND_PORTAMENTO,
        CONTROL106_SUB_LEVEL
    };

    for (size_t i = 0; i < sizeof global_prefix / sizeof global_prefix[0]; ++i) {
        const ControlEvent106 e = control_scheduler106_next(&s);
        CHECK(e.type == global_prefix[i]);
        CHECK(e.voice == CONTROL106_NO_VOICE);
        CHECK(e.pass_id == 0u);
    }

    for (uint8_t v = 0; v < 6u; ++v) {
        const ControlEvent106 e = control_scheduler106_next(&s);
        CHECK(e.type == CONTROL106_DCO_VOICE);
        CHECK(e.voice == v);
    }

    CHECK(control_scheduler106_next(&s).type == CONTROL106_VCF_GLOBAL_BASE);

    for (uint8_t v = 0; v < 6u; ++v) {
        const ControlEvent106 e = control_scheduler106_next(&s);
        CHECK(e.type == CONTROL106_ENV_VCA_VCF_VOICE);
        CHECK(e.voice == v);
    }

    CHECK(control_scheduler106_next(&s).type == CONTROL106_ADC_PERFORMANCE);
    CHECK(control_scheduler106_next(&s).type == CONTROL106_LFO_ADVANCE);
    CHECK(control_scheduler106_next(&s).type == CONTROL106_NOISE_LEVEL);

    const ControlEvent106 next = control_scheduler106_next(&s);
    CHECK(next.type == CONTROL106_FRAME_START);
    CHECK(next.pass_id == 1u);
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
    CHECK(s.macro_index == macro_before);
    CHECK(s.pass_id == pass_before);
    CHECK(s.parameter_interrupts == 1u);

    control_scheduler106_adc_interrupt(&s);
    CHECK(s.macro_index == macro_before);
    CHECK(s.pass_id == pass_before);
    CHECK(s.adc_interrupts == 1u);

    control_scheduler106_voice_event_restart(&s);
    CHECK(s.abandoned_passes == 1u);
    CHECK(s.pass_id == pass_before + 1u);
    CHECK(s.macro_index == 0u);
    CHECK(s.voice_index == 0u);

    const ControlEvent106 restart = control_scheduler106_next(&s);
    CHECK(restart.type == CONTROL106_FRAME_START);
    CHECK(restart.pass_id == pass_before + 1u);
    return 0;
}

int main(void)
{
    if (test_fixed_time()) return EXIT_FAILURE;
    if (test_dac_boundary()) return EXIT_FAILURE;
    if (test_timer_mode3_lengths()) return EXIT_FAILURE;
    if (test_timer_pending_vs_reset()) return EXIT_FAILURE;
    if (test_scheduler_order()) return EXIT_FAILURE;
    if (test_scheduler_interrupt_classes()) return EXIT_FAILURE;

    puts("fm1juno: all scaffold tests passed");
    return EXIT_SUCCESS;
}
