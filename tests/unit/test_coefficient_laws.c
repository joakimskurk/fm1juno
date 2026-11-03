#include <stdio.h>
#include <stdlib.h>

#include "control106/envelope106.h"
#include "control106/lfo106.h"
#include "control106/lfo_delay106.h"
#include "control106/portamento106.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return EXIT_FAILURE; \
    } \
} while (0)

static int test_portamento_compact_law(void)
{
    CHECK(portamento106_step_from_index(0u) == 0u);
    CHECK(portamento106_step_from_index(1u) == 255u);
    CHECK(portamento106_step_from_index(25u) == 63u);
    CHECK(portamento106_step_from_index(26u) == 61u);
    CHECK(portamento106_step_from_index(47u) == 19u);
    CHECK(portamento106_step_from_index(48u) == 18u);
    CHECK(portamento106_step_from_index(49u) == 17u);
    CHECK(portamento106_step_from_index(50u) == 16u);
    CHECK(portamento106_step_from_index(53u) == 16u);
    CHECK(portamento106_step_from_index(54u) == 15u);
    CHECK(portamento106_step_from_index(61u) == 14u);
    CHECK(portamento106_step_from_index(62u) == 13u);
    CHECK(portamento106_step_from_index(66u) == 13u);
    CHECK(portamento106_step_from_index(67u) == 12u);
    CHECK(portamento106_step_from_index(121u) == 2u);
    CHECK(portamento106_step_from_index(122u) == 1u);
    CHECK(portamento106_step_from_index(127u) == 1u);

    CHECK(portamento106_step_from_raw(0u) == 0u);
    CHECK(portamento106_step_from_raw(1u) == 0u);
    CHECK(portamento106_step_from_raw(2u) == 255u);
    CHECK(portamento106_step_from_raw(255u) == 1u);

    uint8_t previous = 255u;
    for (uint16_t i = 1u; i < 128u; ++i) {
        const uint8_t step = portamento106_step_from_index((uint8_t)i);
        CHECK(step >= 1u);
        CHECK(step <= previous);
        previous = step;
    }
    return EXIT_SUCCESS;
}

static int test_env_attack_compact_law(void)
{
    struct Anchor {
        uint8_t index;
        uint16_t value;
    } anchors[] = {
        {0u, 0x4000u}, {1u, 0x2000u}, {2u, 0x1000u},
        {3u, 0x0aaau}, {8u, 0x0400u}, {20u, 0x0199u},
        {28u, 0x0124u}, {32u, 0x0100u}, {63u, 0x0082u},
        {64u, 0x007fu}, {85u, 0x0045u}, {86u, 0x0042u},
        {87u, 0x0040u}, {108u, 0x0021u}, {109u, 0x0021u},
        {121u, 0x001bu}, {122u, 0x001au}, {127u, 0x0015u}
    };

    for (size_t i = 0u; i < sizeof anchors / sizeof anchors[0]; ++i)
        CHECK(envelope106_attack_increment(anchors[i].index) == anchors[i].value);

    uint16_t previous = envelope106_attack_increment(0u);
    for (uint16_t i = 1u; i < 128u; ++i) {
        const uint16_t value = envelope106_attack_increment((uint8_t)i);
        CHECK(value <= previous);
        CHECK(value > 0u);
        previous = value;
    }
    return EXIT_SUCCESS;
}

static int test_env_sustain_law(void)
{
    CHECK(envelope106_sustain_level_from_raw(0u) == 0x0000u);
    CHECK(envelope106_sustain_level_from_raw(1u) == 0x0080u);
    CHECK(envelope106_sustain_level_from_raw(64u) == 0x2000u);
    CHECK(envelope106_sustain_level_from_raw(127u) == 0x3f80u);
    CHECK(envelope106_sustain_level_from_raw(255u) == 0x3f80u);
    return EXIT_SUCCESS;
}

static int test_env_decay_release_compact_law(void)
{
    struct Anchor {
        uint8_t index;
        uint16_t value;
    } anchors[] = {
        {0u, 0x1000u}, {4u, 0x9000u},
        {5u, 0xa000u}, {15u, 0xf000u},
        {16u, 0xf080u}, {43u, 0xfe00u},
        {44u, 0xfe0cu}, {65u, 0xff08u},
        {66u, 0xff0cu}, {123u, 0xfff0u},
        {124u, 0xfff1u}, {127u, 0xfff4u}
    };

    uint16_t previous = 0u;
    for (size_t i = 0u; i < sizeof anchors / sizeof anchors[0]; ++i)
        CHECK(envelope106_decay_release_coefficient(anchors[i].index) == anchors[i].value);

    for (uint16_t i = 0u; i < 128u; ++i) {
        const uint16_t value =
            envelope106_decay_release_coefficient((uint8_t)i);
        if (i != 0u) CHECK(value > previous);
        previous = value;
    }
    return EXIT_SUCCESS;
}

static int test_lfo_depth_compact_law(void)
{
    CHECK(lfo106_depth_scaler(0u) == 0u);
    CHECK(lfo106_depth_scaler(2u) == 0u);
    CHECK(lfo106_depth_scaler(3u) == 1u);
    CHECK(lfo106_depth_scaler(64u) == 62u);
    CHECK(lfo106_depth_scaler(65u) == 64u);
    CHECK(lfo106_depth_scaler(95u) == 124u);
    CHECK(lfo106_depth_scaler(96u) == 128u);
    CHECK(lfo106_depth_scaler(124u) == 240u);
    CHECK(lfo106_depth_scaler(125u) == 248u);
    CHECK(lfo106_depth_scaler(126u) == 255u);
    CHECK(lfo106_depth_scaler(127u) == 255u);
    return EXIT_SUCCESS;
}

static int test_lfo_delay_compact_laws(void)
{
    CHECK(lfo_delay106_holdoff_increment(0u) == envelope106_attack_increment(0u));
    CHECK(lfo_delay106_holdoff_increment(64u) == envelope106_attack_increment(64u));
    CHECK(lfo_delay106_holdoff_increment(127u) == envelope106_attack_increment(127u));

    CHECK(lfo_delay106_ramp_index(0x00u) == 0u);
    CHECK(lfo_delay106_ramp_index(0x10u) == 1u);
    CHECK(lfo_delay106_ramp_index(0x30u) == 3u);
    CHECK(lfo_delay106_ramp_index(0x70u) == 7u);
    CHECK(lfo_delay106_ramp_index(0xffu) == 7u);

    CHECK(lfo_delay106_ramp_increment_from_index(0u) == 0xffffu);
    CHECK(lfo_delay106_ramp_increment_from_index(1u) == 0x0419u);
    CHECK(lfo_delay106_ramp_increment_from_index(2u) == 0x020cu);
    CHECK(lfo_delay106_ramp_increment_from_index(3u) == 0x015eu);
    CHECK(lfo_delay106_ramp_increment_from_index(4u) == 0x0100u);
    CHECK(lfo_delay106_ramp_increment_from_index(7u) == 0x0100u);
    return EXIT_SUCCESS;
}

int main(void)
{
    if (test_portamento_compact_law()) return EXIT_FAILURE;
    if (test_env_attack_compact_law()) return EXIT_FAILURE;
    if (test_env_sustain_law()) return EXIT_FAILURE;
    if (test_env_decay_release_compact_law()) return EXIT_FAILURE;
    if (test_lfo_depth_compact_law()) return EXIT_FAILURE;
    if (test_lfo_delay_compact_laws()) return EXIT_FAILURE;
    puts("fm1juno: compact coefficient-law tests passed");
    return EXIT_SUCCESS;
}
