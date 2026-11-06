#include <stdio.h>
#include <stdlib.h>

#include "common/event_queue.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return EXIT_FAILURE; \
    } \
} while (0)

static int test_time_and_stable_order(void)
{
    Fm1junoEventQueue q;
    fm1juno_event_queue_init(&q);

    CHECK(fm1juno_event_queue_push(&q, fm1juno_time_from_samples(20u), 20u, 2u, 200u, 0u));
    CHECK(fm1juno_event_queue_push(&q, fm1juno_time_from_samples(10u), 10u, 1u, 100u, 0u));
    CHECK(fm1juno_event_queue_push(&q, fm1juno_time_from_samples(10u), 11u, 3u, 101u, 0u));
    CHECK(q.count == 3u);

    Fm1junoQueuedEvent e;
    CHECK(fm1juno_event_queue_pop(&q, &e));
    CHECK(e.type == 10u && e.voice == 1u && e.value == 100u && e.sequence == 1u);
    CHECK(fm1juno_event_queue_pop(&q, &e));
    CHECK(e.type == 11u && e.voice == 3u && e.value == 101u && e.sequence == 2u);
    CHECK(fm1juno_event_queue_pop(&q, &e));
    CHECK(e.type == 20u && e.voice == 2u && e.value == 200u && e.sequence == 0u);
    CHECK(!fm1juno_event_queue_pop(&q, &e));
    return EXIT_SUCCESS;
}

static int test_fractional_time_order(void)
{
    Fm1junoEventQueue q;
    fm1juno_event_queue_init(&q);

    const Fm1junoTimeQ32 s = fm1juno_time_from_samples(7u);
    CHECK(fm1juno_event_queue_push(&q, s + UINT64_C(0x80000000), 2u, 0u, 0u, 0u));
    CHECK(fm1juno_event_queue_push(&q, s + UINT64_C(0x40000000), 1u, 0u, 0u, 0u));

    Fm1junoQueuedEvent e;
    CHECK(fm1juno_event_queue_peek(&q, &e));
    CHECK(e.type == 1u);
    CHECK(fm1juno_event_queue_pop(&q, &e));
    CHECK(e.type == 1u);
    CHECK(fm1juno_event_queue_pop(&q, &e));
    CHECK(e.type == 2u);
    return EXIT_SUCCESS;
}

static int test_capacity(void)
{
    Fm1junoEventQueue q;
    fm1juno_event_queue_init(&q);
    for (uint32_t i = 0u; i < FM1JUNO_EVENT_QUEUE_CAPACITY; ++i)
        CHECK(fm1juno_event_queue_push(&q, fm1juno_time_from_samples(i), i, 0u, i, 0u));
    CHECK(!fm1juno_event_queue_push(&q, 0u, 0u, 0u, 0u, 0u));
    return EXIT_SUCCESS;
}

int main(void)
{
    if (test_time_and_stable_order()) return EXIT_FAILURE;
    if (test_fractional_time_order()) return EXIT_FAILURE;
    if (test_capacity()) return EXIT_FAILURE;
    puts("fm1juno: event queue tests passed");
    return EXIT_SUCCESS;
}
