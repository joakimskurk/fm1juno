/* Independent compiled production oracle for a complete ADC bank with
 * arbitrary four raw and four previously accepted 8-bit values.
 *
 * stdin: bank old0 old1 old2 old3 raw0 raw1 raw2 raw3
 * stdout: held0,conditioned0,held1,conditioned1,...,held3,conditioned3
 * Calls production native_adc106 functions. It does NOT implement their
 * hysteresis or conditioning math here.
 */
#include <stdint.h>
#include <stdio.h>
#include "control106/native_panel106.h"

int main(void)
{
    unsigned bank, old[4], raw[4];
    while (scanf("%u %u %u %u %u %u %u %u %u",
                 &bank, &old[0], &old[1], &old[2], &old[3],
                 &raw[0], &raw[1], &raw[2], &raw[3]) == 9) {
        if (bank > 1u) return 2;
        for (unsigned i = 0; i < 4u; ++i)
            if (old[i] > UINT8_MAX || raw[i] > UINT8_MAX) return 3;
        NativeAdc106 state;
        native_adc106_init(&state);
        uint32_t packed = 0;
        for (unsigned i = 0; i < 4u; ++i) {
            state.held[bank * 4u + i] = (uint8_t)old[i];
            packed |= ((uint32_t)raw[i]) << (i * 8u);
        }
        if (!native_adc106_receive_bank(&state, (uint8_t)bank, packed))
            return 4;
        uint8_t serviced = 255u;
        if (!native_adc106_process_latest(&state, &serviced)
            || serviced != bank)
            return 5;
        for (unsigned i = 0; i < 4u; ++i) {
            const unsigned chan = bank * 4u + i;
            if (printf("%s%u,%u", i ? "," : "",
                       (unsigned)state.held[chan],
                       (unsigned)state.conditioned[chan]) < 0)
                return 6;
        }
        if (putchar('\n') == EOF) return 6;
    }
    return ferror(stdin) ? 7 : 0;
}
