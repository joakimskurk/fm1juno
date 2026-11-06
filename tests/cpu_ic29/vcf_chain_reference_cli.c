/* Differential CLI for the published VCF voice-control MAME window.
 * Uses production vcf106_voice_code14 with signed global base, ENV,
 * 8.8 Portamento pitch and 7-bit native ENV/Key Follow knobs.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "control106/vcf_control106.h"

int main(void)
{
    int base;
    unsigned env, pitch, depth, inverted, key;
    VcfControlParams106 p = {0};
    while (scanf("%d %u %u %u %u %u",
                 &base, &env, &pitch, &depth, &inverted, &key) == 6) {
        if (base < -32768 || base > 32767 || env > 0x3fffu ||
            pitch > 0xffffu || depth > 127u || inverted > 1u ||
            key > 127u)
            return 2;
        p.env_depth7 = (uint8_t)depth;
        p.env_inverted = inverted != 0u;
        p.key_follow7 = (uint8_t)key;
        if (printf("%u\n", (unsigned)vcf106_voice_code14(
                &p, base, (uint16_t)env, (uint16_t)pitch)) < 0)
            return 3;
    }
    return ferror(stdin) ? 4 : 0;
}
