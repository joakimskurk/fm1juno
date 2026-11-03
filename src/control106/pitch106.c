#include "control106/pitch106.h"

uint16_t pitch106_global_offset(int16_t tune,
                                int16_t lfo_pitch,
                                int16_t bend_pitch)
{
    const int32_t value = (int32_t)PITCH106_GLOBAL_BASE
        + (int32_t)tune
        + (int32_t)lfo_pitch
        + (int32_t)bend_pitch;
    return (uint16_t)value;
}

uint16_t pitch106_final(uint16_t voice_pitch, uint16_t global_offset)
{
    return (uint16_t)(voice_pitch + global_offset);
}
