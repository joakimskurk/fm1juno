#include "control106/module_control106.h"

#include <string.h>

#define MODULE106_NO_VOICE UINT8_C(0xff)

static void trace_push(ModuleControl106 *m,
                       ModuleTraceType106 type,
                       Fm1junoTimeQ32 time,
                       uint32_t pass_id,
                       uint64_t sequence,
                       uint8_t voice,
                       uint32_t value)
{
    ModuleTrace106 *trace = &m->trace;
    if (trace->count >= MODULE106_TRACE_CAPACITY) {
        trace->overflowed = true;
        return;
    }

    ModuleTraceEvent106 *e = &trace->events[trace->count++];
    e->type = type;
    e->time = time;
    e->pass_id = pass_id;
    e->sequence = sequence;
    e->voice = voice;
    e->value = value;
}

void module_control106_init(ModuleControl106 *m)
{
    memset(m, 0, sizeof *m);
    control_scheduler106_init(&m->scheduler);
    lfo_delay106_init(&m->lfo_delay);

    for (uint8_t v = 0u; v < MODULE106_VOICES; ++v) {
        ModuleVoiceControl106 *voice = &m->voices[v];
        envelope106_init(&voice->envelope);
        voice->portamento_pitch_8_8 = (uint16_t)(60u << 8u);
        voice->dco_command.count = 1u;
        voice->dco_command.range = TIMER106_RANGE_8;
    }
}

void module_control106_clear_trace(ModuleControl106 *m)
{
    m->trace.count = 0u;
    m->trace.overflowed = false;
}

void module_control106_set_envelope_params(ModuleControl106 *m,
                                           uint8_t voice,
                                           EnvelopeParams106 params)
{
    if (voice >= MODULE106_VOICES) return;
    m->voices[voice].envelope_params = params;
}

void module_control106_set_dco_command(ModuleControl106 *m,
                                       uint8_t voice,
                                       uint16_t count,
                                       TimerRange106 range)
{
    if (voice >= MODULE106_VOICES) return;
    m->voices[voice].dco_command.count = count;
    m->voices[voice].dco_command.range = range;
}

void module_control106_set_vcf_params(ModuleControl106 *m,
                                       VcfControlParams106 params)
{
    m->vcf_params = params;
}

void module_control106_set_voice_pitch(ModuleControl106 *m,
                                       uint8_t voice,
                                       uint16_t pitch_8_8)
{
    if (voice >= MODULE106_VOICES) return;
    m->voices[voice].portamento_pitch_8_8 = pitch_8_8;
}

static bool voice_is_running(const ModuleControl106 *m, uint8_t voice)
{
    return (m->voice_run_bits & (uint8_t)(1u << voice)) != 0u;
}

static void apply_voice_on(ModuleControl106 *m,
                           uint8_t voice_index,
                           uint8_t note,
                           Fm1junoTimeQ32 now)
{
    ModuleVoiceControl106 *voice = &m->voices[voice_index];
    const ModuleVoiceOnDecision106 decision = module106_voice_on_decision(
        voice->previous_note_valid,
        voice->previous_note,
        voice_is_running(m, voice_index),
        note);

    m->note_gate_bits |= (uint8_t)(1u << voice_index);
    if (decision.request_attack)
        envelope106_note_on(&voice->envelope);
    if (decision.request_timer_reset)
        voice->timer_reset_pending = true;

    voice->previous_note = note;
    voice->previous_note_valid = true;

    trace_push(m,
               MODULE_TRACE106_VOICE_ON,
               now,
               m->scheduler.pass_id,
               m->scheduler.sequence,
               voice_index,
               ((uint32_t)note << 1u) | (decision.request_timer_reset ? 1u : 0u));

    control_scheduler106_voice_event_restart(&m->scheduler);
}

static void apply_voice_off(ModuleControl106 *m,
                            uint8_t voice_index,
                            uint8_t note,
                            Fm1junoTimeQ32 now)
{
    m->note_gate_bits &= (uint8_t)~(uint8_t)(1u << voice_index);
    if (!m->sustain_on)
        envelope106_note_off(&m->voices[voice_index].envelope);

    trace_push(m,
               MODULE_TRACE106_VOICE_OFF,
               now,
               m->scheduler.pass_id,
               m->scheduler.sequence,
               voice_index,
               note);

    control_scheduler106_voice_event_restart(&m->scheduler);
}

void module_control106_apply_assigner_event(ModuleControl106 *m,
                                            AssignerEvent106 event,
                                            Fm1junoTimeQ32 now)
{
    if (event.voice >= MODULE106_VOICES) return;

    switch (event.type) {
    case ASSIGNER106_EVENT_VOICE_ON:
        apply_voice_on(m, event.voice, event.note, now);
        break;
    case ASSIGNER106_EVENT_VOICE_OFF:
        apply_voice_off(m, event.voice, event.note, now);
        break;
    case ASSIGNER106_EVENT_NONE:
    default:
        break;
    }
}

void module_control106_apply_assigner_batch(ModuleControl106 *m,
                                            const AssignerEventBatch106 *batch,
                                            Fm1junoTimeQ32 first_event_time,
                                            Fm1junoTimeQ32 event_spacing)
{
    Fm1junoTimeQ32 now = first_event_time;
    for (uint8_t i = 0u; i < batch->count; ++i) {
        module_control106_apply_assigner_event(m, batch->events[i], now);
        now += event_spacing;
    }
}

