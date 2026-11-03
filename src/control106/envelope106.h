#ifndef FM1JUNO_ENVELOPE106_H
#define FM1JUNO_ENVELOPE106_H

#include <stdint.h>

#define ENVELOPE106_MAX UINT16_C(0x3fff)

typedef enum EnvelopePhase106 {
    ENVELOPE106_ATTACK = 0,
    ENVELOPE106_DECAY,
    ENVELOPE106_SUSTAIN,
    ENVELOPE106_RELEASE
} EnvelopePhase106;

typedef struct Envelope106 {
    uint16_t level;
    EnvelopePhase106 phase;
} Envelope106;

typedef struct EnvelopeParams106 {
    uint16_t attack_increment;
    uint16_t decay_coefficient;
    uint16_t sustain_level;
    uint16_t release_coefficient;
} EnvelopeParams106;

/* Exact compact clean-room form of the shared Decay/Release coefficient
 * behavior for raw panel index 0..127. */
uint16_t envelope106_decay_release_coefficient(uint8_t index);

uint16_t envelope106_calc_decay(uint16_t value, uint16_t coefficient);
uint16_t envelope106_attack_tick(uint16_t current, uint16_t increment);
uint16_t envelope106_decay_tick(uint16_t current,
                                uint16_t sustain,
                                uint16_t coefficient);
uint16_t envelope106_release_tick(uint16_t current, uint16_t coefficient);

void envelope106_init(Envelope106 *env);
void envelope106_note_on(Envelope106 *env);
void envelope106_note_off(Envelope106 *env);
uint16_t envelope106_tick(Envelope106 *env, const EnvelopeParams106 *params);

#endif
