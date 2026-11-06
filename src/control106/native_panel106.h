#ifndef FM1JUNO_NATIVE_PANEL106_H
#define FM1JUNO_NATIVE_PANEL106_H

#include <stdbool.h>
#include <stdint.h>

/* Roland module CPU serial controls (addresses 0x014F and 0x0184).
 * Preserve the actual byte and the external IC40/PF outputs separately
 * from high-level semantic panel values. */
typedef struct NativeSwitch106 {
    uint8_t switch1;
    uint8_t switch2;
    uint8_t ic40_register;
    uint8_t ic40_output;
    uint8_t pf_prescaler_bits;
    bool switch1_seen;
    bool switch2_seen;
} NativeSwitch106;

void native_switch106_init(NativeSwitch106 *s);
void native_switch106_receive1(NativeSwitch106 *s, uint8_t value);
void native_switch106_receive2(NativeSwitch106 *s, uint8_t value);
void native_switch106_frame_latch(NativeSwitch106 *s);
bool native_switch106_pulse_enabled(const NativeSwitch106 *s);
bool native_switch106_pwm_manual(const NativeSwitch106 *s);
bool native_switch106_vcf_env_inverted(const NativeSwitch106 *s);
bool native_switch106_vca_gate(const NativeSwitch106 *s);
uint8_t native_switch106_hpf_select(const NativeSwitch106 *s);

/* ADC interrupt snapshots are 4-channel banks (0..3 or 4..7).
 * The main loop later filters only the last bank the ISR published.
 * The conditioned values are NOT the same as 7-bit patch bytes. */
typedef struct NativeAdc106 {
    uint8_t raw[8];
    uint8_t held[8];
    uint8_t conditioned[8];
    uint8_t last_interrupt_bank;
    bool snapshot_pending;
    uint32_t published_snapshots;
    uint32_t serviced_banks;
} NativeAdc106;

void native_adc106_init(NativeAdc106 *a);
bool native_adc106_receive_bank(NativeAdc106 *a, uint8_t bank,
                                uint32_t packed_le_bytes);
uint8_t native_adc106_condition(uint8_t held_raw);
bool native_adc106_process_latest(NativeAdc106 *a, uint8_t *serviced_bank);

#endif
