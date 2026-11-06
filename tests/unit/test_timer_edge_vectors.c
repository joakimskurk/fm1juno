#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/fixed_time.h"
#include "timer106/timer8253_106.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return EXIT_FAILURE; \
    } \
} while (0)

/* The expected count-domain durations below come from Intel's Mode-3
 * high/low duty-cycle law, not from the implementation's helpers. */

static int test_independent_mode3_duty_vectors(void)
{
    CHECK(timer8253_106_regular_mode3_count(0u));
    CHECK(timer8253_106_regular_mode3_count(2u));
    CHECK(timer8253_106_regular_mode3_count(4u));
    CHECK(timer8253_106_regular_mode3_count(65535u));
    CHECK(!timer8253_106_regular_mode3_count(1u));
    CHECK(!timer8253_106_regular_mode3_count(3u));

    const struct Vector {
        uint16_t register_count;
        uint32_t effective;
        uint32_t high_clocks;
        uint32_t low_clocks;
    } vectors[] = {
        {0u, 65536u, 32768u, 32768u},
        {2u, 2u, 1u, 1u},
        {4u, 4u, 2u, 2u},
        {5u, 5u, 3u, 2u},
        {8u, 8u, 4u, 4u},
        {9u, 9u, 5u, 4u},
        {10u, 10u, 5u, 5u},
        {11u, 11u, 6u, 5u},
        {65534u, 65534u, 32767u, 32767u},
        {65535u, 65535u, 32768u, 32767u}
    };

    for (size_t i = 0u; i < sizeof vectors / sizeof vectors[0]; ++i) {
        const struct Vector *v = &vectors[i];
        CHECK(timer8253_106_effective_count(v->register_count) ==
              v->effective);
        CHECK(timer8253_106_high_clocks(v->register_count) ==
              v->high_clocks);
        CHECK(timer8253_106_low_clocks(v->register_count) ==
              v->low_clocks);

        const Fm1junoTimeQ32 t0 =
            fm1juno_time_from_samples(100u) + UINT64_C(0x12345678);
        Timer8253Voice106 timer;
        timer8253_106_reset_and_load_at(
            &timer, v->register_count, TIMER106_RANGE_8, t0);

        const Fm1junoTimeQ32 tick =
            timer8253_106_clock_q32(TIMER106_RANGE_8);
        CHECK(timer.output_high);
        CHECK(timer.next_edge_time ==
              t0 + (Fm1junoTimeQ32)v->high_clocks * tick);
        CHECK(timer8253_106_commit_edge(&timer) ==
              t0 + (Fm1junoTimeQ32)v->high_clocks * tick);
        CHECK(!timer.output_high);
        CHECK(timer.next_edge_time ==
              t0 + (Fm1junoTimeQ32)v->effective * tick);
        CHECK(timer8253_106_commit_edge(&timer) ==
              t0 + (Fm1junoTimeQ32)v->effective * tick);
        CHECK(timer.output_high);
        CHECK(timer.next_edge_time ==
              t0 + (Fm1junoTimeQ32)(v->effective + v->high_clocks) * tick);
    }
    return EXIT_SUCCESS;
}

static int test_queued_write_before_high_boundary(void)
{
    const Fm1junoTimeQ32 t0 = fm1juno_time_from_samples(10u) + 777u;
    const Fm1junoTimeQ32 tick =
        timer8253_106_clock_q32(TIMER106_RANGE_8);
    Timer8253Voice106 timer;
    timer8253_106_reset_and_load_at(&timer, 9u, TIMER106_RANGE_8, t0);

    const Fm1junoTimeQ32 initial_fall = t0 + 5u * tick;
    CHECK(timer.next_edge_time == initial_fall);

    /* Writes during the current high half cannot shorten the high pulse. */
    timer8253_106_request_count(&timer, 8u);
    CHECK(timer.current_count == 9u && timer.pending_count == 8u);
    CHECK(timer.next_edge_time == initial_fall);

    CHECK(timer8253_106_commit_edge(&timer) == initial_fall);
    CHECK(!timer.output_high && !timer.pending_valid);
    CHECK(timer.current_count == 8u);
    CHECK(timer.clocks_remaining == 4u);
    CHECK(timer.next_edge_time == initial_fall + 4u * tick);

    CHECK(timer8253_106_commit_edge(&timer) ==
          initial_fall + 4u * tick);
    CHECK(timer.output_high && timer.clocks_remaining == 4u);
    return EXIT_SUCCESS;
}

