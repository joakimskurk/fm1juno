#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "control106/vcf_control106.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return EXIT_FAILURE; \
    } \
} while (0)

static int test_cutoff_and_dac(void)
{
    VcfControlParams106 p = {0};
    CHECK(vcf106_global_raw(&p) == 0);
    p.cutoff7 = 1u;
    CHECK(vcf106_global_raw(&p) == 0x0080);
    p.cutoff7 = 127u;
    CHECK(vcf106_global_raw(&p) == 0x3f80);
    CHECK(vcf106_voice_code14(&p, vcf106_global_raw(&p), 0u,
                              60u << 8u) == 0x3f80u);
    CHECK(vcf106_voice_dac12(&p, vcf106_global_raw(&p), 0u,
                             60u << 8u) == 0x0fe0u);
    return EXIT_SUCCESS;
}

static int test_lfo_and_bender_pipeline(void)
{
    VcfControlParams106 p = {0};
    p.lfo_depth7 = 127u;
    p.lfo_onset = 255u;
    p.lfo_magnitude = 0x1fffu;
    CHECK(vcf106_global_raw(&p) == 4047);
    p.lfo_negative = true;
    CHECK(vcf106_global_raw(&p) == -4047);
    p.lfo_negative = false;
    p.bend_magnitude = 255u;
    p.bend_sensitivity = 255u;
    CHECK(vcf106_global_raw(&p) == 8111);
    p.cutoff7 = 64u;
    CHECK(vcf106_global_raw(&p) == 16303);
    p.lfo_negative = true;
    p.bend_negative = true;
    CHECK(vcf106_global_raw(&p) == 81);
    p.lfo_onset = 0u;
    CHECK(vcf106_global_raw(&p) == 4128);
    return EXIT_SUCCESS;
}

static int test_key_follow(void)
{
    CHECK(vcf106_key_follow_delta((uint16_t)(60u << 8u), 127u) == 0);
    CHECK(vcf106_key_follow_delta((uint16_t)(48u << 8u), 127u) == -1143);
    CHECK(vcf106_key_follow_delta((uint16_t)(72u << 8u), 127u) == 1143);
    CHECK(vcf106_key_follow_delta((uint16_t)(60u << 8u), 0u) == 0);
    /* Per-ROM separate truncation, not floor(pitch * 3 / 8). */
    CHECK(vcf106_key_follow_delta((uint16_t)((60u << 8u) + 15u), 127u) == 3);
    return EXIT_SUCCESS;
}

static int test_env_composition_and_late_clamp(void)
{
    VcfControlParams106 p = {0};
    p.env_depth7 = 127u;
    CHECK(vcf106_voice_code14(&p, 0, 0x3fffu, 60u << 8u) == 16255u);
    p.env_inverted = true;
    CHECK(vcf106_voice_code14(&p, 0, 0x3fffu, 60u << 8u) == 0u);
    p.env_inverted = false;
    /* Global underflow is not clipped prematurely: ENV may restore it. */
    CHECK(vcf106_voice_code14(&p, -100, 0x1000u, 60u << 8u) == 3964u);
    p.env_inverted = true;
    /* Likewise, an over-range global base may return into range. */
    CHECK(vcf106_voice_code14(&p, 17000, 0x1000u, 60u << 8u) == 12936u);
    p.env_inverted = false;
    CHECK(vcf106_voice_code14(&p, 17000, 0u, 60u << 8u) == 0x3fffu);
    CHECK(vcf106_voice_dac12(&p, 17000, 0u, 60u << 8u) == 0x0fffu);
    return EXIT_SUCCESS;
}

static int test_six_voice_independence(void)
{
    VcfControlParams106 p = {0};
    p.cutoff7 = 64u;
    p.key_follow7 = 127u;
    const int32_t base = vcf106_global_raw(&p);
    const uint16_t pitches[] = {
        (uint16_t)(48u << 8u), (uint16_t)(54u << 8u),
        (uint16_t)(60u << 8u), (uint16_t)(66u << 8u),
        (uint16_t)(72u << 8u), (uint16_t)(84u << 8u)
    };
    uint16_t previous = 0u;
    for (unsigned i = 0u; i < 6u; ++i) {
        const uint16_t value = vcf106_voice_code14(
            &p, base, 0u, pitches[i]);
        if (i != 0u) CHECK(value > previous);
        previous = value;
    }
    CHECK(vcf106_voice_code14(&p, base, 0u, 60u << 8u) == 8192u);
    return EXIT_SUCCESS;
}

int main(void)
{
    if (test_cutoff_and_dac()) return EXIT_FAILURE;
    if (test_lfo_and_bender_pipeline()) return EXIT_FAILURE;
    if (test_key_follow()) return EXIT_FAILURE;
    if (test_env_composition_and_late_clamp()) return EXIT_FAILURE;
    if (test_six_voice_independence()) return EXIT_FAILURE;
    puts("fm1juno: VCF control arithmetic passed");
    return EXIT_SUCCESS;
}
