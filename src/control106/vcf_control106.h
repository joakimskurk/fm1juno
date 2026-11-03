#ifndef FM1JUNO_VCF_CONTROL106_H
#define FM1JUNO_VCF_CONTROL106_H

#include <stdbool.h>
#include <stdint.h>

#define VCF106_MAX_CODE14 UINT16_C(0x3fff)
#define VCF106_KEY_PIVOT_3_8 UINT16_C(0x1680)

/* Firmware-domain parameters, not an analogue cutoff-frequency model.
 * Panel parameters are nominal 7-bit values. */
typedef struct VcfControlParams106 {
    uint8_t cutoff7;
    uint8_t lfo_depth7;
    uint8_t lfo_onset;
    uint16_t lfo_magnitude; /* 0..0x1fff, unsigned; sign is separate */
    bool lfo_negative;
    uint8_t bend_magnitude;
    uint8_t bend_sensitivity;
    bool bend_negative;
    uint8_t env_depth7;
    bool env_inverted;
    uint8_t key_follow7;
} VcfControlParams106;

/* Unclamped signed working value: global Cutoff +/- LFO +/- Bender.
 * IMPORTANT: do not clamp this before adding per-voice ENV/Key Follow. */
int32_t vcf106_global_raw(const VcfControlParams106 *params);

/* Signed Key Follow contribution using *Portamento* pitch in 8.8 units
 * and the ROM's separately truncated (pitch>>2)+(pitch>>3) coordinate. */
int32_t vcf106_key_follow_delta(uint16_t portamento_pitch_8_8,
                                 uint8_t key_follow7);

/* Final 14-bit per-voice VCF code after ENV, Key Follow and clamp. */
uint16_t vcf106_voice_code14(const VcfControlParams106 *params,
                              int32_t global_raw,
                              uint16_t envelope14,
                              uint16_t portamento_pitch_8_8);

/* Physical 12-bit code committed to per-voice VCF sample-and-hold. */
uint16_t vcf106_voice_dac12(const VcfControlParams106 *params,
                             int32_t global_raw,
                             uint16_t envelope14,
                             uint16_t portamento_pitch_8_8);

#endif
