#include "control106/portamento106.h"

uint8_t portamento106_index_from_raw(uint8_t raw_portamento)
{
    return (uint8_t)(raw_portamento >> 1u);
}

uint8_t portamento106_step_from_index(uint8_t index)
{
    /* Exact compact law recovered from the monotone ROM coefficient
     * behavior. This replaces the original 128-entry table without storing
     * or redistributing it wholesale. */
    if (index == 0u) return 0u;

    /* 1..25: 255, 247, ... 63. */
    if (index <= 25u)
        return (uint8_t)(263u - 8u * index);

    /* 26..47: 61, 59, ... 19. */
    if (index <= 47u)
        return (uint8_t)(113u - 2u * index);

    if (index == 48u) return 18u;
    if (index == 49u) return 17u;

    /* 50..61: four indices per step: 16, 15, 14. */
    if (index <= 61u)
        return (uint8_t)(16u - ((index - 50u) >> 2u));

    /* 62..127: five indices per step, clamped at the slowest step=1.
     * The final bucket has six values because the 0..127 domain ends. */
    {
        const uint8_t decrement = (uint8_t)((index - 62u) / 5u);
        return decrement >= 12u ? 1u : (uint8_t)(13u - decrement);
    }
}

uint8_t portamento106_step_from_raw(uint8_t raw_portamento)
{
    if (raw_portamento == 0u) return 0u;
    return portamento106_step_from_index(portamento106_index_from_raw(raw_portamento));
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
