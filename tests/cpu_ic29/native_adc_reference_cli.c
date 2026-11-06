/* Test-only process protocol: read "old_raw incoming_raw" as decimals;
 * return four accepted raw bytes and four conditioned bytes in channel
 * order (accepted0,conditioned0,...,accepted3,conditioned3). The other
 * three channels have deterministic boundary patterns, chosen from
 * incoming_raw parity. This exercises real production pointer layout. */
#include <stdint.h>
#include <stdio.h>

#include "control106/native_panel106.h"

int main(void)
{
    unsigned old_raw, incoming_raw;
    while (scanf("%u %u", &old_raw, &incoming_raw) == 2) {
        if (old_raw > UINT8_MAX || incoming_raw > UINT8_MAX)
            return 2;
        NativeAdc106 state;
        native_adc106_init(&state);
        state.held[0] = (uint8_t)old_raw;
        state.held[1] = 2u;
        state.held[2] = 238u;
        state.held[3] = 255u;

        const uint32_t raw1 = (incoming_raw & 1u) ? 3u : 4u;
        const uint32_t raw2 = (incoming_raw & 1u) ? 240u : 239u;
        const uint32_t raw3 = (incoming_raw & 1u) ? 250u : 249u;
        const uint32_t packed = (uint32_t)incoming_raw |
            (raw1 << 8u) | (raw2 << 16u) | (raw3 << 24u);
        if (!native_adc106_receive_bank(&state, 0u, packed))
            return 3;
        if (!native_adc106_process_latest(&state, 0))
            return 4;
        for (unsigned channel = 0u; channel < 4u; ++channel) {
            if (printf("%s%u,%u", channel == 0u ? "" : ",",
                       (unsigned)state.held[channel],
                       (unsigned)state.conditioned[channel]) < 0)
                return 5;
        }
        if (putchar('\n') == EOF)
            return 5;
    }
    return ferror(stdin) ? 6 : 0;
}
