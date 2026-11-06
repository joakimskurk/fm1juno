#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "control106/native_panel106.h"
#include "control106/vcf_control106.h"

#define CHECK(x) do { \
    if (!(x)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
        return EXIT_FAILURE; \
    } \
} while (0)

/* Test-only instruction-semantic oracle, not an original ROM executor.
 * No production control helpers are called to calculate expectations.
 * Source of skip conditions: NEC uPD7810/11 ALU documentation,
 * independently cross-checked against MAME's uPD7810/11 opcode model.
 *
 * The 8-bit arithmetic below explicitly represents the skip flag from
 * SUBNBX, GTI, SUINB and ADDNC instead of calling a clamp primitive. */
typedef struct Cpu8 {
    uint8_t value;
    bool skip_next;
} Cpu8;

static Cpu8 subtract_no_borrow8(uint8_t left, uint8_t right)
{
    const uint16_t difference = (uint16_t)left - (uint16_t)right;
    const Cpu8 out = {(uint8_t)difference, left >= right};
    return out;
}

static Cpu8 add_no_carry8(uint8_t left, uint8_t right)
{
    const uint16_t sum = (uint16_t)left + (uint16_t)right;
    const Cpu8 out = {(uint8_t)sum, sum <= UINT8_MAX};
    return out;
}

static uint8_t adc_condition_oracle(uint8_t held)
{
    Cpu8 step = subtract_no_borrow8(held, 4u);
    uint8_t accumulator = step.skip_next ? step.value : 0u;
    const uint8_t register_b = accumulator;

    step = subtract_no_borrow8(accumulator, 235u);
    if (step.skip_next) {
        /* SUINB skipped the branch to the normal linear output. */
        const Cpu8 plus = add_no_carry8(step.value, register_b);
        accumulator = plus.skip_next ? plus.value : UINT8_MAX;
    } else {
        /* The branch is taken and restores the linear endpoint. */
        accumulator = register_b;
    }
    return accumulator;
}

static uint8_t adc_held_oracle(uint8_t old_raw, uint8_t fresh_raw)
{
    Cpu8 delta = subtract_no_borrow8(fresh_raw, old_raw);
    uint8_t abs_difference = delta.skip_next
        ? delta.value : (uint8_t)(0u - (unsigned)delta.value);
    /* GTI A,1 skips the fallback load only if A>1. */
    return abs_difference > 1u ? fresh_raw : old_raw;
}

/* Independent split-byte multiply: equivalent to high 16 bits of an
 * unsigned 16x8 product shifted right by 8, via two 8x8 partials. */
static uint16_t mul_high_oracle(uint16_t word, uint8_t scaler)
{
    return (uint16_t)(
        (uint16_t)(word >> 8u) * (uint16_t)scaler +
        (((uint16_t)(word & UINT16_C(0xff)) * scaler) >> 8u));
}

typedef struct Cpu16 {
    uint16_t ea;
    bool low_underflow_flag;
    unsigned carries;
    unsigned borrows;
} Cpu16;

static void add_no_carry16(Cpu16 *cpu, uint16_t value)
{
    const uint32_t sum = (uint32_t)cpu->ea + (uint32_t)value;
    if (sum > UINT16_MAX) {
        ++cpu->carries;
        /* DADDNC did not skip the following MVI flag,0. */
        cpu->low_underflow_flag = false;
    }
    cpu->ea = (uint16_t)sum;
}

static void subtract_no_borrow16(Cpu16 *cpu, uint16_t value)
{
    if (cpu->ea < value) {
        ++cpu->borrows;
        /* DSUBNB did not skip the following MVI flag,1. */
        cpu->low_underflow_flag = true;
    }
    cpu->ea = (uint16_t)((uint32_t)cpu->ea - value);
}

static int32_t key_delta_oracle(uint16_t pitch, uint8_t depth7)
{
    const uint16_t pitch_coordinate =
        (uint16_t)((pitch / 4u) + (pitch / 8u));
    const int32_t distance = (int32_t)pitch_coordinate - 0x1680;
    const uint16_t absolute = (uint16_t)(distance < 0
        ? -distance : distance);
    const uint16_t scaled = mul_high_oracle(absolute,
                                          (uint8_t)(depth7 * 2u));
    return distance < 0 ? -(int32_t)scaled : (int32_t)scaled;
}

