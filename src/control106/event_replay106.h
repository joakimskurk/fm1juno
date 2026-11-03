#ifndef FM1JUNO_EVENT_REPLAY106_H
#define FM1JUNO_EVENT_REPLAY106_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/fixed_time.h"
#include "control106/module_control106.h"

#define EVENT_SCRIPT106_VERSION 1u

typedef enum ScriptEventType106 {
    SCRIPT106_VOICE_ON = 0,
    SCRIPT106_VOICE_OFF,
    SCRIPT106_SUSTAIN,
    SCRIPT106_DCO_COMMAND,
    SCRIPT106_CONTROL_STEP
} ScriptEventType106;

typedef struct ScriptEvent106 {
    Fm1junoTimeQ32 time;
    ScriptEventType106 type;
    uint8_t voice;
    uint32_t value;
    uint32_t aux;
} ScriptEvent106;

typedef struct ReplayResult106 {
    uint64_t trace_signature;
    size_t trace_count;
    bool trace_overflowed;
    uint8_t note_gate_bits;
    uint8_t voice_run_bits;
    uint32_t pass_id;
    uint32_t abandoned_passes;
} ReplayResult106;

bool event_replay106_validate(const ScriptEvent106 *events, size_t count);
bool event_replay106_run(const ScriptEvent106 *events,
                         size_t count,
                         uint32_t total_samples,
                         uint32_t outer_frames,
                         ReplayResult106 *result);

#endif
