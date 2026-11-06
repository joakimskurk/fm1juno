#ifndef FM1JUNO_EVENT_QUEUE_H
#define FM1JUNO_EVENT_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/fixed_time.h"

#define FM1JUNO_EVENT_QUEUE_CAPACITY 256u

typedef struct Fm1junoQueuedEvent {
    Fm1junoTimeQ32 time;
    uint64_t sequence;
    uint32_t type;
    uint8_t voice;
    uint32_t value;
    uint32_t aux;
} Fm1junoQueuedEvent;

typedef struct Fm1junoEventQueue {
    size_t count;
    uint64_t next_sequence;
    Fm1junoQueuedEvent events[FM1JUNO_EVENT_QUEUE_CAPACITY];
} Fm1junoEventQueue;

void fm1juno_event_queue_init(Fm1junoEventQueue *queue);
bool fm1juno_event_queue_push(Fm1junoEventQueue *queue,
                              Fm1junoTimeQ32 time,
                              uint32_t type,
                              uint8_t voice,
                              uint32_t value,
                              uint32_t aux);
bool fm1juno_event_queue_peek(const Fm1junoEventQueue *queue,
                              Fm1junoQueuedEvent *event);
bool fm1juno_event_queue_pop(Fm1junoEventQueue *queue,
                             Fm1junoQueuedEvent *event);

#endif
