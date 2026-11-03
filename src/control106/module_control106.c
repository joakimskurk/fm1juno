#include "control106/module_control106.h"

#include <string.h>

#include "control106/control_math106.h"
#include "control106/portamento106.h"

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
    raw_params106_init(&m->raw_params);
    lfo106_init(&m->lfo, m->raw_params.lfo_rate7);
    performance_lfo106_init(&m->performance_lfo);
    m->requested_global_pitch_offset_8_8 = PITCH106_GLOBAL_BASE;
    m->active_global_pitch_offset_8_8 = PITCH106_GLOBAL_BASE;

    for (uint8_t v = 0u; v < MODULE106_VOICES; ++v) {
        ModuleVoiceControl106 *voice = &m->voices[v];
        envelope106_init(&voice->envelope);
        voice->portamento_pitch_8_8 = (uint16_t)(60u << 8u);
        voice->dco_command.count = 1u;
        voice->dco_command.range = TIMER106_RANGE_8;
    }
}

bool module_control106_receive_parameter(ModuleControl106 *m,
                                          RawParameterId106 id,
                                          uint32_t value,
                                          Fm1junoTimeQ32 now)
{
    if (!raw_params106_set(&m->raw_params, id, value)) return false;
    m->raw_params_active = true;
    control_scheduler106_parameter_interrupt(&m->scheduler);
    trace_push(m, MODULE_TRACE106_PARAMETER_RECEIVED, now,
               m->scheduler.pass_id, m->scheduler.sequence,
               MODULE106_NO_VOICE, ((uint32_t)id << 8u) | value);
    return true;
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

void module_control106_set_pitch_timer_profile(ModuleControl106 *m,
                                                const PitchTimerBase106 *profile)
{
    if (profile == NULL) {
        m->pitch_timer_profile_enabled = false;
        return;
    }
    m->pitch_timer_profile = *profile;
    m->pitch_timer_profile_enabled = true;
}

void module_control106_set_global_pitch_offset(ModuleControl106 *m,
                                                uint16_t global_offset_8_8)
{
    m->requested_global_pitch_offset_8_8 = global_offset_8_8;
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

/* The mux bank and channel are specified by secondary106_route().
 * The trace's 'voice' field is a global secondary-lane ID, not a
 * physical voice number, for MODULE_TRACE106_GLOBAL_S_H events. */
static void service_global_hold(ModuleControl106 *m,
                                SecondaryLane106 lane,
                                uint16_t code14,
                                Fm1junoTimeQ32 now,
                                const ControlEvent106 *event)
{
    SecondaryHold106 *hold = &m->secondary_outputs.global[lane];
    secondary106_commit(hold, code14, now, event->pass_id);
    trace_push(m, MODULE_TRACE106_GLOBAL_S_H, now,
               event->pass_id, event->sequence,
               (uint8_t)lane, hold->dac12);
}

static void service_voice_vca_hold(ModuleControl106 *m,
                                   uint8_t voice_index,
                                   Fm1junoTimeQ32 now,
                                   const ControlEvent106 *event)
{
    const ModuleVoiceControl106 *voice = &m->voices[voice_index];
    const bool running = voice_is_running(m, voice_index);
    const uint16_t code = secondary106_voice_vca_code14(
        voice->envelope.level, m->raw_params.vca_gate_mode, running);
    SecondaryHold106 *hold =
        &m->secondary_outputs.voice_vca[voice_index];
    secondary106_commit(hold, code, now, event->pass_id);
    trace_push(m, MODULE_TRACE106_VOICE_VCA_S_H, now,
               event->pass_id, event->sequence,
               voice_index, hold->dac12);
}

static void service_raw_lfo_depth(ModuleControl106 *m)
{
    if (m->voice_run_bits != 0u)
        m->lfo_onset = lfo_delay106_tick_raw(
            &m->lfo_delay, m->raw_params.lfo_delay7);
    else
        m->lfo_onset = 0u;

    const uint8_t panel = lfo106_depth_scaler(
        m->raw_params.dco_lfo_depth7);
    m->combined_dco_lfo_depth = lfo_delay106_combine_depth(
        panel, m->lfo_onset, m->performance_depth);
}

static void service_raw_pitch(ModuleControl106 *m,
                               Fm1junoTimeQ32 now,
                               const ControlEvent106 *event)
{
    const uint16_t lfo_mag = (uint16_t)(
        control106_mul16x8_hi(m->lfo.magnitude,
                             m->combined_dco_lfo_depth) >> 3u);
    const int16_t signed_lfo = (m->lfo.phase_flags & LFO106_POLARITY_BIT)
        ? -(int16_t)lfo_mag : (int16_t)lfo_mag;

    const uint16_t bend = control106_dco_bend_8_8(
        m->raw_params.bend_magnitude8,
        m->raw_params.dco_bend_sensitivity8);
    const int16_t signed_bend = m->raw_params.bend_negative
        ? -(int16_t)bend : (int16_t)bend;

    const uint16_t offset = pitch106_global_offset(
        control106_tune_8_8(m->raw_params.tune_adc8),
        signed_lfo, signed_bend);
    m->requested_global_pitch_offset_8_8 = offset;
    m->active_global_pitch_offset_8_8 = offset;

    const uint8_t step = portamento106_step_from_raw(
        m->raw_params.portamento_adc8);
    for (uint8_t i = 0u; i < MODULE106_VOICES; ++i) {
        ModuleVoiceControl106 *v = &m->voices[i];
        if (!v->previous_note_valid) continue;

        const uint16_t target = (uint16_t)((uint16_t)v->previous_note << 8u);
        v->portamento_pitch_8_8 = portamento106_step_toward(
            v->portamento_pitch_8_8, target, step);
        trace_push(m, MODULE_TRACE106_PORTAMENTO_PITCH, now,
                   event->pass_id, event->sequence, i,
                   v->portamento_pitch_8_8);
    }
}

static void service_raw_vcf_global(ModuleControl106 *m)
{
    VcfControlParams106 *p = &m->vcf_params;
    p->cutoff7 = m->raw_params.vcf_cutoff7;
    p->lfo_depth7 = m->raw_params.vcf_lfo_depth7;
    p->lfo_onset = m->lfo_onset;
    p->lfo_magnitude = m->lfo.magnitude;
    p->lfo_negative = (m->lfo.phase_flags & LFO106_POLARITY_BIT) != 0u;
    p->bend_magnitude = m->raw_params.bend_magnitude8;
    p->bend_sensitivity = m->raw_params.vcf_bend_sensitivity8;
    p->bend_negative = m->raw_params.bend_negative;
    p->env_depth7 = m->raw_params.vcf_env_depth7;
    p->env_inverted = m->raw_params.vcf_env_inverted;
    p->key_follow7 = m->raw_params.vcf_key_follow7;
    m->vcf_global_raw = vcf106_global_raw(p);
}

static void service_raw_env_params(ModuleControl106 *m,
                                    ModuleVoiceControl106 *voice)
{
    const RawParams106 *raw = &m->raw_params;
    voice->envelope_params.attack_increment =
        envelope106_attack_increment(raw->env_attack7);
    voice->envelope_params.decay_coefficient =
        envelope106_decay_release_coefficient(raw->env_decay7);
    voice->envelope_params.sustain_level =
        envelope106_sustain_level_from_raw(raw->env_sustain7);
    voice->envelope_params.release_coefficient =
        envelope106_decay_release_coefficient(raw->env_release7);

    /* ENV depth and Key Follow are read in the per-voice region rather
     * than frozen with the earlier global Cutoff/LFO/Bend calculation. */
    m->vcf_params.env_depth7 = raw->vcf_env_depth7;
    m->vcf_params.env_inverted = raw->vcf_env_inverted;
    m->vcf_params.key_follow7 = raw->vcf_key_follow7;
}

static void service_dco_voice(ModuleControl106 *m,
                              uint8_t voice_index,
                              Fm1junoTimeQ32 now,
                              const ControlEvent106 *event)
{
    ModuleVoiceControl106 *voice = &m->voices[voice_index];
    ModuleDcoCommand106 command = voice->dco_command;

    if (m->pitch_timer_profile_enabled) {
        /* Both the divisor and the held DCO-CV derive from one final pitch,
         * after Portamento and the earlier global Tune/LFO/Bender latch. */
        voice->final_dco_pitch_8_8 = pitch106_final(
            voice->portamento_pitch_8_8,
            m->active_global_pitch_offset_8_8);
        command.count = pitch106_timer_count_from_pitch(
            &m->pitch_timer_profile, voice->final_dco_pitch_8_8);
    }

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
    } else {
        timer8253_106_request_count(&voice->timer, command.count);
        trace_push(m,
                   MODULE_TRACE106_TIMER_QUEUE_COUNT,
                   now,
                   event->pass_id,
                   event->sequence,
                   voice_index,
                   command.count);
    }

    /* The ROM commits DCO-CV only after this voice's 8253 write.
     * An injected profile is replaceable and does not imply strict count
     * exactness. The CV value is already in physical 12-bit units. */
    if (m->pitch_timer_profile_enabled) {
        voice->dco_cv_dac12 = pitch106_dco_cv12_from_pitch(
            voice->final_dco_pitch_8_8);
        trace_push(m,
                   MODULE_TRACE106_DCO_CV_S_H,
                   now,
                   event->pass_id,
                   event->sequence,
                   voice_index,
                   voice->dco_cv_dac12);
    }
}

static void service_envelope_voice(ModuleControl106 *m,
                                   uint8_t voice_index,
                                   Fm1junoTimeQ32 now,
                                   const ControlEvent106 *event)
{
    ModuleVoiceControl106 *voice = &m->voices[voice_index];
    if (m->raw_params_active) service_raw_env_params(m, voice);
    const uint16_t level = envelope106_tick(&voice->envelope,
                                            &voice->envelope_params);
    trace_push(m,
               MODULE_TRACE106_ENVELOPE_TICK,
               now,
               event->pass_id,
               event->sequence,
               voice_index,
               level);

    if (m->raw_params_active) {
        if (voice_index == 0u) {
            /* ROM 0x0599: first voice sends PWM register from the
             * previous LFO_ADVANCE at 0x0761..0x0788. */
            service_global_hold(m, SECONDARY106_PWM,
                                m->secondary_outputs.pwm_next_code14,
                                now, event);
        } else {
            /* ROM 0x05A6..0x05B2: voice k service writes the
             * *previous* voice k-1 VCA; voice 6 is done after ADC. */
            service_voice_vca_hold(m, (uint8_t)(voice_index - 1u),
                                   now, event);
        }
    }

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
    case CONTROL106_LFO_DELAY_AND_MOD_SCALE:
        if (m->raw_params_active) service_raw_lfo_depth(m);
        break;
    case CONTROL106_ADC_PERFORMANCE:
        if (m->raw_params_active) {
            m->performance_depth = performance_lfo106_tick(
                &m->performance_lfo,
                m->raw_params.performance_trigger,
                m->raw_params.performance_sensitivity8,
                m->raw_params.midi_mod_depth8);
        }
        break;
    case CONTROL106_LFO_ADVANCE:
        if (m->raw_params_active) {
            m->lfo.rate_coefficient = lfo106_rate_coefficient(
                m->raw_params.lfo_rate7);
            const int16_t signed_lfo = lfo106_tick(&m->lfo);
            m->secondary_outputs.pwm_next_code14 =
                secondary106_pwm_code14(
                    m->raw_params.pwm_depth7,
                    m->raw_params.pwm_manual,
                    m->raw_params.pulse_enabled,
                    m->lfo.magnitude,
                    (m->lfo.phase_flags & LFO106_POLARITY_BIT) != 0u);
            trace_push(m, MODULE_TRACE106_PWM_PREPARE, now,
                       event.pass_id, event.sequence,
                       (uint8_t)SECONDARY106_PWM,
                       m->secondary_outputs.pwm_next_code14);
            trace_push(m, MODULE_TRACE106_LFO_TICK, now,
                       event.pass_id, event.sequence, MODULE106_NO_VOICE,
                       (uint32_t)(uint16_t)signed_lfo);
        }
        break;
    case CONTROL106_VCF_RESONANCE:
        if (m->raw_params_active)
            service_global_hold(m, SECONDARY106_RESONANCE,
                secondary106_patch_code14(m->raw_params.vcf_resonance7),
                now, &event);
        break;
    case CONTROL106_GLOBAL_VCA_LEVEL:
        if (m->raw_params_active)
            service_global_hold(m, SECONDARY106_GLOBAL_VCA,
                secondary106_patch_code14(m->raw_params.global_vca_level7),
                now, &event);
        break;
    case CONTROL106_SUB_LEVEL:
        if (m->raw_params_active)
            service_global_hold(m, SECONDARY106_SUB_LEVEL,
                secondary106_patch_code14(m->raw_params.sub_level7),
                now, &event);
        break;
    case CONTROL106_NOISE_LEVEL:
        if (m->raw_params_active)
            service_global_hold(m, SECONDARY106_NOISE,
                secondary106_patch_code14(m->raw_params.noise_level7),
                now, &event);
        break;
    case CONTROL106_FINAL_VCA_VOICE6:
        if (m->raw_params_active)
            service_voice_vca_hold(m, 5u, now, &event);
        break;
    case CONTROL106_DCO_VOICE:
        service_dco_voice(m, event.voice, now, &event);
        break;
    case CONTROL106_PITCH_AND_PORTAMENTO:
        if (m->raw_params_active)
            service_raw_pitch(m, now, &event);
        else
            m->active_global_pitch_offset_8_8 =
                m->requested_global_pitch_offset_8_8;
        break;
    case CONTROL106_VCF_GLOBAL_BASE:
        if (m->raw_params_active)
            service_raw_vcf_global(m);
        else
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
