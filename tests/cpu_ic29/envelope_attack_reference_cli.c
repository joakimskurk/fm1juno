/* Compiled production envelope reference for original IC29 attack MAME probes.
 * No reproduced original Roland bytecode or coefficient table is embedded.
 * Input: attack_index7 envelope_before14
 * Output: production_increment, envelope_after, hit_max
 */
#include <stdint.h>
#include <stdio.h>

#include "control106/envelope106.h"

int main(void)
{
    unsigned index;
    unsigned before;
    while (scanf("%u %u", &index, &before) == 2) {
        if (index > 127u || before > ENVELOPE106_MAX)
            return 2;
        const uint16_t increment = envelope106_attack_increment((uint8_t)index);
        const uint16_t after =
            envelope106_attack_tick((uint16_t)before, increment);
        const unsigned saturated = (after == ENVELOPE106_MAX) ? 1u : 0u;
        if (printf("%u,%u,%u\n", (unsigned)increment, (unsigned)after,
                   saturated) < 0)
            return 3;
    }
    return ferror(stdin) ? 4 : 0;
}
