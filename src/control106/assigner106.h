#ifndef FM1JUNO_ASSIGNER106_H
#define FM1JUNO_ASSIGNER106_H

#include <stdbool.h>
#include <stdint.h>

#define ASSIGNER106_VOICES 6u

typedef enum AssignerMode106 {
    ASSIGNER106_POLY1 = 0,
    ASSIGNER106_POLY2,
    ASSIGNER106_SOLO_UNISON
} AssignerMode106;

typedef enum AssignerEventType106 {
    ASSIGNER106_EVENT_NONE = 0,
    ASSIGNER106_EVENT_VOICE_ON,
    ASSIGNER106_EVENT_VOICE_OFF
} AssignerEventType106;

typedef struct AssignerEvent106 {
    AssignerEventType106 type;
    uint8_t voice;
    uint8_t note;
} AssignerEvent106;

typedef struct AssignerEventBatch106 {
    uint8_t count;
    AssignerEvent106 events[ASSIGNER106_VOICES];
} AssignerEventBatch106;

typedef struct AssignerSlot106 {
    uint8_t note;
    bool free;
} AssignerSlot106;

typedef enum SoloArbitration106 {
    SOLO106_NONE = 0,
    SOLO106_ON_HANDLED,
    SOLO106_OFF_HANDLED
} SoloArbitration106;

typedef struct Assigner106 {
    AssignerMode106 mode;
    AssignerSlot106 slots[ASSIGNER106_VOICES];
    uint8_t poly1_order[ASSIGNER106_VOICES];
    uint8_t poly1_active_count;
    SoloArbitration106 solo_arbitration;
} Assigner106;

typedef struct ModuleVoiceOnDecision106 {
    bool request_attack;
    bool request_timer_reset;
} ModuleVoiceOnDecision106;

void assigner106_init(Assigner106 *a, AssignerMode106 mode);
AssignerEventBatch106 assigner106_note_on(Assigner106 *a, uint8_t note);
AssignerEventBatch106 assigner106_note_off(Assigner106 *a, uint8_t note);

void assigner106_begin_scan(Assigner106 *a);
AssignerEventBatch106 assigner106_solo_changed_note(Assigner106 *a,
                                                     uint8_t note,
                                                     bool is_on);
bool assigner106_end_scan(Assigner106 *a);

ModuleVoiceOnDecision106 module106_voice_on_decision(bool previous_note_valid,
                                                      uint8_t previous_note,
                                                      bool voice_running,
                                                      uint8_t incoming_note);
uint8_t module106_update_voice_run(uint8_t note_gate_bits,
                                   uint8_t previous_voice_run_bits,
                                   bool sustain_on);

#endif
