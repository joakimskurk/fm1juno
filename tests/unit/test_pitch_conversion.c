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

int main(void)
{
    if (test_lookup_clamps()) return EXIT_FAILURE;
    if (test_rom_equivalent_interpolation()) return EXIT_FAILURE;
    if (test_clean_dco_cv_generator()) return EXIT_FAILURE;
    if (test_dco_cv_from_pitch()) return EXIT_FAILURE;
    puts("fm1juno: pitch conversion tests passed");
    return EXIT_SUCCESS;
}
