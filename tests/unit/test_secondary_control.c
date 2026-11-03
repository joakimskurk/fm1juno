#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "common/fixed_time.h"
#include "control106/module_control106.h"
#include "control106/secondary_control106.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return EXIT_FAILURE; \
    } \
} while (0)

static ControlEvent106 until(ModuleControl106 *m,
                             ControlEventType106 target,
                             uint8_t voice,
                             Fm1junoTimeQ32 *time)
{
    for (unsigned guard = 0u; guard < 80u; ++guard) {
        const ControlEvent106 event = module_control106_service_next(m, *time);
        *time += FM1JUNO_TIME_ONE_SAMPLE;
        if (event.type == target &&
            (voice == CONTROL106_NO_VOICE || event.voice == voice))
            return event;
    }
    ControlEvent106 failure = {CONTROL106_NOISE_LEVEL,
                               CONTROL106_NO_VOICE, 0u, 0u};
    return failure;
}

static int test_mux_and_level_laws(void)
{
    CHECK(secondary106_patch_code14(0u) == 0u);
    CHECK(secondary106_patch_code14(1u) == 0x80u);
    CHECK(secondary106_patch_code14(64u) == 0x2000u);
    CHECK(secondary106_patch_code14(127u) == 0x3f80u);

    const SecondaryMuxRoute106 res = secondary106_route(
        SECONDARY106_RESONANCE);
    const SecondaryMuxRoute106 sub = secondary106_route(
        SECONDARY106_SUB_LEVEL);
    const SecondaryMuxRoute106 master = secondary106_route(
        SECONDARY106_GLOBAL_VCA);
    const SecondaryMuxRoute106 pwm = secondary106_route(
        SECONDARY106_PWM);
    const SecondaryMuxRoute106 noise = secondary106_route(
        SECONDARY106_NOISE);

    CHECK(res.bank == SECONDARY106_MUX_IC26_VCA && res.channel == 6u);
    CHECK(sub.bank == SECONDARY106_MUX_IC24_RAMP && sub.channel == 6u);
    CHECK(master.bank == SECONDARY106_MUX_IC23_VCF && master.channel == 6u);
    CHECK(pwm.bank == SECONDARY106_MUX_IC23_VCF && pwm.channel == 7u);
    CHECK(noise.bank == SECONDARY106_MUX_IC26_VCA && noise.channel == 7u);

    for (uint8_t voice = 0u; voice < 6u; ++voice) {
        const SecondaryMuxRoute106 vca =
            secondary106_voice_vca_route(voice);
        CHECK(vca.bank == SECONDARY106_MUX_IC26_VCA);
        CHECK(vca.channel == voice);
    }
    CHECK(secondary106_voice_vca_route(6u).channel == 0xffu);

    SecondaryHold106 hold = {0};
    const Fm1junoTimeQ32 time = fm1juno_time_from_samples(42u) + 1234u;
    secondary106_commit(&hold, 0x3f83u, time, 9u);
    CHECK(hold.valid);
    CHECK(hold.code14 == 0x3f83u);
    CHECK(hold.dac12 == 0xfe0u);
    CHECK(hold.committed_at == time && hold.pass_id == 9u);
    return EXIT_SUCCESS;
}

static int test_pwm_integer_law(void)
{
    CHECK(secondary106_pwm_code14(127u, false, false, 0u, false) == 0u);
    CHECK(secondary106_pwm_code14(0u, true, true, 8191u, true) == 0x3fffu);
    CHECK(secondary106_pwm_code14(127u, true, true, 0u, false) == 128u);
    CHECK(secondary106_pwm_code14(127u, false, true, 0u, false) == 8255u);
    CHECK(secondary106_pwm_code14(127u, false, true, 4096u, false) == 12319u);
    CHECK(secondary106_pwm_code14(127u, false, true, 8191u, false) == 16383u);
    CHECK(secondary106_pwm_code14(127u, false, true, 8191u, true) == 128u);
    CHECK(secondary106_pwm_code14(127u, false, true, 65535u, true) == 128u);
    CHECK(secondary106_pwm_code14(255u, true, true, 0u, false) == 128u);
    return EXIT_SUCCESS;
}

