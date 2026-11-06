#include "control106/assigner106.h"

static AssignerEventBatch106 empty_batch(void)
{
    AssignerEventBatch106 batch = {0};
    return batch;
}

static AssignerEventBatch106 single_event(AssignerEventType106 type,
                                           uint8_t voice,
                                           uint8_t note)
{
    AssignerEventBatch106 batch = {0};
    batch.count = 1u;
    batch.events[0].type = type;
    batch.events[0].voice = voice;
    batch.events[0].note = note;
    return batch;
}

static AssignerEventBatch106 six_events(AssignerEventType106 type,
                                         uint8_t note)
{
    AssignerEventBatch106 batch = {0};
    batch.count = ASSIGNER106_VOICES;
    for (uint8_t v = 0u; v < ASSIGNER106_VOICES; ++v) {
        batch.events[v].type = type;
        batch.events[v].voice = v;
        batch.events[v].note = note;
    }
    return batch;
}

void assigner106_init(Assigner106 *a, AssignerMode106 mode)
{
    a->mode = mode;
    a->poly1_active_count = 0u;
    a->solo_arbitration = SOLO106_NONE;
    for (uint8_t v = 0u; v < ASSIGNER106_VOICES; ++v) {
        a->slots[v].note = 0u;
        a->slots[v].free = true;
        a->poly1_order[v] = v;
    }
}

static void poly1_move_to_boundary(Assigner106 *a, uint8_t order_index)
{
    const uint8_t voice = a->poly1_order[order_index];
    for (uint8_t i = order_index; i > a->poly1_active_count; --i)
        a->poly1_order[i] = a->poly1_order[i - 1u];
    a->poly1_order[a->poly1_active_count] = voice;
}

static void poly1_move_released_to_back(Assigner106 *a, uint8_t order_index)
{
    const uint8_t voice = a->poly1_order[order_index];
    for (uint8_t i = order_index; i + 1u < ASSIGNER106_VOICES; ++i)
        a->poly1_order[i] = a->poly1_order[i + 1u];
    a->poly1_order[ASSIGNER106_VOICES - 1u] = voice;
}

static AssignerEventBatch106 poly1_note_on(Assigner106 *a, uint8_t note)
{
    if (a->poly1_active_count >= ASSIGNER106_VOICES)
        return empty_batch();

    uint8_t chosen_index = a->poly1_active_count;
    for (uint8_t i = a->poly1_active_count; i < ASSIGNER106_VOICES; ++i) {
        const uint8_t v = a->poly1_order[i];
        if (a->slots[v].free && a->slots[v].note == note) {
            chosen_index = i;
            break;
        }
    }

    if (chosen_index != a->poly1_active_count)
        poly1_move_to_boundary(a, chosen_index);

    const uint8_t voice = a->poly1_order[a->poly1_active_count];
    a->slots[voice].note = note;
    a->slots[voice].free = false;
    ++a->poly1_active_count;
    return single_event(ASSIGNER106_EVENT_VOICE_ON, voice, note);
}

static AssignerEventBatch106 poly2_note_on(Assigner106 *a, uint8_t note)
{
    for (uint8_t v = 0u; v < ASSIGNER106_VOICES; ++v) {
        if (a->slots[v].free) {
            a->slots[v].note = note;
            a->slots[v].free = false;
            return single_event(ASSIGNER106_EVENT_VOICE_ON, v, note);
        }
    }
    return empty_batch();
}

AssignerEventBatch106 assigner106_note_on(Assigner106 *a, uint8_t note)
{
    note &= UINT8_C(0x7f);
    if (a->mode == ASSIGNER106_POLY1) return poly1_note_on(a, note);
    if (a->mode == ASSIGNER106_POLY2) return poly2_note_on(a, note);
    return six_events(ASSIGNER106_EVENT_VOICE_ON, note);
}

static int find_active_voice_for_note(const Assigner106 *a, uint8_t note)
{
    for (uint8_t v = 0u; v < ASSIGNER106_VOICES; ++v) {
        if (!a->slots[v].free && a->slots[v].note == note)
            return (int)v;
    }
    return -1;
}

AssignerEventBatch106 assigner106_note_off(Assigner106 *a, uint8_t note)
{
    note &= UINT8_C(0x7f);
    if (a->mode == ASSIGNER106_SOLO_UNISON)
        return six_events(ASSIGNER106_EVENT_VOICE_OFF, note);

    const int found = find_active_voice_for_note(a, note);
    if (found < 0) return empty_batch();
    const uint8_t voice = (uint8_t)found;

    if (a->mode == ASSIGNER106_POLY1) {
        uint8_t order_index = 0u;
        while (order_index < a->poly1_active_count &&
               a->poly1_order[order_index] != voice)
            ++order_index;
        if (order_index < a->poly1_active_count) {
            poly1_move_released_to_back(a, order_index);
            --a->poly1_active_count;
        }
    }

    a->slots[voice].free = true;
    return single_event(ASSIGNER106_EVENT_VOICE_OFF, voice, note);
}

void assigner106_begin_scan(Assigner106 *a)
{
    a->solo_arbitration = SOLO106_NONE;
}

AssignerEventBatch106 assigner106_solo_changed_note(Assigner106 *a,
                                                     uint8_t note,
                                                     bool is_on)
{
    if (a->mode != ASSIGNER106_SOLO_UNISON ||
        a->solo_arbitration != SOLO106_NONE)
        return empty_batch();

    note &= UINT8_C(0x7f);
    if (is_on) {
        a->solo_arbitration = SOLO106_ON_HANDLED;
        return six_events(ASSIGNER106_EVENT_VOICE_ON, note);
    }

    a->solo_arbitration = SOLO106_OFF_HANDLED;
    return six_events(ASSIGNER106_EVENT_VOICE_OFF, note);
}

bool assigner106_end_scan(Assigner106 *a)
{
    return a->mode == ASSIGNER106_SOLO_UNISON &&
           a->solo_arbitration == SOLO106_OFF_HANDLED;
}

ModuleVoiceOnDecision106 module106_voice_on_decision(bool previous_note_valid,
                                                      uint8_t previous_note,
                                                      bool voice_running,
                                                      uint8_t incoming_note)
{
    ModuleVoiceOnDecision106 d;
    d.request_attack = true;
    d.request_timer_reset = !voice_running &&
        (!previous_note_valid || previous_note != incoming_note);
    return d;
}

uint8_t module106_update_voice_run(uint8_t note_gate_bits,
                                   uint8_t previous_voice_run_bits,
                                   bool sustain_on)
{
    const uint8_t six = UINT8_C(0x3f);
    note_gate_bits &= six;
    previous_voice_run_bits &= six;
    return sustain_on
        ? (uint8_t)(note_gate_bits | previous_voice_run_bits)
        : note_gate_bits;
}
