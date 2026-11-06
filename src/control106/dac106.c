#include "control106/dac106.h"

uint16_t dac106_code14_to_dac12(uint16_t code14)
{
    return (uint16_t)((code14 & UINT16_C(0x3fff)) >> 2u);
}
