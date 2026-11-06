#ifndef FM1JUNO_SECONDARY_CONTROL106_H
#define FM1JUNO_SECONDARY_CONTROL106_H

#include <stdbool.h>
#include <stdint.h>

#include "common/fixed_time.h"

/* Distinct mux banks matter even when channel numbers happen to coincide.
 * The names represent the external S/H bank selected after loadDac. */
typedef enum SecondaryMuxBank106 {
    SECONDARY106_MUX_IC23_VCF = 0,
    SECONDARY106_MUX_IC24_RAMP,
    SECONDARY106_MUX_IC26_VCA
} SecondaryMuxBank106;

/* Enum IDs for the five globally controlled DAC/S&H lanes. */
typedef enum SecondaryLane106 {
    SECONDARY106_RESONANCE = 0,
    SECONDARY106_GLOBAL_VCA,
    SECONDARY106_SUB_LEVEL,
    SECONDARY106_PWM,
    SECONDARY106_NOISE,
    SECONDARY106_GLOBAL_LANES
} SecondaryLane106;

typedef struct SecondaryMuxRoute106 {
    SecondaryMuxBank106 bank;
    uint8_t channel;
} SecondaryMuxRoute106;

/* Held *digital* codes, not analogue voltages or gain responses. */
typedef struct SecondaryHold106 {
    uint16_t code14;
    uint16_t dac12;
    Fm1junoTimeQ32 committed_at;
    uint32_t pass_id;
    bool valid;
} SecondaryHold106;

typedef struct SecondaryOutputs106 {
    SecondaryHold106 global[SECONDARY106_GLOBAL_LANES];
    SecondaryHold106 voice_vca[6];
    uint16_t pwm_next_code14;
} SecondaryOutputs106;

SecondaryMuxRoute106 secondary106_route(SecondaryLane106 lane);
SecondaryMuxRoute106 secondary106_voice_vca_route(uint8_t voice);

/* 7-bit Roland patch parameter is left-shifted to 14-bit control code. */
uint16_t secondary106_patch_code14(uint8_t raw7);

/* PWM uses the post-LFO-step 0..0x1fff magnitude; manually selected PWM
 * replaces the LFO carrier with 0x3fff. The pulse-off switch forces zero.
 * The ROM stores this result for the *following* first-voice service slot. */
uint16_t secondary106_pwm_code14(uint8_t pwm_depth7,
                                  bool manual_mode,
                                  bool pulse_enabled,
                                  uint16_t lfo_magnitude,
                                  bool lfo_negative);

/* Per-voice VCA is independent of the global uPC1252 VCA LEVEL.
 * The ROM gate mode bypasses envelope level and uses voiceRun bits. */
uint16_t secondary106_voice_vca_code14(uint16_t envelope14,
                                       bool gate_mode,
                                       bool voice_running);

void secondary106_commit(SecondaryHold106 *hold,
                         uint16_t code14,
                         Fm1junoTimeQ32 time,
                         uint32_t pass_id);

#endif
