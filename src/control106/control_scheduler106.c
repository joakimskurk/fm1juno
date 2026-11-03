#include "control106/control_scheduler106.h"

/* Macro order follows the Phase-0 ROM schedule. DCO and ENV/VCA/VCF entries
 * expand into six physical-voice events each. */
static const ControlEventType106 k_macro_order[] = {
    CONTROL106_FRAME_START,
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
    CONTROL106_FINAL_VCA_VOICE6,
    CONTROL106_LFO_ADVANCE,
    CONTROL106_NOISE_LEVEL
};

#define CONTROL106_MACRO_COUNT \
    ((uint8_t)(sizeof k_macro_order / sizeof k_macro_order[0]))

void control_scheduler106_init(ControlScheduler106 *s)
{
    s->macro_index = 0u;
    s->voice_index = 0u;
    s->pass_id = 0u;
    s->abandoned_passes = 0u;
    s->parameter_interrupts = 0u;
    s->adc_interrupts = 0u;
    s->sequence = 0u;
}

static bool is_voice_event(ControlEventType106 type)
{
    return type == CONTROL106_DCO_VOICE ||
           type == CONTROL106_ENV_VCA_VCF_VOICE;
}

ControlEvent106 control_scheduler106_next(ControlScheduler106 *s)
{
    const ControlEventType106 type = k_macro_order[s->macro_index];
    ControlEvent106 event;
    event.type = type;
    event.voice = is_voice_event(type) ? s->voice_index : CONTROL106_NO_VOICE;
    event.pass_id = s->pass_id;
    event.sequence = s->sequence++;

    if (is_voice_event(type) && s->voice_index < 5u) {
        ++s->voice_index;
        return event;
    }

    s->voice_index = 0u;
    ++s->macro_index;
    if (s->macro_index >= CONTROL106_MACRO_COUNT) {
        s->macro_index = 0u;
        ++s->pass_id;
    }
    return event;
}

void control_scheduler106_voice_event_restart(ControlScheduler106 *s)
{
    ++s->abandoned_passes;
    ++s->pass_id;
    s->macro_index = 0u;
    s->voice_index = 0u;
}

void control_scheduler106_parameter_interrupt(ControlScheduler106 *s)
{
    ++s->parameter_interrupts;
}

void control_scheduler106_adc_interrupt(ControlScheduler106 *s)
{
    ++s->adc_interrupts;
}