void module_control106_set_sustain(ModuleControl106 *m,
                                   bool sustain_on,
                                   Fm1junoTimeQ32 now)
{
    const bool was_on = m->sustain_on;
    m->sustain_on = sustain_on;

    if (was_on && !sustain_on) {
        for (uint8_t v = 0u; v < MODULE106_VOICES; ++v) {
            if ((m->note_gate_bits & (uint8_t)(1u << v)) == 0u)
                envelope106_note_off(&m->voices[v].envelope);
        }
    }

    control_scheduler106_parameter_interrupt(&m->scheduler);
    trace_push(m,
               MODULE_TRACE106_SUSTAIN,
               now,
               m->scheduler.pass_id,
               m->scheduler.sequence,
               MODULE106_NO_VOICE,
               sustain_on ? 1u : 0u);
}

static void service_frame_start(ModuleControl106 *m,
                                Fm1junoTimeQ32 now,
                                const ControlEvent106 *event)
{
    const uint8_t previous = m->voice_run_bits;
    m->voice_run_bits = module106_update_voice_run(m->note_gate_bits,
                                                   previous,
                                                   m->sustain_on);
    if (lfo_delay106_observe_voice_run(&m->lfo_delay, m->voice_run_bits)) {
        trace_push(m,
                   MODULE_TRACE106_LFO_DELAY_RESTART,
                   now,
                   event->pass_id,
                   event->sequence,
                   MODULE106_NO_VOICE,
                   m->voice_run_bits);
    }
}

static void service_dco_voice(ModuleControl106 *m,
                              uint8_t voice_index,
                              Fm1junoTimeQ32 now,
                              const ControlEvent106 *event)
{
    ModuleVoiceControl106 *voice = &m->voices[voice_index];
    const ModuleDcoCommand106 command = voice->dco_command;

    if (!voice->timer_initialized || voice->timer_reset_pending) {
        timer8253_106_reset_and_load_at(&voice->timer,
                                        command.count,
                                        command.range,
                                        now);
        voice->timer_initialized = true;
        voice->timer_reset_pending = false;
        trace_push(m,
                   MODULE_TRACE106_TIMER_RESET_LOAD,
                   now,
                   event->pass_id,
                   event->sequence,
                   voice_index,
                   command.count);
        return;
    }

    timer8253_106_request_count(&voice->timer, command.count);
    trace_push(m,
               MODULE_TRACE106_TIMER_QUEUE_COUNT,
               now,
               event->pass_id,
               event->sequence,
               voice_index,
               command.count);
}

static void service_envelope_voice(ModuleControl106 *m,
                                   uint8_t voice_index,
                                   Fm1junoTimeQ32 now,
                                   const ControlEvent106 *event)
{
    ModuleVoiceControl106 *voice = &m->voices[voice_index];
    const uint16_t level = envelope106_tick(&voice->envelope,
                                            &voice->envelope_params);
    trace_push(m,
               MODULE_TRACE106_ENVELOPE_TICK,
               now,
               event->pass_id,
               event->sequence,
               voice_index,
               level);

    /* Firmware ENV has advanced for this voice before its held VCF CV.
     * Global base comes from the earlier VCF_GLOBAL_BASE slot. */
    voice->vcf_code14 = vcf106_voice_code14(&m->vcf_params,
                                            m->vcf_global_raw,
                                            level,
                                            voice->portamento_pitch_8_8);
    voice->vcf_dac12 = (uint16_t)(voice->vcf_code14 >> 2u);
    trace_push(m,
               MODULE_TRACE106_VCF_S_H,
               now,
               event->pass_id,
               event->sequence,
               voice_index,
               voice->vcf_dac12);
}

ControlEvent106 module_control106_service_next(ModuleControl106 *m,
                                                Fm1junoTimeQ32 now)
{
    const ControlEvent106 event = control_scheduler106_next(&m->scheduler);

    trace_push(m,
               MODULE_TRACE106_CONTROL_EVENT,
               now,
               event.pass_id,
               event.sequence,
               event.voice,
               (uint32_t)event.type);

    switch (event.type) {
    case CONTROL106_FRAME_START:
        service_frame_start(m, now, &event);
        break;
    case CONTROL106_DCO_VOICE:
        service_dco_voice(m, event.voice, now, &event);
        break;
    case CONTROL106_VCF_GLOBAL_BASE:
        m->vcf_global_raw = vcf106_global_raw(&m->vcf_params);
        break;
    case CONTROL106_ENV_VCA_VCF_VOICE:
        service_envelope_voice(m, event.voice, now, &event);
        break;
    default:
        break;
    }
    return event;
}

uint64_t module_control106_trace_signature(const ModuleTrace106 *trace)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    const uint64_t prime = UINT64_C(1099511628211);

    for (size_t i = 0u; i < trace->count; ++i) {
        const ModuleTraceEvent106 *e = &trace->events[i];
        const uint64_t words[] = {
            (uint64_t)e->type,
            e->time,
            (uint64_t)e->pass_id,
            e->sequence,
            (uint64_t)e->voice,
            (uint64_t)e->value
        };
        for (size_t w = 0u; w < sizeof words / sizeof words[0]; ++w) {
            uint64_t value = words[w];
            for (unsigned b = 0u; b < 8u; ++b) {
                hash ^= value & UINT64_C(0xff);
                hash *= prime;
                value >>= 8u;
            }
        }
    }
    hash ^= trace->overflowed ? UINT64_C(0xa5) : UINT64_C(0x5a);
    hash *= prime;
    return hash;
}
