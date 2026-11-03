#include <stdio.h>
#include <stdlib.h>

#include "control106/pitch106.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return EXIT_FAILURE; \
    } \
} while (0)

static int test_lookup_clamps(void)
{
    PitchLookupCoordinate106 c;

    c = pitch106_lookup_coordinate(0x2fffu);
    CHECK(c.index == 0u && c.fraction == 0u);

    c = pitch106_lookup_coordinate(0x3000u);
    CHECK(c.index == 0u && c.fraction == 0u);

    c = pitch106_lookup_coordinate(0x3018u);
    CHECK(c.index == 0u && c.fraction == 0x18u);

    c = pitch106_lookup_coordinate(0x9601u);
    CHECK(c.index == 102u && c.fraction == 0x01u);

    c = pitch106_lookup_coordinate(0x9701u);
    CHECK(c.index == 102u && c.fraction == 0u);
    return EXIT_SUCCESS;
}

static int test_rom_equivalent_interpolation(void)
{
    /* Sparse behavioral anchors from the documented public disassembly.
     * These are regression vectors, not a copied lookup table. */
    const uint16_t n0 = UINT16_C(0xf06e);
    const uint16_t n1 = UINT16_C(0xe2e9);
    CHECK(pitch106_interpolate_descending(n0, n1, 0x00u) == 61550u);
    CHECK(pitch106_interpolate_descending(n0, n1, 0x18u) == 61226u);
    CHECK(pitch106_interpolate_descending(n0, n1, 0x40u) == 60685u);
    CHECK(pitch106_interpolate_descending(n0, n1, 0x80u) == 59820u);
    CHECK(pitch106_interpolate_descending(n0, n1, 0xc0u) == 58955u);
    CHECK(pitch106_interpolate_descending(n0, n1, 0xffu) == 58103u);

    CHECK(pitch106_interpolate_ascending(0x0020u, 0x0022u, 0x00u) == 0x0020u);
    CHECK(pitch106_interpolate_ascending(0x0020u, 0x0022u, 0x80u) == 0x0021u);
    CHECK(pitch106_interpolate_ascending(0x0020u, 0x0022u, 0xffu) == 0x0021u);
    return EXIT_SUCCESS;
}

static int abs_i(int value)
{
    return value < 0 ? -value : value;
}

static int test_clean_dco_cv_generator(void)
{
    struct Anchor {
        uint8_t index;
        uint16_t rom_value;
    } anchors[] = {
        {0u, 0x0020u},
        {12u, 0x0040u},
        {16u, 0x0050u},
        {24u, 0x0080u},
        {40u, 0x0142u},
        {48u, 0x0200u},
        {66u, 0x05a9u},
        {72u, 0x0800u},
        {83u, 0x0f1bu},
        {84u, 0x0fffu},
        {102u, 0x0fffu}
    };

    for (size_t i = 0u; i < sizeof anchors / sizeof anchors[0]; ++i) {
        const uint16_t derived = pitch106_dco_cv_base_derived(anchors[i].index);
        CHECK(abs_i((int)derived - (int)anchors[i].rom_value) <= 1);
    }

    CHECK(pitch106_dco_cv_base_derived(0u) == 32u);
    CHECK(pitch106_dco_cv_base_derived(12u) == 64u);
    CHECK(pitch106_dco_cv_base_derived(24u) == 128u);
    CHECK(pitch106_dco_cv_base_derived(36u) == 256u);
    CHECK(pitch106_dco_cv_base_derived(84u) == PITCH106_DCO_CV_MAX);
    CHECK(pitch106_dco_cv_base_derived(103u) == PITCH106_DCO_CV_MAX);
    return EXIT_SUCCESS;
}

static int test_dco_cv_from_pitch(void)
{
    CHECK(pitch106_dco_cv12_from_pitch(0x2fffu) == 32u);
    CHECK(pitch106_dco_cv12_from_pitch(0x3000u) == 32u);
    CHECK(pitch106_dco_cv12_from_pitch(0x3080u) == 33u);
    CHECK(pitch106_dco_cv12_from_pitch(0x3c00u) == 64u);
    CHECK(pitch106_dco_cv12_from_pitch(0x9701u) == PITCH106_DCO_CV_MAX);
    return EXIT_SUCCESS;
}

static int test_generated_timer_divisor_candidate(void)
{
    PitchTimerBase106 profile;
    pitch106_timer_build_derived_candidate(&profile);

    /* Sparse independent regression anchors from the public module
     * behavior analysis. NOT the original 104-entry ROM data table. */
    struct Anchor { uint8_t index; uint16_t observed; } anchors[] = {
        {0u, 0xf06eu}, {2u, 0xd61fu}, {5u, 0xb404u},
        {12u, 0x780fu}, {21u, 0x4754u}, {24u, 0x3bfcu},
        {36u, 0x1dfbu}, {48u, 0x0efdu}, {60u, 0x077eu},
        {72u, 0x03bfu}, {84u, 0x01dfu}, {96u, 0x00f0u},
        {102u, 0x00a9u}, {103u, 0x00a0u}
    };
    for (size_t i = 0u; i < sizeof anchors / sizeof anchors[0]; ++i) {
        const uint32_t generated = profile.counts[anchors[i].index];
        const uint32_t observed = anchors[i].observed;
        const uint32_t difference = generated > observed
            ? generated - observed : observed - generated;

        /* Conservative integer <0.5-cent relative error for sparse
         * anchors: 0.5 cents ~= 1 / 3462. */
        CHECK(difference * 3462u <= observed);
    }

    for (size_t i = 1u; i < PITCH106_TIMER_ENTRY_COUNT; ++i) {
        CHECK(profile.counts[i] > 0u);
        CHECK(profile.counts[i] < profile.counts[i - 1u]);
    }

    CHECK(pitch106_timer_count_from_pitch(&profile, 0x2fffu) ==
          profile.counts[0]);
    CHECK(pitch106_timer_count_from_pitch(&profile, 0x3000u) ==
          profile.counts[0]);
    CHECK(pitch106_timer_count_from_pitch(&profile, 0x3080u) ==
          pitch106_interpolate_descending(profile.counts[0],
                                           profile.counts[1], 0x80u));
    CHECK(pitch106_timer_count_from_pitch(&profile, 0x96ffu) ==
          pitch106_interpolate_descending(profile.counts[102],
                                           profile.counts[103], 0xffu));
    CHECK(pitch106_timer_count_from_pitch(&profile, 0x9700u) ==
          profile.counts[102]);

    /* This profile is demonstrably not bit-identical and must not be
     * silently promoted to the strict ROM behavior tier. */
    CHECK(profile.counts[0] != anchors[0].observed);
    return EXIT_SUCCESS;
}

int main(void)
{
    if (test_lookup_clamps()) return EXIT_FAILURE;
    if (test_rom_equivalent_interpolation()) return EXIT_FAILURE;
    if (test_clean_dco_cv_generator()) return EXIT_FAILURE;
    if (test_dco_cv_from_pitch()) return EXIT_FAILURE;
    if (test_generated_timer_divisor_candidate()) return EXIT_FAILURE;
    puts("fm1juno: pitch conversion tests passed");
    return EXIT_SUCCESS;
}