static int test_gate_voice_vca_law(void)
{
    CHECK(secondary106_voice_vca_code14(0x2345u, false, false) == 0x2345u);
    CHECK(secondary106_voice_vca_code14(0x2345u, false, true) == 0x2345u);
    CHECK(secondary106_voice_vca_code14(0x2345u, true, false) == 0u);
    CHECK(secondary106_voice_vca_code14(0x2345u, true, true) == 0x3fffu);
    CHECK(secondary106_voice_vca_code14(0xffffu, false, true) == 0x3fffu);
    return EXIT_SUCCESS;
}

static int test_secondary_slots_and_pwm_one_pass_delay(void)
{
    ModuleControl106 m;
    module_control106_init(&m);
    CHECK(module_control106_receive_parameter(&m, RAW106_VCF_RESONANCE7,
                                             80u, 0u));
    CHECK(module_control106_receive_parameter(&m, RAW106_GLOBAL_VCA_LEVEL7,
                                             64u, 0u));
    CHECK(module_control106_receive_parameter(&m, RAW106_SUB_LEVEL7,
                                             90u, 0u));
    CHECK(module_control106_receive_parameter(&m, RAW106_NOISE_LEVEL7,
                                             100u, 0u));
    CHECK(module_control106_receive_parameter(&m, RAW106_PWM_DEPTH7,
                                             127u, 0u));
    CHECK(module_control106_receive_parameter(&m, RAW106_PWM_MANUAL,
                                             0u, 0u));
    CHECK(module_control106_receive_parameter(&m, RAW106_PULSE_ENABLED,
                                             1u, 0u));
    CHECK(module_control106_receive_parameter(&m, RAW106_LFO_RATE7,
                                             127u, 0u));
    CHECK(!module_control106_receive_parameter(&m, RAW106_PWM_DEPTH7,
                                              128u, 0u));
    CHECK(!module_control106_receive_parameter(&m, RAW106_PWM_MANUAL,
                                              2u, 0u));

    const AssignerEvent106 on0 = {
        ASSIGNER106_EVENT_VOICE_ON, 0u, 60u
    };
    const AssignerEvent106 on5 = {
        ASSIGNER106_EVENT_VOICE_ON, 5u, 72u
    };
    module_control106_apply_assigner_event(&m, on0, 0u);
    module_control106_apply_assigner_event(&m, on5, 0u);
    Fm1junoTimeQ32 now = FM1JUNO_TIME_ONE_SAMPLE;

    CHECK(until(&m, CONTROL106_VCF_RESONANCE, CONTROL106_NO_VOICE,
                &now).type == CONTROL106_VCF_RESONANCE);
    SecondaryHold106 *res = &m.secondary_outputs.global[SECONDARY106_RESONANCE];
    CHECK(res->valid && res->code14 == 80u * 128u);
    const Fm1junoTimeQ32 res_time = res->committed_at;

    /* Value is received immediately, but the resonance output is already
     * held and must not change until the following resonance slot. */
    CHECK(module_control106_receive_parameter(
        &m, RAW106_VCF_RESONANCE7, 1u, now));
    CHECK(res->code14 == 80u * 128u);

    (void)until(&m, CONTROL106_GLOBAL_VCA_LEVEL, CONTROL106_NO_VOICE, &now);
    CHECK(m.secondary_outputs.global[SECONDARY106_GLOBAL_VCA].code14 ==
          64u * 128u);
    (void)until(&m, CONTROL106_SUB_LEVEL, CONTROL106_NO_VOICE, &now);
    CHECK(m.secondary_outputs.global[SECONDARY106_SUB_LEVEL].code14 ==
          90u * 128u);

    /* The very first PWM S/H code is the pre-LFO accumulator value 0;
     * it does not jump to an instantaneous PWM based on incoming parameters. */
    (void)until(&m, CONTROL106_ENV_VCA_VCF_VOICE, 0u, &now);
    SecondaryHold106 *pwm = &m.secondary_outputs.global[SECONDARY106_PWM];
    CHECK(pwm->valid && pwm->code14 == 0u);
    CHECK(!m.secondary_outputs.voice_vca[0].valid);

    (void)until(&m, CONTROL106_ENV_VCA_VCF_VOICE, 1u, &now);
    CHECK(m.secondary_outputs.voice_vca[0].valid);
    CHECK(m.secondary_outputs.voice_vca[0].code14 ==
          m.voices[0].envelope.level);

    (void)until(&m, CONTROL106_ENV_VCA_VCF_VOICE, 5u, &now);
    CHECK(m.secondary_outputs.voice_vca[4].valid);
    CHECK(!m.secondary_outputs.voice_vca[5].valid);

    (void)until(&m, CONTROL106_ADC_PERFORMANCE, CONTROL106_NO_VOICE, &now);
    CHECK(!m.secondary_outputs.voice_vca[5].valid);
    (void)until(&m, CONTROL106_FINAL_VCA_VOICE6,
                CONTROL106_NO_VOICE, &now);
    CHECK(m.secondary_outputs.voice_vca[5].valid);
    CHECK(m.secondary_outputs.voice_vca[5].code14 ==
          m.voices[5].envelope.level);

    (void)until(&m, CONTROL106_LFO_ADVANCE, CONTROL106_NO_VOICE, &now);
    CHECK(m.lfo.magnitude == 4096u);
    CHECK(m.secondary_outputs.pwm_next_code14 == 12319u);
    CHECK(pwm->code14 == 0u);

    (void)until(&m, CONTROL106_NOISE_LEVEL, CONTROL106_NO_VOICE, &now);
    CHECK(m.secondary_outputs.global[SECONDARY106_NOISE].code14 ==
          100u * 128u);

    (void)until(&m, CONTROL106_VCF_RESONANCE, CONTROL106_NO_VOICE, &now);
    CHECK(res->committed_at > res_time);
    CHECK(res->code14 == 128u);
    /* Manual switch after PWM computation cannot rewrite the existing
     * precomputed latch value until the next LFO_ADVANCE. */
    CHECK(module_control106_receive_parameter(&m, RAW106_PWM_MANUAL,
                                             1u, now));
    (void)until(&m, CONTROL106_ENV_VCA_VCF_VOICE, 0u, &now);
    CHECK(pwm->code14 == 12319u);
    CHECK(m.secondary_outputs.pwm_next_code14 == 12319u);
    CHECK(!m.trace.overflowed);
    return EXIT_SUCCESS;
}