static int test_two_pending_writes_mid_low(void)
{
    const Fm1junoTimeQ32 t0 =
        fm1juno_time_from_samples(32u) + UINT64_C(0x43210);
    const Fm1junoTimeQ32 tick =
        timer8253_106_clock_q32(TIMER106_RANGE_16);
    Timer8253Voice106 timer;
    timer8253_106_reset_and_load_at(&timer, 10u, TIMER106_RANGE_16, t0);

    CHECK(timer8253_106_commit_edge(&timer) == t0 + 5u * tick);
    CHECK(!timer.output_high);
    CHECK(timer.next_edge_time == t0 + 10u * tick);

    timer8253_106_request_count(&timer, 8u);
    timer8253_106_request_count(&timer, 11u); /* last complete LSB/MSB wins */
    CHECK(timer.pending_valid && timer.pending_count == 11u);
    CHECK(timer.current_count == 10u);
    CHECK(timer.next_edge_time == t0 + 10u * tick);

    CHECK(timer8253_106_commit_edge(&timer) == t0 + 10u * tick);
    CHECK(timer.output_high);
    CHECK(timer.current_count == 11u);
    CHECK(!timer.pending_valid && timer.clocks_remaining == 6u);
    CHECK(timer.next_edge_time == t0 + 16u * tick);
    CHECK(timer8253_106_commit_edge(&timer) == t0 + 16u * tick);
    CHECK(!timer.output_high && timer.clocks_remaining == 5u);
    CHECK(timer.next_edge_time == t0 + 21u * tick);
    return EXIT_SUCCESS;
}

static int test_reset_discards_pending_with_subsample_time(void)
{
    Timer8253Voice106 timer;
    const Fm1junoTimeQ32 start =
        fm1juno_time_from_samples(200u) + 321u;
    timer8253_106_reset_and_load_at(&timer, 9u,
                                    TIMER106_RANGE_16, start);
    timer8253_106_request_count(&timer, 0u);
    const Fm1junoTimeQ32 reset_at =
        start + timer8253_106_clock_q32(TIMER106_RANGE_16) * 2u + 1u;
    timer8253_106_reset_and_load_at(&timer, 13u,
                                    TIMER106_RANGE_4, reset_at);
    CHECK(timer.output_high && timer.range == TIMER106_RANGE_4);
    CHECK(timer.current_count == 13u && !timer.pending_valid);
    CHECK(timer.clocks_remaining == 7u);
    CHECK(timer.next_edge_time ==
          reset_at + 7u * timer8253_106_clock_q32(TIMER106_RANGE_4));
    const Fm1junoTimeQ32 first = timer8253_106_commit_edge(&timer);
    CHECK(!timer.output_high && timer.clocks_remaining == 6u);
    CHECK(timer.next_edge_time ==
          first + 6u * timer8253_106_clock_q32(TIMER106_RANGE_4));
    return EXIT_SUCCESS;
}

static int test_prescale_timings_across_resets(void)
{
    const TimerRange106 ranges[] = {
        TIMER106_RANGE_16, TIMER106_RANGE_8, TIMER106_RANGE_4
    };
    const uint32_t clocks[] = {
        189408058u, 94704029u, 47352014u
    };
    const uint32_t freq[] = {
        1000000u, 2000000u, 4000000u
    };
    for (size_t i = 0u; i < 3u; ++i) {
        CHECK(timer8253_106_input_clock_hz(ranges[i]) == freq[i]);
        CHECK(timer8253_106_clock_q32(ranges[i]) == clocks[i]);
        const Fm1junoTimeQ32 at =
            fm1juno_time_from_samples(1000u) + 42u;
        Timer8253Voice106 timer;
        timer8253_106_reset_and_load_at(&timer, 12u, ranges[i], at);
        const Fm1junoTimeQ32 half = 6u * clocks[i];
        CHECK(timer8253_106_commit_edge(&timer) == at + half);
        CHECK(timer8253_106_commit_edge(&timer) == at + 2u * half);
        CHECK(timer8253_106_commit_edge(&timer) == at + 3u * half);
    }
    return EXIT_SUCCESS;
}

static int test_pending_zero_count_at_boundary(void)
{
    const Fm1junoTimeQ32 start = fm1juno_time_from_samples(400u);
    const Fm1junoTimeQ32 tick =
        timer8253_106_clock_q32(TIMER106_RANGE_8);
    Timer8253Voice106 timer;
    timer8253_106_reset_and_load_at(&timer, 10u,
                                    TIMER106_RANGE_8, start);
    timer8253_106_request_count(&timer, 0u);
    CHECK(timer8253_106_commit_edge(&timer) == start + 5u * tick);
    CHECK(timer.current_count == 0u);
    CHECK(!timer.output_high);
    CHECK(timer.clocks_remaining == 32768u);
    CHECK(timer.next_edge_time == start + (5u + 32768u) * tick);
    CHECK(timer8253_106_commit_edge(&timer) ==
          start + (5u + 32768u) * tick);
    CHECK(timer.output_high && timer.clocks_remaining == 32768u);
    return EXIT_SUCCESS;
}

int main(void)
{
    if (test_independent_mode3_duty_vectors()) return EXIT_FAILURE;
    if (test_queued_write_before_high_boundary()) return EXIT_FAILURE;
    if (test_two_pending_writes_mid_low()) return EXIT_FAILURE;
    if (test_reset_discards_pending_with_subsample_time()) return EXIT_FAILURE;
    if (test_prescale_timings_across_resets()) return EXIT_FAILURE;
    if (test_pending_zero_count_at_boundary()) return EXIT_FAILURE;
    puts("fm1juno: 8253 Mode-3 independent edge vectors passed");
    return EXIT_SUCCESS;
}
