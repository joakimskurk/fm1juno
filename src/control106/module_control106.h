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
} ModuleVoiceControl106;

typedef enum ModuleTraceType106 {
    MODULE_TRACE106_CONTROL_EVENT = 0,
    MODULE_TRACE106_VOICE_ON,
    MODULE_TRACE106_VOICE_OFF,
    MODULE_TRACE106_SUSTAIN,
    MODULE_TRACE106_TIMER_RESET_LOAD,
    MODULE_TRACE106_TIMER_QUEUE_COUNT,
    MODULE_TRACE106_ENVELOPE_TICK,
    MODULE_TRACE106_LFO_DELAY_RESTART
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
    uint8_t note_gate_bits;
    uint8_t voice_run_bits;
    bool sustain_on;
    ModuleTrace106 trace;
} ModuleControl106;

void module_control106_init(ModuleControl106 *m);
void module_control106_clear_trace(ModuleControl106 *m);

void module_control106_set_envelope_params(ModuleControl106 *m,
                                           uint8_t voice,
                                           EnvelopeParams106 params);
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