static int test_gate_mode_and_aborted_pass(void)
{
    ModuleControl106 m;
    module_control106_init(&m);
    CHECK(module_control106_receive_parameter(&m, RAW106_VCA_GATE_MODE,
                                             1u, 0u));
    const AssignerEvent106 on = {
        ASSIGNER106_EVENT_VOICE_ON, 0u, 60u
    };
    module_control106_apply_assigner_event(&m, on, 0u);
    Fm1junoTimeQ32 now = FM1JUNO_TIME_ONE_SAMPLE;
    (void)until(&m, CONTROL106_ENV_VCA_VCF_VOICE, 1u, &now);
    CHECK(m.secondary_outputs.voice_vca[0].code14 == 0x3fffu);
    CHECK(m.secondary_outputs.voice_vca[1].valid == false);

    /* Abort after first voice VCA hold: untouched voices retain their
     * previous held values; a frame restart does not magically complete. */
    const uint32_t abandoned = m.scheduler.abandoned_passes;
    const AssignerEvent106 off = {
        ASSIGNER106_EVENT_VOICE_OFF, 0u, 60u
    };
    module_control106_apply_assigner_event(&m, off, now);
    CHECK(m.scheduler.abandoned_passes == abandoned + 1u);
    CHECK(module_control106_service_next(&m, now).type ==
          CONTROL106_FRAME_START);
    CHECK(!m.secondary_outputs.voice_vca[1].valid);
    CHECK(m.secondary_outputs.voice_vca[0].code14 == 0x3fffu);

    (void)until(&m, CONTROL106_ENV_VCA_VCF_VOICE, 1u, &now);
    CHECK(m.secondary_outputs.voice_vca[0].code14 == 0u);
    return EXIT_SUCCESS;
}

int main(void)
{
    if (test_mux_and_level_laws()) return EXIT_FAILURE;
    if (test_pwm_integer_law()) return EXIT_FAILURE;
    if (test_gate_voice_vca_law()) return EXIT_FAILURE;
    if (test_secondary_slots_and_pwm_one_pass_delay()) return EXIT_FAILURE;
    if (test_gate_mode_and_aborted_pass()) return EXIT_FAILURE;
    puts("fm1juno: secondary digital CV/scheduler tests passed");
    return EXIT_SUCCESS;
}
