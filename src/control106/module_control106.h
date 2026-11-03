#ifndef FM1JUNO_MODULE_CONTROL106_H
#define FM1JUNO_MODULE_CONTROL106_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/fixed_time.h"
#include "control106/assigner106.h"
#include "control106/control_scheduler106.h"
#include "control106/envelope106.h"
#include "control106/lfo_delay106.h"
#include "control106/pitch106.h"
#include "control106/raw_params106.h"
#include "control106/secondary_control106.h"
#include "control106/lfo106.h"
#include "control106/vcf_control106.h"
#include "timer106/timer8253_106.h"

#define MODULE106_VOICES 6u
#define MODULE106_TRACE_CAPACITY 256u

typedef struct ModuleDcoCommand106 {
    uint16_t count;
    TimerRange106 range;
} ModuleDcoCommand106;

typedef struct ModuleVoiceControl106 {
    bool previous_note_valid;
    uint8_t previous_note;
    bool timer_reset_pending;
    bool timer_initialized;
    Envelope106 envelope;
    EnvelopeParams106 envelope_params;
    Timer8253Voice106 timer;
    ModuleDcoCommand106 dco_command;
    uint16_t portamento_pitch_8_8;
    uint16_t vcf_code14;
    uint16_t vcf_dac12;
    uint16_t final_dco_pitch_8_8;
    uint16_t dco_cv_dac12;
} ModuleVoiceControl106;

typedef enum ModuleTraceType106 {
    MODULE_TRACE106_CONTROL_EVENT = 0,
    MODULE_TRACE106_VOICE_ON,
    MODULE_TRACE106_VOICE_OFF,
    MODULE_TRACE106_SUSTAIN,
    MODULE_TRACE106_TIMER_RESET_LOAD,
    MODULE_TRACE106_TIMER_QUEUE_COUNT,
    MODULE_TRACE106_ENVELOPE_TICK,
    MODULE_TRACE106_LFO_DELAY_RESTART,
    MODULE_TRACE106_VCF_S_H,
    MODULE_TRACE106_DCO_CV_S_H,
    MODULE_TRACE106_PARAMETER_RECEIVED,
    MODULE_TRACE106_LFO_TICK,
    MODULE_TRACE106_PORTAMENTO_PITCH,
    MODULE_TRACE106_GLOBAL_S_H,
    MODULE_TRACE106_VOICE_VCA_S_H,
    MODULE_TRACE106_PWM_PREPARE
} ModuleTraceType106;

typedef struct ModuleTraceEvent106 {
    ModuleTraceType106 type;
    Fm1junoTimeQ32 time;
    uint32_t pass_id;
    uint64_t sequence;
    uint8_t voice;
    uint32_t value;
} ModuleTraceEvent106;

typedef struct ModuleTrace106 {
    size_t count;
    bool overflowed;
    ModuleTraceEvent106 events[MODULE106_TRACE_CAPACITY];
} ModuleTrace106;

typedef struct ModuleControl106 {
    ControlScheduler106 scheduler;
    ModuleVoiceControl106 voices[MODULE106_VOICES];
    LfoDelay106 lfo_delay;
    Lfo106State lfo;
    PerformanceLfo106 performance_lfo;
    RawParams106 raw_params;
    SecondaryOutputs106 secondary_outputs;
    bool raw_params_active;
    uint8_t lfo_onset;
    uint8_t performance_depth;
    uint8_t combined_dco_lfo_depth;
    VcfControlParams106 vcf_params;
    int32_t vcf_global_raw;
    bool pitch_timer_profile_enabled;
    PitchTimerBase106 pitch_timer_profile;
    uint16_t requested_global_pitch_offset_8_8;
    uint16_t active_global_pitch_offset_8_8;
    uint8_t note_gate_bits;
    uint8_t voice_run_bits;
    bool sustain_on;
    ModuleTrace106 trace;
} ModuleControl106;

void module_control106_init(ModuleControl106 *m);
/* A timestamped raw parameter write updates the emulated firmware RAM;
 * the derived outputs change only at their own scheduler service slots.
 * Returns false without side effects on invalid id/value. */
bool module_control106_receive_parameter(ModuleControl106 *m,
                                          RawParameterId106 id,
                                          uint32_t value,
                                          Fm1junoTimeQ32 now);
void module_control106_clear_trace(ModuleControl106 *m);

void module_control106_set_envelope_params(ModuleControl106 *m,
                                           uint8_t voice,
                                           EnvelopeParams106 params);
/* Parameters are sampled at their ROM scheduler slots, not applied
 * atomically to all voices on the setter call. */
void module_control106_set_vcf_params(ModuleControl106 *m,
                                       VcfControlParams106 params);
/* External Portamento/pitch producer writes current voice state in 8.8.
 * This is a temporary P1 harness boundary until the pitch producer is wired. */
/* Set a caller-owned base-count profile. May be calibrated reference data
 * or a *clearly identified* derived approximation. Never silently assume
 * the provided profile is ROM-bit-identical. */
void module_control106_set_pitch_timer_profile(ModuleControl106 *m,
                                                const PitchTimerBase106 *profile);
/* Control-frame latch of the firmware 8.8 global Tune/LFO/Bend offset. */
void module_control106_set_global_pitch_offset(ModuleControl106 *m,
                                                uint16_t global_offset_8_8);

void module_control106_set_voice_pitch(ModuleControl106 *m,
                                       uint8_t voice,
                                       uint16_t pitch_8_8);

void module_control106_set_dco_command(ModuleControl106 *m,
                                       uint8_t voice,
                                       uint16_t count,
                                       TimerRange106 range);

void module_control106_apply_assigner_event(ModuleControl106 *m,
                                            AssignerEvent106 event,
                                            Fm1junoTimeQ32 now);
void module_control106_apply_assigner_batch(ModuleControl106 *m,
                                            const AssignerEventBatch106 *batch,
                                            Fm1junoTimeQ32 first_event_time,
                                            Fm1junoTimeQ32 event_spacing);
void module_control106_set_sustain(ModuleControl106 *m,
                                   bool sustain_on,
                                   Fm1junoTimeQ32 now);

ControlEvent106 module_control106_service_next(ModuleControl106 *m,
                                                Fm1junoTimeQ32 now);
uint64_t module_control106_trace_signature(const ModuleTrace106 *trace);

#endif
