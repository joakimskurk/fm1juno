#include <stdio.h>
#include <stdlib.h>

#include "common/fixed_time.h"
#include "control106/event_replay106.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return EXIT_FAILURE; \
    } \
} while (0)

static int test_callback_size_invariance(void)
{
    ScriptEvent106 script[64];
    size_t n = 0u;

#define APPEND(sample, kind, voice_value, value_value, aux_value) do { \
    script[n].time = fm1juno_time_from_samples((sample)); \
    script[n].type = (kind); \
    script[n].voice = (voice_value); \
    script[n].value = (value_value); \
    script[n].aux = (aux_value); \
    ++n; \
} while (0)

    APPEND(1u, SCRIPT106_DCO_COMMAND, 0u, 100u, TIMER106_RANGE_8);
    APPEND(2u, SCRIPT106_VOICE_ON, 0u, 60u, 0u);
    for (uint32_t s = 3u; s < 28u; ++s)
        APPEND(s, SCRIPT106_CONTROL_STEP, 0u, 0u, 0u);
    APPEND(32u, SCRIPT106_DCO_COMMAND, 0u, 90u, TIMER106_RANGE_8);
    APPEND(32u, SCRIPT106_VOICE_ON, 0u, 60u, 0u);
    for (uint32_t s = 33u; s < 50u; ++s)
        APPEND(s, SCRIPT106_CONTROL_STEP, 0u, 0u, 0u);
    APPEND(64u, SCRIPT106_SUSTAIN, 0u, 1u, 0u);
    APPEND(65u, SCRIPT106_VOICE_OFF, 0u, 60u, 0u);
    APPEND(80u, SCRIPT106_CONTROL_STEP, 0u, 0u, 0u);
    APPEND(96u, SCRIPT106_SUSTAIN, 0u, 0u, 0u);
    APPEND(128u, SCRIPT106_CONTROL_STEP, 0u, 0u, 0u);

#undef APPEND

    CHECK(event_replay106_validate(script, n));

    const uint32_t blocks[] = {16u, 32u, 64u, 128u, 256u};
    ReplayResult106 baseline = {0};
    CHECK(event_replay106_run(script, n, 256u, blocks[0], &baseline));
    CHECK(!baseline.trace_overflowed);

    for (size_t i = 1u; i < sizeof blocks / sizeof blocks[0]; ++i) {
        ReplayResult106 result = {0};
        CHECK(event_replay106_run(script, n, 256u, blocks[i], &result));
        CHECK(result.trace_signature == baseline.trace_signature);
        CHECK(result.trace_count == baseline.trace_count);
        CHECK(result.note_gate_bits == baseline.note_gate_bits);
        CHECK(result.voice_run_bits == baseline.voice_run_bits);
        CHECK(result.pass_id == baseline.pass_id);
        CHECK(result.abandoned_passes == baseline.abandoned_passes);
    }

    ScriptEvent106 bad[2] = {
        {fm1juno_time_from_samples(10u), SCRIPT106_CONTROL_STEP, 0u, 0u, 0u},
        {fm1juno_time_from_samples(9u), SCRIPT106_CONTROL_STEP, 0u, 0u, 0u}
    };
    CHECK(!event_replay106_validate(bad, 2u));
    return EXIT_SUCCESS;
}

int main(void)
{
    if (test_callback_size_invariance()) return EXIT_FAILURE;
    puts("fm1juno: callback-size invariance tests passed");
    return EXIT_SUCCESS;
}
