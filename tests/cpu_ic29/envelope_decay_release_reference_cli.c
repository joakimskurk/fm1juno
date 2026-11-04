/* Compiled production reference for the actual IC29 DECAY/RELEASE opcode paths.
 * It does not replicate the published multiplier instructions. The MAME
 * harness independently executes those original bytes and compares results.
 *
 * Input: mode(0=DECAY,1=RELEASE) coeff_index7 current14 sustain14
 * Output: original-domain coefficient16,resulting envelope14
 */
#include <stdint.h>
#include <stdio.h>

#include "control106/envelope106.h"

int main(void)
{
    unsigned mode, index, current, sustain;
    while (scanf("%u %u %u %u", &mode, &index, &current, &sustain) == 4) {
        if (mode > 1u || index > 127u || current > ENVELOPE106_MAX ||
            sustain > ENVELOPE106_MAX)
            return 2;
        const uint16_t coefficient =
            envelope106_decay_release_coefficient((uint8_t)index);
        const uint16_t level = mode != 0u
            ? envelope106_release_tick((uint16_t)current, coefficient)
            : envelope106_decay_tick((uint16_t)current, (uint16_t)sustain,
                                      coefficient);
        if (printf("%u,%u\n", (unsigned)coefficient, (unsigned)level) < 0)
            return 3;
    }
    return ferror(stdin) ? 4 : 0;
}
