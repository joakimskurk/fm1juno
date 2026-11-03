#ifndef FM1JUNO_CONTROL_SCHEDULER106_H
#define FM1JUNO_CONTROL_SCHEDULER106_H

#include <stdbool.h>
#include <stdint.h>

#define CONTROL106_NO_VOICE UINT8_C(0xff)

typedef enum ControlEventType106 {
    CONTROL106_FRAME_START = 0,
    CONTROL106_LATCH_AND_VOICERUN,
    CONTROL106_VCF_RESONANCE,
    CONTROL106_LFO_DELAY_AND_MOD_SCALE,
    CONTROL106_GLOBAL_VCA_LEVEL,
    CONTROL106_PITCH_AND_PORTAMENTO,
    CONTROL106_SUB_LEVEL,
    CONTROL106_DCO_VOICE,
    CONTROL106_VCF_GLOBAL_BASE,
    CONTROL106_ENV_VCA_VCF_VOICE,
    CONTROL106_ADC_PERFORMANCE,
    /* VCA voice 6 is written only after ADC processing; its VCF and
     * envelope were already serviced in the preceding six-voice loop. */
    CONTROL106_FINAL_VCA_VOICE6,
    CONTROL106_LFO_ADVANCE,
    CONTROL106_NOISE_LEVEL
} ControlEventType106;

typedef struct ControlEvent106 {
    ControlEventType106 type;
    uint8_t voice;
    uint32_t pass_id;
    uint64_t sequence;
} ControlEvent106;

typedef struct ControlScheduler106 {
    uint8_t macro_index;
    uint8_t voice_index;
    uint32_t pass_id;
    uint32_t abandoned_passes;
    uint32_t parameter_interrupts;
    uint32_t adc_interrupts;
    uint64_t sequence;
} ControlScheduler106;

void control_scheduler106_init(ControlScheduler106 *s);
ControlEvent106 control_scheduler106_next(ControlScheduler106 *s);
void control_scheduler106_voice_event_restart(ControlScheduler106 *s);
void control_scheduler106_parameter_interrupt(ControlScheduler106 *s);
void control_scheduler106_adc_interrupt(ControlScheduler106 *s);

#endif
