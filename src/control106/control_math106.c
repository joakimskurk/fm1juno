#include "control106/control_math106.h"

uint16_t control106_mul16x8_hi(uint16_t value, uint8_t multiplier)
{
    const uint32_t product = (uint32_t)value * (uint32_t)multiplier;
    return (uint16_t)(product >> 8u);
}

uint16_t control106_dco_bend_8_8(uint8_t magnitude, uint8_t sensitivity)
{
    const uint16_t p = (uint16_t)((uint16_t)magnitude * (uint16_t)sensitivity);
    const uint16_t q = (uint16_t)((p >> 1u) + (p >> 2u) + (p >> 8u));
    return (uint16_t)(q >> 4u);
}

uint16_t control106_vcf_bend_12(uint8_t magnitude, uint8_t sensitivity)
{
    const uint16_t p = (uint16_t)((uint16_t)magnitude * (uint16_t)sensitivity);
    return (uint16_t)(p >> 4u);
}

int16_t control106_tune_8_8(uint8_t raw_tune)
{
    int16_t value = (int16_t)raw_tune - 128;
    if (value < -127) value = -127;
    if (value > 127) value = 127;
    return value;
}