static int32_t global_signed_oracle(const VcfControlParams106 *p)
{
    const uint8_t lfo_depth = (uint8_t)(p->lfo_depth7 * 2u);
    const uint8_t onset_depth =
        (uint8_t)(((uint16_t)lfo_depth * p->lfo_onset) / 256u);
    const uint16_t magnitude = p->lfo_magnitude > UINT16_C(8191)
        ? UINT16_C(8191) : p->lfo_magnitude;
    const uint16_t lfo = (uint16_t)(
        mul_high_oracle(magnitude, onset_depth) / 2u);
    const uint16_t bend = (uint16_t)(
        ((uint16_t)p->bend_magnitude * p->bend_sensitivity) / 16u);
    return (int32_t)p->cutoff7 * 128 +
           (p->lfo_negative ? -(int32_t)lfo : (int32_t)lfo) +
           (p->bend_negative ? -(int32_t)bend : (int32_t)bend);
}

static uint16_t vcf_voice_cpu_oracle(const VcfControlParams106 *p,
                                     uint16_t envelope14,
                                     uint16_t pitch,
                                     unsigned *carry_total,
                                     unsigned *borrow_total)
{
    const uint8_t depth2 = (uint8_t)(p->lfo_depth7 * 2u);
    const uint8_t delayed = (uint8_t)(
        ((uint16_t)depth2 * p->lfo_onset) / 256u);
    const uint16_t lfo_mag = p->lfo_magnitude > UINT16_C(8191)
        ? UINT16_C(8191) : p->lfo_magnitude;
    const uint16_t lfo = (uint16_t)(
        mul_high_oracle(lfo_mag, delayed) / 2u);
    const uint16_t bend = (uint16_t)(
        ((uint16_t)p->bend_magnitude * p->bend_sensitivity) / 16u);

    Cpu16 cpu = {(uint16_t)(p->cutoff7 * 128u), false, 0u, 0u};
    if (p->lfo_negative) {
        subtract_no_borrow16(&cpu, lfo);
    } else {
        /* ROM $04CB adds the positive LFO without a skip-flag write. */
        cpu.ea = (uint16_t)((uint32_t)cpu.ea + lfo);
    }
    if (p->bend_negative)
        subtract_no_borrow16(&cpu, bend);
    else
        add_no_carry16(&cpu, bend);

    const uint16_t env = envelope14 > UINT16_C(0x3fff)
        ? UINT16_C(0x3fff) : envelope14;
    const uint16_t env_value = mul_high_oracle(
        env, (uint8_t)(p->env_depth7 * 2u));
    if (p->env_inverted)
        subtract_no_borrow16(&cpu, env_value);
    else
        add_no_carry16(&cpu, env_value);

    const int32_t key = key_delta_oracle(pitch, p->key_follow7);
    if (key < 0)
        subtract_no_borrow16(&cpu, (uint16_t)(-key));
    else
        add_no_carry16(&cpu, (uint16_t)key);

    if (carry_total != NULL) *carry_total += cpu.carries;
    if (borrow_total != NULL) *borrow_total += cpu.borrows;

    /* EAH & 0xc0 selects saturation. The retained flag distinguishes
     * negative wrap from positive overflow; do not saturate each stage. */
    if ((cpu.ea & UINT16_C(0xc000)) == 0u) return cpu.ea;
    return cpu.low_underflow_flag ? 0u : UINT16_C(0x3fff);
}

static int test_exhaustive_adc_pairwise_instruction_flags(void)
{
    /* Every possible 8-bit accepted state and incoming ADC byte,
     * including ±1 hysteresis, both wrap directions and saturation. */
    for (unsigned old = 0u; old <= 255u; ++old) {
        CHECK(native_adc106_condition((uint8_t)old) ==
              adc_condition_oracle((uint8_t)old));

        for (unsigned fresh = 0u; fresh <= 255u; ++fresh) {
            NativeAdc106 unit;
            native_adc106_init(&unit);
            unit.held[0] = (uint8_t)old;
            const uint32_t bank = (uint32_t)fresh;
            CHECK(native_adc106_receive_bank(&unit, 0u, bank));
            uint8_t processed = UINT8_MAX;
            CHECK(native_adc106_process_latest(&unit, &processed));
            CHECK(processed == 0u);
            const uint8_t held = adc_held_oracle(
                (uint8_t)old, (uint8_t)fresh);
            CHECK(unit.held[0] == held);
            CHECK(unit.conditioned[0] == adc_condition_oracle(held));
        }
    }
    /* Two distinct switch branches at the second correction threshold. */
    CHECK(adc_condition_oracle(238u) == 234u);
    CHECK(adc_condition_oracle(239u) == 235u);
    CHECK(adc_condition_oracle(240u) == 237u);
    CHECK(adc_condition_oracle(248u) == 253u);
    CHECK(adc_condition_oracle(249u) == 255u);
    return EXIT_SUCCESS;
}

static uint32_t rng_state = UINT32_C(0x472e19b5);

