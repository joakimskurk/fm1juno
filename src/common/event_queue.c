#include "common/event_queue.h"

static bool event_before(const Fm1junoQueuedEvent *a,
                         const Fm1junoQueuedEvent *b)
{
    if (a->time != b->time) return a->time < b->time;
    return a->sequence < b->sequence;
}

void fm1juno_event_queue_init(Fm1junoEventQueue *queue)
{
    queue->count = 0u;
    queue->next_sequence = 0u;
}

bool fm1juno_event_queue_push(Fm1junoEventQueue *queue,
                              Fm1junoTimeQ32 time,
                              uint32_t type,
                              uint8_t voice,
                              uint32_t value,
                              uint32_t aux)
{
    if (queue == NULL || queue->count >= FM1JUNO_EVENT_QUEUE_CAPACITY)
        return false;

    Fm1junoQueuedEvent event = {
        time,
        queue->next_sequence++,
        type,
        voice,
        value,
        aux
    };

    size_t pos = queue->count;
    while (pos > 0u && event_before(&event, &queue->events[pos - 1u])) {
        queue->events[pos] = queue->events[pos - 1u];
        --pos;
    }
    queue->events[pos] = event;
    ++queue->count;
    return true;
}

bool fm1juno_event_queue_peek(const Fm1junoEventQueue *queue,
                              Fm1junoQueuedEvent *event)
{
    if (queue == NULL || event == NULL || queue->count == 0u)
        return false;
    *event = queue->events[0];
    return true;
}

bool fm1juno_event_queue_pop(Fm1junoEventQueue *queue,
                             Fm1junoQueuedEvent *event)
{
    if (!fm1juno_event_queue_peek(queue, event))
        return false;

    for (size_t i = 1u; i < queue->count; ++i)
        queue->events[i - 1u] = queue->events[i];
    --queue->count;
    return true;
}
