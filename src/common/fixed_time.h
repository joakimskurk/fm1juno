#ifndef FM1JUNO_FIXED_TIME_H
#define FM1JUNO_FIXED_TIME_H

#include <stdint.h>

/* Q32.32 sample time. The upper 32 bits are the integer sample index and
 * the lower 32 bits are the fractional position inside that sample. */
typedef uint64_t Fm1junoTimeQ32;

#define FM1JUNO_TIME_FRAC_BITS 32u
#define FM1JUNO_TIME_ONE_SAMPLE (UINT64_C(1) << FM1JUNO_TIME_FRAC_BITS)

static inline Fm1junoTimeQ32 fm1juno_time_from_samples(uint32_t samples)
{
    return ((Fm1junoTimeQ32)samples) << FM1JUNO_TIME_FRAC_BITS;
}

static inline uint32_t fm1juno_time_sample_index(Fm1junoTimeQ32 t)
{
    return (uint32_t)(t >> FM1JUNO_TIME_FRAC_BITS);
}

static inline uint32_t fm1juno_time_fraction(Fm1junoTimeQ32 t)
{
    return (uint32_t)t;
}

static inline Fm1junoTimeQ32 fm1juno_time_add(Fm1junoTimeQ32 a,
                                               Fm1junoTimeQ32 b)
{
    return a + b;
}

#endif