static uint32_t next_random(void)
{
    /* Deterministic xorshift avoids a platform-specific rand() sequence. */
    uint32_t v = rng_state;
    v ^= v << 13u;
    v ^= v >> 17u;
    v ^= v << 5u;
    rng_state = v;
    return v;
}

static int compare_vcf_case(const VcfControlParams106 *p,
                            uint16_t envelope,
                            uint16_t pitch,
                            unsigned *carry_total,
                            unsigned *borrow_total)
{
    const int32_t signed_global = global_signed_oracle(p);
    CHECK(vcf106_global_raw(p) == signed_global);
    const uint16_t expected = vcf_voice_cpu_oracle(
        p, envelope, pitch, carry_total, borrow_total);
    CHECK(vcf106_voice_code14(p, signed_global, envelope, pitch) ==
          expected);
    CHECK(vcf106_voice_dac12(p, signed_global, envelope, pitch) ==
          (uint16_t)(expected >> 2u));
    CHECK(vcf106_key_follow_delta(pitch, p->key_follow7) ==
          key_delta_oracle(pitch, p->key_follow7));
    return EXIT_SUCCESS;
}

static int test_vcf_wrap_flags_and_boundaries(void)
{
    unsigned carry_total = 0u;
    unsigned borrow_total = 0u;

    const uint8_t panels[] = {0u, 1u, 63u, 64u, 126u, 127u};
    const uint16_t pitches[] = {0u, 1u, 0x3bffu, 0x3c00u,
                                0x3c01u, 0x7f00u, 0xffffu};
    const uint16_t envelopes[] = {0u, 1u, 0x00ffu,
                                  0x1000u, 0x3ffeu, 0x3fffu};

    for (unsigned sign_mask = 0u; sign_mask < 8u; ++sign_mask) {
        for (unsigned c = 0u; c < 6u; ++c) {
            VcfControlParams106 p = {0};
            p.cutoff7 = panels[c];
            p.lfo_depth7 = panels[(c + 5u) % 6u];
            p.lfo_magnitude = (c & 1u) == 0u
                ? UINT16_C(8191) : UINT16_C(4096);
            p.lfo_onset = (uint8_t)((c & 1u) ? 128u : 255u);
            p.bend_magnitude = (uint8_t)((c & 1u) ? 128u : 255u);
            p.bend_sensitivity = panels[(c + 3u) % 6u];
            p.env_depth7 = panels[(c + 1u) % 6u];
            p.key_follow7 = panels[(c + 4u) % 6u];
            p.lfo_negative = (sign_mask & 1u) != 0u;
            p.bend_negative = (sign_mask & 2u) != 0u;
            p.env_inverted = (sign_mask & 4u) != 0u;
            for (unsigned i = 0u; i < 7u; ++i)
                for (unsigned j = 0u; j < 6u; ++j)
                    if (compare_vcf_case(&p, envelopes[j], pitches[i],
                                         &carry_total, &borrow_total))
                        return EXIT_FAILURE;
        }
    }

    /* Independent fixed seeds cover intermediate carry/borrow combinations,
     * not just final-clamp extremes. */
    for (unsigned i = 0u; i < 90000u; ++i) {
        VcfControlParams106 p = {0};
        p.cutoff7 = (uint8_t)(next_random() % 128u);
        p.lfo_depth7 = (uint8_t)(next_random() % 128u);
        p.lfo_onset = (uint8_t)next_random();
        p.lfo_magnitude = (uint16_t)next_random();
        p.lfo_negative = (next_random() & UINT32_C(0x80000000)) != 0u;
        p.bend_magnitude = (uint8_t)next_random();
        p.bend_sensitivity = (uint8_t)next_random();
        p.bend_negative = (next_random() & UINT32_C(0x80000000)) != 0u;
        p.env_depth7 = (uint8_t)(next_random() % 128u);
        p.env_inverted = (next_random() & UINT32_C(0x80000000)) != 0u;
        p.key_follow7 = (uint8_t)(next_random() % 128u);
        const uint16_t envelope = (uint16_t)next_random();
        const uint16_t pitch = (uint16_t)next_random();
        if (compare_vcf_case(&p, envelope, pitch,
                             &carry_total, &borrow_total))
            return EXIT_FAILURE;
    }
    CHECK(carry_total > 0u && borrow_total > 0u);
    return EXIT_SUCCESS;
}

int main(void)
{
    if (test_exhaustive_adc_pairwise_instruction_flags())
        return EXIT_FAILURE;
    if (test_vcf_wrap_flags_and_boundaries())
        return EXIT_FAILURE;
    puts("fm1juno: independent uPD7811 ALU skip / VCF wrap vectors passed");
    return EXIT_SUCCESS;
}
