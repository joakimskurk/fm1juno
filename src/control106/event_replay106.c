#include "control106/event_replay106.h"

static void apply_script_event(ModuleControl106 *m, const ScriptEvent106 *event)
{
    switch (event->type) {
    case SCRIPT106_VOICE_ON: {
        AssignerEvent106 e = {
            ASSIGNER106_EVENT_VOICE_ON,
            event->voice,
            (uint8_t)(event->value & UINT32_C(0x7f))
        };
        module_control106_apply_assigner_event(m, e, event->time);
        break;
    }
    case SCRIPT106_VOICE_OFF: {
        AssignerEvent106 e = {
            ASSIGNER106_EVENT_VOICE_OFF,
            event->voice,
            (uint8_t)(event->value & UINT32_C(0x7f))
        };
        module_control106_apply_assigner_event(m, e, event->time);
        break;
    }
    case SCRIPT106_SUSTAIN:
        module_control106_set_sustain(m, event->value != 0u, event->time);
        break;
    case SCRIPT106_DCO_COMMAND:
        module_control106_set_dco_command(
            m,
            event->voice,
            (uint16_t)event->value,
            (TimerRange106)event->aux);
        break;
    case SCRIPT106_CONTROL_STEP:
        (void)module_control106_service_next(m, event->time);
        break;
    default:
        break;
    }
}

bool event_replay106_validate(const ScriptEvent106 *events, size_t count)
{
    if (count != 0u && events == NULL) return false;

    Fm1junoTimeQ32 previous = 0u;
    for (size_t i = 0u; i < count; ++i) {
        const ScriptEvent106 *e = &events[i];
        if (i != 0u && e->time < previous) return false;
        if (e->type > SCRIPT106_CONTROL_STEP) return false;
        if ((e->type == SCRIPT106_VOICE_ON ||
             e->type == SCRIPT106_VOICE_OFF ||
             e->type == SCRIPT106_DCO_COMMAND) &&
            e->voice >= MODULE106_VOICES)
            return false;
        if (e->type == SCRIPT106_DCO_COMMAND && e->aux > TIMER106_RANGE_4)
            return false;
        previous = e->time;
    }
    return true;
}

bool event_replay106_run(const ScriptEvent106 *events,
                         size_t count,
                         uint32_t total_samples,
                         uint32_t outer_frames,
                         ReplayResult106 *result)
{
    if (result == NULL || outer_frames == 0u ||
        !event_replay106_validate(events, count))
        return false;

    ModuleControl106 module;
    module_control106_init(&module);

    const EnvelopeParams106 nominal_test_env = {
        UINT16_C(0x0800),
        UINT16_C(0xf000),
        UINT16_C(0x2000),
        UINT16_C(0xe000)
    };
    for (uint8_t v = 0u; v < MODULE106_VOICES; ++v)
        module_control106_set_envelope_params(&module, v, nominal_test_env);

    size_t next_event = 0u;
    uint32_t block_start = 0u;
    while (block_start < total_samples) {
        uint32_t block_end = block_start + outer_frames;
        if (block_end > total_samples || block_end < block_start)
            block_end = total_samples;
        const Fm1junoTimeQ32 block_end_time = fm1juno_time_from_samples(block_end);

        while (next_event < count && events[next_event].time < block_end_time) {
            apply_script_event(&module, &events[next_event]);
            ++next_event;
        }
        block_start = block_end;
    }

    const Fm1junoTimeQ32 end_time = fm1juno_time_from_samples(total_samples);
    while (next_event < count && events[next_event].time == end_time) {
        apply_script_event(&module, &events[next_event]);
        ++next_event;
    }
    if (next_event != count) return false;

    result->trace_signature = module_control106_trace_signature(&module.trace);
    result->trace_count = module.trace.count;
    result->trace_overflowed = module.trace.overflowed;
    result->note_gate_bits = module.note_gate_bits;
    result->voice_run_bits = module.voice_run_bits;
    result->pass_id = module.scheduler.pass_id;
    result->abandoned_passes = module.scheduler.abandoned_passes;
    return true;
}
