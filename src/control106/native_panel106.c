#include "control106/native_panel106.h"

void native_switch106_init(NativeSwitch106 *s)
{
    *s = (NativeSwitch106){0};
    /* IC29 cold-start state at $02b6..$02bf. */
    s->ic40_register = UINT8_C(0x15);
    s->ic40_output = UINT8_C(0x15);
    s->pf_prescaler_bits = UINT8_C(0x40);
    s->switch2 = UINT8_C(0x06);
}

void native_switch106_receive1(NativeSwitch106 *s, uint8_t value)
{
    s->switch1 = value;
    s->switch1_seen = true;

    /* $014f..$0164: preserve IC40 HPF bits, refresh saw and
     * inverted chorus-on plus chorus-speed control bit. */
    s->ic40_register = (uint8_t)(
        (s->ic40_register & UINT8_C(0x0c)) |
        (value & UINT8_C(0x10)) |
        ((value & UINT8_C(0x20)) == 0u ? UINT8_C(0x01) : 0u) |
        ((value & UINT8_C(0x40)) != 0u ? UINT8_C(0x02) : 0u));

    /* $016f..$0182: the octave/range selection is active LOW.
     * 0x04 clear wins (4'); otherwise 0x02 clear selects 8'. */
    s->pf_prescaler_bits = (value & UINT8_C(0x04)) == 0u
        ? UINT8_C(0xc0)
        : ((value & UINT8_C(0x02)) == 0u
           ? UINT8_C(0x40) : 0u);
}

void native_switch106_receive2(NativeSwitch106 *s, uint8_t value)
{
    s->switch2 = value;
    s->switch2_seen = true;
    /* $0184..$0196: XOR HPF selector bits before IC40 bit positions. */
    s->ic40_register = (uint8_t)(
        (s->ic40_register & UINT8_C(0xf3)) |
        (((value ^ UINT8_C(0x18)) & UINT8_C(0x18)) >> 1u));
}

void native_switch106_frame_latch(NativeSwitch106 *s)
{
    /* $02ec..$02ee: register is driven onto $3000 at loop entry. */
    s->ic40_output = s->ic40_register;
}

bool native_switch106_pulse_enabled(const NativeSwitch106 *s)
{
    /* $0166..$016c: bit 3 clear keeps square enabled. */
    return (s->switch1 & UINT8_C(0x08)) == 0u;
}

bool native_switch106_pwm_manual(const NativeSwitch106 *s)
{
    /* $076c..$0770: bit 0 clear substitutes 0x3fff (manual). */
    return (s->switch2 & UINT8_C(0x01)) == 0u;
}

bool native_switch106_vcf_env_inverted(const NativeSwitch106 *s)
{
    /* $05d7..$0619: bit 1 clear selects subtraction. */
    return (s->switch2 & UINT8_C(0x02)) == 0u;
}

bool native_switch106_vca_gate(const NativeSwitch106 *s)
{
    /* $05a6..$05b2: bit 2 clear is gate mode. */
    return (s->switch2 & UINT8_C(0x04)) == 0u;
}

uint8_t native_switch106_hpf_select(const NativeSwitch106 *s)
{
    return (uint8_t)((s->ic40_register >> 2u) & 3u);
}

void native_adc106_init(NativeAdc106 *a)
{
    *a = (NativeAdc106){0};
}

bool native_adc106_receive_bank(NativeAdc106 *a, uint8_t bank,
                                uint32_t packed_le_bytes)
{
    if (bank > 1u) return false;
    for (unsigned i = 0u; i < 4u; ++i)
        a->raw[bank * 4u + i] =
            (uint8_t)(packed_le_bytes >> (i * 8u));
    a->last_interrupt_bank = bank;
    a->snapshot_pending = true;
    ++a->published_snapshots;
    return true;
}

uint8_t native_adc106_condition(uint8_t held_raw)
{
    /* $080c..$081a: remove bottom 4 ADC codes, then a two-slope
     * top-end correction (n>=235: 2*n-235), saturating at 255. */
    const uint16_t n = held_raw > 4u
        ? (uint16_t)held_raw - 4u : 0u;
    if (n < 235u) return (uint8_t)n;
    const uint16_t corrected = 2u * n - 235u;
    return corrected > 255u ? 255u : (uint8_t)corrected;
}

bool native_adc106_process_latest(NativeAdc106 *a, uint8_t *serviced_bank)
{
    if (!a->snapshot_pending) return false;
    const uint8_t bank = a->last_interrupt_bank;
    for (unsigned i = 0u; i < 4u; ++i) {
        const unsigned at = 4u * bank + i;
        const int difference = (int)a->raw[at] - (int)a->held[at];
        if (difference < -1 || difference > 1)
            a->held[at] = a->raw[at];
        a->conditioned[at] = native_adc106_condition(a->held[at]);
    }
    a->snapshot_pending = false;
    ++a->serviced_banks;
    if (serviced_bank != 0) *serviced_bank = bank;
    return true;
}
