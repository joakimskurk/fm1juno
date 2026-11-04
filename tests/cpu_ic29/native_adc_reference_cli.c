/* Test-only process protocol: read "old_raw incoming_raw" as decimals,
 * calculate the real compiled native_panel106 ADC result, print
 * accepted_raw,conditioned. Used as an independent executable target
 * for the original IC29 bytecode interpreter (Python). */
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
        if (!native_adc106_receive_bank(&state, 0u, incoming_raw))
            return 3;
        if (!native_adc106_process_latest(&state, 0))
            return 4;
        if (printf("%u,%u\n", (unsigned)state.held[0],
                   (unsigned)state.conditioned[0]) < 0)
            return 5;
    }
    return ferror(stdin) ? 6 : 0;
}
