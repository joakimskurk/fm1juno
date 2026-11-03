#include "control106/portamento106.h"

uint8_t portamento106_index_from_raw(uint8_t raw_portamento)
{
    return (uint8_t)(raw_portamento >> 1u);
}

uint16_t portamento106_step_toward(uint16_t current,
                                   uint16_t target,
                                   uint8_t step)
{
    /* A zero step is the OFF path: no glide, snap to the target. */
    if (step == 0u || current == target) return target;

    if (current < target) {
        const uint32_t next = (uint32_t)current + step;
        return next >= target ? target : (uint16_t)next;
    }

    if ((uint16_t)(current - target) <= step) return target;
    return (uint16_t)(current - step);
}
