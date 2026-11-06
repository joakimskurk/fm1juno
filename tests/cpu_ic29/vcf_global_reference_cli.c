/* Compiled production-global-VCF decomposition for the MAME opcode probe.
 * Cutoff, unsigned LFO and unsigned bend are inferred by calling the actual
 * production vcf106_global_raw at zero-depth reference states. This avoids
 * creating a second handwritten arithmetic oracle in this harness.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "control106/vcf_control106.h"

int main(void)
{
    unsigned cutoff, depth, onset, mag, lfo_negative, bend_mag, bend_sens;
    unsigned bend_negative;
    while (scanf("%u %u %u %u %u %u %u %u",
                 &cutoff, &depth, &onset, &mag, &lfo_negative,
                 &bend_mag, &bend_sens, &bend_negative) == 8) {
        if (cutoff > 127u || depth > 127u || onset > 255u ||
            mag > 8191u || lfo_negative > 1u || bend_mag > 255u ||
            bend_sens > 255u || bend_negative > 1u)
            return 2;
        VcfControlParams106 p = {0};
        p.cutoff7 = (uint8_t)cutoff;
        const int32_t base = vcf106_global_raw(&p);
        p.lfo_depth7 = (uint8_t)depth;
        p.lfo_onset = (uint8_t)onset;
        p.lfo_magnitude = (uint16_t)mag;
        const int32_t lfo = vcf106_global_raw(&p) - base;
        p.lfo_depth7 = 0u;
        p.bend_magnitude = (uint8_t)bend_mag;
        p.bend_sensitivity = (uint8_t)bend_sens;
        const int32_t bend = vcf106_global_raw(&p) - base;
        p.lfo_depth7 = (uint8_t)depth;
        p.lfo_negative = lfo_negative != 0u;
        p.bend_negative = bend_negative != 0u;
        const int32_t all = vcf106_global_raw(&p);
        if (printf("%u,%u,%u,%d\n", (unsigned)base, (unsigned)lfo,
                   (unsigned)bend, (int)all) < 0)
            return 3;
    }
    return ferror(stdin) ? 4 : 0;
}
