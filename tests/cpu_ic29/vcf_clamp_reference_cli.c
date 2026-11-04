/* Paired with run_vcf_opcode_window.py; production VCF is called directly
 * with an integer signed global base, zero ENV/depth and mid-C key pivot.
 * The caller passes a decimal signed int, not a wrapped CPU register. */
#include <stdint.h>
#include <stdio.h>
#include "control106/vcf_control106.h"

int main(void)
{
    int value;
    VcfControlParams106 p = {0};
    while (scanf("%d", &value) == 1) {
        if (printf("%u\n", (unsigned)vcf106_voice_code14(
                &p, value, 0u, UINT16_C(60) << 8u)) < 0)
            return 2;
    }
    return ferror(stdin) ? 3 : 0;
}
