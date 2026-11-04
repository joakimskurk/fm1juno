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
        /* This is the *phase transition* bit, not merely level==MAX.
         * Original IC29 attack sets decay/release flags only on strict
         * 0x3FFF overflow; exact equality stays in ATTACK this tick. */
        const unsigned saturated = (
            (uint32_t)before + (uint32_t)increment > ENVELOPE106_MAX)
            ? 1u : 0u;
        if (printf("%u,%u,%u\n", (unsigned)increment, (unsigned)after,
                   saturated) < 0)
            return 3;
    }
    return ferror(stdin) ? 4 : 0;
}
